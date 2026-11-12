// A single widget library that unites and standardizes:
// * Animations (spring, tween, easing)
// * Drawing primitives (rounded rect, shadow, glow)
// * ToggleButton, TextButton, TabButton, BarIcon
// * ScrollBar (vertical + horizontal)
// * Slider (value, range)
// * ProgressBar
// * Tooltip
// * Badge / chip
// * Context Menu
// * OpenGL-accelerated overlay batch
//
// GOAL: one file instead of button.hxx + bar_button.hxx + scroll_bar.hxx +
// animations.hxx + draw_helpers.hxx - for readability and consistency.
//
// HOW TO CONNECT:
// #include "ui/morphing.hxx"
// // IN ONE translation unit (.cxx) add:
// #include "ui/morphing.cxx"
//
// REPLACE OLD HEADERS (for backwards compatibility):
// button.hxx -> use MorphButton, MorphToggle, MorphTabButton
// bar_button.hxx -> use MorphBarIcon
// scroll_bar.hxx -> use MorphScrollBar
// animations.hxx -> use MorphAnim, MorphSpring, MorphTween
// draw_helpers.hxx -> use morph::rrect(), morph::shadow()
//
// OPENGL-OVERLAY:
// MorphGLBatch batch;
// batch.begin(win_w, win_h);
// batch.add_rect({x, y, w, h}, {r, g, b, a}, radius);
// batch.flush(); // one draw call for the entire batch
#pragma once

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include <cmath>
#include <algorithm>
#include <chrono>
#include <string>
#include <vector>
#include <functional>

#include <cairo/cairo.h>
#include <pango/pangocairo.h>

#include "../color/themes.hxx"
#include "../base/globals.hxx"

// .............................................................................
//  OpenGL headers - optional, guarded by MORPH_NO_GL
// .............................................................................
#ifndef MORPH_NO_GL
#  include <GL/gl.h>
#  include <GL/glext.h>
#  include <GL/glx.h>
#  define MORPH_GL 1
// GL 2.1 function pointers (loaded at runtime via glXGetProcAddress)
// Declare extern - defined in morphing.cxx via morph::gl_load_procs()
namespace morph_gl {
    extern PFNGLCREATESHADERPROC           glCreateShader;
    extern PFNGLSHADERSOURCEPROC           glShaderSource;
    extern PFNGLCOMPILESHADERPROC          glCompileShader;
    extern PFNGLGETSHADERIVPROC            glGetShaderiv;
    extern PFNGLGETSHADERINFOLOGPROC       glGetShaderInfoLog;
    extern PFNGLDELETESHADERPROC           glDeleteShader;
    extern PFNGLCREATEPROGRAMPROC          glCreateProgram;
    extern PFNGLATTACHSHADERPROC           glAttachShader;
    extern PFNGLBINDATTRIBLOCATIONPROC     glBindAttribLocation;
    extern PFNGLLINKPROGRAMPROC            glLinkProgram;
    extern PFNGLGETPROGRAMIVPROC           glGetProgramiv;
    extern PFNGLGETPROGRAMINFOLOGPROC      glGetProgramInfoLog;
    extern PFNGLDELETEPROGRAMPROC          glDeleteProgram;
    extern PFNGLUSEPROGRAMPROC             glUseProgram;
    extern PFNGLGETUNIFORMLOCATIONPROC     glGetUniformLocation;
    extern PFNGLUNIFORM2FPROC              glUniform2f;
    extern PFNGLGENBUFFERSPROC             glGenBuffers;
    extern PFNGLDELETEBUFFERSPROC          glDeleteBuffers;
    extern PFNGLBINDBUFFERPROC             glBindBuffer;
    extern PFNGLBUFFERDATAPROC             glBufferData;
    extern PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
    extern PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;
    extern PFNGLVERTEXATTRIBPOINTERPROC    glVertexAttribPointer;
    // glDrawArrays is GL 1.1 core - typedef below replaces missing proc type
    typedef void (APIENTRYP MORPH_PFNGLDRAWARRAYSPROC)(GLenum mode, GLint first, GLsizei count);
    extern MORPH_PFNGLDRAWARRAYSPROC       glDrawArrays;
}
// Load all pointers - call once after glXMakeCurrent
namespace morph { void gl_load_procs() noexcept; }
#else
#  define MORPH_GL 0
#endif

