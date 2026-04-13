#include "AH_HandleScanner.h"
#include "AH_Core.h"
#include <windows.h>
#include <map>
#include <cstdio>
#include <cstdlib>

namespace AntiHook::HandleScanner
{
    void LogSuspiciousProcesses(const char* logFile)
    {
        using namespace AntiHook::Core;

        if (!ResolveNt())
            return;

        ULONG size = 0x20000;
        PSYSTEM_HANDLE_INFORMATION info =
            static_cast<PSYSTEM_HANDLE_INFORMATION>(std::malloc(size));
        if (!info)
            return;

        ULONG retLen = 0;
        auto ntQuery = GetNtQuerySystemInformation();
        NTSTATUS st = ntQuery(SystemHandleInformation, info, size, &retLen);

        if (st == STATUS_INFO_LENGTH_MISMATCH)
        {
            PSYSTEM_HANDLE_INFORMATION newInfo =
                static_cast<PSYSTEM_HANDLE_INFORMATION>(std::realloc(info, retLen));
            if (!newInfo)
            {
                std::free(info);
                return;
            }
            info = newInfo;
            st = ntQuery(SystemHandleInformation, info, retLen, &retLen);
        }

        if (!NT_SUCCESS(st))
        {
            std::free(info);
            return;
        }

        FILE* f = std::fopen(logFile, "a");
        if (!f)
        {
            std::free(info);
            return;
        }

        const DWORD selfPid = GetCurrentProcessId();

        std::map<ULONG, int> counter;

        for (ULONG i = 0; i < info->HandleCount; ++i)
        {
            const auto& h = info->Handles[i];

            if (h.ProcessId == selfPid)
                continue;

            bool suspicious =
                (h.GrantedAccess & PROCESS_VM_WRITE) ||
                (h.GrantedAccess & PROCESS_VM_OPERATION) ||
                (h.GrantedAccess & PROCESS_ALL_ACCESS);

            //if (suspicious)
            //{
            //    counter[h.ProcessId]++;
            //
            //    std::fprintf(
            //        f,
            //        "PID=%lu Handle=0x%X Access=0x%08X\n",
            //        static_cast<unsigned long>(h.ProcessId),
            //        static_cast<unsigned>(h.Handle),
            //        static_cast<unsigned>(h.GrantedAccess)
            //    );
            //}
        }

        for (const auto& kv : counter)
        {
            if (kv.second >= 4)
            {
                std::wstring img = GetProcessImageName(kv.first);
                if (!img.empty())
                {
                    std::fwprintf(
                        f,
                        AY_OBFUSCATE(L"[ANTIHOOK] HANDLE FARM: PID=%lu COUNT=%d IMAGE=%s\n"),
                        static_cast<unsigned long>(kv.first),
                        kv.second,
                        img.c_str()
                    );
                }
                else
                {
                    std::fprintf(
                        f,
                        AY_OBFUSCATE("[ANTIHOOK] HANDLE FARM: PID=%lu COUNT=%d (no image)\n"),
                        static_cast<unsigned long>(kv.first),
                        kv.second
                    );
                }
            }
        }

        std::fclose(f);
        std::free(info);

        for (const auto& kv : counter)
        {
            if (kv.second >= 4)
            {
             //   TerminateProcess(GetCurrentProcess(), 0xDEAD);
             //   break;
            }
        }
    }
}
