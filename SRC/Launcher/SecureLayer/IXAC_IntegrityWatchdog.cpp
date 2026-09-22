#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "obfuscate.h"
#include "ProcessScanner.h"

static uintptr_t g_IxacModuleBase = 0;
static size_t    g_IxacTextSize = 0;
static uint32_t  g_IxacTextHash = 0;

static size_t    g_IxacPeHeaderSize = 0;
static uint32_t  g_IxacPeHeaderHash = 0;

// Ugyanaz az FNV-1a, mint a DLL-ben
static uint32_t Fnv1a32(const uint8_t* data, size_t size)
{
    uint32_t hash = 0x811C9DC5u;
    for (size_t i = 0; i < size; ++i)
    {
        hash ^= data[i];
        hash *= 0x01000193u;
    }
    return hash;
}

static bool ComputePeHeaderHash(HMODULE hMod, size_t& outSize, uint32_t& outHash)
{
    if (!hMod)
        return false;

    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(hMod);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) // "MZ"
        return false;

    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(
        reinterpret_cast<uint8_t*>(hMod) + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) // "PE\0\0"
        return false;

    // section headerek kezdete
    auto* firstSection = IMAGE_FIRST_SECTION(nt);
    auto* endSection = firstSection + nt->FileHeader.NumberOfSections;

    uint8_t* start = reinterpret_cast<uint8_t*>(hMod);          // modul eleje (DOS header kezdete)
    uint8_t* end = reinterpret_cast<uint8_t*>(endSection);

    if (end <= start)
        return false;

    size_t size = static_cast<size_t>(end - start);

    outSize = size;
    outHash = Fnv1a32(start, size);
    return true;
}

static bool GetModuleTextRegion(HMODULE hMod, uint8_t*& outBase, size_t& outSize)
{
    if (!hMod)
        return false;

    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(hMod);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        return false;

    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(
        reinterpret_cast<uint8_t*>(hMod) + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
        return false;

    auto* section = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section)
    {
        if (std::memcmp(section->Name, ".text", 5) == 0)
        {
            outBase = reinterpret_cast<uint8_t*>(hMod) + section->VirtualAddress;
            outSize = section->Misc.VirtualSize;
            return true;
        }
    }

    return false;
}

static void OnIxacTampered()
{
    MessageBoxA(nullptr,
        AY_OBFUSCATE("IXAC integrity check FAILED.\n"),
        AY_OBFUSCATE("IXAC Anti-Cheat"),
        MB_ICONERROR | MB_OK);

    TerminateProcess(GetCurrentProcess(), 0);
}

static DWORD WINAPI IxacIntegrityThread(LPVOID)
{
    while (true)
    {
        HMODULE hIxac = GetModuleHandleW(AY_OBFUSCATE(L"IXAC.dll"));
        if (!hIxac)
        {
            OnIxacTampered();
            return 0;
        }

        if (reinterpret_cast<uintptr_t>(hIxac) != g_IxacModuleBase)
        {
            OnIxacTampered();
            return 0;
        }

        uint8_t* textBase = nullptr;
        size_t   textSize = 0;
        if (!GetModuleTextRegion(hIxac, textBase, textSize))
        {
            OnIxacTampered();
            return 0;
        }

        if (textSize != g_IxacTextSize)
        {
            OnIxacTampered();
            return 0;
        }

        MEMORY_BASIC_INFORMATION mbiText{};
        if (!VirtualQuery(textBase, &mbiText, sizeof(mbiText)))
        {
            OnIxacTampered();
            return 0;
        }

        DWORD prot = mbiText.Protect & 0xFF;

        if (prot == PAGE_EXECUTE_READWRITE || prot == PAGE_EXECUTE_WRITECOPY)
        {
            OnIxacTampered();
            return 0;
        }

        if (!(prot & (PAGE_EXECUTE | PAGE_EXECUTE_READ)))
        {
            OnIxacTampered();
            return 0;
        }

        {
            size_t   curPeSize = 0;
            uint32_t curPeHash = 0;
            if (!ComputePeHeaderHash(hIxac, curPeSize, curPeHash))
            {
                OnIxacTampered();
                return 0;
            }

            if (curPeSize != g_IxacPeHeaderSize || curPeHash != g_IxacPeHeaderHash)
            {
                OnIxacTampered();
                return 0;
            }
        }

        uint32_t currentHash = Fnv1a32(textBase, textSize);
        if (currentHash != g_IxacTextHash)
        {
            OnIxacTampered();
            return 0;
        }

        Sleep(1500);
    }
}


bool InitIxacIntegrityWatchdog()
{
    HMODULE hIxac = LoadLibraryW(AY_OBFUSCATE(L"IXAC.dll"));
    if (!hIxac)
        return false;

    using IXAC_GetIntegrityBaseline_t =
        bool(__stdcall*)(uintptr_t*, size_t*, uint32_t*);

    auto pFn = reinterpret_cast<IXAC_GetIntegrityBaseline_t>(
        GetProcAddress(hIxac, AY_OBFUSCATE("IXAC_GetIntegrityBaseline")));
    if (!pFn)
        return false;

    if (!pFn(&g_IxacModuleBase, &g_IxacTextSize, &g_IxacTextHash))
        return false;

    if (!ComputePeHeaderHash(hIxac, g_IxacPeHeaderSize, g_IxacPeHeaderHash))
        return false;

    HANDLE hThread = CreateThread(nullptr, 0, IxacIntegrityThread, nullptr, 0, nullptr);
    if (!hThread)
        return false;

    CloseHandle(hThread);
    return true;
}
