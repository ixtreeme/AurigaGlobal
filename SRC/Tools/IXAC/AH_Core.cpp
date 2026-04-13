#include "AH_Core.h"
#include <tlhelp32.h>
#include <psapi.h>
#include <unordered_set>
#include <cstdio>
#include <cstring>

#include "IXAC_Net.h"

#pragma comment(lib, "psapi.lib")



bool IXAC_ReportCheat()
{
   // IXAC_LOG("IXAC_ReportCheat CALLED");
    const std::string logPath{AY_OBFUSCATE("UserData/AC/_log.txt")};

    IXAC::Net::OnCheatDetected(logPath);
    return true;
}



namespace
{
    NtQuerySystemInformation_t g_pNtQuerySystemInformation = nullptr;
    NtQueryInformationThread_t g_pNtQueryInformationThread = nullptr;

    // ========= WHITELIST (tedd bele a saját listádat) =========
    std::unordered_set<std::wstring> g_ModuleWhitelist =
    {
        OBF_W(L"userinterface.exe"),
        OBF_W(L"nahimicsvc64.exe"),
        OBF_W(L"audiodg.exe"),
        OBF_W(L"secocl64.exe"),
        OBF_W(L"dcv2.exe"),
        OBF_W(L"widgetboard.exe"),
        OBF_W(L"WidgetBoard.exe"),
        OBF_W(L"nvsphelper64.exe"),
        OBF_W(L"ixac.dll"),
        OBF_W(L"libcurl.dll"),
        OBF_W(L"python27.dll"),
        OBF_W(L"devil.dll"),
        OBF_W(L"speedtreert.dll"),
        OBF_W(L"granny2.dll"),
        OBF_W(L"igc64.dll"),
        OBF_W(L"d3d12.dll"),
        OBF_W(L"d3d12core.dll"),
        OBF_W(L"d3d9on12.dll"),
        OBF_W(L"d3dx10_43.dll"),
        OBF_W(L"d3dx9_43.dll"),
        OBF_W(L"d3d9.dll"),
        OBF_W(L"dxcore.dll"),
        OBF_W(L"dsound.dll"),
        OBF_W(L"dxgi.dll"),
        OBF_W(L"lz4.dll"),
        OBF_W(L"xxhash.dll"),
        OBF_W(L"graphics-hook64.dll"),
        OBF_W(L"kernel32.dll"),
        OBF_W(L"ntdll.dll"),
        OBF_W(L"user32.dll"),
        OBF_W(L"gdi32.dll"),
        OBF_W(L"gdi32full.dll"),
        OBF_W(L"gdiplus.dll"),
        OBF_W(L"win32u.dll"),
        OBF_W(L"msvcp140.dll"),
        OBF_W(L"vcruntime140.dll"),
        OBF_W(L"msvcrt.dll"),
        OBF_W(L"inputhost.dll"),
        OBF_W(L"combase.dll"),
        OBF_W(L"ucrtbase.dll"),
        OBF_W(L"atiumd64.dll"),
        OBF_W(L"atiumd6a.dll"),
        OBF_W(L"atiu9p64.dll"),
        OBF_W(L"nvapi64.dll"),
        OBF_W(L"mswsock.dll"),
        OBF_W(L"dinput8.dll"),
        OBF_W(L"amdihk64.dll"),
        OBF_W(L"dnsapi.dll"),
        OBF_W(L"audioses.dll"),
        OBF_W(L"crypt32.dll"),
        OBF_W(L"kernelbase.dll"),
        OBF_W(L"iphlpapi.dll"),
        OBF_W(L"nvd3dumx.dll"),
        OBF_W(L"nvgpucomp64.dll"),
        OBF_W(L"uxtheme.dll"),
        OBF_W(L"ws2_32.dll"),
        OBF_W(L"dwmapi.dll"),

        OBF_W(L"python27_d.dll"),
        OBF_W(L"d3dx10d_43.dll"),
        OBF_W(L"d3dx9d_43.dll"),

        OBF_W(L"ucrtbased.dll"),
        OBF_W(L"nvwgf2umx.dll"),
        OBF_W(L"igd12um64xel.dll"),
        OBF_W(L"igd12dxva64.dll"),
        OBF_W(L"msvcp_win.dll"),
        OBF_W(L"igdgmm64.dll"),
        OBF_W(L"coremessaging.dll"),
        OBF_W(L"winmm.dll"),
        OBF_W(L"nvspcap64.dll"),
        OBF_W(L"rtsshooks64.dll"),
        OBF_W(L"coremessaging.dll"),
        OBF_W(L"msctf.dll"),
        OBF_W(L"amdxn64.dll"),
        OBF_W(L"igdusc64.dll"),
        OBF_W(L"igdumdim64.dll"),
        OBF_W(L"igdumd64.dll"),
        OBF_W(L"textinputframework.dll"),
        OBF_W(L"igd9trinity64.dll"),
        OBF_W(L"nvdlistx.dll"),
        OBF_W(L"nvd3dumx_cfg.dll"),
        OBF_W(L"medal-hook64.dll"),
        OBF_W(L"coreuicomponents.dll"),
        OBF_W(L"aclayers.dll"),
        OBF_W(L"discord-rpc.dll"),
        OBF_W(L"uiautomationcore.dll"),
        OBF_W(L"apphelp.dll"),
        OBF_W(L"rpcrt4.dll"),
        OBF_W(L"ole32.dll"),
        OBF_W(L"oleaut32.dll"),
        OBF_W(L"game_detour_64.dll"),
        OBF_W(L"mmdevapi.dll"),
        OBF_W(L"imagehlp.dll"),
        OBF_W(L"imm32.dll"),
        OBF_W(L"ebehmoni.dll"),
        OBF_W(L"phoneexperiencehost.dll"),
        OBF_W(L"phoneexperiencehost.exe"),
        
        
    };

