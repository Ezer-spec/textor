#ifndef COMMAND_PALETTE_HXX
#define COMMAND_PALETTE_HXX

// Command palette (Ctrl+Shift+P / Ctrl+P).
//
// A box in the top-middle of the window: type to filter, Up/Down to choose,
// Enter to run, Esc to close.  The Editor fills `items` (open files, themes,
// commands) every time the palette opens - see Editor::open_command_palette().

#include <string>
#include "../base/keytext.hxx"
#include <vector>
#include <functional>
#include <algorithm>
#include <cmath>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include "../color/themes.hxx"

struct PaletteItem {
    std::string label;             // main text, e.g. "Toggle Terminal"
    std::string hint;              // grey text on the right, e.g. "Command", "Theme", "Open file"
    std::function<void()> run;     // what happens on Enter
    std::function<void()> preview; // optional: runs while the item is only highlighted (theme preview)
};

struct CommandPalette {
    bool visible = false;
    std::string query;
    std::vector<PaletteItem> items;
    std::vector<int> shown;        // indices into `items` that match the query
    int selected = 0;

    // Called when the highlighted item has no preview (or the palette is cancelled):
    // the Editor uses it to put the previous theme back.
    std::function<void()> on_preview_reset;

    // Layout (pixels)
    static constexpr int WIDTH = 560, ROW_H = 34, INPUT_H = 46, MAX_ROWS = 10, TOP = 70;

    void open(std::vector<PaletteItem> new_items) {
        items = std::move(new_items);
        query.clear();
        visible = true;
        refilter();
    }
    void close() { visible = false; }

    // Esc / click outside: undo any preview, then close.
    void cancel() {
        if (on_preview_reset) on_preview_reset();
        close();
    }

    // Shows the preview of the highlighted item (e.g. a theme), or undoes the previous preview.
    void update_preview() {
        if (selected >= 0 && selected < (int)shown.size() && items[shown[selected]].preview)
            items[shown[selected]].preview();
        else if (on_preview_reset)
            on_preview_reset();
    }

    // Case-insensitive: every typed letter must appear in the label, in order ("tgt" matches "Toggle Terminal").
    static bool matches(const std::string &label, const std::string &q) {
        size_t pos = 0;
        for (char qc : q) {
            qc = (char)std::tolower((unsigned char)qc);
            bool found = false;
            while (pos < label.size()) {
                if (std::tolower((unsigned char)label[pos++]) == qc) { found = true; break; }
            }
            if (!found) return false;
        }
        return true;
    }

