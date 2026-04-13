//#include "AH_ProcessBlacklist.h"
//#include "AH_Core.h"
//#include "AH_Util.h"
//
//#include <windows.h>
//#include <tlhelp32.h>
//#include <psapi.h>
//#include <string>
//#include <vector>
//#include <cstdio>
//#pragma comment(lib, "Version.lib")
//
//// ============================================================
//// AH_ProcessBlacklist – cheat process detektalas
////
//// Kezelt technikak:
////  1) Process nev blacklist  (hlbot.exe, cheatengine stb.)
////  2) PE VersionInfo scan    (bot/hack/cheat kulcsszavak)
////  3) Ablak osztalynev scan  (nem valtozik random cimnel sem)
////  4) Process Hollowing det. (32bit explorer x64 rendszeren,
////                             szulo process anomalia,
////                             PEB image path vs valos path)
////  5) Debugger detektalas
//// ============================================================
//
//namespace
//{
//    static const wchar_t* kBlacklistedProcessNames[] = {
//        L"hlbot.exe", L"hlbot.net.exe", L"hlbotnet.exe",
//        L"cheatengine.exe", L"cheatengine-x86_64.exe",
//        L"cheatengine-x86_64-windowsxp.exe", L"cheatengine-i386.exe",
//        L"x64dbg.exe", L"x32dbg.exe",
//        L"ollydbg.exe", L"ollydbg2.exe",
//        L"windbg.exe",
//        L"ida.exe", L"ida64.exe", L"idaq.exe", L"idaq64.exe",
//        L"scylla.exe", L"scylla_x64.exe", L"scylla_x86.exe",
//        L"processhacker.exe", L"processhacker3.exe",
//        L"extremeinjector.exe", L"xenos.exe", L"xenos64.exe",
//        L"wpe_pro.exe", L"wpe pro.exe",
//        L"metin2bot.exe", L"metin2hack.exe", L"m2bob.exe",
//        L"rawcap.exe", L"packeteditor.exe",
//    };
//
//    static const wchar_t* kBlacklistedWindowTitles[] = {
//        L"hlbot", L"cheat engine", L"x64dbg", L"x32dbg",
//        L"ollydbg", L"process hacker", L"extreme injector",
//        L"xenos injector", L"metin2bot", L"m2bob", L"wpe pro",
//        L"ida pro", L"scylla",
//    };
//
//    static const wchar_t* kBlacklistedClassNames[] = {
//        L"hlbot", L"cheatengine", L"x64dbg", L"ollydbg",
//    };
//
//    static const wchar_t* kCheatKeywords[] = {
//        L"hlbot", L"cheat", L"hack", L"bot", L"aimbot",
//        L"wallhack", L"trainer", L"injector", L"bypass",
//    };
//
//    std::wstring WstrToLower(const std::wstring& s)
//    {
//        std::wstring r = s;
//        for (auto& c : r) c = towlower(c);
//        return r;
//    }
//
//    bool IsBlacklistedProcessName(const std::wstring& nameLow)
//    {
//        for (auto s : kBlacklistedProcessNames)
//            if (_wcsicmp(nameLow.c_str(), s) == 0) return true;
//        return false;
//    }
//
//    bool ContainsBlacklistedTitle(const std::wstring& titleLow)
//    {
//        for (auto s : kBlacklistedWindowTitles)
//            if (titleLow.find(s) != std::wstring::npos) return true;
//        return false;
//    }
//
//    bool IsBlacklistedClassName(const std::wstring& classLow)
//    {
//        for (auto s : kBlacklistedClassNames)
//            if (classLow.find(s) != std::wstring::npos) return true;
//        return false;
//    }
//
//    bool HasCheatVersionInfo(DWORD pid)
//    {
//        wchar_t path[MAX_PATH]{};
//        DWORD size = MAX_PATH;
//        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
//        if (!h) return false;
//        QueryFullProcessImageNameW(h, 0, path, &size);
//        CloseHandle(h);
//
//        DWORD dummy = 0;
//        DWORD verSize = GetFileVersionInfoSizeW(path, &dummy);
//        if (!verSize) return false;
//
//        std::vector<BYTE> data(verSize);
//        if (!GetFileVersionInfoW(path, 0, verSize, data.data()))
//            return false;
//
//        struct LANGCP { WORD lang; WORD cp; };
//        LANGCP* lcp = nullptr;
//        UINT lcpSize = 0;
//        if (!VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation",
//            reinterpret_cast<LPVOID*>(&lcp), &lcpSize) || !lcpSize)
//            return false;
//
//        const wchar_t* kFields[] = {
//            L"FileDescription", L"ProductName", L"CompanyName", L"InternalName"
//        };
//        for (auto field : kFields)
//        {
//            wchar_t sub[256]{};
//            swprintf_s(sub, L"\\StringFileInfo\\%04x%04x\\%s",
//                lcp[0].lang, lcp[0].cp, field);
//            wchar_t* val = nullptr; UINT vs = 0;
//            if (!VerQueryValueW(data.data(), sub,
//                reinterpret_cast<LPVOID*>(&val), &vs) || !val) continue;
//            std::wstring low(val);
//            for (auto& c : low) c = towlower(c);
//            for (auto kw : kCheatKeywords)
//                if (low.find(kw) != std::wstring::npos) return true;
//        }
//        return false;
//    }
//
//    // ---------------------------------------------------------------
//    // Process Hollowing detektalas
//    //
//    // A HLBot technikaja: legitim explorer.exe inditasa SUSPENDED
//    // allapotban, memoria kicserelese, majd thread resume.
//    // Jellemzok:
//    //   A) 32 bites process fut x64 OS-en ahol az igazi pelda 64 bites
//    //   B) PEB.ImagePathName != QueryFullProcessImageName (kicserelve)
//    //   C) A process szulo-PID-je nem userinit.exe / explorer.exe
//    //   D) A process .text szekcio MEM_PRIVATE (nem MEM_IMAGE)
//    // ---------------------------------------------------------------
//
//    // Lekeri hogy egy process 32 bites-e (WOW64 alatt fut-e)
//    bool Is32BitProcess(HANDLE h)
//    {
//        BOOL wow64 = FALSE;
//        IsWow64Process(h, &wow64);
//        return wow64 == TRUE;
//    }
//
//    // Lekeri egy process szulo PID-jet
//    DWORD GetParentPid(DWORD pid)
//    {
//        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
//        if (hSnap == INVALID_HANDLE_VALUE) return 0;
//
//        PROCESSENTRY32W pe{};
//        pe.dwSize = sizeof(pe);
//        DWORD parent = 0;
//
//        if (Process32FirstW(hSnap, &pe))
//        {
//            do {
//                if (pe.th32ProcessID == pid)
//                {
//                    parent = pe.th32ParentProcessID;
//                    break;
//                }
//            } while (Process32NextW(hSnap, &pe));
//        }
//        CloseHandle(hSnap);
//        return parent;
//    }
//
//    // Lekeri egy PID process nevet
//    std::wstring GetProcessName(DWORD pid)
//    {
//        wchar_t path[MAX_PATH]{};
//        DWORD size = MAX_PATH;
//        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
//        if (!h) return {};
//        QueryFullProcessImageNameW(h, 0, path, &size);
//        CloseHandle(h);
//        // Csak a fajlnev
//        std::wstring s(path);
//        auto slash = s.find_last_of(L"\\/");
//        if (slash != std::wstring::npos) s = s.substr(slash + 1);
//        return WstrToLower(s);
//    }
//
//    // Ellenorzi hogy az .exe .text szekcio MEM_PRIVATE-e (hollowing jele)
//    // Normalis esetben MEM_IMAGE kellene legyen
//    bool HasPrivateTextSection(HANDLE hProc, uintptr_t baseAddr)
//    {
//        // A .text szekciora mutat altalaban base + 0x1000
//        // Pontosabb: vegigmegyunk a szekciokon
//        BYTE header[0x1000]{};
//        SIZE_T read = 0;
//        if (!ReadProcessMemory(hProc, reinterpret_cast<LPCVOID>(baseAddr),
//            header, sizeof(header), &read))
//            return false;
//
//        auto* dos = reinterpret_cast<PIMAGE_DOS_HEADER>(header);
//        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
//        if (dos->e_lfanew < 0 || dos->e_lfanew > 0x400) return false;
//
//        auto* nt = reinterpret_cast<PIMAGE_NT_HEADERS>(header + dos->e_lfanew);
//        if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
//
//        PIMAGE_SECTION_HEADER sec = IMAGE_FIRST_SECTION(nt);
//        for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i)
//        {
//            if (strncmp(reinterpret_cast<char*>(sec[i].Name), ".text", 5) != 0)
//                continue;
//
//            uintptr_t textAddr = baseAddr + sec[i].VirtualAddress;
//            MEMORY_BASIC_INFORMATION mbi{};
//            if (!VirtualQueryEx(hProc, reinterpret_cast<LPCVOID>(textAddr),
//                &mbi, sizeof(mbi)))
//                return false;
//
//            // MEM_PRIVATE: a kod kicserelve (hollowing)
//            // MEM_IMAGE:   normalis betoltott PE
//            return mbi.Type == MEM_PRIVATE;
//        }
//        return false;
//    }
//
//    // Fodetektalas: vegigmegy az osszes explorer.exe peldanyon
//    // és megkeresi a hollowolt (HLBot) peldanyt
//    struct HollowResult
//    {
//        bool  found;
//        DWORD pid;
//        wchar_t reason[128];
//    };
//
//    // ---------------------------------------------------------------
//    // Adott nevu process -eket ellenorzi hollowing szempontjabol.
//    // Nem csak explorer.exe – a HLBot svchost.exe, RuntimeBroker.exe
//    // vagy barmi mas legitim Windows process neve mogott is elrejthet.
//    //
//    // Vizsgalt jelek (tobbszoros megerosites – egy jel nem eleg):
//    //   A) 32 bites process fut x64 OS-en (ha az eredeti 64 bites)
//    //   B) Szulo process nem legitim Windows komponens
//    //   C) .text szekcioja MEM_PRIVATE a betoltott modulban
//    //
//    // Ket jel kell a detektalashoz → kevesebb false positive
//    // ---------------------------------------------------------------
//
//    // Legitim Windows process-ok amik sosem lehetnek rosszindulatu szulok
//    bool IsLegitWindowsParent(const std::wstring& nameLow)
//    {
//        static const wchar_t* kLegit[] = {
//            L"winlogon.exe", L"userinit.exe", L"explorer.exe",
//            L"wininit.exe",  L"services.exe", L"smss.exe",
//            L"csrss.exe",    L"lsass.exe",    L"svchost.exe",
//            L"taskhostw.exe",
//        };
//        for (auto s : kLegit)
//            if (_wcsicmp(nameLow.c_str(), s) == 0) return true;
//        return false;
//    }
//
//    // Vizsgalt process nevek – ismert cele: legitim nevu de hollowed
//    static const wchar_t* kHollowTargets[] = {
//        L"explorer.exe",
//        L"svchost.exe",
//        L"RuntimeBroker.exe",
//        L"sihost.exe",
//        L"taskhostw.exe",
//        L"dllhost.exe",
//    };
//
//    HollowResult DetectHollowedExplorer()
//    {
//        HollowResult result{};
//
//        BOOL isOs64 = FALSE;
//        IsWow64Process(GetCurrentProcess(), &isOs64);
//
//        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
//        if (hSnap == INVALID_HANDLE_VALUE) return result;
//
//        PROCESSENTRY32W pe{};
//        pe.dwSize = sizeof(pe);
//
//        if (Process32FirstW(hSnap, &pe))
//        {
//            do {
//                // Csak ismert hollow-celpontokat vizsgalunk
//                bool isTarget = false;
//                for (auto t : kHollowTargets)
//                    if (_wcsicmp(pe.szExeFile, t) == 0) { isTarget = true; break; }
//                if (!isTarget) continue;
//
//                HANDLE hProc = OpenProcess(
//                    PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
//                    FALSE, pe.th32ProcessID);
//                if (!hProc) continue;
//
//                int suspicionScore = 0;
//                wchar_t reasons[256]{};
//
//                // A) 32 bites process x64 OS-en
//                // Explorer, svchost, RuntimeBroker stb. mind 64 bites
//                if (isOs64 && Is32BitProcess(hProc))
//                {
//                    suspicionScore++;
//                    wcscat_s(reasons, L"32bit_on_x64 ");
//                }
//
//                // B) Szulo process anomalia
//                // Ha a szulo nem egy legitim Windows process → gyanus
//                DWORD parentPid = GetParentPid(pe.th32ProcessID);
//                std::wstring parentName = GetProcessName(parentPid);
//
//                if (!parentName.empty() && !IsLegitWindowsParent(parentName))
//                {
//                    suspicionScore++;
//                    wchar_t tmp[128]{};
//                    swprintf_s(tmp, L"bad_parent(%s) ", parentName.c_str());
//                    wcscat_s(reasons, tmp);
//                }
//
//                // C) .text szekcioja MEM_PRIVATE
//                // Ez a legers osebb jel: process hollowing utan a kod
//                // mindig MEM_PRIVATE lesz (nem MEM_IMAGE mint normalis esetben)
//                HMODULE hMod = nullptr;
//                DWORD needed = 0;
//                if (EnumProcessModules(hProc, &hMod, sizeof(hMod), &needed) && hMod)
//                {
//                    if (HasPrivateTextSection(hProc,
//                        reinterpret_cast<uintptr_t>(hMod)))
//                    {
//                        suspicionScore += 2; // Ez onmagaban is elegendo
//                        wcscat_s(reasons, L"private_text ");
//                    }
//                }
//
//                CloseHandle(hProc);
//
//                // Ket jel kell minimum (kiveve private_text ami 2 pont)
//                if (suspicionScore >= 2)
//                {
//                    result.found = true;
//                    result.pid = pe.th32ProcessID;
//                    wcscpy_s(result.reason, reasons);
//                    break;
//                }
//
//            } while (Process32NextW(hSnap, &pe));
//        }
//
//        CloseHandle(hSnap);
//        return result;
//    }
//
//    // Ablak scan HWND callback
//    struct WindowScanCtx {
//        bool    found;
//        wchar_t matchedTitle[256];
//        wchar_t matchedClass[256];
//        DWORD   matchedPid;
//    };
//
//    BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
//    {
//        auto* ctx = reinterpret_cast<WindowScanCtx*>(lParam);
//
//        wchar_t cls[256]{};
//        GetClassNameW(hwnd, cls, 256);
//        if (IsBlacklistedClassName(WstrToLower(cls)))
//        {
//            ctx->found = true;
//            wcsncpy_s(ctx->matchedClass, cls, 255);
//            GetWindowThreadProcessId(hwnd, &ctx->matchedPid);
//            return FALSE;
//        }
//
//        wchar_t title[256]{};
//        if (GetWindowTextW(hwnd, title, 256) && title[0])
//        {
//            if (ContainsBlacklistedTitle(WstrToLower(title)))
//            {
//                ctx->found = true;
//                wcsncpy_s(ctx->matchedTitle, title, 255);
//                GetWindowThreadProcessId(hwnd, &ctx->matchedPid);
//                return FALSE;
//            }
//        }
//        return TRUE;
//    }
//}
//
//namespace AntiHook::ProcessBlacklist
//{
//    bool IsDebuggerAttached()
//    {
//        if (IsDebuggerPresent()) return true;
//
//        using NtQIP_t = NTSTATUS(NTAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);
//        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
//        if (ntdll)
//        {
//            auto NtQIP = reinterpret_cast<NtQIP_t>(
//                GetProcAddress(ntdll, "NtQueryInformationProcess"));
//            if (NtQIP)
//            {
//                HANDLE dbgPort = nullptr;
//                if (NtQIP(GetCurrentProcess(), 7,
//                    &dbgPort, sizeof(dbgPort), nullptr) >= 0
//                    && dbgPort != nullptr)
//                    return true;
//            }
//        }
//
//        BOOL remote = FALSE;
//        CheckRemoteDebuggerPresent(GetCurrentProcess(), &remote);
//        return remote == TRUE;
//    }
//
//    void ScanProcesses(const char* logFile)
//    {
//        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
//        if (hSnap == INVALID_HANDLE_VALUE) return;
//
//        PROCESSENTRY32W pe{};
//        pe.dwSize = sizeof(pe);
//        bool detected = false;
//        DWORD attackerPid = 0;
//
//        if (Process32FirstW(hSnap, &pe))
//        {
//            do {
//                std::wstring nameLow = WstrToLower(pe.szExeFile);
//                bool nameMatch = IsBlacklistedProcessName(nameLow);
//                bool verMatch = !nameMatch && HasCheatVersionInfo(pe.th32ProcessID);
//
//                if (nameMatch || verMatch)
//                {
//                    FILE* f = fopen(logFile, "a");
//                    if (f) {
//                        fwprintf(f,
//                            AY_OBFUSCATE(L"[BLACKLIST] Cheat process:"
//                                L" PID=%lu NAME=%s METHOD=%s\n"),
//                            static_cast<unsigned long>(pe.th32ProcessID),
//                            pe.szExeFile,
//                            nameMatch ? L"PROCESS_NAME" : L"VERSION_INFO");
//                        fclose(f);
//                    }
//                    detected = true;
//                    attackerPid = pe.th32ProcessID;
//                    break;
//                }
//            } while (Process32NextW(hSnap, &pe));
//        }
//        CloseHandle(hSnap);
//
//        if (detected)
//        {
//            IXAC_ReportCheat(AY_OBFUSCATE("BLACKLISTED_PROCESS"), attackerPid);
//            TerminateProcess(GetCurrentProcess(), 0xB1AC);
//        }
//    }
//
//    void ScanWindows(const char* logFile)
//    {
//        WindowScanCtx ctx{};
//        EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&ctx));
//        if (!ctx.found) return;
//
//        FILE* f = fopen(logFile, "a");
//        if (f) {
//            fwprintf(f,
//                AY_OBFUSCATE(L"[BLACKLIST] Cheat window:"
//                    L" PID=%lu CLASS=\"%s\" TITLE=\"%s\"\n"),
//                static_cast<unsigned long>(ctx.matchedPid),
//                ctx.matchedClass, ctx.matchedTitle);
//            fclose(f);
//        }
//        IXAC_ReportCheat(AY_OBFUSCATE("BLACKLISTED_WINDOW"), ctx.matchedPid);
//        TerminateProcess(GetCurrentProcess(), 0xB1AD);
//    }
//
//    void ScanHollowedProcesses(const char* logFile)
//    {
//        auto res = DetectHollowedExplorer();
//        if (!res.found) return;
//
//        FILE* f = fopen(logFile, "a");
//        if (f) {
//            fwprintf(f,
//                AY_OBFUSCATE(L"[BLACKLIST] Hollowed explorer.exe detected:"
//                    L" PID=%lu REASON=%s\n"),
//                static_cast<unsigned long>(res.pid),
//                res.reason);
//            fclose(f);
//        }
//        IXAC_ReportCheat(AY_OBFUSCATE("PROCESS_HOLLOW"), res.pid);
//        TerminateProcess(GetCurrentProcess(), 0xB1AE);
//    }
//
//    void Scan(const char* logFile)
//    {
//        if (IsDebuggerAttached())
//        {
//            FILE* f = fopen(logFile, "a");
//            if (f) { fprintf(f, AY_OBFUSCATE("[BLACKLIST] Debugger detected!\n")); fclose(f); }
//            IXAC_ReportCheat(AY_OBFUSCATE("DEBUGGER_ATTACHED"));
//            TerminateProcess(GetCurrentProcess(), 0xDEB6);
//        }
//
//        ScanProcesses(logFile);
//        ScanWindows(logFile);
//
//        // Process hollowing detektálás – minden 3. korben
//        // (ReadProcessMemory intenziv, nem kell minden iteracioban)
//        static unsigned s_hollowCounter = 0;
//        if (++s_hollowCounter % 3 == 0)
//            ScanHollowedProcesses(logFile);
//    }
//}