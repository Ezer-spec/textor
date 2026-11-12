// morphing.cxx - Implementation of the Morphing GUI library
//
// Connect strictly in ONE translation unit:
// #include "ui/morphing.cxx"
// or compile as a separate object file.
#include "morphing.hxx"
#include "svg_icons.hxx"

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <cmath>
#include <cstdio>
#include <cstring>

// .............................................................................
//  Static member definitions
// ............ GL 2.1 function pointers - definitions ............
#if MORPH_GL
namespace morph_gl {
    PFNGLCREATESHADERPROC            glCreateShader           = nullptr;
    PFNGLSHADERSOURCEPROC            glShaderSource           = nullptr;
    PFNGLCOMPILESHADERPROC           glCompileShader          = nullptr;
    PFNGLGETSHADERIVPROC             glGetShaderiv            = nullptr;
    PFNGLGETSHADERINFOLOGPROC        glGetShaderInfoLog       = nullptr;
    PFNGLDELETESHADERPROC            glDeleteShader           = nullptr;
    PFNGLCREATEPROGRAMPROC           glCreateProgram          = nullptr;
    PFNGLATTACHSHADERPROC            glAttachShader           = nullptr;
    PFNGLBINDATTRIBLOCATIONPROC      glBindAttribLocation     = nullptr;
    PFNGLLINKPROGRAMPROC             glLinkProgram            = nullptr;
    PFNGLGETPROGRAMIVPROC            glGetProgramiv           = nullptr;
    PFNGLGETPROGRAMINFOLOGPROC       glGetProgramInfoLog      = nullptr;
    PFNGLDELETEPROGRAMPROC           glDeleteProgram          = nullptr;
    PFNGLUSEPROGRAMPROC              glUseProgram             = nullptr;
    PFNGLGETUNIFORMLOCATIONPROC      glGetUniformLocation     = nullptr;
    PFNGLUNIFORM2FPROC               glUniform2f              = nullptr;
    PFNGLGENBUFFERSPROC              glGenBuffers             = nullptr;
    PFNGLDELETEBUFFERSPROC           glDeleteBuffers          = nullptr;
    PFNGLBINDBUFFERPROC              glBindBuffer             = nullptr;
    PFNGLBUFFERDATAPROC              glBufferData             = nullptr;
    PFNGLENABLEVERTEXATTRIBARRAYPROC  glEnableVertexAttribArray  = nullptr;
    PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray = nullptr;
    PFNGLVERTEXATTRIBPOINTERPROC     glVertexAttribPointer    = nullptr;
    morph_gl::MORPH_PFNGLDRAWARRAYSPROC glDrawArrays           = nullptr;
}

// Helper macro to load one function
#define MORPH_LOAD_GL(name)     morph_gl::name = (decltype(morph_gl::name))         glXGetProcAddressARB((const GLubyte*)#name)

namespace morph {
void gl_load_procs() noexcept {
    MORPH_LOAD_GL(glCreateShader);
    MORPH_LOAD_GL(glShaderSource);
    MORPH_LOAD_GL(glCompileShader);
    MORPH_LOAD_GL(glGetShaderiv);
    MORPH_LOAD_GL(glGetShaderInfoLog);
    MORPH_LOAD_GL(glDeleteShader);
    MORPH_LOAD_GL(glCreateProgram);
    MORPH_LOAD_GL(glAttachShader);
    MORPH_LOAD_GL(glBindAttribLocation);
    MORPH_LOAD_GL(glLinkProgram);
    MORPH_LOAD_GL(glGetProgramiv);
    MORPH_LOAD_GL(glGetProgramInfoLog);
    MORPH_LOAD_GL(glDeleteProgram);
    MORPH_LOAD_GL(glUseProgram);
    MORPH_LOAD_GL(glGetUniformLocation);
    MORPH_LOAD_GL(glUniform2f);
    MORPH_LOAD_GL(glGenBuffers);
    MORPH_LOAD_GL(glDeleteBuffers);
    MORPH_LOAD_GL(glBindBuffer);
    MORPH_LOAD_GL(glBufferData);
    MORPH_LOAD_GL(glEnableVertexAttribArray);
    MORPH_LOAD_GL(glDisableVertexAttribArray);
    MORPH_LOAD_GL(glVertexAttribPointer);
    MORPH_LOAD_GL(glDrawArrays);
}
} // namespace morph
#undef MORPH_LOAD_GL
#endif // MORPH_GL

namespace morph {

double MorphScrollBar::s_hover_v   = 0.0;
double MorphScrollBar::s_hover_h   = 0.0;
double MorphScrollBar::s_last_tick = 0.0;
double MorphScrollBar::s_press_v   = 0.0;
double MorphScrollBar::s_press_h   = 0.0;


//  SECTION 9 - MorphToggle::draw

void MorphToggle::draw(cairo_t *cr, const ThemeVariant &tv) noexcept {
    // "Segmented tab": a frame with two halves (Off | On); a block slides to the active half
    double a  = anim_value.value;   // 0 = off, 1 = on
    double hv = anim_hover.value;
    double x  = bounds.x, y = bounds.y, w = bounds.w, h = bounds.h;
    const double R   = shape::R;
    const double pad = 2.0;                       // space between the frame and the block
    const double half = (w - pad * 2) / 2.0;      // width of one half = width of the block

    // Frame
    cairo_set_source_rgba(cr, tv.elevated, tv.elevated, tv.elevated, 0.9);
    rrect(cr, x, y, w, h, R);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, 0.40 + 0.25 * hv);
    cairo_set_line_width(cr, 1.0);
    rrect(cr, x + .5, y + .5, w - 1, h - 1, R);
    cairo_stroke(cr);

    // Sliding block: gray when off, accent when on
    cairo_set_source_rgba(cr,
        tv.muted * 0.55 * (1. - a) + tv.accent_r * a,
        tv.muted * 0.55 * (1. - a) + tv.accent_g * a,
        tv.muted * 0.55 * (1. - a) + tv.accent_b * a, 0.95);
    rrect(cr, x + pad + a * half, y + pad, half, h - pad * 2, R);
    cairo_fill(cr);

