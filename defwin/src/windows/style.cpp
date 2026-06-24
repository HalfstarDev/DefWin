#if defined(DM_PLATFORM_WINDOWS) && !defined(DM_HEADLESS)

#include "win.h"
#include <dmsdk/dlib/vmath.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE_OLD
#define DWMWA_USE_IMMERSIVE_DARK_MODE_OLD 19
#endif

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

// Define the integer constants locally to avoid enum definition conflicts
#ifndef DWMWCP_DEFAULT
#define DWMWCP_DEFAULT      0
#define DWMWCP_DONOTROUND   1
#define DWMWCP_ROUND        2
#define DWMWCP_ROUNDSMALL   3
#endif

/* Enables or disables the immersive dark mode titlebar. */
void SetDarkMode(bool enable)
{
    HWND hwnd = GetWindow();
    BOOL value = enable ? TRUE : FALSE;
    HRESULT hr = DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
    if (FAILED(hr)) {
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_OLD, &value, sizeof(value));
    }
    // Force the window to redraw
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

/* 
* Get the prefernce for dark mode of the current user
* Returns:
*  1 if dark mode is preferred
*  0 if light mode is preferred
* -1 if the preference is unknown
*/
int GetDarkModePreference()
{
    HKEY hKey;
    LPCSTR subkey = "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
    LPCSTR valueName = "AppsUseLightTheme";

    LSTATUS status = RegOpenKeyExA(HKEY_CURRENT_USER, subkey, 0, KEY_READ, &hKey);
    if (status != ERROR_SUCCESS) {
        return -1;
    }

    DWORD value = 0;
    DWORD valueSize = sizeof(value);
    DWORD valueType = 0;

    status = RegQueryValueExA(hKey, valueName, NULL, &valueType, (LPBYTE)&value, &valueSize);
    RegCloseKey(hKey);
    if (status != ERROR_SUCCESS || valueType != REG_DWORD) {
        return -1;
    }

    // AppsUseLightTheme: 0 indicates Dark Mode, 1 indicates Light Mode
    return (value == 0) ? 1 : 0;
}

/*
* Set the corner style of the window frame, if available.
* mode: one of "DEFAULT", "DONOTROUND", "ROUND", "ROUNDSMALL"
* Returns true if the attribute was set successfully, false otherwise.
*/
bool SetRoundedCorners(const char* mode)
{
    HWND hwnd = GetWindow();

    DWORD preference = DWMWCP_DEFAULT;

    if (strcmp(mode, "DEFAULT") == 0) {
        preference = DWMWCP_DEFAULT;
    } else if (strcmp(mode, "DONOTROUND") == 0) {
        preference = DWMWCP_DONOTROUND;
    } else if (strcmp(mode, "ROUND") == 0) {
        preference = DWMWCP_ROUND;
    } else if (strcmp(mode, "ROUNDSMALL") == 0) {
        preference = DWMWCP_ROUNDSMALL;
    } else {
        return false;
    }

    HRESULT hr = DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));
    return SUCCEEDED(hr);
}

/*
* Turns the window into a tool window, or turn it back into a normal window.
* Tool windows are not shown on taskbar and Alt-Tab,
* and have no minimize and maximize buttons or icon.
*/
void SetToolWindow(bool enable)
{
    HWND hwnd = GetWindow();
    if (!hwnd) {
        return;
    }

    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);

    if (enable) {
        exStyle |= WS_EX_TOOLWINDOW;
        exStyle &= ~WS_EX_APPWINDOW;
    } else {
        exStyle &= ~WS_EX_TOOLWINDOW;
        exStyle |= WS_EX_APPWINDOW;
    }

    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);
}

void SetRTLLayout(bool enable)
{
    HWND hwnd = GetWindow();
    BOOL value = enable ? TRUE : FALSE;
    HRESULT hr = DwmSetWindowAttribute(hwnd, DWMWA_NONCLIENT_RTL_LAYOUT, &value, sizeof(value));
}

static void SetBorderColorRef(COLORREF value)
{
    HWND hwnd = GetWindow();
    HRESULT hr = DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &value, sizeof(value));
}

static void SetCaptionColorRef(COLORREF value)
{
    HWND hwnd = GetWindow();
    HRESULT hr = DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &value, sizeof(value));
}

static void SetCaptionTextColorRef(COLORREF value)
{
    HWND hwnd = GetWindow();
    HRESULT hr = DwmSetWindowAttribute(hwnd, DWMWA_TEXT_COLOR, &value, sizeof(value));
}

void SetBorderColor(int r, int g, int b)
{
    COLORREF value = RGB(r, g, b);
    SetBorderColorRef(value);
}

void SetBorderColorNone()
{
    SetBorderColorRef(DWMWA_COLOR_NONE);
}

void SetBorderColorDefault()
{
    SetBorderColorRef(DWMWA_COLOR_DEFAULT);
}

void SetCaptionColor(int r, int g, int b)
{
    COLORREF value = RGB(r, g, b);
    SetCaptionColorRef(value);
}

void SetCaptionColorDefault()
{
    SetCaptionColorRef(DWMWA_COLOR_DEFAULT);
}

void SetCaptionTextColor(int r, int g, int b)
{
    COLORREF value = RGB(r, g, b);
    SetCaptionTextColorRef(value);
}

void SetCaptionTextColorDefault()
{
    SetCaptionTextColorRef(DWMWA_COLOR_DEFAULT);
}

dmVMath::Vector4 GetCaptionColor(bool premultiply_alpha, bool transparent)
{
    DWORD color = 0;
    BOOL opaque = FALSE;
    HRESULT hr = DwmGetColorizationColor(&color, &opaque);

    // DwmGetColorizationColor returns a packed ARGB DWORD: 0xAARRGGBB.
    float a = ((color >> 24) & 0xFF) / 255.0f;
    float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >>  8) & 0xFF) / 255.0f;
    float b = ((color >>  0) & 0xFF) / 255.0f;
    
    if (premultiply_alpha && !opaque) {
        r *= a;
        g *= a;
        b *= a;
    }

    if (!transparent) {
        a = 1;
    }
    return dmVMath::Vector4(r, g, b, a);
}

#endif