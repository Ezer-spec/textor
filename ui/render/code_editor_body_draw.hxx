#ifndef RENDER_CODE_EDITOR_BODY_DRAW_HXX
#define RENDER_CODE_EDITOR_BODY_DRAW_HXX

#include "../draw_context.hxx"
#include "../svg_icons.hxx"

static void draw_code_editor_body(DrawContext &dc) {
    cairo_t *cr = dc.cr;
    Editor &ed = dc.ed;
    const ThemeVariant &tv = dc.tv;
    PangoLayout *layout = dc.layout;
    PangoFontDescription *fnt = dc.fnt;
    (void)cr; (void)ed; (void)tv; (void)layout; (void)fnt;
    int    ed_x        = dc.ed_x;
    double con_h       = dc.con_h;
    double eff_height  = dc.eff_height;
    int    line_height = dc.line_height;
    cairo_t *xlib_cr   = dc.xlib_cr;

    // Gutter (only when not on settings tab)
    if (!ed.is_settings_tab_active()) {
    cairo_set_source_rgb(cr, tv.gutter_bg, tv.gutter_bg, tv.gutter_bg);
    cairo_rectangle(cr, ed_x, ed.content_top(), ed.gutter_width, (int)eff_height);
    cairo_fill(cr);

    // Current-line highlight band in gutter (matches editor highlight)
    {
        int cur_ln = 0, dummy2;
        ed.code.index_to_line_col(ed.code.cursor_pos, cur_ln, dummy2);
        double gy = ed.content_top() + 15.0 - ed.code.scroll_y + cur_ln * line_height;
        if (gy >= ed.content_top() && gy < ed.content_top() + eff_height) {
            cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.12);
            cairo_rectangle(cr, ed_x, gy, ed.gutter_width, line_height);
            cairo_fill(cr);
        }
    }

    // Thin right border separating gutter from text
    cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, 0.18);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, ed_x + ed.gutter_width - 0.5, ed.content_top());
    cairo_line_to(cr, ed_x + ed.gutter_width - 0.5, ed.content_top() + eff_height);
    cairo_stroke(cr);

    if (ed.code.line_count > 0 && ed.settings_tab.show_line_numbers) {
        cairo_save(cr);
        cairo_rectangle(cr, ed_x, ed.content_top(), ed.gutter_width, (int)eff_height);
        cairo_clip(cr);

        PangoFontDescription *num_fnt = pango_font_description_copy(fnt);
        pango_font_description_set_size(num_fnt, (tv.font_size - 1) * PANGO_SCALE);
        pango_layout_set_font_description(layout, num_fnt);

        int start_line = std::max(0, (int)(ed.code.scroll_y / line_height));
        int end_line   = std::min(ed.code.line_count,
                                   start_line + (int)(eff_height / line_height) + 2);

        int cur_line_num = 0, dummy3;
        ed.code.index_to_line_col(ed.code.cursor_pos, cur_line_num, dummy3);

        for (int i = start_line; i < end_line; ++i) {
            double ny = ed.content_top() + 15.0 - ed.code.scroll_y + i * line_height;

            if (i == cur_line_num) {
                // Current line: accent colour, full brightness
                cairo_set_source_rgb(cr, tv.accent_r, tv.accent_g, tv.accent_b);
            } else {
                // Other lines: dim, farther = dimmer (like VS Code relative numbers)
                int dist = std::abs(i - cur_line_num);
                double dim = std::max(0.30, 0.70 - dist * 0.025);
                cairo_set_source_rgba(cr, tv.gutter_text, tv.gutter_text, tv.gutter_text, dim);
            }

            std::string num = std::to_string(i + 1);
            pango_layout_set_text(layout, num.c_str(), -1);
            int tw, th; pango_layout_get_pixel_size(layout, &tw, &th);
            cairo_move_to(cr, ed_x + ed.gutter_width - tw - 12, ny);
            pango_cairo_show_layout(cr, layout);
        }
        pango_font_description_free(num_fnt);
        cairo_restore(cr);
    }
    } // end !is_settings_tab_active() for gutter
    double text_area_x = ed_x + ed.gutter_width;   // the +15 pad is inside draw_text_area

    if (ed.is_settings_tab_active()) {
        // Settings tab fills the full content area (no gutter)
        // Reset GPU allocation - it shouldn't be on the settings page
        ed.gpu_sel_active = false;
        ed.gpu_sel_rect_count = 0;
        ed.apply_settings_from_modal();
        ed.settings_tab.draw(cr, ed_x, ed.content_top(),
                              ed.width - ed_x, (int)eff_height, tv);
    } else {

    // code_top is the upper limit of the code area.
    // Find bar moves DOWN from the tab bar and moves the code below,
    // so the scrollbar is never overlapped by the bar.?
    // In project mode the find bar expands to show a results list
    const int FIND_RESULTS_H = Editor::FindBar::RESULTS_H; // height of results area in All files mode
        bool fb_project_expanded = ed.find_bar.visible && ed.find_bar.mode == 1;
        int find_bar_h = 0;
        if (ed.find_bar.visible || ed.find_bar.anim.is_running()) {
            int base_h = ed.find_bar.current_h();
            if (fb_project_expanded && base_h > 0)
                find_bar_h = base_h + FIND_RESULTS_H;
            else
                find_bar_h = base_h;
        }
        int code_top = ed.content_top() + find_bar_h;

        // Suppress editor's current-line highlight when find bar is active:
        ed.code.suppress_line_highlight =
            ed.find_bar.visible && ed.find_bar.mode==0 && !ed.find_bar.matches.empty();

        // GPU mode: xlib_cr is null - pass overlay so selection skips Cairo
        CodeEditor::GpuOverlay gpu_ov;
        bool use_gpu_overlay = (xlib_cr == nullptr);
        if (use_gpu_overlay) {
            gpu_ov.gpu_sel_active = false;
            gpu_ov.gpu_sel_rect_count = 0;
        }

        // LEFT PANE
        {
            int right_bound = ed.split.enabled
                ? (int)ed.split.divider_x(ed_x, ed.width - ed_x)
                : ed.width;

            cairo_save(cr);
            // Clip from ed_x (includes gutter) to right_bound
            cairo_rectangle(cr, ed_x, code_top,
                            right_bound - ed_x, (int)eff_height);
            cairo_clip(cr);

            ed.code.draw_text_area(cr, layout, fnt, tv,
                                    (int)text_area_x, code_top,
                                    right_bound,
                                    code_top + (int)eff_height,
                                    use_gpu_overlay ? &gpu_ov : nullptr);
            cairo_restore(cr);
        }

        // RIGHT PANE (split editor)
        if (ed.split.enabled && ed.split.enable_anim > 0.01) {
            int full_ed_w = ed.width - ed_x;
            int rx2, rw2;
            ed.split.right_rect(ed_x, full_ed_w, rx2, rw2);
            double rtx = rx2 + ed.gutter_width;

            if (rw2 > 20) {
                // Clip entire right pane
                cairo_save(cr);
                cairo_rectangle(cr, rx2, code_top, rw2, (int)eff_height);
                cairo_clip(cr);

                // Background
                cairo_set_source_rgb(cr, tv.bg, tv.bg, tv.bg + 0.006);
                cairo_paint(cr);

                if (ed.md_viewer_open) {
                    // Viewers: the formatted page instead of the text of the second pane
                    double page_h = mdview::draw(cr, tv, ed.code.get_buffer_str(),
                                                 rx2, code_top, rw2, (double)eff_height, ed.code2.scroll_y);
                    ed.code2.total_text_height = page_h;      // the scrollbar and the wheel use it
                    ed.code2.total_text_width  = 0.0;
                } else {
                    // Gutter background
                    cairo_set_source_rgb(cr, tv.gutter_bg, tv.gutter_bg, tv.gutter_bg);
                    cairo_rectangle(cr, rx2, code_top, ed.gutter_width, (int)eff_height);
                    cairo_fill(cr);

                    // Draw text area (handles selection highlight internally, clipped by cairo)
                    ed.code2.draw_text_area(cr, layout, fnt, tv,
                                             (int)rtx, code_top,
                                             rx2 + rw2,
                                             code_top + (int)eff_height,
                                             nullptr);

                    // Line numbers for right pane
                    if (ed.code2.line_count > 0 && ed.settings_tab.show_line_numbers) {
                        PangoFontDescription *num_fnt2 = pango_font_description_copy(fnt);
                        pango_font_description_set_size(num_fnt2, (tv.font_size - 1) * PANGO_SCALE);
                        pango_layout_set_font_description(layout, num_fnt2);

                        int r_start = std::max(0, (int)(ed.code2.scroll_y / line_height));
                        int r_end   = std::min(ed.code2.line_count,
                                               r_start + (int)(eff_height / line_height) + 2);
                        int cur_ln2 = 0, dummy4;
                        ed.code2.index_to_line_col(ed.code2.cursor_pos, cur_ln2, dummy4);

                        for (int i = r_start; i < r_end; ++i) {
                            double ny = code_top + 15.0 - ed.code2.scroll_y + i * line_height;
                            if (ny < code_top - line_height || ny > code_top + eff_height) continue;
                            if (i == cur_ln2)
                                cairo_set_source_rgb(cr, tv.accent_r, tv.accent_g, tv.accent_b);
                            else {
                                int dist = std::abs(i - cur_ln2);
                                double dim = std::max(0.30, 0.70 - dist * 0.025);
                                cairo_set_source_rgba(cr, tv.gutter_text, tv.gutter_text, tv.gutter_text, dim);
                            }
                            std::string num = std::to_string(i + 1);
                            pango_layout_set_text(layout, num.c_str(), -1);
                            int tw2, th2; pango_layout_get_pixel_size(layout, &tw2, &th2);
                            cairo_move_to(cr, rx2 + ed.gutter_width - tw2 - 12, ny);
                            pango_cairo_show_layout(cr, layout);
                        }
                        pango_font_description_free(num_fnt2);
                        pango_layout_set_font_description(layout, fnt);
                    }

                }

                // Gutter right border
                if (!ed.md_viewer_open) {
                    cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, 0.18);
                    cairo_set_line_width(cr, 1.0);
                    cairo_move_to(cr, rtx - 0.5, code_top);
                    cairo_line_to(cr, rtx - 0.5, code_top + eff_height);
                    cairo_stroke(cr);
                }

                // Active pane top indicator
                if (ed.split.active_pane == 1 && !ed.md_viewer_open) {
                    cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.60);
                    cairo_rectangle(cr, rx2, code_top, rw2, 2);
                    cairo_fill(cr);
                }

                cairo_restore(cr);

                // RIGHT PANE SCROLLBAR (right edge of right pane)
                static ScrollBar right_sb;
                double r_content_h = ed.code2.total_text_height;
                double r_view_h    = (double)eff_height;
                if (r_content_h > r_view_h) {
                    // win_w parameter: pass rx2+rw2 as "window width" so
                    // scrollbar anchors to right edge of right pane
                    right_sb.draw_vertical(cr,
                        rx2 + rw2,
                        code_top + (int)eff_height,
                        ed.code2.scroll_y, r_content_h, tv,
                        ed.tabpanel_sb_margin(), code_top, {}, -1);
                }
                double r_content_w = ed.code2.total_text_width;
                double r_view_w    = (double)rw2 - ed.gutter_width;
                if (r_content_w > r_view_w) {
                    right_sb.draw_horizontal(cr,
                        (int)rtx, rx2 + rw2,
                        code_top + (int)eff_height,
                        ed.code2.scroll_x, r_content_w, tv,
                        0.0, 0.0);
                }
            }

            // LEFT PANE scrollbar - at divider left edge
            // Only draw when split is active so it doesn't overlap right pane
            {
                int div_x = (int)ed.split.divider_x(ed_x, full_ed_w);
                double l_content_h = ed.code.total_text_height;
                double l_view_h    = (double)eff_height;
                if (l_content_h > l_view_h) {
                    // Temporarily pass div_x as "win_w" to anchor scrollbar to divider
                    ed.scrollbar.draw_vertical(cr,
                        div_x,
                        code_top + (int)eff_height,
                        ed.code.scroll_y, l_content_h, tv,
                        0.0, code_top, {}, -1);
                }

                // Horizontal scrollbar for the left panel - it was completely missing
                // in split mode (the rendering call for non-split mode,
                // shown below, is skipped when split mode is enabled),
                // making it impossible to see or grab the horizontal scrollbar
                // of the first (left) panel with the mouse. (I identified and fixed this
                // after testing-after all, I couldn't just leave bugs for you, right?
                // Though I'm sure if you're reading the code, you likely want to find or fix them :))
                double l_content_w = ed.code.total_text_width;
                double l_view_w    = (double)div_x - text_area_x;
                if (l_content_w > l_view_w) {
                    ed.scrollbar.draw_horizontal(cr,
                        (int)text_area_x, div_x,
                        code_top + (int)eff_height,
                        ed.code.scroll_x, l_content_w, tv,
                        0.0, 0.0);
                }
            }

            // Draw divider on top of everything
            ed.split.draw_divider(cr, ed_x, full_ed_w,
                                   code_top, ed.height, con_h, tv);
        }

        // Sync GPU overlay results back to Editor for base.cxx to render
        if (use_gpu_overlay) {
            ed.gpu_sel_active = gpu_ov.gpu_sel_active;
            ed.gpu_sel_r      = gpu_ov.gpu_sel_r;
            ed.gpu_sel_g      = gpu_ov.gpu_sel_g;
            ed.gpu_sel_b      = gpu_ov.gpu_sel_b;
            ed.gpu_sel_rect_count = gpu_ov.gpu_sel_rect_count;
            static_assert(sizeof(ed.gpu_sel_rects[0]) == sizeof(gpu_ov.gpu_sel_rects[0]),
                          "sel rect size mismatch");
            for (int si = 0; si < gpu_ov.gpu_sel_rect_count; ++si) {
                ed.gpu_sel_rects[si].x = gpu_ov.gpu_sel_rects[si].x;
                ed.gpu_sel_rects[si].y = gpu_ov.gpu_sel_rects[si].y;
                ed.gpu_sel_rects[si].w = gpu_ov.gpu_sel_rects[si].w;
                ed.gpu_sel_rects[si].h = gpu_ov.gpu_sel_rects[si].h;
            }
        } else {
            ed.gpu_sel_active = false;
        }

        // Find-bar: extends DOWN from the tab bar, shifting the code
        // Animated via find_bar.anim: 0=hidden, 1=open.
        // code_top is already shifted to find_bar_h, so the code does not overlap.
        if (ed.find_bar.visible || ed.find_bar.anim.is_running()) {
            int fbx = ed_x;
            int fby = ed.content_top();                  // always under the tab bar
            int fbw = ed.width - ed_x;
            int fbh = find_bar_h;                    // height depends on animation
            if (fbh <= 0) goto skip_find_bar_draw;   // hasn't opened yet / has already closed

            {
            // Use only the input row height for widget placement
            int row_h = ed.find_bar.current_h();  // animated 0..H (42)
            if (row_h <= 0) goto skip_find_bar_draw;

            using FB = Editor::FindBar;
            const auto g = FB::geom(fbw, row_h);
            const bool list_open = fb_project_expanded && row_h == FB::H;
            auto text_rgba = [&](double a) { cairo_set_source_rgba(cr, tv.text, tv.text, tv.text, a); };
            auto muted_rgba = [&](double a) { cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, a); };

            // Panel background, bottom border, separator above the results
            cairo_set_source_rgb(cr, tv.panel * 1.05, tv.panel * 1.05, tv.panel * 1.05);
            cairo_rectangle(cr, fbx, fby, fbw, fbh); cairo_fill(cr);
            muted_rgba(0.30);
            cairo_rectangle(cr, fbx, fby + fbh - 1, fbw, 1); cairo_fill(cr);
            if (list_open) { muted_rgba(0.18); cairo_rectangle(cr, fbx, fby + row_h, fbw, 1); cairo_fill(cr); }

            // Search icon
            morph::draw_icon(cr, morph::Icon::Search, fbx + 20, fby + row_h / 2.0, 16.0,
                             tv.accent_r, tv.accent_g, tv.accent_b, 0.9);

            // File / All files switch: one frame, the active half is filled
            {
                PangoFontDescription *psf = pango_font_description_copy(fnt);
                pango_font_description_set_size(psf, 9 * PANGO_SCALE);
                pango_layout_set_font_description(layout, psf);
                const char *labels[2] = {"File", "All files"};
                const double mw[2] = {FB::MODE_W0, FB::MODE_W1};
                double my = fby + g.mode_y;
                for (int mi = 0; mi < 2; mi++) {
                    double mx = fbx + g.mode_x[mi];
                    bool active = (mi == ed.find_bar.mode);
                    if (active) {
                        cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.28);
                        morph::rrect(cr, mx, my, mw[mi], FB::MODE_H, shape::R); cairo_fill(cr);
                    }
                    int lw, lh;
                    pango_layout_set_text(layout, labels[mi], -1);
                    pango_layout_get_pixel_size(layout, &lw, &lh);
                    text_rgba(active ? 1.0 : 0.55);
                    cairo_move_to(cr, mx + (mw[mi] - lw) / 2.0, my + (FB::MODE_H - lh) / 2.0);
                    pango_cairo_show_layout(cr, layout);
                }
                muted_rgba(0.40);
                cairo_set_line_width(cr, 1.0);
                morph::rrect(cr, fbx + g.mode_x[0] + .5, my + .5, FB::MODE_W0 + FB::MODE_W1 - 1, FB::MODE_H - 1, shape::R);
                cairo_stroke(cr);
                pango_font_description_free(psf);
                pango_layout_set_font_description(layout, fnt);
            }

            // Input field: flat, thin neutral frame
            double fi_x = fbx + g.fi_x, fi_y = fby + g.fi_y, fi_w = g.fi_w, fi_h = g.fi_h;
            cairo_set_source_rgba(cr, tv.elevated, tv.elevated, tv.elevated, 0.9);
            morph::rrect(cr, fi_x, fi_y, fi_w, fi_h, shape::R); cairo_fill(cr);
            muted_rgba(0.45);
            cairo_set_line_width(cr, 1.0);
            morph::rrect(cr, fi_x + .5, fi_y + .5, fi_w - 1, fi_h - 1, shape::R); cairo_stroke(cr);

            // Input text + blinking cursor
            {
                PangoFontDescription *sf = pango_font_description_copy(fnt);
                pango_font_description_set_size(sf, 11 * PANGO_SCALE);
                pango_layout_set_font_description(layout, sf);
                cairo_save(cr);
                cairo_rectangle(cr, fi_x + 8, fi_y, fi_w - 16, fi_h); cairo_clip(cr);

                const std::string &q = ed.find_bar.query;
                const char *hint = ed.find_bar.mode == 1 ? "Search in all files..." : "Find in this file...";
                pango_layout_set_text(layout, q.c_str(), (int)q.size());
                int tw, th; pango_layout_get_pixel_size(layout, &tw, &th);
                double tx2 = fi_x + 8;
                if (tw > fi_w - 16) tx2 = fi_x + fi_w - 8 - tw;       // show the end of long text
                double ty = fi_y + (fi_h - th) / 2.0;

                if (q.empty()) {
                    pango_layout_set_text(layout, hint, -1);
                    int hw, hh; pango_layout_get_pixel_size(layout, &hw, &hh);
                    text_rgba(0.35);
                    cairo_move_to(cr, fi_x + 8, fi_y + (fi_h - hh) / 2.0);
                    pango_cairo_show_layout(cr, layout);
                } else {
                    text_rgba(0.9);
                    cairo_move_to(cr, tx2, ty);
                    pango_cairo_show_layout(cr, layout);
                }

                // Cursor (always visible position, blinks)
                using clk = std::chrono::steady_clock;
                static auto blink_t = clk::now();
                static bool blink_on = true;
                if (std::chrono::duration<double>(clk::now() - blink_t).count() > 0.53) {
                    blink_on = !blink_on; blink_t = clk::now();
                }
                if (blink_on) {
                    cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.9);
                    cairo_rectangle(cr, q.empty() ? fi_x + 8 : tx2 + tw + 1, fi_y + (fi_h - th) / 2.0, 1.5, th);
                    cairo_fill(cr);
                }
                cairo_restore(cr);
                pango_font_description_free(sf);
                pango_layout_set_font_description(layout, fnt);
            }

            // Match counter: its own column, cut with ".." if too long
            {
                PangoFontDescription *sf = pango_font_description_copy(fnt);
                pango_font_description_set_size(sf, 10 * PANGO_SCALE);
                pango_layout_set_font_description(layout, sf);
                std::string st = ed.find_bar.status();
                pango_layout_set_text(layout, st.c_str(), (int)st.size());
                pango_layout_set_width(layout, (int)(FB::CNT_W * PANGO_SCALE));
                pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
                int sw, sh; pango_layout_get_pixel_size(layout, &sw, &sh);
                bool no_results = !st.empty() && st[0] == 'N';
                if (no_results) cairo_set_source_rgba(cr, 0.9, 0.3, 0.3, 0.9); else text_rgba(0.55);
                cairo_move_to(cr, fbx + g.cnt_x, fby + (row_h - sh) / 2.0);
                pango_cairo_show_layout(cr, layout);
                pango_layout_set_width(layout, -1);
                pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_NONE);
                pango_font_description_free(sf);
                pango_layout_set_font_description(layout, fnt);
            }

            // Previous / next match arrows (File mode only)
            if (ed.find_bar.mode == 0) {
                for (int ni = 0; ni < 2; ni++) {
                    double bx = fbx + g.nav_x[ni], by = fby + g.nav_y;
                    // SVG arrow button: up = previous match, down = next match
                    svg_icons::draw_ui(cr, ni == 0 ? "arrow_up" : "arrow_down", bx, by, FB::BTN,
                                       tv.text, tv.text, tv.text, 0.65);
                }
            }

            // Close button
            {
                double cx2 = fbx + g.close_x, cy2 = fby + g.close_y;
                muted_rgba(0.18);
                morph::rrect(cr, cx2, cy2, FB::BTN, FB::BTN, shape::R); cairo_fill(cr);
                svg_icons::draw_ui(cr, "cross", cx2 + 7, cy2 + 7, 10, tv.text, tv.text, tv.text, 0.65);
            }

            // Results list (All files mode)
            if (list_open) {
                bool query_empty = ed.find_bar.query.empty();
                double list_y = fby + row_h + 4.0;
                double list_h = FIND_RESULTS_H - 8.0;
                const double SB_W = 6.0;
                double rx = (double)fbx + 8.0;
                double rw = (double)fbw - 16.0 - SB_W - 4.0;
                const int ITEM_H = FB::ITEM_H;
                const auto &results = ed.find_bar.global_results;
                PangoFontDescription *pf  = pango_font_description_copy(fnt);   // file name
                pango_font_description_set_size(pf, 10 * PANGO_SCALE);
                PangoFontDescription *sf2 = pango_font_description_copy(fnt);  // folder, line text
                pango_font_description_set_size(sf2, 9 * PANGO_SCALE);

                double total_res_h = results.size() * (double)ITEM_H;
                auto &scroll = ed.find_bar.global_scroll;
                scroll = std::clamp(scroll, 0.0, std::max(0.0, total_res_h - list_h));
                // Keep the selected result visible
                if (ed.find_bar.global_idx >= 0) {
                    double sel_top = ed.find_bar.global_idx * (double)ITEM_H, sel_bot = sel_top + ITEM_H;
                    if (sel_top < scroll)               scroll = sel_top;
                    else if (sel_bot > scroll + list_h) scroll = sel_bot - list_h;
                }
                double sc = scroll;

                cairo_save(cr);
                cairo_rectangle(cr, rx, list_y, rw, list_h); cairo_clip(cr);
                if (results.empty()) {
                    pango_layout_set_font_description(layout, sf2);
                    pango_layout_set_text(layout, query_empty ? "Type to search in all files..." : "No matches found", -1);
                    int lw2, lh2; pango_layout_get_pixel_size(layout, &lw2, &lh2);
                    muted_rgba(0.55);
                    cairo_move_to(cr, rx + (rw - lw2) / 2.0, list_y + (list_h - lh2) / 2.0);
                    pango_cairo_show_layout(cr, layout);
                } else {
                    for (int ri = 0; ri < (int)results.size(); ++ri) {
                        double ry = list_y + ri * (double)ITEM_H - sc;
                        if (ry + ITEM_H < list_y) continue;
                        if (ry > list_y + list_h) break;
                        const auto &res = results[ri];
                        bool sel = (ri == ed.find_bar.global_idx);
                        const double TX = rx + 14.0;                 // left edge of the text

                        // Row background (a gap of 2px above and below separates the rows)
                        if (sel) {
                            cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.16);
                            morph::rrect(cr, rx, ry + 2, rw, ITEM_H - 4, shape::R); cairo_fill(cr);
                            cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.9);
                            cairo_rectangle(cr, rx, ry + 2, 2, ITEM_H - 4); cairo_fill(cr);
                        }

                        // --- Line 1: file name, folder, ":line" on the right ---
                        pango_layout_set_attributes(layout, nullptr);
                        pango_layout_set_font_description(layout, sf2);
                        std::string lns = ":" + std::to_string(res.line_no);
                        pango_layout_set_text(layout, lns.c_str(), -1);
                        pango_layout_set_width(layout, -1);
                        pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_NONE);
                        int lnw, lnh; pango_layout_get_pixel_size(layout, &lnw, &lnh);
                        cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.85);
                        double line1_y = ry + 10;
                        cairo_move_to(cr, rx + rw - lnw - 10, line1_y + 1); pango_cairo_show_layout(cr, layout);

                        double avail   = rw - 14.0 - lnw - 24.0;
                        double name_w  = avail * 0.55;
                        pango_layout_set_font_description(layout, pf);
                        pango_layout_set_text(layout, fs::path(res.path).filename().string().c_str(), -1);
                        pango_layout_set_width(layout, (int)(name_w * PANGO_SCALE));
                        pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_MIDDLE);
                        int nw, nh; pango_layout_get_pixel_size(layout, &nw, &nh);
                        text_rgba(sel ? 1.0 : 0.88);
                        cairo_move_to(cr, TX, line1_y); pango_cairo_show_layout(cr, layout);

                        if (!res.rel_dir.empty()) {
                            double dir_x = TX + nw + 10.0, dir_w = avail - nw - 10.0;
                            if (dir_w > 30.0) {
                                pango_layout_set_font_description(layout, sf2);
                                pango_layout_set_text(layout, res.rel_dir.c_str(), -1);
                                pango_layout_set_width(layout, (int)(dir_w * PANGO_SCALE));
                                pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_START);
                                muted_rgba(0.65);
                                cairo_move_to(cr, dir_x, line1_y + 1); pango_cairo_show_layout(cr, layout);
                            }
                        }

                        // --- Line 2: the matching line, the query highlighted ---
                        pango_layout_set_font_description(layout, sf2);
                        pango_layout_set_text(layout, res.line_text.c_str(), -1);
                        pango_layout_set_width(layout, (int)((rw - 24.0) * PANGO_SCALE));
                        pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
                        PangoAttrList *al = nullptr;
                        if (res.hl_start >= 0 && res.hl_len > 0) {
                            al = pango_attr_list_new();
                            PangoAttribute *fg = pango_attr_foreground_new(
                                (guint16)(tv.accent_r * 65535), (guint16)(tv.accent_g * 65535), (guint16)(tv.accent_b * 65535));
                            PangoAttribute *bd = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
                            fg->start_index = bd->start_index = (guint)res.hl_start;
                            fg->end_index   = bd->end_index   = (guint)(res.hl_start + res.hl_len);
                            pango_attr_list_insert(al, fg); pango_attr_list_insert(al, bd);
                            pango_layout_set_attributes(layout, al);
                        }
                        text_rgba(sel ? 0.85 : 0.65);
                        cairo_move_to(cr, TX, line1_y + std::max(nh, lnh) + 8);
                        pango_cairo_show_layout(cr, layout);
                        pango_layout_set_attributes(layout, nullptr);
                        if (al) pango_attr_list_unref(al);
                    }
                }
                cairo_restore(cr);
                pango_layout_set_width(layout, -1);
                pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_NONE);
                pango_font_description_free(pf);
                pango_font_description_free(sf2);
                pango_layout_set_font_description(layout, fnt);

                // Scrollbar of the results list
                if (total_res_h > list_h) {
                    double sb_x = rx + rw + 4.0;
                    double ratio = std::clamp(list_h / total_res_h, 0.05, 1.0);
                    double thumb_h = std::max(20.0, list_h * ratio);
                    double thumb_y = list_y + (sc / std::max(1.0, total_res_h - list_h)) * (list_h - thumb_h);
                    muted_rgba(0.12);
                    morph::rrect(cr, sb_x, list_y, SB_W, list_h, shape::R); cairo_fill(cr);
                    cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.6);
                    morph::rrect(cr, sb_x, thumb_y, SB_W, thumb_h, shape::R); cairo_fill(cr);
                }
            }

            // Highlight all matches in code area
            if (!ed.find_bar.matches.empty() && !ed.find_bar.query.empty()) {
                int qlen = (int)ed.find_bar.query.size();
                // We need line metrics - use cached_line_h
                double lh = ed.code.cached_line_h > 1.0 ? ed.code.cached_line_h : 18.0;
                // Clip highlights to the region BELOW the find bar (avoid
                // drawing under the solid find-bar panel).
                // clip_top uses actual animated find_bar_h so highlights
                // never bleed under the find bar or tab bar.
                double clip_top = (double)(ed.content_top() + find_bar_h);
                double clip_bot = (double)(ed.content_top()) + eff_height;

                cairo_save(cr);
                cairo_rectangle(cr, text_area_x, clip_top, ed.width - text_area_x, clip_bot - clip_top);
                cairo_clip(cr);

                for (int mi = 0; mi < (int)ed.find_bar.matches.size(); ++mi) {
                    int off = ed.find_bar.matches[mi];
                    int ln = 0, col = 0;
                    ed.code.index_to_line_col(off, ln, col);
                    // code_top already includes find_bar_h, use it directly
                    double ry2 = (double)code_top + 15.0 + (double)ln * lh - ed.code.scroll_y;
                    if (ry2 + lh < clip_top || ry2 > clip_bot) continue;

                    // Measure x position of match start/end using Pango
                    pango_layout_set_attributes(layout, nullptr);
                    const std::string &ln_str = ed.code.lines[ln];
                    pango_layout_set_text(layout, ln_str.c_str(), (int)ln_str.size());
                    PangoRectangle ps, pe;
                    int col_end = std::min(col + qlen, (int)ln_str.size());
                    pango_layout_get_cursor_pos(layout, col,     &ps, nullptr);
                    pango_layout_get_cursor_pos(layout, col_end, &pe, nullptr);
                    double x1 = (double)ps.x / PANGO_SCALE + text_area_x + 15.0 - ed.code.scroll_x;
                    double x2 = (double)pe.x / PANGO_SCALE + text_area_x + 15.0 - ed.code.scroll_x;

                    bool is_current = (mi == ed.find_bar.match_idx);
                    if (is_current) {
                        // Current match: bright accent fill
                        cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.45);
                    } else {
                        // Other matches: subtle fill
                        cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.15);
                    }
                    double mw = std::max(4.0, x2 - x1);
                    double mr = g_rounded_corners ? 3.0 : 0.0;
                    morph::rrect(cr, x1, ry2 + 2, mw, lh - 4, mr);
                    cairo_fill(cr);
                }
                cairo_restore(cr);
                pango_layout_set_attributes(layout, nullptr);
                pango_layout_set_font_description(layout, fnt);
            }
            } // end inner block (opened after goto check)
        } // end if find_bar.visible || anim.is_running()

        skip_find_bar_draw:;   // goto target: skip drawing if height = 0
        // (needed ; after the label because labels cannot appear before the declaration)

        // Scrollbars
        // top_scroll = code_top: the scrollbar starts exactly where the code begins,
        // that is, below the find bar (if it is open).
        double content_h = ed.code.total_text_height + 30.0;
        double content_w = ed.code.total_text_width  + 30.0;
        // Effective height for scrollbar - without find bar
        double eff_h_scroll = eff_height - find_bar_h;

        bool   need_v        = content_h > eff_h_scroll;
        double view_w        = std::max(1.0, (double)ed.width - text_area_x - 20.0);
        bool   need_h        = content_w > view_w;
        double right_reserved = need_v ? 20.0 : 0.0;

        // Tick scrollbar hover animation (expand-on-hover)
        {
            double v_x, v_y, v_w, v_h;
            ed.scrollbar.vertical_track_rect(ed.width, (int)(code_top + eff_height),
                                              ed.tabpanel_sb_margin(), v_x, v_y, v_w, v_h, code_top);
            bool hov_v = (ed.dragging_v ||
                          (ed.code.total_text_height > eff_height - find_bar_h));
            // actual hover detection - use mouse pos from last mouse_move
            // We approximate: dragging_v forces hover on
            bool hov_h = ed.dragging_h;
            ScrollBar::tick_hover(hov_v && ed.dragging_v, hov_h);
            // Press: mouse button + cursor on scrollbar track pressed
            ScrollBar::tick_press(ed.dragging_v, ed.dragging_h);
        }

        // Match markers on the scrollbar (find bar or current word selection)
        // Pass to draw_vertical as a vector of normalized positions [0..1]
        // In split mode: left pane scrollbar drawn at divider edge (see split block above)
        // Main scrollbar only drawn when NOT split
        if (!ed.split.enabled) {
        if (need_v) {
            std::vector<double> markers;
            if (ed.find_bar.visible && !ed.find_bar.matches.empty() &&
                !ed.find_bar.query.empty())
            {
                double total_h = std::max(1.0, ed.code.total_text_height);
                double lh = ed.code.cached_line_h > 1.0 ? ed.code.cached_line_h : 18.0;
                for (int mi = 0; mi < (int)ed.find_bar.matches.size(); ++mi) {
                    int off = ed.find_bar.matches[mi];
                    int ln = 0, col = 0;
                    ed.code.index_to_line_col(off, ln, col);
                    double pos = ((double)ln * lh) / total_h;
                    markers.push_back(std::clamp(pos, 0.0, 1.0));
                }
            }
            else if (ed.code.has_selection()) {
                std::string sel = ed.code.get_selection_text();
                if (!sel.empty() && sel.size() < 200 &&
                    sel.find('\n') == std::string::npos)
                {
                    const std::string &buf_str = ed.code.get_buffer_str();
                    std::string lo_buf = buf_str, lo_sel = sel;
                    for (auto &c : lo_buf) c = (char)std::tolower((unsigned char)c);
                    for (auto &c : lo_sel) c = (char)std::tolower((unsigned char)c);
                    double total_h = std::max(1.0, ed.code.total_text_height);
                    double lh = ed.code.cached_line_h > 1.0 ? ed.code.cached_line_h : 18.0;
                    size_t pos = 0;
                    while ((pos = lo_buf.find(lo_sel, pos)) != std::string::npos) {
                        int ln = 0, col = 0;
                        ed.code.index_to_line_col((int)pos, ln, col);
                        double npos = ((double)ln * lh) / total_h;
                        markers.push_back(std::clamp(npos, 0.0, 1.0));
                        pos += lo_sel.size();
                    }
                }
            }
            ed.scrollbar.draw_vertical(cr, ed.width,
                                       code_top + (int)eff_h_scroll,
                                       ed.code.scroll_y, content_h, tv,
                                       ed.tabpanel_sb_margin(), code_top,
                                       markers,
                                       ed.find_bar.visible ? ed.find_bar.match_idx : -1);
        }
        if (need_h) ed.scrollbar.draw_horizontal(cr, (int)text_area_x, ed.width,
                                                  code_top + (int)eff_h_scroll,
                                                  ed.code.scroll_x, content_w, tv, 0.0, right_reserved);
        } // end !split
    }

    // Save button removed - dirty indicator shown only in tab title (dot)

}

#endif
