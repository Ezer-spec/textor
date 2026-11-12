#ifndef ONBOARDING_MODAL_HXX
#define ONBOARDING_MODAL_HXX

/*
    onboarding_modal.hxx - small "quick setup" window shown once, the very
    first time Textor is launched (see Editor::first_run_done in
    editor.hxx and the `first_run_done` key in settings.conf). Covers every
    toggle/slider in Settings -> General and Settings -> Performance, so
    "configure the editor for first use" genuinely means all of it, not a
    curated subset.
    Deliberately does NOT reuse SettingsTab::draw_general/draw_perf: those
    assume they're the only thing on screen (their own nav sidebar, their
    own cached click-geometry keyed to a specific section). Building the
    same set of controls here, generically from two small tables (one for
    toggles, one for sliders) instead of one hand-written row per control,
    keeps this file short despite covering everything.
*/
#pragma once

#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <X11/keysym.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "../color/themes.hxx"
#include "morphing.hxx"

struct OnboardingModal {
    bool visible = false;
    AnimController open_anim;   // fade + scale-in when shown

    // -- Values this window edits - mirrors of the matching SettingsTab
    // fields. sync_from_settings()/Editor::finish_onboarding() move them
    // in and out; kept separate from SettingsTab so nothing changes for
    // real until "Finish Setup" is actually pressed.
    bool show_line_numbers      = true;
    bool show_splash            = true;
    bool autocomplete_on        = true;
    bool auto_bracket           = true;
    bool word_hints_on          = true;
    bool inline_hints_on        = true;
    bool double_click_select    = true;
    bool rounded_corners        = true;
    bool animations_enabled     = true;
    bool smart_render           = true;
    bool text_optimized         = true;
    bool line_scroll            = true;

    double indent_size    = 4;     // 2..8
    double gutter_density = 1.0;   // 0.5..2.0
    double line_spacing   = 1.0;   // 0.5..2.0
    double font_size_pt   = 12;    // 8..24
    double fps_max        = 60;    // 15..240
    double fps_avg        = 30;    // 5..120

    struct ToggleSpec {
        const char *label, *desc;
        bool *value;
        double row_y = 0, row_h = 0; // cached each draw() for hit-testing
        // Widget lives in toggle_widgets[], indexed in lockstep with this
        // vector - ToggleButton has reference members (bounds.x/y/w/h
        // aliases), which makes it non-assignable and so unusable as a
        // std::vector element type directly; keeping widgets in their own
        // fixed-size array sidesteps that instead of fighting it.
    };
    struct SliderSpec {
        const char *label;
        double *value;
        double lo, hi;
        int decimals;              // 0 = integer display, else fixed decimals
        double row_y = 0;          // cached each draw()
        double track_x = 0, track_w = 0;
    };

    static constexpr int MAX_TOGGLES = 12;
    ToggleButton toggle_widgets[MAX_TOGGLES];

    std::vector<ToggleSpec> toggles;
    std::vector<SliderSpec> sliders;
    bool built = false;

    void build_specs() {
        if (built) return;
        built = true;
        toggles = {
            {"Line numbers",    "Show line numbers in the gutter",              &show_line_numbers},
            {"Splash screen",   "Play the intro animation on startup",          &show_splash},
            {"Autocomplete",    "Suggest completions while typing",             &autocomplete_on},
            {"Auto-close brackets","Insert the matching ) ] } \" automatically",&auto_bracket},
            {"Word suggestions","Suggest words already used in the buffer",     &word_hints_on},
            {"Inline hints",    "Show type/parameter hints while typing",       &inline_hints_on},
            {"Double-click select","Double-click selects the word under cursor",&double_click_select},
            {"Rounded corners", "Rounded UI elements vs. flat pixel-perfect",   &rounded_corners},
            {"Animations",      "Smooth transitions - disable to save CPU",     &animations_enabled},
            {"Smart render",    "Only render visible lines",                    &smart_render},
            {"Optimized text",  "Faster font rendering, less sub-pixel AA",     &text_optimized},
            {"Inertial scroll", "Mouse wheel scrolling keeps coasting briefly", &line_scroll},
        };
        sliders = {
            {"Indent size",    &indent_size,    2,   8,   0},
            {"Gutter width",   &gutter_density, 0.5, 2.0, 2},
            {"Line spacing",   &line_spacing,   0.5, 2.0, 2},
            {"Font size",      &font_size_pt,   8,   24,  0},
            {"Max FPS",        &fps_max,        15,  240, 0},
            {"Average FPS",    &fps_avg,        5,   120, 0},
        };
    }

