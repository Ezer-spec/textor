#ifndef EDITOR_INPUT_HXX
#define EDITOR_INPUT_HXX
// editor_input.hxx - editor method definitions for handling input
// (mouse clicks, mouse movement/dragging, mouse wheel scrolling, keyboard input).
// This code was extracted from the original editor.hxx file (which was about 3,900 lines long)
// to improve readability: these four methods alone accounted for approximately 1,400 lines.
// For a description of the operational logic (click processing order, drag states,
// keyboard focus areas, key combinations), please refer to docs/INPUT.md.
//
// This file is included at the end of editor.hxx, following the definition of the Editor
// structure (within which these methods remain declared as standard prototypes).
// It follows the same approach used in typical .cxx files; however,
// much like the rest of the project, the code remains entirely header-only-because I want it that way, and what are you going to do about it?! :)
// Do not include this file directly; it requires the Editor structure
// to be fully declared beforehand.

void Editor::handle_mouse_click(int x, int y, bool shift_held, bool ctrl_held) {

        // -- First-run onboarding - highest priority, blocks everything else
        // while shown
        if (onboarding.visible) {
            if (onboarding.handle_mouse_down(x, y)) finish_onboarding();
            return;
        }

        // ............ Find bar click handling ............
        if (find_bar.visible) {
            const int fbx = activity_bar_w + current_sidebar_px();
            const int fby = content_top();
            const int row_h = find_bar.current_h();            // input row height (animated)
            const bool list_open = (find_bar.mode == 1 && row_h == FindBar::H);
            const int fbh_total = list_open ? row_h + FindBar::RESULTS_H : row_h;
            const auto g = FindBar::geom(width - fbx, row_h);
            auto inside = [&](double bx, double by, double bw, double bh) {
                return x >= fbx + bx && x < fbx + bx + bw && y >= fby + by && y < fby + by + bh;
            };

            if (inside(g.close_x, g.close_y, FindBar::BTN, FindBar::BTN)) { close_find_bar(); return; }

            // File / All files switch
            if (inside(g.mode_x[0], g.mode_y, FindBar::MODE_W0, FindBar::MODE_H) ||
                inside(g.mode_x[1], g.mode_y, FindBar::MODE_W1, FindBar::MODE_H)) {
                int new_mode = inside(g.mode_x[0], g.mode_y, FindBar::MODE_W0, FindBar::MODE_H) ? 0 : 1;
                if (new_mode != find_bar.mode) { find_bar.mode = new_mode; run_find_search(); }
                return;
            }

            // Previous / next match (current file only)
            if (find_bar.mode == 0) {
                if (inside(g.nav_x[0], g.nav_y, FindBar::BTN, FindBar::BTN)) { find_bar.prev(); jump_to_find_match(); return; }
                if (inside(g.nav_x[1], g.nav_y, FindBar::BTN, FindBar::BTN)) { find_bar.next(); jump_to_find_match(); return; }
            }

            // Click on a result - open the file at that line
            if (list_open) {
                double list_y = fby + row_h + 4.0;
                if (y >= (int)list_y && y < fby + fbh_total) {
                    int ri = (int)((y - list_y + find_bar.global_scroll) / FindBar::ITEM_H);
                    if (ri >= 0 && ri < (int)find_bar.global_results.size()) open_global_result(ri);
                    return;
                }
            }

            // Consume all clicks inside the bar area
            if (y >= fby && y < fby + fbh_total) return;
        }

        if (modal.visible) {
            int prev = modal.selected_theme_idx;
            bool handled = modal.handle_mouse_down(x, y, width, height);
            if (handled) {
                if (modal.selected_theme_idx != prev)
                    start_theme_transition(modal.get_selected_theme());
                apply_settings_from_modal();
                save_current_settings();
                return;
            }
            bool click_handled = modal.handle_click(x, y, width, height);
            if (click_handled) {
                if (modal.selected_theme_idx != prev)
                    start_theme_transition(modal.get_selected_theme());
                apply_settings_from_modal();
                save_current_settings();
                return;
            }
        }

        // Save button removed (use Ctrl+S)
        if (false && code.dirty && save_button_hit(x, y)) {
            save_current_file();
            return;
        }

        // ............ Welcome screen buttons (when no tabs open) ............
        if (tabs.empty() && !file_picker.visible && !settings_open) {
            if (welcome_btn_folder.hit(x, y)) {
                file_picker.open(true, sidebar.current_path);
                return;
            }
            if (welcome_btn_file.hit(x, y)) {
                file_picker.open(false, sidebar.current_path);
                return;
            }
        }

        // ............ File picker click ............
        if (palette.visible) { palette.handle_click(x, y, width, height); return; }

        if (file_picker.visible) {
            file_picker.handle_click(x, y, width, height);
            if (file_picker.confirmed) {
                // Snapshot and immediately clear flag to prevent double-trigger
                std::string rpath = file_picker.result_path;
                bool is_folder    = file_picker.mode_folder;
                file_picker.confirmed = false;   // clear before any side-effects

                if (is_folder) {
                    try {
                        fs::path p(rpath);
                        sidebar.navigate(p);
                        if (!sidebar.visible) {
                            sidebar.visible = true;
                            sidebar_anim.start(true, 0.28);
                        }
                    } catch (...) {}
                } else {
                    std::ifstream ifs(rpath);
                    if (ifs) {
                        std::string buf_content((std::istreambuf_iterator<char>(ifs)),
                                                 std::istreambuf_iterator<char>());
                        open_file_in_tab(rpath, buf_content);
                    }
                }
            }
            return;
        }

        // -- Top bar click (single merged bar: Explorer toggle - Split/
        // Addons/Console + language, centered - Settings/Exit)
        if (y >= 0 && y < TAB_BAR_H - (int)chrome_shift() && x >= activity_bar_w) {
            if (topbar_pill_hit(btn_sidebar.x, sidebar_pill_w, x, y)) {
                toggle_explorer();
                return;
            }
            if (btn_settings.is_hit(x, y)) {
                icon_anim::trigger(&btn_settings);
                if (settings_open) close_settings(); else open_settings_tab();
                return;
            }
            if (btn_exit.is_hit(x, y)) {
                icon_anim::trigger(&btn_exit);
                request_quit = true; return;
            }
            if (lang_pill_hit(btn_split.x, lang_pill_split_w, x, y)) {
                icon_anim::trigger(&btn_split);
                toggle_split();
                return;
            }
            if (lang_pill_hit(btn_addons.x, lang_pill_addons_w, x, y)) {
                toggle_addons_panel();
                return;
            }
            if (lang_pill_hit(btn_console.x, lang_pill_console_w, x, y)) {
                icon_anim::trigger(&btn_console);
                console.visible = !console.visible;
                console_anim.start(console.visible, 0.26);
                btn_console.active  = console.visible;
                console_focused     = console.visible;
                return;
            }
            if (lang_pill_viewers_w > 0 && active_is_markdown() &&
                lang_pill_hit(btn_viewers.x, lang_pill_viewers_w, x, y)) {
                icon_anim::trigger(&btn_viewers);
                toggle_md_viewer();
                return;
            }
            return;
        }

        // ............ Tab strip click ............
        {
            bool close_hit = false;
            int tidx = tabstrip_hit_test((double)x, (double)y, close_hit);
            if (tidx >= 0) {
                if (close_hit) {
                    close_tab(tidx);
                } else {
                    if (active_tab != tidx) switch_to_tab(tidx);
                    ui_focus = UIFocus::EditorText;
                    tabpanel_kbd_idx = tidx;
                }
                return;
            }
        }


        if (modal.visible) return;

        int side_end = activity_bar_w + (sidebar.visible ? sidebar.width : 0);
        int global_top = content_top();
        int sy = y - global_top; // sidebar's own coordinate space starts below the global bars

        // ............ Sidebar file list ............
        if (sidebar.visible && x >= activity_bar_w && x < side_end && y >= global_top) {
            // New File button (shrinks to icon when search is expanded, but
            // stays clickable either way)
            if (sidebar.new_file_btn_hit(x, sy, activity_bar_w)) {
                sidebar.begin_new_file();
                return;
            }

            // Search icon - toggles expand/collapse. Clicking elsewhere in
            // an already-expanded input just focuses it for typing.
            if (sidebar.search_icon_glyph_hit(x, sy, activity_bar_w)) {
                sidebar.search_expanded = !sidebar.search_expanded;
                sidebar.search_active   = sidebar.search_expanded;
                if (!sidebar.search_expanded) { sidebar.filter.clear(); sidebar.apply_filter(); }
                return;
            }

            // Mode pill click - cycle through Name/Text/Regex
            if (sidebar.search_mode_pill_hit(x, sy, activity_bar_w)) {
                sidebar.search_mode = (Sidebar::SearchMode)(((int)sidebar.search_mode + 1) % 3);
                sidebar.global_match_paths.clear();
                if (!sidebar.filter.empty() && sidebar.search_mode != Sidebar::SEARCH_FILENAME)
                    sidebar.do_global_search();
                return;
            }

            // Multi-select row click in delete mode
            if (sidebar.action_mode == Sidebar::ACT_DELETE) {
                int idx = sidebar.hit_test_row(x, sy, activity_bar_w);
                if (idx >= 0) { sidebar.toggle_multi_sel(idx); return; }
            }

            if (sidebar.search_region_hit(x, sy, activity_bar_w)) {
                if (sidebar.search_expanded) sidebar.search_active = true;
                return;
            }
            sidebar.search_active = false;

            int idx = sidebar.hit_test_row(x, sy, activity_bar_w);
            if (idx >= 0 && idx < (int)sidebar.flat.size()) {
                sidebar.selected_idx = idx;
                ui_focus = UIFocus::Sidebar;
                auto *node = sidebar.flat[idx];
                if (node->is_dir) {
                    // Toggle expand/collapse in tree - no navigation
                    sidebar.toggle_expand(idx);
                } else {
                    fs::path target = node->path;
                    std::ifstream ifs(target);
                    if (ifs) {
                        std::string buf_content((std::istreambuf_iterator<char>(ifs)),
                                                 std::istreambuf_iterator<char>());
                        open_file_in_tab(target.string(), buf_content);
                    }
                }
            }
            return;
        }

        // ............ Status bar click ............
        if (status_bar_click(x, y)) return;

        // ............ Console panel click ............
        double con_h = console_h_now();
        if (console.visible && con_h - STATUS_H > 4.0 && y >= (int)(height - con_h) && y < height - STATUS_H) {
            int lx  = activity_bar_w;
            int cy2 = (int)(height - con_h);
            int cw2 = width - lx;
            if (console.handle_mouse_click(x, y, lx, cy2, cw2, (int)(con_h - STATUS_H))) {
                console_focused = true;
                ui_focus = UIFocus::Console;
            }
            return;
        }

        // Clicking editor deselects console focus
        console_focused = false;

        // ............ Settings tab click routing ............
        if (is_settings_tab_active()) {
            int ed_x = activity_bar_w + current_sidebar_px();
            int ax = ed_x, ay = content_top();
            int aw = width - ax, ah = height - (int)con_h - content_top();
            ThemeVariant tv2 = current_tv();
            bool changed = settings_tab.handle_mouse_down(x, y, ax, ay, aw, ah, tv2);
            if (changed) { apply_settings_from_modal(); save_current_settings(); }
            return;
        }

        // ............ Scrollbar hit detection ............
        int    ed_x       = activity_bar_w + current_sidebar_px();
        double tx         = (double)(ed_x + gutter_width);
        double eff_height = height - con_h - content_top();

        // In split mode: left pane right edge = divider_x
        int left_right_edge = width;
        if (split.enabled && split.enable_anim > 0.1) {
            int full_ed_w = width - ed_x;
            left_right_edge = (int)split.divider_x(ed_x, full_ed_w);
        }

        double content_h = code.total_text_height + 30.0;
        double content_w = code.total_text_width  + 30.0;
        bool   need_v    = content_h > eff_height;
        bool   need_h    = content_w > std::max(1.0, (double)left_right_edge - tx - 20.0);

        double v_x, v_y, v_w, v_h;
        scrollbar.vertical_track_rect(left_right_edge, (int)(eff_height + content_top()),
                                       tabpanel_sb_margin(), v_x, v_y, v_w, v_h, content_top());

        if (need_v && x >= (int)v_x && x <= (int)(v_x+v_w) && x < left_right_edge) {
            dragging_v          = true;
            drag_start_mouse    = y;
            drag_start_val      = code.scroll_y;
            code.auto_scroll_to_cursor = false;
            // Grabbing the scrollbar takes priority over the tab panel -
            // collapse it and nudge it further left out of the way so a
            // tall thumb/track and the panel never fight over the same
            // strip of screen.
            sb_mark_active();
            return;
        }

        double right_reserved = need_v ? 20.0 : 0.0;
        double h_x, h_y, h_w, h_h;
        scrollbar.horizontal_track_rect((int)tx, left_right_edge,
                                         (int)(eff_height + content_top()),
                                         right_reserved, 0.0,
                                         h_x, h_y, h_w, h_h);

        if (need_h && y >= (int)h_y && y <= (int)(h_y+h_h) &&
            x >= (int)h_x && x <= (int)(h_x+h_w))
        {
            dragging_h          = true;
            drag_start_mouse    = x;
            drag_start_val      = code.scroll_x;
            code.auto_scroll_to_cursor = false;
            return;
        }

        // ............ Right pane scrollbar hit detection ............
        if (split.enabled) {
            int full_ed_w = width - ed_x;
            int rx2, rw2;
            split.right_rect(ed_x, full_ed_w, rx2, rw2);
            int r_right = rx2 + rw2;

            double r_content_h = code2.total_text_height + 30.0;
            double r_view_h    = eff_height;
            bool   r_need_v    = r_content_h > r_view_h;

            if (r_need_v) {
                double rv_x, rv_y, rv_w, rv_h;
                scrollbar.vertical_track_rect(r_right, (int)(eff_height + content_top()),
                                              tabpanel_sb_margin(), rv_x, rv_y, rv_w, rv_h, content_top());
                // Only hit right pane scrollbar if click is inside right pane
                if (x >= rx2 && x >= (int)rv_x && x <= (int)(rv_x + rv_w)) {
                    dragging_v2         = true;
                    drag_start_mouse    = y;
                    drag_start_val      = code2.scroll_y;
                    code2.auto_scroll_to_cursor = false;
                    sb_mark_active();
                            return;
                }
            }
        }

        // ............ Split divider hit detection ............
        if (split.enabled && y >= content_top()) {
            int full_ed_w = width - ed_x;
            if (split.divider_hit(x, y, ed_x, full_ed_w,
                                   content_top(), height, console_h_now())) {
                // Double-click the divider resets it to a perfect 50/50 split
                using clk = std::chrono::steady_clock;
                static auto last_t = clk::now() - std::chrono::seconds(5);
                static int  last_x = -999, last_y = -999;
                auto now = clk::now();
                double dt = std::chrono::duration<double>(now - last_t).count();
                int dxp = x - last_x, dyp = y - last_y;
                bool is_dbl = (dt < 0.40 && dxp*dxp + dyp*dyp < 16*16);
                last_t = now; last_x = x; last_y = y;
                if (is_dbl) { split.reset_ratio(); return; }
                split.dragging = true;
                return;
            }
            // Switch active pane only on text-area clicks (not tab bar / UI buttons)
            if (x >= ed_x) {
                int pane = split.pane_hit(x, ed_x, full_ed_w);
                if (pane == 0) { split.active_pane = 0; split_right_pane_active = false; }
                else if (pane == 1) { split.active_pane = 1; split_right_pane_active = true; }
            }
        }

        // ............ Text area click ............
        {
            // find_bar occupies the top of the editor area - offset text_area_y accordingly
            int fb_h_now = 0;
            if (find_bar.visible) {
                const int FIND_RESULTS_H = FindBar::RESULTS_H;
                int base_h = find_bar.current_h();
                fb_h_now = (find_bar.mode == 1 && base_h == FindBar::H)
                           ? base_h + FIND_RESULTS_H : base_h;
            }
            int text_top = content_top() + fb_h_now;
            if (x >= (int)tx && x < width && y >= text_top) {
                TmpLayout tmp(current_tv());
                ui_focus = UIFocus::EditorText;  // clicking the code area returns keyboard focus to it
                if (split.enabled && split_right_pane_active) {
                    int full_ed_w = width - ed_x;
                    int rx2, rw2;
                    split.right_rect(ed_x, full_ed_w, rx2, rw2);
                    double rtx = rx2 + gutter_width;
                    code2.handle_mouse_click(x, y, (int)rtx, text_top, gutter_width,
                                              tmp.layout, tmp.fnt, shift_held, ctrl_held);
                } else {
                    code.handle_mouse_click(x, y, (int)tx, text_top, gutter_width,
                                             tmp.layout, tmp.fnt, shift_held, ctrl_held);
                }
                // Ctrl+click only adds a cursor: dragging after it must not select text
                text_drag_active = !(ctrl_held && !shift_held);
            }
        }
    }

