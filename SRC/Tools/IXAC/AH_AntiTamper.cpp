#include "AH_AntiTamper.h"
#include "AH_Core.h"

#include <windows.h>
#include <vector>
#include <string>
#include <cstdio>
#include <cstring>


#define LOG_SAFE(...) do { FILE* ff = fopen(AY_OBFUSCATE("UserData/AC/_log.txt"),"a"); if(ff){fprintf(ff,__VA_ARGS__); fclose(ff);} } while(0)


namespace
{
    using AntiHook::Core::ModuleInfo;
    using AntiHook::Core::Fnv1a;
    using AntiHook::Core::ToLower;
    using AntiHook::Core::BuildModuleMap;
    using AntiHook::Core::IsModuleWhitelisted;

    struct TextRegion
    {
        std::wstring name;
        uintptr_t    moduleBase;
        uint8_t* textBase;
        size_t       textSize;
        uint32_t     hash;
    };

    std::vector<TextRegion> g_TextRegions;

    bool GetTextSectionFromModule(uintptr_t base, uint8_t*& outBase, size_t& outSize)
    {
        if (!base)
            return false;

        auto* mod = reinterpret_cast<uint8_t*>(base);

        auto* dos = reinterpret_cast<PIMAGE_DOS_HEADER>(mod);
        if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE)
            return false;

        auto* nt = reinterpret_cast<PIMAGE_NT_HEADERS>(mod + dos->e_lfanew);
        if (!nt || nt->Signature != IMAGE_NT_SIGNATURE)
            return false;

        PIMAGE_SECTION_HEADER sec = IMAGE_FIRST_SECTION(nt);
        for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i)
        {
            if (std::strncmp(reinterpret_cast<char*>(sec[i].Name), AY_OBFUSCATE(".text"), 5) == 0)
            {
                outBase = mod + sec[i].VirtualAddress;
                outSize = sec[i].Misc.VirtualSize;
                if (!outSize)
                    outSize = sec[i].SizeOfRawData;
                return (outBase != nullptr && outSize > 0);
            }
        }

        return false;
    }

    bool IsTextHashProtected(const std::wstring& name)
    {
        std::wstring lower = ToLower(name);

        static const wchar_t* kSkipList[] = {
            AY_OBFUSCATE(L"widgetboard.exe"),
            AY_OBFUSCATE(L"WidgetBoard.exe"),
            AY_OBFUSCATE(L"nvsphelper64.exe"),
            AY_OBFUSCATE(L"ixac.dll"),
            AY_OBFUSCATE(L"libcurl.dll"),
            AY_OBFUSCATE(L"dsound.dll"),
			AY_OBFUSCATE(L"dxcore.dll"),
			AY_OBFUSCATE(L"d3d9.dll"),
			AY_OBFUSCATE(L"d3d10.dll"),
        	AY_OBFUSCATE(L"d3dx9_43.dll"),
			AY_OBFUSCATE(L"d3dx10_43.dll"),
			AY_OBFUSCATE(L"d3d12core.dll"),
			AY_OBFUSCATE(L"d3d9on12.dll"),
			AY_OBFUSCATE(L"d3d12.dll"),
			AY_OBFUSCATE(L"igc64.dll"),
            AY_OBFUSCATE(L"dxgi.dll"),
            AY_OBFUSCATE(L"opengl32.dll"),
            AY_OBFUSCATE(L"nvapi64.dll"),
            AY_OBFUSCATE(L"nvwgf2umx.dll"),
            AY_OBFUSCATE(L"amdxx64.dll"),
            AY_OBFUSCATE(L"atidxx64.dll"),
            AY_OBFUSCATE(L"igd10iumd64.dll"),
            AY_OBFUSCATE(L"user32.dll"),
            AY_OBFUSCATE(L"win32u.dll"),
            AY_OBFUSCATE(L"kernelbase.dll"),
            AY_OBFUSCATE(L"rtsshooks64.dll"),
            AY_OBFUSCATE(L"coremessaging.dll"),
            AY_OBFUSCATE(L"nvspcap64.dll"),
            AY_OBFUSCATE(L"igd12dxva64.dll"),
            AY_OBFUSCATE(L"msctf.dll"),
            AY_OBFUSCATE(L"amdxn64.dll"),
        	AY_OBFUSCATE(L"igdusc64.dll"),
        	AY_OBFUSCATE(L"igdumdim64.dll"),
        	AY_OBFUSCATE(L"igdumd64.dll"),
        	AY_OBFUSCATE(L"xxhash.dll"),
        	AY_OBFUSCATE(L"textinputframework.dll"),
        	AY_OBFUSCATE(L"igd9trinity64.dll"),
        	AY_OBFUSCATE(L"nvdlistx.dll"),
        	AY_OBFUSCATE(L"nvd3dumx_cfg.dll"),
        	AY_OBFUSCATE(L"medal-hook64.dll"),
        	AY_OBFUSCATE(L"coreuicomponents.dll"),
        	AY_OBFUSCATE(L"aclayers.dll"),
        	AY_OBFUSCATE(L"apphelp.dll"),
        	AY_OBFUSCATE(L"oleaut32.dll"),
        	AY_OBFUSCATE(L"game_detour_64.dll"),
        	AY_OBFUSCATE(L"mmdevapi.dll"),
        	AY_OBFUSCATE(L"imagehlp.dll"),
        	AY_OBFUSCATE(L"imm32.dll"),
        };

        for (auto s : kSkipList)
        {
            if (_wcsicmp(lower.c_str(), s) == 0)
                return false; // NEM védjük hash-sel
        }

        return true; 
    }
}

