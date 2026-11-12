#ifndef SETTINGS_SHARED_HXX
#define SETTINGS_SHARED_HXX
/*
    settings_help_about.hxx - definitions of SettingsTab class methods
    extracted from the settings_tab.hxx source file.
    This file is included at the end of settings_tab.hxx, after the class declaration
    (where these methods remain represented only by prototypes);
    it employs the same approach used for standard .cxx files but maintains
    the "header-only" format adopted by this project.
    Do not include this file directly; it requires the SettingsTab class
    to be fully declared already. (Though you can try-I won't stop you.)
*/

void SettingsTab::draw_heading(cairo_t *cr, PangoLayout *layout, PangoFontDescription *fnt,
                      const char *title, int cx, int cy, const ThemeVariant &tv) {
        // Large bold title
        PangoFontDescription *hf = pango_font_description_copy(fnt);
        pango_font_description_set_weight(hf, PANGO_WEIGHT_BOLD);
        pango_font_description_set_size(hf, (tv.font_size + 5) * PANGO_SCALE);
        pango_layout_set_font_description(layout, hf);
        pango_layout_set_text(layout, title, -1);
        pango_layout_set_width(layout, -1);
        cairo_set_source_rgb(cr, tv.text, tv.text, tv.text);
        cairo_move_to(cr, cx, cy);
        pango_cairo_show_layout(cr, layout);
        pango_font_description_free(hf);
        pango_font_description_set_weight(fnt, PANGO_WEIGHT_NORMAL);
        pango_font_description_set_size(fnt, tv.font_size * PANGO_SCALE);
        pango_layout_set_font_description(layout, fnt);
    }

void SettingsTab::row_label(cairo_t *cr,PangoLayout *layout,PangoFontDescription *fnt,
                    const char *label,const char *desc,int cx,int cy,int max_w,const ThemeVariant &tv) {
        // Title - semibold, ellipsize at max_w
        PangoFontDescription *bf = pango_font_description_copy(fnt);
        pango_font_description_set_weight(bf, PANGO_WEIGHT_SEMIBOLD);
        pango_font_description_set_size(bf, tv.font_size * PANGO_SCALE);
        pango_layout_set_font_description(layout, bf);
        pango_layout_set_width(layout, max_w * PANGO_SCALE);
        pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
        cairo_set_source_rgb(cr, tv.text, tv.text, tv.text);
        pango_layout_set_text(layout, label, -1);
        cairo_move_to(cr, cx, cy + 4);
        pango_cairo_show_layout(cr, layout);
        pango_font_description_free(bf);
        // Description - small muted, ellipsize
        if (desc && desc[0]) {
            PangoFontDescription *sm = pango_font_description_copy(fnt);
            pango_font_description_set_size(sm, (int)(tv.font_size * 0.86) * PANGO_SCALE);
            pango_font_description_set_weight(sm, PANGO_WEIGHT_NORMAL);
            pango_layout_set_font_description(layout, sm);
            pango_layout_set_width(layout, max_w * PANGO_SCALE);
            pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
            cairo_set_source_rgba(cr, tv.muted * 1.35, tv.muted * 1.35, tv.muted * 1.35, 0.78);
            pango_layout_set_text(layout, desc, -1);
            cairo_move_to(cr, cx, cy + 22);
            pango_cairo_show_layout(cr, layout);
            pango_font_description_free(sm);
        }
        pango_font_description_set_weight(fnt, PANGO_WEIGHT_NORMAL);
        pango_layout_set_width(layout, -1);
        pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_NONE);
        pango_layout_set_font_description(layout, fnt);
    }

void SettingsTab::slider(cairo_t *cr,PangoLayout *layout,PangoFontDescription *fnt,
                double sl_x,double sl_y,double sl_w,
                double anim_val,double min_v,double max_v,
                const char *val_str,const ThemeVariant &tv,
                double drag_anim) {
                    // drag_anim is kept for the call sites; the thumb no longer animates
                    (void)drag_anim;
        double prog    = std::clamp((anim_val - min_v) / (max_v - min_v), 0.0, 1.0);

        // Same look as the sliders of the Welcome screen
        morph::draw_slider_bar(cr, sl_x, sl_y, sl_w, prog, tv);

        // ............ Value label ............
        PangoFontDescription *sm = pango_font_description_copy(fnt);
        pango_font_description_set_size(sm, (int)(tv.font_size * 0.85) * PANGO_SCALE);
        pango_font_description_set_weight(sm, PANGO_WEIGHT_MEDIUM);
        pango_layout_set_font_description(layout, sm);
        cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.95);
        pango_layout_set_text(layout, val_str, -1);
        int vw, vh;
        pango_layout_get_pixel_size(layout, &vw, &vh);
        cairo_move_to(cr, sl_x + sl_w + 14, sl_y - vh / 2.0);
        pango_cairo_show_layout(cr, layout);
        pango_font_description_free(sm);
        pango_layout_set_font_description(layout, fnt);
    }

#endif