void Editor::handle_scroll(int delta, int x, int y, bool shift) {
        if (onboarding.visible)  { onboarding.handle_scroll(delta);  return; }
        if (palette.visible) return;
        if (file_picker.visible) { file_picker.handle_scroll(delta); return; }
        if (modal.visible)       { modal.handle_scroll(delta);       return; }

        // -- Tab strip scroll: wheel over the strip moves the tabs sideways --
        if (!tabs.empty() && in_tab_strip(y) && x >= tabstrip_x0()) {
            tabstrip_scroll_x = std::clamp(tabstrip_scroll_x + delta, 0.0, tabstrip_scroll_max());
            return;
        }


        if (find_bar.visible && find_bar.mode == 1) {
            const int FIND_RESULTS_H = FindBar::RESULTS_H;
            const int ITEM_H = FindBar::ITEM_H;
            int fby = content_top();
            int row_h = find_bar.H;
            double list_y = fby + row_h + 4.0;
            double list_h = FIND_RESULTS_H - 8.0;
            if (y >= (int)list_y && y < (int)(list_y + list_h)) {
                double total_res = find_bar.global_results.size() * ITEM_H;
                find_bar.global_scroll = std::clamp(
                    find_bar.global_scroll + delta,
                    0.0, std::max(0.0, total_res - list_h));
                return;
            }
        }

        if (is_settings_tab_active()) {
            // Same area height as the drawing code: without the status bar and the terminal
            settings_tab.handle_scroll(delta, x, y, content_top(), height - (int)console_h_now() - content_top());
            return;
        }

        int side_end = activity_bar_w + (sidebar.visible ? sidebar.width : 0);
        if (sidebar.visible && x >= activity_bar_w && x < side_end) {
            double ms = sidebar.max_scroll(height - content_top());   // same height as the drawn panel
            sidebar.scroll_y = std::clamp(sidebar.scroll_y + delta, 0.0, ms);
            return;
        }

        if (y >= 0 && y < TAB_BAR_H - (int)chrome_shift()) {
            // Top bar no longer scrolls (tabs live in the floating panel now).
            return;
        }

        double con_h = console_h_now();
        if (console.visible && con_h - STATUS_H > 4.0 && y >= (int)(height - con_h) && y < height - STATUS_H) {
            int lx  = activity_bar_w;
            int cy2 = (int)(height - con_h);
            int cw2 = width - lx;
            console.handle_scroll_event(delta, x, y, lx, cy2, cw2, (int)(con_h - STATUS_H));
            return;
        }

        bool scroll_right_pane = false;
        if (split.enabled) {
            int ed_x2 = activity_bar_w + current_sidebar_px();
            double div_x = split.divider_x(ed_x2, width - ed_x2);
            scroll_right_pane = (x > (int)div_x);
        }

        if (scroll_right_pane) {
            double con_h2 = console_h_now();
            double eff_h2 = (double)height - con_h2 - content_top();
            if (shift) {
                int ed_x2 = activity_bar_w + current_sidebar_px();
                int rx2, rw2;
                split.right_rect(ed_x2, width - ed_x2, rx2, rw2);
                double view_w = std::max(1.0, (double)rw2 - gutter_width - 20.0);
                double max_sx = std::max(0.0, code2.total_text_width - view_w + 100.0);
                code2.scroll_vx += (double)delta * 0.9;
                code2.scroll_x = std::clamp(code2.scroll_x, 0.0, max_sx);
            } else {
                double max_sy = std::max(0.0, code2.total_text_height - eff_h2 + 100.0);
                code2.scroll_vy += (double)delta * 0.9;
                code2.scroll_y = std::clamp(code2.scroll_y, 0.0, max_sy);
            }
        } else {
            if (shift) {
                double tx = (double)text_area_x_now();
                // In split mode the left pane's visible width stops at the
                // divider, not the full window - using the full window
                // width here inflated view_w, which in turn clamped
                // scroll_x back toward 0 almost immediately. This was why
                // horizontal scroll looked like it did nothing on the
                // left/first pane whenever split was on.
                double right_edge = (double)width;
                if (split.enabled) {
                    int ed_x2 = activity_bar_w + current_sidebar_px();
                    right_edge = split.divider_x(ed_x2, width - ed_x2);
                }
                double view_w = std::max(1.0, right_edge - tx - 20.0);
                double max_sx = std::max(0.0, code.total_text_width - view_w + 100.0);
                code.scroll_vx += (double)delta * 0.9;
                code.scroll_x = std::clamp(code.scroll_x, 0.0, max_sx);
            } else {
                double eff_h  = (double)height - con_h - content_top();
                double max_sy = std::max(0.0, code.total_text_height - eff_h + 100.0);
                code.scroll_vy += (double)delta * 0.9;
                code.scroll_y = std::clamp(code.scroll_y, 0.0, max_sy);
            }
        }
    }

