#include "AH_Guard.h"
#include "AH_Core.h"
#include "SelfUpdate.h"
#include "IXAC_Net.h"

#include <tlhelp32.h>
#include <psapi.h>
#include <windows.h>


static std::wstring ExtractFileName(const wchar_t* path)
{
    if (!path || !*path)
        return L"";

    const wchar_t* p1 = wcsrchr(path, L'\\');
    const wchar_t* p2 = wcsrchr(path, L'/');
    const wchar_t* p = (p1 > p2 ? p1 : p2);

    if (p)
        return std::wstring(p + 1);

    return std::wstring(path);
}


static std::wstring GetCurrentExeName()
{
    wchar_t path[MAX_PATH] = {};
    if (!GetModuleFileNameW(NULL, path, MAX_PATH))
        return L"";

    return ExtractFileName(path);
}


static std::wstring GetParentExeName()
{
    DWORD myPid = GetCurrentProcessId();
    DWORD parentPid = 0;

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE)
        return L"";

    PROCESSENTRY32 pe{};
    pe.dwSize = sizeof(pe);

    if (Process32First(hSnap, &pe))
    {
        do
        {
            if (pe.th32ProcessID == myPid)
            {
                parentPid = pe.th32ParentProcessID;
                break;
            }
        } while (Process32Next(hSnap, &pe));
    }

    CloseHandle(hSnap);

    if (!parentPid)
        return L"";

    HANDLE hParent = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, parentPid);
    if (!hParent)
        return L"";

    wchar_t path[MAX_PATH] = {};
    DWORD size = MAX_PATH;

    if (!QueryFullProcessImageNameW(hParent, 0, path, &size))
    {
        CloseHandle(hParent);
        return L"";
    }

    CloseHandle(hParent);
    return ExtractFileName(path);
}



static bool IsStartedByAurigaGlobal()
{
    std::wstring selfName = GetCurrentExeName();
    std::wstring parentName = GetParentExeName();

    if (_wcsicmp(selfName.c_str(), AY_OBFUSCATE(L"UserInterface.exe")) == 0)
        return true;

    if (_wcsicmp(parentName.c_str(), AY_OBFUSCATE(L"UserInterface.exe")) == 0)
        return true;

    return false;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:

        if (!IsStartedByAurigaGlobal())
        {
            MessageBoxA(NULL, AY_OBFUSCATE("Use valid launcher"), AY_OBFUSCATE("IXAC"), MB_OK | MB_ICONERROR);
            ExitProcess(0);
        }

        g_hDll = h;


        DisableThreadLibraryCalls(h);


        {
            IXAC::Net::Init();

            IXAC::Net::Config cfg;
            cfg.serverUrl = AY_OBFUSCATE("https://ixac.bwmt2global.eu/api/ac/upload");
            cfg.apiKey = AY_OBFUSCATE("your_secret_api_key");
            cfg.connectTimeoutSec = 5;
            cfg.requestTimeoutSec = 10;
            cfg.verifySsl = true;

            IXAC::Net::SetConfig(cfg);
        }


        CreateThread(NULL, 0, [](LPVOID)->DWORD
            {
                Sleep(1500);

                CheckForUpdate();
                AntiHook::Start();

                return 0;
            }, NULL, 0, NULL);

        break;
    }
    return TRUE;
}
