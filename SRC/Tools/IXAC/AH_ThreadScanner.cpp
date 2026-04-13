#include "AH_ThreadScanner.h"
#include "AH_Core.h"

#include <windows.h>
#include <tlhelp32.h>
#include <vector>
#include <cstdio>

namespace
{
    unsigned long g_WatchdogTid = 0;
    unsigned long g_GuardianTid = 0;

    bool IsAddressInModule(const AntiHook::Core::ModuleInfo& m, uintptr_t addr)
    {
        return addr >= m.base && addr < m.end;
    }

    const AntiHook::Core::ModuleInfo* FindModuleForAddress(
        const std::vector<AntiHook::Core::ModuleInfo>& mods,
        uintptr_t addr)
    {
        for (const auto& m : mods)
        {
            if (IsAddressInModule(m, addr))
                return &m;
        }
        return nullptr;
    }

    bool IsRwExecutable(DWORD prot)
    {
        return (prot & PAGE_EXECUTE_READWRITE) ||
            (prot & PAGE_EXECUTE_WRITECOPY);
    }
}

namespace AntiHook::ThreadScanner
{
    void SetWatchdogThreadId(unsigned long tid) { g_WatchdogTid = tid; }
    void SetGuardianThreadId(unsigned long tid) { g_GuardianTid = tid; }

    // =====================================================================
    //  THREAD-IP SCAN (kicsit finomított változat)
    // =====================================================================

    void ScanThreadsForSuspiciousEip(const char* logFile)
    {
        using namespace AntiHook::Core;

        std::vector<ModuleInfo> modules = BuildModuleMap();

        FILE* f = std::fopen(logFile, "a");
        if (!f)
            return;


        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snap == INVALID_HANDLE_VALUE)
        {
            fprintf(f, AY_OBFUSCATE("CreateToolhelp32Snapshot(THREAD) failed\n"));
            fclose(f);
            return;
        }

        const DWORD selfPid = GetCurrentProcessId();

        THREADENTRY32 te;
        te.dwSize = sizeof(te);

        bool foundSuspicious = false;

        if (Thread32First(snap, &te))
        {
            do
            {
                if (te.th32OwnerProcessID != selfPid)
                    continue;

                if (te.th32ThreadID == GetCurrentThreadId())
                    continue;

                if (te.th32ThreadID == g_WatchdogTid || te.th32ThreadID == g_GuardianTid)
                    continue;

                HANDLE hThread = OpenThread(THREAD_QUERY_INFORMATION | THREAD_GET_CONTEXT, FALSE, te.th32ThreadID);
                if (!hThread)
                    continue;

#ifdef _M_X64
                CONTEXT ctx{};
                ctx.ContextFlags = CONTEXT_CONTROL;
                if (!GetThreadContext(hThread, &ctx))
                {
                    CloseHandle(hThread);
                    continue;
                }
                uintptr_t ip = static_cast<uintptr_t>(ctx.Rip);
#else
                CONTEXT ctx{};
                ctx.ContextFlags = CONTEXT_CONTROL;
                if (!GetThreadContext(hThread, &ctx))
                {
                    CloseHandle(hThread);
                    continue;
                }
                uintptr_t ip = static_cast<uintptr_t>(ctx.Eip);
#endif

                CloseHandle(hThread);

                // 1) IP memóriája – ha RWX privát, az már magában gyanús (shellcode)
                MEMORY_BASIC_INFORMATION mbi{};
                if (VirtualQuery(reinterpret_cast<LPCVOID>(ip), &mbi, sizeof(mbi)))
                {
                    if (mbi.State == MEM_COMMIT &&
                        mbi.Type == MEM_PRIVATE &&
                        IsRwExecutable(mbi.Protect))
                    {
                        fprintf(
                            f,
                            AY_OBFUSCATE("[ANTIHOOK] THREAD-IP in RWX PRIVATE region: TID=%lu IP=0x%p prot=0x%08X\n"),
                            static_cast<unsigned long>(te.th32ThreadID),
                            reinterpret_cast<void*>(ip),
                            mbi.Protect
                        );
                        foundSuspicious = true;
                        continue;
                    }
                }

                // 2) Modul tulajdonos keresése
                const ModuleInfo* owner = FindModuleForAddress(modules, ip);

                // Owner nélküli IP → Win10/11 alatt normális lehet (scheduler thunk, stb.).
                if (!owner)
                    continue;

                // Ha modul nincs whitelisten → gyanús
                if (!IsModuleWhitelisted(owner->name))
                {
                    
                   fwprintf(
                        f,
                        AY_OBFUSCATE(L"[ANTIHOOK] THREAD-IP in non-whitelisted module: "
                       L"TID=%lu IP=0x%p MOD=%s PATH=%s\n"),
                        static_cast<unsigned long>(te.th32ThreadID),
                        reinterpret_cast<void*>(ip),
                        owner->name.c_str(),
                        owner->path.c_str()
                    );
                    foundSuspicious = true;
                }

            } while (Thread32Next(snap, &te));
        }

