#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <poll.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <cairo/cairo-xlib.h>
#include <time.h>
#include <sys/select.h>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include "base/globals.hxx"
#include "ui/morphing.hxx"
#include "ui/editor.hxx"
#include "ui/splash.hxx"

// X11 clipboard globals (used by code_editor.hxx via globals.hxx)
#include "base/globals.hxx"
Display     *g_x11_dpy      = nullptr;
Window       g_x11_win      = 0;
std::string  g_x11_clipboard;


// Global definitions (declared extern in globals.hxx)
bool g_animations_enabled = true;
bool g_rounded_corners    = true;
bool g_text_optimized     = false;
bool g_line_scroll        = false;
bool g_smart_render       = false;

// GLX visual
// No depth buffer, no stencil: we only need a colour buffer for a 2-D blit.
static const int s_glx_attribs[] = {
    GLX_RGBA,
    GLX_RED_SIZE,    8,
    GLX_GREEN_SIZE,  8,
    GLX_BLUE_SIZE,   8,
    GLX_DEPTH_SIZE,  0,
    GLX_DOUBLEBUFFER,
    None
};

enum AppState { SPLASH, EDITOR };

// Returns true if any animation is currently running
// Focus mode: EWMH borderless fullscreen toggle
// Sends a standard _NET_WM_STATE_FULLSCREEN client message to the window
// manager. This is the portable way to get a truly borderless, full-screen
// window across virtually all X11 window managers (no per-WM decoration
// hacks needed) used by Textor's Focus Mode (F11).
static void set_wm_fullscreen(Display *dpy, Window win, bool enable) {
    Atom wm_state    = XInternAtom(dpy, "_NET_WM_STATE", False);
    Atom fullscreen  = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
    if (wm_state == None || fullscreen == None) return;

    XEvent xev{};
    xev.type                 = ClientMessage;
    xev.xclient.window       = win;
    xev.xclient.message_type = wm_state;
    xev.xclient.format       = 32;
    xev.xclient.data.l[0]    = enable ? 1 /* _NET_WM_STATE_ADD */ : 0 /* _NET_WM_STATE_REMOVE */;
    xev.xclient.data.l[1]    = (long)fullscreen;
    xev.xclient.data.l[2]    = 0;
    xev.xclient.data.l[3]    = 1;  // source indication: normal application
    xev.xclient.data.l[4]    = 0;
    XSendEvent(dpy, DefaultRootWindow(dpy), False,
               SubstructureNotifyMask | SubstructureRedirectMask, &xev);
    XFlush(dpy);
}

static bool any_anim_running(AppState state, const Editor &ed) {
    // Splash ALWAYS needs rendering - uses wall clock, immune to g_animations_enabled
    if (state == SPLASH) return true;

    if (!g_animations_enabled) return false;

    // Smooth cursor glide: keep rendering while cursor hasn't settled.
    // Guard against the "uninitialised" sentinel (-1.0) misreading as a
    // huge pending glide when no tab has ever run its cursor-position
    // update (e.g. zero tabs open) - that previously pinned the whole
    // app at max frame rate forever, which is most of what was behind
    // reports of constant high CPU/memory use.
    bool cursor_moving =
        ed.code.cursor_anim_x >= 0.0 && ed.code.cursor_target_x >= 0.0 &&
        (std::abs(ed.code.cursor_anim_x - ed.code.cursor_target_x) > 0.5 ||
         std::abs(ed.code.cursor_anim_y - ed.code.cursor_target_y) > 0.5);
    bool scroll_coasting =
        std::abs(ed.code.scroll_vy) > 0.1 ||
        std::abs(ed.code.scroll_vx) > 0.1;
    return ed.sidebar_anim.is_running()       ||
           ed.modal_anim.is_running()         ||
           ed.console_anim.is_running()       ||
           ed.addons_anim.is_running()        ||
           ed.file_picker.anim.is_running()   ||
           ed.theme_tween.is_running()        ||
           ed.onboarding.visible              ||
           ed.find_bar.visible                ||
           cursor_moving                      ||
           scroll_coasting;
}