// ======================================================================
//  PUBLIC API
// ======================================================================

namespace AntiHook::AntiTamper
{
    void Init()
    {
        g_TextRegions.clear();

        const auto mods = BuildModuleMap();

        for (const auto& m : mods)
        {
            if (!IsModuleWhitelisted(m.name))
                continue;
            
            if (!IsTextHashProtected(m.name))
            {
                continue;
            }

            uint8_t* textBase = nullptr;
            size_t   textSize = 0;

            if (!GetTextSectionFromModule(m.base, textBase, textSize)) {
                LOG_SAFE(AY_OBFUSCATE("GetTextSectionFromModule failed: %S\n"), m.name.c_str());
                continue;
            }


            TextRegion tr;
            tr.name = m.name;
            tr.moduleBase = m.base;
            tr.textBase = textBase;
            tr.textSize = textSize;
            tr.hash = Fnv1a(textBase, textSize);

            DWORD oldProt = 0;
            if (!VirtualProtect(textBase, textSize, PAGE_EXECUTE_READ, &oldProt))
            {
                LOG_SAFE(AY_OBFUSCATE("VirtualProtect failed [%S] base=0x%p size=%llu GetLastError=%u\n"),
                    m.name.c_str(), textBase, (unsigned long long)textSize, GetLastError());
            }

            g_TextRegions.push_back(tr);
        }
    }

    void Scan(const char* logFile)
    {
        if (g_TextRegions.empty())
            return;

        FILE* f = std::fopen(logFile, "a");
        if (!f)
            return;


        bool tampered = false;

        for (const auto& tr : g_TextRegions)
        {
            uint32_t now = Fnv1a(tr.textBase, tr.textSize);
            if (now != tr.hash)
            {
                std::fwprintf(
                    f,
                    AY_OBFUSCATE(L"[ANTIHOOK] TEXT HASH MISMATCH: %s base=0x%p text=0x%p size=0x%Ix\n"),
                    tr.name.c_str(),
                    reinterpret_cast<void*>(tr.moduleBase),
                    tr.textBase,
                    static_cast<size_t>(tr.textSize)
                );
                tampered = true;
            }
        }

        std::fclose(f);

        if (tampered)
        {
            IXAC_ReportCheat();
        	TerminateProcess(GetCurrentProcess(), 0x4E54); // "NT"
        }
    }

}
