#include "AH_ExceptionGuard.h"
#include "AH_Core.h"
#include "AH_Util.h"

#include <windows.h>
#include <cstdio>
#include <vector>
#include <atomic>


namespace
{
    using AntiHook::Core::ModuleInfo;
    using AntiHook::Core::BuildModuleMap;
    using AntiHook::Core::ToLower;
    using AntiHook::Core::IsModuleWhitelisted;

    std::atomic<bool> g_Initialized{ false };
    PVOID             g_VehHandle = nullptr;

    // Log helper – próbál mindig ugyanabba az AC mappába írni.
    void LogException(const char* fmt, ...)
    {
        FILE* f = std::fopen(LOG_FILE, "a");
        if (!f)
            return;

        SYSTEMTIME st;
        GetLocalTime(&st);
        std::fprintf(f, "[%04d-%02d-%02d %02d:%02d:%02d] ",
            st.wYear, st.wMonth, st.wDay,
            st.wHour, st.wMinute, st.wSecond);

        va_list ap;
        va_start(ap, fmt);
        std::vfprintf(f, fmt, ap);
        va_end(ap);

        std::fprintf(f, "\n");
        std::fclose(f);
    }

    const ModuleInfo* FindModuleForAddress(const std::vector<ModuleInfo>& mods,
        uintptr_t addr)
    {
        for (const auto& m : mods)
        {
            if (addr >= m.base && addr < m.end)
                return &m;
        }
        return nullptr;
    }

    bool IsRwExecutable(DWORD prot)
    {
        return (prot & PAGE_EXECUTE_READWRITE) ||
            (prot & PAGE_EXECUTE_WRITECOPY);
    }

    LONG NTAPI VehHandler(PEXCEPTION_POINTERS info)
    {
        if (!info || !info->ExceptionRecord)
            return EXCEPTION_CONTINUE_SEARCH;

        DWORD code = info->ExceptionRecord->ExceptionCode;
        void* addr = info->ExceptionRecord->ExceptionAddress;

        // Csak egyszer dolgozzuk fel, hogy ne legyen rekurzív káosz:
        static thread_local bool inHandler = false;
        if (inHandler)
            return EXCEPTION_CONTINUE_SEARCH;
        inHandler = true;

        // Alap modul-map az aktuális processre
        std::vector<ModuleInfo> mods = BuildModuleMap();
        const ModuleInfo* owner = nullptr;

        if (addr)
        {
            owner = FindModuleForAddress(mods, reinterpret_cast<uintptr_t>(addr));
        }

        // Alap log infó
        const wchar_t* modName = AY_OBFUSCATE(L"(no module)");
        const wchar_t* modPath = AY_OBFUSCATE(L"");
        if (owner)
        {
            modName = owner->name.c_str();
            modPath = owner->path.c_str();
        }

        // Döntés, hogy mikor tekintjük cheat-gyanúnak
        bool suspicious = false;

        switch (code)
        {
        case STATUS_DATATYPE_MISALIGNMENT:
            // Ez tipikusan hibás shellcode / detour / trampoline injekció.
            suspicious = true;
            LogException(
                AY_OBFUSCATE("[ANTIHOOK][EXC] STATUS_DATATYPE_MISALIGNMENT at %p (module: %S, path: %S)"),
                addr, modName, modPath);
            break;

        case STATUS_ILLEGAL_INSTRUCTION:
        case STATUS_PRIVILEGED_INSTRUCTION:
            // CPU utasítás, amit normál kód soha nem használ itt.
            suspicious = true;
            LogException(
                AY_OBFUSCATE("[ANTIHOOK][EXC] ILLEGAL/PRIVILEGED INSTRUCTION at %p (module: %S, path: %S)"),
                addr, modName, modPath);
            break;

        case STATUS_ACCESS_VIOLATION:
        {
            // Nézzük meg, hogy az IP egy privát RWX régióban van-e (shellcode thread/yield).
            MEMORY_BASIC_INFORMATION mbi{};
            if (VirtualQuery(addr, &mbi, sizeof(mbi)))
            {
                if (mbi.State == MEM_COMMIT &&
                    mbi.Type == MEM_PRIVATE &&
                    IsRwExecutable(mbi.Protect))
                {
                    suspicious = true;
                    LogException(
                        AY_OBFUSCATE("[ANTIHOOK][EXC] ACCESS_VIOLATION in PRIVATE RWX region at %p (prot=0x%08X, module: %S, path: %S)"),
                        addr, mbi.Protect, modName, modPath);
                }
            }
            break;
        }

        default:
            // Egyéb kivételeket hagyjuk a normál handlernek
            break;
        }

        // Ha modulhoz tartozik, és nem whitelistes modul → még egy plusz gyanújel.
        // (Pl. valami idegen DLL, ami hibás trampoline-t futtat.)
        if (!suspicious && owner && !IsModuleWhitelisted(owner->name))
        {
            // Nem akarjuk minden 3rd party overlay-t instant lelőni,
            // ezért ezt inkább csak logoljuk, nem lövünk azonnal.
            LogException(
                AY_OBFUSCATE("[ANTIHOOK][EXC] Exception 0x%08X at %p in non-whitelisted module %S (%S) – ignoring for now"),
                code, addr, modName, modPath);
            
        }

        if (suspicious)
        {
            // Ha ide jutottunk, nagyon valószínű, hogy injektált shellcode/cheat kód okozta a kivételt.
            IXAC_ReportCheat();
            inHandler = false; // mielőtt kilőjük, engedjük el a flaget
            TerminateProcess(GetCurrentProcess(), 0xE0E0); // külön exit code az ExceptionGuard-nak
            return EXCEPTION_EXECUTE_HANDLER; // elvileg már nem jut ide
        }

        inHandler = false;
        return EXCEPTION_CONTINUE_SEARCH;
    }
} // unnamed namespace

namespace AntiHook::ExceptionGuard
{
    void Init()
    {
        if (g_Initialized.exchange(true))
            return;

        g_VehHandle = AddVectoredExceptionHandler(1, VehHandler);
        if (!g_VehHandle)
        {
            g_Initialized.store(false);
            LogException(AY_OBFUSCATE("[ANTIHOOK][EXC] AddVectoredExceptionHandler failed (GLE=%u)"), GetLastError());
        }
        /*else
        {
            LogException(AY_OBFUSCATE("[ANTIHOOK][EXC] ExceptionGuard initialized"));
        }*/
    }

    void Shutdown()
    {
        if (!g_Initialized.exchange(false))
            return;

        if (g_VehHandle)
        {
            RemoveVectoredExceptionHandler(g_VehHandle);
            g_VehHandle = nullptr;
            LogException(AY_OBFUSCATE("[ANTIHOOK][EXC] ExceptionGuard shutdown"));
        }
    }
}
