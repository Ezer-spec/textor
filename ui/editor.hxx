#ifndef EDITOR_HXX
#define EDITOR_HXX

#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <deque>
#include <chrono>
#include <cmath>
#include <set>
#include <cstring>
#include <unistd.h>
#include <pango/pangocairo.h>
#include <filesystem>
#include <X11/Xlib.h>

#include "../color/themes.hxx"
#include "sidebar.hxx"
#include "morphing.hxx"
#include "icon_anim.hxx"
#include "settings_modal.hxx"
#include "../base/config.hxx"
#include "code_editor.hxx"
#include "console_panel.hxx"
#include "settings_tab.hxx"
#include "../base/addon_loader.hxx"
#include "file_picker.hxx"
#include "command_palette.hxx"
#include "onboarding_modal.hxx"
#include "../base/globals.hxx"
#include "split_editor.hxx"
#include "markdown_view.hxx"
#include "draw_context.hxx"

namespace fs = std::filesystem;
/*
    draw_rounded_rect_ed -> see morphing.hxx (morph::rrect)
    Editor - the main UI object of the Textor editor
    This is the central class that stores ALL interface state.
    Created once in main() and lives throughout the program's execution.
    MAIN COMPONENTS:
    code - code editor (CodeEditor): buffer, cursor, highlighting
    sidebar - file explorer on the left
    modal - modal settings window (old)
    settings_tab - settings tab (new, in the tab bar)
    console - built-in console at the bottom
    file_picker - file/folder selection dialog
    scrollbar - vertical and horizontal scrollbars
    LIFE CYCLE:
    1. Editor() constructor - loads config, addons, themes
    2. The main loop (base.cxx) receives events and calls handle_*
    3. draw_ui() (lower in this file) draws everything through Cairo
    Do you want to add a new UI element?
    a) Add its data here (in the struct Editor)
    b) Draw it in draw_ui() in the desired order (Z-order)
    c) Add click processing to handle_mouse_click()
    See ui/renderer.hxx and ui/input_handler.hxx for details
*/
struct Editor {
// Window size (updated with ConfigureNotify from X11)
    int width  = 1100;
    int height = 750;

    // Dirty rect - which part of the frame has changed
    // GPU blit updates only this horizontal bar via glTexSubImage2D.
    // y0=0, y1=height means 'whole frame' (after resize or theme change).
    int dirty_rect_y0 = 0;
    int dirty_rect_y1 = 750;

    /*
        GPU selection overlay
        The text selection is drawn via OpenGL on top of the Cairo texture.
        This is faster than redrawing the selection via Cairo every frame.

        Data is filled into draw_ui() from CodeEditor::GpuOverlay,
        and used in base.cxx (gpu_blit) for drawing via glBegin/GL_QUADS.

        Maximum of 3 rectangles: first row, middle block, last row.
    */
    struct GpuSelRect { double x, y, w, h; };
    static constexpr int MAX_SEL_RECTS = 3;
    GpuSelRect  gpu_sel_rects[MAX_SEL_RECTS] = {};
    int         gpu_sel_rect_count = 0;  // how many rectangles are active
    bool        gpu_sel_active = false;  // is there any selection at all?
    float       gpu_sel_r = 0.4f, gpu_sel_g = 0.6f, gpu_sel_b = 1.0f; // highlight color

    int gutter_width      = 50;
    const int activity_bar_w = 0;

    // Tab system
    struct Tab {
        std::string filename;
        std::string buffer;
        int  cursor_pos = 0;
        int  sel_anchor = 0;
        bool sel_active = false;
        double scroll_y = 0.0;
        double scroll_x = 0.0;
        bool dirty = false;
        SyntaxHighlighter::Lang syn_lang = SyntaxHighlighter::Lang::Shell;  // default: bash highlighting

        // Per-tab undo/redo - freed automatically when tab is closed (erase calls dtor)
        std::deque<CodeEditor::UndoRecord>  undo_stack;
        std::deque<CodeEditor::UndoRecord>  redo_stack;
    };
    std::vector<Tab> tabs;
    int active_tab = 0;
    // Top chrome: the top bar (TAB_BAR_H) and the tab strip under it (LANG_BAR_H).
    // The editor starts at content_top() - see below.
    const int TAB_BAR_H = 44;
    const int LANG_BAR_H = 34;   // the tab strip under the top bar
    const int STATUS_H   = 24;   // status bar along the bottom edge (see render/status_bar_draw.hxx)
    // -- Top chrome = top bar + tab strip, one block that slides as a whole --
    // Normally it is chrome_h() pixels high and the editor starts below it.
    // In focus mode (F11) it slides off the top and the editor moves up with it,
    // so no empty space is left; hovering the top edge brings both back together.
    // update_chrome() runs once per frame, so every part of a frame sees the same value.
    int chrome_top_px = TAB_BAR_H + LANG_BAR_H;   // current height of the visible chrome
    int    chrome_h() const     { return TAB_BAR_H + LANG_BAR_H; }
    int    content_top() const  { return chrome_top_px; }                 // y where the editor starts
    double chrome_shift() const { return chrome_h() - chrome_top_px; }    // how far the chrome is slid up
    void update_chrome() {
        double reveal = focus_mode ? focus_topbar_anim.progress() : 1.0;
        chrome_top_px = (int)std::lround(chrome_h() * reveal);
    }
    // Is screen row y inside the (possibly sliding) tab strip?
    bool in_tab_strip(int y) const {
        double top = TAB_BAR_H - chrome_shift();
        return y >= top && y < top + LANG_BAR_H;
    }

    // Tab strip: one row of tabs under the top bar
    // Drawn by draw_tab_strip() in render/tab_panel_draw.hxx.
    static constexpr double TAB_W_MAX = 190.0, TAB_W_MIN = 120.0;
    double tabstrip_scroll_x = 0.0;   // sideways scroll once the tabs don't fit
    int    tabstrip_hover    = -1;    // tab under the mouse (-1 = none)
    // Explorer toggle is now a pill (icon-only -> icon+label), so the
    // reserved space before the tabs grows/shrinks with it each frame.
    double    TOPBAR_LEFT_RESERVE  = 96; // Explorer pill + divider, before the tabs
    const int TOPBAR_RIGHT_RESERVE = 76; // settings + exit icons, after the tabs
    double tab_scroll_x = 0.0;  // horizontal scroll offset for tab bar

    void open_file_in_tab(const std::string &filename, const std::string &buffer_content, bool is_new = false) {
        // Check if already open
        for (int i = 0; i < (int)tabs.size(); i++) {
            if (tabs[i].filename == filename) { switch_to_tab(i); return; }
        }
        Tab t;
        t.filename = filename;
        t.buffer   = buffer_content;
        t.syn_lang = SyntaxHighlighter::detect(filename);
        t.dirty    = is_new;
        tabs.push_back(t);
        switch_to_tab((int)tabs.size() - 1);
    }

    // Open a path passed on the command line (file or folder)
    // Mirrors what the "Open Folder" / "Open File" welcome-screen buttons do,
    //   textor main.cxx  -> opens that file in a tab
    //   textor ~/project -> opens that folder in the sidebar/explorer
    void open_cli_path(const fs::path &raw_path) {
        std::error_code ec;
        fs::path p = fs::absolute(raw_path, ec);
        if (ec) p = raw_path;

        if (fs::is_directory(p, ec)) {
            sidebar.navigate(p);
            sidebar.visible = true;
            sidebar_anim.start(true, 0.28);
            return;
        }

        if (fs::is_regular_file(p, ec)) {
            std::ifstream ifs(p);
            if (ifs) {
                std::string buf_content((std::istreambuf_iterator<char>(ifs)),
                                         std::istreambuf_iterator<char>());
                open_file_in_tab(p.string(), buf_content);
            }
            // Point the explorer at the file's folder too, same as opening
            // a single file normally would leave you oriented in the tree. >_-
            fs::path parent = p.parent_path();
            if (!parent.empty()) {
                sidebar.current_path = parent;
                sidebar.refresh();
            }
            return;
        }

        // Path doesn't exist yet - treat it like "New File" under that name,
        // so e.g. `textor notes.md` on a fresh filename works like most editors. . . . .
        if (!p.empty())
            open_file_in_tab(p.string(), "", true);
    }

    void switch_to_tab(int idx) {
        // Only save current editor state back when there are multiple tabs
        // and we're actually switching away (avoid clobbering first-opened file)
        if (tabs.size() > 1 && active_tab >= 0 && active_tab < (int)tabs.size() && active_tab != idx) {
            tabs[active_tab].buffer     = code.get_buffer_str();
            tabs[active_tab].cursor_pos = code.cursor_pos;
            tabs[active_tab].sel_anchor = code.sel_anchor;
            tabs[active_tab].sel_active = code.sel_active;
            tabs[active_tab].scroll_y   = code.scroll_y;
            tabs[active_tab].scroll_x   = code.scroll_x;
            tabs[active_tab].dirty      = code.dirty;
            // Save undo/redo so history is per-tab and freed when tab closes
            tabs[active_tab].undo_stack = std::move(code.undo_stack);
            tabs[active_tab].redo_stack = std::move(code.redo_stack);
            code.undo_stack.clear();
            code.redo_stack.clear();
        }
        active_tab = std::clamp(idx, 0, (int)tabs.size() - 1);
        if (tabs.empty()) return;
        auto &t = tabs[active_tab];
        code.buffer     = t.buffer;
        code.cursor_pos = t.cursor_pos;
        code.sel_anchor = t.sel_anchor;
        code.sel_active = t.sel_active;
        code.scroll_y   = t.scroll_y;
        code.scroll_x   = t.scroll_x;
        // Restore per-tab undo/redo history
        code.undo_stack = std::move(t.undo_stack);
        code.redo_stack = std::move(t.redo_stack);
        t.undo_stack.clear();
        t.redo_stack.clear();
        code.dirty      = t.dirty;
        code.filename   = t.filename;
        code.syn_lang   = t.syn_lang;
        code.rebuild_lines();
        code.ml_comment_state.clear();
        code.highlight_dirty = true;
        code.update_syn_lang();   // resolves active_addon from addon_loader
        code.clear_extra_cursors();
        // Bug fix: reset GPU allocation when changing tabs
        // Without this, OpenGL overlay continues to draw the selection rectangle
        // on top of settings / another tab where there is no selection. (0_0)
        gpu_sel_active = false;
        gpu_sel_rect_count = 0;
        code.autocomplete.hide();
        code.word_hints_on_ptr  = &settings_tab.word_hints_on;
        code2.word_hints_on_ptr = &settings_tab.word_hints_on;
        code.init_autocomplete();
        tabstrip_ensure_visible(active_tab);
    }