// .............................................................................
//  Namespace: morph
// All utilities and data types live here
// .............................................................................
namespace morph {

// SECTION 1 - Time & Clock

/// Current time in seconds (monotonic clock, used everywhere in the library)
inline double now() noexcept {
    using namespace std::chrono;
    return duration_cast<duration<double>>(
        steady_clock::now().time_since_epoch()).count();
}

/// Clamp to range [lo, hi]
template<typename T>
inline T clamp(T v, T lo, T hi) noexcept { return v < lo ? lo : v > hi ? hi : v; }

/// Linear interpolation
inline double lerp(double a, double b, double t) noexcept { return a + (b - a) * t; }


// SECTION 2 - Easing Functions

inline double ease_linear      (double t) noexcept { return clamp(t, 0.0, 1.0); }
inline double ease_smooth      (double t) noexcept { t = clamp(t,0.,1.); return t*t*(3.-2.*t); }
inline double ease_out_cubic   (double t) noexcept { t = clamp(t,0.,1.); double r=1.-t; return 1.-r*r*r; }
inline double ease_in_out_cubic(double t) noexcept {
    t = clamp(t,0.,1.);
    return t < 0.5 ? 4.*t*t*t : 1. - std::pow(-2.*t+2., 3.) / 2.;
}
inline double ease_out_expo    (double t) noexcept {
    t = clamp(t,0.,1.);
    return t == 0. ? 0. : 1. - std::pow(2., -10.*t);
}
inline double ease_out_spring  (double t) noexcept {
    t = clamp(t,0.,1.);
    if (t == 0. || t == 1.) return t;
    const double c4 = (2. * M_PI) / 2.5;
    return std::pow(2., -10.*t) * std::sin((t*10. - 0.75) * c4) + 1.;
}
inline double ease_out_back    (double t) noexcept {
    t = clamp(t,0.,1.);
    const double c1 = 1.70158, c3 = c1 + 1.;
    return 1. + c3*(t-1.)*(t-1.)*(t-1.) + c1*(t-1.)*(t-1.);
}

//SECTION 3 - Spring Physics

/// Single value with spring physics (numerical integration)
struct Spring {
    double value    = 0.0;
    double velocity = 0.0;
    double target   = 0.0;
    double stiffness = 900.0;  ///< Hardness (higher = faster)
    double damping   = 60.0;   ///< Damping (higher = less vibration)

    void set_target(double t) noexcept { target = t; }

    /// Immediate jump without animation
    void snap(double v) noexcept { value = velocity = 0.; target = value = v; }

    void update(double dt) noexcept {
        if (!g_animations_enabled) { value = target; velocity = 0.; return; }
        dt = std::min(dt, 0.05);
        // Sub-steps for stability at high stiffness
        int steps = std::max(1, static_cast<int>(dt / 0.008));
        double sub_dt = dt / steps;
        for (int i = 0; i < steps; ++i) {
            double force = stiffness * (target - value) - damping * velocity;
            velocity += force * sub_dt;
            value    += velocity * sub_dt;
        }
        if (std::abs(value - target) < 0.001 && std::abs(velocity) < 0.01)
            value = target, velocity = 0.;
    }

    bool settled() const noexcept {
        return std::abs(value - target) < 0.001 && std::abs(velocity) < 0.01;
    }
};

// SECTION 4 - Scalar Animation (smoothstep 0->1)

/// Animate a scalar value between 0 and 1
/// Used for hover/active/press states of widgets
struct ScalarAnim {
    double value = 0.0;
    double last  = now();

    /// The value ticks towards the target at speed (1/s)
    void tick_to(double target, double speed = 14.0) noexcept {
        double t  = morph::now();
        double dt = std::min(0.06, t - last);
        last = t;
        value += (target - value) * clamp(speed * dt, 0.0, 1.0);
    }

    operator double() const noexcept { return value; }
};


// SECTION 5 - AnimController (timed open/close)

/// Timer animation - movement 0->1 or 1->0 for a fixed time
struct AnimController {
    std::chrono::steady_clock::time_point t0;
    double dur     = 0.25;
    bool running   = false;
    bool opening   = true;

