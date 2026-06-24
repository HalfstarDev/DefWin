#if defined(DM_PLATFORM_WINDOWS) && !defined(DM_HEADLESS)

#include "win.h"
#include <stdint.h>

static LONG_PTR GetGWLStyle()
{
    return GetWindowLongPtr(GetWindow(), GWL_STYLE);
}

static LONG_PTR GetGWLExStyle()
{
    return GetWindowLongPtr(GetWindow(), GWL_EXSTYLE);
}

static void SetGWLStyleKeepClientRect(LONG_PTR newStyle)
{
    HWND hwnd = GetWindow();

    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    POINT topLeft     = { clientRect.left, clientRect.top };
    POINT bottomRight = { clientRect.right, clientRect.bottom };
    ClientToScreen(hwnd, &topLeft);
    ClientToScreen(hwnd, &bottomRight);

    LONG_PTR exStyle = GetGWLExStyle();
    SetWindowLongPtr(hwnd, GWL_STYLE, newStyle);

    RECT wantedRect = { topLeft.x, topLeft.y, bottomRight.x, bottomRight.y };
    
    AdjustWindowRectEx(&wantedRect, (DWORD)newStyle, FALSE, (DWORD)exStyle);

    SetWindowPos(hwnd, NULL,
        wantedRect.left, wantedRect.top,
        wantedRect.right  - wantedRect.left,
        wantedRect.bottom - wantedRect.top,
        SWP_NOZORDER | SWP_FRAMECHANGED);
}

static void SetGWLExStyle(LONG_PTR style)
{
    SetWindowLongPtr(GetWindow(), GWL_EXSTYLE, style);
}

static void EnableBorderless()
{
    LONG_PTR style = GetGWLStyle();
    if (style & WS_CAPTION || style & WS_THICKFRAME) {
        style &= ~(WS_CAPTION | WS_THICKFRAME);
        SetGWLStyleKeepClientRect(style);
    }
}

static void DisableBorderless()
{
    LONG_PTR style = GetGWLStyle();
    if (!(style & WS_CAPTION) || !(style & WS_THICKFRAME)) {
        style |= (WS_CAPTION | WS_THICKFRAME);
        SetGWLStyleKeepClientRect(style);
    }
}

static void EnableLayered()
{
    LONG_PTR exStyle = GetGWLExStyle();
    if (!(exStyle & WS_EX_LAYERED)) {
        exStyle |= WS_EX_LAYERED;
        SetGWLExStyle(exStyle);
    }
}

static void DisableLayered()
{
    LONG_PTR exStyle = GetGWLExStyle();
    if (exStyle & WS_EX_LAYERED) {
        exStyle &= ~WS_EX_LAYERED;
        SetGWLExStyle(exStyle);
    }
}

static void SetFrameMargin(int margin)
{
    MARGINS margins = {margin, margin, margin, margin};
    DwmExtendFrameIntoClientArea(GetWindow(), &margins);
}

void SetBorderless(bool enable)
{
    if (enable) {
        EnableBorderless();
    } else {
        DisableBorderless();
    }
}

void SetWindowAlpha(float alpha)
{
    EnableLayered();
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    BYTE bAlpha = (BYTE)(alpha * 255.0f);
    SetLayeredWindowAttributes(GetWindow(), 0, bAlpha, LWA_ALPHA);
}

void SetColorKey(int r, int g, int b)
{
    EnableBorderless();
    EnableLayered();

    COLORREF colorRef = RGB(r, g, b);
    SetLayeredWindowAttributes(GetWindow(), colorRef, 0, LWA_COLORKEY);

    SetFrameMargin(-1);
}

bool SetImageBuffer(const uint8_t* pixels, int width, int height, bool flip)
{
    EnableLayered();

    HDC screenDC = GetDC(NULL);
    HDC memDC = CreateCompatibleDC(screenDC);
    if (!memDC) {
        ReleaseDC(NULL, screenDC);
        return false;
    }

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = width;
    bmi.bmiHeader.biHeight      = -height;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* dibBits = NULL;
    HBITMAP hBitmap = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &dibBits, NULL, 0);
    if (!hBitmap) {
        DeleteDC(memDC);
        ReleaseDC(NULL, screenDC);
        return false;
    }

    uint8_t* dst = (uint8_t*)dibBits;
    for (int y = 0; y < height; y++) {
        int srcY = flip ? y : (height - 1 - y);
        int srcRowStart = srcY * width * 4;
        int dstRowStart = y * width * 4;
        for (int x = 0; x < width; x++) {
            uint8_t r = pixels[srcRowStart + x * 4 + 0];
            uint8_t g = pixels[srcRowStart + x * 4 + 1];
            uint8_t b = pixels[srcRowStart + x * 4 + 2];
            uint8_t a = pixels[srcRowStart + x * 4 + 3];
            dst[dstRowStart + x * 4 + 0] = (uint8_t)((b * a) / 255);
            dst[dstRowStart + x * 4 + 1] = (uint8_t)((g * a) / 255);
            dst[dstRowStart + x * 4 + 2] = (uint8_t)((r * a) / 255);
            dst[dstRowStart + x * 4 + 3] = a;
        }
    }

    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, hBitmap);

    HWND hwnd = GetWindow();    
    RECT windowRect;
    GetWindowRect(hwnd, &windowRect);
    POINT ptDst = { windowRect.left, windowRect.top };
    SIZE szWnd = { width, height };
    POINT ptSrc = { 0, 0 };

    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.BlendFlags = 0;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    bool ok = UpdateLayeredWindow(hwnd, screenDC, &ptDst, &szWnd, memDC, &ptSrc, 0, &blend, ULW_ALPHA) != FALSE;

    SelectObject(memDC, oldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(memDC);
    ReleaseDC(NULL, screenDC);

    return ok;
}

void Reset()
{
    DisableLayered();
    DisableBorderless();
    SetFrameMargin(0);
    RedrawWindow(GetWindow(), NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_FRAME);
}

#endif