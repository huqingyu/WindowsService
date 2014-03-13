
#include <windows.h>
#include <stdio.h>

void LogInfo(char * file){
	FILE * log = NULL;

	log = fopen(file, "w+");
	if (log != NULL)
	{
		fprintf(log, getenv("temp"));
		fclose(log);
		log = NULL;
	}

}

int main()
{
	LogInfo("C:\\Users\\Administrator\\AppData\\Local\\Temp\\1.log");
	LogInfo("C:\\Windows\\Temp\\1.log");
	LogInfo("C:\\1.log");
}
