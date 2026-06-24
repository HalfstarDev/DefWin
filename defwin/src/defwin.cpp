#if defined(DM_PLATFORM_WINDOWS) && !defined(DM_HEADLESS)

#define EXTENSION_NAME DefWin
#define LIB_NAME "DefWin"
#define MODULE_NAME "defwin"
#ifndef DLIB_LOG_DOMAIN
#define DLIB_LOG_DOMAIN LIB_NAME
#endif

#include <dmsdk/sdk.h>
#include <cstring>

/* transparency.cpp */
extern void SetColorKey(int r, int g, int b);
extern bool SetImageBuffer(const uint8_t* pixels, int w, int h, bool flip);
extern void SetWindowAlpha(float alpha);
extern void SetBorderless(bool enable);
extern void Reset();

/* close.cpp*/
extern void WindowcloseInstallHook();
extern void WindowcloseRemoveHook();

/* resize.cpp */
extern void LockAspectRatio(float aspect_ratio);
extern void UnlockAspectRatio();
extern void SetMinimumWindowSize(int width, int height);
extern void FitVertical(float aspect_ratio);
extern void FitHorizontal(float aspect_ratio);
extern void FitGrow(float aspect_ratio);
extern void FitShrink(float aspect_ratio);
extern void FitScreen();

/* taskbar.cpp */
extern void Flash(bool tray, bool caption, bool timer, bool timernofg, bool stop, int count, int timeout);
extern void SetProgress(double value);
extern void SetProgressNormal();
extern void SetProgressLoad();
extern void SetProgressError();
extern void SetProgressPause();
extern void HideProgress();

/* drag.cpp */
extern void StartDrag();
extern void UpdateDrag();
extern void StopDrag();

/* style.cpp */
extern bool SetRoundedCorners(const char* mode);
extern void SetDarkMode(bool enable);
extern int GetDarkModePreference();
extern void SetToolWindow(bool enable);
extern void SetRTLLayout(bool enable);
extern void SetBorderColor(int r, int g, int b);
extern void SetBorderColorNone();
extern void SetBorderColorDefault();
extern void SetCaptionColor(int r, int g, int b);
extern void SetCaptionColorDefault();
extern void SetCaptionTextColor(int r, int g, int b);
extern void SetCaptionTextColorDefault();
extern dmVMath::Vector4 GetCaptionColor(bool premultiply_alpha, bool transparent);

/* activity.cpp */
extern void PreventSleep(bool enable);

/* config */
static float g_ConfigRatio = 960.0f / 640.0f;
static float g_ClearR = 0.0f;
static float g_ClearG = 0.0f;
static float g_ClearB = 0.0f;

/* state */
static dmScript::LuaCallbackInfo* g_LuaCallback = nullptr;
static bool     g_CloseInProgress   = false;
static bool     g_DelayEnabled      = false;
static float    g_DelayDuration     = 0.0f;
static float    g_DelayTimer        = 0.0f;
static uint64_t g_LastUpdateTime    = 0;
static bool     g_FadeoutEnabled    = false;
static float    g_InitialMasterGain = 1.0f;
bool            g_CloseRequested    = false;

/* ---------------- Helper functions ---------------- */

/* Exit the game, independent of close settings */
static void DoExit()
{
    exit(0);
}

/* Get the master gain from Defold's sound system. */
static float GetMasterGain(lua_State* L)
{
    float gain = 1.0f;
    lua_getglobal(L, "sound");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return gain;
    }
    lua_getfield(L, -1, "get_group_gain");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return gain;
    }
    lua_pushstring(L, "master");
    if (lua_pcall(L, 1, 1, 0) == 0) {
        if (lua_isnumber(L, -1)) {
            gain = (float)lua_tonumber(L, -1);
        }
    }
    lua_pop(L, 2);
    return gain;
}

