#ifndef AUTOCOMPLETE_HXX
#define AUTOCOMPLETE_HXX

//  Autocomplete - floating completion popup for IDE
//
//  Shows a popup below the cursor with filtered suggestions
//  Navigation: Up/Down arrows, Tab/Enter to accept, Escape to dismiss
//  Triggered automatically after typing identifiers
//  Blah blah blah. . .

#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include "../color/themes.hxx"

struct Autocomplete {
    // State
    bool enabled  = false;   // user-controlled setting
    bool visible  = false;

    std::string prefix;                  // current word being typed
    std::vector<std::string> all_items;  // full completion pool
    std::vector<std::string> filtered;   // filtered by prefix
    int selected = 0;
    int scroll   = 0;
    int anchor_pos = -1;                 // cursor offset the popup belongs to (-1 = not set yet)

    // Minimum characters typed before the popup is allowed to appear
    // Lowered to 1 so suggestions react immediately on the very first
    // keystroke instead of waiting for a 2nd character
    size_t min_prefix_len = 1;

    // Popup geometry
    double popup_x = 0, popup_y = 0;
    const int MAX_VISIBLE = 8;
    const int ITEM_H = 22;
    const int POPUP_W = 260;

    // Animation
    float anim = 0.0f;

    // ............ Built-in D completions ............
    static const std::vector<std::string>& d_builtins() {
        static std::vector<std::string> s {
            // Phobos / runtime - frequently used
            "writeln", "writef", "writefln", "write", "readln",
            "stdin", "stdout", "stderr", "printf", "scanf",
            "to", "format", "formattedWrite",
            "enforce", "assumeUnique", "assumeSafeAppend",
            "sort", "filter", "map", "reduce", "fold", "each",
            "find", "findSplit", "findAmong", "canFind",
            "array", "assocArray", "iota", "zip", "chain",
            "joiner", "splitter", "split", "join", "strip",
            "startsWith", "endsWith", "indexOf", "lastIndexOf",
            "replace", "replaceAll", "toLower", "toUpper",
            "isAlpha", "isDigit", "isAlphaNum", "isWhite",
            "min", "max", "abs", "sqrt", "pow", "log", "log2", "log10",
            "floor", "ceil", "round", "fmod", "isNaN", "isInfinity",
            "PI", "E", "SQRT2", "LN2",
            "File", "Dir", "read", "readText", "write", "append",
            "exists", "remove", "rename", "mkdir", "mkdirRecurse",
            "dirEntries", "buildPath", "baseName", "dirName",
            "Thread", "Fiber", "spawn", "thisTid", "ownerTid",
            "send", "receive", "receiveOnly", "receiveTimeout",
            "Mutex", "Condition", "Monitor",
            "RegexException", "regex", "match", "matchFirst", "matchAll",
            "Appender", "appender", "RefCounted", "Tuple", "tuple",
            "Duration", "dur", "seconds", "minutes", "hours",
            "msecs", "usecs", "nsecs", "Clock", "MonoTime",
            // Keywords (also useful as completions)
            "abstract", "alias", "align", "asm", "assert", "auto",
            "body", "break", "case", "cast", "catch", "class", "const",
            "continue", "debug", "default", "delegate", "delete",
            "deprecated", "do", "else", "enum", "export", "extern",
            "false", "final", "finally", "for", "foreach",
            "foreach_reverse", "function", "goto", "if", "immutable",
            "import", "in", "inout", "interface", "invariant", "is",
            "lazy", "mixin", "module", "new", "nothrow", "null", "out",
            "override", "package", "pragma", "private", "protected",
            "public", "pure", "ref", "return", "scope", "shared",
            "static", "struct", "super", "switch", "synchronized",
            "template", "this", "throw", "true", "try", "typedef",
            "typeid", "typeof", "union", "unittest", "version", "void",
            "volatile", "while", "with",
            // Types
            "bool", "byte", "ubyte", "short", "ushort", "int", "uint",
            "long", "ulong", "cent", "ucent", "float", "double", "real",
            "char", "wchar", "dchar", "string", "wstring", "dstring",
            "size_t", "ptrdiff_t",
            // Attributes
            "@safe", "@trusted", "@system", "@nogc", "@property",
            "@disable", "@live",
            // Special tokens
            "__FILE__", "__LINE__", "__DATE__", "__TIME__",
            "__FUNCTION__", "__PRETTY_FUNCTION__", "__MODULE__",
        };
        return s;
    }

    // Populate pool
    // Call with empty vector to use D builtins only.
    // Call with non-empty vector to use ONLY those items (addon mode)
    void set_items(const std::vector<std::string> &addon_items, bool use_d_builtins = false) {
        all_items.clear();
        if (use_d_builtins) {
            all_items = d_builtins();
        }
        for (const auto &item : addon_items) {
            if (!item.empty() &&
                std::find(all_items.begin(), all_items.end(), item) == all_items.end())
                all_items.push_back(item);
        }
        std::sort(all_items.begin(), all_items.end());
    }

