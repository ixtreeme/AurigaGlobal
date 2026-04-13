#include "AH_SyscallScanner.h"
#include "AH_Core.h"
#include <windows.h>
#include <vector>
#include <string>
#include <cstdio>

namespace
{
    struct SyscallStub
    {
        std::string name;
        void* addr;
        uint32_t    hash;
    };

    std::vector<SyscallStub> g_Syscalls;

    uint32_t HashStubBytes(void* addr)
    {
        if (!addr)
            return 0;

        // első 32 byte-ot hash-eljük
        static const size_t STUB_LEN = 32;
        return AntiHook::Core::Fnv1a(static_cast<uint8_t*>(addr), STUB_LEN);
    }

    void AddSyscall(const char* name)
    {
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        if (!ntdll)
            return;

        FARPROC p = GetProcAddress(ntdll, name);
        if (!p)
            return;

        SyscallStub s;
        s.name = name;
        s.addr = reinterpret_cast<void*>(p);
        s.hash = HashStubBytes(s.addr);
        g_Syscalls.push_back(s);
    }
}

namespace AntiHook::SyscallScanner
{
    void InitSyscallBaseline()
    {
        g_Syscalls.clear();

        AddSyscall(AY_OBFUSCATE("NtOpenProcess"));
        AddSyscall(AY_OBFUSCATE("NtReadVirtualMemory"));
        AddSyscall(AY_OBFUSCATE("NtWriteVirtualMemory"));
        AddSyscall(AY_OBFUSCATE("NtCreateThreadEx"));
        AddSyscall(AY_OBFUSCATE("NtQueueApcThread"));
        AddSyscall(AY_OBFUSCATE("NtResumeThread"));
        AddSyscall(AY_OBFUSCATE("NtSuspendThread"));
    }

    void ScanSyscallStubs(const char* logFile)
    {
        if (g_Syscalls.empty())
            return;

        FILE* f = std::fopen(logFile, "a");
        if (!f)
            return;

        bool tampered = false;

        for (auto& s : g_Syscalls)
        {
            uint32_t now = HashStubBytes(s.addr);
            if (now != s.hash)
            {
                fprintf(
                    f,
                    AY_OBFUSCATE("[ANTIHOOK] Syscall stub tampered: %s (addr=0x%p)\n"),
                    s.name.c_str(),
                    s.addr
                );
                tampered = true;
            }
        }

        fclose(f);

        if (tampered)
        {
            IXAC_ReportCheat();
            TerminateProcess(GetCurrentProcess(), 0x5153); // "QS"
        }
    }
}
