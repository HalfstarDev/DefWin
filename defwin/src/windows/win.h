#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwmapi.h>
#include <dmsdk/graphics/graphics_native.h>

inline HWND GetWindow() {
	return dmGraphics::GetNativeWindowsHWND();
}