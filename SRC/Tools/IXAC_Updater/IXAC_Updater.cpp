#include <windows.h>
#include <string>
#include <filesystem>
#include <tlhelp32.h>

#include "../../Launcher/SecureLayer/obfuscate.h"


void KillProcessByName(const std::wstring& exeName)
{
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snap == INVALID_HANDLE_VALUE)
		return;

	PROCESSENTRY32W pe{};
	pe.dwSize = sizeof(pe);

	if (Process32FirstW(snap, &pe))
	{
		do
		{
			if (_wcsicmp(pe.szExeFile, exeName.c_str()) == 0)
			{
				HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
				if (hProc)
				{
					TerminateProcess(hProc, 0);
					CloseHandle(hProc);
				}
			}
		} while (Process32NextW(snap, &pe));
	}

	CloseHandle(snap);
}



static void Log(const char* msg)
{
#ifdef _DEBUG
	FILE* f = fopen(AY_OBFUSCATE("UserData/AC/IXAC_Updater.log"), "a");
	if (f)
	{
		fprintf(f, "%s\n", msg);
		fclose(f);
	}
#else
	(void)msg;
#endif
}

int wmain(int argc, wchar_t* argv[])
{

	KillProcessByName(OBF_W(L"AurigaPatcher.exe"));

	Sleep(1000);

	if (argc < 3)
	{
		Log(AY_OBFUSCATE("Usage: IXAC_Updater.exe <target_dll> <new_dll> [restart_exe]"));
		return 1;
	}

	std::wstring targetDll = argv[1]; // pl. C:\Games\Auriga\IXAC.dll
	std::wstring newDll = argv[2]; // pl. C:\Games\Auriga\IXAC_new.dll
	std::wstring restartExe = (argc >= 4) ? argv[3] : L"";

	{
		std::string s(restartExe.begin(), restartExe.end());
		Log((std::string(AY_OBFUSCATE("restartExe = ")) + s).c_str());
	}

	// Kis késleltetés, hogy az AC / kliens biztosan kilépjen
	Sleep(1500);

	// Biztos ami biztos: ha a target létezik, megpróbálhatod elõtte átnevezni .bak-ra
	std::wstring backup = targetDll + OBF_W(L".bak");
	MoveFileExW(targetDll.c_str(), backup.c_str(),
		MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);

	bool replaced = false;

	// Többszöri próbálkozás, ha valami még fogja a fájlt
	for (int i = 0; i < 20; ++i)
	{
		if (MoveFileExW(newDll.c_str(), targetDll.c_str(),
			MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		{
			replaced = true;
			break;
		}

		// Ha nem sikerült, várunk kicsit és újra próbáljuk
		Sleep(500);
	}

	if (!replaced)
	{
		Log(AY_OBFUSCATE("Failed to replace IXAC.dll"));
		return 2;
	}

	// Ha nem kell backup, törölheted:
	DeleteFileW(backup.c_str());

	// Opcionális: kliens újraindítása
	if (!restartExe.empty())
	{
		ShellExecuteW(NULL, AY_OBFUSCATE(L"runas"), restartExe.c_str(), NULL,NULL,SW_SHOWNORMAL);
	}

	// SELF-DELETE: saját exe törlése egy háttér CMD-vel
	wchar_t myPath[MAX_PATH];
	GetModuleFileNameW(NULL, myPath, MAX_PATH);

	std::wstring delCmd = OBF_W(L"/C ping 127.0.0.1 -n 2 >NUL & del /f /q \"");
	delCmd += myPath;
	delCmd += L"\"";

	STARTUPINFOW si2{};
	si2.cb = sizeof(si2);
	PROCESS_INFORMATION pi2{};

	std::wstring fullCmd = OBF_W(L"cmd.exe ");
	fullCmd += delCmd;

	if (CreateProcessW(NULL, fullCmd.data(), NULL, NULL, FALSE,
		CREATE_NO_WINDOW, NULL, NULL, &si2, &pi2))
	{
		CloseHandle(pi2.hThread);
		CloseHandle(pi2.hProcess);
	}

	return 0;
}
