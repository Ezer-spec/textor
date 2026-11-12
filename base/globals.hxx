#ifndef GLOBALS_HXX
#define GLOBALS_HXX

#include <string>
#include <algorithm>
#include <cmath>
#include <cairo/cairo.h>

// ............ Forward declarations (avoid including X11 header files here if you intend to extend the code) ............
struct _XDisplay;
typedef struct _XDisplay Display;
typedef unsigned long Window;

// ............ Performance flags (defined in base.cxx) ............
extern bool g_animations_enabled;  // false -> AnimController snaps instantly
extern bool g_rounded_corners;     // false -> all rrect helpers use cairo_rectangle
extern bool g_text_optimized;      // true  -> fast Pango hint mode, no sub-pixel AA
extern bool g_line_scroll;         // true  -> scroll by whole lines (console style)
extern bool g_smart_render;        // true  -> only render visible lines (virtual viewport)

// ............ X11 clipboard globals (set by base.cxx after window creation) ............
// These allow code_editor.hxx to do native X11 selection without xclip/xsel
extern Display     *g_x11_dpy;      // shared X11 display connection
extern Window       g_x11_win;      // main application window
extern std::string  g_x11_clipboard; // text currently owned by our window

// ............ A consistent, rigorous style: a single radius, with corners shaped like a superellipse (or nearly so) ............
// All rounded shapes are connected exclusively via shape::rrect / shape::disc
extern bool g_rounded_corners;

namespace shape {

constexpr double R = 10.0;   // the only corner radius used in the interface (px)
constexpr double K = 0.2;   // 0.45 = a standard arc; a smaller value results in a shape closer to a superellipse

// Path of a rectangle with superelliptical corners. The radius r is merely an upper
// limit; the actual value is never greater than R. r < 1 means "no rounding"
inline void path(cairo_t *cr, double x, double y, double w, double h, double r) {
    if (w <= 0 || h <= 0) return;
    if (r < 1.0) { cairo_rectangle(cr, x, y, w, h); return; }
    r = std::min({r, R, w / 2.0, h / 2.0});
    double c = r * K, x2 = x + w, y2 = y + h;
    cairo_new_sub_path(cr);
    cairo_move_to(cr, x + r, y);
    cairo_line_to(cr, x2 - r, y);
    cairo_curve_to(cr, x2 - c, y, x2, y + c, x2, y + r);
    cairo_line_to(cr, x2, y2 - r);
    cairo_curve_to(cr, x2, y2 - c, x2 - c, y2, x2 - r, y2);
    cairo_line_to(cr, x + r, y2);
    cairo_curve_to(cr, x + c, y2, x, y2 - c, x, y2 - r);
    cairo_line_to(cr, x, y + r);
    cairo_curve_to(cr, x, y + c, x + c, y, x + r, y);
    cairo_close_path(cr);
}

// Same, but taking into account the "Rounded Corners" setting (disabled -> sharp corners)
inline void rrect(cairo_t *cr, double x, double y, double w, double h, double r) {
    path(cr, x, y, w, h, g_rounded_corners ? r : 0.0);
}

// Replacing the circle: a square with a side length of 2*r and minimal rounding—why? Because it’s cool.
inline void disc(cairo_t *cr, double cx, double cy, double r) {
    rrect(cr, cx - r, cy - r, r * 2.0, r * 2.0, R);
}

}

#endif
