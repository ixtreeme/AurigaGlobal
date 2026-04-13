#include "AH_Util.h"


#include "../../Launcher/SecureLayer/obfuscate.h"
#include <windows.h>
#include <string>
#include <vector>

#include <psapi.h>
#include <tlhelp32.h>
#pragma comment(lib, "Version.lib")


static bool QueryVersionField(const BYTE* verData, const wchar_t* field, std::wstring& out)
{
    struct LANGANDCODEPAGE { WORD wLanguage; WORD wCodePage; };
    LANGANDCODEPAGE* translate = nullptr;
    UINT cbTranslate = 0;

    // Nyelvi információk lekérése
    if (!VerQueryValueW((LPVOID)verData, AY_OBFUSCATE(L"\\VarFileInfo\\Translation"),
        (LPVOID*)&translate, &cbTranslate) || cbTranslate < sizeof(LANGANDCODEPAGE))
        return false;

    wchar_t subBlock[256];
    swprintf(subBlock, 256,
        AY_OBFUSCATE(L"\\StringFileInfo\\%04x%04x\\%s"),
        translate[0].wLanguage,
        translate[0].wCodePage,
        field);

    LPVOID ptr = nullptr;
    UINT size = 0;

    if (VerQueryValueW((LPVOID)verData, subBlock, &ptr, &size) && size > 1)
    {
        out.assign((wchar_t*)ptr, size - 1);
        return true;
    }
    return false;
}


DWORD FindExternalThreadOwner()
{
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnap == INVALID_HANDLE_VALUE)
        return 0;

    THREADENTRY32 te{};
    te.dwSize = sizeof(te);

    DWORD myPid = GetCurrentProcessId();

    if (Thread32First(hSnap, &te))
    {
        do
        {
            if (te.th32OwnerProcessID != myPid) // külső folyamat
            {
                // Ez a cheat által létrehozott vagy hijackelt thread
                CloseHandle(hSnap);
                return te.th32OwnerProcessID;
            }

        } while (Thread32Next(hSnap, &te));
    }

    CloseHandle(hSnap);
    return 0;
}


DWORD FindWhoOpenedMyProcess()
{
    DWORD myPid = GetCurrentProcessId();

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE)
        return 0;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);

    if (Process32FirstW(hSnap, &pe))
    {
        do {
            HANDLE hProc = OpenProcess(PROCESS_DUP_HANDLE, FALSE, pe.th32ProcessID);
            if (!hProc) continue;

            HANDLE hDup = NULL;
            if (DuplicateHandle(hProc, (HANDLE)myPid, GetCurrentProcess(), &hDup, 0, FALSE, DUPLICATE_SAME_ACCESS))
            {
                CloseHandle(hProc);
                CloseHandle(hSnap);
                return pe.th32ProcessID; // megtaláltuk a cheat processzt
            }

            CloseHandle(hProc);
        } while (Process32NextW(hSnap, &pe));
    }

    CloseHandle(hSnap);
    return 0;
}



bool GetProcessVersionFingerprint(DWORD pid,
    std::wstring& outDescription,
    std::wstring& outProductName,
    std::wstring& outOriginalFilename)
{
    outDescription.clear();
    outProductName.clear();
    outOriginalFilename.clear();

    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc)
        return false;

    wchar_t imagePath[MAX_PATH];
    DWORD size = MAX_PATH;

    if (!QueryFullProcessImageNameW(hProc, 0, imagePath, &size))
    {
        CloseHandle(hProc);
        return false;
    }
    CloseHandle(hProc);

    DWORD dummy = 0;
    DWORD verSize = GetFileVersionInfoSizeW(imagePath, &dummy);
    if (verSize == 0)
        return false;

    std::vector<BYTE> buffer(verSize);
    if (!GetFileVersionInfoW(imagePath, 0, verSize, buffer.data()))
        return false;

    QueryVersionField(buffer.data(), AY_OBFUSCATE(L"FileDescription"), outDescription);
    QueryVersionField(buffer.data(), AY_OBFUSCATE(L"ProductName"), outProductName);
    QueryVersionField(buffer.data(), AY_OBFUSCATE(L"OriginalFilename"), outOriginalFilename);

    return true;
}



void AppendLog(const char* fmt, ...)
{
    FILE* f = std::fopen(LOG_FILE, "a");
    if (!f)
        return;

    va_list args;
    va_start(args, fmt);
    std::vfprintf(f, fmt, args);
    va_end(args);

    std::fclose(f);
}


