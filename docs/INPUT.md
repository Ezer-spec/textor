# Textor input handling

Documentation for how mouse/keyboard input is dispatched. The
implementation lives in `ui/editor.hxx` (`struct Editor`) - this file used
to be `ui/input_handler.hxx`, a documentation-only header nothing
included (and which had gone stale: it still described tabs as a "tab
bar", from before they moved into the floating panel - see
`docs/TABS.md`).

All input is processed in `struct Editor` through:
- `handle_mouse_click(x, y, shift, ctrl)`
- `handle_mouse_move(x, y)`
- `handle_mouse_release(x, y)`
- `handle_scroll(delta, x, y, shift)`
- `handle_key(XKeyEvent*)`
- `handle_key_release(XKeyEvent*)`

## Mouse click Z-order

`handle_mouse_click` checks regions top to bottom, first match wins:

1. Find bar (Ctrl+F panel)
2. Modal window (Settings modal)
3. Save button
4. Welcome screen (when no files are open)
5. File picker overlay
6. Top bar (Explorer toggle - Split/Addons/Console + language - Settings/Exit)
7. Floating tab panel (right edge of the viewport)
8. Sidebar (file tree)
9. Console resize handle
10. Console panel
11. Settings tab (when active)
12. Scrollbar (vertical / horizontal)
13. Text area (cursor positioning, start of selection)

## Drag state

Stored on `Editor`:
- `dragging_v` / `dragging_v2` - dragging the vertical scrollbar (main /
  split-right pane)
- `dragging_h` - dragging the horizontal scrollbar
- `text_drag_active` - dragging out a text selection
- `console_resize_active` - dragging the console's resize border

Drags start in `handle_mouse_click` (ButtonPress), continue in
`handle_mouse_move` (MotionNotify), end in `handle_mouse_release`
(ButtonRelease).

## Keyboard focus regions

`Editor::ui_focus` (`UIFocus::EditorText` / `Sidebar` / `TabPanel` /
`Console`) tracks which region owns arrow-key/Enter navigation, so typing
in the code buffer is unaffected unless a panel explicitly has focus:

- `Ctrl+1..4` - focus Sidebar / Editor / Tab panel / Console
- `Ctrl+Alt+Left/Right` - cycle focus between whichever of those are
  currently visible
- Clicking the code area, a sidebar row, or the console returns/sets
  focus accordingly
- Sidebar focused: Up/Down move the selection, Left/Right
  collapse/expand a folder, Enter opens the file
- Tab panel focused: Up/Down move the highlighted tab, Enter switches to
  it, Delete/Backspace closes it
- Settings tab active: Left/Right (or PageUp/PageDown) switch sections,
  Escape closes it, Ctrl+, toggles it open/closed

## Focus Mode (F11)

Borderless fullscreen via an EWMH `_NET_WM_STATE_FULLSCREEN` client
message (`base.cxx`, `set_wm_fullscreen`). While active, the top bar and
(if it was open) the sidebar slide out of view and only reveal themselves
while the pointer hovers the corresponding screen edge - see
`focus_topbar_anim`, `focus_sidebar_hover`, `focus_mode` on `Editor`.
Escape exits Focus Mode.

## Shortcuts

Loaded from `shortcuts.conf` via `ShortcutManager` (`core/`); checked in
`handle_key` via `settings_tab.shortcuts.matches(name, ev)`. See
`shortcuts.conf` for the full current list - it now also covers focus
regions, Focus Mode, and Settings navigation, not just the original
save/open/tabs/etc. set.

## Auto-scroll while selecting with the mouse

When `text_drag_active == true` and the pointer is near the top/bottom
edge, `handle_mouse_move` scrolls automatically:
- `EDGE_MARGIN` = 40px - trigger zone at the top/bottom border
- `EDGE_SPEED` = 18px/event - autoscroll speed

(Separately, `CodeEditor::ensure_cursor_visible` keeps the *keyboard*
cursor on-screen after any keypress, including arrow-key navigation -
unrelated to this mouse-drag case.)

## Inertial scrolling

`scroll_vy` / `scroll_vx` are the current scroll velocity (px/frame). The
mouse wheel adds to it rather than jumping straight to the target
position; `FRICTION = 0.82` damps it out (~150ms of inertia at 60fps).

## Scrollbar vs. floating tab panel

The main editor scrollbar always sits at the true right edge of the
window (it does not move to avoid the tab panel). Instead, while it's
being pressed, held, or scrolled, the floating tab panel collapses and
nudges itself further left for the duration
(`Editor::sb_mark_active()`/`sb_recently_active()`,
`tabpanel_sb_avoid_anim`), so the two never overlap.
