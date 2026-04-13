#include "AH_ThreadHijackGuard.h"
#include "AH_Core.h"

#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <vector>
#include <string>

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

namespace
{

    using AntiHook::Core::BuildModuleMap;
    using AntiHook::Core::GetNtQueryInformationThread;
    using AntiHook::Core::IsModuleWhitelisted;
    using AntiHook::Core::ModuleInfo;
    using AntiHook::Core::ResolveNtThread;
    using AntiHook::Core::ToLower;

    

    bool IsExecProt(DWORD prot)
    {
        return (prot & PAGE_EXECUTE) ||
            (prot & PAGE_EXECUTE_READ) ||
            (prot & PAGE_EXECUTE_READWRITE) ||
            (prot & PAGE_EXECUTE_WRITECOPY);
    }

    bool IsPrivateExec(const MEMORY_BASIC_INFORMATION& mbi)
    {
        return mbi.State == MEM_COMMIT &&
            mbi.Type == MEM_PRIVATE &&
            IsExecProt(mbi.Protect);
    }

    const ModuleInfo* FindModuleForAddress(
        const std::vector<ModuleInfo>& mods,
        uintptr_t addr)
    {
        for (const auto& m : mods)
        {
            if (addr >= m.base && addr < m.end)
                return &m;
        }
        return nullptr;
    }

    

    uintptr_t GetThreadStartAddress(HANDLE hThread)
    {
        auto ntQueryInformationThread = GetNtQueryInformationThread();
        if (!ntQueryInformationThread)
            return 0;

        ULONG_PTR startAddrValue = 0;
        NTSTATUS st = ntQueryInformationThread(
            hThread,
            ThreadQuerySetWin32StartAddress,
            &startAddrValue,
            sizeof(startAddrValue),
            nullptr);

        if (!NT_SUCCESS(st) || startAddrValue == 0)
            return 0;

        return static_cast<uintptr_t>(startAddrValue);
    }
}

namespace AntiHook::ThreadHijackGuard
{
    void Scan(const char* logFile)
    {
        if (!ResolveNtThread())
        {
            FILE* f = std::fopen(logFile, "a");
            if (f)
            {
                std::fprintf(f, AY_OBFUSCATE("[ANTIHOOK] NtQueryInformationThread unavailable; skipping ThreadHijack scan.\n"));
                std::fclose(f);
            }
            return;
        }

        const DWORD selfPid = GetCurrentProcessId();

        std::vector<ModuleInfo> modules = BuildModuleMap();

        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (hSnap == INVALID_HANDLE_VALUE)
            return;

        FILE* f = std::fopen(logFile, "a");
        if (!f)
        {
            CloseHandle(hSnap);
            return;
        }

        THREADENTRY32 te{};
        te.dwSize = sizeof(te);

        bool cheatDetected = false;

        if (Thread32First(hSnap, &te))
        {
            do
            {
                if (te.th32OwnerProcessID != selfPid)
                    continue;

                HANDLE hThread = OpenThread(
                    THREAD_QUERY_INFORMATION,
                    FALSE,
                    te.th32ThreadID);

                if (!hThread)
                    continue;

                uintptr_t startAddr = GetThreadStartAddress(hThread);
                CloseHandle(hThread);

                if (!startAddr)
                    continue;

                MEMORY_BASIC_INFORMATION mbi{};
                VirtualQuery(reinterpret_cast<void*>(startAddr), &mbi, sizeof(mbi));

                const ModuleInfo* modStart = FindModuleForAddress(modules, startAddr);

                bool suspicious = false;
                const char* reason = nullptr;

                if (IsPrivateExec(mbi))
                {
                    suspicious = true;
                    reason = AY_OBFUSCATE("start in PRIVATE+EXEC region (injected thread/shellcode)");
                }
                else if (!modStart && IsExecProt(mbi.Protect))
                {
                    suspicious = true;
                    reason =AY_OBFUSCATE( "start in EXEC region with NO MODULE (manual map)");
                }
                else if (modStart && IsExecProt(mbi.Protect))
                {
                    std::wstring lowerName = ToLower(modStart->name);
                    if (!IsModuleWhitelisted(lowerName))
                    {
                        suspicious = true;
                        reason = AY_OBFUSCATE("start in EXEC of non-whitelisted module");
                    }
                }

                if (suspicious)
                {
                    if (modStart)
                    {
                        std::fwprintf(
                            f,
                            AY_OBFUSCATE(L"[ANTIHOOK] THREAD-HIJACK v2-lite: TID=%lu START=0x%p MOD=%s PATH=%s\n"),
                            te.th32ThreadID,
                            reinterpret_cast<void*>(startAddr),
                            modStart->name.c_str(),
                            modStart->path.c_str()
                        );
                    }
                    else
                    {
                        std::fprintf(
                            f,
                            AY_OBFUSCATE("[ANTIHOOK] THREAD-HIJACK v2-lite: TID=%lu START=0x%p MOD=(none)\n"),
                            te.th32ThreadID,
                            reinterpret_cast<void*>(startAddr)
                        );
                    }

                    std::fprintf(
                        f,
                        AY_OBFUSCATE("[ANTIHOOK]   -> reason: %s\n"),
                        reason ? reason : AY_OBFUSCATE("(unknown)")
                    );

                    cheatDetected = true;
                }

            } while (Thread32Next(hSnap, &te));
        }

        std::fclose(f);
        CloseHandle(hSnap);

        if (cheatDetected)
        {
            FILE* fx = std::fopen(logFile, "a");
            if (fx)
            {
                std::fprintf(fx, AY_OBFUSCATE("[ANTIHOOK] TERMINATING CLIENT due to THREAD-HIJACK v2-lite.\n"));
                std::fclose(fx);
            }
        	IXAC_ReportCheat();

            TerminateProcess(GetCurrentProcess(), 0x7F1F);
        }
    }
}