    void close_tab(int idx) {
        if (idx < 0 || idx >= (int)tabs.size()) return;
        // Explicitly free all per-tab heap data before erase
        { auto &t = tabs[idx];
          std::string().swap(t.buffer);
          std::deque<CodeEditor::UndoRecord>().swap(t.undo_stack);
          std::deque<CodeEditor::UndoRecord>().swap(t.redo_stack); }
        tabs.erase(tabs.begin() + idx);
        if (tabs.empty()) {
            active_tab = 0;
            code.buffer.clear();
            code.filename.clear();
            // Release the live undo/redo history
            { std::deque<CodeEditor::UndoRecord>  tmp; tmp.swap(code.undo_stack); }
            { std::deque<CodeEditor::UndoRecord>  tmp; tmp.swap(code.redo_stack); }
            code.rebuild_lines();
            // Also shrink the line and ML-comment vectors so their capacity
            // returns to zero after a large file is unloaded.
            code.lines.shrink_to_fit();
            code.ml_comment_state.shrink_to_fit();
            return;
        }
        active_tab = std::clamp(active_tab, 0, (int)tabs.size() - 1);
        switch_to_tab(active_tab);
    }

    // ............ Tab strip geometry ............
    // The strip starts where the editor starts (right of the sidebar).
    double tabstrip_x0() const { return (double)activity_bar_w + current_sidebar_px(); }
    // Every tab has the same width: shrinks when there are many, but not below TAB_W_MIN.
    double tabstrip_tab_w() const {
        if (tabs.empty()) return TAB_W_MAX;
        double room = (double)width - tabstrip_x0();
        return std::clamp(room / (double)tabs.size(), TAB_W_MIN, TAB_W_MAX);
    }
    double tabstrip_scroll_max() const {
        double room = (double)width - tabstrip_x0();
        return std::max(0.0, tabstrip_tab_w() * (double)tabs.size() - room);
    }
    // Scrolls the strip just enough to show tab `i` completely.
    void tabstrip_ensure_visible(int i) {
        if (i < 0 || tabs.empty()) return;
        double left  = i * tabstrip_tab_w();
        double right = left + tabstrip_tab_w();
        double room  = (double)width - tabstrip_x0();
        if (left  < tabstrip_scroll_x)         tabstrip_scroll_x = left;
        if (right > tabstrip_scroll_x + room)  tabstrip_scroll_x = right - room;
        tabstrip_scroll_x = std::clamp(tabstrip_scroll_x, 0.0, tabstrip_scroll_max());
    }
    // Which tab is under (mx, my)? Returns its index or -1.
    // close_hit is set when the mouse is over the "x" at the tab's right end.
    int tabstrip_hit_test(double mx, double my, bool &close_hit) const {
        close_hit = false;
        if (tabs.empty()) return -1;
        if (!in_tab_strip((int)my)) return -1;
        double x0 = tabstrip_x0();
        if (mx < x0) return -1;
        double tw = tabstrip_tab_w();
        int i = (int)((mx - x0 + tabstrip_scroll_x) / tw);
        if (i < 0 || i >= (int)tabs.size()) return -1;
        double tab_right = x0 + (i + 1) * tw - tabstrip_scroll_x;
        close_hit = (mx > tab_right - 28.0);
        return i;
    }

    /*
        The main editor scrollbar always lives at the true right edge of the
        window (margin 0) - it does NOT move to avoid the floating tab
        panel. Instead, the tab panel is the one that gets out of the way:
        pressing/holding/scrolling the main scrollbar collapses the panel
        and nudges it further left for the duration (see sb_mark_active(),
        tabpanel_sb_avoid_anim, and their use in draw_ui).
    */
    double tabpanel_sb_margin() const { return 0.0; }

    // Time of the last use of the main scrollbar (pressed, dragged or wheel).
    std::chrono::steady_clock::time_point sb_last_active_time{};
    void sb_mark_active() { sb_last_active_time = std::chrono::steady_clock::now(); }
    bool sb_recently_active() const {
        using namespace std::chrono;
        return duration_cast<milliseconds>(steady_clock::now() - sb_last_active_time).count() < 450;
    }

    Sidebar       sidebar;
    ScrollBar     scrollbar;
    SettingsModal modal;
    ConsolePanel  console;
    SettingsTab   settings_tab;
    FilePicker    file_picker;
    CommandPalette palette;      // Ctrl+Shift+P / Ctrl+P
    OnboardingModal onboarding;
    bool first_run_done = false;   // set true forever once onboarding.finish is clicked

    // Returns true while the Settings overlay is showing. Settings is no
    // longer a fake entry inside `tabs` - it's a standalone overlay (same
    // idea as file_picker.visible/console.visible) that floats on top of
    // whatever real tab was active, so opening/closing it never touches
    // `tabs` or `active_tab` at all.
    bool settings_open = false;
    bool is_settings_tab_active() const { return settings_open; }

    // Open the Settings overlay. Real tabs are left completely alone
    // nothing is pushed into `tabs`, nothing is saved/restored - so
    // closing Settings again just reveals the same tab exactly as it was.
    void open_settings_tab() { settings_open = true; }
    // Close the Settings overlay and return to the real tabs.
    void close_settings()    { settings_open = false; }

    // Split editor state
    SplitEditor split;
    // Second CodeEditor instance for the right pane (shares buffer with code)
    CodeEditor  code2;
    bool        split_right_pane_active = false;

    // Buffer sync state - detect changes without full string compare each frame
    bool   _sync_dirty  = false;
    bool   _sync_dirty2 = false;
    size_t _sync_sz     = 0;
    size_t _sync_sz2    = 0;

    BarIcon btn_sidebar  = {10, 20,  30};
    BarIcon btn_addons   = {10, 60,  30};   // repositioned into lang-bar each frame
    BarIcon btn_split    = {10, 100, 30};   // repositioned into lang-bar each frame
    BarIcon btn_console  = {10, 0,   30};   // repositioned into lang-bar each frame
    BarIcon btn_viewers  = {10, 0,   30};   // only shown while a .md file is open
    bool    md_viewer_open = false;         // Viewers: the right pane shows the formatted .md
    BarIcon btn_exit     = {10, 0,   30};   // now lives top-right of the tab bar
    BarIcon btn_settings = {10, 0,   30};   // now lives top-right of the tab bar

    /*
        Split/addons/console buttons live in the new strip below the tab bar
        (left side) instead of the vertical activity bar. Computed fresh each
        frame from the current sidebar/activity-bar width so it always lines
        up whether the sidebar is open, closed, or mid-animation.
        Actual on-screen bounds of the split/addons/console pills in the
        second bar - they're wide rectangles, not squares, so BarIcon's
        square is_hit() can't be used for them, the click handler does its
        own rect test against these instead.
    */
    double lang_pill_split_w = 30, lang_pill_addons_w = 30, lang_pill_console_w = 30;
    double lang_pill_viewers_w = 0;   // 0 = button hidden (the open file is not .md)
    double lang_pill_y = 0, lang_pill_h = 22;

    // Explorer toggle in the top bar is now the same growing pill shape as
    // Split/Addons/Console, so it needs the same non-square hit-test.
    double sidebar_pill_w = 30;
    double topbar_pill_y = 0, topbar_pill_h = 22;
    bool topbar_pill_hit(int bx, double bw, int mx, int my) const {
        return mx>=bx && mx<=bx+bw && my>=topbar_pill_y && my<=topbar_pill_y+topbar_pill_h;
    }

    void position_lang_bar_buttons() {
        int ed_x2 = activity_bar_w + current_sidebar_px();
        int icon_sz = 22;
        int by = TAB_BAR_H + (LANG_BAR_H - icon_sz) / 2;
        int bx = ed_x2 + 10;
        btn_split.size   = icon_sz; btn_split.x   = bx;      btn_split.y   = by;
        btn_addons.size  = icon_sz; btn_addons.x  = bx + 30; btn_addons.y  = by;
        btn_console.size = icon_sz; btn_console.x = bx + 60; btn_console.y = by;
    }