void Editor::handle_mouse_move(int x, int y) {
        if (onboarding.visible) { onboarding.handle_mouse_move(x, y); return; }
        if (modal.visible) {
            bool handled = modal.handle_mouse_move(x, y, width, height);
            if (handled) { apply_settings_from_modal(); return; }
            modal.handle_hover(x, y, width, height);
            return;
        }

        // Update positions of dynamic buttons first so is_hit() is valid
        position_topbar_buttons();

        btn_sidebar.hovered  = topbar_pill_hit(btn_sidebar.x, sidebar_pill_w, x, y);
        btn_addons.hovered   = lang_pill_hit(btn_addons.x, lang_pill_addons_w, x, y);
        btn_split.hovered    = lang_pill_hit(btn_split.x, lang_pill_split_w, x, y);
        btn_exit.hovered     = btn_exit.is_hit(x, y);
        btn_settings.hovered = btn_settings.is_hit(x, y);
        btn_console.hovered  = lang_pill_hit(btn_console.x, lang_pill_console_w, x, y);
        btn_viewers.hovered  = lang_pill_viewers_w > 0 && active_is_markdown() &&
                               lang_pill_hit(btn_viewers.x, lang_pill_viewers_w, x, y);

        // Split divider hover
        {
            int ed_x2 = activity_bar_w + current_sidebar_px();
            int full_ed_w = width - ed_x2;
            split.hovered = split.divider_hit(x, y, ed_x2, full_ed_w,
                                               content_top(), height, console_h_now());
        }

        // Welcome screen hover
        if (tabs.empty()) {
            welcome_btn_hovered = welcome_btn_folder.hit(x,y) ? 0 :
                                  welcome_btn_file.hit(x,y)   ? 1 : -1;
        }

        // File picker hover
        if (palette.visible) { palette.handle_move(x, y, width, height); return; }
        if (file_picker.visible) {
            file_picker.handle_move(x, y, width, height);
        }

        double bx = width - save_button.w - 16, by = 12;
        save_button.hovered = (x >= (int)bx && x <= (int)(bx+save_button.w) &&
                                y >= (int)by && y <= (int)(by+save_button.h));

        // ............ Floating tab panel hover (expands toward the left on hover) ............
        /*
            Skipped while a scrollbar is being dragged, or for a short beat
            after it was pressed/scrolled - grabbing/using the main
            scrollbar always takes priority, so the panel can't pop back
            open mid-interaction and steal the space back. Also skipped
            while Settings is the active tab: Settings' content spans the
            full width right under the panel (unlike the code editor), so
            letting it expand there would cover real settings content
            instead of empty background.
        */
        {
            bool close_hit = false;
            tabstrip_hover = tabstrip_hit_test((double)x, (double)y, close_hit);
        }

        // ............ Focus Mode: reveal chrome when the pointer nears a screen edge ............
        // Hysteresis: a thin strip at the very edge *triggers* the reveal,
        // but once revealed the panel stays open as long as the pointer is
        // anywhere over its actual (fully open) bounds - otherwise it would
        // snap shut the instant you moved off the 6px trigger strip and
        // onto the button/row you were trying to click.
        if (focus_mode) {
            bool want_top  = focus_topbar_hover  ? (y <= chrome_h() + 4)
                                                  : (y <= 6);
            bool want_side = focus_sidebar_was_visible &&
                              (focus_sidebar_hover ? (x <= sidebar.width + 4)
                                                    : (x <= 6));
            if (want_top != focus_topbar_hover) {
                focus_topbar_hover = want_top;
                focus_topbar_anim.start(want_top, want_top ? 0.16 : 0.20);
            }
            if (want_side != focus_sidebar_hover) {
                focus_sidebar_hover = want_side;
                sidebar.visible = want_side;
                sidebar_anim.start(want_side, want_side ? 0.16 : 0.20);
            }
            // The floating tab panel already reveals itself via its own
            // hover test above - focus mode just leaves it collapsed to
            // its icon-only width by default, same as normal mode.
        }

        // Sidebar hover
        int side_end = activity_bar_w + (sidebar.visible ? sidebar.width : 0);
        int global_top2 = content_top();
        if (sidebar.visible && x >= activity_bar_w && x < side_end && y >= global_top2) {
            sidebar.hovered_idx = sidebar.hit_test_row(x, y - global_top2, activity_bar_w);
            sidebar.update_hover_btn(x, y - global_top2, activity_bar_w);
        } else {
            sidebar.hovered_idx = -1;
            sidebar.hovered_btn = -1;
        }

        // Addons panel hover
        if (addons_panel_visible && x >= activity_bar_w && x < activity_bar_w + 300 && y >= global_top2) {
            int ry0 = 52, row_h = 52;
            int rel_y = (y - global_top2) - ry0;
            if (rel_y >= 0) addons_panel_hovered = rel_y / row_h;
            else addons_panel_hovered = -1;
        } else {
            addons_panel_hovered = -1;
        }

        // ............ Split editor divider drag ............
        if (split.dragging) {
            int ed_x2    = activity_bar_w + current_sidebar_px();
            int full_ed_w = width - ed_x2;
            split.drag_to(x, ed_x2, full_ed_w);
            return;
        }

        // ............ Console panel mouse motion (split drag, hover) ............
        double con_h = console_h_now();
        if (console.visible && con_h - STATUS_H > 4.0) {
            int lx  = activity_bar_w;
            int cy2 = (int)(height - con_h);
            int cw2 = width - lx;
            console.handle_mouse_motion(x, y, lx, cy2, cw2, (int)(con_h - STATUS_H));
        }

        // ............ Settings tab mouse move ............
        if (is_settings_tab_active()) {
            int ed_x = activity_bar_w + current_sidebar_px();
            settings_tab.handle_mouse_move(x, y, width, height, content_top());
            settings_tab.update_hover(x, y, ed_x, content_top(), width - ed_x);
            if (settings_tab.dragging_spacing || settings_tab.dragging_gutter ||
                settings_tab.dragging_font_size || settings_tab.dragging_indent) {
                apply_settings_from_modal();
                save_current_settings();
            }
            return;
        }

        // ............ Scrollbar drag ............
        double eff_height = height - con_h - content_top();

        if (dragging_v) {
            sb_mark_active();
            double view_h   = eff_height;
            double cont_h   = std::max(1.0, code.total_text_height + 30.0);
            double ratio    = std::clamp(view_h / cont_h, 0.05, 1.0);
            double thumb_h  = std::max(28.0, view_h * ratio);
            double scrollable = std::max(0.0, cont_h - view_h);
            double movable  = view_h - thumb_h;
            if (movable > 0.0) {
                double ds = (y - drag_start_mouse) * (scrollable / movable);
                code.scroll_y = std::clamp(drag_start_val + ds, 0.0, scrollable);
            }
            return;
        }
        if (dragging_v2) {
            sb_mark_active();
            double view_h   = eff_height;
            double cont_h   = std::max(1.0, code2.total_text_height + 30.0);
            double ratio    = std::clamp(view_h / cont_h, 0.05, 1.0);
            double thumb_h  = std::max(28.0, view_h * ratio);
            double scrollable = std::max(0.0, cont_h - view_h);
            double movable  = view_h - thumb_h;
            if (movable > 0.0) {
                double ds = (y - drag_start_mouse) * (scrollable / movable);
                code2.scroll_y = std::clamp(drag_start_val + ds, 0.0, scrollable);
            }
            return;
        }
        if (dragging_h) {
            int    ed_x   = activity_bar_w + current_sidebar_px();
            double tx     = ed_x + gutter_width;
            // Same split-divider fix as the wheel-scroll path above - the
            // dragged scrollbar's own thumb-to-scroll_x math must use the
            // left pane's actual (divider-limited) width too.
            double right_edge = (double)width;
            if (split.enabled) {
                int full_ed_w = width - ed_x;
                right_edge = split.divider_x(ed_x, full_ed_w);
            }
            double view_w = std::max(1.0, right_edge - tx - 20.0);
            double cont_w = std::max(1.0, code.total_text_width + 30.0);
            double ratio  = std::clamp(view_w / cont_w, 0.05, 1.0);
            double thumb_w = std::max(28.0, view_w * ratio);
            double scrollable = std::max(0.0, cont_w - view_w);
            double movable  = view_w - thumb_w;
            if (movable > 0.0) {
                double ds = (x - drag_start_mouse) * (scrollable / movable);
                code.scroll_x = std::clamp(drag_start_val + ds, 0.0, scrollable);
            }
            return;
        }

        // ............ Text selection drag ............
        if (text_drag_active) {
            int    ed_x = activity_bar_w + current_sidebar_px();
            TmpLayout tmp(current_tv());

            constexpr int    EDGE_MARGIN = 40;
            constexpr double EDGE_SPEED  = 18.0;
            int code_top    = content_top();
            int code_bottom = height - STATUS_H - (console.visible ? console.height : 0);

            // Route drag to whichever pane owns this drag session
            bool drag_right = split.enabled && split_right_pane_active;

            if (drag_right) {
                // Right pane drag
                int full_ed_w = width - ed_x;
                int rx2, rw2;
                split.right_rect(ed_x, full_ed_w, rx2, rw2);
                double rtx = rx2 + gutter_width;

                double edge_delta = textor_edge_scroll_delta(y, code_top, code_bottom, EDGE_MARGIN, EDGE_SPEED);
                if (edge_delta != 0.0) {
                    double content_h = code2.line_count * 20.0;
                    double view_h    = (double)(code_bottom - code_top) - 30.0;
                    double max_sy    = std::max(0.0, content_h - view_h);
                    code2.scroll_y = std::clamp(code2.scroll_y + edge_delta, 0.0, max_sy);
                }

                code2.handle_mouse_drag(x, y, (int)rtx, code_top, tmp.layout, tmp.fnt);
            } else {
                // Left (or only) pane drag
                double tx = ed_x + gutter_width;

                double edge_delta = textor_edge_scroll_delta(y, code_top, code_bottom, EDGE_MARGIN, EDGE_SPEED);
                if (edge_delta != 0.0) {
                    double content_h = code.line_count * 20.0;
                    double view_h    = (double)(code_bottom - code_top) - 30.0;
                    double max_sy    = std::max(0.0, content_h - view_h);
                    code.scroll_y = std::clamp(code.scroll_y + edge_delta, 0.0, max_sy);
                }

                code.handle_mouse_drag(x, y, (int)tx, code_top, tmp.layout, tmp.fnt);
            }
            return;
        }
    }