    // Labels: the one under the block is white, the other one is muted
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *fd = pango_font_description_from_string(tv.font_family.c_str());
    pango_font_description_set_absolute_size(fd, std::max(8.0, h * 0.38) * PANGO_SCALE);
    pango_font_description_set_weight(fd, PANGO_WEIGHT_SEMIBOLD);
    pango_layout_set_font_description(layout, fd);

    const char *labels[2] = {"Off", "On"};
    for (int i = 0; i < 2; ++i) {
        double under = (i == 0) ? (1. - a) : a;                   // 1 = block is under this label
        double c = tv.muted * (1. - under) + 1.0 * under;         // muted -> white
        int lw, lh;
        pango_layout_set_text(layout, labels[i], -1);
        pango_layout_get_pixel_size(layout, &lw, &lh);
        cairo_set_source_rgb(cr, c, c, c);
        cairo_move_to(cr, x + pad + half * i + (half - lw) / 2.0, y + (h - lh) / 2.0);
        pango_cairo_show_layout(cr, layout);
    }
    pango_font_description_free(fd);
    g_object_unref(layout);
}


//  SECTION 10 - MorphButton::draw

void MorphButton::draw(cairo_t *cr, PangoLayout *layout, PangoFontDescription *fnt,
                       const ThemeVariant &tv) noexcept {
    double hv = anim_hover.value;
    double pv = anim_press.value;
    double x  = bounds.x, y = bounds.y, w = bounds.w, h = bounds.h;
    double rr = g_rounded_corners ? 8.0 : 2.0;

    if (primary) {
        // ............ Filled accent button ............
        cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b,
                              0.85 + 0.15 * hv);
        rrect(cr, x, y, w, h, rr);
        cairo_fill(cr);

        // Press overlay
        if (pv > 0.001) {
            cairo_set_source_rgba(cr, 0, 0, 0, 0.12 * pv);
            rrect(cr, x+1, y+1, w-2, h-2, rr-1);
            cairo_fill(cr);
        }

        // Label (white)
        cairo_set_source_rgb(cr, 1, 1, 1);
    } else {
        // ............ Tonal / ghost button ............
        // Surface fill (Mica-like)
        cairo_set_source_rgba(cr, tv.elevated, tv.elevated, tv.elevated,
                              0.07 + 0.10 * hv);
        rrect(cr, x, y, w, h, rr);
        cairo_fill(cr);

        // MD3 state layer hover
        if (hv > 0.01) {
            cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b,
                                  0.08 * hv);
            rrect(cr, x, y, w, h, rr);
            cairo_fill(cr);
        }

        // Press ripple analog
        if (pv > 0.001) {
            cairo_set_source_rgba(cr, 0, 0, 0, 0.09 * pv);
            rrect(cr, x+1, y+1, w-2, h-2, rr-1);
            cairo_fill(cr);
        }

        // Border - accent-tinted on hover
        {
            double br = hv;
            cairo_set_source_rgba(cr,
                tv.muted * (1.-br) + tv.accent_r * br,
                tv.muted * (1.-br) + tv.accent_g * br,
                tv.muted * (1.-br) + tv.accent_b * br,
                0.20 + 0.20 * hv);
            cairo_set_line_width(cr, 1.0);
            rrect(cr, x+.5, y+.5, w-1, h-1, rr);
            cairo_stroke(cr);
        }

        cairo_set_source_rgb(cr, tv.text, tv.text, tv.text);
    }

    // Label
    pango_font_description_set_weight(fnt, PANGO_WEIGHT_MEDIUM);
    pango_layout_set_font_description(layout, fnt);
    pango_layout_set_attributes(layout, nullptr);
    pango_layout_set_text(layout, label.c_str(), -1);
    int tw, th;
    pango_layout_get_pixel_size(layout, &tw, &th);
    cairo_move_to(cr, x + (w - tw) / 2.0, y + (h - th) / 2.0);
    pango_cairo_show_layout(cr, layout);
}


//  SECTION 11 - MorphTabButton::draw

void MorphTabButton::draw(cairo_t *cr, PangoLayout *layout, PangoFontDescription *fnt,
                          const ThemeVariant &tv, double icon_x) noexcept {
    double act = anim_active.value;
    double hv  = anim_hover.value;
    double x   = bounds.x, y = bounds.y, w = bounds.w, h = bounds.h;

    double pad = 10.0;
    double rx  = x + pad;
    double rw  = w - pad * 2;
    double ry  = y + (h - 44) / 2.0;
    double rh  = 44.0;
    double rr  = g_rounded_corners ? 10.0 : 3.0;

    // ............ Active container (tonal container) ............
    if (act > 0.001) {
        cairo_set_source_rgba(cr,
            tv.accent_r * 0.18 + tv.elevated * 0.82,
            tv.accent_g * 0.18 + tv.elevated * 0.82,
            tv.accent_b * 0.18 + tv.elevated * 0.82,
            act);
        rrect(cr, rx, ry, rw, rh, rr);
        cairo_fill(cr);

        // Left accent stripe
        cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.95 * act);
        double bar_r = g_rounded_corners ? 1.5 : 0;
        rrect(cr, rx, ry + 6, 3, rh - 12, bar_r);
        cairo_fill(cr);
    }

    // ............ Hover overlay ............
    if (hv > 0.001 && act < 0.85) {
        cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.08 * hv);
        rrect(cr, rx, ry, rw, rh, rr);
        cairo_fill(cr);
    }

    // ............ icon ............
    Icon icon = Icon::IcNone;
    if      (label == "General")     icon = Icon::General;
    else if (label == "Themes")      icon = Icon::Themes;
    else if (label == "Performance") icon = Icon::Performance;
    else if (label == "Keybindings") icon = Icon::Keybindings;
    else if (label == "Help")        icon = Icon::Help;
    else if (label == "About")       icon = Icon::About;

    double ir = active ? tv.accent_r : tv.icon_r;
    double ig = active ? tv.accent_g : tv.icon_g;
    double ib = active ? tv.accent_b : tv.icon_b;

    if (icon != Icon::IcNone)
        draw_icon(cr, icon, icon_x, y + h / 2.0, 20.0, ir, ig, ib);

    // ............ Label ............
    double label_alpha = active ? 1.0 : 0.75 + 0.25 * hv;
    cairo_set_source_rgba(cr,
        active ? tv.accent_r : tv.text,
        active ? tv.accent_g : tv.text,
        active ? tv.accent_b : tv.text,
        label_alpha);
    pango_font_description_set_weight(fnt,
        active ? PANGO_WEIGHT_SEMIBOLD : PANGO_WEIGHT_NORMAL);
    pango_layout_set_font_description(layout, fnt);
    pango_layout_set_attributes(layout, nullptr);
    pango_layout_set_text(layout, label.c_str(), -1);
    cairo_move_to(cr, x + 76, y + (h - 16) / 2.0);
    pango_cairo_show_layout(cr, layout);
}


