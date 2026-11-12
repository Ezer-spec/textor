#ifndef RENDER_TAB_PANEL_DRAW_HXX
#define RENDER_TAB_PANEL_DRAW_HXX

// Tab strip: one row of open files right under the top bar
//
//   [icon name  x] [icon name  x] [icon name  x] ...
//
// - the active tab has the editor's background and an accent line on top
// - a filled dot replaces the "x" while a file has unsaved changes
// - when the tabs don't fit they scroll sideways (mouse wheel over the strip)
//
// Geometry and hit-testing live in Editor (tabstrip_*), so drawing and
// clicking always agree.

#include "../svg_icons.hxx"
#include "../draw_context.hxx"

static void draw_tab_strip(DrawContext &dc) {
    cairo_t *cr = dc.cr;
    Editor &ed = dc.ed;
    const ThemeVariant &tv = dc.tv;
    PangoLayout *layout = dc.layout;

    const double y0 = ed.TAB_BAR_H - ed.chrome_shift();   // slides away together with the top bar
    const double h  = ed.LANG_BAR_H;
    const double x0 = ed.tabstrip_x0();

    // Strip background + bottom border (also fills the corner above the sidebar)
    cairo_set_source_rgb(cr, tv.panel, tv.panel, tv.panel);
    cairo_rectangle(cr, 0, y0, ed.width, h);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 1, 1, 1, 0.06);
    cairo_rectangle(cr, 0, y0 + h - 1, ed.width, 1);
    cairo_fill(cr);

    if (ed.tabs.empty()) return;

    const double tab_w = ed.tabstrip_tab_w();
    ed.tabstrip_scroll_x = std::clamp(ed.tabstrip_scroll_x, 0.0, ed.tabstrip_scroll_max());

    PangoFontDescription *small = pango_font_description_copy(dc.fnt);
    pango_font_description_set_size(small, (tv.font_size - 1) * PANGO_SCALE);
    pango_layout_set_font_description(layout, small);

    // Tabs never draw outside the strip (they scroll under the sidebar edge)
    cairo_save(cr);
    cairo_rectangle(cr, x0, y0, ed.width - x0, h);
    cairo_clip(cr);

    for (int i = 0; i < (int)ed.tabs.size(); i++) {
        double tx = x0 + i * tab_w - ed.tabstrip_scroll_x;
        if (tx + tab_w < x0 || tx > ed.width) continue;   // off-screen

        const auto &tab = ed.tabs[i];
        bool is_active = (i == ed.active_tab);
        bool is_hover  = (i == ed.tabstrip_hover);
        bool is_dirty  = is_active ? ed.code.dirty : tab.dirty;

        // Background: active tab looks like part of the editor below it
        if (is_active) {
            cairo_set_source_rgb(cr, tv.bg, tv.bg, tv.bg);
            cairo_rectangle(cr, tx, y0, tab_w, h);
            cairo_fill(cr);
            cairo_set_source_rgb(cr, tv.accent_r, tv.accent_g, tv.accent_b);
            cairo_rectangle(cr, tx, y0, tab_w, 2);
            cairo_fill(cr);
        } else if (is_hover) {
            cairo_set_source_rgba(cr, 1, 1, 1, 0.04);
            cairo_rectangle(cr, tx, y0, tab_w, h);
            cairo_fill(cr);
        }
        // Keyboard focus on the strip (focus_tabs shortcut)
        if (ed.ui_focus == Editor::UIFocus::TabPanel && i == ed.tabpanel_kbd_idx) {
            cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.55);
            cairo_set_line_width(cr, 1.2);
            cairo_rectangle(cr, tx + 1.5, y0 + 2.5, tab_w - 3, h - 4);
            cairo_stroke(cr);
        }
        // Separator on the right side of every tab
        cairo_set_source_rgba(cr, 1, 1, 1, 0.06);
        cairo_rectangle(cr, tx + tab_w - 1, y0 + 6, 1, h - 12);
        cairo_fill(cr);

        // File name (without folders) and extension (for the icon)
        std::string fname = tab.filename.empty() ? "untitled" : tab.filename;
        auto slash = fname.find_last_of("/\\");
        if (slash != std::string::npos) fname = fname.substr(slash + 1);
        std::string ext = svg_icons::icon_ext(fname);

        // Icon
        const double icon_sz = 16;
        ed.sidebar.draw_lang_icon(cr, tx + 10, y0 + (h - icon_sz) / 2, icon_sz, ext, tv);

        // Name (clipped so it never runs under the close button)
        cairo_save(cr);
        cairo_rectangle(cr, tx + 32, y0, tab_w - 32 - 28, h);
        cairo_clip(cr);
        double tc = is_active ? tv.text : tv.muted * 1.15;
        cairo_set_source_rgb(cr, tc, tc, tc);
        pango_layout_set_text(layout, fname.c_str(), -1);
        int lw, lh; pango_layout_get_pixel_size(layout, &lw, &lh);
        cairo_move_to(cr, tx + 32, y0 + (h - lh) / 2.0);
        pango_cairo_show_layout(cr, layout);
        cairo_restore(cr);

        // Right end: "x" (active or hovered tab) or the unsaved dot
        double cx = tx + tab_w - 15, cy = y0 + h / 2.0;
        if (is_dirty && !is_hover) {
            cairo_set_source_rgb(cr, tv.accent_r, tv.accent_g, tv.accent_b);
            shape::disc(cr, cx, cy, 3.2);
            cairo_fill(cr);
        } else if (is_active || is_hover) {
            svg_icons::draw_ui(cr, "cross", cx - 4.5, cy - 4.5, 9, tv.text, tv.text, tv.text, is_hover ? 0.85 : 0.5);
        }
    }
    cairo_restore(cr);

    pango_layout_set_font_description(layout, dc.fnt);
    pango_font_description_free(small);
}

#endif