    // Panel toggles (used by the top bar buttons and the command palette)
    void toggle_explorer() {
        icon_anim::trigger(&btn_sidebar);
        sidebar.visible = !sidebar.visible;
        sidebar_anim.start(sidebar.visible, 0.26);
        if (sidebar.visible && addons_panel_visible) {       // only one side panel at a time
            addons_panel_visible = false;
            addons_anim.start(false, 0.20);
            btn_addons.active = false;
        }
    }
    void toggle_addons_panel() {
        icon_anim::trigger(&btn_addons);
        addons_panel_visible = !addons_panel_visible;
        addons_anim.start(addons_panel_visible, 0.26);
        btn_addons.active = addons_panel_visible;
        if (addons_panel_visible && sidebar.visible) {
            sidebar.visible = false;
            sidebar_anim.start(false, 0.20);
            btn_sidebar.active = false;
        }
    }
    void open_search_panel() {
        if (!sidebar.visible) toggle_explorer();
        sidebar.search_expanded = true;
        sidebar.search_active   = true;
    }

    // Explorer-toggle (left) + settings/exit (right) now live inside the
    // top bar itself instead of a separate vertical activity bar.
    void position_topbar_buttons() {
        int icon_sz = 22;
        int by = (TAB_BAR_H - icon_sz) / 2;
        btn_sidebar.size  = icon_sz; btn_sidebar.x  = activity_bar_w + 8;  btn_sidebar.y = by;
        btn_exit.size     = icon_sz; btn_exit.x     = width - 8 - icon_sz;             btn_exit.y = by;
        btn_settings.size = icon_sz; btn_settings.x = width - 8 - icon_sz*2 - 10;      btn_settings.y = by;
    }

    TextButton save_button;

    bool request_quit = false;

    // Welcome screen button hit areas
    struct Rect { int x,y,w,h; bool hit(int mx,int my) const { return mx>=x&&mx<x+w&&my>=y&&my<y+h; } };
    Rect welcome_btn_folder = {0,0,0,0};
    Rect welcome_btn_file   = {0,0,0,0};
    int  welcome_btn_hovered = -1; // 0=folder, 1=file

    // Scrollbar drag state
    bool   dragging_v      = false;
    bool   dragging_h      = false;
    bool   dragging_v2     = false;   // right pane vertical scrollbar drag
    double drag_start_mouse = 0.0;
    double drag_start_val   = 0.0;

    // Text selection drag
    bool   text_drag_active = false;

    // Console focus
    bool console_focused = false;

    // Focus Mode (F11)
    // Borderless fullscreen; top bar + sidebar + floating tab panel hide
    // themselves and slide back in only while the pointer hovers the
    // corresponding screen edge. Only the code viewport stays visible.
    bool focus_mode = false;
    AnimController focus_topbar_anim;   // top edge reveal
    bool focus_topbar_hover  = false;
    bool focus_sidebar_hover = false;
    bool focus_sidebar_was_visible = false; // sidebar.visible snapshot before entering focus mode

    // Full keyboard control
    // Which UI region currently owns keyboard navigation (arrows/enter).
    // Typing always goes to the code editor except when ui_focus targets a
    // panel that consumes navigation keys itself (Sidebar file tree, the
    // floating Tab panel, or the Console).
    enum class UIFocus { EditorText, Sidebar, TabPanel, Console };
    UIFocus ui_focus = UIFocus::EditorText;
    int     tabpanel_kbd_idx = -1;  // keyboard-highlighted tab in the tab strip

    ThemeVariant   current_theme_variant;
    ThemeTween     theme_tween;
    AnimController sidebar_anim;
    AnimController modal_anim;
    AnimController console_anim;
    AnimController addons_anim;

    // Addons panel
    bool addons_panel_visible = false;
    int  addons_panel_scroll  = 0;
    int  addons_panel_hovered = -1;

    // Find bar (Ctrl+F) - In-file AND global project search
    struct FindBar {
        bool        visible     = false;
        std::string query;
        int         match_idx   = -1;
        std::vector<int> matches;  // byte offsets for in-file mode

        // Mode: 0 = in current file, 1 = in project (all files)
        int  mode = 0;   // 0=file, 1=project

        // Sizes shared by drawing and mouse handling - change them only here
        static constexpr int    H          = 44;    // height of the input row
        static constexpr int    RESULTS_H  = 320;   // height of the results list (All files mode)
        static constexpr int    ITEM_H     = 60;    // height of one result (two lines + spacing)
        static constexpr int    MAX_RESULTS  = 500;
        static constexpr int    MAX_PER_FILE = 40;
        static constexpr double BTN     = 24.0;     // square buttons (arrows, close)
        static constexpr double MODE_H  = 26.0;     // height of the File / All files switch
        static constexpr double MODE_W0 = 52.0;     // width of "File"
        static constexpr double MODE_W1 = 84.0;     // width of "All files"
        static constexpr double PAD     = 12.0;
        static constexpr double CNT_W   = 96.0;     // width of the "3 / 12" counter

        // Positions of every control, relative to the top-left corner of the panel
        struct Geom {
            double fi_x, fi_y, fi_w, fi_h;   // input field
            double cnt_x;                    // match counter
            double nav_x[2], nav_y;          // previous / next arrows
            double mode_x[2], mode_y;        // File / All files switch
            double close_x, close_y;         // close button
        };
        static Geom geom(double panel_w, double row_h) {
            Geom g;
            g.close_x   = panel_w - PAD - BTN;
            g.close_y   = (row_h - BTN) / 2.0;
            g.mode_x[1] = g.close_x - PAD - MODE_W1;
            g.mode_x[0] = g.mode_x[1] - MODE_W0;
            g.mode_y    = (row_h - MODE_H) / 2.0;
            g.nav_x[1]  = g.mode_x[0] - PAD - BTN;
            g.nav_x[0]  = g.nav_x[1] - 4.0 - BTN;
            g.nav_y     = (row_h - BTN) / 2.0;
            g.cnt_x     = g.nav_x[0] - 8.0 - CNT_W;
            g.fi_x      = 40.0;
            g.fi_h      = row_h - 12.0;
            g.fi_y      = (row_h - g.fi_h) / 2.0;
            g.fi_w      = std::max(120.0, g.cnt_x - 8.0 - g.fi_x);
            return g;
        }

        AnimController anim;

        // Search results across all files
        struct GlobalResult {
            std::string path;      // full file path
            std::string rel_dir;   // folder of the file, relative to the search root
            std::string line_text; // the matching line (trimmed, one line)
            int         line_no;   // 1-based
            int         hl_start;  // where the query starts inside line_text (bytes)
            int         hl_len;    // length of the query in bytes
        };
        std::vector<GlobalResult> global_results;
        bool                      truncated     = false;  // hit MAX_RESULTS
        int                       global_idx    = -1;     // selected result
        double                    global_scroll = 0.0;    // vertical scroll of results list

        FindBar() { anim.opening = false; }

        int current_h() { return (int)(H * anim.progress()); }

        static std::string lower(std::string t) {
            for (auto &c : t) c = (char)std::tolower((unsigned char)c);
            return t;
        }

        void search(const std::string &buf) {
            matches.clear(); match_idx = -1;
            if (query.empty()) return;
            std::string lo_buf = lower(buf), lo_q = lower(query);
            size_t pos = 0;
            while ((pos = lo_buf.find(lo_q, pos)) != std::string::npos) {
                matches.push_back((int)pos); pos += lo_q.size();
            }
            if (!matches.empty()) match_idx = 0;
        }

        // A short one-line version of a matching line, cut around the match
        static void make_display(const std::string &raw, const std::string &lo_q,
                                 std::string &out, int &hl_start) {
            const size_t qlen = lo_q.size();
            std::string line;
            for (char c : raw) {
                if (c == '\t') line += ' ';
                else if ((unsigned char)c >= 0x20 || (c & 0x80)) line += c;   // drop control chars
            }
            size_t lead = line.find_first_not_of(' ');
            if (lead == std::string::npos) lead = 0;
            line.erase(0, lead);
            size_t pos = lower(line).find(lo_q);          // position in the cleaned line
            if (pos == std::string::npos) pos = 0;

            // Start a bit before the match if it is far from the beginning
            size_t start = 0;
            std::string prefix;
            if (pos > 50) {
                start = pos - 30;
                while (start < pos && (line[start] & 0xC0) == 0x80) ++start;
                prefix = "..";
            }
            size_t end = std::min(line.size(), start + 140);
            while (end < line.size() && (line[end] & 0xC0) == 0x80) --end;
            out = prefix + line.substr(start, end - start);
            if (end < line.size()) out += "..";
            hl_start = (int)(prefix.size() + (pos - start));
            if (hl_start + (int)qlen > (int)out.size()) hl_start = -1;   // match was cut off
        }

        // Scan all text files under root for the query
        void search_project(const fs::path &root) {
            global_results.clear(); global_idx = -1; global_scroll = 0.0; truncated = false;
            if (query.empty() || root.empty()) return;
            const std::string lo_q = lower(query);

            static const std::set<std::string> skip_dirs = {
                "build", "node_modules", "target", "dist", "__pycache__"
            };
            static const std::set<std::string> bin_ext = {
                ".png",".jpg",".jpeg",".gif",".bmp",".ico",".mp3",".mp4",".avi",".mkv",
                ".zip",".tar",".gz",".xz",".7z",".rar",".pdf",".o",".so",".a",".exe",
                ".bin",".ttf",".otf",".woff"
            };

            // 1) collect files: skip hidden and build folders
            std::vector<fs::path> files;
            try {
                fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied), end;
                for (; it != end; ++it) {
                    std::string name = it->path().filename().string();
                    if (it->is_directory()) {
                        if ((!name.empty() && name[0] == '.') || skip_dirs.count(name))
                            it.disable_recursion_pending();
                        continue;
                    }
                    if (!it->is_regular_file()) continue;
                    if (bin_ext.count(lower(it->path().extension().string()))) continue;
                    files.push_back(it->path());
                }
            } catch (...) {}
            std::sort(files.begin(), files.end());

