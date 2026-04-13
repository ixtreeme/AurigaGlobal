#include "AH_Guard.h"
#include "AH_Core.h"
#include "AH_TextGuard.h"
#include "AH_HandleScanner.h"
#include "AH_RwxScanner.h"
#include "AH_ThreadScanner.h"
#include "AH_SyscallScanner.h"
#include "AH_AntiTamper.h"
#include "AH_ExceptionGuard.h"
//#include "AH_SignerGuard.h"
//#include "AH_DiagnosticSignerDump.h"
#include "AH_InMemoryPEGuard.h"
#include "AH_NoImageTargetingGuard.h"
#include "AH_ThreadHijackGuard.h"
#include "AH_Util.h"

#include "resource.h"

#include <windows.h>
#include <atomic>
#include <thread>
#include <cstdarg>
#include <string>
#include <cstdio>

#include <gdiplus.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

#include "IXAC_SuspiciousModuleHeuristics.h"
#include "AH_PackerGuard.h"
#pragma comment(lib, "gdiplus.lib")




bool LoadSplashImageFromResource(std::vector<unsigned char>& outData)
{
    HRSRC hRes = FindResourceW(g_hDll, MAKEINTRESOURCEW(IDB_SPLASH_IMAGE), MAKEINTRESOURCEW(RT_RCDATA));
    if (!hRes)
        return false;

    HGLOBAL hData = LoadResource(g_hDll, hRes);
    if (!hData)
        return false;

    DWORD size = SizeofResource(g_hDll, hRes);
    if (size == 0)
        return false;

    void* pData = LockResource(hData);
    if (!pData)
        return false;

    outData.assign((unsigned char*)pData, (unsigned char*)pData + size);
    return true;
}


// Splash megjelenítése közvetlenül memóriából – soha nem ír temp fájlt,
// ezzel elkerüljük a "Dropper" AV detektálást.
void ShowEmbeddedSplash(int delayMS)
{
    using namespace Gdiplus;

    // 1. Kép betöltése az erőforrásból
    std::vector<unsigned char> img;
    if (!LoadSplashImageFromResource(img))
        return;

    // 2. IStream létrehozása közvetlenül a memória bufferből
    //    – SHCreateMemStream nem igényel fájlt, csak egy byte tömböt
    IStream* pStream = SHCreateMemStream(
        reinterpret_cast<const BYTE*>(img.data()),
        static_cast<UINT>(img.size()));

    if (!pStream)
        return;

    // 3. GDI+ Bitmap betöltése az IStream-ből (soha nem érinti a lemezt)
    Bitmap* bmp = Bitmap::FromStream(pStream);
    pStream->Release();

    if (!bmp || bmp->GetLastStatus() != Ok)
    {
        delete bmp;
        return;
    }

    int w = static_cast<int>(bmp->GetWidth());
    int h = static_cast<int>(bmp->GetHeight());

    if (w <= 0 || h <= 0)
    {
        delete bmp;
        return;
    }

    // 4. Ablak létrehozása és megjelenítése
    HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST,
        AY_OBFUSCATE(L"STATIC"),
        L"",
        WS_POPUP,
        (GetSystemMetrics(SM_CXSCREEN) - w) / 2,
        (GetSystemMetrics(SM_CYSCREEN) - h) / 2,
        w, h,
        nullptr, nullptr, nullptr, nullptr
    );

    if (!hwnd)
    {
        delete bmp;
        return;
    }

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem    = CreateCompatibleDC(hdcScreen);

    HBITMAP hbmp = NULL;
    bmp->GetHBITMAP(Color(0, 0, 0, 0), &hbmp);
    HGDIOBJ hOld = SelectObject(hdcMem, hbmp);

    POINT ptSrc  = { 0, 0 };
    SIZE  sizeWnd = { w, h };
    POINT ptDst  = {
        (GetSystemMetrics(SM_CXSCREEN) - w) / 2,
        (GetSystemMetrics(SM_CYSCREEN) - h) / 2
    };

    BLENDFUNCTION blend  = {};
    blend.BlendOp        = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat    = AC_SRC_ALPHA;

    UpdateLayeredWindow(hwnd, hdcScreen, &ptDst, &sizeWnd,
                        hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(hdcMem, hOld);
    DeleteObject(hbmp);
    ReleaseDC(NULL, hdcScreen);
    DeleteDC(hdcMem);

    ShowWindow(hwnd, SW_SHOW);
    Sleep(delayMS);
    DestroyWindow(hwnd);
    delete bmp;
}




