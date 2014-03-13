
#include <string>
#include <stdio.h>

#include <windows.h>
#include <Tlhelp32.h>
#include <psapi.h>
#include <stdlib.h>
#include <WtsApi32.h>
#include <Userenv.h>
#include <process.h>

#include "FirstService.h"

static SERVICE_STATUS			serviceStatus;
static SERVICE_STATUS_HANDLE	hServiceStatus = NULL;
static HANDLE	serviceStopEvent = NULL;
static char SERVICENAME[] = "FirstService";


static DWORD WINAPI WorkerThreader(LPVOID lpParam);
static void WINAPI ServiceCtrlHandler(DWORD request);
static void WINAPI ServiceMain(int argc, char** argv);
static BOOL Invoke(const char * processPath, const char * arg);
static BOOL GetCurrentUserToken(PHANDLE primaryToken);

//
//  SCM call ServiceCtrlHandler to change the service status            
//
void WINAPI ServiceCtrlHandler(DWORD request)
{
	switch (request)
	{
	case SERVICE_CONTROL_STOP:

		serviceStatus.dwCurrentState = SERVICE_STOPPED;
		serviceStatus.dwWin32ExitCode = 0;
		
		// This will signal the worker thread to start shutting down
		SetServiceStatus(hServiceStatus, &serviceStatus);
		SetEvent(serviceStopEvent);
		break;

	case SERVICE_CONTROL_SHUTDOWN:
		serviceStatus.dwCurrentState = SERVICE_STOPPED;
		serviceStatus.dwWin32ExitCode = 0;

		SetServiceStatus(hServiceStatus, &serviceStatus);
		SetEvent(serviceStopEvent);
		break;

	default:
		break;
	}
	return;
}

void WINAPI ServiceMain(int argc, char** argv)
{
	hServiceStatus = RegisterServiceCtrlHandler(SERVICENAME, ServiceCtrlHandler);

	if (hServiceStatus == NULL)
	{
		return;
	}

	// Init Service Status
	memset(&serviceStatus, 0, sizeof (serviceStatus));
	serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS;
	serviceStatus.dwCurrentState = SERVICE_START_PENDING;
	serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	serviceStatus.dwWin32ExitCode = 0;
	serviceStatus.dwServiceSpecificExitCode = 0;
	serviceStatus.dwCheckPoint = 0;
	serviceStatus.dwWaitHint = 0;

	SetServiceStatus(hServiceStatus, &serviceStatus);

	serviceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (serviceStopEvent == NULL)
	{
		serviceStatus.dwControlsAccepted = 0;
		serviceStatus.dwCurrentState = SERVICE_STOPPED;
		serviceStatus.dwWin32ExitCode = GetLastError();
		serviceStatus.dwCheckPoint = 1;

		SetServiceStatus(hServiceStatus, &serviceStatus);
		return;
	}

	// Start Service
	serviceStatus.dwCurrentState = SERVICE_RUNNING;
	if (SetServiceStatus(hServiceStatus, &serviceStatus) == FALSE)
	{
		return;
	}

	HANDLE hThread = (HANDLE)CreateThread(NULL, 0, WorkerThreader, NULL, 0, NULL);
	if (hThread != INVALID_HANDLE_VALUE)
		WaitForSingleObject(hThread, INFINITE);

	CloseHandle(serviceStopEvent);

	serviceStatus.dwControlsAccepted = 0;
	serviceStatus.dwCurrentState = SERVICE_STOPPED;
	serviceStatus.dwWin32ExitCode = 0;
	serviceStatus.dwCheckPoint = 3;
	
	SetServiceStatus(hServiceStatus, &serviceStatus);
}

void CreateAndStartService()
{
	SERVICE_TABLE_ENTRY ServiceTable[2] =
	{
		{ SERVICENAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain},
		{ NULL, NULL }
	};

	if (StartServiceCtrlDispatcher(ServiceTable) != TRUE)
	{
		return ;
	}

}



