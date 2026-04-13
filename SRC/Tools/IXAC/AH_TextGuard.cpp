#include "AH_TextGuard.h"
#include <stdexcept>
#include <cstring>
#include <cstdio>

namespace
{
    AntiHook::Core::SectionInfo FindTextSection()
    {
        HMODULE mod = GetModuleHandleW(nullptr);
        if (!mod)
            throw std::runtime_error(AY_OBFUSCATE("GetModuleHandleW(nullptr) failed"));

        auto* dos = reinterpret_cast<PIMAGE_DOS_HEADER>(mod);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE)
            throw std::runtime_error(AY_OBFUSCATE("Invalid DOS header"));

        auto* nt = reinterpret_cast<PIMAGE_NT_HEADERS>(
            reinterpret_cast<uint8_t*>(mod) + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE)
            throw std::runtime_error(AY_OBFUSCATE("Invalid NT header"));

        PIMAGE_SECTION_HEADER sec = IMAGE_FIRST_SECTION(nt);
        for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i)
        {
            if (std::strncmp(reinterpret_cast<char*>(sec[i].Name), ".text", 5) == 0)
            {
                AntiHook::Core::SectionInfo info{};
                info.base = reinterpret_cast<uint8_t*>(mod) + sec[i].VirtualAddress;
                info.size = sec[i].Misc.VirtualSize;
                return info;
            }
        }
        throw std::runtime_error(AY_OBFUSCATE("Could not find .text section"));
    }

    void ProtectText(const AntiHook::Core::SectionInfo& s)
    {
        DWORD oldProt = 0;
        if (!VirtualProtect(s.base, s.size, PAGE_EXECUTE_READ, &oldProt))
            throw std::runtime_error(AY_OBFUSCATE("VirtualProtect(PAGE_EXECUTE_READ) failed"));
    }
}

namespace AntiHook::TextGuard
{
    void Init()
    {
        using namespace AntiHook::Core;

        SectionInfo s = FindTextSection();
        ProtectText(s);

        const uint32_t hash = Fnv1a(s.base, s.size);
        SetTextSection(s);
        SetTextExpectedHash(hash);
    }

    bool Verify(const char* logFile)
    {
        using namespace AntiHook::Core;

        const SectionInfo& s = GetTextSection();
        if (!s.base || !s.size)
            return true; // nincs baseline → inkább ne lőjük ki
        const uint32_t expected = GetTextExpectedHash();
        const uint32_t now = Fnv1a(s.base, s.size);
        if (now == expected)
            return true;

        if (logFile)
        {
            FILE* f = std::fopen(logFile, "a");
            if (f)
            {
                std::fprintf(
                    f,
                    AY_OBFUSCATE("[ANTIHOOK] .text hash mismatch detected (expected=0x%08X, actual=0x%08X)\n"),
                    expected,
                    now);
                std::fclose(f);
            }
        }

        return false;
    }
}