            // 2) scan files one by one
            for (const auto &p : files) {
                if ((int)global_results.size() >= MAX_RESULTS) { truncated = true; break; }
                try {
                    if (fs::file_size(p) > 1024 * 1024) continue;
                    std::ifstream f(p, std::ios::binary);
                    if (!f) continue;
                    char head[4096];
                    f.read(head, sizeof(head));
                    if (memchr(head, 0, (size_t)f.gcount())) continue;   // binary file
                    f.clear(); f.seekg(0);

                    std::string rel_dir = fs::relative(p.parent_path(), root).string();
                    if (rel_dir == ".") rel_dir.clear();

                    std::string line; int lno = 0, in_file = 0;
                    while (std::getline(f, line) && in_file < MAX_PER_FILE) {
                        ++lno;
                        if (!line.empty() && line.back() == '\r') line.pop_back();
                        size_t pos = lower(line).find(lo_q);
                        if (pos == std::string::npos) continue;
                        GlobalResult r;
                        r.path = p.string(); r.rel_dir = rel_dir; r.line_no = lno;
                        r.hl_len = (int)lo_q.size();
                        make_display(line, lo_q, r.line_text, r.hl_start);
                        global_results.push_back(std::move(r));
                        ++in_file;
                        if ((int)global_results.size() >= MAX_RESULTS) { truncated = true; break; }
                    }
                } catch (...) {}
            }
            if (!global_results.empty()) global_idx = 0;
        }

        void next() {
            if (mode == 0) { if (!matches.empty()) match_idx = (match_idx+1)%(int)matches.size(); }
            else { if (!global_results.empty()) global_idx = (global_idx+1)%(int)global_results.size(); }
        }
        void prev() {
            if (mode == 0) { if (!matches.empty()) match_idx = (match_idx-1+(int)matches.size())%(int)matches.size(); }
            else { if (!global_results.empty()) global_idx = (global_idx-1+(int)global_results.size())%(int)global_results.size(); }
        }

        void clear() {
            query.clear(); match_idx = -1;
            std::vector<int>().swap(matches);
            global_results.clear(); global_idx = -1; global_scroll = 0.0; truncated = false;
        }