//  SECTION 12 - MorphBarIcon  [draw method]

static constexpr double BAR_HIGHLIGHT_PAD        = 4.0;
static constexpr double BAR_HIGHLIGHT_SCALE_RATE  = 0.0;
static constexpr double BAR_HIGHLIGHT_RADIUS_BASE = 10.0;

void MorphBarIcon::draw(cairo_t *cr, const ThemeVariant &tv, const std::string &type) noexcept {
    double h   = anim_hover.value;
    double act = anim_active.value;
    double combined = std::max(h, act);

    double cx = x + size / 2.0;
    double cy = y + size / 2.0;

    // ............ Background container ............
    if (combined > 0.005) {
        double pad = BAR_HIGHLIGHT_PAD;
        double bx  = x - pad;
        double by  = y - pad;
        double bw  = size + 2.0 * pad;
        double bh  = size + 2.0 * pad;
        double br  = g_rounded_corners
                   ? BAR_HIGHLIGHT_RADIUS_BASE * (0.6 + 0.4 * combined)
                   : 2.0;

        double fill_alpha, fr, fg, fb;
        if (act > h) {
            fill_alpha = 0.18 + 0.04 * act;
            fr = tv.accent_r * 0.22 + tv.elevated * 0.78;
            fg = tv.accent_g * 0.22 + tv.elevated * 0.78;
            fb = tv.accent_b * 0.22 + tv.elevated * 0.78;
        } else {
            fill_alpha = 0.09 * h;
            fr = tv.accent_r; fg = tv.accent_g; fb = tv.accent_b;
        }
        cairo_set_source_rgba(cr, fr, fg, fb, fill_alpha);
        rrect(cr, bx, by, bw, bh, br);
        cairo_fill(cr);

        if (act > 0.1) {
            // Container border
            cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.22 * act);
            cairo_set_line_width(cr, 1.0);
            rrect(cr, bx+.5, by+.5, bw-1, bh-1, br);
            cairo_stroke(cr);

            // Left accent stripe
            if (g_rounded_corners) {
                cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.95 * act);
                double bar_h = bh * 0.5;
                double bar_y = by + (bh - bar_h) / 2.0;
                rrect(cr, (double)x - 2.0, bar_y, 3.0, bar_h, 1.5);
                cairo_fill(cr);
            }
        }
    }

    // ............ Scaling on hover ............
    cairo_save(cr);
    double scale = 1.0 + BAR_HIGHLIGHT_SCALE_RATE * h;
    cairo_translate(cr, cx, cy);
    cairo_scale(cr, scale, scale);
    cairo_translate(cr, -cx, -cy);

    // ............ icons color ............
    double ir = tv.icon_r, ig = tv.icon_g, ib = tv.icon_b;
    if (override_r >= 0.0) { ir = override_r; ig = override_g; ib = override_b; }

    if (type == "exit") {
        ir = ir * (1.0 - h) + 0.92 * h;
        ig = ig * (1.0 - h) + 0.18 * h;
        ib = ib * (1.0 - h) + 0.18 * h;
    } else if (act > 0.1) {
        double blend = std::min(1.0, act * 1.5);
        ir = ir*(1.-blend) + tv.accent_r*blend;
        ig = ig*(1.-blend) + tv.accent_g*blend;
        ib = ib*(1.-blend) + tv.accent_b*blend;
    } else if (h > 0.01) {
        double hh = 0.25 * h;
        ir = ir*(1.-hh) + tv.accent_r*hh;
        ig = ig*(1.-hh) + tv.accent_g*hh;
        ib = ib*(1.-hh) + tv.accent_b*hh;
    }

    // ............ icon ............
    Icon icon = icon_from_string(type);
    draw_icon(cr, icon, cx, cy, (double)size, ir, ig, ib);

    cairo_restore(cr);
}


//  SECTION 12b - MorphBarIcon  [minimalist circular variant]
//  Used only by the settings/exit buttons. Quieter than the pill/activity
//  style: a soft circular halo on hover, a firmer circular fill on press,
//  no border, no accent stripe - reads as "chrome", not "content". (+_-)

static constexpr double MIN_BTN_SCALE_RATE = 0.0;

