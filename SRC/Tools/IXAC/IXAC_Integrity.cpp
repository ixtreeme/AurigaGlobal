// IXAC_Integrity.cpp – ezt tedd az IXAC.dll projektedbe
#include <windows.h>
#include <cstdint>
#include <cstring>

namespace IXAC
{
    // Egyszerû FNV-1a 32 bites hash
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

    struct TextRegionInfo
    {
        uint8_t* base;
        size_t   size;
    };

    static bool GetSelfTextRegion(TextRegionInfo& outInfo)
    {
        HMODULE hSelf = nullptr;
        if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&GetSelfTextRegion),
            &hSelf))
        {
            return false;
        }

        auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(hSelf);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE)
            return false;

        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(
            reinterpret_cast<uint8_t*>(hSelf) + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE)
            return false;

        auto* section = IMAGE_FIRST_SECTION(nt);
        for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section)
        {
            if (std::memcmp(section->Name, ".text", 5) == 0)
            {
                outInfo.base = reinterpret_cast<uint8_t*>(hSelf) + section->VirtualAddress;
                outInfo.size = section->Misc.VirtualSize;
                return true;
            }
        }

        return false;
    }

    // Ezt exportáljuk a kliensnek
    extern "C" __declspec(dllexport)
        bool __stdcall IXAC_GetIntegrityBaseline(uintptr_t* outModuleBase,
            size_t* outTextSize,
            uint32_t* outTextHash)
    {
        if (!outModuleBase || !outTextSize || !outTextHash)
            return false;

        TextRegionInfo info{};
        if (!GetSelfTextRegion(info))
            return false;

        HMODULE hSelf = nullptr;
        if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&IXAC_GetIntegrityBaseline),
            &hSelf))
        {
            return false;
        }

        *outModuleBase = reinterpret_cast<uintptr_t>(hSelf);
        *outTextSize = info.size;
        *outTextHash = Fnv1a32(info.base, info.size);
        return true;
    }
}
