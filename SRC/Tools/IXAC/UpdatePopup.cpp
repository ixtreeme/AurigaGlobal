#include "UpdatePopup.h"
#include <string>

static HWND g_hPopup = NULL;

LRESULT CALLBACK PopupWndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l)
{
    switch (msg)
    {
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        return 0;
    default:
        return DefWindowProc(hwnd, msg, w, l);
    }
}

HWND ShowUpdatePopup()
{
    if (g_hPopup)
        return g_hPopup;

    HINSTANCE hInst = GetModuleHandle(NULL);

    WNDCLASSW wc{};
    wc.lpfnWndProc = PopupWndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"IXAC_UpdatePopup";

    RegisterClassW(&wc);

    g_hPopup = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        wc.lpszClassName,
        L"Updating...",
        WS_POPUP,
        CW_USEDEFAULT, CW_USEDEFAULT,
        320, 100,
        NULL, NULL,
        hInst, NULL);

    if (!g_hPopup)
        return NULL;

    // A szöveg
    CreateWindowExW(
        0, L"STATIC",
        L"Update started...\nPlease wait...",
        WS_VISIBLE | WS_CHILD | SS_CENTER,
        0, 10, 320, 60,
        g_hPopup, NULL, hInst, NULL);

    // Középre helyezés
    RECT rc{};
    SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0);

    int x = (rc.right - 320) / 2;
    int y = (rc.bottom - 100) / 2;

    SetWindowPos(g_hPopup, HWND_TOPMOST, x, y, 320, 100, SWP_SHOWWINDOW);

    return g_hPopup;
}

void CloseUpdatePopup()
{
    if (g_hPopup)
    {
        DestroyWindow(g_hPopup);
        g_hPopup = NULL;
    }
}
