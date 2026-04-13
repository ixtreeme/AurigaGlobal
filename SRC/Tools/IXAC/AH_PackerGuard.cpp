// AH_PackerGuard.cpp
// Themida / WinLicense / VMProtect packer detektálás futó processzekben
// PE szekció nevek alapján.
//
// Detektált packerek:
//   Themida / WinLicense : .themida  .boot  _winlice  .winlice  _oreans
//   VMProtect            : .vmp0     .vmp1  .vmp2
//   Obsidium             : .obs      .obsidium
//   ASProtect            : .aspr
//   Enigma Protector     : .enigma1  .enigma2
//   MPRESS               : .MPRESS1  .MPRESS2
//   PELock               : .pelock
//   ExeCryptor           : .xdata
//
// A HLBot konkrétan .themida + .boot szekciókat használ (Themida 3.x).

#include "AH_PackerGuard.h"
#include "AH_Util.h"
#include "AH_Core.h"

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <string>
#include <vector>
#include <algorithm>

#pragma comment(lib, "psapi.lib")

namespace
{
    // -------------------------------------------------------------------------
    // Ismert packer szekció nevek és a hozzájuk tartozó packer neve
    // -------------------------------------------------------------------------
    struct PackerSig
    {
        const char* sectionName;  // pontosan 8 char max (PE korlát)
        const char* packerName;
        int         score;        // veszélyességi pont
    };

    static const PackerSig kPackerSections[] =
    {
        // Themida / WinLicense (Oreans Technologies)
        { ".themida",  "Themida",         10 },
        { ".boot",     "Themida/boot",    8  },
        { "_winlice",  "WinLicense",      10 },
        { ".winlice",  "WinLicense",      10 },
        { "_oreans",   "Oreans",          10 },
        { ".oreans",   "Oreans",          10 },

        // VMProtect
        { ".vmp0",     "VMProtect",       10 },
        { ".vmp1",     "VMProtect",       10 },
        { ".vmp2",     "VMProtect",       10 },

        // Obsidium
        { ".obs",      "Obsidium",        9  },
        { ".obsidium", "Obsidium",        9  },

        // ASProtect
        { ".aspr",     "ASProtect",       8  },
        { ".aspr2",    "ASProtect",       8  },

        // Enigma Protector
        { ".enigma1",  "Enigma",          9  },
        { ".enigma2",  "Enigma",          9  },

        // MPRESS
        { ".MPRESS1",  "MPRESS",          7  },
        { ".MPRESS2",  "MPRESS",          7  },

        // PELock
        { ".pelock",   "PELock",          8  },

        // ExeCryptor
        { ".xdata",    "ExeCryptor",      6  },

        // UPX (kevésbé veszélyes, de gyanús játék kontextusban)
        { "UPX0",      "UPX",             5  },
        { "UPX1",      "UPX",             5  },
    };

    static const size_t kPackerSectionCount =
        sizeof(kPackerSections) / sizeof(kPackerSections[0]);

    // -------------------------------------------------------------------------
    // Whitelisted process nevek - ezeket NEM vizsgáljuk
    // (rendszer folyamatok amelyek legitim módon packelve lehetnek)
    // -------------------------------------------------------------------------
    static const wchar_t* kWhitelistedProcesses[] =
    {
        L"system",
        L"smss.exe",
        L"csrss.exe",
        L"wininit.exe",
        L"winlogon.exe",
        L"services.exe",
        L"lsass.exe",
        L"svchost.exe",
        L"dwm.exe",
        L"explorer.exe",   // a hollowing detektálás külön kezeli
        L"taskhostw.exe",
        L"conhost.exe",
        L"dllhost.exe",
        L"sihost.exe",
        L"fontdrvhost.exe",
        L"spoolsv.exe",
        L"searchindexer.exe",
        L"audiodg.exe",
        L"wuauclt.exe",
        L"msiexec.exe",
        L"userinit.exe",
        L"RuntimeBroker.exe",
        L"ShellExperienceHost.exe",
        L"StartMenuExperienceHost.exe",
        L"SearchUI.exe",
        L"SearchApp.exe",
        L"UserInterface.exe",   // maga a Metin2 kliens
        L"metin2client.exe",
        L"metin2.exe",
    };