int main(int argc, char **argv) {
    Display *dpy = XOpenDisplay(nullptr);
    if (!dpy) return 1;

    std::cout << "Starting editor on display via X11 path.\n";
    int screen = DefaultScreen(dpy);

    // Attempt GLX initialisation
    XVisualInfo *vi = glXChooseVisual(dpy, screen, const_cast<int*>(s_glx_attribs));
    bool use_gpu    = (vi != nullptr);
    GLXContext glctx = nullptr;
    if (use_gpu) {
        glctx    = glXCreateContext(dpy, vi, nullptr, GL_TRUE);
        use_gpu  = (glctx != nullptr);
    }

    Editor ed;

    // Open paths passed on the command line
    // `textor file.cxx`   -> opens that file in a tab
    // `textor some_dir/`  -> opens that folder in the sidebar
    // `textor a.cxx b.cxx`-> opens multiple files as tabs
    bool opened_from_cli = false;
    for (int i = 1; i < argc; i++) {
        if (!argv[i] || !argv[i][0]) continue;
        ed.open_cli_path(argv[i]);
        opened_from_cli = true;
    }

    // Create window
    Window win;
    if (use_gpu) {
        Colormap cmap = XCreateColormap(dpy, RootWindow(dpy, vi->screen),
                                         vi->visual, AllocNone);
        XSetWindowAttributes swa{};
        swa.colormap     = cmap;
        swa.event_mask   = ExposureMask | KeyPressMask | KeyReleaseMask |
                           ButtonPressMask | ButtonReleaseMask |
                           PointerMotionMask | StructureNotifyMask;
        swa.background_pixel = 0;
        win = XCreateWindow(dpy, RootWindow(dpy, vi->screen),
                            100, 100,
                            (unsigned)ed.width, (unsigned)ed.height,
                            0, vi->depth, InputOutput, vi->visual,
                            CWColormap | CWEventMask | CWBackPixel, &swa);
    } else {
        win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen),
                                  100, 100,
                                  (unsigned)ed.width, (unsigned)ed.height,
                                  0, 0, 0);
        XSelectInput(dpy, win,
            ExposureMask | KeyPressMask | KeyReleaseMask |
            ButtonPressMask | ButtonReleaseMask |
            PointerMotionMask | StructureNotifyMask);
    }

    Atom wmDelete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wmDelete, 1);
    XMapWindow(dpy, win);
    g_x11_dpy = dpy; g_x11_win = win;

    // GPU: one-time OpenGL setup
    GLuint gl_tex = 0;
    // MorphGLBatch - OpenGL accelerated widget renderer
    static morph::MorphGLBatch s_gl_batch;

    if (use_gpu) {
        glXMakeCurrent(dpy, win, glctx);
        morph::gl_load_procs();  // load GL2+ function pointers
        s_gl_batch.init();       // compile the SDF shader

        glGenTextures(1, &gl_tex);
        glBindTexture(GL_TEXTURE_2D, gl_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glEnable(GL_TEXTURE_2D);

        // Orthographic projection for the fullscreen quad
        glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(-1,1,-1,1,-1,1);
        glMatrixMode(GL_MODELVIEW);  glLoadIdentity();
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
    }

    // CPU path: Xlib-backed Cairo surface
    cairo_surface_t *xlib_surf = nullptr;
    if (!use_gpu) {
        xlib_surf = cairo_xlib_surface_create(
            dpy, win, DefaultVisual(dpy, screen), ed.width, ed.height);
    }

    // Timing / dirty state
    using clk = std::chrono::steady_clock;
    auto start_time        = clk::now();
    auto editor_fade_start = start_time;
    auto last_input_time   = start_time;
    bool needs_redraw      = true;
    bool running           = true;

    AppState state = (ed.modal.show_splash && !opened_from_cli) ? SPLASH : EDITOR;
    // Splash is a full "grand entrance" moment - start it borderless
    // fullscreen, then drop back out of fullscreen the instant it ends
    // (see the state==SPLASH block below, right where draw_splash
    // returns done==true).
    if (state == SPLASH) set_wm_fullscreen(dpy, win, true);

    // GPU blit helper
    // Track whether the texture has been fully initialised at least once.
    bool gpu_tex_full_uploaded = false;

    auto gpu_blit = [&](cairo_surface_t *img_surf) {
        if (!img_surf) return;
        cairo_surface_flush(img_surf);
        unsigned char *px = cairo_image_surface_get_data(img_surf);
        if (!px) return;

        glXMakeCurrent(dpy, win, glctx);
        glViewport(0, 0, ed.width, ed.height);
        glBindTexture(GL_TEXTURE_2D, gl_tex);

        // Cairo ARGB32 = BGRA on little-endian x86.
        // Full re-upload is required when: first frame, resize, or theme switch
        // Otherwise use glTexSubImage2D for the dirty rect only
        // dramatically less GPU bandwidth for selection / cursor blinks.
        static int last_w = 0, last_h = 0;
        bool size_changed = (ed.width != last_w || ed.height != last_h);

        if (!gpu_tex_full_uploaded || size_changed) {
            // Full upload - needed once per size change
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                         ed.width, ed.height, 0,
                         GL_BGRA, GL_UNSIGNED_BYTE, px);
            gpu_tex_full_uploaded = true;
            last_w = ed.width; last_h = ed.height;
        } else {
            // Dirty-rect partial upload
            // cairo_image_surface_get_stride() is the row byte pitch.
            // glPixelStorei(GL_UNPACK_ROW_LENGTH) lets us pass a subregion.
            int stride = cairo_image_surface_get_stride(img_surf);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, stride / 4); // pixels per row

            // Dirty rect from editor (set each frame by draw_ui)
            int dy0 = std::max(0, ed.dirty_rect_y0);
            int dy1 = std::min(ed.height, ed.dirty_rect_y1);
            if (dy1 > dy0) {
                // Upload only the dirty horizontal band
                unsigned char *band = px + (size_t)dy0 * stride;
                glTexSubImage2D(GL_TEXTURE_2D, 0,
                                0, dy0,              // x=0, y=dy0
                                ed.width, dy1 - dy0, // w, h of dirty band
                                GL_BGRA, GL_UNSIGNED_BYTE, band);
            } else {
                // Nothing changed - skip upload entirely (cursor blink idle)
            }
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0); // reset
        }

        // Cairo texture fullscreen blit (GL 1.x - intentional)
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glBegin(GL_QUADS);
            glTexCoord2f(0,1); glVertex2f(-1,-1);
            glTexCoord2f(1,1); glVertex2f( 1,-1);
            glTexCoord2f(1,0); glVertex2f( 1, 1);
            glTexCoord2f(0,0); glVertex2f(-1, 1);
        glEnd();
        // Selection/widgets overlay - MorphGLBatch (GL2+ SDF shader) below

        // GPU selection overlay via MorphGLBatch
        // SDF shader: anti-aliased rounded rectangles with one draw call.
        // Faster than the old glBegin/GL_QUADS - no matrix setup, no per-vertex call.
        if (ed.gpu_sel_active && ed.height > 0 && ed.gpu_sel_rect_count > 0) {
            // Scissor clip - не выходим за tab bar и find bar
            {
                int find_h = (ed.find_bar.visible || ed.find_bar.anim.is_running())
                             ? ed.find_bar.current_h() : 0;
                int clip_top   = ed.content_top() + find_h;
                int clip_left  = 0;
                int clip_right = ed.width;
                if (ed.split.enabled && ed.split.enable_anim > 0.1) {
                    int ed_x2 = ed.activity_bar_w + ed.current_sidebar_px();
                    int div_x = (int)ed.split.divider_x(ed_x2, ed.width - ed_x2);
                    clip_right = div_x;
                }
                // Bottom exclusion for the console/terminal panel. Without this,
                // GL y=0 (the bottom of the window) was always included in the
                // scissor rect, so a text selection drawn before the console
                // panel opened kept rendering *through* the console - this SDF
                // overlay is composited after the cairo texture blit that
                // already contains the console's own (opaque) pixels.
                int clip_bottom = (int)std::lround(ed.console_h_now());
                int scissor_h   = std::max(0, ed.height - clip_top - clip_bottom);
                glEnable(GL_SCISSOR_TEST);
                glScissor(clip_left, clip_bottom,
                          clip_right - clip_left,
                          scissor_h);
            }

            glDisable(GL_TEXTURE_2D);

            // Batch - all rectangles in one draw call
            s_gl_batch.begin(ed.width, ed.height);
            for (int si = 0; si < ed.gpu_sel_rect_count; ++si) {
                auto &sr = ed.gpu_sel_rects[si];
                if (sr.w <= 0) continue;
                s_gl_batch.add_rect(
                    (float)sr.x, (float)sr.y, (float)sr.w, (float)sr.h,
                    ed.gpu_sel_r, ed.gpu_sel_g, ed.gpu_sel_b, 0.38f,
                    3.0f   // slight rounding of selection corners
                );
            }
            s_gl_batch.flush();

            glDisable(GL_SCISSOR_TEST);
            glEnable(GL_TEXTURE_2D);
            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        }

        glXSwapBuffers(dpy, win);
        // Reset dirty rect for next frame
        ed.dirty_rect_y0 = 0;
        ed.dirty_rect_y1 = ed.height;
    };

    // Main loop
    struct timespec frame_start;
    clock_gettime(CLOCK_MONOTONIC, &frame_start);

    while (running) {
        bool had_event = false;

        while (XPending(dpy)) {
            XEvent ev; XNextEvent(dpy, &ev);
            had_event = true;


            // Clipboard: serve selection requests from other apps
            if (ev.type == SelectionRequest) {
                XSelectionRequestEvent &req = ev.xselectionrequest;
                XEvent resp;
                resp.xselection.type      = SelectionNotify;
                resp.xselection.display   = req.display;
                resp.xselection.requestor = req.requestor;
                resp.xselection.selection = req.selection;
                resp.xselection.target    = req.target;
                resp.xselection.time      = req.time;
                Atom utf8  = XInternAtom(dpy, "UTF8_STRING", False);
                Atom str_a = XA_STRING;
                Atom tgts  = XInternAtom(dpy, "TARGETS", False);
                if (req.target == tgts) {
                    Atom avail[2] = { utf8, str_a };
                    XChangeProperty(req.display, req.requestor, req.property,
                                    XA_ATOM, 32, PropModeReplace,
                                    (unsigned char*)avail, 2);
                    resp.xselection.property = req.property;
                } else if (req.target == utf8 || req.target == str_a) {
                    XChangeProperty(req.display, req.requestor, req.property,
                                    utf8, 8, PropModeReplace,
                                    (unsigned char*)g_x11_clipboard.data(),
                                    (int)g_x11_clipboard.size());
                    resp.xselection.property = req.property;
                } else {
                    resp.xselection.property = None;
                }
                XSendEvent(req.display, req.requestor, False, 0, &resp);
                XFlush(dpy);
                continue;
            }

            if (ev.type == ClientMessage &&
                (Atom)ev.xclient.data.l[0] == wmDelete)
            {
                running = false; break;
            }

            if (ev.type == ConfigureNotify) {
                ed.width  = ev.xconfigure.width;
                ed.height = ev.xconfigure.height;
                if (!use_gpu && xlib_surf)
                    cairo_xlib_surface_set_size(xlib_surf, ed.width, ed.height);
                // GPU: glViewport + glTexImage2D are updated every blit
            }

            if (state == EDITOR) {

                // Mouse motion
                if (ev.type == MotionNotify) {
                    // Coalesce: drain any further MotionNotify in the queue,
                    // keeping only the latest position.  This prevents dozens
                    // of redundant redraws during fast mouse-drag selection.
                    while (XPending(dpy)) {
                        XEvent peek; XPeekEvent(dpy, &peek);
                        if (peek.type != MotionNotify) break;
                        XNextEvent(dpy, &ev);  // discard older, keep newer
                    }
                    if (ed.modal.visible) {
                        bool handled = ed.modal.handle_mouse_move(
                            ev.xmotion.x, ev.xmotion.y, ed.width, ed.height);
                        if (handled) { ed.apply_settings_from_modal(); continue; }
                        ed.modal.handle_hover(
                            ev.xmotion.x, ev.xmotion.y, ed.width, ed.height);
                    } else {
                        ed.handle_mouse_move(ev.xmotion.x, ev.xmotion.y);
                    }
                }

                // Scroll wheel & button press
                if (ev.type == ButtonPress) {
                    if (ev.xbutton.button == 4 || ev.xbutton.button == 5) {
                        int delta = (ev.xbutton.button == 4) ? -40 : 40;

                    // Line-scroll mode: snap to whole line height instead of pixels
                    if (g_line_scroll) {
                        double lh = ed.code.cached_line_h > 1.0 ? ed.code.cached_line_h : 18.0;
                        delta = (ev.xbutton.button == 4) ? -(int)lh : (int)lh;
                    }

                        bool shift = (ev.xbutton.state & ShiftMask) != 0;
                        ed.handle_scroll(delta, ev.xbutton.x, ev.xbutton.y, shift);
                        continue;
                    }

                    if (ev.xbutton.button == 1 ||
                        ev.xbutton.button == 2 ||
                        ev.xbutton.button == 3)
                    {
                        if (ed.modal.visible) {
                            int prev = ed.modal.selected_theme_idx;
                            bool handled = ed.modal.handle_mouse_down(
                                ev.xbutton.x, ev.xbutton.y, ed.width, ed.height);
                            if (handled) {
                                if (ed.modal.selected_theme_idx != prev)
                                    ed.start_theme_transition(ed.modal.get_selected_theme());
                                ed.apply_settings_from_modal();
                                ed.save_current_settings();
                                continue;
                            } else {
                                bool ch = ed.modal.handle_click(
                                    ev.xbutton.x, ev.xbutton.y, ed.width, ed.height);
                                if (ch) {
                                    if (ed.modal.selected_theme_idx != prev)
                                        ed.start_theme_transition(ed.modal.get_selected_theme());
                                    ed.apply_settings_from_modal();
                                    ed.save_current_settings();
                                    continue;
                                }
                            }
                        }
                        bool shift_held = (ev.xbutton.state & ShiftMask)   != 0;
                        bool ctrl_held  = (ev.xbutton.state & ControlMask) != 0;

                        // Double-click detection for word selection
                        if (ev.xbutton.button == 1 &&
                            ed.settings_tab.double_click_word_select &&
                            !shift_held && !ctrl_held)
                        {
                            using clk = std::chrono::steady_clock;
                            static auto last_click_t  = clk::now() - std::chrono::seconds(5);
                            static int  last_click_x  = -999, last_click_y = -999;
                            auto now = clk::now();
                            double dt = std::chrono::duration<double>(now - last_click_t).count();
                            int dx = ev.xbutton.x - last_click_x;
                            int dy = ev.xbutton.y - last_click_y;
                            bool is_dbl = (dt < 0.40 && dx*dx + dy*dy < 16*16);
                            last_click_t = now;
                            last_click_x = ev.xbutton.x;
                            last_click_y = ev.xbutton.y;
                            if (is_dbl) {
                                // First let the single-click position the cursor, then select word
                                ed.handle_mouse_click(ev.xbutton.x, ev.xbutton.y, false, false);
                                if (ed.split.enabled && ed.split_right_pane_active)
                                    ed.code2.select_word_at_cursor();
                                else
                                    ed.code.select_word_at_cursor();
                                continue;
                            }
                        }

                        // Right button does nothing
                        if (ev.xbutton.button != 3) {
                            ed.handle_mouse_click(ev.xbutton.x, ev.xbutton.y,
                                                  shift_held, ctrl_held);
                        }
                    }
                }

                // Button release
                if (ev.type == ButtonRelease) {
                    if (ed.modal.visible) {
                        bool was = ed.modal.handle_mouse_up(
                            ev.xbutton.x, ev.xbutton.y, ed.width, ed.height);
                        if (was) {
                            ed.apply_settings_from_modal();
                            ed.save_current_settings();
                            continue;
                        }
                    }
                    ed.handle_mouse_release(ev.xbutton.x, ev.xbutton.y);
                }

                // Keyboard
                if (ev.type == KeyPress)   ed.handle_key(&ev.xkey);
                if (ev.type == KeyRelease) ed.handle_key_release(&ev.xkey);

                // Focus Mode: toggled via F11 inside ed.handle_key(); apply the
                // actual borderless-fullscreen window state change here.
                static bool s_focus_mode_applied = false;
                if (ed.focus_mode != s_focus_mode_applied) {
                    s_focus_mode_applied = ed.focus_mode;
                    set_wm_fullscreen(dpy, win, ed.focus_mode);
                }
            }
        } // event drain

        if (ed.request_quit) { running = false; break; }

        if (had_event) {
            needs_redraw   = true;
            last_input_time = clk::now();
        }

        // Render?
        auto   now_t  = clk::now();
        double since  = std::chrono::duration<double>(now_t - last_input_time).count();
        bool   anim   = any_anim_running(state, ed);

        // Cursor blink: needs a redraw every ~530 ms even when fully idle.
        // We track the last blink-render time to avoid full 60fps for just a blink.
        static auto last_blink_render = now_t;
        bool blink_due = state == EDITOR &&
            std::chrono::duration<double>(now_t - last_blink_render).count() > 0.48;
        if (blink_due) { last_blink_render = now_t; }

        // Keep rendering for 0.5 s after last input to finish fade-outs
        bool   active = anim || since < 0.5 || needs_redraw || blink_due;

        if (active) {
            if (state == SPLASH) {
                // Splash
                if (use_gpu) {
                    cairo_surface_t *tmp =
                        cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                                   ed.width, ed.height);
                    cairo_t *cr = cairo_create(tmp);
                    if (cr) {
                        bool done = draw_splash(cr, ed.width, ed.height,
                                                start_time, now_t);
                        if (done) {
                            state = EDITOR;
                            editor_fade_start = now_t;
                            ed.sidebar.refresh();
                            set_wm_fullscreen(dpy, win, false);
                        }
                        cairo_destroy(cr);
                        gpu_blit(tmp);
                    }
                    cairo_surface_destroy(tmp);
                } else {
                    cairo_t *cr = cairo_create(xlib_surf);
                    if (cr) {
                        bool done = draw_splash(cr, ed.width, ed.height,
                                                start_time, now_t);
                        if (done) {
                            state = EDITOR;
                            editor_fade_start = now_t;
                            ed.sidebar.refresh();
                            set_wm_fullscreen(dpy, win, false);
                        }
                        cairo_destroy(cr);
                        XFlush(dpy);
                    }
                }
            } else {
                // Editor
                float elapsed = std::chrono::duration<float>(
                                    now_t - editor_fade_start).count();

                // Inertial scroll: apply velocity -> friction each frame
                if (std::abs(ed.code.scroll_vy) > 0.1 ||
                    std::abs(ed.code.scroll_vx) > 0.1) {
                    double con_h2 = ed.console_h_now();
                    double eff_h2 = (double)ed.height - con_h2 - ed.content_top();
                    double max_sy = std::max(0.0,
                        ed.code.total_text_height - eff_h2 + 100.0);
                    double tx2    = (double)ed.text_area_x_now();
                    // Same fix as Editor::handle_scroll: in split mode the
                    // left/first pane's visible width stops at the divider,
                    // not the full window. Using the full window width here
                    // (as this inertia tick used to) inflated view_w2 on
                    // every single frame while the scroll was coasting,
                    // which shrank max_sx and clamped scroll_x straight back
                    // down - so the keyboard/wheel horizontal scroll on
                    // screen 1 looked like it briefly moved and then
                    // snapped back to nothing whenever split was enabled.
                    double right_edge2 = (double)ed.width;
                    if (ed.split.enabled) {
                        int ed_x3 = ed.activity_bar_w + ed.current_sidebar_px();
                        right_edge2 = ed.split.divider_x(ed_x3, ed.width - ed_x3);
                    }
                    double view_w2 = std::max(1.0, right_edge2 - tx2 - 20.0);
                    double max_sx = std::max(0.0,
                        ed.code.total_text_width - view_w2 + 100.0);

                    ed.code.scroll_y = std::clamp(
                        ed.code.scroll_y + ed.code.scroll_vy, 0.0, max_sy);
                    ed.code.scroll_x = std::clamp(
                        ed.code.scroll_x + ed.code.scroll_vx, 0.0, max_sx);

                    // Friction - multiply per frame (~60fps -> ~150ms coast)
                    const double FRICTION = 0.82;
                    ed.code.scroll_vy *= FRICTION;
                    ed.code.scroll_vx *= FRICTION;
                    if (std::abs(ed.code.scroll_vy) < 0.1) ed.code.scroll_vy = 0.0;
                    if (std::abs(ed.code.scroll_vx) < 0.1) ed.code.scroll_vx = 0.0;
                    needs_redraw = true;
                }

                // Right pane inertial scroll
                if (ed.split.enabled &&
                    (std::abs(ed.code2.scroll_vy) > 0.1 ||
                     std::abs(ed.code2.scroll_vx) > 0.1)) {
                    double con_h2  = ed.console_h_now();
                    double eff_h2  = (double)ed.height - con_h2 - ed.content_top();
                    double max_sy2 = std::max(0.0, ed.code2.total_text_height - eff_h2 + 100.0);
                    int ed_x2 = ed.activity_bar_w + ed.current_sidebar_px();
                    int rx2, rw2;
                    ed.split.right_rect(ed_x2, ed.width - ed_x2, rx2, rw2);
                    double view_w2 = std::max(1.0, (double)rw2 - ed.gutter_width - 20.0);
                    double max_sx2 = std::max(0.0, ed.code2.total_text_width - view_w2 + 100.0);

                    ed.code2.scroll_y = std::clamp(
                        ed.code2.scroll_y + ed.code2.scroll_vy, 0.0, max_sy2);
                    ed.code2.scroll_x = std::clamp(
                        ed.code2.scroll_x + ed.code2.scroll_vx, 0.0, max_sx2);

                    const double FRICTION2 = 0.82;
                    ed.code2.scroll_vy *= FRICTION2;
                    ed.code2.scroll_vx *= FRICTION2;
                    if (std::abs(ed.code2.scroll_vy) < 0.1) ed.code2.scroll_vy = 0.0;
                    if (std::abs(ed.code2.scroll_vx) < 0.1) ed.code2.scroll_vx = 0.0;
                    needs_redraw = true;
                }

                if (use_gpu) {
                    // CPU renders to s_frame_buf (no Xlib paint)
                    draw_ui(nullptr, nullptr, ed);
                    cairo_surface_t *fb = get_frame_buf();

                    // Fade-in overlay on top of the raw frame
                    if (elapsed < 1.0f && fb) {
                        cairo_t *ov = cairo_create(fb);
                        if (ov) {
                            cairo_set_source_rgba(ov, 0, 0, 0, 1.0 - elapsed);
                            cairo_rectangle(ov, 0, 0, ed.width, ed.height);
                            cairo_fill(ov);
                            cairo_destroy(ov);
                        }
                    }
                    gpu_blit(fb);
                } else {
                    cairo_t *cr = cairo_create(xlib_surf);
                    if (cr) {
                        draw_ui(cr, xlib_surf, ed);
                        if (elapsed < 1.0f) {
                            cairo_set_source_rgba(cr, 0, 0, 0, 1.0 - elapsed);
                            cairo_rectangle(cr, 0, 0, ed.width, ed.height);
                            cairo_fill(cr);
                        }
                        cairo_destroy(cr);
                        XFlush(dpy);
                    }
                }
            }
            needs_redraw = false;
        }

        // Frame pacing event-driven sleep, minimal CPU
        //
        // select() on the X11 fd with a timeout = remaining frame budget.
        // The process blocks in the kernel until an event arrives OR the
        // timeout fires - giving 0% CPU when the editor is idle.
        //
        // Tiers (budget = max time we wait before next render):
        //   SPLASH / animating       -> 16 667 us  (60 fps - smooth)
        //   Recent input  (< 0.5 s)  -> 16 667 us  (60 fps - responsive)
        //   Cursor blink  (0.5-3 s)  -> 33 333 us  (30 fps - saves ~50% CPU)
        //   Fully idle    (> 3 s)    -> 500 000 us  (2 fps wake - ~0% CPU)
        //     At 2 fps we still catch cursor blink (530 ms period) and
        //     redraw on any event immediately thanks to select() early-wake.
        {
            struct timespec now_ts;
            clock_gettime(CLOCK_MONOTONIC, &now_ts);
            long elapsed_us = (now_ts.tv_sec  - frame_start.tv_sec)  * 1000000L
                            + (now_ts.tv_nsec - frame_start.tv_nsec) / 1000L;

            double since_input = std::chrono::duration<double>(
                clk::now() - last_input_time).count();

            long budget_us;
            if (state == SPLASH || anim || since_input < 0.5)
                budget_us = 16667L;    // 60 fps
            else if (since_input < 3.0)
                budget_us = 33333L;    // 30 fps - cursor blink
            else
                budget_us = 500000L;   // 2 fps - fully idle, near-zero CPU

            long sleep_us = budget_us - elapsed_us;
            if (sleep_us > 200L) {
                int xfd = ConnectionNumber(dpy);
                fd_set rfds;
                FD_ZERO(&rfds);
                FD_SET(xfd, &rfds);
                struct timeval tv_wait;
                tv_wait.tv_sec  = sleep_us / 1000000L;
                tv_wait.tv_usec = sleep_us % 1000000L;
                // Blocks until X event arrives or timeout - zero CPU burn
                select(xfd + 1, &rfds, nullptr, nullptr, &tv_wait);
            }

            clock_gettime(CLOCK_MONOTONIC, &frame_start);
        }
    }

    // Cleanup
    if (s_frame_buf)   cairo_surface_destroy(s_frame_buf);
    if (xlib_surf)     cairo_surface_destroy(xlib_surf);
    if (use_gpu && gl_tex) glDeleteTextures(1, &gl_tex);
    if (use_gpu) {
        glXMakeCurrent(dpy, None, nullptr);
        glXDestroyContext(dpy, glctx);
    }
    if (vi) XFree(vi);
    s_gl_batch.destroy();
    XCloseDisplay(dpy);
    return 0;
}
