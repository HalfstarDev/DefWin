#if defined(DM_PLATFORM_WINDOWS) && !defined(DM_HEADLESS)

#include "win.h"
#include <shobjidl.h>

/* Lazily-created, process-lifetime instance of the taskbar interface. */
inline ITaskbarList3* GetTaskbarList()
{
    static ITaskbarList3* taskbarList = NULL;
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        if (SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE) {
            hr = CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, IID_ITaskbarList3, (void**)&taskbarList);
            if (SUCCEEDED(hr) && taskbarList) {
                if (FAILED(taskbarList->HrInit())) {
                    taskbarList->Release();
                    taskbarList = NULL;
                }
            } else {
                taskbarList = NULL;
            }
        }
    }
    return taskbarList;
}

/*
* Set the progress bar on the taskbar button.
* value is the progress fraction in the range [0.0, 1.0].
*/
void SetProgress(double value)
{
    ITaskbarList3* taskbarList = GetTaskbarList();
    if (!taskbarList) {
        return;
    }

    HWND hwnd = GetWindow();
    if (value < 0.0) value = 0.0;
    if (value > 1.0) value = 1.0;
    const ULONGLONG total = 1000;
    ULONGLONG completed = (ULONGLONG)(value * total);
    
    taskbarList->SetProgressValue(hwnd, completed, total);
}

static void SetProgressState(TBPFLAG state)
{
    ITaskbarList3* taskbarList = GetTaskbarList();
    if (!taskbarList) {
        return;
    }
    HWND hwnd = GetWindow();
    taskbarList->SetProgressState(hwnd, state);
}

void SetProgressNormal()
{
    SetProgressState(TBPF_NORMAL);
}

void SetProgressLoad()
{
    SetProgressState(TBPF_INDETERMINATE);
}

void SetProgressError()
{
    SetProgressState(TBPF_ERROR);
}

void SetProgressPause()
{
    SetProgressState(TBPF_PAUSED);
}

void HideProgress()
{
    SetProgressState(TBPF_NOPROGRESS);
}

/* Flash the taskbar button or window caption. */
void Flash(bool tray, bool caption, bool timer, bool timernofg, bool stop, int count, int timeout)
{
    FLASHWINFO fwi;
    ZeroMemory(&fwi, sizeof(fwi));
    fwi.cbSize = sizeof(fwi);
    fwi.hwnd = GetWindow();
    fwi.dwFlags = 0;
    if (tray)      fwi.dwFlags = fwi.dwFlags | FLASHW_TRAY;
    if (caption)   fwi.dwFlags = fwi.dwFlags | FLASHW_CAPTION;
    if (timer)     fwi.dwFlags = fwi.dwFlags | FLASHW_TIMER;
    if (timernofg) fwi.dwFlags = fwi.dwFlags | FLASHW_TIMERNOFG;
    if (stop)      fwi.dwFlags = fwi.dwFlags | FLASHW_STOP;
    fwi.uCount = count;
    fwi.dwTimeout = timeout;
    FlashWindowEx(&fwi);
}

#endif