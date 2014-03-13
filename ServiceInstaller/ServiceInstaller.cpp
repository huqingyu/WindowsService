// ServiceInstaller.cpp : Defines the entry point for the console application.
//

#include <windows.h>
#include <string>
#include <stdio.h>

using namespace std;

bool IsInstalled();
bool Install();
bool Uninstall();
void GetCurrentFolder(char * pFolder);

static const char SERVICENAME[] = "FirstService";

void GetCurrentFolder(string &folder)
{
	char szTemp[MAX_PATH] = {0};
	
	GetModuleFileName(NULL, szTemp, MAX_PATH);

	string file(szTemp);
	
	int lastSlash = file.find_last_of('\\');
	file.erase(lastSlash);

	folder.assign(file.c_str());
}



bool IsInstalled()
{
	bool bResult = false;

	// Open SCM
	SC_HANDLE hSCM, hService;
	hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

	if (hSCM != NULL)
	{ 
		hService = OpenService(hSCM, SERVICENAME, SERVICE_QUERY_CONFIG);
		if (hService != NULL)
		{
			bResult = true;
			CloseServiceHandle(hService);
		}
		
		CloseServiceHandle(hSCM);
	}else{
		return false;
	}
	return bResult;
}

bool Install()
{
	SC_HANDLE hSCM, hService;
	SERVICE_STATUS service_status;

	if (! IsInstalled())
	{
		char szFile[MAX_PATH] = {0};

		string folder;
		GetCurrentFolder(folder);
		sprintf(szFile, "%s\\FirstService.exe", folder.c_str());

		hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		if (!hSCM)
		{
			return false;
		}

		// Create Service
		hService = CreateService(hSCM, SERVICENAME, SERVICENAME,
			SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
			SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
			szFile, NULL, NULL, "", NULL, NULL);

		if (hService == NULL)
		{
			CloseServiceHandle(hSCM);
			return false;
		}
	}

	SC_LOCK sclLock = LockServiceDatabase(hSCM); 
	if (sclLock != NULL) 
	{ 
		// change description
		SERVICE_DESCRIPTION sdBuf;
		sdBuf.lpDescription = "The Description of My First Service.";
		
		ChangeServiceConfig2(hService, SERVICE_CONFIG_DESCRIPTION, &sdBuf);
		UnlockServiceDatabase(sclLock);
	}

	QueryServiceStatus(hService, &service_status);
	if (service_status.dwCurrentState = SERVICE_STOPPED)
	{
		if (StartService(hService, 0, NULL) == true)
		{
			CloseServiceHandle(hService);
			CloseServiceHandle(hSCM);
			return true;
		}else{
			CloseServiceHandle(hService);
			CloseServiceHandle(hSCM);
			return false;
		}
	}

	CloseServiceHandle(hService);
	CloseServiceHandle(hSCM);

	return true;
}

bool Uninstall()
{
	SC_HANDLE hSCM, hService;
	SERVICE_STATUS status;
	bool bDelete;

	if (!IsInstalled())
		return true;

	hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

	if (hSCM == NULL)
	{
		return false;
	}

	hService = OpenService(hSCM, SERVICENAME, SERVICE_STOP | DELETE);

	if (hService == NULL)
	{
		CloseServiceHandle(hSCM);
		return false;
	}

	ControlService(hService, SERVICE_CONTROL_STOP, &status);

	// Uninstall service 
	bDelete = DeleteService(hService);

	CloseServiceHandle(hService);
	CloseServiceHandle(hSCM);

	if (bDelete)
	{
		return true;
	}
	return false;
}


int main(int argc, char* argv[])
{
	if(argc>1 && strcmp(argv[1],"-u") == 0){
		Uninstall();
		return 0;
	}

	Install();

	return 0;
}