/* Set the master gain from Defold's sound system. */
static void SetMasterGain(lua_State* L, float gain)
{
    lua_getglobal(L, "sound");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }
    lua_getfield(L, -1, "set_group_gain");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return;
    }
    lua_pushstring(L, "master");
    lua_pushnumber(L, (double)gain);
    if (lua_pcall(L, 2, 0, 0) != 0) {
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

/* Reads an aspect ratio from Lua arguments. If nil, uses projet aspect ratio as default. */
static double GetRatioFromLua(lua_State* L)
{
    if (lua_isnumber(L, 1)) {
        if (lua_isnumber(L, 2)) {
            return lua_tonumber(L, 1) / lua_tonumber(L, 2);
        } else {
            return lua_tonumber(L, 1);
        }
    } else if (lua_isuserdata(L, 1) && dmScript::IsVector3(L, 1)) {
        dmVMath::Vector3* v = dmScript::CheckVector3(L, 1);
        return v->getX() / v->getY();
    } else {
        return g_ConfigRatio;
    }
}

/* Update loop for handling close requests. */
static dmExtension::Result UpdateClose(lua_State* L)
{
    uint64_t now = dmTime::GetTime(); // microseconds
    float dt = 0.0f;
    if (g_LastUpdateTime != 0) {
        dt = (float)(now - g_LastUpdateTime) / 1000000.0f;
    }
    g_LastUpdateTime = now;

    if (g_CloseRequested) {
        g_CloseRequested = false;
        if (!g_CloseInProgress) {
            g_CloseInProgress = true;

            if (g_LuaCallback != nullptr && dmScript::IsCallbackValid(g_LuaCallback)) {
                lua_State* L = dmScript::GetCallbackLuaContext(g_LuaCallback);
                DM_LUA_STACK_CHECK(L, 0);

                if (dmScript::SetupCallback(g_LuaCallback)) {
                    dmScript::PCall(L, 1, 0);
                    dmScript::TeardownCallback(g_LuaCallback);
                } else {
                    dmLogError("DefWin: failed to setup listener callback");
                }
            }

            if (g_DelayEnabled) {
                g_DelayTimer = 0.0f;
                if (g_FadeoutEnabled) {
                    g_InitialMasterGain = GetMasterGain(L);
                }
            } else {
                if (g_LuaCallback == nullptr) {
                    DoExit();
                    return dmExtension::RESULT_OK;
                }
            }
        }
    }

    if (g_CloseInProgress && g_DelayEnabled) {
        g_DelayTimer += dt;

        if (g_FadeoutEnabled && g_DelayDuration > 0.0f) {
            float t = g_DelayTimer / g_DelayDuration;
            if (t > 1.0f) t = 1.0f;
            float currentGain = g_InitialMasterGain * (1.0f - t);
            SetMasterGain(L, currentGain);
        }

        if (g_DelayTimer >= g_DelayDuration) {
            SetMasterGain(L, 0);
            DoExit();
            g_CloseInProgress = false;
        }
    }

    return dmExtension::RESULT_OK;
}

/* ---------------- Lua functions ---------------- */

/*
* defwin.set_alpha_color(r, g, b)
* Set which color should be rendered transparent by the window, and removes the window border.
* The transparent areas will also be click-through.
* If no color is given, automatically use the clear color.
* number r, g, b: RGB color in [0.0, 1.0]
*/
static int Lua_SetAlphaColor(lua_State* L)
{
    int r = 0;
    int g = 0;
    int b = 0;
    if (lua_isnoneornil(L, 1)) {
        r = (int) (g_ClearR * 255);
        g = (int) (g_ClearG * 255);
        b = (int) (g_ClearB * 255);
    } else if (lua_isuserdata(L, 1) && dmScript::IsVector3(L, 1)) {
        dmVMath::Vector3* v = dmScript::CheckVector3(L, 1);
        r = (int) (v->getX() * 255);
        g = (int) (v->getY() * 255);
        b = (int) (v->getZ() * 255);
    } else if (lua_isuserdata(L, 1) && dmScript::IsVector4(L, 1)) {
        dmVMath::Vector4* v = dmScript::CheckVector4(L, 1);
        r = (int) (v->getX() * 255);
        g = (int) (v->getY() * 255);
        b = (int) (v->getZ() * 255);
    } else {
        r = luaL_checknumber(L, 1) * 255;
        g = luaL_checknumber(L, 2) * 255;
        b = luaL_checknumber(L, 3) * 255;
    }
    SetColorKey(r, g, b);
    lua_pushboolean(L, 1);
    return 1;
}

/*
* defwin.set_image_buffer(bytes_string, width, height, flip)
* Use an image instead of the default window.
* string bytes_string:  raw RGBA string from buffer.get_bytes(buf, hash("rgba")).
* number width, height: size of the image buffer
* boolean flip:         (optional) if true the rows are flipped vertically (default false)
*/
static int Lua_SetImageBuffer(lua_State* L)
{
    size_t len = 0;
    const uint8_t* pixels = (const uint8_t*)luaL_checklstring(L, 1, &len);
    int width = luaL_checkinteger(L, 2);
    int height = luaL_checkinteger(L, 3);
    bool flip = lua_isboolean(L, 4) && lua_toboolean(L, 4);
    bool ok = SetImageBuffer(pixels, width, height, flip);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

/*
* defwin.set_alpha(alpha)
* Set the transparency of the whole window, including border.
* Can't be used together with set_alpha_color.
* number alpha: opacity in [0.0, 1.0]
* alpha: 0.0 = fully transparent, 1.0 = fully opaque
*/
static int Lua_SetAlpha(lua_State* L)
{
    float alpha = (float)luaL_checknumber(L, 1);
    SetWindowAlpha(alpha);
    return 0;
}

/*
* defwin.set_borderless(enable)
* bool enable
* Remove the window border.
*/
static int Lua_SetBorderless(lua_State* L)
{
    bool enable = lua_isboolean(L, 1) && lua_toboolean(L, 1);
    SetBorderless(enable);
    return 0;
}

/*
* defwin.reset_alpha()
* Remove the window transparency, restoring the original window.
*/
static int Lua_Reset(lua_State* L)
{
    Reset();
    return 0;
}

/*
* defwin.set_close_listener(listener)
* function listener(self)
* If listener is set, listener will be called instead of quitting game if close button is pressed.
* Pass nil to clear the listener and restore default close behaviour.
*/
static int Lua_SetListener(lua_State* L)
{
    if (g_LuaCallback != nullptr) {
        dmScript::DestroyCallback(g_LuaCallback);
        g_LuaCallback = nullptr;
    }

    if (!lua_isnil(L, 1)) {
        luaL_checktype(L, 1, LUA_TFUNCTION);
        g_LuaCallback = dmScript::CreateCallback(L, 1);
    }
    return 0;
}

/*
* defwin.set_close_delay(delay)
* Set time after the close request until the game exits automatically.
* number delay: Time in seconds
* delay = nil:  No automatic close, depends on listener function
* delay = 0:    Close as soon as button pressed
*/
static int Lua_SetDelay(lua_State* L)
{
    if (lua_isnil(L, 1)) {
        g_DelayEnabled = false;
    } else {
        float newDuration = (float)luaL_checknumber(L, 1);
        if (newDuration < 0.0f) {
            newDuration = 0.0f;
        }
        g_DelayDuration = newDuration;
        g_DelayEnabled = true;
    }
    return 0;
}

/*
* defwin.set_close_sound_fadeout(enable)
* bool enable
* If enabled and a delay is set, the master sound group gain will be fade out until shutdown.
*/
static int Lua_SetSoundFadeout(lua_State* L)
{
    if (lua_isnoneornil(L, 1)) {
        g_FadeoutEnabled = false;
    } else {
        luaL_checktype(L, 1, LUA_TBOOLEAN);
        g_FadeoutEnabled = (bool)lua_toboolean(L, 1);
    }
    return 0;
}

/*
* defwin.quit()
* Quit the game, respecting the delay and fade-out settings.
*/
static int Lua_StartQuit(lua_State* L)
{
    if (g_DelayEnabled) {
        g_CloseInProgress = true;
    } else {
        DoExit();
    }
    return 0;
}

/*
* defwin.request_quit()
* Request to quit the game, like when pressing the close button or Alt+F4.
*/
static int Lua_RequestQuit(lua_State* L)
{
    g_CloseRequested = true;
    return 0;
}

/*
* defwin.cancel_close()
* Cancel the attempt to close the window.
* Until this is called, further tries to close the window will not call listener or restart delay.
*/
static int Lua_Cancel(lua_State* L)
{
    if (g_CloseInProgress && g_FadeoutEnabled) {
        SetMasterGain(L, g_InitialMasterGain);
    }
    g_CloseInProgress = false;
    g_DelayTimer = 0.0f;
    return 0;
}

/*
* defwin.lock_ratio([ratio])
* number ratio: The aspect ratio to lock (width / height)
* Lock aspect ratio of window on dragging border.
* If no ratio is given, uses the aspect ratio of the default resolution in game.project.
*/
static int Lua_LockAspectRatio(lua_State* L)
{
    double ratio = GetRatioFromLua(L);
    LockAspectRatio(ratio);
    return 0;
}

/*
* defwin.unlock_ratio()
* Remove the lock of the aspect ratio.
*/
static int Lua_UnlockAspectRatio(lua_State* L)
{
    UnlockAspectRatio();
    return 0;
}

/*
* defwin.set_minimum_window_size(width, height)
* number width, height: The minimum window size
* Set the minimum size a window is allowed to have on resize.
*/
static int Lua_SetMinimumWindowSize(lua_State* L)
{
    int width = 0;
    int height = 0;
    if (lua_isnumber(L, 1)) {
        width = (int)lua_tonumber(L, 1);
    }
    if (lua_isnumber(L, 2)) {
        height = (int)lua_tonumber(L, 2);
    }
    if (lua_isuserdata(L, 1) && dmScript::IsVector3(L, 1)) {
        dmVMath::Vector3* v = dmScript::CheckVector3(L, 1);
        width  = (int) v->getX();
        height = (int) v->getY();
    }
    
    SetMinimumWindowSize(width, height);
    return 0;
}

/*
* defwin.fit_vertical([ratio])
* number ratio: The aspect ratio to fit the window to
* Fit the window to the aspect ratio by resizing only the height.
* If no ratio is given, the aspect ratio from the display size config is used.
*/
static int Lua_FitVertical(lua_State* L)
{
    double ratio = GetRatioFromLua(L);
    FitVertical(ratio);
    return 0;
}

/*
* defwin.fit_horizontal([ratio])
* number ratio: The aspect ratio to fit the window to
* Fit the window to the aspect ratio by resizing only the width.
* If no ratio is given, the aspect ratio from the display size config is used.
*/
static int Lua_FitHorizontal(lua_State* L)
{
    double ratio = GetRatioFromLua(L);
    FitHorizontal(ratio);
    return 0;
}

/*
* defwin.fit_grow([ratio])
* number ratio: The aspect ratio to fit the window to
* Fit the window to the aspect ratio by only growing the size of one direction.
* If no ratio is given, the aspect ratio from the display size config is used.
*/
static int Lua_FitGrow(lua_State* L)
{
    double ratio = GetRatioFromLua(L);
    FitGrow(ratio);
    return 0;
}

/*
* defwin.fit_shrink([ratio])
* number ratio: The aspect ratio to fit the window to
* Fit the window to the aspect ratio by only shrinking the size of one direction.
* If no ratio is given, the aspect ratio from the display size config is used.
*/
static int Lua_FitShrink(lua_State* L)
{
    double ratio = GetRatioFromLua(L);
    FitShrink(ratio);
    return 0;
}

/*
* defwin.fit_screen()
* Repositions and resizes the window so that no part is outside of the screen.
*/
static int Lua_FitScreen(lua_State* L)
{
    FitScreen();
    return 0;
}

/*
* defwin.flash(taskbar, caption, count, timeout)
* bool   taskbar:  Should the taskbar button flash
* bool   caption:  Should the window caption flash
* number count:    How often should the flash happen (default = 1)
* number timeout:  Time between flashes in seconds 
*/
static int Lua_Flash(lua_State* L)
{
    bool taskbar = lua_isboolean(L, 1) && lua_toboolean(L, 1);
    bool caption = lua_isboolean(L, 2) && lua_toboolean(L, 2);
    int count = 1;
    if (lua_isnumber( L, 3)) {
        count = lua_tonumber( L, 3);
    }
    int timeout = 0;
    if (lua_isnumber( L, 4)) {
        timeout = lua_tonumber( L, 4) * 1000;
    }
    Flash(taskbar, caption, false, false, false, count, timeout);
    return 0;
}

/*
* defwin.start_flash(taskbar, caption, until_focused, timeout)
* bool   taskbar:       Should the taskbar button flash
* bool   caption:       Should the window caption flash
* bool   until_focused: If true, focusing window stops flash, otherwise it will flash until manually stopped
* number timeout:       Time between flashes in seconds
*/
static int Lua_StartFlash(lua_State* L)
{
    bool taskbar       = lua_isboolean(L, 1) && lua_toboolean(L, 1);
    bool caption       = lua_isboolean(L, 2) && lua_toboolean(L, 2);
    bool until_focused = lua_isboolean(L, 3) && lua_toboolean(L, 3);
    int timeout = 0;
    if (lua_isnumber( L, 4)) {
        timeout = lua_tonumber( L, 4) * 1000;
    }
    Flash(taskbar, caption, !until_focused, until_focused, false, 0, timeout);
    return 0;
}

/*
* defwin.stop_flash()
* Stop the flashing of the window.
* As the current flash will still be completed, it can take a few seconds to disappear.
*/
static int Lua_StopFlash(lua_State* L)
{
    Flash(false, false, false, false, true, 0, 0);
    return 0;
}

/*
* defwin.set_progress(value)
* Set the progress bar on the taskbar button.
* number value: progress in [0.0, 1.0]
*/
static int Lua_SetProgress(lua_State* L)
{
    double value = 0;
    if (lua_isnumber (L, 1)) {
        value = lua_tonumber( L, 1);
    }
    SetProgressNormal();
    SetProgress(value);
    return 0;
}

/*
* defwin.set_progress_load()
* Set the progress bar on the taskbar button to an indeterminate loading state.
*/
static int Lua_SetProgressLoad(lua_State* L)
{
    SetProgressLoad();
    return 0;
}

/*
* defwin.set_progress_error()
* Set the progress bar on the taskbar button to an error state, turning it red.
*/
static int Lua_SetProgressError(lua_State* L)
{
    SetProgressError();
    return 0;
}

/*
* defwin.pause_progress()
* Pauses the progress bar on the taskbar button, turning it yellow until a new value or state is set.
*/
static int Lua_PauseProgress(lua_State* L)
{
    SetProgressPause();
    return 0;
}

/*
* defwin.hide_progress()
* Hide the progress bar on the taskbar button, until a new value or state is set.
*/
static int Lua_HideProgress(lua_State* L)
{
    HideProgress();
    return 0;
}

/*
* defwin.set_dark_mode(enable)
* Set the window style to dark mode on true, and to light mode on false.
*/
static int Lua_SetDarkMode(lua_State* L)
{
    bool enable = lua_isboolean(L, 1) && lua_toboolean(L, 1);
    SetDarkMode(enable);
    return 0;
}

/*
* defwin.get_dark_mode_preference()
* Get the prefernce for dark mode of the current user
* return:
* - ´true´  if dark mode is preferred
* - ´false´ if light mode is preferred
* - ´nil´   if the preference is unknown
*/
static int Lua_GetDarkModePreference(lua_State* L)
{
    static int darkModePreference = GetDarkModePreference();
    if (darkModePreference == 0) {
        lua_pushboolean(L, 0);
    } else if (darkModePreference == 1) {
        lua_pushboolean(L, 1);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

/*
* defwin.set_corners_rounded()
* Set the corner style of the window frame to rounded, if available.
*/
static int Lua_SetCornersRounded(lua_State* L)
{
    SetRoundedCorners("ROUND");
    return 0;
}

/*
* defwin.set_corners_rounded_small()
* Set the corner style of the window frame to slightly rounded, if available.
*/
static int Lua_SetCornersRoundedSmall(lua_State* L)
{
    SetRoundedCorners("ROUNDSMALL");
    return 0;
}

/*
* defwin.set_corners_not_rounded()
* Set the corner style of the window frame to not rounded.
*/
static int Lua_SetCornersNotRounded(lua_State* L)
{
    SetRoundedCorners("DONOTROUND");
    return 0;
}

/*
* defwin.set_corners_default()
* Set the corner style of the window frame to the default.
*/
static int Lua_SetCornersDefault(lua_State* L)
{
    SetRoundedCorners("DEFAULT");
    return 0;
}

static int SetBorderColors(lua_State* L, bool border, bool caption, bool text)
{
    int r = 0;
    int g = 0;
    int b = 0;
    if (lua_isnoneornil(L, 1)) {
        if (border) {
            SetBorderColorDefault();
        }
        if (caption) {
            SetCaptionColorDefault();
        }
        if (text) {
            SetCaptionTextColorDefault();
        }
        return 0;
    } else if (lua_isuserdata(L, 1) && dmScript::IsVector3(L, 1)) {
        dmVMath::Vector3* v = dmScript::CheckVector3(L, 1);
        r = (int) (v->getX() * 255);
        g = (int) (v->getY() * 255);
        b = (int) (v->getZ() * 255);
    } else if (lua_isuserdata(L, 1) && dmScript::IsVector4(L, 1)) {
        dmVMath::Vector4* v = dmScript::CheckVector4(L, 1);
        r = (int) (v->getX() * 255);
        g = (int) (v->getY() * 255);
        b = (int) (v->getZ() * 255);
    } else {
        r = luaL_checknumber(L, 1) * 255;
        g = luaL_checknumber(L, 2) * 255;
        b = luaL_checknumber(L, 3) * 255;
    }
    if (border) {
        SetBorderColor(r, g, b);
    }
    if (caption) {
        SetCaptionColor(r, g, b);
    }
    if (text) {
        SetCaptionTextColor(r, g, b);
    }
    return 0;
}

/*
* defwin.set_border_color(r, g, b)
* Set window border color, with RGB in [0.0, 1.0]
* defwin.set_border_color(nil) for default color
*/
static int Lua_SetBorderColor(lua_State* L)
{
    SetBorderColors(L, true, false, false);
    return 0;
}

/*
* defwin.set_caption_color(r, g, b)
* Set window caption color, with RGB in [0.0, 1.0]
* defwin.set_caption_color(nil) for default color
*/
static int Lua_SetCaptionColor(lua_State* L)
{
    SetBorderColors(L, false, true, false);
    return 0;
}

/*
* defwin.set_caption_text_color()
* Set window caption text color, with RGB in [0.0, 1.0]
* defwin.set_caption_text_color(nil) for default color
*/
static int Lua_SetCaptionTextColor(lua_State* L)
{
    SetBorderColors(L, false, false, true);
    return 0;
}

/*
* defwin.set_border_color_none()
* Set the window border color to be transparent
*/
static int Lua_SetBorderColorNone(lua_State* L)
{
    SetBorderColorNone();
    return 0;
}

/*
* defwin.get_caption_color()
* Get the opaque color of the window caption
* return: vmath.vector4 color = vmath.vector4(r, g, b, 1) with r, g, b in [0.0, 1.0]
*/
static int Lua_GetCaptionColor(lua_State* L)
{
    dmVMath::Vector4 captionColor = GetCaptionColor(true, false);
    dmScript::PushVector4(L, captionColor);
    return 1;
}

/*
* defwin.set_rtl_layout(enable)
* Set window bar layout to right-to-left, affecting buttons, title, and icon
*/
static int Lua_SetRTLLayout(lua_State* L)
{
    bool enable = lua_isboolean(L, 1) && lua_toboolean(L, 1);
    SetRTLLayout(enable);
    return 0;
}

/*
* defwin.set_tool_window(enable)
* Turns the window into a tool window, or turn it back into a normal window.
* Tool windows are not shown on taskbar and Alt-Tab,
* and have no minimize and maximize buttons or icon.
*/
static int Lua_SetToolWindow(lua_State* L)
{
    bool enable = lua_isboolean(L, 1) && lua_toboolean(L, 1);
    SetToolWindow(enable);
    return 0;
}

/*
* defwin.start_drag()
* Start to drag the window along the cursor.
*/
static int Lua_StartDrag(lua_State* L)
{
    StartDrag();
    return 0;
}

/*
* defwin.update_drag()
* Update one frame of the mouse drag. If this is not called, start_drag does nothing.s
*/
static int Lua_UpdateDrag(lua_State* L)
{
    UpdateDrag();
    return 0;
}

/*
* defwin.stop_drag()
* Stop to drag the window along the cursor.
*/
static int Lua_StopDrag(lua_State* L)
{
    StopDrag();
    return 0;
}

/*
* defwin.prevent_sleep(enable)
* Keep the window active to prevent the system from going to sleep while enabled.
*/
static int Lua_PreventSleep(lua_State* L)
{
    bool enable = lua_isboolean(L, 1) && lua_toboolean(L, 1);
    PreventSleep(enable);
    return 0;
}

/* ---------------- Extension functions ---------------- */

static const luaL_Reg LuaMethods[] =
{
    // -------- transparency --------
    {"set_alpha",                 Lua_SetAlpha},
    {"set_alpha_color",           Lua_SetAlphaColor},
    {"set_borderless",            Lua_SetBorderless},
    {"set_image_buffer",          Lua_SetImageBuffer},
    {"reset_alpha",               Lua_Reset},
    // -------- close --------
    {"set_close_listener",        Lua_SetListener},
    {"set_close_delay",           Lua_SetDelay},
    {"set_close_sound_fadeout",   Lua_SetSoundFadeout},
    {"quit",                      Lua_StartQuit},
    {"request_quit",              Lua_RequestQuit},
    {"cancel_close",              Lua_Cancel},
    // -------- resize --------
    {"lock_ratio",                Lua_LockAspectRatio},
    {"unlock_ratio",              Lua_UnlockAspectRatio},
    {"set_minimum_window_size",   Lua_SetMinimumWindowSize},
    {"fit_vertical",              Lua_FitVertical},
    {"fit_horizontal",            Lua_FitHorizontal},
    {"fit_grow",                  Lua_FitGrow},
    {"fit_shrink",                Lua_FitShrink},
    {"fit_screen",                Lua_FitScreen},
    // -------- style --------
    {"set_dark_mode",             Lua_SetDarkMode},
    {"get_dark_mode_preference",  Lua_GetDarkModePreference},
    {"set_corners_rounded",       Lua_SetCornersRounded},
    {"set_corners_rounded_small", Lua_SetCornersRoundedSmall},
    {"set_corners_not_rounded",   Lua_SetCornersNotRounded},
    {"set_corners_default",       Lua_SetCornersDefault},
    {"set_border_color",          Lua_SetBorderColor},
    {"set_border_color_none",     Lua_SetBorderColorNone},
    {"set_caption_color",         Lua_SetCaptionColor},
    {"set_caption_text_color",    Lua_SetCaptionTextColor},
    {"get_caption_color",         Lua_GetCaptionColor},
    {"set_rtl_layout",            Lua_SetRTLLayout},
    {"set_tool_window",           Lua_SetToolWindow},
    // -------- taskbar --------
    {"flash",                     Lua_Flash},
    {"start_flash",               Lua_StartFlash},
    {"stop_flash",                Lua_StopFlash},
    {"set_progress",              Lua_SetProgress},
    {"set_progress_load",         Lua_SetProgressLoad},
    {"set_progress_error",        Lua_SetProgressError},
    {"pause_progress",            Lua_PauseProgress},
    {"hide_progress",             Lua_HideProgress},
    // -------- drag --------
    {"start_drag",                Lua_StartDrag},
    {"update_drag",               Lua_UpdateDrag},
    {"stop_drag",                 Lua_StopDrag},
    // -------- activity --------
    {"prevent_sleep",             Lua_PreventSleep},
    {0, 0}
};

static dmExtension::Result DefWinAppInit(dmExtension::AppParams* params)
{
    HConfigFile config = params->m_ConfigFile;

    float width  = ConfigFileGetFloat(config, "display.width",  960.0);
    float height = ConfigFileGetFloat(config, "display.height", 640.0);
    g_ConfigRatio = width / height;
    g_ClearR = ConfigFileGetFloat(config, "render.clear_color_red", 0.0f);
    g_ClearG = ConfigFileGetFloat(config, "render.clear_color_green", 0.0f);
    g_ClearB = ConfigFileGetFloat(config, "render.clear_color_blue", 0.0f);
    
    return dmExtension::RESULT_OK;
}

static dmExtension::Result DefWinAppFinal(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result DefWinInit(dmExtension::Params* params)
{
    lua_State* L = params->m_L;
    luaL_register(L, MODULE_NAME, LuaMethods);
    lua_pop(L, 1);
    
    WindowcloseInstallHook();

    /* Apply settings from game.project on init */

    HConfigFile config = params->m_ConfigFile;

    const int useDarkModePreference = ConfigFileGetInt(config, "defwin.use_dark_mode_preference", 0);
    if (useDarkModePreference == 1) {
        int configDarkModePreference = GetDarkModePreference();
        if (configDarkModePreference == 0) {
            SetDarkMode(false);
        } else if (configDarkModePreference == 1) {
            SetDarkMode(true);
        }
    }

    const int minimumWidth  = ConfigFileGetInt(config, "defwin.minimum_width", 0);
    const int minimumHeight = ConfigFileGetInt(config, "defwin.minimum_height", 0);
    if (minimumWidth > 0 || minimumHeight > 0) {
        SetMinimumWindowSize(minimumWidth, minimumHeight);
    }

    const int lockAspectRatio = ConfigFileGetInt(config, "defwin.lock_aspect_ratio", 0);
    if (lockAspectRatio == 1) {
        LockAspectRatio(g_ConfigRatio);
    }
    
    const int setClearTransparent  = ConfigFileGetInt(config, "defwin.set_clear_transparent", 0);
    if (setClearTransparent == 1) {
        const int r = g_ClearR * 255;
        const int g = g_ClearG * 255;
        const int b = g_ClearB * 255;
        SetColorKey(r, g, b);
    }

    const char* roundedCorners = dmConfigFile::GetString(config, "defwin.rounded_corners", "DEFAULT");
    if (roundedCorners) {
        SetRoundedCorners(roundedCorners);
    }

    const int useRTLLayout = ConfigFileGetInt(config, "defwin.rtl_layout", 0);
    if (useRTLLayout == 1) {
        SetRTLLayout(true);
    }

    const int disableBorder = ConfigFileGetInt(config, "defwin.disable_border", 0);
    if (disableBorder == 1) {
        SetBorderColorNone();
    }
    
    return dmExtension::RESULT_OK;
}

static dmExtension::Result DefWinFinal(dmExtension::Params* params)
{
    UnlockAspectRatio();
    WindowcloseRemoveHook();
    if (g_LuaCallback != nullptr) {
        dmScript::DestroyCallback(g_LuaCallback);
        g_LuaCallback = nullptr;
    }
    return dmExtension::RESULT_OK;
}

static dmExtension::Result DefWinUpdate(dmExtension::Params* params)
{
    return UpdateClose(params->m_L);
}

DM_DECLARE_EXTENSION(EXTENSION_NAME, LIB_NAME, DefWinAppInit, DefWinAppFinal, DefWinInit, DefWinUpdate, 0, DefWinFinal)

#endif