void MorphBarIcon::draw_minimal(cairo_t *cr, const ThemeVariant &tv, const std::string &type,
                                 bool active) noexcept {
    // `active` keeps the icon visibly "on" (a permanent, dimmer version of
    // the hover halo/tint) even with the mouse elsewhere - used for the
    // Settings icon, so it stays clearly marked as "open, click to close"
    // for as long as the Settings overlay is up.
    double h = std::max(anim_hover.value, active ? 0.55 : 0.0);

    double cx = x + size / 2.0;
    double cy = y + size / 2.0;
    double radius = size * 0.62;

    // ............ Circular hover halo ............
    if (h > 0.005) {
        bool is_exit = (type == "exit");
        double fr, fg, fb;
        if (is_exit) { fr = 0.85; fg = 0.24; fb = 0.24; }
        else         { fr = tv.muted; fg = tv.muted; fb = tv.muted; }
        cairo_set_source_rgba(cr, fr, fg, fb, (is_exit ? 0.16 : 0.11) * h);
        shape::disc(cr, cx, cy, radius);
        cairo_fill(cr);
    }

    // ............ Subtle scale on hover ............
    cairo_save(cr);
    double scale = 1.0 + MIN_BTN_SCALE_RATE * h;
    cairo_translate(cr, cx, cy);
    cairo_scale(cr, scale, scale);
    cairo_translate(cr, -cx, -cy);

    double ir = tv.icon_r, ig = tv.icon_g, ib = tv.icon_b;
    if (type == "exit") {
        ir = ir * (1.0 - h) + 0.90 * h;
        ig = ig * (1.0 - h) + 0.22 * h;
        ib = ib * (1.0 - h) + 0.22 * h;
    } else if (h > 0.01) {
        double hh = 0.35 * h;
        ir = ir*(1.-hh) + tv.accent_r*hh;
        ig = ig*(1.-hh) + tv.accent_g*hh;
        ib = ib*(1.-hh) + tv.accent_b*hh;
    }

    Icon icon = icon_from_string(type);
    draw_icon(cr, icon, cx, cy, (double)size * 0.86, ir, ig, ib);

    cairo_restore(cr);
}


//  SECTION 13 - MorphScrollBar

void MorphScrollBar::tick_hover(bool hover_v, bool hover_h) noexcept {
    double t  = morph::now();
    double dt = std::min(0.05, t - s_last_tick);
    s_last_tick = t;
    double speed = 10.0;
    s_hover_v += ((hover_v ? 1.0 : 0.0) - s_hover_v) * clamp(speed*dt, 0.0, 1.0);
    s_hover_h += ((hover_h ? 1.0 : 0.0) - s_hover_h) * clamp(speed*dt, 0.0, 1.0);
}

void MorphScrollBar::tick_press(bool press_v, bool press_h) noexcept {
    // Fast response: speed 24 (press) and 10 (release)
    double spd_v = press_v ? 24.0 : 10.0;
    double spd_h = press_h ? 24.0 : 10.0;
    double t  = morph::now();
    double dt = std::min(0.05, t - s_last_tick);
    s_press_v += ((press_v ? 1.0 : 0.0) - s_press_v) * clamp(spd_v*dt, 0.0, 1.0);
    s_press_h += ((press_h ? 1.0 : 0.0) - s_press_h) * clamp(spd_h*dt, 0.0, 1.0);
}

bool MorphScrollBar::draw_vertical(cairo_t *cr, int win_w, int win_h,
                                    double scroll_y, double content_h,
                                    const ThemeVariant &theme,
                                    double right_margin, double top_y,
                                    const std::vector<double> &markers,
                                    int current_marker) noexcept {
    double view_h = (double)win_h - top_y;
    if (view_h <= 0 || content_h <= view_h) return false;

    double h = s_hover_v;

    // Track: thin at rest, wider on hover
    double track_w_rest = 6.0, track_w_full = 6.0;
    double track_w = track_w_rest + (track_w_full - track_w_rest) * h;
    double padding  = 4.0;
    double track_x  = (double)win_w - track_w - padding - right_margin;
    double track_y  = top_y + 4.0;
    double track_h  = view_h - 8.0;

    // Track background (appears when hovered)
    if (h > 0.01) {
        cairo_set_source_rgba(cr, theme.muted, theme.muted, theme.muted, 0.12 * h);
        rrect(cr, track_x, track_y, track_w, track_h, track_w / 2.0);
        cairo_fill(cr);
    }

    // Slider
    double ratio     = clamp(view_h / content_h, 0.04, 1.0);
    double thumb_h   = std::max(24.0, track_h * ratio);
    double max_scr   = std::max(0.0, content_h - view_h);
    double thumb_raw = max_scr > 0.0
                     ? (scroll_y / max_scr) * (track_h - thumb_h) : 0.0;
    double thumb_y   = track_y + clamp(thumb_raw, 0.0, track_h - thumb_h);

    double pv         = s_press_v;
    double base_tw    = track_w;
    double press_tw   = base_tw;
    double draw_tw    = base_tw - (base_tw - press_tw) * pv;
    double ox         = (base_tw - draw_tw) / 2.0;
    double shrink_h   = 0.0;
    double draw_ty    = thumb_y + shrink_h * 0.5;
    double draw_th    = thumb_h - shrink_h;
    double thumb_corner = g_rounded_corners ? draw_tw / 2.0 : 2.0;


    // Thumb - respects g_rounded_corners
    double alpha = 0.60 + 0.35 * h - 0.15 * pv;
    cairo_set_source_rgba(cr, theme.accent_r, theme.accent_g, theme.accent_b, alpha);
    rrect(cr, track_x + ox, draw_ty, draw_tw, draw_th, thumb_corner);
    cairo_fill(cr);

    // Markers - drawn ON OVER the thumb, always visible (without the condition h>0.05)
    if (!markers.empty()) {
        const double MIN_GAP = 3.0;
        double prev_py = -999.0;
        int mi = 0;
        for (double norm_pos : markers) {
            double py = track_y + norm_pos * track_h;
            if (py - prev_py < MIN_GAP) { ++mi; continue; }
            prev_py = py;
            bool is_cur = (mi == current_marker);
            double mr    = is_cur ? 3.5 : 2.5;
            double alpha2 = is_cur ? 1.0 : 0.70;
            // Outline over thumb for readability
            cairo_set_source_rgba(cr, theme.bg, theme.bg, theme.bg, 0.6);
            shape::disc(cr, track_x + track_w / 2.0, py, mr + 1.2);
            cairo_fill(cr);
            cairo_set_source_rgba(cr, theme.accent_r, theme.accent_g, theme.accent_b, alpha2);
            shape::disc(cr, track_x + track_w / 2.0, py, mr);
            cairo_fill(cr);
            ++mi;
        }
    }

    return true;
}

