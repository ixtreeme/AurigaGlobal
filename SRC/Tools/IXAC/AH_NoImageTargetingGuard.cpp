#include "AH_NoImageTargetingGuard.h"
#include "AH_Core.h"

#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

#ifndef SystemExtendedHandleInformation
#define SystemExtendedHandleInformation (SYSTEM_INFORMATION_CLASS)64
#endif

#ifndef STATUS_INFO_LENGTH_MISMATCH
#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004L)
#endif

namespace
{
    using AntiHook::Core::ResolveNt;
    using AntiHook::Core::GetNtQuerySystemInformation;

    struct MY_SYSTEM_HANDLE_TABLE_ENTRY_INFO
    {
        PVOID       Object;
        ULONG_PTR   UniqueProcessId;   // a HANDLE tulajdonosa (attacker PID)
        ULONG_PTR   HandleValue;
        ULONG       GrantedAccess;
        USHORT      CreatorBackTraceIndex;
        USHORT      ObjectTypeIndex;
        ULONG       HandleAttributes;
        ULONG       Reserved;
    };

    struct MY_SYSTEM_HANDLE_INFORMATION_EX
    {
        ULONG_PTR NumberOfHandles;
        ULONG_PTR Reserved;
        MY_SYSTEM_HANDLE_TABLE_ENTRY_INFO Handles[1];
    };

    bool IsDangerous(ACCESS_MASK acc)
    {
        return  (acc & PROCESS_ALL_ACCESS) ||
            (acc & PROCESS_VM_WRITE) ||
            (acc & PROCESS_VM_OPERATION) ||
            (acc & PROCESS_CREATE_THREAD) ||
            (acc & PROCESS_DUP_HANDLE);
    }

    bool TryGetImagePath(DWORD pid, std::wstring& out)
    {
        out.clear();

        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!h)
            return false;

        wchar_t buf[MAX_PATH];
        DWORD sz = MAX_PATH;
        BOOL ok = QueryFullProcessImageNameW(h, 0, buf, &sz);
        CloseHandle(h);

        if (!ok || sz == 0)
            return false;

        out.assign(buf, sz);
        return true;
    }
}

namespace AntiHook::NoImageTargetingGuard
{
    void Scan(const char* logFile, unsigned minDangerousHandles)
    {
        if (!ResolveNt())
            return;

        auto NtQuery = GetNtQuerySystemInformation();
        if (!NtQuery)
            return;

        const DWORD gamePid = GetCurrentProcessId();
        const HANDLE hSelf = GetCurrentProcess();

        ULONG bufSize = 0x40000;
        auto* info = static_cast<MY_SYSTEM_HANDLE_INFORMATION_EX*>(std::malloc(bufSize));
        if (!info)
            return;

        ULONG retLen = 0;
        NTSTATUS st;

        while (true)
        {
            st = NtQuery(SystemExtendedHandleInformation, info, bufSize, &retLen);
            if (st == STATUS_INFO_LENGTH_MISMATCH)
            {
                bufSize = retLen;
                auto* newInfo = static_cast<MY_SYSTEM_HANDLE_INFORMATION_EX*>(std::realloc(info, bufSize));
                if (!newInfo)
                {
                    std::free(info);
                    return;
                }
                info = newInfo;
                continue;
            }
            break;
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

        std::map<DWORD, unsigned> attackerDangerCount;

        // cache: attacker PID -> OpenProcess(PROCESS_DUP_HANDLE) handle
        std::map<DWORD, HANDLE> attackerProcHandles;

        const ULONG_PTR handleCount = info->NumberOfHandles;

        for (ULONG_PTR i = 0; i < handleCount; ++i)
        {
            const MY_SYSTEM_HANDLE_TABLE_ENTRY_INFO& h = info->Handles[i];

            DWORD attackerPid = static_cast<DWORD>(h.UniqueProcessId);

            if (attackerPid == gamePid)
                continue;

            if (!IsDangerous(h.GrantedAccess))
                continue;

            HANDLE hAttackerProc = nullptr;
            auto it = attackerProcHandles.find(attackerPid);
            if (it == attackerProcHandles.end())
            {
                hAttackerProc = OpenProcess(PROCESS_DUP_HANDLE, FALSE, attackerPid);
                attackerProcHandles.emplace(attackerPid, hAttackerProc);
            }
            else
            {
                hAttackerProc = it->second;
            }

            if (!hAttackerProc)
                continue;

            HANDLE hDup = nullptr;
            if (!DuplicateHandle(hAttackerProc,
                reinterpret_cast<HANDLE>(h.HandleValue),
                hSelf,
                &hDup,
                0,
                FALSE,
                DUPLICATE_SAME_ACCESS))
            {
                continue;
            }

            DWORD targetPid = GetProcessId(hDup);
            CloseHandle(hDup);

            if (targetPid == 0 || targetPid == (DWORD)-1)
                continue; // nem process handle

            if (targetPid != gamePid)
                continue;

            attackerDangerCount[attackerPid]++;
        }

        bool cheatDetected = false;

        for (auto& kv : attackerDangerCount)
        {
            DWORD attackerPid = kv.first;
            unsigned cnt = kv.second;

            if (cnt < minDangerousHandles)
                continue;

            std::wstring img;
            bool hasImage = TryGetImagePath(attackerPid, img);

            if (!hasImage || img.empty())
            {
                std::fprintf(
                    f,
                    AY_OBFUSCATE("[ANTIHOOK] NO-IMAGE ATTACKER: PID=%lu dangerousHandlesToGame=%u\n"),
                    static_cast<unsigned long>(attackerPid),
                    cnt
                );
                cheatDetected = true;
            }
            else
            {
                std::fwprintf(
                    f,
                    AY_OBFUSCATE(L"[ANTIHOOK] IMAGE ATTACKER: PID=%lu dangerousHandlesToGame=%u IMAGE=%s\n"),
                    static_cast<unsigned long>(attackerPid),
                    cnt,
                    img.c_str()
                );
            }
        }

        std::fclose(f);

        for (auto& kv : attackerProcHandles)
        {
            if (kv.second)
                CloseHandle(kv.second);
        }

        std::free(info);

        if (cheatDetected)
        {
            FILE* fx = std::fopen(logFile, "a");
            if (fx)
            {
                std::fprintf(fx, AY_OBFUSCATE("[ANTIHOOK] TERMINATING CLIENT due to NO-IMAGE attacker.\n"));
                std::fclose(fx);
            }
            IXAC_ReportCheat();
            TerminateProcess(GetCurrentProcess(), 0x6B17); // "KILL"
        }
    }
}
