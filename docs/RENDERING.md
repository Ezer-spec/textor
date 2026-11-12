# Textor rendering pipeline

This describes **how** UI rendering works. The implementation itself lives
in `ui/editor.hxx` (function `draw_ui` + `s_frame_buf`) - this file used to
be `ui/renderer.hxx`, a header containing nothing but this comment and an
`#include "editor.hxx"`. Nothing included it, so it did nothing at build
time; the documentation has been moved here instead of deleted outright.

## Pipeline (per frame)

1. **`base.cxx` - main loop**
   - Receives X11 events (and macOS events, on that platform)
   - Decides whether a redraw is needed (`any_anim_running`)
   - Calls `draw_ui(cr, surf, editor)`

2. **`draw_ui` (`ui/editor.hxx`) - CPU render into `s_frame_buf` (Cairo ARGB32)**
   - Activity bar
   - Tab bar
   - Gutter (line numbers)
   - Text area -> `CodeEditor::draw_text_area`
   - Find bar (Ctrl+F)
   - Scrollbars
   - File picker overlay
   - Console panel
   - Sidebar overlay (on top of everything)
   - Addons panel overlay

3. **GPU path (Linux GLX / macOS Metal)**
   - `gpu_blit()` loads `s_frame_buf` into the GL texture
     - Optimization: dirty-rect -> `glTexSubImage2D` (partial update)
   - GL overlay draws text selection via OpenGL (faster than Cairo for
     large allocations)

## Rendering modes

**GPU (priority):**
`draw_ui` is called with `xlib_cr = nullptr`; the result is read via
`get_frame_buf()`. The OpenGL overlay draws selection directly to the GPU.

**CPU (fallback if GLX is not available):**
`draw_ui` is called with a real Cairo `xlib_cr`; selection is drawn
directly via Cairo.

## Optimizations

**`dirty_rect`:**
`ed.dirty_rect_y0` / `ed.dirty_rect_y1` mark the range of lines that
changed this frame. `gpu_blit()` updates only that horizontal strip via
`glTexSubImage2D`. A full redraw (resize, theme change) covers `0..height`.

**Sleep tiers (`base.cxx`):**
- Splash / animation -> 60 fps (16.7 ms)
- After input, < 0.5s -> 60 fps
- Cursor blinking -> 30 fps (33 ms)
- Fully idle > 3s -> 2 fps (500 ms, close to 0% CPU)

**SmartRender (`g_smart_render`):**
`CodeEditor::draw_text_area` only draws visible lines, skipping anything
outside the viewport (`start_ln..end_ln`).

## Adding a new UI element

1. Declare the element's data in `struct Editor` (`ui/editor.hxx`).
2. In `draw_ui`, draw it via Cairo at the right Z-order.
3. In `handle_mouse_click` / `handle_mouse_move`, add its hit-test.
4. If the element floats above the code, draw it **after**
   `ed.code.draw_text_area`.
