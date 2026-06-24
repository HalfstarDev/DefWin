![banner](/docs/images/banner.jpg)
# DefWin

A Defold extension for advanced window features on Windows.
* Custom window shapes and colors
* Window transparency
* Lock aspect ratio for resize
* Hook for custom function of close button
* Flashing of the taskbar button
* Progress bar
* Dark mode
* Right-to-left window
* Drag window

## Installation

This asset can be added as a [library dependency](https://defold.com/manuals/libraries/#setting-up-library-dependencies) in your project.

Add this link to your dependencies: https://github.com/HalfstarDev/defwin/archive/master.zip

## Compatibility

This extension works only on Windows. On other systems `defwin` will be `nil`, so you can safely handle multiplatform releases like this:

```lua
if defwin then
    defwin.set_alpha_color()
end
```

## Transparency

#### defwin.set_alpha_color([r, g, b])
Set which color should be rendered transparent by the window, and removes the window border. The transparent areas will also be click-through.

You can either use the clear color of your project by calling it without arguments, or use manual RGB colors in [0.0, 1.0].

So while simply calling
```lua
defwin.set_alpha_color()
```
on init once would make the shape of the window to that of what you rendered over the clear color, that would also remove all pixels that you rendered that happen to be the clear color. To prevent that from happening, use the safealpha render script, that will make sure that your game never renders the clear color after clear.

#### defwin.set_alpha(alpha)
Set the transparency of the whole window, including the border. Can't be used together with set_alpha_color.
* `number alpha`: Opacity in [0.0, 1.0]. 0.0 = fully transparent, 1.0 = fully opaque.
```lua
defwin.set_alpha(0.8)
```

#### defwin.reset_alpha()
Remove the window transparency, restoring the original opaque window.
```lua
defwin.reset_alpha()
```

## Close handling

#### defwin.set_close_listener(listener)
Set a listener that is called instead of quitting the game when the close button is pressed.
* `function listener`: Called when a close is requested. Pass nil to clear the listener and restore default close behaviour.
```lua
defwin.set_close_listener(function(self)
    if ask_before_quit then
        show_close_popup()
    else
        defwin.quit()
    end
end)
```

#### defwin.set_close_delay(delay)
Set the time after a close request until the game exits automatically.
* `number delay`: Time in seconds before the game exits. 0 closes as soon as the close button is pressed. Pass nil for no automatic close, leaving the close listener fully in control.
```lua
defwin.set_close_delay(2)
```

#### defwin.set_close_sound_fadeout(enable)
If enabled and a close delay is set, the master sound group gain is faded out over the delay duration until shutdown.
```lua
defwin.set_close_sound_fadeout(true)
```

#### defwin.quit()
Quit the game, respecting the configured close delay and sound fade-out settings, but ignoring the close listener.
```lua
defwin.set_close_listener(function(self)
    if ask_before_quit then
        show_close_popup()
    else
        defwin.quit()
    end
end)
```

#### defwin.request_quit()
Request to quit the game, behaving as if the user pressed the close button or Alt+F4. Triggers the close listener and delay handling.
```lua
if gui.pick_node(gui.get_node("my_close_button"), x, y) then
    defwin.request_quit()
end
```

#### defwin.cancel_close()
Cancel an in-progress close attempt. Until this is called again on a future close, further attempts to close the window will not re-trigger the listener or restart the delay.
```lua
defwin.cancel_close()
```

## Style

#### defwin.set_dark_mode(enable)
Set the window style to dark mode on true, and to light mode on false.

#### defwin.get_dark_mode_preference()
Get the preference for dark mode of the current user. You can use it to apply dark mode to the window, or to use a dark theme by default in your game.

return:
* ´true´  if dark mode is preferred
* ´false´ if light mode is preferred
* ´nil´   if the preference is unknown

```lua
local preference = defwin.get_dark_mode_preference()
if preference ~= nil then
    defwin.set_dark_mode(preference)
end
```

#### defwin.set_corners_rounded()
Set the corner style of the window frame to rounded, if available.

```lua
defwin.set_corners_rounded()
```

#### defwin.set_corners_rounded_small()
Set the corner style of the window frame to slightly rounded, if available.

```lua
defwin.set_corners_rounded_small()
```

#### defwin.set_corners_not_rounded()
Set the corner style of the window frame to not rounded.

```lua
defwin.set_corners_not_rounded()
```

#### defwin.set_corners_default()
Set the corner style of the window frame to the default.

```lua
defwin.set_corners_default()
```

#### defwin.set_border_color(r, g, b)
Set window border color, with RGB in [0.0, 1.0].

´defwin.set_border_color(nil)´ for default color.

```lua
defwin.set_border_color(1.0, 0.0, 0.0)
```

#### defwin.set_border_color_none()
Set the window border color to be transparent.

```lua
defwin.set_border_color_none()
```

#### defwin.set_caption_color(r, g, b)
Set window caption color, with RGB in [0.0, 1.0].

´defwin.set_caption_color(nil)´ for default color.

```lua
defwin.set_caption_color(0.0, 1.0, 1.0)
```

#### defwin.set_caption_text_color(r, g, b)
Set window caption text color, with RGB in [0.0, 1.0].

´defwin.set_caption_text_color(nil)´ for default color.

```lua
defwin.set_caption_text_color(1.0, 0.0, 0.0)
```

#### defwin.get_caption_color()
Get the opaque color of the window caption.

return: ´vmath.vector4 color = vmath.vector4(r, g, b, 1)´ with r, g, b in [0.0, 1.0]

```lua
local color = defwin.get_caption_color()
gui.set_color(gui.get_node("window"), color)
```

#### defwin.set_rtl_layout(enable)
Set window bar layout to right-to-left, affecting buttons, title, and icon

```lua
if language == "arabic" then
    defwin.set_rtl_layout(true)
else
    defwin.set_rtl_layout(false)
end
```

#### defwin.set_tool_window(enable)
Turns the window into a tool window, or turn it back into a normal window. Tool windows are not shown on taskbar and Alt-Tab, and have no minimize and maximize buttons or icon.

```lua
defwin.set_tool_window(true)
```

## Resizing

#### defwin.lock_ratio([ratio])
Lock the aspect ratio of the window when the user drags its border.
* `number ratio`: The aspect ratio to lock (width / height). If omitted, the aspect ratio of the default resolution in game.project is used.
```lua
defwin.lock_ratio()
```

#### defwin.unlock_ratio()
Remove the aspect ratio lock set by lock_ratio.
```lua
defwin.unlock_ratio()
```

#### defwin.set_minimum_window_size(width, height)
Set the minimum size a window is allowed to have when resized.
```lua
defwin.set_minimum_window_size(192, 128)
```

#### defwin.fit_vertical([ratio])
Fit the window to the given aspect ratio by resizing only the height.
* `number ratio`: The aspect ratio to fit to (width / height). If omitted, the aspect ratio from the display size in `game.project` is used.
```lua
defwin.fit_vertical()
```

#### defwin.fit_horizontal([ratio])
Like `defwin.fit_vertical`, but only resizing width.
```lua
defwin.fit_horizontal()
```

#### defwin.fit_grow([ratio])
Uses `defwin.fit_vertical` or `defwin.fit_horizontal`, depending on which increases the window size.
```lua
defwin.fit_grow()
```

#### defwin.fit_shrink([ratio])
Uses `defwin.fit_vertical` or `defwin.fit_horizontal`, depending on which decreases the window size.
```lua
defwin.fit_shrink()
```

#### defwin.fit_screen()
Reposition and resize the window so that no part of it is outside of the screen.
```lua
defwin.fit_screen()
```

## Taskbar

#### defwin.flash(taskbar, caption, count, timeout)
Flash the window a limited number of times.
* `bool taskbar`: Should the taskbar button flash
* `bool caption`: Should the window caption flash
* `number count`: How many times to flash (default 1)
* `number timeout`: Time between flashes, in seconds (default platform cursor blink rate)
```lua
defwin.flash(true, true, 3)
```

#### defwin.start_flash(taskbar, caption, until_focused, timeout)
Start flashing the window until manually stopped or until the window regains focus.
* `bool taskbar`: Should the taskbar button flash
* `bool caption`: Should the window caption flash
* `bool until_focused`: If `true`, focusing the window stops the flash. If `false`, it flashes until `stop_flash()` is called.
* `number timeout`: Time between flashes, in seconds (default platform cursor blink rate)
```lua
defwin.start_flash(true, false)
```

#### defwin.stop_flash()
Stop the flashing of the window. As the current flash will still be completed, it can take a few seconds to disappear.
```lua
defwin.stop_flash()
```

#### defwin.set_progress(value)
Set the progress bar on the taskbar button.
* `number value`: progress in [0.0, 1.0]
```lua
defwin.set_progress(downloaded/total)
```

#### defwin.set_progress_error()
Set the progress bar on the taskbar button to an error state, turning it red.
```lua
defwin.set_progress_error()
```

#### defwin.set_progress_load()
Set the progress bar on the taskbar button to an indeterminate loading state.
```lua
defwin.set_progress_load()
```

#### defwin.pause_progress()
Pauses the progress bar on the taskbar button, turning it yellow until a new value or state is set.
```lua
defwin.pause_progress()
```

#### defwin.hide_progress()
Hide the progress bar on the taskbar button, until a new value or state is set.
```lua
defwin.hide_progress()
```

## Drag

#### defwin.start_drag()
Start dragging the window along the cursor. Must be paired with repeated calls to `update_drag()` to have any effect.

#### defwin.update_drag()
Update one frame of the window drag started with `start_drag()`. Should be called every frame (e.g. from an on_input or update callback) while dragging is active.

#### defwin.stop_drag()
Stop dragging the window along the cursor.

```lua
function on_input(self, action_id, action)
	if action_id == hash("touch") then
		if action.pressed and gui.pick_node(gui.get_node("drag"), action.x, action.y) then
			defwin.start_drag()
		end
		defwin.update_drag()
		if action.released then
			defwin.stop_drag()
		end
	end
end
```

## Activity

#### defwin.prevent_sleep(enable)
Keep the window active to prevent the system from going to sleep while enabled.
```lua
-- On start of cutscene:
defwin.prevent_sleep(true)
-- On end of cutscene:
defwin.prevent_sleep(false)
```

## Safealpha

The safealpha render script will make sure that you don't accidently render the clear color above the original clear, by shifting matching fragments just far enough to not set the window transparent.

To use it, add `safealpha.go` to your root collection, and set the render file in game.project to `/defwin/safealpha/render/safealpha.render`, or integrate it into your own render pipeline.

## Config

You can set some of the setting to be applied automatically in `game.project`:

```ini
[defwin]
lock_aspect_ratio = 0 | 1
use_dark_mode_preference = 0 | 1
set_clear_transparent = 0 | 1
rtl_layout = 0 | 1
disable_border = 0 | 1
minimum_width = 0+
minimum_height = 0+
rounded_corners = DEFAULT | ROUND | ROUNDSMALL | DONOTROUND
```

![config](/docs/images/config.png)