bool MorphScrollBar::draw_horizontal(cairo_t *cr, int offset_x, int win_w, int win_h,
                                      double scroll_x, double content_w,
                                      const ThemeVariant &theme,
                                      double bottom_margin,
                                      double right_reserved) noexcept {
    double view_w = (double)(win_w - offset_x);
    if (content_w <= view_w) return false;

    double h = s_hover_h;

    double track_h_rest = 6.0, track_h_full = 6.0;
    double track_h_now  = track_h_rest + (track_h_full - track_h_rest) * h;
    double padding      = 4.0;
    double track_y      = (double)win_h - track_h_now - padding - bottom_margin;
    double track_x      = (double)offset_x + 8.0;
    double usable_w     = view_w - right_reserved - 16.0;
    if (usable_w <= 0.0) return false;

    if (h > 0.01) {
        cairo_set_source_rgba(cr, theme.muted, theme.muted, theme.muted, 0.12 * h);
        rrect(cr, track_x, track_y, usable_w, track_h_now, track_h_now / 2.0);
        cairo_fill(cr);
    }

    double ratio     = clamp(usable_w / content_w, 0.04, 1.0);
    double thumb_w   = std::max(24.0, usable_w * ratio);
    double max_scr   = std::max(0.0, content_w - usable_w);
    double thumb_raw = max_scr > 0.0
                     ? (scroll_x / max_scr) * (usable_w - thumb_w) : 0.0;
    double thumb_x   = track_x + clamp(thumb_raw, 0.0, usable_w - thumb_w);

    // Press: thumb shrinks in height
    double ph        = s_press_h;
    double base_th   = track_h_now;
    double press_th  = base_th;
    double draw_th   = base_th - (base_th - press_th) * ph;
    double oy        = (base_th - draw_th) / 2.0;
    double shrink_w  = 0.0;
    double draw_tx   = thumb_x + shrink_w * 0.5;
    double draw_tw2  = thumb_w - shrink_w;


    double alpha_h = 0.60 + 0.35 * h - 0.15 * ph;
    cairo_set_source_rgba(cr, theme.accent_r, theme.accent_g, theme.accent_b, alpha_h);
    rrect(cr, draw_tx, track_y + oy, draw_tw2, draw_th, shape::R);
    cairo_fill(cr);

    return true;
}

void MorphScrollBar::vertical_track_rect(int win_w, int win_h, double right_margin,
                                          double &ox, double &oy, double &ow, double &oh,
                                          double top_y) const noexcept {
    double track_w = 14.0, padding = 2.0;
    ox = (double)win_w - track_w - padding - right_margin;
    oy = top_y;
    ow = track_w + padding;
    oh = (double)win_h - top_y;
}

void MorphScrollBar::horizontal_track_rect(int offset_x, int win_w, int win_h,
                                            double right_reserved, double bottom_margin,
                                            double &ox, double &oy, double &ow, double &oh) const noexcept {
    double track_h = 14.0, padding = 2.0;
    ox = (double)offset_x + 8.0;
    oy = (double)win_h - track_h - padding - bottom_margin;
    ow = std::max(0.0, (double)(win_w - offset_x) - right_reserved - 16.0);
    oh = track_h + padding;
}

//  SECTION 14 - MorphSlider::draw

void MorphSlider::draw(cairo_t *cr, PangoLayout *layout, PangoFontDescription *fnt,
                       const ThemeVariant &tv) noexcept {
    double hv   = anim_hover.value;
    double fill = anim_fill.value;
    double x    = bounds.x, y = bounds.y, w = bounds.w, h = bounds.h;

    double track_y  = y + h / 2.0 - 2.5;
    double track_h2 = 5.0;
    double track_x  = x + 10.0;
    double track_w  = w - 20.0;
    double track_r  = track_h2 / 2.0;

    // ............ Track (inactive) ............
    cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, 0.25 + 0.10 * hv);
    rrect(cr, track_x, track_y, track_w, track_h2, track_r);
    cairo_fill(cr);

    // ............ Track (filled / active) ............
    double fill_w = track_w * fill;
    if (fill_w > 0.5) {
        cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b,
                              0.75 + 0.20 * hv);
        rrect(cr, track_x, track_y, fill_w, track_h2, track_r);
        cairo_fill(cr);
    }

    // ............ Thumb: strict rectangle, minimal corners ............
    double thumb_cx = track_x + fill_w;
    double thumb_cy = y + h / 2.0;
    double tw = disabled ? 6.0 : 8.0, th = disabled ? 12.0 : 16.0;

    cairo_set_source_rgb(cr, 1, 1, 1);
    rrect(cr, thumb_cx - tw / 2, thumb_cy - th / 2, tw, th, shape::R);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.60 + 0.35 * hv);
    cairo_set_line_width(cr, 1.5);
    rrect(cr, thumb_cx - tw / 2 + .75, thumb_cy - th / 2 + .75, tw - 1.5, th - 1.5, shape::R);
    cairo_stroke(cr);

    // ............ Label and meaning ............
    if (!label.empty() || !format.empty()) {
        cairo_set_source_rgba(cr, tv.text, tv.text, tv.text, 0.7);
        pango_font_description_set_size(fnt, 9 * PANGO_SCALE);
        pango_layout_set_font_description(layout, fnt);
        pango_layout_set_attributes(layout, nullptr);

        if (!label.empty()) {
            pango_layout_set_text(layout, label.c_str(), -1);
            cairo_move_to(cr, track_x, y + 2);
            pango_cairo_show_layout(cr, layout);
        }

        if (!format.empty()) {
            char buf[64];
            double display_v = min_v + fill * (max_v - min_v);
            // Formatting depends on the string
            if (format.find('%') != std::string::npos) {
                // Check the format type
                if (format.find('d') != std::string::npos ||
                    format.find('i') != std::string::npos) {
                    std::snprintf(buf, sizeof(buf), format.c_str(), (int)std::round(display_v));
                } else {
                    std::snprintf(buf, sizeof(buf), format.c_str(), display_v);
                }
            } else {
                std::snprintf(buf, sizeof(buf), "%.1f", display_v);
            }
            pango_layout_set_text(layout, buf, -1);
            int tw, th;
            pango_layout_get_pixel_size(layout, &tw, &th);
            cairo_move_to(cr, track_x + track_w - tw, y + 2);
            pango_cairo_show_layout(cr, layout);
        }
    }
}