    AnimController() noexcept = default;
    /*
        Construct already settled in a given resting state - no animation
        plays until start() is actually called. Needed for any controller
        whose rest state should read as 0/closed rather than the default
        1/open: without this, progress() reports 1.0 ("fully open") from
        the very first frame, before start() has ever run once, which
        looks like that effect is already active by default instead of
        only appearing once the real trigger for it happens.
    */
    explicit AnimController(bool initial_open) noexcept
        : running(false), opening(initial_open) {}

    void start(bool open, double duration = 0.25) noexcept {
        t0 = std::chrono::steady_clock::now();
        dur = std::min(duration, 0.12); running = true; opening = open;
        if (!g_animations_enabled) running = false;
    }

    double progress() const noexcept {
        if (!g_animations_enabled) return opening ? 1.0 : 0.0;
        if (!running) return opening ? 1.0 : 0.0;
        using namespace std::chrono;
        double e = duration_cast<duration<double>>(steady_clock::now() - t0).count();
        double t = clamp(e / dur, 0.0, 1.0);
        if (t >= 1.0) { const_cast<AnimController*>(this)->running = false; return opening ? 1.0 : 0.0; }
        return opening ? ease_smooth(t) : 1.0 - ease_smooth(t);
    }

    bool is_running() const noexcept { return g_animations_enabled && running; }
};


// SECTION 6 - ThemeTween (colour cross-fade)

/// Smooth transition between two themes
struct ThemeTween {
    ThemeVariant from, to;
    std::chrono::steady_clock::time_point t0;
    double dur     = 0.35;
    bool running   = false;

    void start(const ThemeVariant &a, const ThemeVariant &b, double duration = 0.35) noexcept {
        from = a; to = b; dur = std::min(duration, 0.15);
        t0   = std::chrono::steady_clock::now();
        running = true;
        if (!g_animations_enabled) running = false;
    }

    ThemeVariant current() noexcept {
        if (!g_animations_enabled || !running) return to;
        using namespace std::chrono;
        double e = duration_cast<duration<double>>(steady_clock::now() - t0).count();
        double t = clamp(e / dur, 0.0, 1.0);
        if (t >= 1.0) { running = false; return to; }
        return interpolate_theme(from, to, ease_in_out_cubic(t));
    }

    bool is_running() const noexcept { return g_animations_enabled && running; }
};


// SECTION 7 - SlideIndicator (animated pill / nav rail)

/// Sliding active element indicator (like Android nav rail)
struct SlideIndicator {
    Spring pos_y  { 0., 0., 0., 700., 53. };
    Spring height { 0., 0., 0., 700., 53. };

    std::chrono::steady_clock::time_point last_tick;
    bool initialized = false;

    void jump_to(double y, double h) noexcept {
        pos_y.snap(y); height.snap(h); initialized = true;
        last_tick = std::chrono::steady_clock::now();
    }

    void target_at(double y, double h) noexcept {
        if (!initialized) { jump_to(y, h); return; }
        pos_y.set_target(y);
        height.set_target(h);
    }

    void tick() noexcept {
        auto now_tp = std::chrono::steady_clock::now();
        if (!initialized) { last_tick = now_tp; return; }
        double dt = std::chrono::duration_cast<std::chrono::duration<double>>(
            now_tp - last_tick).count();
        last_tick = now_tp;
        pos_y.update(dt);
        height.update(dt);
    }

