#ifndef RENDER_SIDEBAR_OVERLAY_DRAW_HXX
#define RENDER_SIDEBAR_OVERLAY_DRAW_HXX

#include "../draw_context.hxx"

static void draw_sidebar_overlay(DrawContext &dc) {
    cairo_t *cr = dc.cr;
    Editor &ed = dc.ed;
    const ThemeVariant &tv = dc.tv;
    PangoLayout *layout = dc.layout;
    PangoFontDescription *fnt = dc.fnt;
    (void)cr; (void)ed; (void)tv; (void)layout; (void)fnt;
    int side_x = ed.activity_bar_w;
    double sidebar_p    = ed.sidebar_anim.progress();
    bool   draw_sidebar = (sidebar_p > 0.002) || ed.sidebar_anim.is_running();

    // Sidebar panel - attached to window, pushes editor right
    if (draw_sidebar) {
        int full_sw = ed.sidebar.width;
        double p    = std::clamp(sidebar_p, 0.0, 1.0);
        int panel_w = (int)(full_sw * p);
        int panel_x = side_x;
        int global_top = ed.content_top();
        int panel_h = ed.height - global_top;

        if (panel_w > 2) {
            cairo_save(cr);
            cairo_rectangle(cr, panel_x, global_top, panel_w, panel_h);
            cairo_clip(cr);

            // Panel background
            double sb_bg = tv.panel * 0.97 + tv.elevated * 0.03;
            cairo_set_source_rgb(cr, sb_bg, sb_bg, sb_bg + 0.008);
            cairo_paint(cr);

            // Draw content (fade in slightly with anim)
            if (p > 0.08) {
                cairo_save(cr);
                cairo_translate(cr, 0, global_top);
                ed.sidebar.width = panel_w;
                ed.sidebar.draw(cr, panel_h, panel_x, tv);
                ed.sidebar.width = full_sw;
                cairo_restore(cr);
            }

            cairo_restore(cr);

            // Right-edge divider - flat line, no gradient falloff
            cairo_set_source_rgba(cr, 0, 0, 0, 0.18 * p);
            cairo_rectangle(cr, panel_x + panel_w - 1, global_top, 1, panel_h);
            cairo_fill(cr);
        }
    }

}

#endif
