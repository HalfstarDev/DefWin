#if defined(DM_PLATFORM_WINDOWS) && !defined(DM_HEADLESS)

#include "win.h"

/* Keep the window active to prevent the system from going to sleep while enabled. */
void PreventSleep(bool enable)
{
    if (enable) {
        SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
    } else {
        SetThreadExecutionState(ES_CONTINUOUS);
    }
}

#endif