        CloseHandle(snap);

        fclose(f);

        if (foundSuspicious)
        {
            IXAC_ReportCheat();
            TerminateProcess(GetCurrentProcess(), 0xBEEF);
        }
    }

    // =====================================================================
    //  THREAD-START SCAN – ÚJ, STABIL VÁLTOZAT
    // =====================================================================

    void ScanThreadStartAddresses(const char* logFile)
    {
        using namespace AntiHook::Core;

        if (!ResolveNtThread())
            return;

        auto ntQueryThread = GetNtQueryInformationThread();
        if (!ntQueryThread)
            return;

        std::vector<ModuleInfo> modules = BuildModuleMap();

        FILE* f = std::fopen(logFile, "a");
        if (!f)
            return;

        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snap == INVALID_HANDLE_VALUE)
        {
            std::fprintf(f, AY_OBFUSCATE("CreateToolhelp32Snapshot(THREAD) failed\n"));
            std::fclose(f);
            return;
        }

        const DWORD selfPid = GetCurrentProcessId();

        THREADENTRY32 te;
        te.dwSize = sizeof(te);

        bool foundSuspicious = false;

        if (Thread32First(snap, &te))
        {
            do
            {
                if (te.th32OwnerProcessID != selfPid)
                    continue;

                if (te.th32ThreadID == GetCurrentThreadId())
                    continue;

                if (te.th32ThreadID == g_WatchdogTid || te.th32ThreadID == g_GuardianTid)
                    continue;

                HANDLE hThread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, te.th32ThreadID);
                if (!hThread)
                    continue;

                PVOID startAddr = nullptr;
                NTSTATUS st = ntQueryThread(
                    hThread,
                    ThreadQuerySetWin32StartAddress,
                    &startAddr,
                    sizeof(startAddr),
                    nullptr);

                CloseHandle(hThread);

                if (!NT_SUCCESS(st) || !startAddr)
                    continue;

                uintptr_t a = reinterpret_cast<uintptr_t>(startAddr);

                // 1) Memória ellenőrzés – RWX privát → nagyon gyanús (shellcode thread)
                MEMORY_BASIC_INFORMATION mbi{};
                if (VirtualQuery(reinterpret_cast<LPCVOID>(a), &mbi, sizeof(mbi)))
                {
                    if (mbi.State == MEM_COMMIT &&
                        mbi.Type == MEM_PRIVATE &&
                        IsRwExecutable(mbi.Protect))
                    {
                        fprintf(
                            f,
                            AY_OBFUSCATE("[ANTIHOOK] THREAD-START in RWX PRIVATE region: "
                            "TID=%lu START=0x%p prot=0x%08X\n"),
                            static_cast<unsigned long>(te.th32ThreadID),
                            startAddr,
                            mbi.Protect
                        );
                        foundSuspicious = true;
                        continue;
                    }
                }

                // 2) Modul tulajdonos keresése
                const ModuleInfo* owner = FindModuleForAddress(modules, a);

                // Ha nincs modul owner → Win10/11-ben normális (scheduler/internal thunk).
                if (!owner)
                    continue;

                // 3) Modul nincs whitelisten → gyanús
                if (!IsModuleWhitelisted(owner->name))
                {
                    fwprintf(
                        f,
                        AY_OBFUSCATE(L"[ANTIHOOK] THREAD-START in non-whitelisted module: "
                        L"TID=%lu START=0x%p MOD=%s PATH=%s\n"),
                        static_cast<unsigned long>(te.th32ThreadID),
                        startAddr,
                        owner->name.c_str(),
                        owner->path.c_str()
                    );
                    foundSuspicious = true;
                }

            } while (Thread32Next(snap, &te));
        }

        CloseHandle(snap);

        fclose(f);

        if (foundSuspicious)
        {
            IXAC_ReportCheat();
            Sleep(10000);
            TerminateProcess(GetCurrentProcess(), 0xC0DE);
        }
    }

} // namespace AntiHook::ThreadScanner
