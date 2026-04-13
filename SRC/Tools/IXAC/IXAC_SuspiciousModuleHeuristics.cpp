// AH_Heuristics.cpp
#include "IXAC_SuspiciousModuleHeuristics.h"
#include "AH_Core.h"

#include <windows.h>
#include <vector>
#include <string>
#include <algorithm>

namespace
{
    using AntiHook::Core::ModuleInfo;
    using AntiHook::Heuristics::SuspiciousModule;

    // Veszélyes API-k – COM2 és hasonló injektorok/cheatek előszeretettel használják
    static const char* kDangerousApis[] =
    {
       AY_OBFUSCATE( "WriteProcessMemory"),
       AY_OBFUSCATE("VirtualAllocEx"),
       AY_OBFUSCATE("CreateRemoteThread"),
       AY_OBFUSCATE("SetThreadContext"),
       AY_OBFUSCATE("QueueUserAPC"),
       AY_OBFUSCATE("DeviceIoControl"),
       AY_OBFUSCATE("SetWindowsHookExW"),
       AY_OBFUSCATE("SetWindowsHookExA"),
       AY_OBFUSCATE("GetAsyncKeyState"),
       AY_OBFUSCATE("SendInput"),
       AY_OBFUSCATE("mouse_event"),
       AY_OBFUSCATE("MapViewOfFile"),
       AY_OBFUSCATE("CreateFileMappingA"),
       AY_OBFUSCATE("CreateFileMappingW"),
       AY_OBFUSCATE("ZwQuerySystemInformation"),
       AY_OBFUSCATE("NtQuerySystemInformation"),
    };

    bool EqualsIgnoreCase(const char* a, const char* b)
    {
        while (*a && *b)
        {
            char ca = *a++;
            char cb = *b++;
            if (ca >= 'A' && ca <= 'Z') ca = char(ca - 'A' + 'a');
            if (cb >= 'A' && cb <= 'Z') cb = char(cb - 'A' + 'a');
            if (ca != cb) return false;
        }
        return (*a == 0 && *b == 0);
    }

    std::wstring ToLower(const std::wstring& s)
    {
        std::wstring r = s;
        std::transform(r.begin(), r.end(), r.begin(), [](wchar_t c) {
            if (c >= L'A' && c <= L'Z') return wchar_t(c - L'A' + L'a');
            return c;
            });
        return r;
    }


    bool IsSystemPath(const std::wstring& path)
    {
        auto lower = ToLower(path);
        // nagyon egyszerű, ha kell finomíthatod
        return (lower.find(L"\\windows\\") != std::wstring::npos);
    }

    // PE import tábla kiolvasása: importált függvénynevek listája
    bool GetImportFunctionNames(uintptr_t moduleBase, std::vector<std::string>& outNames)
    {
        outNames.clear();

        if (!moduleBase)
            return false;

        const BYTE* base = reinterpret_cast<const BYTE*>(moduleBase);
        const IMAGE_DOS_HEADER* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE)
            return false;

        const IMAGE_NT_HEADERS* nt =
            reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE)
            return false;

        const IMAGE_OPTIONAL_HEADER& opt = nt->OptionalHeader;
        const IMAGE_DATA_DIRECTORY& dir =
            opt.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

        if (dir.VirtualAddress == 0)
            return true; // nincs import – nem baj, csak üres lista

        const IMAGE_IMPORT_DESCRIPTOR* impDesc =
            reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress);

        for (; impDesc->Name != 0; ++impDesc)
        {
            const IMAGE_THUNK_DATA* pThunkData =
                impDesc->OriginalFirstThunk
                ? reinterpret_cast<const IMAGE_THUNK_DATA*>(base + impDesc->OriginalFirstThunk)
                : reinterpret_cast<const IMAGE_THUNK_DATA*>(base + impDesc->FirstThunk);

            for (; pThunkData && pThunkData->u1.AddressOfData != 0; ++pThunkData)
            {
                if (IMAGE_SNAP_BY_ORDINAL(pThunkData->u1.Ordinal))
                    continue;

                const IMAGE_IMPORT_BY_NAME* ibn =
                    reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(
                        base + pThunkData->u1.AddressOfData
                        );
                const char* funcName = reinterpret_cast<const char*>(ibn->Name);
                if (funcName && *funcName)
                    outNames.emplace_back(funcName);
            }
        }

        return true;
    }

    int ScoreFromImportNames(const std::vector<std::string>& names, std::string& reasonOut)
    {
        int score = 0;
        bool hasHook = false, hasRemote = false, hasInput = false, hasDriver = false;

        for (const auto& n : names)
        {
            for (const char* api : kDangerousApis)
            {
                if (!EqualsIgnoreCase(n.c_str(), api))
                    continue;

                if (EqualsIgnoreCase(api, AY_OBFUSCATE("SetWindowsHookExW")) ||
                    EqualsIgnoreCase(api, AY_OBFUSCATE("SetWindowsHookExA")))
                {
                    score += 3;
                    hasHook = true;
                }
                else if (EqualsIgnoreCase(api, AY_OBFUSCATE("DeviceIoControl")))
                {
                    score += 3;
                    hasDriver = true;
                }
                else if (EqualsIgnoreCase(api, AY_OBFUSCATE("WriteProcessMemory")) ||
                    EqualsIgnoreCase(api, AY_OBFUSCATE("VirtualAllocEx")) ||
                    EqualsIgnoreCase(api, AY_OBFUSCATE("CreateRemoteThread")))
                {
                    score += 2;
                    hasRemote = true;
                }
                else if (EqualsIgnoreCase(api, AY_OBFUSCATE("GetAsyncKeyState")) ||
                    EqualsIgnoreCase(api, AY_OBFUSCATE("SendInput")) ||
                    EqualsIgnoreCase(api, AY_OBFUSCATE("mouse_event")))
                {
                    score += 1;
                    hasInput = true;
                }
                else
                {
                    score += 1;
                }
            }
        }

        if (score > 0)
        {
            reasonOut = AY_OBFUSCATE("Imports:");
            if (hasHook)   reasonOut += AY_OBFUSCATE(" HookAPI");
            if (hasRemote) reasonOut += AY_OBFUSCATE(" RemoteMem");
            if (hasInput)  reasonOut += AY_OBFUSCATE(" InputAPI");
            if (hasDriver) reasonOut += AY_OBFUSCATE(" DeviceIo");
        }

        return score;
    }
}

namespace AntiHook::Heuristics
{
    std::vector<SuspiciousModule> ScanModulesHeuristic(int scoreThreshold)
    {
        std::vector<SuspiciousModule> result;

        // **ITT használjuk újra az AH_Core funkcióidat**
        auto mods = Core::BuildModuleMap();

        for (const ModuleInfo& m : mods)
        {
            // whitelist → skip
            if (Core::IsModuleWhitelisted(m.name))
                continue;

            bool isSystem = IsSystemPath(m.path);

            std::vector<std::string> imports;
            std::string reason;
            int score = 0;

            if (!GetImportFunctionNames(m.base, imports))
                continue;

            score = ScoreFromImportNames(imports, reason);

            // system DLL-ekre legyünk kevésbé agresszívek
            int effectiveThreshold = scoreThreshold;
            if (isSystem)
                effectiveThreshold += 3;

            if (score >= effectiveThreshold)
            {
                SuspiciousModule sm;
                sm.mod = m;
                sm.score = score;
                sm.reason = reason;
                result.push_back(sm);
            }
        }

        return result;
    }
}
