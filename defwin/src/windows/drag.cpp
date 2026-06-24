#if defined(DM_PLATFORM_WINDOWS) && !defined(DM_HEADLESS)

#include "win.h"

static float cursor_x = 0;
static float cursor_y = 0;
static bool  dragging = false;

static void MoveWindow(float dx, float dy)
{
    HWND window = GetWindow();
    RECT rect;
    GetWindowRect(window, &rect);
    int w = (rect.right - rect.left);
    int h = (rect.bottom - rect.top);
    int x = (rect.left + dx);
    int y = (rect.top + dy);
    SetWindowPos(window, window, x, y, w, h, SWP_NOZORDER);
}

void StartDrag()
{
    POINT point;
    GetCursorPos(&point);
    cursor_x = point.x;
    cursor_y = point.y;
    dragging = true;
}

void UpdateDrag()
{
    if (dragging) {
        POINT point;
        GetCursorPos(&point);
        float new_x = point.x;
        float new_y = point.y;
        MoveWindow(new_x - cursor_x, new_y - cursor_y);
        cursor_x = new_x;
        cursor_y = new_y;
    }
}

void StopDrag()
{
    dragging = false;
}

#endif