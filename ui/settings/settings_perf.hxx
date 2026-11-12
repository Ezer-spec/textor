#ifndef SETTINGS_PERF_HXX
#define SETTINGS_PERF_HXX
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

void SettingsTab::draw_perf(cairo_t *cr, PangoLayout *layout, PangoFontDescription *fnt,
                   int cx, int cy, int cw, int ch, const ThemeVariant &tv) {
        const int PAD  = 20;
        const int ROW_H = 52;           // compact rows
        const int SECTION_GAP = 20;
        const int BLOCK_PAD = 10;

        int cc   = (int)(cw * 0.44);
        int lmax = cc - 12;
        double sl_x = cx + PAD + cc;
        double sl_w = cw - PAD * 2 - cc - 48;
        cached_sl_x_p = sl_x; cached_sl_w_p = sl_w;

        draw_heading(cr, layout, fnt, "Performance", cx + PAD, cy + 18, tv);
        div(cr, cx + PAD, cy + 54, cw - PAD * 2, tv);

        cairo_save(cr);
        cairo_rectangle(cr, cx, cy + 58, cw, ch - 58);
        cairo_clip(cr);

        int ry = (int)(cy + 68 - perf_scroll);
        char buf[64];

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

        // Section label with left accent bar - above the card with a gap
        auto section_label = [&](const char *txt) {
            cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.80);
            rrect(cr, cx + PAD, ry + 1, 3.0, 14.0, 1.5);
            cairo_fill(cr);
            PangoFontDescription *sf = pango_font_description_copy(fnt);
            pango_font_description_set_size(sf, (tv.font_size - 1) * PANGO_SCALE);
            pango_font_description_set_weight(sf, PANGO_WEIGHT_BOLD);
            pango_layout_set_font_description(layout, sf);
            pango_layout_set_text(layout, txt, -1);
            int slw2, slh2; pango_layout_get_pixel_size(layout, &slw2, &slh2);
            cairo_set_source_rgba(cr, tv.text, tv.text, tv.text, 0.88);
            cairo_move_to(cr, cx + PAD + 10, ry);
            pango_cairo_show_layout(cr, layout);
            pango_font_description_free(sf);
            pango_layout_set_font_description(layout, fnt);
            ry += slh2 + 10;   // gap: text height + 10px to card
        };

        auto row = [&](const char *lbl, const char *dsc, auto fn) {
            row_label(cr, layout, fnt, lbl, dsc, cx + PAD, ry, lmax, tv);
            fn();
            div(cr, cx + PAD, ry + ROW_H - 8, cw - PAD * 2, tv);
            ry += ROW_H;
        };

        // ............ SECTION: Rendering ............
        ry += SECTION_GAP / 2;   // space at the top before the section header
        section_label("RENDERING");
        section_card(ry, ROW_H * 5 + 8);

        row("Animations", "Smooth transitions - disable to free ~3% CPU per frame", [&]{
            toggle_animations.x = sl_x; toggle_animations.y = ry + 6;
            toggle_animations.w = 52;   toggle_animations.h = 28;
            toggle_animations.tick(); toggle_animations.draw(cr, tv);
        });
        row("Rounded Corners", "Rounded UI elements - disable for flat pixel-perfect look", [&]{
            toggle_rounded.x = sl_x; toggle_rounded.y = ry + 6;
            toggle_rounded.w = 52;   toggle_rounded.h = 28;
            toggle_rounded.tick(); toggle_rounded.draw(cr, tv);
        });
        row("Optimise Text", "Fast Cairo hint mode - halves Pango CPU cost", [&]{
            toggle_text_optimized.x = sl_x; toggle_text_optimized.y = ry + 6;
            toggle_text_optimized.w = 52;   toggle_text_optimized.h = 28;
            toggle_text_optimized.value = text_optimized;
            toggle_text_optimized.tick(); toggle_text_optimized.draw(cr, tv);
        });
        row("Line Scroll", "Scroll line-by-line instead of smooth pixel scroll", [&]{
            toggle_line_scroll.x = sl_x; toggle_line_scroll.y = ry + 6;
            toggle_line_scroll.w = 52;   toggle_line_scroll.h = 28;
            toggle_line_scroll.value = line_scroll;
            toggle_line_scroll.tick(); toggle_line_scroll.draw(cr, tv);
        });
        row("Smart Render", "Only render visible lines - saves VRAM and CPU", [&]{
            toggle_smart_render.x = sl_x; toggle_smart_render.y = ry + 6;
            toggle_smart_render.w = 52;   toggle_smart_render.h = 28;
            toggle_smart_render.value = smart_render;
            toggle_smart_render.tick(); toggle_smart_render.draw(cr, tv);
        });

        ry += SECTION_GAP;

        // ............ SECTION: Frame Rate ............
        section_label("FRAME RATE");

        // ............ Dedicated FPS load bar block ............
        {
            const int BAR_BLOCK_H = 52;
            section_card(ry, BAR_BLOCK_H);

            // Block label
            PangoFontDescription *sf2 = pango_font_description_copy(fnt);
            pango_font_description_set_size(sf2, (tv.font_size - 2) * PANGO_SCALE);
            pango_layout_set_font_description(layout, sf2);
            pango_layout_set_text(layout, "Frame budget zones  (min / avg / max)", -1);
            cairo_set_source_rgba(cr, tv.muted * 1.3, tv.muted * 1.3, tv.muted * 1.3, 0.75);
            cairo_move_to(cr, cx + PAD, ry + 4);
            pango_cairo_show_layout(cr, layout);
            pango_font_description_free(sf2);
            pango_layout_set_font_description(layout, fnt);

            // Bar itself - spans full usable width
            double bar_x = cx + PAD;
            double bar_y = (double)(ry + 24);
            double bar_w = (double)(cw - PAD * 2 - 14);
            double bar_h = 10.0;
            double bar_r = g_rounded_corners ? 5.0 : 0.0;

            // Background track
            cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, 0.14);
            rrect(cr, bar_x, bar_y, bar_w, bar_h, bar_r);
            cairo_fill(cr);

            double min_prog = std::clamp((double)fps_min / 240.0, 0.0, 1.0);
            double avg_prog = std::clamp((double)fps_avg / 240.0, 0.0, 1.0);
            double max_prog = std::clamp((double)fps_max / 240.0, 0.0, 1.0);

            // Green zone up to max
            cairo_set_source_rgba(cr, tv.success_r, tv.success_g, tv.success_b, 0.40);
            rrect(cr, bar_x, bar_y, bar_w * max_prog, bar_h, bar_r);
            cairo_fill(cr);
            // Orange zone up to avg
            cairo_set_source_rgba(cr, tv.warning_r, tv.warning_g, tv.warning_b, 0.55);
            rrect(cr, bar_x, bar_y, bar_w * avg_prog, bar_h, bar_r);
            cairo_fill(cr);
            // Red zone up to min
            cairo_set_source_rgba(cr, tv.error_r, tv.error_g, tv.error_b, 0.70);
            rrect(cr, bar_x, bar_y, bar_w * min_prog, bar_h, bar_r);
            cairo_fill(cr);

            // Labels only - no tick-line at bar edge
            auto fps_tick = [&](double prog, const char *label, double cr_r, double cr_g, double cr_b) {
                double tx = bar_x + bar_w * prog;
                PangoFontDescription *tf = pango_font_description_copy(fnt);
                pango_font_description_set_size(tf, (tv.font_size - 3) * PANGO_SCALE);
                pango_layout_set_font_description(layout, tf);
                pango_layout_set_text(layout, label, -1);
                int lw2, lh2; pango_layout_get_pixel_size(layout, &lw2, &lh2);
                cairo_set_source_rgba(cr, cr_r, cr_g, cr_b, 0.75);
                cairo_move_to(cr, tx - lw2 / 2.0, bar_y + bar_h + 3);
                pango_cairo_show_layout(cr, layout);
                pango_font_description_free(tf);
                pango_layout_set_font_description(layout, fnt);
            };

            char lbuf[16];
            snprintf(lbuf, sizeof(lbuf), "%d", fps_min);
            fps_tick(min_prog, lbuf, tv.error_r, tv.error_g, tv.error_b);
            snprintf(lbuf, sizeof(lbuf), "%d", fps_avg);
            fps_tick(avg_prog, lbuf, tv.warning_r, tv.warning_g, tv.warning_b);
            snprintf(lbuf, sizeof(lbuf), "%d", fps_max);
            fps_tick(max_prog, lbuf, tv.success_r, tv.success_g, tv.success_b);

            ry += BAR_BLOCK_H + 14;
        }

        // ............ FPS slider card ............
        section_card(ry, ROW_H * 3 + 8);

        row("Max FPS", "Target frame rate ceiling (15-240 fps)", [&]{
            cached_fps_max_y = ry + 14;
            if (!dragging_fps_max) fps_max_anim = fps_max_anim * 0.88 + fps_max * 0.12;
            snprintf(buf, sizeof(buf), "%d fps", fps_max);
            { double spd=g_animations_enabled?0.18:1.0;
              drag_anim_fps_max += ((dragging_fps_max?1.0:0.0)-drag_anim_fps_max)*spd;
              slider(cr,layout,fnt,sl_x,ry+14,sl_w,fps_max_anim,15.0,240.0,buf,tv,drag_anim_fps_max); }
        });
        row("Average FPS", "Target for mid-priority frames (15-240 fps)", [&]{
            cached_fps_avg_y = ry + 14;
            if (!dragging_fps_avg) fps_avg_anim = fps_avg_anim * 0.88 + fps_avg * 0.12;
            snprintf(buf, sizeof(buf), "%d fps", fps_avg);
            { double spd=g_animations_enabled?0.18:1.0;
              drag_anim_fps_avg += ((dragging_fps_avg?1.0:0.0)-drag_anim_fps_avg)*spd;
              slider(cr,layout,fnt,sl_x,ry+14,sl_w,fps_avg_anim,15.0,240.0,buf,tv,drag_anim_fps_avg); }
        });
        cached_fps_min_y = ry + 14;
        row_label(cr, layout, fnt, "Min FPS",
                  "Floor frame rate - app never drops below this (5-120 fps)",
                  cx + PAD, ry, lmax, tv);
        if (!dragging_fps_min) fps_min_anim = fps_min_anim * 0.88 + fps_min * 0.12;
        snprintf(buf, sizeof(buf), "%d fps", fps_min);
        { double spd=g_animations_enabled?0.18:1.0;
              drag_anim_fps_min += ((dragging_fps_min?1.0:0.0)-drag_anim_fps_min)*spd;
              slider(cr,layout,fnt,sl_x,ry+14,sl_w,fps_min_anim,5.0,120.0,buf,tv,drag_anim_fps_min); }
        div(cr, cx + PAD, ry + ROW_H - 8, cw - PAD * 2, tv);

        cairo_restore(cr);

        // Scrollbar
        double total_perf = 68.0 + SECTION_GAP/2 + 24 + ROW_H * 5 + 8 + SECTION_GAP
                          + 20 + 52 + 14 + ROW_H * 3 + 8 + 20;
        perf_total_height = total_perf;
        double view_h = (double)(ch - 64);
        if (total_perf > view_h) {
            perf_scroll = std::clamp(perf_scroll, 0.0, total_perf - view_h);
            double sp_p = g_animations_enabled ? 0.22 : 1.0;
            scroll_press_anim_p += (scroll_pressed ? 1.0 : 0.0 - scroll_press_anim_p) * sp_p;
            scrollbar(cr, cx + cw - 10, (double)(cy + 64), view_h, perf_scroll, total_perf, tv,
                      scroll_press_anim_p, &perf_scroll);
        }
    }

#endif
