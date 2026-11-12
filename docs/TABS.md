# Textor tab system

Documentation for how open files are tracked. The implementation lives in
`ui/editor.hxx` (`struct Editor`) - this file used to be `ui/tab_manager.hxx`,
a documentation-only header nothing included (and which had gone stale,
still describing the old horizontal tab strip and its `tab_hit_test`/
`tab_scroll_x`, both removed when tabs moved into the floating panel on the
right edge of the editor - see below).

Multiple files can be open at once. Each tab stores the *full* state of the
editor for that file: buffer, cursor, scrolling, and undo/redo history.
Switching tabs saves the outgoing tab's state and restores the incoming
one.

## Settings overlay

Settings is **not** one of the tabs in `Editor::tabs` - it's a standalone
overlay (same idea as `file_picker.visible`/`console.visible`), tracked by
`Editor::settings_open` and exposed through `Editor::is_settings_tab_active()`.
Opening it (`open_settings_tab()`) and closing it (`close_settings()`) never
touch `tabs` or `active_tab` at all, so:
- the floating tab panel disappears entirely while Settings is open (it's
  hidden in `draw_floating_tab_panel`, and `tabpanel_hit_test`/
  `tabpanel_thumb_hit` refuse to hit-test while it's up) - Settings never
  shows up as a row in it, and closing Settings brings the same real tabs
  straight back, exactly as they were;
- Settings is reached and closed only through the top bar (the gear icon -
  clicking it again while open closes it, same as Escape or the `settings`
  shortcut, Ctrl+,) - there is no x for it in the tab panel anymore since
  it was never a row there.
When active, `draw_ui`/`draw_code_editor_body` show `SettingsTab` in place
of `CodeEditor`, without disturbing whatever real tab/buffer was active
underneath.

## Data

- `Editor::tabs` - vector of all open tabs (`Editor::Tab`)
- `Editor::active_tab` - index of the active tab

## Basic operations

- `open_file_in_tab(filename, content)` - open a file, or switch to it if
  it's already open
- `switch_to_tab(idx)` - switch the active tab, saving the outgoing one's
  state first
- `close_tab(idx)` - close a tab and free its memory
- `open_settings_tab()` / `close_settings()` - show/hide the Settings
  overlay (see "Settings overlay" above - does not touch `tabs`)

## The floating tab panel (current UI)

Tabs are **not** a horizontal strip anymore - they live in a small
vertical panel that floats over the right edge of the editor viewport.
Collapsed, it shows just each tab's file-type icon; hovering it eases it
open toward the left to also show filenames (and, once mostly open, a
close button per row).

Relevant `Editor` members:
- `tabpanel_x0()`, `tabpanel_y0()`, `tabpanel_current_w()`,
  `tabpanel_visible_h()` - the panel's current on-screen geometry
- `tabpanel_hit_test(mx, my, close_hit)` - which tab (if any) was clicked;
  `close_hit` is set if the x was hit
- `tabpanel_scroll_y` / `tabpanel_scroll_max()` - vertical scroll once the
  open tabs overflow the panel's height
- `tabpanel_kbd_idx` - keyboard-highlighted row (Ctrl+3 to focus the
  panel, then Up/Down/Enter/Delete)
- `tabpanel_sb_avoid_anim` - extra left-push applied while the editor's
  *main* vertical scrollbar is being pressed/held/scrolled, so the two
  never overlap (the scrollbar itself always stays at the true right edge)

## Saved per tab

`switch_to_tab` saves into `Tab`: buffer, `cursor_pos`, `sel_anchor`,
`sel_active`, `scroll_y`, `scroll_x`, `dirty`, `undo_stack`, `redo_stack`.

## Memory

Undo/redo stacks can get large (hundreds of KB for big files). `close_tab`
explicitly releases them via the swap trick:
```cpp
{ std::vector<UndoRecord> tmp; tmp.swap(t.undo_stack); }
```
so the memory comes back immediately instead of waiting on the next
allocator reuse.

## Typical code

```cpp
// Open a file:
std::ifstream ifs(path);
std::string content((std::istreambuf_iterator<char>(ifs)), {});
editor.open_file_in_tab(path.string(), content);

// Check whether the Settings tab is active:
if (editor.is_settings_tab_active()) { ... }

// Close the current tab (same as Ctrl+W):
editor.close_tab(editor.active_tab);
```