    bool needs_redraw() const noexcept { return !pos_y.settled() || !height.settled(); }
};

// SECTION 8 - Cairo Draw Primitives
/// Constructs the path of a rounded rectangle (does not fill, does not draw a stroke).
/// Respects g_rounded_corners - if false, uses cairo_rectangle.
inline void rrect(cairo_t *cr, double x, double y, double w, double h, double r) noexcept {
    shape::rrect(cr, x, y, w, h, r);
}

/// Rounded rectangle without checking g rounded corners (forced)
inline void rrect_forced(cairo_t *cr, double x, double y, double w, double h, double r) noexcept {
    shape::path(cr, x, y, w, h, r);
}

/// Draws a tone (soft shadow) under the rectangle through several fills
inline void shadow(cairo_t *cr, double x, double y, double w, double h, double r,
                   double blur = 8.0, double alpha = 0.25) noexcept {
    (void)cr; (void)x; (void)y; (void)w; (void)h; (void)r; (void)blur; (void)alpha;  // strict style: no shadows
}

/// Draws a glow under the rectangle with an accent color
inline void glow(cairo_t *cr, double x, double y, double w, double h, double r,
                 double ar, double ag, double ab,
                 double blur = 10.0, double alpha = 0.35) noexcept {
    (void)cr; (void)x; (void)y; (void)w; (void)h; (void)r; (void)ar; (void)ag; (void)ab; (void)blur; (void)alpha;  // strict style: no glow
}

/// Auxiliary widget state flags (bitfield)
struct WidgetState {
    bool hovered  = false;
    bool pressed  = false;
    bool focused  = false;
    bool disabled = false;
    bool active   = false;  // "selected" / enabled
};

/// Widget geometry
struct Rect {
    double x = 0, y = 0, w = 0, h = 0;
    bool contains(double mx, double my) const noexcept {
        return mx >= x && mx <= x+w && my >= y && my <= y+h;
    }
};

// SECTION 9 - MorphToggle (Toggle / Switch)
/// Toggle button in Switch
/// Complete replacement of ToggleButton from button.hxx
struct MorphToggle {
    Rect bounds { 0, 0, 52, 28 };
    bool value   = false;
    bool hovered = false;
    bool pressed = false; ///< true while the mouse button is held down

    ScalarAnim anim_value; ///< position thumb (0=off, 1=on)
    ScalarAnim anim_hover; ///< halo on hover
    ScalarAnim anim_squeeze; ///< compression thumb during transition (0=normal, 1=squeezed)

    bool is_hit(double mx, double my) const noexcept { return bounds.contains(mx, my); }

    void tick() noexcept {
        // A little slower - the squeeze animation has time to play
        anim_value.tick_to(value ? 1.0 : 0.0, 10.0);
        anim_hover.tick_to(hovered ? 1.0 : 0.0, 16.0);
        // anim_squeeze is not needed - squeeze is calculated in draw() from anim_value.value and pressed
    }

    bool needs_redraw() const noexcept {
        return std::abs(anim_value.value - (value ? 1.0 : 0.0)) > 0.002 ||
               std::abs(anim_hover.value - (hovered ? 1.0 : 0.0)) > 0.002;
    }

    void toggle() noexcept { value = !value; }

    void draw(cairo_t *cr, const ThemeVariant &tv) noexcept;
};


// SECTION 10 - MorphButton (Text Button)

/// Text button
/// Complete replacement of TextButton from button.hxx
struct MorphButton {
    Rect        bounds { 0, 0, 120, 36 };
    std::string label  = "Button";
    bool        hovered = false;
    bool        pressed = false;
    bool        primary = false;  // filled accent background

    ScalarAnim anim_hover;
    ScalarAnim anim_press;

    bool is_hit(double mx, double my) const noexcept { return bounds.contains(mx, my); }

    void tick() noexcept {
        anim_hover.tick_to(hovered ? 1.0 : 0.0, 14.0);
        anim_press.tick_to(pressed ? 1.0 : 0.0, 22.0);
    }

    void draw(cairo_t *cr, PangoLayout *layout, PangoFontDescription *fnt,
              const ThemeVariant &tv) noexcept;
};

// SECTION 11 - MorphTabButton (Settings sidebar tab)

/// Settings sidebar tab
/// Complete replacement of TabButton from button.hxx
struct MorphTabButton {
    Rect        bounds { 0, 0, 0, 48 };
    std::string label;
    bool        hovered = false;
    bool        active  = false;

    ScalarAnim anim_active;
    ScalarAnim anim_hover;

    bool is_hit(double mx, double my) const noexcept { return bounds.contains(mx, my); }

    void tick(bool target_active, bool target_hover) noexcept {
        anim_active.tick_to(target_active ? 1.0 : 0.0, 14.0);
        anim_hover .tick_to(target_hover  ? 1.0 : 0.0, 18.0);
        active  = target_active;
        hovered = target_hover;
    }