    static std::string lower(std::string s) {
        for (auto &c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    }

    // Exact (substring) matches come first, loose letter-by-letter matches after them.
    void refilter() {
        shown.clear();
        std::string q = lower(query);
        for (int i = 0; i < (int)items.size(); i++)
            if (lower(items[i].label).find(q) != std::string::npos) shown.push_back(i);
        for (int i = 0; i < (int)items.size(); i++)
            if (lower(items[i].label).find(q) == std::string::npos && matches(items[i].label, query)) shown.push_back(i);
        selected = 0;
        update_preview();
    }

    // Runs the chosen item. The palette closes first so the command can open other panels.
    void run_selected() {
        if (selected < 0 || selected >= (int)shown.size()) return;
        const PaletteItem &it = items[shown[selected]];
        auto run = it.run;
        if (!it.preview && on_preview_reset) on_preview_reset();   // a command: drop the preview of the last theme
        close();
        if (run) run();
    }

    // Returns true when the key was used (the palette swallows every key while open).
    bool handle_key(XKeyEvent *kev) {
        if (!visible) return false;
        char buf[32]{}; KeySym ks;
        int len = textor_lookup_key(kev, buf, sizeof(buf), &ks);
        bool ctrl = (kev->state & ControlMask) != 0;

        if (ks == XK_Escape) { cancel(); return true; }
        if (ks == XK_Return || ks == XK_KP_Enter) { run_selected(); return true; }
        if (ks == XK_Up)   { if (selected > 0) { selected--; update_preview(); } return true; }
        if (ks == XK_Down) { if (selected + 1 < (int)shown.size()) { selected++; update_preview(); } return true; }
        if (ks == XK_BackSpace) {
            // remove the last UTF-8 character
            while (!query.empty() && ((unsigned char)query.back() & 0xC0) == 0x80) query.pop_back();
            if (!query.empty()) query.pop_back();
            refilter();
            return true;
        }
        if (!ctrl && len > 0 && (unsigned char)buf[0] >= 0x20) { query.append(buf, len); refilter(); }
        return true;
    }

    // Box position and size for a window of win_w x win_h.
    void rect(int win_w, int win_h, int &x, int &y, int &w, int &h) const {
        int rows = std::min((int)shown.size(), MAX_ROWS);
        w = std::min(WIDTH, win_w - 40);
        h = INPUT_H + rows * ROW_H + (rows > 0 ? 8 : 0);
        x = (win_w - w) / 2;
        y = std::min(TOP, std::max(10, win_h / 8));
    }

    // Click on a row runs it; a click outside the box closes the palette.
    void handle_click(int mx, int my, int win_w, int win_h) {
        int x, y, w, h; rect(win_w, win_h, x, y, w, h);
        if (mx < x || mx > x + w || my < y || my > y + h) { cancel(); return; }
        int row = (my - (y + INPUT_H + 4)) / ROW_H;
        if (my >= y + INPUT_H + 4 && row >= 0 && row < std::min((int)shown.size(), MAX_ROWS)) {
            selected = row;
            run_selected();
        }
    }

    void handle_move(int mx, int my, int win_w, int win_h) {
        int x, y, w, h; rect(win_w, win_h, x, y, w, h);
        if (mx < x || mx > x + w) return;
        int row = (my - (y + INPUT_H + 4)) / ROW_H;
        if (my >= y + INPUT_H + 4 && row >= 0 && row < std::min((int)shown.size(), MAX_ROWS) && row != selected) {
            selected = row;
            update_preview();
        }
    }

    static void rrect(cairo_t *cr, double x, double y, double w, double h, double r) {
        shape::rrect(cr, x, y, w, h, r);
    }

    void draw(cairo_t *cr, PangoLayout *layout, PangoFontDescription *fnt,
              int win_w, int win_h, const ThemeVariant &tv) {
        if (!visible) return;
        int x, y, w, h; rect(win_w, win_h, x, y, w, h);

        // Dim everything behind the palette
        cairo_set_source_rgba(cr, 0, 0, 0, 0.35);
        cairo_rectangle(cr, 0, 0, win_w, win_h);
        cairo_fill(cr);

        // Box with a thin border
        double bg = tv.elevated;
        rrect(cr, x, y, w, h, 10);
        cairo_set_source_rgb(cr, bg, bg, bg);
        cairo_fill_preserve(cr);
        cairo_set_source_rgba(cr, 1, 1, 1, 0.10);
        cairo_set_line_width(cr, 1);
        cairo_stroke(cr);

        pango_layout_set_font_description(layout, fnt);
        auto text_at = [&](const std::string &s, double tx, double ty, double r, double g, double b) {
            pango_layout_set_text(layout, s.c_str(), -1);
            int lw, lh; pango_layout_get_pixel_size(layout, &lw, &lh);
            cairo_set_source_rgb(cr, r, g, b);
            cairo_move_to(cr, tx, ty - lh / 2.0);
            pango_cairo_show_layout(cr, layout);
            return lw;
        };

        // Input line: typed text or a grey placeholder, plus a blinking-free caret
        double iy = y + INPUT_H / 2.0;
        if (query.empty()) {
            text_at("Type a command, theme or open file name...", x + 18, iy, tv.muted, tv.muted, tv.muted);
        } else {
            int tw = text_at(query, x + 18, iy, tv.text, tv.text, tv.text);
            cairo_set_source_rgb(cr, tv.accent_r, tv.accent_g, tv.accent_b);
            cairo_rectangle(cr, x + 20 + tw, iy - 9, 1.5, 18);
            cairo_fill(cr);
        }
        cairo_set_source_rgba(cr, 1, 1, 1, 0.08);
        cairo_rectangle(cr, x, y + INPUT_H, w, 1);
        cairo_fill(cr);

        // Result rows
        int rows = std::min((int)shown.size(), MAX_ROWS);
        for (int r = 0; r < rows; r++) {
            const PaletteItem &it = items[shown[r]];
            double ry = y + INPUT_H + 4 + r * ROW_H;
            if (r == selected) {
                rrect(cr, x + 6, ry, w - 12, ROW_H - 2, 6);
                cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.20);
                cairo_fill(cr);
            }
            text_at(it.label, x + 18, ry + ROW_H / 2.0 - 1, tv.text, tv.text, tv.text);
            pango_layout_set_text(layout, it.hint.c_str(), -1);
            int hw, hh; pango_layout_get_pixel_size(layout, &hw, &hh);
            text_at(it.hint, x + w - 18 - hw, ry + ROW_H / 2.0 - 1, tv.muted, tv.muted, tv.muted);
        }
        if (shown.empty()) {
            // (the box is only INPUT_H high with no rows, nothing else to draw)
        }
    }
};

#endif
