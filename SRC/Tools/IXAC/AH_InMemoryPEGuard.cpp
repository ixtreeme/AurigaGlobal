#include <windows.h>
#include "AH_Core.h"
#include "AH_InMemoryPEGuard.h"

bool IsInMemoryPe(BYTE* base)
{
    return (base[0] == 'M' && base[1] == 'Z');
}

void ScanForInMemoryPE()
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);

    BYTE* addr = (BYTE*)si.lpMinimumApplicationAddress;
    BYTE* end = (BYTE*)si.lpMaximumApplicationAddress;

    while (addr < end)
    {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(addr, &mbi, sizeof(mbi)))
            break;

        if (mbi.Type == MEM_PRIVATE &&
            (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)))
        {
            BYTE* base = (BYTE*)mbi.BaseAddress;
            if (IsInMemoryPe(base))
            {
                // guaranteed HLBOT / in-memory DLL loader
                IXAC_ReportCheat();
                TerminateProcess(GetCurrentProcess(), 0xD1E);  // "DIE"
            }
        }

        addr += mbi.RegionSize ? mbi.RegionSize : 0x1000;
    }
}