    int dragging_slider = -1;   // index into sliders, or -1

    TextButton btn_finish;
    double scroll_y      = 0.0;
    double content_h      = 0.0;   // measured by the last draw() - for scroll clamping
    double view_h_cached  = 0.0;
    double modal_x = 0, modal_y = 0, modal_w = 620, modal_h = 0;

    void sync_from_settings(bool ln, bool sp, bool ac, bool ab, bool wh, bool ih, bool dc,
                             bool rc, bool an, bool sr, bool to, bool ls,
                             double ind, double gd, double lsp, double fsz, double fmax, double favg) {
        build_specs();
        show_line_numbers=ln; show_splash=sp; autocomplete_on=ac; auto_bracket=ab;
        word_hints_on=wh; inline_hints_on=ih; double_click_select=dc;
        rounded_corners=rc; animations_enabled=an; smart_render=sr; text_optimized=to; line_scroll=ls;
        indent_size=ind; gutter_density=gd; line_spacing=lsp; font_size_pt=fsz; fps_max=fmax; fps_avg=favg;
        for (size_t i = 0; i < toggles.size() && i < MAX_TOGGLES; i++) toggle_widgets[i].value = *toggles[i].value;
    }

    double scroll_max() const { return std::max(0.0, content_h - view_h_cached); }

    static std::string fmt_val(double v, int decimals) {
        char buf[32];
        if (decimals == 0) snprintf(buf, sizeof(buf), "%d", (int)std::lround(v));
        else                snprintf(buf, sizeof(buf), "%.*f", decimals, v);
        return buf;
    }

    void draw(cairo_t *cr, int win_w, int win_h,
              PangoLayout *layout, PangoFontDescription *fnt, const ThemeVariant &tv)
    {
        build_specs();
        if (!visible) return;
        double p = open_anim.progress();
        if (p <= 0.001) return;

        cairo_set_source_rgba(cr, 0, 0, 0, 0.55 * p);
        cairo_rectangle(cr, 0, 0, win_w, win_h);
        cairo_fill(cr);

        modal_w = std::min((double)win_w - 60.0, 620.0);
        modal_h = std::min((double)win_h - 60.0, 680.0);
        modal_x = (win_w - modal_w) / 2.0;
        modal_y = (win_h - modal_h) / 2.0;

        double scale = 1.0;
        cairo_save(cr);
        cairo_translate(cr, modal_x + modal_w/2.0, modal_y + modal_h/2.0);
        cairo_scale(cr, scale, scale);
        cairo_translate(cr, -(modal_x + modal_w/2.0), -(modal_y + modal_h/2.0));
        cairo_push_group(cr);

        // ............ Window body ............
        cairo_set_source_rgb(cr, tv.elevated, tv.elevated, tv.elevated);
        morph::rrect(cr, modal_x, modal_y, modal_w, modal_h, 14.0);
        cairo_fill(cr);
        cairo_set_source_rgba(cr, 0, 0, 0, 0.25);
        morph::rrect(cr, modal_x, modal_y, modal_w, modal_h, 14.0);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);

        const double PAD = 28.0;
        double cx = modal_x + PAD, cw = modal_w - PAD * 2.0;

        // ............ Header: editor name + subtitle - both centered ............
        double header_bottom;
        {
            cairo_save(cr);
            cairo_rectangle(cr, cx, modal_y, cw, 80.0);
            cairo_clip(cr);

            PangoFontDescription *hf = pango_font_description_from_string((tv.font_family + " Bold 24").c_str());
            pango_layout_set_font_description(layout, hf);
            pango_layout_set_text(layout, "Textor", -1);
            int tw, th; pango_layout_get_pixel_size(layout, &tw, &th);
            cairo_set_source_rgb(cr, tv.accent_r, tv.accent_g, tv.accent_b);
            cairo_move_to(cr, modal_x + (modal_w - tw)/2.0, modal_y + 20.0);
            pango_cairo_show_layout(cr, layout);
            pango_font_description_free(hf);

            PangoFontDescription *sf = pango_font_description_from_string((tv.font_family + " 10.5").c_str());
            pango_layout_set_font_description(layout, sf);
            pango_layout_set_width(layout, (int)(cw * PANGO_SCALE));
            pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
            pango_layout_set_text(layout, "Quick setup - you can change any of this later in Settings", -1);
            cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, 0.9);
            cairo_move_to(cr, cx, modal_y + 56.0);
            pango_cairo_show_layout(cr, layout);
            pango_layout_set_width(layout, -1);
            pango_layout_set_alignment(layout, PANGO_ALIGN_LEFT);
            pango_font_description_free(sf);
            cairo_restore(cr);

            cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, 0.18);
            cairo_rectangle(cr, cx, modal_y + 82.0, cw, 1);
            cairo_fill(cr);
            header_bottom = modal_y + 96.0;
        }

        // ............ Scrollable body: every toggle, then every slider ............
        double footer_h = 76.0;   // Finish button strip, not scrolled
        view_h_cached = modal_h - (header_bottom - modal_y) - footer_h;

        cairo_save(cr);
        cairo_rectangle(cr, modal_x, header_bottom, modal_w, view_h_cached);
        cairo_clip(cr);

        double y = header_bottom - scroll_y;
        const double ROW_H = 44.0;
        double label_max_w = cw - 64.0;   // leave room for the toggle switch

        for (size_t ti = 0; ti < toggles.size(); ti++) {
            auto &t = toggles[ti];
            ToggleButton &w = toggle_widgets[ti];
            t.row_y = y; t.row_h = ROW_H;
            if (y + ROW_H >= header_bottom - ROW_H && y <= header_bottom + view_h_cached) {
                cairo_save(cr);
                cairo_rectangle(cr, cx, y, label_max_w, ROW_H);
                cairo_clip(cr);
                PangoFontDescription *lf = pango_font_description_from_string((tv.font_family + " 11").c_str());
                pango_layout_set_font_description(layout, lf);
                pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
                pango_layout_set_width(layout, (int)(label_max_w * PANGO_SCALE));
                pango_layout_set_text(layout, t.label, -1);
                cairo_set_source_rgb(cr, tv.text, tv.text, tv.text);
                cairo_move_to(cr, cx, y + 3.0);
                pango_cairo_show_layout(cr, layout);
                pango_font_description_free(lf);

                PangoFontDescription *df = pango_font_description_from_string((tv.font_family + " 8.5").c_str());
                pango_layout_set_font_description(layout, df);
                pango_layout_set_text(layout, t.desc, -1);
                cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, 0.85);
                cairo_move_to(cr, cx, y + 21.0);
                pango_cairo_show_layout(cr, layout);
                pango_font_description_free(df);
                pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_NONE);
                pango_layout_set_width(layout, -1);
                cairo_restore(cr);

                w.x = cx + cw - 52.0; w.y = y + 4.0; w.w = 52.0; w.h = 26.0;
                w.tick();
                w.draw(cr, tv);
            } else {
                w.x = cx + cw - 52.0; w.y = y + 4.0; w.w = 52.0; w.h = 26.0;
            }
            y += ROW_H;
        }

        y += 8.0;
        for (auto &s : sliders) {
            s.row_y = y;
            if (y + 44.0 >= header_bottom - 44.0 && y <= header_bottom + view_h_cached) {
                PangoFontDescription *lf = pango_font_description_from_string((tv.font_family + " 11").c_str());
                pango_layout_set_font_description(layout, lf);
                std::string lbl = std::string(s.label) + ": " + fmt_val(*s.value, s.decimals);
                pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
                pango_layout_set_width(layout, (int)(cw * PANGO_SCALE));
                pango_layout_set_text(layout, lbl.c_str(), -1);
                cairo_set_source_rgb(cr, tv.text, tv.text, tv.text);
                cairo_move_to(cr, cx, y);
                pango_cairo_show_layout(cr, layout);
                pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_NONE);
                pango_layout_set_width(layout, -1);
                pango_font_description_free(lf);

                s.track_x = cx; s.track_w = cw;
                double track_y = y + 26.0;
                double frac = std::clamp((*s.value - s.lo) / (s.hi - s.lo), 0.0, 1.0);
                morph::draw_slider_bar(cr, s.track_x, track_y + 2.0, s.track_w, frac, tv);
            } else {
                s.track_x = cx; s.track_w = cw;
            }
            y += 46.0;
        }

        content_h = y - (header_bottom - scroll_y);
        cairo_restore(cr); // end clip

        if (scroll_max() > 0.5) {
            double track_h = view_h_cached - 8.0;
            double thumb_h = std::max(24.0, track_h * (view_h_cached / content_h));
            double thumb_y = header_bottom + 4.0 + (track_h - thumb_h) * (scroll_y / scroll_max());
            cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.4);
            morph::rrect(cr, modal_x + modal_w - 6.0, thumb_y, 2.5, thumb_h, 1.25);
            cairo_fill(cr);
        }

        // ............ Footer: Finish Setup button - centered ............
        {
            cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, 0.14);
            cairo_rectangle(cr, modal_x, modal_y + modal_h - footer_h, modal_w, 1);
            cairo_fill(cr);

            double bw = 200.0, bh = 42.0;
            btn_finish.x = modal_x + (modal_w - bw) / 2.0;
            btn_finish.y = modal_y + modal_h - footer_h/2.0 - bh/2.0;
            btn_finish.w = bw; btn_finish.h = bh;
            btn_finish.label = "Finish Setup";
            btn_finish.primary = true;
            btn_finish.tick(btn_finish.hovered, btn_finish.pressed);
            btn_finish.draw(cr, layout, fnt, tv);
        }

        cairo_pop_group_to_source(cr);
        cairo_paint_with_alpha(cr, p);
        cairo_restore(cr);
    }

    // Returns true if the Finish button was clicked (caller applies +
    // saves settings and hides this window).
    bool handle_mouse_down(int mx, int my) {
        if (!visible) return false;
        for (size_t i = 0; i < toggles.size(); i++) {
            ToggleButton &w = toggle_widgets[i];
            if (w.is_hit(mx, my)) {
                w.pressed = true;
                w.toggle();
                *toggles[i].value = w.value;
                return false;
            }
        }
        for (size_t i = 0; i < sliders.size(); i++) {
            auto &s = sliders[i];
            if (mx >= s.track_x && mx <= s.track_x + s.track_w &&
                my >= s.row_y + 14.0 && my <= s.row_y + 40.0) {
                dragging_slider = (int)i;
                update_slider_from_mouse(mx);
                return false;
            }
        }
        if (btn_finish.is_hit(mx, my)) {
            btn_finish.pressed = true;
            return true;
        }
        return false;
    }

    void update_slider_from_mouse(int mx) {
        if (dragging_slider < 0 || dragging_slider >= (int)sliders.size()) return;
        auto &s = sliders[dragging_slider];
        double frac = std::clamp((mx - s.track_x) / std::max(1.0, s.track_w), 0.0, 1.0);
        double v = s.lo + frac * (s.hi - s.lo);
        if (s.decimals == 0) v = std::round(v);
        *s.value = v;
    }

    void handle_mouse_move(int mx, int my) {
        if (!visible) return;
        if (dragging_slider >= 0) { update_slider_from_mouse(mx); return; }
        for (size_t i = 0; i < toggles.size(); i++) toggle_widgets[i].hovered = toggle_widgets[i].is_hit(mx, my);
        btn_finish.hovered = btn_finish.is_hit(mx, my);
    }

    void handle_mouse_up() {
        dragging_slider = -1;
        for (size_t i = 0; i < toggles.size(); i++) toggle_widgets[i].pressed = false;
        btn_finish.pressed = false;
    }

    void handle_scroll(int delta) {
        if (!visible) return;
        // Same sign convention as every other scrollable panel in the app
        // (sidebar, console, settings, file picker: scroll_y += delta) -
        // this used to subtract delta, which made the wheel feel inverted
        // compared to everywhere else in the editor.
        scroll_y = std::clamp(scroll_y + (double)delta, 0.0, scroll_max());
    }

    // Keyboard scrolling (Up/Down/PageUp/PageDown/Home/End) - the onboarding
    // window used to swallow all keyboard input with nothing hooked up for
    // scrolling, so it could only be scrolled with the mouse wheel.
    // Returns true if the key was consumed.
    bool handle_key_scroll(long keysym) {
        if (!visible) return false;
        const double LINE_STEP = 46.0;   // one row's worth
        const double PAGE_STEP = std::max(1.0, view_h_cached - 40.0);
        switch (keysym) {
            case XK_Up:
                scroll_y = std::clamp(scroll_y - LINE_STEP, 0.0, scroll_max());
                return true;
            case XK_Down:
                scroll_y = std::clamp(scroll_y + LINE_STEP, 0.0, scroll_max());
                return true;
            case XK_Page_Up:
                scroll_y = std::clamp(scroll_y - PAGE_STEP, 0.0, scroll_max());
                return true;
            case XK_Page_Down:
                scroll_y = std::clamp(scroll_y + PAGE_STEP, 0.0, scroll_max());
                return true;
            case XK_Home:
                scroll_y = 0.0;
                return true;
            case XK_End:
                scroll_y = scroll_max();
                return true;
            default:
                return false;
        }
    }
};

#endif