    // Update filtered list based on current prefix
    void update(const std::string &word_prefix) {
        prefix = word_prefix;
        filtered.clear();
        if (prefix.size() < 1) { hide(); return; }

        for (const auto &item : all_items) {
            if (item.size() >= prefix.size() &&
                item.compare(0, prefix.size(), prefix) == 0 &&
                item != prefix)   // don't show exact match
            {
                filtered.push_back(item);
                if ((int)filtered.size() >= 80) break;  // cap
            }
        }

        if (filtered.empty()) { hide(); return; }

        visible = true;
        anchor_pos = -1;   // re-anchored on the next frame, at the new cursor offset
        if (selected >= (int)filtered.size()) selected = 0;
        scroll = std::max(0, std::min(scroll, (int)filtered.size() - MAX_VISIBLE));
    }

    // Word suggestions from buffer (fuzzy prefix + similar words)
    // Called when word_hints_on=true. Extracts all words from buf_str and
    // adds those that share the prefix or are "similar" (edit distance <= 1)
    void update_with_buffer_words(const std::string &word_prefix,
                                   const std::string &buf_str,
                                   bool word_hints_on)
    {
        prefix = word_prefix;
        filtered.clear();
        if (prefix.size() < min_prefix_len) { hide(); return; }

        // Step 1: exact-prefix matches from all_items
        std::vector<std::string> exact, similar;
        for (const auto &item : all_items) {
            if (item.size() > prefix.size() &&
                item.compare(0, prefix.size(), prefix) == 0)
                exact.push_back(item);
        }

        // Step 2: words from buffer (if enabled)
        if (word_hints_on && !buf_str.empty()) {
            // Extract unique words from buffer (4+ chars, alpha-numeric+_)
            std::vector<std::string> buf_words;
            size_t i = 0;
            while (i < buf_str.size()) {
                if (std::isalpha((unsigned char)buf_str[i]) || buf_str[i] == '_') {
                    size_t j = i;
                    while (j < buf_str.size() &&
                           (std::isalnum((unsigned char)buf_str[j]) || buf_str[j] == '_'))
                        ++j;
                    if (j - i >= 4) {
                        std::string w = buf_str.substr(i, j - i);
                        if (w != prefix &&
                            std::find(buf_words.begin(), buf_words.end(), w) == buf_words.end() &&
                            std::find(exact.begin(), exact.end(), w) == exact.end())
                            buf_words.push_back(w);
                    }
                    i = j;
                } else { ++i; }
            }

            // Add buffer words that start with prefix
            for (const auto &w : buf_words) {
                if (w.size() > prefix.size() &&
                    w.compare(0, prefix.size(), prefix) == 0)
                    exact.push_back(w);
            }

            // Add "similar" words (transposition or 1 substitution at end)
            if (prefix.size() >= 3) {
                for (const auto &w : buf_words) {
                    if (w.size() < prefix.size() || w.size() > prefix.size() + 6) continue;
                    if (w.compare(0, prefix.size(), prefix) == 0) continue; // already exact
                    // Check first (prefix.size()-1) chars match
                    if (prefix.size() >= 2 &&
                        w.compare(0, prefix.size() - 1, prefix, 0, prefix.size() - 1) == 0) {
                        similar.push_back(w);
                    }
                }
            }
        }

        // Sort and deduplicate
        std::sort(exact.begin(), exact.end());
        exact.erase(std::unique(exact.begin(), exact.end()), exact.end());
        std::sort(similar.begin(), similar.end());

        // Merge: exact first, then similar, cap at 80
        for (const auto &w : exact) {
            filtered.push_back(w);
            if ((int)filtered.size() >= 60) break;
        }
        for (const auto &w : similar) {
            if (std::find(filtered.begin(), filtered.end(), w) == filtered.end())
                filtered.push_back(w);
            if ((int)filtered.size() >= 80) break;
        }

        if (filtered.empty()) { hide(); return; }

        visible = true;
        anchor_pos = -1;   // re-anchored on the next frame, at the new cursor offset
        if (selected >= (int)filtered.size()) selected = 0;
        scroll = std::max(0, std::min(scroll, (int)filtered.size() - MAX_VISIBLE));
    }

    // Call once per frame with the cursor offset. Typing re-anchors the popup;
    // any other cursor move (click, arrows, another line) closes it.
    void follow_cursor(int pos) {
        if (!visible) return;
        if (anchor_pos < 0) { anchor_pos = pos; return; }
        if (pos != anchor_pos) hide();
    }

    void hide() {
        visible = false;
        anchor_pos = -1;
        selected = 0;
        scroll = 0;
        anim = 0.0f;
    }