        std::string status() const {
            if (query.empty()) return "";
            if (mode == 0) {
                if (matches.empty()) return "No results";
                return std::to_string(match_idx+1)+" / "+std::to_string(matches.size());
            } else {
                if (global_results.empty()) return "No results";
                return std::to_string(global_results.size())+(truncated ? "+ matches" : " matches");
            }
        }
    } find_bar;

    CodeEditor  code;
    AddonLoader addon_loader;

    // Constructor
    Editor() {
        code.update_line_count();
        sidebar.history.push_back(sidebar.current_path);

        // Addons panel always starts closed
        addons_panel_visible = false;
        btn_addons.active    = false;

        // Ensure ~/.config/textor/ directory tree exists before anything else
        ensure_textor_dirs();

        // Themes: built-ins (minus "Custom") first, then every theme found in .adco files
        auto &th = modal.themes;
        th.erase(std::remove_if(th.begin(), th.end(),
            [](const ThemeVariant &t){ return t.id == "custom"; }), th.end());

        // Load all .adco files (languages + themes). Built-in themes are
        // passed in so a theme can say:  theme "Mine" extends "Midnight" { ... }
        addon_loader.load_all(th);

        // An .adco theme replaces a built-in theme with the same id
        for (auto &t : addon_loader.themes) {
            bool replaced = false;
            for (auto &old : th) if (old.id == t.id) { old = t; replaced = true; }
            if (!replaced) th.push_back(t);
        }
        if (modal.selected_theme_idx >= (int)th.size()) modal.selected_theme_idx = 0;

        modal.card_hover.resize(th.size());
        for (auto &s : modal.card_hover) { s.stiffness=600; s.damping=49; }

        // The settings tab shows the same list
        settings_tab.themes = th;
        if (settings_tab.selected_theme_idx >= (int)settings_tab.themes.size())
            settings_tab.selected_theme_idx = 0;

        settings_tab.shortcuts.load();  // load keybindings from shortcuts.conf

        auto cfg = load_config();
        settings_tab.load_from_map(cfg);
        settings_tab.addon_loader = &addon_loader;  // expose addons to settings UI (non-const for reload)
        settings_tab.on_addon_reload = [this]() {
            // Re-resolve active_addon for the currently open file
            code.update_syn_lang();
        };
        settings_tab.on_show_onboarding = [this]() {
            first_run_done = false;
            auto m = settings_tab.to_map();
            m["first_run_done"] = "0";
            save_config(m);
            sync_onboarding_from_settings();
            onboarding.visible = true;
            onboarding.open_anim.start(true, 0.28);
        };
        modal.load_from_map(cfg);
        apply_settings_from_modal();
        current_theme_variant = settings_tab.get_selected_theme();

        code.scrollbar    = &scrollbar;
        code.modal        = &modal;
        code.theme_tween  = &theme_tween;
        code.addon_loader = &addon_loader;
        code2.scrollbar   = &scrollbar;
        code2.modal       = &modal;
        code2.theme_tween = &theme_tween;
        code2.addon_loader= &addon_loader;
        sidebar.addon_loader = &addon_loader;
        code.autocomplete.enabled = settings_tab.autocomplete_on;
        code.auto_bracket = settings_tab.auto_bracket;
        code.init_autocomplete();

        save_button.label = "";
        save_button.w     = 40;
        save_button.h     = 40;

        // Console starts closed
        console_anim.opening = false;

        // Addons: forcefully close the animation at startup.
        // Without this addons_anim.progress() returns 1.0 (open),
        // and the panel is displayed until the first click.
        addons_panel_visible = false;
        btn_addons.active    = false;
        addons_anim.opening  = false;

        // First-run onboarding
        if (cfg.count("first_run_done")) first_run_done = (cfg.at("first_run_done") == "1");
        if (!first_run_done) {
            sync_onboarding_from_settings();
            onboarding.visible = true;
            onboarding.open_anim.start(true, 0.32);
        }

        // Start with no tabs - show welcome screen
    }

    // Pushes the current real settings into the onboarding window's local
    // copy - used both at startup (if first-run) and when re-armed from
    // Settings ("Show Welcome Screen Again").
    void sync_onboarding_from_settings() {
        onboarding.sync_from_settings(
            settings_tab.show_line_numbers, settings_tab.show_splash,
            settings_tab.autocomplete_on, settings_tab.auto_bracket,
            settings_tab.word_hints_on, settings_tab.inline_hints_on,
            settings_tab.double_click_word_select,
            settings_tab.rounded_corners, settings_tab.animations_enabled,
            settings_tab.smart_render, settings_tab.text_optimized, settings_tab.line_scroll,
            settings_tab.indent_size, settings_tab.gutter_density, settings_tab.line_spacing,
            settings_tab.font_size_pt, settings_tab.fps_max, settings_tab.fps_avg);
    }

    // Applies the onboarding window's edited values to the real settings
    // (same fields the full Settings tab uses) and marks first-run done so
    // it never shows again on its own - see docs/INPUT.md for how it can
    // be brought back from Settings.
    void finish_onboarding() {
        settings_tab.show_line_numbers      = onboarding.show_line_numbers;
        settings_tab.show_splash            = onboarding.show_splash;
        settings_tab.autocomplete_on        = onboarding.autocomplete_on;
        settings_tab.auto_bracket           = onboarding.auto_bracket;
        settings_tab.word_hints_on          = onboarding.word_hints_on;
        settings_tab.inline_hints_on        = onboarding.inline_hints_on;
        settings_tab.double_click_word_select = onboarding.double_click_select;
        settings_tab.rounded_corners        = onboarding.rounded_corners;
        settings_tab.animations_enabled     = onboarding.animations_enabled;
        settings_tab.smart_render           = onboarding.smart_render;
        settings_tab.text_optimized         = onboarding.text_optimized;
        settings_tab.line_scroll            = onboarding.line_scroll;
        settings_tab.indent_size            = (int)std::lround(onboarding.indent_size);
        settings_tab.gutter_density         = onboarding.gutter_density;
        settings_tab.line_spacing           = onboarding.line_spacing;
        settings_tab.font_size_pt           = (int)std::lround(onboarding.font_size_pt);
        settings_tab.fps_max                = (int)std::lround(onboarding.fps_max);
        settings_tab.fps_avg                = (int)std::lround(onboarding.fps_avg);
        code.autocomplete.enabled           = settings_tab.autocomplete_on;
        code.auto_bracket                   = settings_tab.auto_bracket;
        apply_settings_from_modal();

        first_run_done = true;
        onboarding.visible = false;

        auto m = settings_tab.to_map();
        m["first_run_done"] = "1";
        save_config(m);
    }

    // Theme helpers
    void start_theme_transition(const ThemeVariant &target) {
        theme_tween.start(current_theme_variant, target, 0.35);
        current_theme_variant = target;
    }

    void apply_settings_from_modal() {
        code.indent_size    = settings_tab.indent_size;
        code.gutter_density = settings_tab.gutter_density;
        code.line_spacing   = settings_tab.line_spacing;
        code.autocomplete.enabled = settings_tab.autocomplete_on;
        code.auto_bracket = settings_tab.auto_bracket;

        // Sync perf/visual globals from settings
        g_animations_enabled = settings_tab.animations_enabled;
        g_rounded_corners    = settings_tab.rounded_corners;
        g_text_optimized     = settings_tab.text_optimized;
        g_line_scroll        = settings_tab.line_scroll;
        g_smart_render       = settings_tab.smart_render;

        // Suppress any pending auto-scroll-to-cursor so that changing fonts
        // or other settings never teleports the viewport to the cursor line.
        code.auto_scroll_to_cursor = false;

        // Apply font to current theme variant
        ThemeVariant target = settings_tab.get_selected_theme();
        // Override font family and size from settings
        if (settings_tab.font_family_idx >= 0 &&
            settings_tab.font_family_idx < (int)settings_tab.font_families.size())
        {
            target.font_family = settings_tab.font_families[settings_tab.font_family_idx];
        }
        if (settings_tab.font_size_pt >= 8 && settings_tab.font_size_pt <= 24)
            target.font_size = settings_tab.font_size_pt;

        if (target.id != current_theme_variant.id ||
            target.font_family != current_theme_variant.font_family ||
            target.font_size   != current_theme_variant.font_size)
        {
            theme_tween.start(current_theme_variant, target, 0.35);
            current_theme_variant = target;
        }
    }

    void save_current_settings() {
        auto m = settings_tab.to_map();
        save_config(m);
    }

    // Run the search for the current mode (file or all files)
    void run_find_search() {
        if (find_bar.mode == 0) { find_bar.search(code.get_buffer_str()); jump_to_find_match(); }
        else                    { find_bar.search_project(sidebar.current_path); }
    }

    // Open the search panel (mode 0 = current file, 1 = all files)
    void open_find_bar(int mode) {
        if (!find_bar.visible) { find_bar.visible = true; find_bar.anim.start(true, 0.18); }
        find_bar.mode = mode;
        if (code.has_selection()) {
            std::string sel = code.get_selection_text();
            if (sel.find('\n') == std::string::npos) find_bar.query = sel;
        }
        run_find_search();
    }

    void close_find_bar() {
        find_bar.visible = false;
        find_bar.anim.start(false, 0.15);
        find_bar.clear();
    }

    // Open the file of a result and put the cursor on its line
    void open_global_result(int ri) {
        if (ri < 0 || ri >= (int)find_bar.global_results.size()) return;
        const auto gr = find_bar.global_results[ri];
        std::ifstream ifs(gr.path);
        if (ifs) {
            std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
            open_file_in_tab(gr.path, content);
            int target = std::max(0, gr.line_no - 1), off = 0;
            const auto &buf = code.get_buffer_str();
            for (int li = 0; li < target && off < (int)buf.size(); ) {
                if (buf[off] == '\n') ++li;
                ++off;
            }
            code.cursor_pos = off;
            code.auto_scroll_to_cursor = true;
        }
        close_find_bar();
    }

    // Jump the editor viewport and cursor to the current find-bar match
    void jump_to_find_match() {
        if (find_bar.match_idx < 0 || find_bar.matches.empty()) return;
        int off = find_bar.matches[find_bar.match_idx];
        // Move cursor to match start and select the matched text
        int qlen = (int)find_bar.query.size();
        code.cursor_pos = off;
        code.sel_anchor = off + qlen;
        code.sel_active = !find_bar.query.empty();
        code.auto_scroll_to_cursor = true;
    }

    bool save_current_file() {
        // Sync active tab buffer first
        if (!tabs.empty() && active_tab < (int)tabs.size())
            tabs[active_tab].buffer = code.get_buffer_str();
        std::string fname = code.filename.empty() ? "untitled.txt" : code.filename;
        std::ofstream ofs(fname, std::ofstream::trunc);
        if (!ofs) return false;
        ofs << code.buffer.to_string();
        code.dirty = false;
        if (!tabs.empty() && active_tab < (int)tabs.size())
            tabs[active_tab].dirty = false;
        return true;
    }

    // Layout helpers
    int current_sidebar_px() {
        // Returns animated panel width - editor slides right as panel opens
        if (sidebar.visible || sidebar_anim.is_running())
            return (int)(sidebar.width * sidebar_anim.progress());
        if (addons_panel_visible || addons_anim.is_running())
            return (int)(300 * addons_anim.progress());
        return 0;
    }
    // const-safe version: uses last known state without ticking animation
    int current_sidebar_px() const {
        if (sidebar.visible)         return sidebar.width;
        if (addons_panel_visible)    return 300;
        return 0;
    }

    // text_area_x for click/draw: ed_x + gutter_width  (the +15 text-pad is inside CodeEditor)
    int text_area_x_now() {
        return activity_bar_w + current_sidebar_px() + gutter_width;
    }

    int text_area_y_now() { return content_top(); }

    // Height of everything along the bottom: the status bar plus the open terminal.
    // The terminal sits directly above the status bar.
    double console_h_now() {
        return STATUS_H + console.height * console_anim.progress();
    }

    // The status bar only shows information; a click on it does nothing but must not reach the editor.
    bool status_bar_click(int, int y) { return y >= height - STATUS_H; }

    void toggle_terminal() {
        console.visible = !console.visible;
        console_anim.start(console.visible, console.visible ? 0.28 : 0.20);
        btn_console.active = console.visible;
        console_focused    = console.visible;
    }

    // ............ Command palette ............
    // Fills the palette with: open files, themes, commands.
    // Ctrl+P lists the open files first, Ctrl+Shift+P lists the commands first.
    int palette_theme_before = 0;   // theme that was active when the palette opened (for the preview)

    void open_command_palette(bool files_first) {
        std::vector<PaletteItem> files, themes, commands;

        for (int i = 0; i < (int)tabs.size(); i++) {
            std::string name = tabs[i].filename.empty() ? "untitled" : tabs[i].filename;
            auto slash = name.find_last_of("/\\");
            if (slash != std::string::npos) name = name.substr(slash + 1);
            files.push_back({name, "Open file", [this, i]() { switch_to_tab(i); }});
        }
        for (int i = 0; i < (int)settings_tab.themes.size(); i++)
            themes.push_back({"Theme: " + settings_tab.themes[i].name, "Theme",
                              [this, i]() { settings_tab.selected_theme_idx = i; save_current_settings(); },   // Enter: keep it
                              [this, i]() { settings_tab.selected_theme_idx = i; }});                         // highlighted: preview

        auto cmd = [&](const char *name, std::function<void()> fn) { commands.push_back({name, "Command", fn}); };
        cmd("Open File...",         [this]() { file_picker.open(false, sidebar.current_path.empty() ? fs::current_path() : sidebar.current_path); });
        cmd("New File",             [this]() { open_file_in_tab("untitled.sh", "", true); });
        cmd("Save File",            [this]() { save_current_file(); });
        cmd("Close Tab",            [this]() { if (settings_open) close_settings(); else close_tab(active_tab); });
        cmd("Next Tab",             [this]() { if (!tabs.empty()) switch_to_tab((active_tab + 1) % (int)tabs.size()); });
        cmd("Previous Tab",         [this]() { if (!tabs.empty()) switch_to_tab((active_tab - 1 + (int)tabs.size()) % (int)tabs.size()); });
        cmd("Toggle Explorer",      [this]() { toggle_explorer(); });
        cmd("Toggle Addons Panel",  [this]() { toggle_addons_panel(); });
        cmd("Toggle Terminal",      [this]() { toggle_terminal(); });
        cmd("Toggle Split Editor",  [this]() { toggle_split(); });
        cmd("Search in Explorer",   [this]() { open_search_panel(); });
        cmd("Open Settings",        [this]() { if (!settings_open) open_settings_tab(); });
        cmd("Quit Textor",          [this]() { request_quit = true; });

        std::vector<PaletteItem> all;
        auto append = [&](std::vector<PaletteItem> &v) { all.insert(all.end(), v.begin(), v.end()); };
        if (files_first) { append(files); append(commands); append(themes); }
        else             { append(commands); append(files); append(themes); }
        palette_theme_before = settings_tab.selected_theme_idx;
        palette.on_preview_reset = [this]() { settings_tab.selected_theme_idx = palette_theme_before; };
        palette.open(std::move(all));
    }

    // Save button hit
    bool save_button_hit(int mx, int my) const {
        double bx = width - save_button.w - 16;
        double by = 12;
        return mx >= (int)bx && mx <= (int)(bx+save_button.w) &&
               my >= (int)by && my <= (int)(by+save_button.h);
    }

    // Create a temporary Pango layout (for click resolution outside draw)
    struct TmpLayout {
        cairo_surface_t *surf = nullptr;
        cairo_t         *cr   = nullptr;
        PangoLayout     *layout = nullptr;
        PangoFontDescription *fnt = nullptr;

        TmpLayout(const ThemeVariant &tv) {
            surf   = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
            cr     = cairo_create(surf);
            layout = pango_cairo_create_layout(cr);
            std::string spec = tv.font_family + " " + std::to_string(tv.font_size);
            fnt    = pango_font_description_from_string(spec.c_str());
            pango_layout_set_font_description(layout, fnt);
        }
        ~TmpLayout() {
            pango_font_description_free(fnt);
            g_object_unref(layout);
            cairo_destroy(cr);
            cairo_surface_destroy(surf);
        }
    };

    ThemeVariant current_tv() {
        ThemeVariant tv = current_theme_variant;
        if (theme_tween.is_running()) tv = theme_tween.current();
        return tv;
    }

    // Mouse click
    // true when the file in the active pane is Markdown (.md)
    bool active_is_markdown() const {
        const CodeEditor &c = (split.enabled && split_right_pane_active) ? code2 : code;
        return c.syn_lang == SyntaxHighlighter::Lang::Markdown;
    }

    bool lang_pill_hit(int bx, double bw, int mx, int my) const {
        return mx>=bx && mx<=bx+bw && my>=lang_pill_y && my<=lang_pill_y+lang_pill_h;
    }

    void toggle_split() {
        split.enabled = !split.enabled;
        btn_split.active = split.enabled;
        if (split.enabled) {
            // Open the same file in right pane initially
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
    }

    // Viewers button: split the editor, source on the left, the formatted page on the right
    void toggle_md_viewer() {
        if (md_viewer_open) {
            md_viewer_open = false;
            if (split.enabled) toggle_split();                 // close the right pane again
        } else {
            if (!split.enabled) toggle_split();
            md_viewer_open = true;
            code2.scroll_y = 0.0; code2.scroll_vy = 0.0;      // the page starts at its top
        }
    }

    void handle_mouse_click(int x, int y, bool shift_held = false, bool ctrl_held = false);

    // Mouse release
    void handle_mouse_release(int x, int y) {
        onboarding.handle_mouse_up();
        if (modal.visible) {
            bool was = modal.handle_mouse_up(x, y, width, height);
            if (was) { apply_settings_from_modal(); save_current_settings(); }
        }
        settings_tab.handle_mouse_up();
        dragging_v            = false;
        dragging_h            = false;
        dragging_v2           = false;
        text_drag_active      = false;

        split.dragging        = false;
        split.snapped         = false;
        console.handle_mouse_release(0, 0);
    }

    // Mouse scroll
    // Routes a mouse-wheel scroll event to whichever widget is under the
    // pointer (file picker, modal, project-search results, settings tab,
    // sidebar, tab bar, console, or the code editor / split pane). `delta`
    // is in pixels (already sign-adjusted for wheel direction by the
    // caller); `shift` requests horizontal instead of vertical scroll.
    // Kept as a single shared method (instead of inline in the X11 event
    // loop) purely to avoid ~120 lines of duplicated per-widget branching.
    void handle_scroll(int delta, int x, int y, bool shift);

    void handle_mouse_move(int x, int y);


    // Keyboard
    void handle_key(XKeyEvent *kev);

    void handle_key_release(XKeyEvent *kev) {
        if (split.enabled && split_right_pane_active)
            code2.handle_key_release(kev);
        else
            code.handle_key_release(kev);
    }
};