    void draw(cairo_t *cr, PangoLayout *layout, PangoFontDescription *fnt,
              const ThemeVariant &tv, double icon_x) noexcept;
};

// SECTION 12 - MorphBarIcon (Activity Bar icon)

/// Activity bar icon (editor sidebar)
/// Complete replacement of BarIcon from bar_button.hxx
struct MorphBarIcon {
    int x = 0, y = 0, size = 32;
    bool hovered = false;
    bool pressed = false;
    bool active  = false;

    double override_r = -1., override_g = -1., override_b = -1.;

    ScalarAnim anim_hover;
    ScalarAnim anim_active;

    bool is_hit(int mx, int my) const noexcept {
        return mx >= x && mx <= x+size && my >= y && my <= y+size;
    }

    void tick(bool target_hover) noexcept {
        anim_hover .tick_to(target_hover ? 1.0 : 0.0, 14.0);
        anim_active.tick_to(active       ? 1.0 : 0.0, 12.0);
        hovered = target_hover;
    }

    void draw(cairo_t *cr, const ThemeVariant &tv, const std::string &type) noexcept;

    /// Minimalist variant: circular hover/press halo instead of the
    /// rounded-square container, used for the settings/exit buttons so
    /// they read as a distinct, quieter pair from the activity pills.
    void draw_minimal(cairo_t *cr, const ThemeVariant &tv, const std::string &type,
                       bool active = false) noexcept;
};

//SECTION 13 - MorphScrollBar

/// Scrollbar (thin rest, expands on hover)
/// Complete replacement of ScrollBar from scroll_bar.hxx
struct MorphScrollBar {
    // Static hover animations (common for vertical and horizontal)
    static double s_hover_v;
    static double s_hover_h;
    static double s_last_tick;
    // Press animation thumb (0=normal, 1=pressed)
    static double s_press_v;
    static double s_press_h;

    static void tick_hover(bool hover_v, bool hover_h) noexcept;
    static void tick_press(bool press_v, bool press_h) noexcept;

    /// Draws a vertical scroll bar
    bool draw_vertical(cairo_t *cr, int win_w, int win_h,
                       double scroll_y, double content_h,
                       const ThemeVariant &theme,
                       double right_margin = 0.0, double top_y = 0.0,
                       const std::vector<double> &markers = {},
                       int current_marker = -1) noexcept;

    /// Draws a horizontal scrollbar
    bool draw_horizontal(cairo_t *cr, int offset_x, int win_w, int win_h,
                         double scroll_x, double content_w,
                         const ThemeVariant &theme,
                         double bottom_margin = 0.0,
                         double right_reserved = 0.0) noexcept;

    /// Vertical track geometry for hit-test
    void vertical_track_rect(int win_w, int win_h, double right_margin,
                              double &x, double &y, double &w, double &h,
                              double top_y = 0.0) const noexcept;

    /// Horizontal track geometry for hit-test
    void horizontal_track_rect(int offset_x, int win_w, int win_h,
                                double right_reserved, double bottom_margin,
                                double &x, double &y, double &w, double &h) const noexcept;
};


// SECTION 14 - MorphSlider

/// Horizontal slider in Slider style :)))
struct MorphSlider {
    Rect   bounds { 0, 0, 200, 32 };
    double value  = 0.5;      ///< Current normalized value [0..1]
    double min_v  = 0.0;
    double max_v  = 1.0;
    double step   = 0.0;      ///< 0 = continuous

    bool hovered  = false;
    bool dragging = false;
    bool disabled = false;

    ScalarAnim anim_hover;
    ScalarAnim anim_fill;

    // Label (optional)
    std::string label;
    // Formatting the value (optional, e.g. "%d px")
    std::string format;

    bool is_hit(double mx, double my) const noexcept { return bounds.contains(mx, my); }

    /// Convert mouse x-coordinate to value
    double pos_to_value(double mx) const noexcept {
        double track_x = bounds.x + 10.0;
        double track_w = bounds.w - 20.0;
        double norm = morph::clamp((mx - track_x) / track_w, 0.0, 1.0);
        double raw  = min_v + norm * (max_v - min_v);
        if (step > 0) raw = std::round(raw / step) * step;
        return morph::clamp(raw, min_v, max_v);
    }

