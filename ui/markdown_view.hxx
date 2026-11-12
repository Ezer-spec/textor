#ifndef MARKDOWN_VIEW_HXX
#define MARKDOWN_VIEW_HXX

//  markdown_view.hxx - the "Viewers" pane: draws a Markdown text as a formatted page.
//
//  Supported: # headings, paragraphs, - * + and 1. lists, > quotes, ``` code blocks,
//  rules, and inside the text: `code`, **bold**, *italic*, [link](url).
//  The text is drawn from the top to the bottom in one pass; `draw` returns the height
//  of the whole page, which the editor uses for the scrollbar and the scroll limit.

#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include "../color/themes.hxx"

namespace mdview {

// "#rrggbb" for Pango markup
static inline std::string hex(double r, double g, double b) {
    auto c = [](double v) { return (int)(std::clamp(v, 0.0, 1.0) * 255.0 + 0.5); };
    char buf[16];
    std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", c(r), c(g), c(b));
    return buf;
}

// Pango markup treats & < > as special characters
static inline std::string escape(const std::string &s) {
    std::string out;
    for (char c : s) {
        if      (c == '&') out += "&amp;";
        else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else               out += c;
    }
    return out;
}

// One line of Markdown text -> Pango markup
static inline std::string inline_markup(const std::string &t, const ThemeVariant &tv) {
    const std::string code = hex(tv.string_r, tv.string_g, tv.string_b);
    const std::string link = hex(tv.accent_r, tv.accent_g, tv.accent_b);
    std::string out;
    const size_t n = t.size();
    size_t i = 0;
    while (i < n) {
        char c = t[i];
        if (c == '`') {                                   // `code`
            size_t j = t.find('`', i + 1);
            if (j != std::string::npos) {
                out += "<span foreground=\"" + code + "\">" + escape(t.substr(i + 1, j - i - 1)) + "</span>";
                i = j + 1; continue;
            }
        }
        if ((c == '*' || c == '_') && i + 1 < n && t[i + 1] == c) {     // **bold**
            size_t j = t.find(std::string(2, c), i + 2);
            if (j != std::string::npos && j > i + 2) {
                out += "<b>" + inline_markup(t.substr(i + 2, j - i - 2), tv) + "</b>";
                i = j + 2; continue;
            }
        }
        if (c == '*' && i + 1 < n && t[i + 1] != ' ') {                 // *italic*
            size_t j = t.find('*', i + 1);
            if (j != std::string::npos && j > i + 1) {
                out += "<i>" + inline_markup(t.substr(i + 1, j - i - 1), tv) + "</i>";
                i = j + 1; continue;
            }
        }
        if (c == '[') {                                   // [text](url) - the text is drawn as a link
            size_t j = t.find(']', i + 1);
            if (j != std::string::npos && j + 1 < n && t[j + 1] == '(') {
                size_t k = t.find(')', j + 2);
                if (k != std::string::npos) {
                    out += "<span foreground=\"" + link + "\" underline=\"single\">"
                         + inline_markup(t.substr(i + 1, j - i - 1), tv) + "</span>";
                    i = k + 1; continue;
                }
            }
        }
        out += escape(std::string(1, c));
        ++i;
    }
    return out;
}

static inline std::string trim(const std::string &s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
    return s.substr(a, b - a);
}

static inline bool starts_with(const std::string &s, const char *p) {
    return s.compare(0, std::char_traits<char>::length(p), p) == 0;
}

static inline int heading_level(const std::string &s) {       // "## Title" -> 2, otherwise 0
    size_t j = 0;
    while (j < s.size() && s[j] == '#') ++j;
    return (j >= 1 && j <= 6 && j < s.size() && s[j] == ' ') ? (int)j : 0;
}

static inline bool is_rule(const std::string &s) {            // ---  ***  ___
    if (s.size() < 3 || (s[0] != '-' && s[0] != '*' && s[0] != '_')) return false;
    int count = 0;
    for (char c : s) {
        if (c == s[0]) ++count;
        else if (c != ' ') return false;
    }
    return count >= 3;
}

// List item: returns the length of the marker ("- " is 2, "12. " is 4), 0 = not a list item
static inline size_t list_marker(const std::string &s, bool &numbered) {
    numbered = false;
    if (s.size() >= 2 && (s[0] == '-' || s[0] == '*' || s[0] == '+') && s[1] == ' ') return 2;
    size_t j = 0;
    while (j < s.size() && std::isdigit((unsigned char)s[j])) ++j;
    if (j > 0 && j + 1 < s.size() && (s[j] == '.' || s[j] == ')') && s[j + 1] == ' ') {
        numbered = true;
        return j + 2;
    }
    return 0;
}

// Does the line start a block of its own (so a paragraph stops before it)?
static inline bool starts_block(const std::string &s) {
    bool num;
    return s.empty() || heading_level(s) || starts_with(s, "```") || starts_with(s, "~~~") ||
           starts_with(s, ">") || is_rule(s) || list_marker(s, num) > 0;
}

// ---- Tables:  | a | b |  then  |---|:--:|  then the rows ----

// Cells of one table row: "| a | b |" -> {"a", "b"}   (\| is a bar inside a cell)
static inline std::vector<std::string> split_row(const std::string &row) {
    std::string r = trim(row);
    if (!r.empty() && r[0] == '|') r.erase(0, 1);
    std::vector<std::string> cells;
    std::string cur;
    for (size_t k = 0; k < r.size(); ++k) {
        if (r[k] == '\\' && k + 1 < r.size() && r[k + 1] == '|') { cur += '|'; ++k; }
        else if (r[k] == '|') { cells.push_back(trim(cur)); cur.clear(); }
        else cur += r[k];
    }
    if (!trim(cur).empty()) cells.push_back(trim(cur));      // no bar after the last cell
    return cells;
}

// The line under the header:  |---|:---:|---:|
static inline bool is_table_sep(const std::string &line) {
    if (line.find('-') == std::string::npos || line.find('|') == std::string::npos) return false;
    for (char c : line)
        if (c != '|' && c != '-' && c != ':' && c != ' ' && c != '\t') return false;
    return true;
}

// Draws the page inside the rectangle (x, top, w, view_h); scroll_y is how far it is scrolled.
// The caller clips to the rectangle and paints the background. Returns the height of the page.
static inline double draw(cairo_t *cr, const ThemeVariant &tv, const std::string &text,
                          double x, double top, double w, double view_h, double scroll_y)
{
    const double PAD = 18.0;
    const double text_w = std::max(40.0, w - 2 * PAD);
    PangoLayout *pl = pango_cairo_create_layout(cr);

    // One block of text: Pango markup, left indent, font scale, colour. Returns its height.
    double y = top - scroll_y + PAD;
    auto block = [&](const std::string &markup, double indent, double scale,
                     double r, double g, double b) -> double {
        PangoFontDescription *fd = pango_font_description_from_string(tv.font_family.c_str());
        pango_font_description_set_size(fd, (gint)(tv.font_size * scale * PANGO_SCALE));
        pango_layout_set_font_description(pl, fd);
        pango_font_description_free(fd);
        pango_layout_set_width(pl, (int)((text_w - indent) * PANGO_SCALE));
        pango_layout_set_wrap(pl, PANGO_WRAP_WORD_CHAR);
        pango_layout_set_markup(pl, markup.c_str(), -1);
        int lw, lh; pango_layout_get_pixel_size(pl, &lw, &lh);
        if (y + lh > top && y < top + view_h) {
            cairo_set_source_rgb(cr, r, g, b);
            cairo_move_to(cr, x + PAD + indent, y);
            pango_cairo_show_layout(cr, pl);
        }
        return lh;
    };
    auto rect = [&](double rx, double ry, double rw, double rh, double r, double g, double b, double a) {
        if (ry + rh < top || ry > top + view_h) return;
        cairo_set_source_rgba(cr, r, g, b, a);
        cairo_rectangle(cr, rx, ry, rw, rh);
        cairo_fill(cr);
    };

    // A table: rows[0] is the header. align: 0 left, 1 centre, 2 right (one per column).
    // Columns are as wide as their text, when the table is wider than the page they shrink and wrap.
    auto table = [&](const std::vector<std::vector<std::string>> &rows,
                     const std::vector<int> &align) -> double {
        const double CP = 10.0, RP = 5.0;                     // cell padding: sides, top and bottom
        const size_t cols = align.size();
        auto set_font = [&]() {
            PangoFontDescription *fd = pango_font_description_from_string(tv.font_family.c_str());
            pango_font_description_set_size(fd, (gint)(tv.font_size * PANGO_SCALE));
            pango_layout_set_font_description(pl, fd);
            pango_font_description_free(fd);
            pango_layout_set_wrap(pl, PANGO_WRAP_WORD_CHAR);
        };
        auto markup_of = [&](size_t r, size_t c) {
            std::string m = inline_markup(rows[r][c], tv);
            return r == 0 ? "<b>" + m + "</b>" : m;
        };
        set_font();
        std::vector<double> colw(cols, 30.0);
        for (size_t r = 0; r < rows.size(); ++r)
            for (size_t c = 0; c < cols; ++c) {
                pango_layout_set_width(pl, -1);
                pango_layout_set_markup(pl, markup_of(r, c).c_str(), -1);
                int lw, lh; pango_layout_get_pixel_size(pl, &lw, &lh);
                colw[c] = std::max(colw[c], lw + 2 * CP);
            }
        double sum = 0; for (double v : colw) sum += v;
        if (sum > text_w) for (double &v : colw) v = std::max(40.0, v * text_w / sum);

        // height of every row = its tallest cell
        std::vector<double> rowh(rows.size(), 0.0);
        for (size_t r = 0; r < rows.size(); ++r) {
            for (size_t c = 0; c < cols; ++c) {
                pango_layout_set_width(pl, (int)((colw[c] - 2 * CP) * PANGO_SCALE));
                pango_layout_set_markup(pl, markup_of(r, c).c_str(), -1);
                int lw, lh; pango_layout_get_pixel_size(pl, &lw, &lh);
                rowh[r] = std::max(rowh[r], lh + 2 * RP);
            }
        }
        double total_w = 0; for (double v : colw) total_w += v;
        double total_h = 0; for (double v : rowh) total_h += v;

        // header background, then the text, then the grid lines
        rect(x + PAD, y, total_w, rowh[0], tv.accent_r, tv.accent_g, tv.accent_b, 0.14);
        double ry = y;
        for (size_t r = 0; r < rows.size(); ++r) {
            double cx = x + PAD;
            for (size_t c = 0; c < cols; ++c) {
                if (ry + rowh[r] > top && ry < top + view_h) {
                    pango_layout_set_width(pl, (int)((colw[c] - 2 * CP) * PANGO_SCALE));
                    pango_layout_set_alignment(pl, align[c] == 1 ? PANGO_ALIGN_CENTER
                                                  : align[c] == 2 ? PANGO_ALIGN_RIGHT : PANGO_ALIGN_LEFT);
                    pango_layout_set_markup(pl, markup_of(r, c).c_str(), -1);
                    cairo_set_source_rgb(cr, tv.text, tv.text, tv.text);
                    cairo_move_to(cr, cx + CP, ry + RP);
                    pango_cairo_show_layout(cr, pl);
                }
                cx += colw[c];
            }
            ry += rowh[r];
        }
        pango_layout_set_alignment(pl, PANGO_ALIGN_LEFT);
        if (y + total_h > top && y < top + view_h) {
            cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, 0.45);
            cairo_set_line_width(cr, 1.0);
            double gx = x + PAD, gy = y;
            for (size_t r = 0; r <= rows.size(); ++r) {            // horizontal lines
                cairo_move_to(cr, gx, gy + 0.5); cairo_line_to(cr, gx + total_w, gy + 0.5);
                if (r < rows.size()) gy += rowh[r];
            }
            for (size_t c = 0; c <= cols; ++c) {                   // vertical lines
                cairo_move_to(cr, gx + 0.5, y); cairo_line_to(cr, gx + 0.5, y + total_h);
                if (c < cols) gx += colw[c];
            }
            cairo_stroke(cr);
        }
        return total_h;
    };