    AntiHook::Core::SectionInfo g_TextSec{ nullptr, 0 };
    uint32_t g_TextHash = 0;
}

// =======================================================================
// =======================   IMPLEMENTATION   ============================
// =======================================================================

namespace AntiHook::Core
{
    uint32_t Fnv1a(const uint8_t* data, size_t len)
    {
        uint32_t h = 2166136261u;
        for (size_t i = 0; i < len; ++i)
        {
            h ^= data[i];
            h *= 16777619u;
        }
        return h;
    }

    const SectionInfo& GetTextSection() { return g_TextSec; }
    void SetTextSection(const SectionInfo& s) { g_TextSec = s; }

    uint32_t GetTextExpectedHash() { return g_TextHash; }
    void SetTextExpectedHash(uint32_t h) { g_TextHash = h; }

    std::wstring ToLower(const std::wstring& in)
    {
        std::wstring out = in;
        for (auto& c : out) c = towlower(c);
        return out;
    }

    bool IsModuleWhitelisted(const std::wstring& name)
    {
        return g_ModuleWhitelist.contains(ToLower(name));
    }

    std::wstring GetProcessImageName(DWORD pid)
    {
        wchar_t buf[MAX_PATH]{ 0 };
        DWORD size = MAX_PATH;

        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!h) return L"";

        QueryFullProcessImageNameW(h, 0, buf, &size);
        CloseHandle(h);
        return buf;
    }

    // ================= MODULE MAP ===================
    std::vector<ModuleInfo> BuildModuleMap()
    {
        std::vector<ModuleInfo> out;

        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
        if (snap == INVALID_HANDLE_VALUE)
            return out;

        MODULEENTRY32W me;
        me.dwSize = sizeof(me);

        if (Module32FirstW(snap, &me))
        {
            do {
                ModuleInfo m;
                m.base = (uintptr_t)me.modBaseAddr;
                m.end = m.base + me.modBaseSize;
                m.name = me.szModule;
                m.path = me.szExePath;
                out.push_back(m);
            } while (Module32NextW(snap, &me));
        }

        CloseHandle(snap);
        return out;
    }

    void CheckLoadedModules()
    {
        auto mods = BuildModuleMap();
        for (auto& m : mods)
        {
            if (!IsModuleWhitelisted(m.name))
            {
                // LOG, majd letilthatod
                // fwprintf(stderr, L"[ANTIHOOK] Suspicious module: %s (%s)\n", m.name.c_str(), m.path.c_str());
            }
        }
    }

    // ================= NT resolving =====================

    bool ResolveNt()
    {
        if (g_pNtQuerySystemInformation)
            return true;

        HMODULE ntdll = GetModuleHandleW(AY_OBFUSCATE(L"ntdll.dll"));
        if (!ntdll) return false;

        g_pNtQuerySystemInformation =
            (NtQuerySystemInformation_t)GetProcAddress(ntdll, AY_OBFUSCATE("NtQuerySystemInformation"));

        return g_pNtQuerySystemInformation != nullptr;
    }

    bool ResolveNtThread()
    {
        if (g_pNtQueryInformationThread)
            return true;

        HMODULE ntdll = GetModuleHandleW(AY_OBFUSCATE(L"ntdll.dll"));
        if (!ntdll) return false;

        g_pNtQueryInformationThread =
            (NtQueryInformationThread_t)GetProcAddress(ntdll, AY_OBFUSCATE("NtQueryInformationThread"));

        return g_pNtQueryInformationThread != nullptr;
    }

    NtQuerySystemInformation_t GetNtQuerySystemInformation()
    {
        return g_pNtQuerySystemInformation;
    }

    NtQueryInformationThread_t GetNtQueryInformationThread()
    {
        return g_pNtQueryInformationThread;
    }

}