// Out-of-line input-handling method definitions (mouse/keyboard) - see
// docs/INPUT.md and this file's own header comment. Must come after the
// class body above, since it defines Editor::method(...) against it.
#include "editor_input.hxx"

// Out-of-line pieces of draw_ui, one per visual section - see
// draw_context.hxx for why. Must come after struct Editor's full
// definition above, since each one calls real Editor/Sidebar/etc. methods
// (a forward declaration alone isn't enough to compile their bodies).
#include "render/sidebar_overlay_draw.hxx"
#include "render/addons_overlay_draw.hxx"
#include "render/tab_panel_draw.hxx"
#include "render/status_bar_draw.hxx"
#include "render/code_editor_body_draw.hxx"

// Exposed back-buffer (used by GPU blit in base.cxx)
static cairo_surface_t *s_frame_buf   = nullptr;
static int              s_frame_buf_w = 0;
static int              s_frame_buf_h = 0;

// Returns the last rendered CPU frame (ARGB32 image surface).
static inline cairo_surface_t *get_frame_buf() { return s_frame_buf; }

//  draw_ui  -  main compositing function
//  xlib_cr == nullptr -> GPU mode: render to s_frame_buf only, skip Xlib blit
static void draw_ui(cairo_t *xlib_cr, cairo_surface_t *xlib_surf, Editor &ed) {
    // In GPU mode xlib_cr is null; xlib_surf check is only for CPU path
    if (!xlib_cr && xlib_surf) return; // shouldn't happen
    if (xlib_cr && (!xlib_surf)) return;

    // Back-buffer (CPU rendering target)
    if (!s_frame_buf || s_frame_buf_w != ed.width || s_frame_buf_h != ed.height) {
        if (s_frame_buf) cairo_surface_destroy(s_frame_buf);
        s_frame_buf_w = ed.width; s_frame_buf_h = ed.height;
        s_frame_buf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                                  s_frame_buf_w, s_frame_buf_h);
    }
    cairo_surface_t *buf = s_frame_buf;

    ed.apply_settings_from_modal();
    ThemeVariant tv = ed.current_theme_variant;
    if (ed.theme_tween.is_running()) tv = ed.theme_tween.current();

    ed.gutter_width = (int)std::round((double)tv.base_gutter_width * ed.code.gutter_density);

    // Focus Mode: how "revealed" the top-bar chrome currently is (1 = fully
    // shown, 0 = slid off the top edge). Outside focus mode it's always 1.
    double topbar_reveal = ed.focus_mode ? ed.focus_topbar_anim.progress() : 1.0;
    ed.update_chrome();

    cairo_t *cr = cairo_create(buf);
    if (!cr) return;

    // Dirty-rect tracking for GPU partial upload
    ed.dirty_rect_y0 = 0;
    ed.dirty_rect_y1 = ed.height;

    // Clear
    cairo_set_source_rgb(cr, tv.bg, tv.bg, tv.bg);
    cairo_paint(cr);

    // Shared Pango layout
    PangoLayout *layout = pango_cairo_create_layout(cr);
    std::string font_spec = tv.font_family + " " + std::to_string(tv.font_size);
    PangoFontDescription *fnt = pango_font_description_from_string(font_spec.c_str());
    pango_layout_set_font_description(layout, fnt);

    // Compute line height
    pango_layout_set_text(layout, "Ag", -1);
    PangoRectangle ink, logical;
    pango_layout_get_extents(layout, &ink, &logical);
    double base_line_h = (double)logical.height / PANGO_SCALE;
    if (base_line_h <= 0)
        base_line_h = (double)pango_font_description_get_size(fnt) / PANGO_SCALE * 1.2;
    int line_height = (int)std::round(base_line_h * std::max(0.5, ed.code.line_spacing));

    // Console height
    double con_h     = ed.console_h_now();
    double eff_height = (double)ed.height - con_h - ed.content_top();

    // Bundles the state every extracted render/*.hxx function needs
    // see draw_context.hxx for why this exists instead of five separate
    // parameters everywhere.
    DrawContext dc{cr, ed, tv, layout, fnt, con_h, eff_height, topbar_reveal, line_height};
    dc.xlib_cr = xlib_cr;

    // Position dynamic buttons
    ed.position_topbar_buttons();

    // Tick hover animations
    ed.btn_sidebar.active = ed.sidebar.visible;
    ed.btn_sidebar.tick(ed.btn_sidebar.hovered);
    ed.btn_addons.tick(ed.btn_addons.hovered);
    ed.btn_split.tick(ed.btn_split.hovered);
    ed.btn_exit.tick(ed.btn_exit.hovered);
    ed.btn_settings.tick(ed.btn_settings.hovered);
    ed.btn_console.tick(ed.btn_console.hovered);
    if (ed.md_viewer_open && (!ed.split.enabled || ed.code.syn_lang != SyntaxHighlighter::Lang::Markdown)) {
        ed.md_viewer_open = false;                         // split closed by hand, or another file type opened
        if (ed.split.enabled) ed.toggle_split();
    }
    if (ed.md_viewer_open) { ed.split.active_pane = 0; ed.split_right_pane_active = false; }  // the page is read-only
    ed.btn_viewers.active = ed.md_viewer_open;
    ed.btn_viewers.tick(ed.btn_viewers.hovered);

    // Tick split editor
    ed.split.tick();

    // Tick file picker physics scroll
    ed.file_picker.tick(0.016);

    // Shared "toolbar pill" helpers
    // Used by the Explorer toggle (top bar) and Split/Addons/Console
    // (second bar) so all four buttons share one consistent shape: icon-only
    // while off, grows to show a label when active. Text sits in a fixed
    // 34px zone after the icon; the pill width now reserves an equal-feeling
    // pad on the right (was 2px - the old formula forgot the icon zone had
    // grown from 22px to 34px, so text used to crowd the right edge)*_-
    const double PILL_COLLAPSED_W = 36.0, PILL_GAP = 8.0;
    const double PILL_TEXT_X = 34.0, PILL_RIGHT_PAD = 16.0;

    auto expanded_w_for = [&](const char *label) -> double {
        PangoFontDescription *sf=pango_font_description_from_string((tv.font_family+" Bold 11").c_str());
        pango_layout_set_font_description(layout,sf);
        pango_layout_set_attributes(layout, nullptr);
        pango_layout_set_text(layout,label,-1);
        int lw,lh; pango_layout_get_pixel_size(layout,&lw,&lh);
        pango_font_description_free(sf);
        return PILL_TEXT_X + lw + PILL_RIGHT_PAD; // icon zone + label + even right pad
    };

    auto draw_pill = [&](double px, double by_, double pw, double bh_, double br_,
                          bool active, bool hovered_, const char *label, const void *anim_key,
                          auto draw_icon) {
        double fill_a = active ? 0.90 : (hovered_ ? 0.18 : 0.10);
        if (active) cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, fill_a);
        else        cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, fill_a);
        morph::rrect(cr, px, by_, pw, bh_, br_); cairo_fill(cr);

        double icx = px + 19.0, icy = by_ + bh_/2.0;
        double ir = active ? tv.bg : tv.text, ig = active ? tv.bg : tv.text, ib = active ? tv.bg : tv.text;
        if (!active) { ir=tv.muted; ig=tv.muted; ib=tv.muted; }
        {
            // Small "pop" on click - only wraps the glyph itself, not the
            // pill background/label, and only costs anything while a pop
            // triggered by icon_anim::trigger() is actually mid-flight.
            icon_anim::Scope pop(cr, anim_key, icx, icy);
            draw_icon(icx, icy, ir, ig, ib);
        }

        if (pw > PILL_COLLAPSED_W + 8.0) {
            PangoFontDescription *sf=pango_font_description_from_string((tv.font_family+" Bold 11").c_str());
            pango_layout_set_font_description(layout,sf);
            pango_layout_set_attributes(layout, nullptr);
            pango_layout_set_text(layout,label,-1);
            cairo_set_source_rgba(cr, active?tv.bg:tv.text, active?tv.bg:tv.text, active?tv.bg:tv.text, active?1.0:0.85);
            int lw,lh; pango_layout_get_pixel_size(layout,&lw,&lh);
            cairo_move_to(cr, px+PILL_TEXT_X, by_+(bh_-lh)/2.0);
            pango_cairo_show_layout(cr,layout);
            pango_font_description_free(sf);
        }
    };

    // Explorer toggle - same pill shape/behavior as Split/Addons/Console
    auto draw_explorer_pill = [&](double bar_x0) {
        int icon_sz = 22;
        double by_ = (ed.TAB_BAR_H - icon_sz) / 2.0 - 1.0, bh_ = (double)icon_sz + 2.0, br_ = 6.0;
        double expl_ew = expanded_w_for("Explorer");
        double expl_w  = PILL_COLLAPSED_W + (expl_ew - PILL_COLLAPSED_W) * ed.btn_sidebar.anim_active.value;
        double px = bar_x0 + 8.0;

        draw_pill(px, by_, expl_w, bh_, br_, ed.sidebar.visible, ed.btn_sidebar.hovered, "Explorer",
            &ed.btn_sidebar,
            [&](double icx,double icy,double r,double g,double b){
                morph::draw_icon(cr, morph::Icon::Sidebar, icx, icy, (double)icon_sz, r, g, b);
            });

        ed.btn_sidebar.x = (int)px;
        ed.sidebar_pill_w = expl_w;
        ed.topbar_pill_y  = by_;
        ed.topbar_pill_h  = bh_;
        ed.TOPBAR_LEFT_RESERVE = px + expl_w + 14.0 - bar_x0;
    };


    // Centered toolbar group (Split / Addons / Console + language)
    // Previously a second bar below the tab strip; now merged into the
    // single top bar and centered in the middle of it, between the
    // Explorer pill (left) and Settings/Exit icons (right). Captured as a
    // lambda so it can be invoked once the shared top-bar background has
    // actually been painted (welcome screen vs. normal tab view use
    // different top-bar draw paths below).
    auto draw_center_toolbar = [&]() {
        const int bar_x0 = 0;
        double by = 5.0, bh = (double)ed.TAB_BAR_H - 10.0, br = 7.0;

        double split_ew   = expanded_w_for("Split");
        double addons_ew  = expanded_w_for("Addons");
        double console_ew = expanded_w_for("Console");

        double split_w   = PILL_COLLAPSED_W + (split_ew  - PILL_COLLAPSED_W) * ed.btn_split.anim_active.value;
        double addons_w  = PILL_COLLAPSED_W + (addons_ew - PILL_COLLAPSED_W) * ed.btn_addons.anim_active.value;
        double console_w = PILL_COLLAPSED_W + (console_ew- PILL_COLLAPSED_W) * ed.btn_console.anim_active.value;

        auto &active_code = (ed.split.enabled && ed.split_right_pane_active) ? ed.code2 : ed.code;
        const char *lname = active_code.language_name();
        double lang_w = expanded_w_for(lname) - PILL_TEXT_X + 18.0; // no icon, tighter pad

        // Viewers: between Console and the language pill, only for .md files
        const bool show_viewers = ed.active_is_markdown();
        double viewers_ew = expanded_w_for("Viewers");
        double viewers_w  = show_viewers
            ? PILL_COLLAPSED_W + (viewers_ew - PILL_COLLAPSED_W) * ed.btn_viewers.anim_active.value
            : 0.0;

        // Centre the whole group between the Explorer divider and the
        // Settings/Exit icons on the right.
        double group_w  = split_w + PILL_GAP + addons_w + PILL_GAP + console_w + PILL_GAP
                        + (show_viewers ? viewers_w + PILL_GAP : 0.0) + lang_w;
        double avail_x0 = bar_x0 + ed.TOPBAR_LEFT_RESERVE;
        double avail_x1 = ed.width - ed.TOPBAR_RIGHT_RESERVE;
        double sx = avail_x0 + std::max(0.0, ((avail_x1 - avail_x0) - group_w) / 2.0);

        ed.lang_pill_y = by; ed.lang_pill_h = bh;

        draw_pill(sx, by, split_w, bh, br, ed.split.enabled, ed.btn_split.hovered, "Split",
            &ed.btn_split,
            [&](double icx,double icy,double r,double g,double b){
                morph::draw_icon(cr, morph::Icon::Split, icx, icy, 22.0, r, g, b);
            });
        ed.btn_split.x=(int)sx; ed.lang_pill_split_w = split_w;
        sx += split_w + PILL_GAP;

        draw_pill(sx, by, addons_w, bh, br, ed.addons_panel_visible, ed.btn_addons.hovered, "Addons",
            &ed.btn_addons,
            [&](double icx,double icy,double r,double g,double b){
                morph::draw_icon(cr, morph::Icon::Addons, icx, icy, 22.0, r, g, b);
            });
        ed.btn_addons.x=(int)sx; ed.lang_pill_addons_w = addons_w;
        sx += addons_w + PILL_GAP;

        draw_pill(sx, by, console_w, bh, br, ed.console.visible, ed.btn_console.hovered, "Console",
            &ed.btn_console,
            [&](double icx,double icy,double r,double g,double b){
                morph::draw_icon(cr, morph::Icon::Console, icx, icy, 22.0, r, g, b);
            });
        ed.btn_console.x=(int)sx; ed.lang_pill_console_w = console_w;
        sx += console_w + PILL_GAP;

        ed.lang_pill_viewers_w = viewers_w;
        if (show_viewers) {
            draw_pill(sx, by, viewers_w, bh, br, ed.md_viewer_open, ed.btn_viewers.hovered, "Viewers",
                &ed.btn_viewers,
                [&](double icx,double icy,double r,double g,double b){
                    morph::draw_icon(cr, morph::Icon::Viewers, icx, icy, 22.0, r, g, b);
                });
            ed.btn_viewers.x = (int)sx;
            sx += viewers_w + PILL_GAP;
        }

        // Language pill - display-only, same visual family
        {
            cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, 0.10);
            morph::rrect(cr, sx, by, lang_w, bh, br); cairo_fill(cr);
            PangoFontDescription *sf=pango_font_description_from_string((tv.font_family+" 10").c_str());
            pango_layout_set_font_description(layout,sf);
            pango_layout_set_attributes(layout, nullptr);
            pango_layout_set_text(layout,lname,-1);
            int lw,lh; pango_layout_get_pixel_size(layout,&lw,&lh);
            cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,0.9);
            cairo_move_to(cr, sx+(lang_w-lw)/2.0, by+(bh-lh)/2.0);
            pango_cairo_show_layout(cr,layout);
            pango_font_description_free(sf);
        }
    };

    // Editor area shifts right as sidebar/addons panel slides open
    int side_x = ed.activity_bar_w;
    int ed_x   = side_x + ed.current_sidebar_px();
    dc.ed_x    = ed_x;

    draw_sidebar_overlay(dc);
    draw_addons_overlay(dc);

    // Top bar chrome
    // Always drawn - with tabs open or on the welcome screen - so Explorer /
    // Split-Addons-Console / Settings / Exit stay reachable either way.
    // (Previously duplicated verbatim in both the "no tabs" and "tabs open"
    // branches below; a change to one copy but not the other was an easy way
    // to make the welcome screen's top bar quietly drift out of sync with
    // the normal one. Drawing it once here removes that whole failure mode.)
    auto draw_top_bar_chrome = [&]() {
        cairo_save(cr);
        cairo_translate(cr, 0, -ed.chrome_shift());
        const int bar_x0 = 0; // global bar - starts at the true left edge

        // Flat surface color, no gradient.
        double tbg = tv.bg * 0.95 + tv.elevated * 0.05;
        cairo_set_source_rgb(cr, tbg, tbg, tbg);
        cairo_rectangle(cr, bar_x0, 0, ed.width - bar_x0, ed.TAB_BAR_H);
        cairo_fill(cr);

        // Bottom border - thin hairline (outline variant)
        cairo_set_source_rgba(cr, 0, 0, 0, 0.09);
        cairo_rectangle(cr, bar_x0, ed.TAB_BAR_H - 1, ed.width - bar_x0, 1);
        cairo_fill(cr);

        // Explorer toggle + divider - left edge of the top bar
        draw_explorer_pill(bar_x0);
        {
            double dvx = bar_x0 + ed.TOPBAR_LEFT_RESERVE - 6.0;
            cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, 0.18);
            cairo_rectangle(cr, dvx, 7, 1, ed.TAB_BAR_H - 14);
            cairo_fill(cr);
        }

        // Settings + exit - right edge of the top bar
        {
            double scx = ed.btn_settings.x + ed.btn_settings.size / 2.0;
            double scy = ed.btn_settings.y + ed.btn_settings.size / 2.0;
            icon_anim::Scope pop(cr, &ed.btn_settings, scx, scy);
            ed.btn_settings.draw_minimal(cr, tv, "settings", ed.settings_open);
        }
        {
            double ecx = ed.btn_exit.x + ed.btn_exit.size / 2.0;
            double ecy = ed.btn_exit.y + ed.btn_exit.size / 2.0;
            icon_anim::Scope pop(cr, &ed.btn_exit, ecx, ecy);
            ed.btn_exit.draw_minimal(cr, tv, "exit");
        }

        // Split / Addons / Console + language - centered in the middle
        draw_center_toolbar();
        cairo_restore(cr);
    };
    draw_top_bar_chrome();

    // Editor body: welcome screen (no tabs open) or the code editor itself
    if (ed.tabs.empty() && !ed.settings_open) {
        int global_top2 = ed.content_top();
        // Fill editor area with bg
        cairo_set_source_rgb(cr, tv.bg, tv.bg, tv.bg);
        cairo_rectangle(cr, ed_x, global_top2, ed.width - ed_x, ed.height - global_top2);
        cairo_fill(cr);

        // Center point
        double cx2 = ed_x + (ed.width - ed_x) / 2.0;
        double cy2 = global_top2 + (ed.height - con_h - global_top2) / 2.0;

        // Logo / icon - folder outline
        {
            cairo_save(cr);
            double fx = cx2 - 28, fy = cy2 - 80, fw = 56, fh = 42;
            // Folder tab
            cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.22);
            cairo_new_path(cr);
            cairo_move_to(cr, fx, fy + 10);
            cairo_line_to(cr, fx + fw * 0.4, fy + 10);
            cairo_line_to(cr, fx + fw * 0.52, fy);
            cairo_line_to(cr, fx + fw, fy);
            cairo_line_to(cr, fx + fw, fy + fh);
            cairo_line_to(cr, fx, fy + fh);
            cairo_close_path(cr);
            cairo_fill(cr);
            cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.70);
            cairo_set_line_width(cr, 1.5);
            cairo_new_path(cr);
            cairo_move_to(cr, fx, fy + 10);
            cairo_line_to(cr, fx + fw * 0.4, fy + 10);
            cairo_line_to(cr, fx + fw * 0.52, fy);
            cairo_line_to(cr, fx + fw, fy);
            cairo_line_to(cr, fx + fw, fy + fh);
            cairo_line_to(cr, fx, fy + fh);
            cairo_close_path(cr);
            cairo_stroke(cr);
            // inner lines
            cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.40);
            for (int li = 0; li < 3; li++) {
                double lx1 = fx + 10, lx2 = fx + fw - 10;
                double ly = fy + 20 + li * 7;
                double len_frac = li == 2 ? 0.55 : 1.0;
                cairo_move_to(cr, lx1, ly);
                cairo_line_to(cr, lx1 + (lx2 - lx1) * len_frac, ly);
                cairo_stroke(cr);
            }
            cairo_restore(cr);
        }

        // Title
        {
            PangoFontDescription *tf = pango_font_description_copy(fnt);
            pango_font_description_set_size(tf, (tv.font_size + 4) * PANGO_SCALE);
            pango_font_description_set_weight(tf, PANGO_WEIGHT_SEMIBOLD);
            pango_layout_set_font_description(layout, tf);
            pango_layout_set_text(layout, "No file open", -1);
            int tw2, th2; pango_layout_get_pixel_size(layout, &tw2, &th2);
            cairo_set_source_rgb(cr, tv.text, tv.text, tv.text);
            cairo_move_to(cr, cx2 - tw2 / 2.0, cy2 - 28);
            pango_cairo_show_layout(cr, layout);
            pango_font_description_free(tf);
        }
        // Subtitle
        {
            PangoFontDescription *sf2 = pango_font_description_copy(fnt);
            pango_font_description_set_size(sf2, (int)(tv.font_size * 0.88) * PANGO_SCALE);
            pango_layout_set_font_description(layout, sf2);
            pango_layout_set_text(layout, "Open a folder or file to get started", -1);
            int tw2, th2; pango_layout_get_pixel_size(layout, &tw2, &th2);
            cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, 0.65);
            cairo_move_to(cr, cx2 - tw2 / 2.0, cy2 - 4);
            pango_cairo_show_layout(cr, layout);
            pango_font_description_free(sf2);
            pango_layout_set_font_description(layout, fnt);
        }

        // Two buttons
        const int BTN_W = 180, BTN_H = 40, BTN_GAP = 14;
        int b1x = (int)(cx2 - BTN_W - BTN_GAP / 2);
        int b2x = (int)(cx2 + BTN_GAP / 2);
        int by  = (int)(cy2 + 28);

        ed.welcome_btn_folder = {b1x, by, BTN_W, BTN_H};
        ed.welcome_btn_file   = {b2x, by, BTN_W, BTN_H};

        auto draw_btn = [&](int bx, int bby, bool hov, bool primary,
                            const char *label, double ir, double ig, double ib) {
            if (primary) {
                cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, hov ? 0.95 : 0.82);
                // rounded rect fill
                double r = 8.0;
                shape::rrect(cr, bx, bby, BTN_W, BTN_H, r); cairo_fill(cr);
            } else {
                double r = 8.0;
                cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, hov ? 0.15 : 0.08);
                shape::rrect(cr, bx, bby, BTN_W, BTN_H, r); cairo_fill(cr);
                cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, hov ? 0.35 : 0.20);
                shape::rrect(cr, bx, bby, BTN_W, BTN_H, r);
                cairo_set_line_width(cr, 1.0); cairo_stroke(cr);
            }
            PangoFontDescription *bf2 = pango_font_description_copy(fnt);
            pango_font_description_set_size(bf2, (int)(tv.font_size * 0.92) * PANGO_SCALE);
            pango_font_description_set_weight(bf2, PANGO_WEIGHT_MEDIUM);
            pango_layout_set_font_description(layout, bf2);
            pango_layout_set_text(layout, label, -1);
            int lw2, lh2; pango_layout_get_pixel_size(layout, &lw2, &lh2);
            if (primary) cairo_set_source_rgba(cr, 0.06, 0.06, 0.06, 0.95);
            else         cairo_set_source_rgba(cr, ir, ig, ib, 0.9);
            cairo_move_to(cr, bx + (BTN_W - lw2) / 2.0, bby + (BTN_H - lh2) / 2.0);
            pango_cairo_show_layout(cr, layout);
            pango_font_description_free(bf2);
            pango_layout_set_font_description(layout, fnt);
        };

        draw_btn(b1x, by, ed.welcome_btn_hovered == 0, true,
                 "Open Folder", tv.accent_r, tv.accent_g, tv.accent_b);
        draw_btn(b2x, by, ed.welcome_btn_hovered == 1, false,
                 "Open File", tv.text, tv.text, tv.text);

        // Keyboard hint
        {
            PangoFontDescription *hf = pango_font_description_copy(fnt);
            pango_font_description_set_size(hf, (int)(tv.font_size * 0.78) * PANGO_SCALE);
            pango_layout_set_font_description(layout, hf);
            pango_layout_set_text(layout, "or browse files in the sidebar  <-", -1);
            int hw2, hh2; pango_layout_get_pixel_size(layout, &hw2, &hh2);
            cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, 0.38);
            cairo_move_to(cr, cx2 - hw2 / 2.0, by + BTN_H + 18);
            pango_cairo_show_layout(cr, layout);
            pango_font_description_free(hf);
            pango_layout_set_font_description(layout, fnt);
        }
    } else {
        // Top bar chrome for this branch is already drawn once, above
        // (draw_top_bar_chrome()) - this branch only needs the code
        // editor itself: gutter, text area, scrollbars, find bar.

    draw_code_editor_body(dc);
    } // end else

    draw_tab_strip(dc);
    // File picker overlay
    if (ed.file_picker.visible || ed.file_picker.anim.is_running()) {
        ed.file_picker.draw(cr, ed.width, ed.height, tv);
    }

    // Console panel (PTY terminal)
    double con_p = ed.console_anim.progress();
    if (ed.console.visible || ed.console_anim.is_running()) {
        if (!ed.console_anim.is_running() && con_p <= 0.0) {
            ed.console.visible = false;
        } else {
            int lx  = ed.activity_bar_w;
            int ch2 = (int)(ed.console.height * con_p);
            int cy2 = ed.height - ed.STATUS_H - ch2;
            int cw2 = ed.width - lx;
            ed.console.tick(0.016);   // inertial scroll physics
            if (ch2 > 4 && cw2 > 10) {
                ed.console.draw(cr, layout, fnt, tv, lx, cy2, cw2, ch2);
            }
        }
    }

    // Status bar - bottom edge, below the terminal
    draw_status_bar(dc);

    // Command palette - above everything except onboarding
    if (ed.palette.visible) ed.palette.draw(cr, layout, fnt, ed.width, ed.height, tv);

    // First-run onboarding - drawn last, on top of everything else
    ed.onboarding.draw(cr, ed.width, ed.height, layout, fnt, tv);

    // Composite to screen (CPU path only; GPU path reads s_frame_buf directly)
    cairo_destroy(cr);
    if (xlib_cr) {
        cairo_set_source_surface(xlib_cr, buf, 0, 0);
        cairo_paint(xlib_cr);
    }

    pango_font_description_free(fnt);
    g_object_unref(layout);
}

#endif