//  SECTION 15 - MorphProgressBar::draw

void MorphProgressBar::draw(cairo_t *cr, const ThemeVariant &tv) noexcept {
    double x  = bounds.x, y = bounds.y, w = bounds.w, h = bounds.h;
    double r  = h / 2.0;

    // Background - flat track
    cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, 0.18);
    rrect(cr, x, y, w, h, r);
    cairo_fill(cr);

    if (value >= 0) {
        // Determinate - flat fill, no gradient
        double fw = w * anim_value.value;
        if (fw > 1.0) {
            cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.9);
            rrect(cr, x, y, fw, h, r);
            cairo_fill(cr);
        }
    } else {
        // Indeterminate - flat moving bar, no fade gradient. Clipped to the
        // track bounds and rounded to match, so it still reads smoothly as
        // it enters/exits without needing a per-frame pattern.
        double bar_w = w * 0.35;
        double bar_x = x + (indeterminate_phase * (w + bar_w)) - bar_w;
        double cx1 = std::max(bar_x, x);
        double cx2 = std::min(bar_x + bar_w, x + w);
        if (cx2 > cx1) {
            cairo_save(cr);
            rrect(cr, x, y, w, h, r);
            cairo_clip(cr);
            cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.85);
            rrect(cr, bar_x, y, bar_w, h, r);
            cairo_fill(cr);
            cairo_restore(cr);
        }
    }
}


//  SECTION 16 - MorphTooltip::draw

void MorphTooltip::draw(cairo_t *cr, PangoLayout *layout, PangoFontDescription *fnt,
                        const ThemeVariant &tv, int win_w, int win_h) noexcept {
    double alpha = anim_alpha.value;
    if (alpha < 0.01 || text.empty()) return;

    // Measure the text
    pango_font_description_set_size(fnt, 10 * PANGO_SCALE);
    pango_layout_set_font_description(layout, fnt);
    pango_layout_set_text(layout, text.c_str(), -1);
    int tw, th;
    pango_layout_get_pixel_size(layout, &tw, &th);

    double pad = 8.0;
    double bw  = tw + pad * 2;
    double bh  = th + pad * 1.5;
    double bx  = anchor_x + 12.0;
    double by  = anchor_y - bh - 6.0;

    // Don't go beyond the window boundaries
    if (bx + bw > win_w - 4) bx = win_w - bw - 4;
    if (by < 4) by = anchor_y + 20.0;

    // Background rectangle
    double rr = g_rounded_corners ? 5.0 : 2.0;


    // Background
    cairo_set_source_rgba(cr,
        tv.elevated + 0.08, tv.elevated + 0.08, tv.elevated + 0.08,
        0.97 * alpha);
    rrect(cr, bx, by, bw, bh, rr);
    cairo_fill(cr);

    // Border
    cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, 0.25 * alpha);
    cairo_set_line_width(cr, 1.0);
    rrect(cr, bx + .5, by + .5, bw - 1, bh - 1, rr);
    cairo_stroke(cr);

    // Text
    cairo_set_source_rgba(cr, tv.text, tv.text, tv.text, alpha);
    cairo_move_to(cr, bx + pad, by + pad * 0.75);
    pango_cairo_show_layout(cr, layout);
}


//  SECTION 17 - MorphBadge::draw

void MorphBadge::draw(cairo_t *cr, PangoLayout *layout, PangoFontDescription *fnt,
                      const ThemeVariant &tv) noexcept {
    double scale = anim_scale.value;
    if (scale < 0.05 || count <= 0) return;

    double r = radius;


    // Fill (error color)
    cairo_set_source_rgba(cr, tv.error_r, tv.error_g, tv.error_b, 0.95 * scale);
    shape::disc(cr, cx, cy, r);
    cairo_fill(cr);

    // shadow
    char buf[8];
    if (count < 100) std::snprintf(buf, sizeof(buf), "%d", count);
    else             std::snprintf(buf, sizeof(buf), "99+");

    pango_font_description_set_size(fnt, static_cast<int>(8 * scale) * PANGO_SCALE);
    pango_font_description_set_weight(fnt, PANGO_WEIGHT_BOLD);
    pango_layout_set_font_description(layout, fnt);
    pango_layout_set_text(layout, buf, -1);
    int tw, th;
    pango_layout_get_pixel_size(layout, &tw, &th);

    cairo_set_source_rgba(cr, 1, 1, 1, scale);
    cairo_move_to(cr, cx - tw / 2.0, cy - th / 2.0);
    pango_cairo_show_layout(cr, layout);
}


//  SECTION 18 - MorphContextMenu::draw