    static const size_t kWhitelistCount =
        sizeof(kWhitelistedProcesses) / sizeof(kWhitelistedProcesses[0]);

    // -------------------------------------------------------------------------
    // Segédfüggvények
    // -------------------------------------------------------------------------

    std::wstring WstrToLower(const std::wstring& s)
    {
        std::wstring r = s;
        for (auto& c : r)
        {
            if (c >= L'A' && c <= L'Z')
                c = wchar_t(c - L'A' + L'a');
        }
        return r;
    }

    bool IsWhitelisted(const std::wstring& processNameLower)
    {
        for (size_t i = 0; i < kWhitelistCount; ++i)
        {
            std::wstring wl = WstrToLower(kWhitelistedProcesses[i]);
            if (processNameLower == wl)
                return true;
        }
        return false;
    }

    // Szekció név összehasonlítás (max 8 char, null-padded)
    bool SectionNameEquals(const BYTE rawName[8], const char* target)
    {
        char buf[9] = {};
        memcpy(buf, rawName, 8);
        // case-insensitive
        for (int i = 0; i < 8; ++i)
        {
            char a = buf[i];
            char b = target[i];
            if (!b) return (a == 0);   // target rövidebb → match ha buf is vége
            if (!a) return false;
            if (a >= 'A' && a <= 'Z') a = char(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = char(b - 'A' + 'a');
            if (a != b) return false;
        }
        return true;
    }

    // -------------------------------------------------------------------------
    // PE fejléc beolvasása egy idegen processből
    // Visszatér: igaz ha sikerült, és feltölti a szekció neveket
    // -------------------------------------------------------------------------
    struct SectionResult
    {
        const char* packerName;
        int         score;
        char        sectionName[16];
    };

    // Megpróbálja beolvasni a PE fejlécet a processből és megkeresi a packer
    // szekciókat. Ha talál, feltölti az out vektort.
    bool ScanProcessPESections(
        HANDLE hProc,
        uintptr_t baseAddr,
        std::vector<SectionResult>& outFindings)
    {
        outFindings.clear();

        // Olvassuk be a PE fejlécet (első 4KB elegendő)
        BYTE header[0x1000] = {};
        SIZE_T bytesRead = 0;

        if (!ReadProcessMemory(
            hProc,
            reinterpret_cast<LPCVOID>(baseAddr),
            header, sizeof(header), &bytesRead))
        {
            return false;
        }

        // DOS header
        auto* dos = reinterpret_cast<PIMAGE_DOS_HEADER>(header);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE)
            return false;

        LONG e_lfanew = dos->e_lfanew;
        if (e_lfanew < 0 || e_lfanew > 0x800)
            return false;

        // NT header
        auto* nt = reinterpret_cast<PIMAGE_NT_HEADERS>(header + e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE)
            return false;

        WORD numSections = nt->FileHeader.NumberOfSections;
        if (numSections == 0 || numSections > 96)
            return false;

        // Szekció fejlécek
        PIMAGE_SECTION_HEADER sec = IMAGE_FIRST_SECTION(nt);

        // Ellenőrizzük hogy a szekció tábla a bufferben van-e
        uintptr_t secTableEnd = reinterpret_cast<uintptr_t>(sec + numSections)
            - reinterpret_cast<uintptr_t>(header);
        if (secTableEnd > sizeof(header))
            return false;

        for (WORD i = 0; i < numSections; ++i)
        {
            for (size_t j = 0; j < kPackerSectionCount; ++j)
            {
                if (SectionNameEquals(sec[i].Name, kPackerSections[j].sectionName))
                {
                    SectionResult sr{};
                    sr.packerName = kPackerSections[j].packerName;
                    sr.score      = kPackerSections[j].score;
                    // Szekció nevet null-terminated stringként mentjük
                    memcpy(sr.sectionName, sec[i].Name, 8);
                    sr.sectionName[8] = '\0';
                    outFindings.push_back(sr);
                    break;  // egy szekció csak egyszer illik
                }
            }
        }

        return true;
    }

    // -------------------------------------------------------------------------
    // Egy process összes moduljában keres packer szekciókat
    // -------------------------------------------------------------------------
    struct ProcessPackerResult
    {
        DWORD       pid;
        std::wstring processName;
        std::wstring processPath;
        std::vector<SectionResult> findings;
        int totalScore;
    };

