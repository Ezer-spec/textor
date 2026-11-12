#ifndef RENDER_STATUS_BAR_DRAW_HXX
#define RENDER_STATUS_BAR_DRAW_HXX

// Status bar along the bottom edge of the window.
//
//   right:  Ln 12, Col 5   Spaces: 4   UTF-8   Adco
//
// The bar is STATUS_H pixels high.  Editor::console_h_now() already includes
// STATUS_H, so the code viewport, scrollbars and the terminal all end above it.

#include "../draw_context.hxx"

// Draws `text` at (x, y) and returns its width in pixels.
static double status_text(cairo_t *cr, PangoLayout *layout, const std::string &text,
                          double x, double y, double r, double g, double b)
{
    pango_layout_set_text(layout, text.c_str(), -1);
    int w = 0, h = 0;
    pango_layout_get_pixel_size(layout, &w, &h);
    cairo_set_source_rgb(cr, r, g, b);
    cairo_move_to(cr, x, y + (24 - h) / 2.0);
    pango_cairo_show_layout(cr, layout);
    return w;
}

// Number of characters (not bytes) in the first `bytes` bytes of `s`.
static int status_char_count(const std::string &s, int bytes)
{
    int n = 0;
    for (int i = 0; i < bytes && i < (int)s.size(); i++)
        if (((unsigned char)s[i] & 0xC0) != 0x80) n++;
    return n;
}

static void draw_status_bar(DrawContext &dc)
{
    cairo_t *cr = dc.cr;
    Editor &ed = dc.ed;
    const ThemeVariant &tv = dc.tv;

    const double bar_h = ed.STATUS_H;
    const double y = ed.height - bar_h;

    // Background + top border
    cairo_set_source_rgb(cr, tv.panel, tv.panel, tv.panel);
    cairo_rectangle(cr, 0, y, ed.width, bar_h);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 1, 1, 1, 0.07);
    cairo_rectangle(cr, 0, y, ed.width, 1);
    cairo_fill(cr);

    // Small font for the bar
    PangoFontDescription *small = pango_font_description_copy(dc.fnt);
    pango_font_description_set_size(small, 10 * PANGO_SCALE);
    pango_layout_set_font_description(dc.layout, small);

    // Right: cursor position, indent, encoding, language (built right to left)
    std::vector<std::string> items;
    if (!ed.is_settings_tab_active()) {
        int line = 0, col = 0;
        ed.code.index_to_line_col(ed.code.cursor_pos, line, col);
        int char_col = (line >= 0 && line < (int)ed.code.lines.size()) ? status_char_count(ed.code.lines[line], col) : col;
        items.push_back("Ln " + std::to_string(line + 1) + ", Col " + std::to_string(char_col + 1));
        items.push_back("Spaces: " + std::to_string((int)tv.indent_size));
        items.push_back("UTF-8");
        items.push_back(ed.code.language_name());
    }
    double x = ed.width - 14;
    for (int i = (int)items.size() - 1; i >= 0; i--) {
        pango_layout_set_text(dc.layout, items[i].c_str(), -1);
        int w = 0, h = 0;
        pango_layout_get_pixel_size(dc.layout, &w, &h);
        x -= w;
        bool is_lang = (i == (int)items.size() - 1);   // the language gets the accent colour
        status_text(cr, dc.layout, items[i], x, y,
                    is_lang ? tv.accent_r : tv.muted, is_lang ? tv.accent_g : tv.muted, is_lang ? tv.accent_b : tv.muted);
        x -= 18;
    }

    pango_layout_set_font_description(dc.layout, dc.fnt);
    pango_font_description_free(small);
}

#endif
