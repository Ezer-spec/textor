#ifndef SPLIT_EDITOR_HXX
#define SPLIT_EDITOR_HXX

#include <cmath>
#include <algorithm>
#include <chrono>
#include <cairo/cairo.h>
#include "../color/themes.hxx"
#include "../base/globals.hxx"
#include "morphing.hxx"

// SplitEditor - control of splitting the editor into two vertical panels
//
// Stores:
// enabled - whether split mode is active
// split_ratio - 0.0..1.0, splitter position (default 0.5 = centered)
// right_tab_idx - index of the tab opened in the right panel
// active_pane - 0=left, 1=right
// dragging - the user drags the separator
//
// Interacts with Editor:
// - Editor stores one instance of SplitEditor
// - draw_ui() draws both editors if enabled
// - handle_mouse_click() switches active_pane on click
//
// Separator:
// - 5px hit area, 1px visual line
// - When hover - expands to 3px, accent color
// - dragging shows handle-pill

static inline double now_split() {
    using namespace std::chrono;
    return duration_cast<duration<double>>(steady_clock::now().time_since_epoch()).count();
}

struct SplitEditor {
    bool   enabled       = false;
    double split_ratio   = 0.5;   // 0.0 (left) .. 1.0 (right)
    int    right_tab_idx = -1;    // -1 = same file as left
    int    active_pane   = 0;     // 0=left, 1=right

    bool   dragging      = false;
    bool   hovered       = false;

    double hover_anim    = 0.0;   // 0..1 - divider hover animation
    double enable_anim   = 0.0;   // 0..1 - split open/close animation
    double last_tick     = now_split();

    static constexpr double DIVIDER_HIT_W    = 8.0;   // click area width
    static constexpr double DIVIDER_VIS_W_REST = 1.0;
    static constexpr double DIVIDER_VIS_W_HOV  = 3.0;
    static constexpr double MIN_RATIO        = 0.15;
    static constexpr double MAX_RATIO        = 0.85;
    static constexpr double SNAP_THRESHOLD   = 0.025; // magnetic snap distance to 50/50

    bool   snapped        = false;  // true while dragged ratio is magnet-snapped to center
    double snap_glow      = 0.0;    // 0..1 - fades in when snapped, for a subtle highlight

    void tick() {
        double t  = now_split();
        double dt = std::min(0.06, t - last_tick);
        last_tick = t;
        hover_anim  += ((hovered   ? 1.0 : 0.0) - hover_anim)  * std::clamp(16.0 * dt, 0.0, 1.0);
        enable_anim += ((enabled   ? 1.0 : 0.0) - enable_anim) * std::clamp(10.0 * dt, 0.0, 1.0);
        snap_glow   += ((snapped   ? 1.0 : 0.0) - snap_glow)   * std::clamp(20.0 * dt, 0.0, 1.0);
    }

    // Returns x position of the divider for given editor area
    double divider_x(int ed_x, int ed_w) const {
        return ed_x + ed_w * std::clamp(split_ratio, MIN_RATIO, MAX_RATIO) * enable_anim;
    }

    // Returns true if (mx,my) is within the divider hit area
    bool divider_hit(int mx, int my, int ed_x, int ed_w,
                     int tab_bar_h, int win_h, double con_h) const
    {
        if (!enabled || enable_anim < 0.1) return false;
        double dx = divider_x(ed_x, ed_w);
        return mx >= (int)(dx - DIVIDER_HIT_W/2)
            && mx <= (int)(dx + DIVIDER_HIT_W/2)
            && my >= tab_bar_h
            && my <= (int)(win_h - con_h);
    }

    // Called on mouse move during dragging. Snaps magnetically to a perfect
    // 50/50 split when the pointer is close to center, so it's easy to land
    // exactly on an even split without pixel-hunting.
    void drag_to(int mx, int ed_x, int ed_w) {
        if (!dragging || ed_w <= 0) return;
        double new_ratio = (double)(mx - ed_x) / (double)ed_w;
        new_ratio = std::clamp(new_ratio, MIN_RATIO, MAX_RATIO);
        if (std::abs(new_ratio - 0.5) < SNAP_THRESHOLD) {
            new_ratio = 0.5;
            snapped   = true;
        } else {
            snapped = false;
        }
        split_ratio = new_ratio;
    }

    // Restores an even 50/50 split - used on divider double-click.
    void reset_ratio() { split_ratio = 0.5; snapped = false; }

    // Returns which pane was clicked (0 or 1). -1 = divider area or outside.
    int pane_hit(int mx, int ed_x, int ed_w) const {
        if (!enabled) return 0;
        double dx = divider_x(ed_x, ed_w);
        if (std::abs(mx - dx) < DIVIDER_HIT_W / 2) return -1;
        return (mx < dx) ? 0 : 1;
    }

    // Left pane rect
    void left_rect(int ed_x, int ed_w, int &lx, int &lw) const {
        lx = ed_x;
        lw = enabled ? (int)(divider_x(ed_x, ed_w) - ed_x) : ed_w;
    }

    // Right pane rect
    void right_rect(int ed_x, int ed_w, int &rx, int &rw) const {
        double dx = divider_x(ed_x, ed_w);
        rx = (int)dx + 1;
        rw = ed_x + ed_w - rx;
    }

    // Draw the divider line + handle - flat, minimal (Material-style):
    // no shadows, no glow/blur gradients, just solid colors with smooth
    // eased motion. Cheap to draw every frame.
    void draw_divider(cairo_t *cr, int ed_x, int ed_w,
                      int tab_bar_h, int win_h, double con_h,
                      const ThemeVariant &tv) const
    {
        if (enable_anim < 0.01) return;
        double dx = divider_x(ed_x, ed_w);
        double dh = morph::ease_out_cubic(hover_anim); // eased for a snappier, less linear feel
        double alpha = enable_anim;
        double top = tab_bar_h, bot = win_h - con_h;

        // Line - flat color, blends from muted to accent on hover/drag.
        // A gentle flat brightening (no glow) confirms the magnetic snap.
        double vis_w = DIVIDER_VIS_W_REST + (DIVIDER_VIS_W_HOV - DIVIDER_VIS_W_REST) * dh;
        double line_alpha = (0.14 + 0.30 * dh + 0.10 * snap_glow) * alpha;

        cairo_set_source_rgba(cr,
            tv.muted * (1.0 - dh) + tv.accent_r * dh,
            tv.muted * (1.0 - dh) + tv.accent_g * dh,
            tv.muted * (1.0 - dh) + tv.accent_b * dh,
            line_alpha);
        cairo_set_line_width(cr, vis_w);
        cairo_move_to(cr, dx, top);
        cairo_line_to(cr, dx, bot);
        cairo_stroke(cr);

        // handle pill (shown on hover + drag) - flat fill, no glow
        if (dh > 0.05 || dragging) {
            double pill_h  = 48.0 + 16.0 * dh;
            double pill_w  = 4.0 + 4.0 * dh;
            double pill_y  = (top + bot) / 2.0 - pill_h / 2.0;
            double ph_alpha = (0.4 + 0.5 * dh) * alpha;

            cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, ph_alpha);
            cairo_new_sub_path(cr);
            double px = dx - pill_w / 2.0;
            double py = pill_y;
            shape::rrect(cr, px, py, pill_w, pill_h, 2.0);
            cairo_fill(cr);
        }
    }
};

#endif