    bool CheckProcess(DWORD pid, const std::wstring& processName,
                      ProcessPackerResult& outResult)
    {
        outResult = {};
        outResult.pid = pid;
        outResult.processName = processName;
        outResult.totalScore = 0;

        HANDLE hProc = OpenProcess(
            PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
            FALSE, pid);

        if (!hProc)
        {
            // PROCESS_QUERY_LIMITED_INFORMATION-nal is megpróbáljuk
            hProc = OpenProcess(
                PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ,
                FALSE, pid);
            if (!hProc)
                return false;
        }

        // Process teljes elérési útja
        wchar_t pathBuf[MAX_PATH] = {};
        DWORD pathSize = MAX_PATH;
        QueryFullProcessImageNameW(hProc, 0, pathBuf, &pathSize);
        outResult.processPath = pathBuf;

        // Fő modul (EXE) base address lekérése
        HMODULE hMods[1] = {};
        DWORD needed = 0;
        if (EnumProcessModules(hProc, hMods, sizeof(hMods), &needed) && hMods[0])
        {
            std::vector<SectionResult> findings;
            if (ScanProcessPESections(
                hProc,
                reinterpret_cast<uintptr_t>(hMods[0]),
                findings))
            {
                for (const auto& f : findings)
                {
                    outResult.findings.push_back(f);
                    outResult.totalScore += f.score;
                }
            }
        }

        CloseHandle(hProc);
        return !outResult.findings.empty();
    }

} // anonymous namespace


namespace AntiHook::PackerGuard
{
    void ScanProcessesForPackers(const char* logFile)
    {
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap == INVALID_HANDLE_VALUE)
            return;

        PROCESSENTRY32W pe{};
        pe.dwSize = sizeof(pe);

        if (!Process32FirstW(hSnap, &pe))
        {
            CloseHandle(hSnap);
            return;
        }

        // Saját PID - magunkat nem ellenőrizzük
        DWORD selfPid = GetCurrentProcessId();

        do
        {
            if (pe.th32ProcessID == selfPid)
                continue;

            std::wstring nameLow = WstrToLower(pe.szExeFile);

            // Whitelist ellenőrzés
            if (IsWhitelisted(nameLow))
                continue;

            ProcessPackerResult result;
            if (!CheckProcess(pe.th32ProcessID, pe.szExeFile, result))
                continue;

            // Találat! → logolás és terminálás

            // Log összeállítása
            char logBuf[1024] = {};
            char findingsStr[512] = {};

            for (const auto& f : result.findings)
            {
                char tmp[64];
                snprintf(tmp, sizeof(tmp),
                    AY_OBFUSCATE("%s(%s) "),
                    f.packerName, f.sectionName);
                strncat(findingsStr, tmp,
                    sizeof(findingsStr) - strlen(findingsStr) - 1);
            }

            // Process path - wide to narrow (csak ASCII karakterek)
            char pathNarrow[MAX_PATH] = {};
            WideCharToMultiByte(CP_ACP, 0,
                result.processPath.c_str(), -1,
                pathNarrow, MAX_PATH, nullptr, nullptr);

            char processNameNarrow[256] = {};
            WideCharToMultiByte(CP_ACP, 0,
                result.processName.c_str(), -1,
                processNameNarrow, 256, nullptr, nullptr);

            snprintf(logBuf, sizeof(logBuf),
                AY_OBFUSCATE("[CHEAT PACKER] Packed process detected!\n"
                    "PID: %lu\n"
                    "Name: %s\n"
                    "Path: %s\n"
                    "Packer(s): %s\n"
                    "Score: %d\n"
                    "-------------------------\n"),
                static_cast<unsigned long>(result.pid),
                processNameNarrow,
                pathNarrow,
                findingsStr,
                result.totalScore);

            AppendLog(logBuf);

            // Riport küldés az IXAC szervernek
            IXAC_ReportCheat();

            CloseHandle(hSnap);

            // Kis késleltetés hogy a log biztosan elküldjön
            Sleep(2500);
            TerminateProcess(GetCurrentProcess(), 0xB1CC);
            return;

        } while (Process32NextW(hSnap, &pe));

        CloseHandle(hSnap);
    }

} // namespace AntiHook::PackerGuard