void Editor::handle_key(XKeyEvent *kev) {
        if (!kev) return;

        // First-run onboarding blocks all keyboard input while shown -
        // there's nothing to type into yet (no tabs open on first launch)
        // and it's meant to be dismissed via its Finish button, not Escape.
        // It still needs Up/Down/PageUp/PageDown/Home/End to scroll its
        // body from the keyboard, same as every other scrollable panel.
        if (onboarding.visible) {
            onboarding.handle_key_scroll(XLookupKeysym(kev, 0));
            return;
        }

        // The command palette swallows every key while it is open
        if (palette.visible) { palette.handle_key(kev); return; }

        // File picker intercepts all keys when visible
        if (file_picker.visible) {
            if (settings_tab.shortcuts.matches("paste", kev)) file_picker.paste_text(CodeEditor::x11_clipboard_get());
            else file_picker.handle_key(kev);
            if (file_picker.confirmed) {
                std::string rpath = file_picker.result_path;
                bool is_folder    = file_picker.mode_folder;
                file_picker.confirmed = false;
                if (is_folder) {
                    try {
                        fs::path p(rpath);
                        sidebar.navigate(p);
                        if (!sidebar.visible) {
                            sidebar.visible = true;
                            sidebar_anim.start(true, 0.28);
                        }
                    } catch (...) {}
                } else {
                    std::ifstream ifs(rpath);
                    if (ifs) {
                        std::string buf_content((std::istreambuf_iterator<char>(ifs)),
                                                 std::istreambuf_iterator<char>());
                        open_file_in_tab(rpath, buf_content);
                    }
                }
            }
            return;
        }

        if (modal.visible) {
            if (XLookupKeysym(kev, 0) == XK_Escape) modal_anim.start(false, 0.18);
            return;
        }

        // Console gets keys when focused
        if (console.visible && console_focused) {
            KeySym ks2 = XLookupKeysym(kev, 0);
            if (ks2 == XK_Escape) {
                console.visible = false;
                console_anim.start(false, 0.20);
                btn_console.active = false;
                console_focused    = false;
                return;
            }
            console.handle_key(kev);
            return;
        }

        // Settings tab: keybinding capture
        if (is_settings_tab_active()) {
            if (settings_tab.handle_key_for_rebind(kev)) return;

            KeySym ks_st = XLookupKeysym(kev, 0);
            // Escape closes Settings and returns to whatever was open before.
            if (ks_st == XK_Escape) {
                close_settings();
                return;
            }
            // Left/Right (or PageUp/PageDown) switch between Settings
            // sections (General/Performance/Themes/Keybindings/Help/About)
            // without needing the mouse. (:
            if (ks_st == XK_Left || ks_st == XK_Page_Up) {
                settings_tab.current_section = std::max(0, settings_tab.current_section - 1);
                settings_tab.editing_key_idx = -1; settings_tab.waiting_for_key = false;
                return;
            }
            if (ks_st == XK_Right || ks_st == XK_Page_Down) {
                settings_tab.current_section = std::min((int)SettingsTab::SEC_ABOUT, settings_tab.current_section + 1);
                settings_tab.editing_key_idx = -1; settings_tab.waiting_for_key = false;
                return;
            }
        }

        char buf[32]; KeySym ks;
        int klen = textor_lookup_key(kev, buf, sizeof(buf), &ks);
        bool ctrl  = (kev->state & ControlMask) != 0;
        bool shift = (kev->state & ShiftMask)   != 0;

        std::string layout_text;
        if (klen <= 0 && ks != 0) {
            layout_text = textor_map_key_to_text(static_cast<uint64_t>(ks), static_cast<uint64_t>(kev->state), 1u);
        }
        if (!layout_text.empty()) {
            klen = (int)layout_text.size();
            std::memcpy(buf, layout_text.data(), std::min<size_t>(klen, sizeof(buf) - 1));
            buf[klen] = '\0';
        }
        (void)shift;  // used indirectly via ShortcutManager
        const auto &sc = settings_tab.shortcuts;

        // Ctrl+F - search in all files, Ctrl+Shift+F - search in the current file
        if (ctrl && (ks == XK_f || ks == XK_F)) {
            open_find_bar(shift ? 0 : 1);
            return;
        }

        // Find-bar active: capture all keys
        if (find_bar.visible) {
            if (ks == XK_Escape) { close_find_bar(); return; }
            // Tab - switch between current file and all files
            if (ks == XK_Tab) {
                find_bar.mode = 1 - find_bar.mode;
                run_find_search();
                return;
            }
            if (ks == XK_Return || ks == XK_KP_Enter) {
                if (find_bar.mode == 1) {
                    open_global_result(find_bar.global_idx);
                } else {
                    if (shift) find_bar.prev(); else find_bar.next();
                    jump_to_find_match();
                }
                return;
            }
            if (ks == XK_BackSpace) {
                if (!find_bar.query.empty()) {
                    auto &q = find_bar.query;
                    int i = (int)q.size() - 1;
                    while (i > 0 && (q[i] & 0xC0) == 0x80) --i;
                    q.erase(i);
                    run_find_search();
                }
                return;
            }
            if (ctrl && (ks == XK_v || ks == XK_V)) {
                std::string pasted = CodeEditor::x11_clipboard_get();
                if (pasted.empty()) pasted = code.clipboard;
                if (!pasted.empty()) {
                    for (char c : pasted) { if (c != '\n' && c != '\r') find_bar.query += c; }
                    run_find_search();
                }
                return;
            }
            if (ctrl && (ks == XK_a || ks == XK_A)) {
                find_bar.query.clear();
                if (find_bar.mode == 0) find_bar.search(code.get_buffer_str());
                else find_bar.clear();
                return;
            }
            if (ks == XK_Up)   { find_bar.prev(); if (find_bar.mode==0) jump_to_find_match(); return; }
            if (ks == XK_Down) { find_bar.next(); if (find_bar.mode==0) jump_to_find_match(); return; }
            if (klen > 0 && !ctrl && (unsigned char)buf[0] >= 0x20) {
                find_bar.query.append(buf, klen);
                run_find_search();
                return;
            }
            return;
        }

        // Global shortcuts
        if (sc.matches("open_file", kev)) {
            fs::path start = sidebar.current_path.empty()
                           ? fs::current_path() : sidebar.current_path;
            file_picker.open(false, start);
            return;
        }
        if (sc.matches("command_palette", kev)) { open_command_palette(false); return; }
        if (sc.matches("quick_open",      kev)) { open_command_palette(true);  return; }
        if (sc.matches("new_file", kev)) {
            open_file_in_tab("untitled.sh", "", true);  // default: bash highlighting
            return;
        }
        if (sc.matches("save",           kev)) { save_current_file();  return; }
        if (sc.matches("close_tab",      kev)) {
            if (settings_open) close_settings(); else close_tab(active_tab);
            return;
        }
        if (sc.matches("next_tab",       kev)) {
            if (!tabs.empty()) switch_to_tab((active_tab + 1) % (int)tabs.size());
            return;
        }
        if (sc.matches("prev_tab",       kev)) {
            if (!tabs.empty()) switch_to_tab((active_tab - 1 + (int)tabs.size()) % (int)tabs.size());
            return;
        }
        if (sc.matches("settings",       kev)) {
            // Toggle: Ctrl+, again while already in Settings closes it.
            if (settings_open) close_settings(); else open_settings_tab();
            return;
        }
        if (sc.matches("toggle_sidebar", kev)) {
            sidebar.visible = !sidebar.visible;
            sidebar_anim.start(sidebar.visible, sidebar.visible ? 0.28 : 0.20);
            if (sidebar.visible && addons_panel_visible) {
                addons_panel_visible = false;
                addons_anim.start(false, 0.20);
                btn_addons.active = false;
            }
            return;
        }
        if (sc.matches("toggle_console", kev)) { toggle_terminal(); return; }

        // ............ Focus Mode (F11) ............
        if (sc.matches("focus_mode", kev)) {
            focus_mode = !focus_mode;
            if (focus_mode) {
                // Remember & collapse chrome; both slide back in on hover.
                // The sidebar reuses its own existing visible/animation
                // machinery (so width reflow keeps working for free)
                focus_sidebar_was_visible = sidebar.visible;
                if (sidebar.visible) { sidebar.visible = false; sidebar_anim.start(false, 0.0); }
                focus_topbar_anim.start(false, 0.0);
                focus_topbar_hover = false;
                focus_sidebar_hover = false;
            } else {
                // Leaving focus mode restores normal chrome immediately.
                focus_topbar_anim.start(true, 0.0);
                if (focus_sidebar_was_visible && !sidebar.visible) {
                    sidebar.visible = true;
                    sidebar_anim.start(true, 0.0);
                }
            }
            return;
        }

        // ............ Full keyboard control: move focus between panels ............
        if (sc.matches("focus_sidebar", kev)) {
            if (!sidebar.visible) {
                sidebar.visible = true;
                sidebar_anim.start(true, 0.28);
            }
            ui_focus = UIFocus::Sidebar;
            if (sidebar.selected_idx < 0 && !sidebar.flat.empty()) sidebar.selected_idx = 0;
            return;
        }
        if (sc.matches("focus_editor", kev)) { ui_focus = UIFocus::EditorText; return; }
        if (sc.matches("focus_tabs",   kev)) {
            ui_focus = UIFocus::TabPanel;
            if (tabpanel_kbd_idx < 0 && !tabs.empty()) tabpanel_kbd_idx = active_tab;
            tabstrip_ensure_visible(tabpanel_kbd_idx);
            return;
        }
        if (sc.matches("focus_console", kev)) {
            if (!console.visible) {
                console.visible = true;
                console_anim.start(true, 0.28);
                btn_console.active = true;
            }
            ui_focus = UIFocus::Console;
            console_focused = true;
            return;
        }
        if (sc.matches("move_focus_next", kev) || sc.matches("move_focus_prev", kev)) {
            bool fwd = sc.matches("move_focus_next", kev);
            // Cycle: Sidebar (if open) -> Editor -> TabPanel -> Console (if open) -> ...
            std::vector<UIFocus> ring;
            if (sidebar.visible) ring.push_back(UIFocus::Sidebar);
            ring.push_back(UIFocus::EditorText);
            if (!tabs.empty()) ring.push_back(UIFocus::TabPanel);
            if (console.visible) ring.push_back(UIFocus::Console);
            int cur = 0;
            for (int i = 0; i < (int)ring.size(); i++) if (ring[i] == ui_focus) { cur = i; break; }
            int nxt = fwd ? (cur + 1) % (int)ring.size() : (cur - 1 + (int)ring.size()) % (int)ring.size();
            ui_focus = ring[nxt];
            console_focused = (ui_focus == UIFocus::Console);
            return;
        }

        // ............ Sidebar keyboard navigation (Up/Down/Left/Right/Enter) ............
        // Only intercepts when the sidebar owns keyboard focus, so normal
        // typing in the editor is completely unaffected.
        if (ui_focus == UIFocus::Sidebar && sidebar.visible &&
            sidebar.action_mode == Sidebar::ACT_NONE && !sidebar.search_active) {
            // Moving the selection also scrolls it into view (done in Sidebar::draw)
            const int last_row = (int)sidebar.flat.size() - 1;
            const int page = std::max(1, (height - content_top() - sidebar.header_h()) / Sidebar::ROW_H - 1);
            if (ks == XK_Down || ks == XK_Up || ks == XK_Page_Down || ks == XK_Page_Up ||
                ks == XK_Home || ks == XK_End) {
                if (last_row >= 0) {
                    int &sel = sidebar.selected_idx;
                    if      (ks == XK_Down)      sel = std::min(last_row, sel + 1);
                    else if (ks == XK_Up)        sel = std::max(0, sel - 1);
                    else if (ks == XK_Page_Down) sel = std::min(last_row, std::max(0, sel) + page);
                    else if (ks == XK_Page_Up)   sel = std::max(0, std::max(0, sel) - page);
                    else if (ks == XK_Home)      sel = 0;
                    else                         sel = last_row;
                    sidebar.follow_selection = true;
                }
                return;
            }
            if (ks == XK_Right) {
                if (sidebar.selected_idx >= 0 && sidebar.selected_idx < (int)sidebar.flat.size()) {
                    TreeNode *n = sidebar.flat[sidebar.selected_idx];
                    if (n->is_dir && !n->expanded) sidebar.toggle_expand(sidebar.selected_idx);
                }
                sidebar.follow_selection = true;
                return;
            }
            if (ks == XK_Left) {
                if (sidebar.selected_idx >= 0 && sidebar.selected_idx < (int)sidebar.flat.size()) {
                    TreeNode *n = sidebar.flat[sidebar.selected_idx];
                    if (n->is_dir && n->expanded) sidebar.toggle_expand(sidebar.selected_idx);
                }
                sidebar.follow_selection = true;
                return;
            }
            if (ks == XK_Return) {
                if (sidebar.selected_idx >= 0 && sidebar.selected_idx < (int)sidebar.flat.size()) {
                    TreeNode *n = sidebar.flat[sidebar.selected_idx];
                    if (n->is_dir) {
                        sidebar.toggle_expand(sidebar.selected_idx);
                    } else {
                        std::ifstream ifs(n->path);
                        if (ifs) {
                            std::string buf_content((std::istreambuf_iterator<char>(ifs)),
                                                     std::istreambuf_iterator<char>());
                            open_file_in_tab(n->path.string(), buf_content);
                            ui_focus = UIFocus::EditorText;
                        }
                    }
                }
                return;
            }
        }

        // -- Floating tab panel keyboard navigation (Up/Down/Enter/Ctrl+W) --
        if (ui_focus == UIFocus::TabPanel && !tabs.empty()) {
            if (ks == XK_Down) {
                tabpanel_kbd_idx = std::min((int)tabs.size() - 1, tabpanel_kbd_idx + 1);
                tabstrip_ensure_visible(tabpanel_kbd_idx);
                return;
            }
            if (ks == XK_Up)   {
                tabpanel_kbd_idx = std::max(0, tabpanel_kbd_idx - 1);
                tabstrip_ensure_visible(tabpanel_kbd_idx);
                return;
            }
            if (ks == XK_Return) {
                if (tabpanel_kbd_idx >= 0 && tabpanel_kbd_idx < (int)tabs.size()) {
                    switch_to_tab(tabpanel_kbd_idx);
                    ui_focus = UIFocus::EditorText;
                }
                return;
            }
            // Delete/Backspace - close the keyboard-highlighted tab (not
            // necessarily the active one) without touching the mouse.
            // (Ctrl+W already closes the *active* tab globally, handled
            // earlier in this function regardless of focus.)
            if (ks == XK_Delete || ks == XK_BackSpace) {
                if (tabpanel_kbd_idx >= 0 && tabpanel_kbd_idx < (int)tabs.size()) {
                    int closing = tabpanel_kbd_idx;
                    close_tab(closing);
                    tabpanel_kbd_idx = tabs.empty() ? -1 : std::min(closing, (int)tabs.size() - 1);
                    if (tabs.empty()) ui_focus = UIFocus::EditorText;
                    tabstrip_ensure_visible(tabpanel_kbd_idx);
                }
                return;
            }
        }

        if (ctrl && (ks == XK_backslash || ks == 0x5c)) {
            split.enabled = !split.enabled;
            btn_split.active = split.enabled;
            if (split.enabled) {
                code2.buffer     = code.buffer.to_string();
                code2.filename   = code.filename;
                code2.syn_lang   = code.syn_lang;
                code2.dirty      = false;
                code2.rebuild_lines();
                code2.ml_comment_state.clear();
                code2.highlight_dirty = true;
                code2.update_syn_lang();
                code2.scroll_y   = code.scroll_y;
                code2.scroll_x   = 0.0;
                code2.cursor_pos = code.cursor_pos;
                split.active_pane = 0;
                split_right_pane_active = false;
            }
            return;
        }
        // Ctrl+Shift+\ - reset split to a perfect 50/50 (same as double-clicking divider)
        if (ctrl && shift && (ks == XK_backslash || ks == 0x5c)) {
            if (split.enabled) split.reset_ratio();
            return;
        }
        if (sc.matches("quit", kev)) { request_quit = true; return; }

        // Ctrl+L - scroll to cursor (jump viewport to current cursor line)
        if (sc.matches("scroll_to_cursor", kev)) {
            code.auto_scroll_to_cursor = true;
            return;
        }

        // Escape
        if (ks == XK_Escape) {
            // 1. The Explorer comes first: cancel a pending new file / folder / delete,
            //    or collapse the search. The search box slides back and the
            //    "New File" button grows out again (see Sidebar::tick_anims).
            //    This also works while the sidebar is revealed in focus mode.
            if (sidebar.action_mode != Sidebar::ACT_NONE) {
                sidebar.cancel_action();
                return;
            }
            if (sidebar.search_active || sidebar.search_expanded) {
                sidebar.search_active   = false;
                sidebar.search_expanded = false;
                sidebar.filter.clear();
                sidebar.apply_filter();
                return;
            }
            // 2. Leave focus mode
            if (focus_mode) {
                focus_mode = false;
                focus_topbar_anim.start(true, 0.0);
                if (focus_sidebar_was_visible && !sidebar.visible) {
                    sidebar.visible = true;
                    sidebar_anim.start(true, 0.0);
                }
                return;
            }
            // 3. Give the keyboard back to the editor text
            if (ui_focus != UIFocus::EditorText) {
                ui_focus = UIFocus::EditorText;
                console_focused = false;
                return;
            }
            // 4. Close the terminal
            if (console.visible) {
                console.visible = false;
                console_anim.start(false, 0.20);
                btn_console.active = false;
                console_focused    = false;
                return;
            }
        }

        // Sidebar action dialog (new file / new folder / delete confirm)
        // Only intercept keys when sidebar is actually visible/open
        if (sidebar.action_mode != Sidebar::ACT_NONE && sidebar.visible) {
            if (sc.matches("paste", kev)) sidebar.paste_text(CodeEditor::x11_clipboard_get());
            else sidebar.handle_action_key(ks, buf, klen);
            return;
        } else if (sidebar.action_mode != Sidebar::ACT_NONE && !sidebar.visible) {
            // Sidebar was closed while action was pending - cancel it
            sidebar.cancel_action();
        }

        // Sidebar search input
        if (sidebar.search_active && sidebar.visible) {
            if (ks == XK_BackSpace) {
                if (!sidebar.filter.empty()) {
                    textor_pop_utf8(sidebar.filter);
                    sidebar.apply_filter();
                    if (sidebar.search_mode != Sidebar::SEARCH_FILENAME)
                        sidebar.do_global_search();
                }
                return;
            }
            if (ks == XK_Return) {
                // In content/regex mode Enter triggers search
                if (sidebar.search_mode != Sidebar::SEARCH_FILENAME)
                    sidebar.do_global_search();
                sidebar.search_active = false;
                return;
            }
            if (klen > 0 && !ctrl) {
                sidebar.filter.append(buf, klen);
                sidebar.apply_filter();
                if (sidebar.search_mode != Sidebar::SEARCH_FILENAME)
                    sidebar.do_global_search();
            }
            return;
        }

        // A non-editor panel owns keyboard focus (Sidebar/TabPanel/Console) -
        // don't leak ordinary typing into the code buffer.
        if (ui_focus != UIFocus::EditorText) return;

        if (split.enabled && split_right_pane_active) {
            code2.handle_key(kev);
            // Sync right -> left: mirror text changes (VS Code linked editing)
            if (code2.dirty != _sync_dirty2 || code2.buffer.size() != _sync_sz2) {
                _sync_dirty2 = code2.dirty;
                _sync_sz2    = code2.buffer.size();
                code.buffer  = code2.buffer.to_string();
                code.highlight_dirty = true;
                code.rebuild_lines();
            }
        } else {
            code.handle_key(kev);
            // Sync left -> right
            if (code.dirty != _sync_dirty || code.buffer.size() != _sync_sz) {
                _sync_dirty = code.dirty;
                _sync_sz    = code.buffer.size();
                code2.buffer = code.buffer.to_string();
                code2.highlight_dirty = true;
                code2.rebuild_lines();
            }
        }
    }

#endif