void MorphContextMenu::draw(cairo_t *cr, PangoLayout *layout, PangoFontDescription *fnt,
                             const ThemeVariant &tv) noexcept {
    double prog = open_anim.progress();
    if (prog < 0.02) return;

    double total_h = total_height();
    double rr = g_rounded_corners ? RADIUS : 2.0;

    // ............ Menu shadow ............

    // ............ Menu background ............
    cairo_set_source_rgba(cr,
        tv.surface + 0.04, tv.surface + 0.04, tv.surface + 0.04,
        0.97 * prog);

    // Clip + opening animation scale
    cairo_save(cr);

    rrect(cr, x, y, WIDTH, total_h, rr);
    cairo_fill(cr);

    // border
    cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, 0.18 * prog);
    cairo_set_line_width(cr, 1.0);
    rrect(cr, x + .5, y + .5, WIDTH - 1, total_h - 1, rr);
    cairo_stroke(cr);

    // ............ Menu items............
    double iy = y + PAD;
    for (int i = 0; i < (int)items.size(); ++i) {
        const auto &it = items[i];

        if (it.separator) {
            // Separator
            cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, 0.15);
            cairo_set_line_width(cr, 1.0);
            cairo_move_to(cr, x + 8,         iy + SEP_H / 2.0);
            cairo_line_to(cr, x + WIDTH - 8, iy + SEP_H / 2.0);
            cairo_stroke(cr);
            iy += SEP_H;
            continue;
        }

        // Hover bacground
        double hv = it.anim_hover.value;
        if (hv > 0.01) {
            cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.09 * hv);
            rrect(cr, x + 4, iy + 2, WIDTH - 8, ITEM_H - 4, g_rounded_corners ? 5.0 : 1.0);
            cairo_fill(cr);
        }

        // Label
        double text_alpha = it.disabled ? 0.35 : (0.85 + 0.15 * hv);
        cairo_set_source_rgba(cr, tv.text, tv.text, tv.text, text_alpha * prog);

        pango_font_description_set_weight(fnt, PANGO_WEIGHT_NORMAL);
        pango_font_description_set_size(fnt, 11 * PANGO_SCALE);
        pango_layout_set_font_description(layout, fnt);
        pango_layout_set_text(layout, it.label.c_str(), -1);
        cairo_move_to(cr, x + 14, iy + (ITEM_H - 14) / 2.0);
        pango_cairo_show_layout(cr, layout);

        // Hotkey
        if (!it.shortcut.empty()) {
            cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, 0.65 * prog);
            pango_layout_set_text(layout, it.shortcut.c_str(), -1);
            int tw, th;
            pango_layout_get_pixel_size(layout, &tw, &th);
            cairo_move_to(cr, x + WIDTH - tw - 12, iy + (ITEM_H - th) / 2.0);
            pango_cairo_show_layout(cr, layout);
        }

        // Checkbox for checked items
        if (it.checked) {
            cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.9 * prog);
            // Simple checkmark
            double cx2 = x + WIDTH - (it.shortcut.empty() ? 16.0 : 40.0);
            double cy2 = iy + ITEM_H / 2.0;
            cairo_set_line_width(cr, 1.5);
            cairo_move_to(cr, cx2 - 4, cy2);
            cairo_line_to(cr, cx2 - 1, cy2 + 3.5);
            cairo_line_to(cr, cx2 + 4, cy2 - 4);
            cairo_stroke(cr);
        }

        iy += ITEM_H;
    }

    cairo_restore(cr);
}


//  SECTION 19 - MorphGLBatch (OpenGL implementation)

#if MORPH_GL

// ............ Vertex shader ............
static const char *s_vert_src = R"glsl(
#version 120
attribute vec2 a_pos;
attribute vec4 a_color;
attribute vec4 a_rect;    // x, y, w, h normalized
attribute float a_radius; // rounding radius in pixels

varying vec4  v_color;
varying vec2  v_uv;       //UV in rectangle space
varying vec4  v_rect;
varying float v_radius;
varying vec2  v_win_size;

uniform vec2 u_win_size;  // window size in pixels

void main() {
    v_color  = a_color;
    v_rect   = a_rect;
    v_radius = a_radius;
    v_win_size = u_win_size;

    // Local UVs [0..1] in the rectangle
    v_uv = (a_pos - a_rect.xy) / a_rect.zw;

    // NDC: Y inverted (Cairo draws from the top left corner)
    vec2 ndc = (a_pos / u_win_size) * 2.0 - 1.0;
    ndc.y = -ndc.y;
    gl_Position = vec4(ndc, 0.0, 1.0);
}
)glsl";

// ............ Fragment shader ............
// Implements SDF rounded rect for smooth edges without aliasing
static const char *s_frag_src = R"glsl(
#version 120
varying vec4  v_color;
varying vec2  v_uv;
varying vec4  v_rect;
varying float v_radius;
varying vec2  v_win_size;

// SDF of a rectangle with rounded corners
float sdf_rrect(vec2 p, vec2 half_size, float r) {
    vec2 d = abs(p) - half_size + r;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - r;
}

void main() {
// Pixel position relative to the center of the rectangle (in pixels)
    vec2 half_size = v_rect.zw * 0.5;
    vec2 p = (v_uv - 0.5) * v_rect.zw;

    float dist = sdf_rrect(p, half_size, v_radius);

    // Anti-aliased edge (1 px feather)
    float alpha = clamp(-dist, 0.0, 1.0);

    gl_FragColor = v_color * vec4(1.0, 1.0, 1.0, alpha);
}
)glsl";

// ............ Vertex layout ............
// Each rect expands into 6 vertices (2 triangles)
// Each vertex: x, y, r, g, b, a, rx, ry, rw, rh, radius
struct GLVertex {
    float x, y;           // position
    float r, g, b, a;     // color
    float rx, ry, rw, rh; //rectangle (for SDF)
    float radius;
};

GLuint MorphGLBatch::compile_shader(GLenum type, const char *src) noexcept {
    GLuint sh = morph_gl::glCreateShader(type);
    morph_gl::glShaderSource(sh, 1, &src, nullptr);
    morph_gl::glCompileShader(sh);
    GLint ok = 0;
    morph_gl::glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        morph_gl::glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
        std::fprintf(stderr, "[morphing] Shader compile error: %s\n", log);
        morph_gl::glDeleteShader(sh);
        return 0;
    }
    return sh;
}

