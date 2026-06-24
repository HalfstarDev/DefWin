#if defined(DM_PLATFORM_WINDOWS) && !defined(DM_HEADLESS)

#include "win.h"

static WNDPROC g_DefoldWndProc = nullptr;
static bool    g_AspectRatioLocked = false;
static float   g_TargetAspectRatio = 960.0f / 640.0f;
static int     g_MinWidth = 0;
static int     g_MinHeight = 0;

/* Computes the non-client border size */
static void GetWindowBorderSize(HWND hwnd, int& borderWidth, int& borderHeight)
{
    RECT adj = {0, 0, 100, 100};
    DWORD style = GetWindowLongPtr(hwnd, GWL_STYLE);
    DWORD exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    BOOL hasMenu = (GetMenu(hwnd) != NULL);
    AdjustWindowRectEx(&adj, style, hasMenu, exStyle);

    borderWidth = (adj.right - adj.left) - 100;
    borderHeight = (adj.bottom - adj.top) - 100;
}

/* Returns the current client area size of the window. */
static void GetClientSize(HWND hwnd, int& clientWidth, int& clientHeight)
{
    RECT client;
    GetClientRect(hwnd, &client);
    clientWidth = client.right - client.left;
    clientHeight = client.bottom - client.top;
}

/* Resizes the window so that its client area becomes targetClientW x targetClientH. */
static void ResizeWindowClientArea(HWND hwnd, int targetClientW, int targetClientH, bool centerHorizontal, bool centerVertical)
{
    if (IsZoomed(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    }

    int borderWidth, borderHeight;
    GetWindowBorderSize(hwnd, borderWidth, borderHeight);

    RECT windowRect;
    GetWindowRect(hwnd, &windowRect);

    if (targetClientW < g_MinWidth) {
        targetClientH = targetClientH * g_MinWidth / targetClientW;
        targetClientW = g_MinWidth;
    }
    if (targetClientH < g_MinHeight) {
        targetClientW = targetClientW * g_MinHeight / targetClientH;
        targetClientH = g_MinHeight;
    }

    int currentWindowW = windowRect.right - windowRect.left;
    int currentWindowH = windowRect.bottom - windowRect.top;

    int targetWindowW = targetClientW + borderWidth;
    int targetWindowH = targetClientH + borderHeight;

    int dw = targetWindowW - currentWindowW;
    int dh = targetWindowH - currentWindowH;

    int newLeft = windowRect.left;
    int newTop = windowRect.top;

    if (centerHorizontal) {
        newLeft -= dw / 2;
    }
    if (centerVertical) {
        newTop -= dh / 2;
    }

    SetWindowPos(hwnd, NULL, newLeft, newTop, targetWindowW, targetWindowH, SWP_NOZORDER | SWP_NOACTIVATE);
}

/* Changes only the height of the window so that the client area matches the given aspect ratio. */
void FitVertical(float aspect_ratio)
{
    HWND hwnd = GetWindow();
    int clientW, clientH;
    GetClientSize(hwnd, clientW, clientH);

    int targetClientH = (int)(clientW / aspect_ratio);

    ResizeWindowClientArea(hwnd, clientW, targetClientH, /*centerHorizontal=*/false, /*centerVertical=*/true);
}

/* Changes only the width of the window so that the client area matches the given aspect ratio. */
void FitHorizontal(float aspect_ratio)
{
    HWND hwnd = GetWindow();
    int clientW, clientH;
    GetClientSize(hwnd, clientW, clientH);

    int targetClientW = (int)(clientH * aspect_ratio);

    ResizeWindowClientArea(hwnd, targetClientW, clientH, /*centerHorizontal=*/true, /*centerVertical=*/false);
}

/* Fits window to the given aspect ratio using whichever of FitVertical/Horizontal increases the window's size. */
void FitGrow(float aspect_ratio)
{
    HWND hwnd = GetWindow();
    int clientW, clientH;
    GetClientSize(hwnd, clientW, clientH);

    float currentRatio = (float)clientW / (float)clientH;

    if (currentRatio > aspect_ratio) {
        FitVertical(aspect_ratio);
    } else {
        FitHorizontal(aspect_ratio);
    }
}

/* Fits window to the given aspect ratio using whichever of FitVertical/Horizontal decreases the window's size. */
void FitShrink(float aspect_ratio)
{
    HWND hwnd = GetWindow();
    int clientW, clientH;
    GetClientSize(hwnd, clientW, clientH);

    float currentRatio = (float)clientW / (float)clientH;

    if (currentRatio > aspect_ratio) {
        FitHorizontal(aspect_ratio);
    } else {
        FitVertical(aspect_ratio);
    }
}

/* Repositions and, if necessary, resizes the window so that it is fully contained within the screen. */
void FitScreen()
{
    HWND hwnd = GetWindow();

    RECT windowRect;
    GetWindowRect(hwnd, &windowRect);

    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo;
    monitorInfo.cbSize = sizeof(MONITORINFO);
    GetMonitorInfo(monitor, &monitorInfo);
    RECT screenRect = monitorInfo.rcWork;

    int windowW = windowRect.right - windowRect.left;
    int windowH = windowRect.bottom - windowRect.top;
    int screenW = screenRect.right - screenRect.left;
    int screenH = screenRect.bottom - screenRect.top;

    int newW = (windowW > screenW) ? screenW : windowW;
    int newH = (windowH > screenH) ? screenH : windowH;

    int newLeft = windowRect.left;
    if (newLeft + newW > screenRect.right) {
        newLeft = screenRect.right - newW;
    }
    if (newLeft < screenRect.left) {
        newLeft = screenRect.left;
    }

    int newTop = windowRect.top;
    if (newTop + newH > screenRect.bottom) {
        newTop = screenRect.bottom - newH;
    }
    if (newTop < screenRect.top) {
        newTop = screenRect.top;
    }

    if (newLeft == windowRect.left && newTop == windowRect.top && newW == windowW && newH == windowH) {
        return;
    }

    SetWindowPos(hwnd, NULL, newLeft, newTop, newW, newH, SWP_NOZORDER | SWP_NOACTIVATE);
}

/* Window procedure hook to intercept sizing events */
static LRESULT CALLBACK WindowProcHook(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_SIZING) {
        PRECT rect = (PRECT)lParam;

        int borderWidth, borderHeight;
        GetWindowBorderSize(hwnd, borderWidth, borderHeight);

        int proposedClientW = (rect->right - rect->left) - borderWidth;
        int proposedClientH = (rect->bottom - rect->top) - borderHeight;
        int targetClientW = proposedClientW;
        int targetClientH = proposedClientH;

        if (g_AspectRatioLocked) {
            switch (wParam) {
                case WMSZ_LEFT:
                case WMSZ_RIGHT:
                // Dragging horizontally: adjust height to match aspect ratio
                targetClientH = (int)(proposedClientW / g_TargetAspectRatio);
                break;

                case WMSZ_TOP:
                case WMSZ_BOTTOM:
                // Dragging vertically: adjust width to match aspect ratio
                targetClientW = (int)(proposedClientH * g_TargetAspectRatio);
                break;

                case WMSZ_TOPLEFT:
                case WMSZ_TOPRIGHT:
                case WMSZ_BOTTOMLEFT:
                case WMSZ_BOTTOMRIGHT:
                // Dragging corner: adjust width based on the proposed height change
                targetClientW = (int)(proposedClientH * g_TargetAspectRatio);
                break;
            }
        }
        
        if (targetClientW < g_MinWidth) {
            targetClientW = g_MinWidth;
            if (g_AspectRatioLocked) {
                targetClientH = (int)(targetClientW / g_TargetAspectRatio);
            }
        }
        if (targetClientH < g_MinHeight) {
            targetClientH = g_MinHeight;
            if (g_AspectRatioLocked) {
                targetClientW = (int)(targetClientH * g_TargetAspectRatio);
            }
        }

        int finalWindowW = targetClientW + borderWidth;
        int finalWindowH = targetClientH + borderHeight;

        // Update the dragging rectangle coordinates.
        switch (wParam) {
            case WMSZ_LEFT:
            rect->left = rect->right - finalWindowW;
            rect->bottom = rect->top + finalWindowH;
            break;

            case WMSZ_RIGHT:
            rect->right = rect->left + finalWindowW;
            rect->bottom = rect->top + finalWindowH;
            break;

            case WMSZ_TOP:
            rect->top = rect->bottom - finalWindowH;
            rect->right = rect->left + finalWindowW;
            break;

            case WMSZ_BOTTOM:
            rect->bottom = rect->top + finalWindowH;
            rect->right = rect->left + finalWindowW;
            break;

            case WMSZ_TOPLEFT:
            rect->left = rect->right - finalWindowW;
            rect->top = rect->bottom - finalWindowH;
            break;

            case WMSZ_TOPRIGHT:
            rect->right = rect->left + finalWindowW;
            rect->top = rect->bottom - finalWindowH;
            break;

            case WMSZ_BOTTOMLEFT:
            rect->left = rect->right - finalWindowW;
            rect->bottom = rect->top + finalWindowH;
            break;

            case WMSZ_BOTTOMRIGHT:
            rect->right = rect->left + finalWindowW;
            rect->bottom = rect->top + finalWindowH;
            break;
        }

        return TRUE;
    }

    return CallWindowProcW(g_DefoldWndProc, hwnd, uMsg, wParam, lParam);
}

void LockAspectRatio(float aspect_ratio)
{
    g_TargetAspectRatio = aspect_ratio;
    g_AspectRatioLocked = true;
    
    if (g_DefoldWndProc == nullptr) {
        g_DefoldWndProc = (WNDPROC)SetWindowLongPtr(GetWindow(), GWLP_WNDPROC, (LONG_PTR)WindowProcHook);
    }
}

void UnlockAspectRatio()
{
    g_AspectRatioLocked = false;
    if (g_DefoldWndProc != nullptr && g_MinWidth <= 0 && g_MinHeight <= 0) {
        SetWindowLongPtr(GetWindow(), GWLP_WNDPROC, (LONG_PTR)g_DefoldWndProc);
        g_DefoldWndProc = nullptr;
    }
}

void SetMinimumWindowSize(int width = 0, int height = 0)
{
    g_MinWidth = width;
    g_MinHeight = height;

    if (g_DefoldWndProc == nullptr) {
        g_DefoldWndProc = (WNDPROC)SetWindowLongPtr(GetWindow(), GWLP_WNDPROC, (LONG_PTR)WindowProcHook);
    }
}

#endif