    void tick() noexcept {
        anim_hover.tick_to(hovered || dragging ? 1.0 : 0.0, 14.0);
        double norm = (max_v > min_v) ? (value - min_v) / (max_v - min_v) : 0.0;
        anim_fill.tick_to(norm, 18.0);
    }

    void draw(cairo_t *cr, PangoLayout *layout, PangoFontDescription *fnt,
              const ThemeVariant &tv) noexcept;
};


// SECTION 15 - MorphProgressBar
/// Progress bar (determinate + indeterminate)
struct MorphProgressBar {
    Rect   bounds  { 0, 0, 200, 6 };
    double value   = 0.0;      ///< [0..1], or < 0 for indeterminate
    bool   animate = true;

    ScalarAnim anim_value;
    double indeterminate_phase = 0.0;  ///< Ticker phase

    void tick(double dt = 0.016) noexcept {
        if (value >= 0) {
            anim_value.tick_to(value, 8.0);
        } else {
            indeterminate_phase += dt * 0.9;
            if (indeterminate_phase > 1.0) indeterminate_phase -= 1.0;
        }
    }

    void draw(cairo_t *cr, const ThemeVariant &tv) noexcept;
};

// SECTION 16 - MorphTooltip

/// Tooltip
struct MorphTooltip {
    std::string text;
    double anchor_x = 0, anchor_y = 0; ///< Anchor point (usually a cursor)
    bool visible = false;
    double delay = 0.5; ///< Delay before display (seconds)

    double hover_since = 0.0; ///< When they started pointing
    ScalarAnim  anim_alpha;

    void on_hover_start(double x, double y) noexcept {
        if (!visible) hover_since = morph::now();
        anchor_x = x; anchor_y = y;
    }

    void on_hover_end() noexcept {
        visible = false;
        anim_alpha.value = 0;
    }

    void tick() noexcept {
        if (!visible && (morph::now() - hover_since) > delay) visible = true;
        anim_alpha.tick_to(visible ? 1.0 : 0.0, 18.0);
    }

    void draw(cairo_t *cr, PangoLayout *layout, PangoFontDescription *fnt,
              const ThemeVariant &tv, int win_w, int win_h) noexcept;
};


// SECTION 17 - MorphBadge

/// Label / badge (e.g. number of errors above the icon)
struct MorphBadge {
    int    count  = 0;
    double cx = 0, cy = 0;   ///< Center badge
    double radius = 9.0;

    ScalarAnim anim_scale;

    void set_count(int c) noexcept {
        if (count != c) { count = c; anim_scale.value = 0.5; }
    }

    void tick() noexcept {
        anim_scale.tick_to(count > 0 ? 1.0 : 0.0, 18.0);
    }

    void draw(cairo_t *cr, PangoLayout *layout, PangoFontDescription *fnt,
              const ThemeVariant &tv) noexcept;
};

// SECTION 18 - MorphContextMenu

/// Context menu item
struct MorphMenuItem {
    std::string label;
    std::string shortcut;   ///< Hotkey text (optional)
    bool        separator = false;
    bool        disabled  = false;
    bool        checked   = false;
    ScalarAnim  anim_hover;
};
/// Context menu
struct MorphContextMenu {
    std::vector<MorphMenuItem> items;
    double x = 0, y = 0;
    bool   visible = false;
    int    hovered_item = -1;

    static constexpr double ITEM_H  = 32.0;
    static constexpr double PAD     = 6.0;
    static constexpr double WIDTH   = 180.0;
    static constexpr double SEP_H   = 9.0;
    static constexpr double RADIUS  = 8.0;

    AnimController open_anim;

    void show(double px, double py) noexcept {
        x = px; y = py; visible = true;
        open_anim.start(true, 0.18);
    }

    void hide() noexcept {
        visible = false;
        open_anim.start(false, 0.12);
    }

    double total_height() const noexcept {
        double h = PAD * 2;
        for (auto &it : items) h += it.separator ? SEP_H : ITEM_H;
        return h;
    }

    bool contains(double mx, double my) const noexcept {
        return mx >= x && mx <= x + WIDTH && my >= y && my <= y + total_height();
    }