GLuint MorphGLBatch::link_program(GLuint vs, GLuint fs) noexcept {
    GLuint prog = morph_gl::glCreateProgram();
    morph_gl::glAttachShader(prog, vs);
    morph_gl::glAttachShader(prog, fs);
    // Bind attributes before linking
    morph_gl::glBindAttribLocation(prog, 0, "a_pos");
    morph_gl::glBindAttribLocation(prog, 1, "a_color");
    morph_gl::glBindAttribLocation(prog, 2, "a_rect");
    morph_gl::glBindAttribLocation(prog, 3, "a_radius");
    morph_gl::glLinkProgram(prog);
    GLint ok = 0;
    morph_gl::glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        morph_gl::glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        std::fprintf(stderr, "[morphing] Program link error: %s\n", log);
        morph_gl::glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

void MorphGLBatch::init() noexcept {
    GLuint vs = compile_shader(GL_VERTEX_SHADER,   s_vert_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, s_frag_src);
    if (!vs || !fs) return;
    prog = link_program(vs, fs);
    morph_gl::glDeleteShader(vs);
    morph_gl::glDeleteShader(fs);
    if (!prog) return;

    morph_gl::glGenBuffers(1, &vbo);
    ready = true;
}

void MorphGLBatch::destroy() noexcept {
    if (vbo)  morph_gl::glDeleteBuffers(1, &vbo);
    if (prog) morph_gl::glDeleteProgram(prog);
    vbo = prog = 0;
    ready = false;
}

void MorphGLBatch::flush() noexcept {
    if (!ready || rects.empty()) return;

    // ............ Expand rects -> GL vertices (6 vertices per rect) ............
    std::vector<GLVertex> verts;
    verts.reserve(rects.size() * 6);

    for (const auto &rc : rects) {
        float x1 = rc.x,        y1 = rc.y;
        float x2 = rc.x + rc.w, y2 = rc.y + rc.h;

        // 2 triangles - 6 vertices
        float quad[6][2] = {
            {x1,y1}, {x2,y1}, {x2,y2},
            {x1,y1}, {x2,y2}, {x1,y2},
        };
        for (auto &q : quad) {
            verts.push_back({
                q[0], q[1],
                rc.r, rc.g, rc.b, rc.a,
                rc.x, rc.y, rc.w, rc.h,
                rc.radius
            });
        }
    }

    // ............ GL state ............
    morph_gl::glUseProgram(prog);
    morph_gl::glUniform2f(morph_gl::glGetUniformLocation(prog, "u_win_size"),
                (float)win_w, (float)win_h);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    morph_gl::glBindBuffer(GL_ARRAY_BUFFER, vbo);
    morph_gl::glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(verts.size() * sizeof(GLVertex)),
                 verts.data(), GL_STREAM_DRAW);

    // Attributes
    GLsizei stride = sizeof(GLVertex);
    morph_gl::glEnableVertexAttribArray(0);
    morph_gl::glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride,
                          (void*)offsetof(GLVertex, x));
    morph_gl::glEnableVertexAttribArray(1);
    morph_gl::glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride,
                          (void*)offsetof(GLVertex, r));
    morph_gl::glEnableVertexAttribArray(2);
    morph_gl::glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride,
                          (void*)offsetof(GLVertex, rx));
    morph_gl::glEnableVertexAttribArray(3);
    morph_gl::glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride,
                          (void*)offsetof(GLVertex, radius));

    morph_gl::glDrawArrays(GL_TRIANGLES, 0, (GLsizei)verts.size());

    morph_gl::glDisableVertexAttribArray(0);
    morph_gl::glDisableVertexAttribArray(1);
    morph_gl::glDisableVertexAttribArray(2);
    morph_gl::glDisableVertexAttribArray(3);
    morph_gl::glBindBuffer(GL_ARRAY_BUFFER, 0);
    morph_gl::glUseProgram(0);

    rects.clear();
}

#endif
// MORPH_GL
//  SECTION 20 - Icon Drawing

Icon icon_from_string(const std::string &s) noexcept {
    if (s == "sidebar")     return Icon::Sidebar;
    if (s == "settings")    return Icon::Settings;
    if (s == "exit")        return Icon::Exit;
    if (s == "console")     return Icon::Console;
    if (s == "addons")      return Icon::Addons;
    if (s == "split")       return Icon::Split;
    if (s == "viewers")     return Icon::Viewers;
    if (s == "General")     return Icon::General;
    if (s == "Themes")      return Icon::Themes;
    if (s == "Performance") return Icon::Performance;
    if (s == "Keybindings") return Icon::Keybindings;
    if (s == "Help")        return Icon::Help;
    if (s == "About")       return Icon::About;
    if (s == "search")      return Icon::Search;
    return Icon::IcNone;
}

// Which SVG element should be used to describe a UI component
static const char *icon_svg_name(Icon icon) noexcept {
    switch (icon) {
    case Icon::Sidebar:     return "explorer";
    case Icon::Settings:    return "settings";
    case Icon::Exit:        return "exit";
    case Icon::Console:     return "console";
    case Icon::Addons:      return "plugin";
    case Icon::Split:       return "split";
    case Icon::Viewers:     return "viewers";
    case Icon::General:     return "general";
    case Icon::Themes:      return "themes";
    case Icon::Performance: return "performance";
    case Icon::Keybindings: return "keybindings";
    case Icon::Help:        return "help";
    case Icon::About:       return "about";
    case Icon::Search:      return "search";
    default:                return nullptr;
    }
}

void draw_icon(cairo_t *cr, Icon icon, double cx, double cy, double size,
               double r, double g, double b, double alpha) noexcept {
    const char *name = icon_svg_name(icon);
    if (!name) return;
    const double box = size * 0.85;   // the SVGs fill their whole box
    svg_icons::draw_ui(cr, name, cx - box / 2, cy - box / 2, box, r, g, b, alpha);
}

}