BOOL Invoke(const char * processPath, const char * arg)
{
    HANDLE primaryToken = 0;
    // Get token of the current user
    if (GetCurrentUserToken(&primaryToken) == 0)
    {
        return FALSE;
    }

    STARTUPINFO StartupInfo;
    PROCESS_INFORMATION processInfo;

	memset(&processInfo, 0, sizeof(PROCESS_INFORMATION));
	memset(&StartupInfo, 0, sizeof(STARTUPINFO));

	StartupInfo.cb = sizeof(STARTUPINFO);
	StartupInfo.dwFlags = STARTF_USESHOWWINDOW;
	StartupInfo.wShowWindow = SW_SHOWNORMAL;
	StartupInfo.lpDesktop = NULL;


	SECURITY_ATTRIBUTES Security1;
    Security1.nLength=sizeof(Security1);
    Security1.bInheritHandle=FALSE;
    Security1.lpSecurityDescriptor=NULL;

	SECURITY_ATTRIBUTES Security2;
    Security2.nLength=sizeof(Security2);
    Security2.bInheritHandle=FALSE;
    Security2.lpSecurityDescriptor=NULL;


    std::string command("\"");
	command.append(processPath);
	command.append("\"");

    void* lpEnvironment = NULL;

    // Get all necessary environment variables of logged in user
    // to pass them to the process
    BOOL resultEnv = CreateEnvironmentBlock(&lpEnvironment, primaryToken, FALSE);
    if (resultEnv == 0)
    {                                
        long nError = GetLastError();                                
    }

    // Start the process on behalf of the current user
	//BOOL result = CreateProcessWithTokenW(primaryToken, LOGON_WITH_PROFILE, 0, (LPSTR)(command.c_str()), 
	//	CREATE_NO_WINDOW | NORMAL_PRIORITY_CLASS | CREATE_UNICODE_ENVIRONMENT, lpEnvironment, 0, &StartupInfo, &processInfo);

    BOOL result = CreateProcessAsUser(primaryToken, 0, (LPSTR)(command.c_str()), &Security1, &Security2, FALSE, 
		CREATE_NO_WINDOW | NORMAL_PRIORITY_CLASS | CREATE_UNICODE_ENVIRONMENT, lpEnvironment, 0, &StartupInfo, &processInfo);

	if(result == FALSE){
        long nError = GetLastError();                                
	}
    CloseHandle(primaryToken);
    return result;
}

BOOL GetCurrentUserToken(PHANDLE primaryToken)
{
    int dwSessionId = 0;
    HANDLE hUserToken = 0;

    PWTS_SESSION_INFO pSessionInfo = 0;
    DWORD dwCount = 0;

    // Get the list of all terminal sessions    
	WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pSessionInfo, &dwCount);

    int dataSize = sizeof(WTS_SESSION_INFO);
    
    // look over obtained list in search of the active session
    for (DWORD i = 0; i < dwCount; ++i)
    {
        WTS_SESSION_INFO si = pSessionInfo[i];
        if (WTSActive == si.State)
        {
			// If the current session is active ¨C store its ID
			// Get token of the logged in user by the active session ID
			BOOL bRet = WTSQueryUserToken(si.SessionId, &hUserToken);
			if (bRet == FALSE)
			{
				continue;
			}

			dwSessionId = si.SessionId;
            break;
        }
    }

    return DuplicateTokenEx(hUserToken, TOKEN_ASSIGN_PRIMARY | TOKEN_ALL_ACCESS, 0, SecurityImpersonation, TokenPrimary, primaryToken);
}


DWORD WINAPI WorkerThreader(LPVOID lpParam)
{
	// Invoke MyExe
	Invoke("MyExe.exe", NULL);

	int i = 0;
	while (i < 5)
	{
		Sleep(1000);
		i++;
	}

	return 1;
}