    // Keyboard intercept
    // Returns true if key was consumed by the autocomplete
    // out_insert is set to the string to insert when accepting
    bool handle_key(XKeyEvent *kev, std::string &out_insert) {
        if (!visible || !enabled) return false;
        KeySym ks = XLookupKeysym(kev, 0);

        if (ks == XK_Down) {
            selected = std::min(selected + 1, (int)filtered.size() - 1);
            if (selected >= scroll + MAX_VISIBLE) scroll = selected - MAX_VISIBLE + 1;
            return true;
        }
        if (ks == XK_Up) {
            selected = std::max(selected - 1, 0);
            if (selected < scroll) scroll = selected;
            return true;
        }
        if (ks == XK_Tab || ks == XK_Return) {
            if (selected >= 0 && selected < (int)filtered.size()) {
                // Insert the suffix (after prefix)
                out_insert = filtered[selected].substr(prefix.size());
            }
            hide();
            return true;
        }
        if (ks == XK_Escape) {
            hide();
            return true;
        }
        return false;
    }

    // Draw popup
    void draw(cairo_t *cr, double cursor_px, double cursor_py,
              int win_w, int win_h, const ThemeVariant &tv,
              PangoLayout *layout, PangoFontDescription *fnt)
    {
        if (!visible || !enabled || filtered.empty()) return;

        // Animate in
        anim = std::min(1.0f, anim + 0.12f);
        float alpha = anim;

        int show = std::min(MAX_VISIBLE, (int)filtered.size());
        double ph = show * ITEM_H + 10;
        double pw = POPUP_W;

        // Position below cursor; flip above if too close to bottom
        double px = cursor_px - 4;
        double py = cursor_py + 20;
        if (py + ph > win_h - 20) py = cursor_py - ph - 4;
        if (px + pw > win_w - 12) px = win_w - pw - 12;
        if (px < 4) px = 4;

        popup_x = px; popup_y = py;

        cairo_save(cr);
        cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

        // Background
        double bg = tv.surface + 0.04;
        cairo_set_source_rgba(cr, bg, bg, bg, alpha);
        double sr = 10;
        shape::rrect(cr, px, py, pw, ph, sr);
        cairo_fill(cr);

        // Border
        cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, 0.18 * alpha);
        cairo_set_line_width(cr, 1.0);
        shape::rrect(cr, px, py, pw, ph, sr);
        cairo_stroke(cr);

        // Items clip
        cairo_rectangle(cr, px, py, pw, ph);
        cairo_clip(cr);

        // Set small font
        PangoFontDescription *smf = pango_font_description_copy(fnt);
        pango_font_description_set_size(smf, 10 * PANGO_SCALE);
        pango_layout_set_font_description(layout, smf);

        for (int i = 0; i < show; ++i) {
            int idx = scroll + i;
            bool is_sel = (idx == selected);
            double iy = py + 5 + i * ITEM_H;

            // Selection highlight
            if (is_sel) {
                cairo_set_source_rgba(cr,
                    tv.accent_r, tv.accent_g, tv.accent_b, 0.22 * alpha);
                cairo_rectangle(cr, px + 4, iy, pw - 8, ITEM_H - 2);
                cairo_fill(cr);
            }

            const std::string &item = filtered[idx];

            // Draw prefix in accent, suffix in text color
            std::string pre = item.substr(0, prefix.size());
            std::string suf = item.substr(prefix.size());

            // Prefix part
            cairo_set_source_rgba(cr,
                tv.accent_r, tv.accent_g, tv.accent_b, alpha);
            pango_layout_set_text(layout, pre.c_str(), -1);
            int pw2, ph2; pango_layout_get_pixel_size(layout, &pw2, &ph2);
            cairo_move_to(cr, px + 14, iy + (ITEM_H - ph2) / 2.0);
            pango_cairo_show_layout(cr, layout);

            // Suffix part
            cairo_set_source_rgba(cr,
                tv.text, tv.text, tv.text, (is_sel ? 0.95 : 0.7) * alpha);
            pango_layout_set_text(layout, suf.c_str(), -1);
            cairo_move_to(cr, px + 14 + pw2, iy + (ITEM_H - ph2) / 2.0);
            pango_cairo_show_layout(cr, layout);
        }

        // Scrollbar indicator (if more items than visible)
        if ((int)filtered.size() > MAX_VISIBLE) {
            double ratio  = (double)MAX_VISIBLE / (double)filtered.size();
            double thumbh = std::max(12.0, ph * ratio);
            double maxs   = (double)(filtered.size() - MAX_VISIBLE);
            double toff   = (maxs > 0) ? (scroll / maxs) * (ph - thumbh) : 0;
            cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.35 * alpha);
            cairo_rectangle(cr, px + pw - 5, py + toff, 3, thumbh);
            cairo_fill(cr);
        }

        pango_font_description_free(smf);
        pango_layout_set_font_description(layout, fnt);
        cairo_restore(cr);
    }

    // Extract current word under cursor in buffer
    static std::string word_at(const std::string &buf, int cursor) {
        if (buf.empty() || cursor <= 0) return "";
        int i = cursor - 1;
        while (i >= 0 && (isalnum((unsigned char)buf[i]) ||
                          buf[i] == '_' || buf[i] == '@')) --i;
        std::string w = buf.substr(i + 1, cursor - i - 1);
        return w;
    }
};

#endif
