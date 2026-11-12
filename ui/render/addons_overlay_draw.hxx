#ifndef RENDER_ADDONS_OVERLAY_DRAW_HXX
#define RENDER_ADDONS_OVERLAY_DRAW_HXX

#include "../draw_context.hxx"

static void draw_addons_overlay(DrawContext &dc) {
    cairo_t *cr = dc.cr;
    Editor &ed = dc.ed;
    const ThemeVariant &tv = dc.tv;
    PangoLayout *layout = dc.layout;
    PangoFontDescription *fnt = dc.fnt;
    (void)cr; (void)ed; (void)tv; (void)layout; (void)fnt;

    // Addons panel - attached to window edge, pushes editor right
    {
        double ap = ed.addons_anim.progress();
        bool draw_ap = (ap > 0.002) || ed.addons_anim.is_running();
        if (draw_ap) {
            double p    = std::clamp(ap, 0.0, 1.0);
            int full_aw = 300;
            int panel_w = (int)(full_aw * p);
            int panel_x = ed.activity_bar_w;
            int global_top = ed.content_top();
            int panel_h = ed.height - global_top;

            if (panel_w > 2) {
                cairo_save(cr);
                cairo_rectangle(cr, panel_x, global_top, panel_w, panel_h);
                cairo_clip(cr);
                cairo_translate(cr, 0, global_top);

                // Panel bg
                double bg = tv.panel * 0.97;
                cairo_set_source_rgb(cr, bg, bg, bg + 0.01);
                cairo_paint(cr);

                // Content (fade in)
                if (p > 0.08) {
                    double fade = std::clamp((p - 0.08) / 0.5, 0.0, 1.0);
                    PangoLayout *lo = pango_cairo_create_layout(cr);

                    // Header
                    PangoFontDescription *hf = pango_font_description_from_string(
                        (tv.font_family + " Bold 11").c_str());
                    pango_layout_set_font_description(lo, hf);
                    pango_layout_set_text(lo, "Addons", -1);
                    cairo_set_source_rgba(cr, tv.text, tv.text, tv.text, fade);
                    cairo_move_to(cr, panel_x + 14, 10);
                    pango_cairo_show_layout(cr, lo);
                    pango_font_description_free(hf);

                    PangoFontDescription *sf = pango_font_description_from_string(
                        (tv.font_family + " 8").c_str());
                    pango_layout_set_font_description(lo, sf);
                    int addon_count = (int)ed.addon_loader.addons.size();
                    std::string sub = std::to_string(addon_count) + " addon"
                                    + (addon_count != 1 ? "s" : "") + " loaded";
                    pango_layout_set_text(lo, sub.c_str(), -1);
                    cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, 0.6 * fade);
                    cairo_move_to(cr, panel_x + 14, 28);
                    pango_cairo_show_layout(cr, lo);

                    cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, 0.12 * fade);
                    cairo_rectangle(cr, panel_x + 8, 44, panel_w - 16, 1);
                    cairo_fill(cr);

                    PangoFontDescription *nf = pango_font_description_from_string(
                        (tv.font_family + " 9").c_str());
                    pango_layout_set_font_description(lo, nf);

                    int ry = 52;
                    const int ROW = 52;
                    for (int i = 0; i < addon_count && ry < panel_h - 8; ++i) {
                        const auto &a = ed.addon_loader.addons[i];
                        bool hov = (ed.addons_panel_hovered == i);
                        if (hov) {
                            cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.10 * fade);
                            cairo_rectangle(cr, panel_x + 6, ry, panel_w - 12, ROW - 4);
                            cairo_fill(cr);
                        }
                        // File icon of the language (the SVG from the addon folder, if it has one)
                        {
                            std::string icon_ext = a.extensions.empty() ? std::string() : a.extensions[0].substr(1);
                            cairo_save(cr);
                            cairo_push_group(cr);
                            ed.sidebar.draw_lang_icon(cr, panel_x + 14, ry + (ROW - 4 - 22) / 2.0, 22.0, icon_ext, tv);
                            cairo_pop_group_to_source(cr);
                            cairo_paint_with_alpha(cr, fade);
                            cairo_restore(cr);
                        }

                        pango_layout_set_font_description(lo, nf);
                        pango_layout_set_text(lo, a.addon_name.c_str(), -1);
                        pango_layout_set_width(lo, (panel_w - 68) * PANGO_SCALE);
                        pango_layout_set_ellipsize(lo, PANGO_ELLIPSIZE_END);
                        cairo_set_source_rgba(cr, tv.text, tv.text, tv.text, fade);
                        cairo_move_to(cr, panel_x + 46, ry + 6);
                        pango_cairo_show_layout(cr, lo);

                        std::string info = a.lang_name;
                        if (!a.extensions.empty()) {
                            info += "  ";
                            for (auto &e2 : a.extensions) info += e2 + " ";
                        }
                        pango_layout_set_font_description(lo, sf);
                        pango_layout_set_text(lo, info.c_str(), -1);
                        pango_layout_set_width(lo, (panel_w - 68) * PANGO_SCALE);
                        cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, 0.65 * fade);
                        cairo_move_to(cr, panel_x + 46, ry + 22);
                        pango_cairo_show_layout(cr, lo);

                        std::string av = "v" + a.version;
                        if (!a.author.empty()) av += "  by " + a.author;
                        pango_layout_set_text(lo, av.c_str(), -1);
                        pango_layout_set_width(lo, (panel_w - 68) * PANGO_SCALE);
                        cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, 0.40 * fade);
                        cairo_move_to(cr, panel_x + 46, ry + 34);
                        pango_cairo_show_layout(cr, lo);

                        cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, 0.08 * fade);
                        cairo_rectangle(cr, panel_x + 12, ry + ROW - 2, panel_w - 24, 1);
                        cairo_fill(cr);
                        ry += ROW;
                    }

                    if (addon_count == 0) {
                        pango_layout_set_font_description(lo, sf);
                        pango_layout_set_text(lo, "No addons found.", -1);
                        pango_layout_set_width(lo, -1);
                        cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, 0.4 * fade);
                        int tw, th; pango_layout_get_pixel_size(lo, &tw, &th);
                        cairo_move_to(cr, panel_x + (panel_w - tw)/2, 80);
                        pango_cairo_show_layout(cr, lo);
                    }

                    pango_font_description_free(nf);
                    pango_font_description_free(sf);
                    g_object_unref(lo);
                }

                cairo_restore(cr);

                // Right-edge divider
                cairo_set_source_rgba(cr, 0, 0, 0, 0.18 * p);
                cairo_rectangle(cr, panel_x + panel_w - 1, global_top, 1, panel_h);
                cairo_fill(cr);
            }
        }
    }


}

#endif
