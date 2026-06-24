#if defined(DM_PLATFORM_WINDOWS) && !defined(DM_HEADLESS)

#include "win.h"

static HWND    g_Hwnd = nullptr;
static WNDPROC g_OriginalWindowProc = nullptr;
extern bool    g_CloseRequested;

void OnNativeCloseRequested()
{
    g_CloseRequested = true;
}

static LRESULT CALLBACK CustomWindowProcW(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_CLOSE) {
        OnNativeCloseRequested();
        return 0;
    }
    return CallWindowProcW(g_OriginalWindowProc, hwnd, msg, wp, lp);
}

void WindowcloseInstallHook()
{
    g_Hwnd = GetWindow();
    if (g_Hwnd) {
        g_OriginalWindowProc = (WNDPROC)SetWindowLongPtrW(g_Hwnd, GWLP_WNDPROC, (LONG_PTR)CustomWindowProcW);
    }
}

void WindowcloseRemoveHook()
{
    if (g_Hwnd && g_OriginalWindowProc) {
        SetWindowLongPtrW(g_Hwnd, GWLP_WNDPROC, (LONG_PTR)g_OriginalWindowProc);
    }
}

#endif