    std::vector<std::string> lines;
    {
        size_t s = 0;
        while (s <= text.size()) {
            size_t e = text.find('\n', s);
            if (e == std::string::npos) e = text.size();
            lines.push_back(text.substr(s, e - s));
            s = e + 1;
        }
    }

    static const double heading_scale[7] = {1.0, 1.8, 1.5, 1.3, 1.15, 1.05, 1.0};
    const size_t N = lines.size();
    size_t i = 0;
    while (i < N) {
        std::string raw = lines[i];
        std::string line = trim(raw);

        if (line.empty()) { y += 8; ++i; continue; }

        // Code block
        if (starts_with(line, "```") || starts_with(line, "~~~")) {
            std::string code;
            ++i;
            while (i < N && !starts_with(trim(lines[i]), "```") && !starts_with(trim(lines[i]), "~~~")) {
                if (!code.empty()) code += "\n";
                code += lines[i];
                ++i;
            }
            if (i < N) ++i;                                     // the closing fence
            PangoFontDescription *fd = pango_font_description_from_string(tv.font_family.c_str());
            pango_font_description_set_size(fd, (gint)(tv.font_size * 0.95 * PANGO_SCALE));
            pango_layout_set_font_description(pl, fd);
            pango_font_description_free(fd);
            pango_layout_set_width(pl, -1);
            pango_layout_set_text(pl, code.c_str(), -1);
            int lw, lh; pango_layout_get_pixel_size(pl, &lw, &lh);
            rect(x + PAD, y, text_w, lh + 16, tv.muted, tv.muted, tv.muted, 0.12);
            if (y + lh + 16 > top && y < top + view_h) {
                cairo_save(cr);
                cairo_rectangle(cr, x + PAD, y, text_w, lh + 16);
                cairo_clip(cr);
                cairo_set_source_rgba(cr, tv.text, tv.text, tv.text, 0.92);
                cairo_move_to(cr, x + PAD + 10, y + 8);
                pango_cairo_show_layout(cr, pl);
                cairo_restore(cr);
            }
            y += lh + 16 + 8;
            continue;
        }

        // Table: a header line with bars, and under it the |---|---| line
        if (line.find('|') != std::string::npos && i + 1 < N && is_table_sep(trim(lines[i + 1]))) {
            std::vector<std::vector<std::string>> rows;
            rows.push_back(split_row(line));
            std::vector<int> align;
            for (const std::string &cell : split_row(trim(lines[i + 1]))) {
                bool l = !cell.empty() && cell.front() == ':', r = !cell.empty() && cell.back() == ':';
                align.push_back(l && r ? 1 : (r ? 2 : 0));
            }
            align.resize(std::max(align.size(), rows[0].size()), 0);
            i += 2;
            while (i < N && !trim(lines[i]).empty() && trim(lines[i]).find('|') != std::string::npos)
                rows.push_back(split_row(trim(lines[i++])));
            for (auto &row : rows) row.resize(align.size());      // short rows get empty cells
            y += table(rows, align) + 12;
            continue;
        }

        // Heading
        if (int lvl = heading_level(line)) {
            std::string title = trim(line.substr(lvl + 1));
            y += (lvl <= 2 ? 8 : 4);
            double h = block("<b>" + inline_markup(title, tv) + "</b>", 0, heading_scale[lvl],
                             lvl <= 2 ? tv.accent_r : tv.text, lvl <= 2 ? tv.accent_g : tv.text,
                             lvl <= 2 ? tv.accent_b : tv.text);
            y += h;
            if (lvl <= 2) { y += 4; rect(x + PAD, y, text_w, 1, tv.muted, tv.muted, tv.muted, 0.25); y += 1; }
            y += 8; ++i; continue;
        }

        // Horizontal rule
        if (is_rule(line)) {
            y += 6; rect(x + PAD, y, text_w, 1, tv.muted, tv.muted, tv.muted, 0.35); y += 1 + 10; ++i; continue;
        }

        // Quote (several lines in a row are one block)
        if (line[0] == '>') {
            std::string quote;
            while (i < N && !trim(lines[i]).empty() && trim(lines[i])[0] == '>') {
                std::string q = trim(lines[i]).substr(1);
                if (!q.empty() && q[0] == ' ') q.erase(0, 1);
                if (!quote.empty()) quote += " ";
                quote += q;
                ++i;
            }
            const double qc = std::min(1.0, tv.muted * 1.4);
            double h = block(inline_markup(quote, tv), 14, 1.0, qc, qc, qc);
            rect(x + PAD, y, 3, h, tv.accent_r, tv.accent_g, tv.accent_b, 0.7);
            y += h + 8; continue;
        }

        // List item
        bool numbered = false;
        size_t mlen = list_marker(line, numbered);
        if (mlen > 0) {
            size_t lead = 0;
            while (lead < raw.size() && (raw[lead] == ' ' || raw[lead] == '\t')) ++lead;
            double level = (double)(lead / 2) * 16.0;
            std::string item = trim(line.substr(mlen));
            std::string mark = numbered ? line.substr(0, mlen - 1) : std::string("\xE2\x80\xA2");   // 1.  or the bullet
            block("<b>" + escape(mark) + "</b>", level, 1.0, tv.accent_r, tv.accent_g, tv.accent_b);
            double h = block(inline_markup(item, tv), level + 22, 1.0, tv.text, tv.text, tv.text);
            y += h + 3; ++i; continue;
        }

        // Paragraph: lines up to the next blank line or the next block
        std::string para = line;
        ++i;
        while (i < N && !starts_block(trim(lines[i]))) { para += " " + trim(lines[i]); ++i; }
        y += block(inline_markup(para, tv), 0, 1.0, tv.text, tv.text, tv.text) + 8;
    }

    g_object_unref(pl);
    return (y + scroll_y - top) + PAD;
}

}

#endif