    int item_at(double mx, double my) const noexcept {
        if (!contains(mx, my)) return -1;
        double iy = y + PAD;
        for (int i = 0; i < (int)items.size(); ++i) {
            double ih = items[i].separator ? SEP_H : ITEM_H;
            if (my < iy + ih && !items[i].separator && !items[i].disabled) return i;
            iy += ih;
        }
        return -1;
    }

    void tick() noexcept {
        for (auto &it : items)
            it.anim_hover.tick_to(0.0, 14.0);  // reset - caller will update the required one
    }

    void draw(cairo_t *cr, PangoLayout *layout, PangoFontDescription *fnt,
              const ThemeVariant &tv) noexcept;
};


// SECTION 19 - MorphGLBatch (OpenGL accelerated rect batch)

// Draws a batch of rectangles with one GL draw call.
// Used for:
// * Selecting text (selection highlight - acceleration vs Cairo)
// * Background overlays (dimming when opening the modal)
// * Light particles / ripple effects
//
// Requires OpenGL 2.1+ (VBO + base shaders).
// If MORPH_GL == 0, all methods are no-op.

#if MORPH_GL

struct GlRect {
    float x, y, w, h;
    float r, g, b, a;
    float radius;   ///< Rounding in pixels (processed in the shader)
};

struct MorphGLBatch {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint prog = 0;

    std::vector<GlRect> rects;
    int win_w = 0, win_h = 0;

    bool ready = false;

    /// Initialize GL resources (call once after context creation)
    void init() noexcept;

    /// Release GL resources
    void destroy() noexcept;

    /// Start a new batch for frame size [win_w x win_h]
    void begin(int w, int h) noexcept {
        win_w = w; win_h = h;
        rects.clear();
    }

    /// Add a rectangle to the batch
    void add_rect(float x, float y, float w, float h,
                  float r, float g, float b, float a,
                  float radius = 0.f) noexcept {
        rects.push_back({ x, y, w, h, r, g, b, a, radius });
    }

    void add_rect(const Rect &rc,
                  float r, float g, float b, float a,
                  float radius = 0.f) noexcept {
        add_rect((float)rc.x,(float)rc.y,(float)rc.w,(float)rc.h, r,g,b,a, radius);
    }

    /// Fill all rectangles with one draw call and clear the batch
    void flush() noexcept;

    // Public static methods - shader compilation
    static GLuint compile_shader(GLenum type, const char *src) noexcept;
    static GLuint link_program(GLuint vs, GLuint fs) noexcept;
};

#else  // !MORPH_GL - stubs

struct GlRect { float x,y,w,h,r,g,b,a,radius; };

struct MorphGLBatch {
    std::vector<GlRect> rects;
    int win_w=0, win_h=0;
    void init()    noexcept {}
    void destroy() noexcept {}
    void begin(int w, int h) noexcept { win_w=w; win_h=h; rects.clear(); }
    void add_rect(float,float,float,float, float,float,float,float, float=0.f) noexcept {}
    void add_rect(const Rect&, float,float,float,float, float=0.f) noexcept {}
    void flush()   noexcept {}
};

#endif  // MORPH_GL

/// Slider body, the same everywhere (Welcome screen, Settings): a thin track,
/// the accent-coloured filled part and a square thumb with minimal corners.
/// x, w = track start and width; cy = vertical centre; frac = value 0..1.
inline void draw_slider_bar(cairo_t *cr, double x, double cy, double w, double frac,
                            const ThemeVariant &tv) noexcept {
    const double th = 4.0;                       // track thickness
    frac = std::clamp(frac, 0.0, 1.0);

    cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, 0.25);
    shape::rrect(cr, x, cy - th / 2, w, th, shape::R);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, tv.accent_r, tv.accent_g, tv.accent_b);
    shape::rrect(cr, x, cy - th / 2, w * frac, th, shape::R);
    cairo_fill(cr);

    double tx = x + w * frac;                    // thumb: accent square with a light edge
    cairo_set_source_rgb(cr, tv.accent_r, tv.accent_g, tv.accent_b);
    shape::disc(cr, tx, cy, 7.0);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 1, 1, 1, 0.35);
    cairo_set_line_width(cr, 1.0);
    shape::disc(cr, tx, cy, 6.5);
    cairo_stroke(cr);
}