DWORD FindProcessOwningMemory(uintptr_t base)
{
    DWORD pids[2048], needed = 0;

    if (!EnumProcesses(pids, sizeof(pids), &needed))
        return 0;

    int count = needed / sizeof(DWORD);

    for (int i = 0; i < count; i++)
    {
        HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pids[i]);
        if (!h) continue;

        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQueryEx(h, (LPCVOID)base, &mbi, sizeof(mbi)) &&
            mbi.AllocationBase == (LPVOID)base)
        {
            CloseHandle(h);
            return pids[i];
        }

        CloseHandle(h);
    }

    return 0;
}

bool FindSuspiciousModuleInside(DWORD pid, std::string& outPath)
{
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!h) return false;

    HMODULE hMods[1024];
    DWORD needed;

    if (EnumProcessModules(h, hMods, sizeof(hMods), &needed))
    {
        int count = needed / sizeof(HMODULE);
        for (int i = 0; i < count; i++)
        {
            wchar_t path[MAX_PATH];
            if (GetModuleFileNameExW(h, hMods[i], path, MAX_PATH))
            {
                std::wstring wpath(path);

                // Ha NEM Microsoft modul → valószínű cheat
                if (wpath.find(AY_OBFUSCATE(L"Microsoft")) == std::wstring::npos &&
                    wpath.find(AY_OBFUSCATE(L"Windows")) == std::wstring::npos)
                {
                    outPath = std::string(wpath.begin(), wpath.end());
                    CloseHandle(h);
                    return true;
                }
            }
        }
    }

    CloseHandle(h);
    return false;
}


bool HasManualMapRegion(DWORD pid)
{
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!h) return false;

    SYSTEM_INFO si;
    GetSystemInfo(&si);

    uint8_t* addr = (uint8_t*)si.lpMinimumApplicationAddress;

    while (addr < si.lpMaximumApplicationAddress)
    {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQueryEx(h, addr, &mbi, sizeof(mbi)))
            break;

        if ((mbi.Protect & PAGE_EXECUTE_READWRITE || mbi.Protect & PAGE_EXECUTE_WRITECOPY) &&
            mbi.Type == MEM_PRIVATE)
        {
            // manual map shellcode found
            CloseHandle(h);
            return true;
        }

        addr += mbi.RegionSize;
    }

    CloseHandle(h);
    return false;
}




void LogCheatFingerprint(DWORD pid)
{
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc) return;

    wchar_t path[MAX_PATH]{};
    DWORD size = MAX_PATH;
    QueryFullProcessImageNameW(hProc, 0, path, &size);

    // Verzióinfók
    DWORD dummy = 0;
    DWORD verSize = GetFileVersionInfoSizeW(path, &dummy);

    std::wstring desc = OBF_W(L"(unknown)");
    std::wstring prod = OBF_W(L"(unknown)");
    std::wstring orig = OBF_W(L"(unknown)");

    if (verSize)
    {
        std::vector<BYTE> data(verSize);
        if (GetFileVersionInfoW(path, 0, verSize, data.data()))
        {
            struct LANGANDCODEPAGE { WORD wLanguage; WORD wCodePage; };
            LANGANDCODEPAGE* trans = nullptr;
            UINT tsize = 0;

            if (VerQueryValueW(data.data(), AY_OBFUSCATE(L"\\VarFileInfo\\Translation"),
                (LPVOID*)&trans, &tsize))
            {
                wchar_t block[256];
                LPVOID val = nullptr;
                UINT vsize = 0;

                swprintf(block, 256, AY_OBFUSCATE(L"\\StringFileInfo\\%04x%04x\\FileDescription"),
                    trans->wLanguage, trans->wCodePage);
                if (VerQueryValueW(data.data(), block, &val, &vsize))
                    desc = (wchar_t*)val;

                swprintf(block, 256, AY_OBFUSCATE(L"\\StringFileInfo\\%04x%04x\\ProductName"),
                    trans->wLanguage, trans->wCodePage);
                if (VerQueryValueW(data.data(), block, &val, &vsize))
                    prod = (wchar_t*)val;

                swprintf(block, 256, AY_OBFUSCATE(L"\\StringFileInfo\\%04x%04x\\OriginalFilename"),
                    trans->wLanguage, trans->wCodePage);
                if (VerQueryValueW(data.data(), block, &val, &vsize))
                    orig = (wchar_t*)val;
            }
        }
    }

    FILE* f = fopen(LOG_FILE, "a");
    if (f)
    {
        fwprintf(f,
            AY_OBFUSCATE(L"[CHEAT RWX]\n"
            L"PID: %u\n"
            L"Path: %s\n"
            L"FileDescription: %s\n"
            L"ProductName: %s\n"
            L"OriginalFilename: %s\n"
            L"-------------------------\n"),
            pid, path, desc.c_str(), prod.c_str(), orig.c_str());

        fclose(f);
    }

    CloseHandle(hProc);
}