static std::atomic<bool> g_ImmediateScan{ false };

namespace AntiHook
{
    static std::atomic<bool>      g_Running{ false };
    static std::atomic<ULONGLONG> g_HeartbeatTick{ 0 };
    static DWORD                  g_WatchdogThreadId = 0;
    static DWORD                  g_GuardianThreadId = 0;
    static std::thread            g_WatchdogThread;
    static std::thread            g_GuardianThread;

    


    bool EnsureLogDirectoryExists()
    {
        CreateDirectoryA(AY_OBFUSCATE("UserData"), nullptr);
        CreateDirectoryA(AY_OBFUSCATE("UserData/AC"), nullptr);

        DWORD attr = GetFileAttributesA(AY_OBFUSCATE("UserData/AC"));
        return attr != INVALID_FILE_ATTRIBUTES &&
            (attr & FILE_ATTRIBUTE_DIRECTORY);
    }

  


    void RequestImmediateScan()
    {
        g_ImmediateScan.store(true, std::memory_order_release);
    }


    static void Guardian(unsigned intervalMs)
    {
        g_GuardianThreadId = GetCurrentThreadId();
        ThreadScanner::SetGuardianThreadId(g_GuardianThreadId);

        while (g_Running.load(std::memory_order_acquire))
        {
            DWORD wid = g_WatchdogThreadId;
            DWORD exitCode = STILL_ACTIVE;

            if (wid != 0)
            {
                HANDLE hThread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, wid);
                if (!hThread)
                {
                    IXAC_ReportCheat();
                    TerminateProcess(GetCurrentProcess(), 0xD1F);
                    return;
                }

                if (!GetExitCodeThread(hThread, &exitCode))
                    exitCode = STILL_ACTIVE;

                CloseHandle(hThread);

                if (exitCode != STILL_ACTIVE)
                {
                    IXAC_ReportCheat();
                    TerminateProcess(GetCurrentProcess(), 0xD20);
                    return;
                }
            }

            ULONGLONG last = g_HeartbeatTick.load(std::memory_order_acquire);
            if (last != 0 && wid != 0 && exitCode == STILL_ACTIVE)
            {
                ULONGLONG now = GetTickCount64();
                ULONGLONG maxLag = static_cast<ULONGLONG>(intervalMs) * 7ULL;
                if (maxLag < 5000ULL)
                    maxLag = 5000ULL;

                if (now - last > maxLag)
                {
                    IXAC_ReportCheat();
                    TerminateProcess(GetCurrentProcess(), 0xD1E);
                    return;
                }
            }

            Sleep(intervalMs);
        }
    }

    static void Watchdog(unsigned intervalMs)
    {
        g_WatchdogThreadId = GetCurrentThreadId();
        ThreadScanner::SetWatchdogThreadId(g_WatchdogThreadId);


        if (!Core::ResolveNt())
        {
            AppendLog(AY_OBFUSCATE("[ANTIHOOK] Failed to resolve NtQuerySystemInformation; terminating.\n"));
            IXAC_ReportCheat();
            TerminateProcess(GetCurrentProcess(), 0xD30);
            return;
        }

        if (!Core::ResolveNtThread())
        {
            AppendLog(AY_OBFUSCATE("[ANTIHOOK] Failed to resolve NtQueryInformationThread; terminating.\n"));
            IXAC_ReportCheat();
            TerminateProcess(GetCurrentProcess(), 0xD31);
            return;
        }

        unsigned counter = 0;

        while (g_Running.load(std::memory_order_acquire))
        {
            ++counter;
            g_HeartbeatTick.store(GetTickCount64(), std::memory_order_release);


            if (!TextGuard::Verify(LOG_FILE))
            {
                AppendLog(AY_OBFUSCATE("[ANTIHOOK] TERMINATING CLIENT due to .text integrity failure.\n"));
                IXAC_ReportCheat();
                TerminateProcess(GetCurrentProcess(), 0x7F10);
                return;
            }

            
            HandleScanner::LogSuspiciousProcesses(LOG_FILE);
            Core::CheckLoadedModules();



            // 🔥 ÚJ: heurisztikus COM2 / injektor detektálás
            if (counter % 5 == 0) // pl. minden 3. körben
            {
                using AntiHook::Heuristics::SuspiciousModule;

                auto suspicious = AntiHook::Heuristics::ScanModulesHeuristic(/*scoreThreshold=*/10);

                for (const SuspiciousModule& s : suspicious)
                {
                    // LOG
                    char buf[512];
                    std::snprintf(buf, sizeof(buf),
                        AY_OBFUSCATE("[ANTIHOOK] Heuristic suspicious module: %ls (score=%d, reason=%s)\n"),
                        s.mod.name.c_str(), s.score, s.reason.c_str());
                    AppendLog(buf);

                    // IXAC report – ide te raksz saját event nevet / kódot
                    IXAC_ReportCheat(); // vagy IXAC_ReportCheatWithReason("HEURISTIC_MOD", ...)
                    Sleep(2500);
                    // Választható: azonnali terminálás
                    TerminateProcess(GetCurrentProcess(), 0xC02); // saját error code
                    return;
                }
            }


            
        	RwxScanner::ScanForRwxRegions(LOG_FILE);


            ThreadScanner::ScanThreadsForSuspiciousEip(LOG_FILE);
            ThreadScanner::ScanThreadStartAddresses(LOG_FILE);

            if (counter % 5 == 0)
                SyscallScanner::ScanSyscallStubs(LOG_FILE);

            if (counter % 5 == 0)
                AntiTamper::Scan(LOG_FILE);

            // Themida / VMProtect / WinLicense packer detektálás futó processzekben
            if (counter % 5 == 0)
                AntiHook::PackerGuard::ScanProcessesForPackers(LOG_FILE);

            /*if (counter % 5 == 0)
                SignerGuard::ScanProcesses(LOG_FILE);*/

            if (counter % 10 == 0)
                AntiHook::NoImageTargetingGuard::Scan(LOG_FILE, 3);

           // DumpAllProcessSigners();
            ScanForInMemoryPE();


            if (counter % 8 == 0)
            {
                AntiHook::ThreadHijackGuard::Scan(LOG_FILE);
            }

            Sleep(intervalMs);
        }
    }

    void Start(unsigned intervalMs)
    {
        if (g_Running.exchange(true))
            return;

        ULONG_PTR gdiToken;
        Gdiplus::GdiplusStartupInput gdiSI;
        Gdiplus::GdiplusStartup(&gdiToken, &gdiSI, nullptr);


        ShowEmbeddedSplash( 2000);


        try
        {
            /*SignerGuard::SetBlockedSigner(
                L"BOOST - NET KRZYSZTOF ZAGÓRSKI"
            );*/
            // Baseline inicializálás
            ExceptionGuard::Init();
            TextGuard::Init();
            SyscallScanner::InitSyscallBaseline();
            AntiTamper::Init();
        }
        catch (...)
        {
            g_Running.store(false, std::memory_order_release);
            return;
        }

        if (!EnsureLogDirectoryExists())
        {
            g_Running.store(false, std::memory_order_release);
            return;
        }

        g_WatchdogThread = std::thread(Watchdog, intervalMs);
        g_GuardianThread = std::thread(Guardian, intervalMs);
    }

    void Stop()
    {
        g_Running.store(false, std::memory_order_release);
        ExceptionGuard::Shutdown();

        if (g_WatchdogThread.joinable())
            g_WatchdogThread.join();

        if (g_GuardianThread.joinable())
            g_GuardianThread.join();

        g_WatchdogThreadId = 0;
        g_GuardianThreadId = 0;
    }
}
