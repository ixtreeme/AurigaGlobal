#include "AH_RwxScanner.h"
#include "AH_Core.h"
#include "AH_Util.h"
#include "IXAC_Net.h"
#include <windows.h>
#include <cstdio>
#include <vector>
#include <cstring>

#include "HWID/CHwidManager.h"


// ===============================================================
// KÜLSŐ TÁMADÓ FOLYAMAT KERESÉSE (HANDLE alapján)
// ===============================================================
DWORD FindAttackerProcess(DWORD targetPid)
{
    ULONG len = 0;
    auto NtQuerySystemInformation = AntiHook::Core::GetNtQuerySystemInformation();
    if (NtQuerySystemInformation)
    {
        NtQuerySystemInformation(SystemHandleInformation, nullptr, 0, &len);
    }
    else
    {
        // Handle error: function pointer not resolved
        return 0;
    }

    auto buffer = (PSYSTEM_HANDLE_INFORMATION)malloc(len);
    if (!buffer) return 0;

    if (NtQuerySystemInformation(SystemHandleInformation, buffer, len, &len) != 0)
    {
        free(buffer);
        return 0;
    }

    DWORD attackerPid = 0;

    for (ULONG i = 0; i < buffer->HandleCount; ++i)
    {
        const auto& h = buffer->Handles[i];

        if (h.ObjectTypeNumber != 0x7) // Process handle
            continue;

        if (h.ProcessId == targetPid)
            continue; // saját magunk

        // Ha a handle célpontja a mi PID-ünk:
        if ((DWORD)h.Handle != 0)
        {
            if (h.GrantedAccess &
                (PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_CREATE_THREAD))
            {
                attackerPid = h.ProcessId;
                break;
            }
        }
    }

    free(buffer);
    return attackerPid;
}


namespace
{
    using AntiHook::Core::ModuleInfo;
    using AntiHook::Core::BuildModuleMap;

    bool IsExec(DWORD prot)
    {
        return (prot & PAGE_EXECUTE) ||
            (prot & PAGE_EXECUTE_READ) ||
            (prot & PAGE_EXECUTE_READWRITE) ||
            (prot & PAGE_EXECUTE_WRITECOPY);
    }
}

namespace AntiHook::RwxScanner
{
    void ScanForRwxRegions(const char* logFile)
    {
        SYSTEM_INFO si;
        GetSystemInfo(&si);

        auto* addr = static_cast<uint8_t*>(si.lpMinimumApplicationAddress);
        auto* end = static_cast<uint8_t*>(si.lpMaximumApplicationAddress);

        std::vector<ModuleInfo> modules = BuildModuleMap();

        FILE* f = std::fopen(logFile, "a");
        if (!f)
            return;

        bool foundLargeSuspicious = false;

        const SIZE_T LARGE_EXEC_THRESHOLD = 512 * 1024; // 512 KB; ha akarod, legyen 1 * 1024 * 1024

        while (addr < end)
        {
            MEMORY_BASIC_INFORMATION mbi;
            std::memset(&mbi, 0, sizeof(mbi));

            SIZE_T res = VirtualQuery(addr, &mbi, sizeof(mbi));
            if (res == 0)
                break;

            if (mbi.State == MEM_COMMIT &&
                mbi.Type == MEM_PRIVATE &&
                IsExec(mbi.Protect))
            {
                uintptr_t regionBase = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
                SIZE_T    regionSize = mbi.RegionSize;

                const ModuleInfo* owner = nullptr;
                for (const auto& m : modules)
                {
                    if (regionBase >= m.base && regionBase < m.end)
                    {
                        owner = &m;
                        break;
                    }
                }

                
                
				auto CheatPID = FindExternalThreadOwner();

                if (!CheatPID)
                {
                    // 2) RWX régió PID keresés
                    CheatPID = FindProcessOwningMemory(regionBase);
                }
                if (!CheatPID)
                {
                    // 3) OpenProcess scanner
                    CheatPID = FindWhoOpenedMyProcess();
                }
                std::string modPath;
                bool foundModule = FindSuspiciousModuleInside(CheatPID, modPath);

                bool isManualMap = HasManualMapRegion(CheatPID);



                if (foundModule)
                {
                   // IXAC_LOG("PID: %lu, path: %s ", CheatPID, modPath.c_str());
                }
                else if (isManualMap)
                {
                  //  IXAC_LOG("PID: %lu ,Explorer infect, ManualMap, Shellcode", CheatPID);
                }


                LogCheatFingerprint(CheatPID);


                if (owner)
                {
                    std::fwprintf(
                        f,
                        AY_OBFUSCATE(L"[ANTIHOOK] PRIVATE EXEC region INSIDE module: "
                        L"base=0x%p size=0x%Ix prot=0x%08X MOD=%s PATH=%s\n"),
                        mbi.BaseAddress,
                        static_cast<size_t>(regionSize),
                        mbi.Protect,
                        owner->name.c_str(),
                        owner->path.c_str()
                    );
                }
                else
                {
                    std::fprintf(
                        f,
                        AY_OBFUSCATE("[ANTIHOOK] PRIVATE EXEC region with NO MODULE:"
									 " base=0x%p size=0x%Ix prot=0x%08X\n"),
                        mbi.BaseAddress,
                        static_cast<size_t>(regionSize),
                        mbi.Protect
                    );
                }

                // Csak a NAGY privát+exec régiókat tekintjük automatikusan cheatnek
                if (regionSize >= LARGE_EXEC_THRESHOLD)
                {
                    foundLargeSuspicious = true;
                }
            }

            addr += mbi.RegionSize ? mbi.RegionSize : 0x1000;
        }






        fclose(f);

        if (foundLargeSuspicious)
        {

			DWORD myPid = GetCurrentProcessId();
			DWORD aPid = FindAttackerProcess(myPid);

            



            if (aPid != 0)
            {
             //   LogCheatFingerprint(CheatPID);
            }
        	else
            {
                FILE* f = fopen(LOG_FILE, "a");
                if (f)
                {
                    fprintf(f,
                        AY_OBFUSCATE("[CHEAT RWX] Attacker process NOT FOUND (HandleScan failed)\n"));
                    fclose(f);
                }
            }

            if (IXAC_ReportCheat())
            {
                //IXAC_LOG("RWX cheat");
            }

            Sleep(500);

            TerminateProcess(GetCurrentProcess(), 0xF0E0);
        }
    }
}

