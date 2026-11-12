#ifndef SETTINGS_GENERAL_HXX
#define SETTINGS_GENERAL_HXX
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

void SettingsTab::draw_general(cairo_t *cr, PangoLayout *layout, PangoFontDescription *fnt,
                      int cx, int cy, int cw, int ch, const ThemeVariant &tv) {
        const int PAD  = 20;
        const int ROW_H = 52;           // compact rows
        const int SECTION_GAP = 20;
        const int BLOCK_PAD = 10;

        int cc   = (int)(cw * 0.44);
        int lmax = cc - 12;
        double sl_x = cx + PAD + cc;
        double sl_w = cw - PAD * 2 - cc - 48;
        cached_sl_x_g = sl_x; cached_sl_w_g = sl_w;

        // Heading
        draw_heading(cr, layout, fnt, "General", cx + PAD, cy + 18, tv);
        div(cr, cx + PAD, cy + 54, cw - PAD * 2, tv);

        /*
            Small link, top-right of the heading row - re-opens the first-run
            onboarding window on the next launch (see Editor::onboarding /
            Editor::first_run_done). Outside the clipped+scrolled region
            below, so it never interacts with general_total_height's math.
        */

        {
            PangoFontDescription *lf = pango_font_description_copy(fnt);
            pango_font_description_set_size(lf, (tv.font_size - 1) * PANGO_SCALE);
            pango_layout_set_font_description(layout, lf);
            pango_layout_set_text(layout, "Show Welcome Screen Again", -1);
            int lw, lh; pango_layout_get_pixel_size(layout, &lw, &lh);
            show_onboarding_link_x = cx + cw - PAD - lw;
            show_onboarding_link_y = cy + 18;
            show_onboarding_link_w = lw;
            show_onboarding_link_h = lh;
            cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b,
                                   show_onboarding_link_hovered ? 1.0 : 0.75);
            cairo_move_to(cr, show_onboarding_link_x, show_onboarding_link_y);
            pango_cairo_show_layout(cr, layout);
            pango_font_description_free(lf);
            pango_layout_set_font_description(layout, fnt);
        }

        cairo_save(cr);
        cairo_rectangle(cr, cx, cy + 58, cw, ch - 58);
        cairo_clip(cr);

        int ry = (int)(cy + 68 - general_scroll);
        char buf[64];

        // Section label: bold uppercase, accent color, above the card
        auto section_label_small = [&](const char *txt) {
            // Left accent bar (vertical strip aligned with text)
            cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.80);
            rrect(cr, cx + PAD, ry + 1, 3.0, 14.0, 1.5);
            cairo_fill(cr);
            // Label text - uppercase section title
            PangoFontDescription *sf2 = pango_font_description_copy(fnt);
            pango_font_description_set_size(sf2, (tv.font_size - 1) * PANGO_SCALE);
            pango_font_description_set_weight(sf2, PANGO_WEIGHT_BOLD);
            pango_layout_set_font_description(layout, sf2);
            pango_layout_set_text(layout, txt, -1);
            int slw, slh; pango_layout_get_pixel_size(layout, &slw, &slh);
            cairo_set_source_rgba(cr, tv.text, tv.text, tv.text, 0.88);
            cairo_move_to(cr, cx + PAD + 10, ry);
            pango_cairo_show_layout(cr, layout);
            pango_font_description_free(sf2);
            pango_layout_set_font_description(layout, fnt);
            ry += slh + 10;   // gap between title and card: text height + 10px
        };

        // Section card background
        auto section_card = [&](int sy, int sh) {
            double cr2 = g_rounded_corners ? 8.0 : 2.0;
            cairo_set_source_rgba(cr, tv.elevated, tv.elevated, tv.elevated + 0.012, 0.40);
            rrect(cr, cx + PAD - BLOCK_PAD, sy - BLOCK_PAD / 2,
                  cw - PAD * 2 + BLOCK_PAD * 2, sh + BLOCK_PAD, cr2);
            cairo_fill(cr);
            cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, 0.08);
            cairo_set_line_width(cr, 1.0);
            rrect(cr, cx + PAD - BLOCK_PAD + 0.5, sy - BLOCK_PAD / 2 + 0.5,
                  cw - PAD * 2 + BLOCK_PAD * 2 - 1, sh + BLOCK_PAD - 1, cr2);
            cairo_stroke(cr);
        };

        // SECTION: Typography
        ry += SECTION_GAP / 2;   // indentation at the top of the first section
        section_label_small("TYPOGRAPHY");
        section_card(ry, ROW_H * 4 + 8);

        auto row = [&](const char *lbl, const char *dsc, auto fn) {
            row_label(cr, layout, fnt, lbl, dsc, cx + PAD, ry, lmax, tv);
            fn();
            div(cr, cx + PAD, ry + ROW_H - 8, cw - PAD * 2, tv);
            ry += ROW_H;
        };

        row("Font Size", "Editor font size in points (8-24 pt)", [&]{
            cached_font_size_y = ry + 14;
            if (!dragging_font_size) font_size_anim = font_size_anim * 0.88 + font_size_pt * 0.12;
            snprintf(buf, sizeof(buf), "%d pt", font_size_pt);
            { double spd=g_animations_enabled?0.18:1.0;
              drag_anim_font_size += ((dragging_font_size?1.0:0.0)-drag_anim_font_size)*spd;
              slider(cr,layout,fnt,sl_x,ry+14,sl_w,font_size_anim,8.0,24.0,buf,tv,drag_anim_font_size); }
        });
        row("Font Family", "Monospace font used in the editor", [&]{
            cached_font_family_y = ry + 4;
            double bx = sl_x, by = ry + 4, bsz = 24, pw2 = sl_w - bsz * 2 - 10;
            double cr2 = g_rounded_corners ? 6.0 : 2.0;
            cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, 0.18);
            rrect(cr, bx, by, bsz, bsz, cr2); cairo_fill(cr);
            cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.70);
            cairo_set_line_width(cr, 1.5);
            cairo_move_to(cr, bx+16, by+7); cairo_line_to(cr, bx+9, by+12); cairo_line_to(cr, bx+16, by+17);
            cairo_stroke(cr);
            double px = bx + bsz + 5;
            cairo_set_source_rgba(cr, tv.elevated * 1.6, tv.elevated * 1.6, tv.elevated * 1.6, 0.90);
            rrect(cr, px, by, pw2, bsz, cr2); cairo_fill(cr);
            cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.35);
            cairo_set_line_width(cr, 1.0);
            rrect(cr, px + 0.5, by + 0.5, pw2 - 1, bsz - 1, cr2); cairo_stroke(cr);
            {
                PangoFontDescription *sf2 = pango_font_description_copy(fnt);
                pango_font_description_set_size(sf2, 10 * PANGO_SCALE);
                pango_layout_set_font_description(layout, sf2);
                pango_layout_set_text(layout, font_families[font_family_idx].c_str(), -1);
                int iw, ih; pango_layout_get_pixel_size(layout, &iw, &ih);
                cairo_set_source_rgb(cr, tv.text, tv.text, tv.text);
                cairo_move_to(cr, px + (pw2 - iw) / 2.0, by + (bsz - ih) / 2.0);
                pango_cairo_show_layout(cr, layout);
                pango_font_description_free(sf2);
                pango_layout_set_font_description(layout, fnt);
            }
            double rx2 = px + pw2 + 5;
            cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, 0.18);
            rrect(cr, rx2, by, bsz, bsz, cr2); cairo_fill(cr);
            cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.70);
            cairo_set_line_width(cr, 1.5);
            cairo_move_to(cr, rx2 + 8, by + 7); cairo_line_to(cr, rx2 + 15, by + 12); cairo_line_to(cr, rx2 + 8, by + 17);
            cairo_stroke(cr);
        });
        row("Line Spacing", "Vertical space between lines (0.5x - 2.0x)", [&]{
            cached_spacing_y = ry + 14;
            if (!dragging_spacing) spacing_anim = spacing_anim * 0.88 + line_spacing * 0.12;
            snprintf(buf, sizeof(buf), "%.2fx", line_spacing);
            { double spd=g_animations_enabled?0.18:1.0;
              drag_anim_spacing += ((dragging_spacing?1.0:0.0)-drag_anim_spacing)*spd;
              slider(cr,layout,fnt,sl_x,ry+14,sl_w,spacing_anim,0.5,2.0,buf,tv,drag_anim_spacing); }
        });
        row("Indent Width", "Spaces per indent level (1-8)", [&]{
            cached_indent_y = ry + 14;
            if (!dragging_indent) indent_size_anim = indent_size_anim * 0.88 + indent_size * 0.12;
            snprintf(buf, sizeof(buf), "%d spaces", indent_size);
            { double spd=g_animations_enabled?0.18:1.0;
              drag_anim_indent += ((dragging_indent?1.0:0.0)-drag_anim_indent)*spd;
              slider(cr,layout,fnt,sl_x,ry+14,sl_w,indent_size_anim,1.0,8.0,buf,tv,drag_anim_indent); }
        });

        ry += SECTION_GAP;

        // SECTION: Display
        section_label_small("DISPLAY");
        section_card(ry, ROW_H * 3 + 8);

        row("Line Numbers", "Show line numbers in the gutter", [&]{
            toggle_line_numbers.x = sl_x; toggle_line_numbers.y = ry + 6;
            toggle_line_numbers.w = 52;   toggle_line_numbers.h = 28;
            toggle_line_numbers.value = show_line_numbers;
            toggle_line_numbers.tick(); toggle_line_numbers.draw(cr, tv);
        });
        row("Gutter Width", "Width of the line-number gutter (0.5x - 2.0x)", [&]{
            cached_gutter_y = ry + 14;
            if (!dragging_gutter) gutter_anim = gutter_anim * 0.88 + gutter_density * 0.12;
            snprintf(buf, sizeof(buf), "%.1fx", gutter_density);
            { double spd=g_animations_enabled?0.18:1.0;
              drag_anim_gutter += ((dragging_gutter?1.0:0.0)-drag_anim_gutter)*spd;
              slider(cr,layout,fnt,sl_x,ry+14,sl_w,gutter_anim,0.5,2.0,buf,tv,drag_anim_gutter); }
        });
        row("Splash Screen", "Show the welcome animation on startup", [&]{
            toggle_splash.x = sl_x; toggle_splash.y = ry + 6;
            toggle_splash.w = 52;   toggle_splash.h = 28;
            toggle_splash.value = show_splash;
            toggle_splash.tick(); toggle_splash.draw(cr, tv);
        });

        ry += SECTION_GAP;

        // SECTION: Editor Hints
        section_label_small("EDITOR HINTS");
        section_card(ry, ROW_H * 5 + 8);

        row("Autocomplete", "Show code completion suggestions while typing", [&]{
            toggle_autocomplete.x = sl_x; toggle_autocomplete.y = ry + 6;
            toggle_autocomplete.w = 52;   toggle_autocomplete.h = 28;
            toggle_autocomplete.value = autocomplete_on;
            toggle_autocomplete.tick(); toggle_autocomplete.draw(cr, tv);
        });
        row("Word Suggestions", "Suggest similar words already present in the file", [&]{
            toggle_word_hints.x = sl_x; toggle_word_hints.y = ry + 6;
            toggle_word_hints.w = 52;   toggle_word_hints.h = 28;
            toggle_word_hints.value = word_hints_on;
            toggle_word_hints.tick(); toggle_word_hints.draw(cr, tv);
        });
        row("Inline Hints", "Show inline parameter and type hints as ghost text", [&]{
            toggle_inline_hints.x = sl_x; toggle_inline_hints.y = ry + 6;
            toggle_inline_hints.w = 52;   toggle_inline_hints.h = 28;
            toggle_inline_hints.value = inline_hints_on;
            toggle_inline_hints.tick(); toggle_inline_hints.draw(cr, tv);
        });
        row("Double-Click Select", "Select the whole word under cursor on double-tap", [&]{
            toggle_double_click_word.x = sl_x; toggle_double_click_word.y = ry + 6;
            toggle_double_click_word.w = 52;   toggle_double_click_word.h = 28;
            toggle_double_click_word.value = double_click_word_select;
            toggle_double_click_word.tick(); toggle_double_click_word.draw(cr, tv);
        });
        row_label(cr, layout, fnt, "Auto Brackets",
                  "Auto-insert closing bracket / quote after opening one",
                  cx + PAD, ry, lmax, tv);
        toggle_auto_bracket.x = sl_x; toggle_auto_bracket.y = ry + 6;
        toggle_auto_bracket.w = 52;   toggle_auto_bracket.h = 28;
        toggle_auto_bracket.value = auto_bracket;
        toggle_auto_bracket.tick(); toggle_auto_bracket.draw(cr, tv);
        div(cr, cx + PAD, ry + ROW_H - 8, cw - PAD * 2, tv);

        cairo_restore(cr);

        // Scrollbar
        double total_gen = 68.0 + SECTION_GAP/2 + (24 + ROW_H * 4 + 8 + SECTION_GAP + 4) * 3 - 10;
        general_total_height = total_gen; // exposed so handle_scroll() clamps to the same bound
        double view_h = (double)(ch - 64);
        if (total_gen > view_h) {
            general_scroll = std::clamp(general_scroll, 0.0, total_gen - view_h);
            // Press anim tick (speed 20 = fast response)
            double sp_g = g_animations_enabled ? 0.22 : 1.0;
            scroll_press_anim_g += (scroll_pressed ? 1.0 : 0.0 - scroll_press_anim_g) * sp_g;
            scrollbar(cr, cx + cw - 10, (double)(cy + 64), view_h, general_scroll, total_gen, tv,
                      scroll_press_anim_g, &general_scroll);
        }
    }

#endif