// sECTION 20 - Icon Drawing Helpers
//
// All icons are collected here so as not to duplicate Cairo code in each widget.

/// Icon type
enum class Icon {
    IcNone,     // (named so because X11/X.h #defines IcNone)
    // Activity bar and top bar
    Sidebar, Settings, Exit, Console, Addons, Split, Viewers,
    // Settings sections
    General, Themes, Performance, Keybindings, Help, About,
    // Find bar
    Search
};

/// Draws an icon in the center (cx, cy), size=diameter of the circumscribing circle
void draw_icon(cairo_t *cr, Icon icon, double cx, double cy, double size,
               double r, double g, double b, double alpha = 1.0) noexcept;

/// Convenient overload with color from the theme
inline void draw_icon(cairo_t *cr, Icon icon, double cx, double cy, double size,
                      const ThemeVariant &tv) noexcept {
    draw_icon(cr, icon, cx, cy, size, tv.icon_r, tv.icon_g, tv.icon_b);
}

/// Convert a string (as in BarIcon) to type Icon
Icon icon_from_string(const std::string &s) noexcept;


}
// namespace morph


// Backward compatibility - aliases of old names
//
// old names continue to work,
// simply refer to new types.

using AnimController = morph::AnimController;
using ThemeTween     = morph::ThemeTween;
using SpringValue    = morph::Spring;
using SlideIndicator = morph::SlideIndicator;

/// ToggleButton -> MorphToggle (with shim for old API)
struct ToggleButton : morph::MorphToggle {
    // Compatibility: old code puts .x, .y, .w, .h directly
    double &x = bounds.x, &y = bounds.y, &w = bounds.w, &h = bounds.h;
    // Redefine draw for the old signature
    void draw(cairo_t *cr, const ThemeVariant &tv) noexcept {
        MorphToggle::draw(cr, tv);
    }
};

/// TextButton -> MorphButton
struct TextButton : morph::MorphButton {
    double &x = bounds.x, &y = bounds.y, &w = bounds.w, &h = bounds.h;
    void set_geometry(double gx, double gy, double gw, double gh) noexcept {
        bounds = {gx, gy, gw, gh};
    }
    bool is_hit(int mx, int my) const noexcept {
        return bounds.contains((double)mx, (double)my);
    }
    void tick(bool th, bool tp) noexcept { hovered=th; pressed=tp; MorphButton::tick(); }
    void draw(cairo_t *cr, PangoLayout *l, PangoFontDescription *f,
              const ThemeVariant &tv) noexcept { MorphButton::draw(cr,l,f,tv); }
};

/// TabButton -> MorphTabButton
struct TabButton : morph::MorphTabButton {
    double &x = bounds.x, &y = bounds.y, &w = bounds.w, &h = bounds.h;
    void set_geometry(double gx, double gy, double gw, double gh) noexcept {
        bounds = {gx, gy, gw, gh};
    }
    bool is_hit(int mx, int my) const noexcept {
        return bounds.contains((double)mx, (double)my);
    }
};

/// BarIcon -> MorphBarIcon
using BarIcon = morph::MorphBarIcon;

/// ScrollBar -> MorphScrollBar
// ScrollBar - backward-compat struct (forward-declarable, unlike 'using')
struct ScrollBar : morph::MorphScrollBar {
    using morph::MorphScrollBar::MorphScrollBar;
};

// draw_rounded_rect_ed / draw_rounded_rect_btn / draw_rounded_rect_bar
// - now thin wrappers around morph::rrect
static inline void draw_rounded_rect_ed(cairo_t *cr,
    double x, double y, double w, double h, double r) noexcept {
    morph::rrect(cr, x, y, w, h, r);
}
static inline void draw_rounded_rect_btn(cairo_t *cr,
    double x, double y, double w, double h, double r) noexcept {
    morph::rrect(cr, x, y, w, h, r);
}
static inline void draw_rounded_rect_bar(cairo_t *cr,
    double x, double y, double w, double h, double r) noexcept {
    morph::rrect(cr, x, y, w, h, r);
}
