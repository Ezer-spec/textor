#ifndef SETTINGS_THEMES_HXX
#define SETTINGS_THEMES_HXX
/*
    settings_help_about.hxx - definitions of SettingsTab class methods
    extracted from the settings_tab.hxx source file.
    This file is included at the end of settings_tab.hxx, after the class declaration
    (where these methods remain represented only by prototypes);
    it employs the same approach used for standard .cxx files but maintains
    the "header-only" format adopted by this project.
    Do not include this file directly; it requires the SettingsTab class
    to be fully declared already. (Though you can try-I won't stop you.)
*/

void SettingsTab::draw_themes(cairo_t *cr,PangoLayout *layout,PangoFontDescription *fnt,
                      int cx,int area_y,int cw,int ch,int ax,int aw,
                      const ThemeVariant &tv) {
        // Grid layout constants
        const int COLS       = 3;
        const int CARD_H     = 130;   // slightly taller for name bar
        const int CARD_GAP   = 18;    // gap between cards (no text below)
        const int HEADER_H   = 52;
        const int SIDE_PAD   = 16;

        int card_w = (cw - SIDE_PAD * 2 - CARD_GAP * (COLS - 1)) / COLS;
        if (card_w < 80) card_w = 80;
        int n = (int)themes.size();
        int rows = (n + COLS - 1) / COLS;

        themes_total_height = (double)(rows * (CARD_H + CARD_GAP) + HEADER_H + 8);
        themes_view_height  = (double)ch;
        double max_s = std::max(0.0, themes_total_height - themes_view_height);
        themes_scroll = std::clamp(themes_scroll, 0.0, max_s);

        // Section heading
        {
            // "Themes" title
            PangoFontDescription *hf = pango_font_description_copy(fnt);
            pango_font_description_set_size(hf, (tv.font_size + 5) * PANGO_SCALE);
            pango_font_description_set_weight(hf, PANGO_WEIGHT_BOLD);
            pango_layout_set_font_description(layout, hf);
            pango_layout_set_text(layout, "Themes", -1);
            int htw, hth; pango_layout_get_pixel_size(layout, &htw, &hth);
            cairo_set_source_rgb(cr, tv.text, tv.text, tv.text);
            cairo_move_to(cr, cx + SIDE_PAD, area_y + 10);
            pango_cairo_show_layout(cr, layout);
            pango_font_description_free(hf);

            // Count badge - positioned below title, not inline
            PangoFontDescription *sf2 = pango_font_description_copy(fnt);
            pango_font_description_set_size(sf2, (tv.font_size - 3) * PANGO_SCALE);
            pango_layout_set_font_description(layout, sf2);
            std::string cnt = std::to_string(n) + " themes";
            pango_layout_set_text(layout, cnt.c_str(), -1);
            int cntw, cnth; pango_layout_get_pixel_size(layout, &cntw, &cnth);
            // Draw badge pill
            double bx = cx + SIDE_PAD + htw + 10;
            double by = area_y + 10 + (hth - cnth) / 2.0;
            double cr2 = g_rounded_corners ? (cnth + 4) / 2.0 : 2.0;
            cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.18);
            rrect(cr, bx - 5, by - 2, cntw + 10, cnth + 4, cr2); cairo_fill(cr);
            cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.70);
            cairo_move_to(cr, bx, by);
            pango_cairo_show_layout(cr, layout);
            pango_font_description_free(sf2);
            pango_layout_set_font_description(layout, fnt);
        }

        // Grid clip
        cairo_save(cr);
        cairo_rectangle(cr, cx, area_y + HEADER_H, cw, ch - HEADER_H);
        cairo_clip(cr);

        PangoFontDescription *name_fnt = pango_font_description_copy(fnt);
        pango_font_description_set_size(name_fnt, (int)(tv.font_size * 0.92) * PANGO_SCALE);

        PangoFontDescription *badge_fnt = pango_font_description_copy(fnt);
        pango_font_description_set_size(badge_fnt, 6 * PANGO_SCALE);

        for (int i = 0; i < n; i++) {
            int col = i % COLS;
            int row = i / COLS;
            double card_x = cx + SIDE_PAD + col * (card_w + CARD_GAP);
            double card_y = area_y + HEADER_H + row * (CARD_H + CARD_GAP) - themes_scroll;
            const ThemeVariant &t = themes[i];
            bool sel = (i == selected_theme_idx);
            bool hov = (i == hovered_theme_idx);

            if (card_y + CARD_H < area_y + HEADER_H || card_y > area_y + ch) continue;

            double rr2 = g_rounded_corners ? 10.0 : 2.0;


            // Card background - mini editor preview
            cairo_set_source_rgb(cr, t.bg, t.bg, t.bg);
            rrect(cr, card_x, card_y, card_w, CARD_H, rr2);
            cairo_fill(cr);

            // Clip to card for content
            cairo_save(cr);
            rrect(cr, card_x, card_y, card_w, CARD_H, rr2);
            cairo_clip(cr);

            // Mini gutter strip
            double gut_w = card_w * 0.10;
            cairo_set_source_rgb(cr, t.gutter_bg, t.gutter_bg, t.gutter_bg);
            cairo_rectangle(cr, card_x, card_y, gut_w, CARD_H);
            cairo_fill(cr);
            // Gutter separator
            cairo_set_source_rgba(cr, t.muted, t.muted, t.muted, 0.15);
            cairo_rectangle(cr, card_x + gut_w, card_y, 1, CARD_H);
            cairo_fill(cr);

            // Tab bar strip at top (mock editor chrome)
            double tab_h = 14;
            double tab_bg = t.panel;
            cairo_set_source_rgb(cr, tab_bg, tab_bg, tab_bg + 0.01);
            cairo_rectangle(cr, card_x + gut_w + 1, card_y, card_w - gut_w - 1, tab_h);
            cairo_fill(cr);
            // Fake tab label
            cairo_set_source_rgba(cr, t.accent_r, t.accent_g, t.accent_b, 0.75);
            cairo_rectangle(cr, card_x + gut_w + 5, card_y + tab_h - 1.5, 22, 1.5);
            cairo_fill(cr);
            cairo_set_source_rgba(cr, t.text, t.text, t.text, 0.55);
            cairo_rectangle(cr, card_x + gut_w + 5, card_y + 4, 16, 4);
            cairo_fill(cr);

            // Accent bottom border of tab bar
            cairo_set_source_rgba(cr, t.accent_r, t.accent_g, t.accent_b, 0.30);
            cairo_rectangle(cr, card_x + gut_w + 1, card_y + tab_h, card_w - gut_w - 1, 1);
            cairo_fill(cr);

            // Code lines (5 lines)
            struct ML { double indent, len, r, g, b; };
            ML lines[5] = {
                {0.0, 0.65, t.keyword_r,  t.keyword_g,  t.keyword_b},
                {1.0, 0.55, t.function_r, t.function_g, t.function_b},
                {2.0, 0.40, t.string_r,   t.string_g,   t.string_b},
                {1.0, 0.25, t.text*0.85,  t.text*0.85,  t.text*0.85},
                {0.0, 0.38, t.comment_r,  t.comment_g,  t.comment_b},
            };
            double code_top2 = card_y + tab_h + 5;
            double code_h2   = CARD_H - tab_h - 28;
            double lh2       = code_h2 / 5.0;
            double cx2       = card_x + gut_w + 6;
            double cw2       = card_w - gut_w - 12;

            // Current-line highlight on line 1
            cairo_set_source_rgba(cr, t.accent_r, t.accent_g, t.accent_b, 0.07);
            cairo_rectangle(cr, card_x + gut_w + 1, code_top2 + lh2, cw2, lh2);
            cairo_fill(cr);

            // Gutter line numbers
            for (int li = 0; li < 5; li++) {
                double dot_x = card_x + gut_w * 0.5;
                double dot_y = code_top2 + li * lh2 + lh2 * 0.3;
                double dot_w = gut_w * 0.45;
                cairo_set_source_rgba(cr, t.gutter_text, t.gutter_text, t.gutter_text, 0.55);
                cairo_rectangle(cr, dot_x - dot_w * 0.5, dot_y, dot_w, lh2 * 0.45);
                cairo_fill(cr);
            }

            // Code bars
            for (int li = 0; li < 5; li++) {
                if (lines[li].len < 0.05) continue;
                double lx = cx2 + lines[li].indent * 5;
                double lw2b = std::max(4.0, (cw2 - lines[li].indent * 5) * lines[li].len);
                double ly2 = code_top2 + li * lh2 + lh2 * 0.22;
                double bar_h2 = lh2 * 0.52;
                double br = g_rounded_corners ? 1.5 : 0.0;
                cairo_set_source_rgba(cr, lines[li].r, lines[li].g, lines[li].b, 0.88);
                rrect(cr, lx, ly2, lw2b, bar_h2, br);
                cairo_fill(cr);
            }

            // Cursor
            cairo_set_source_rgba(cr, t.cursor_r, t.cursor_g, t.cursor_b, 0.90);
            double cur_x = cx2 + (card_w - gut_w - 12) * 0.38;
            double cur_y = code_top2 + lh2;
            cairo_rectangle(cr, cur_x, cur_y + lh2 * 0.1, 1.5, lh2 * 0.8);
            cairo_fill(cr);

            // Bottom info bar: name + dots + badge
            double info_y2 = card_y + CARD_H - 26;
            double info_h2 = 26.0;
            double info_bg = t.panel * 0.88;
            cairo_set_source_rgb(cr, info_bg, info_bg, info_bg + 0.01);
            cairo_rectangle(cr, card_x, info_y2, card_w, info_h2);
            cairo_fill(cr);

            // Theme name (left side, clipped)
            {
                pango_font_description_set_weight(name_fnt,
                    sel ? PANGO_WEIGHT_SEMIBOLD : PANGO_WEIGHT_NORMAL);
                pango_layout_set_font_description(layout, name_fnt);
                pango_layout_set_text(layout, t.name.c_str(), -1);
                pango_layout_set_width(layout, (card_w - 54) * PANGO_SCALE);
                pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
                int nlw2, nlh2; pango_layout_get_pixel_size(layout, &nlw2, &nlh2);
                cairo_save(cr);
                cairo_rectangle(cr, card_x + 6, info_y2, card_w - 52, info_h2);
                cairo_clip(cr);
                if (sel)
                    cairo_set_source_rgb(cr, t.accent_r, t.accent_g, t.accent_b);
                else
                    cairo_set_source_rgba(cr, tv.text, tv.text, tv.text, 0.82);
                cairo_move_to(cr, card_x + 7, info_y2 + (info_h2 - nlh2) / 2.0);
                pango_cairo_show_layout(cr, layout);
                cairo_restore(cr);
                pango_layout_set_font_description(layout, fnt);
            }

            // DARK/LIGHT badge (right side)
            {
                pango_layout_set_font_description(layout, badge_fnt);
                const char *badge_txt = t.is_light ? "L" : "D";
                pango_layout_set_text(layout, badge_txt, -1);
                pango_layout_set_width(layout, -1);
                int bw2, bh2; pango_layout_get_pixel_size(layout, &bw2, &bh2);
                double badge_x = card_x + card_w - bw2 - 7;
                double badge_y = info_y2 + (info_h2 - bh2) / 2.0;
                cairo_set_source_rgba(cr,
                    t.is_light ? 0.7 : 0.45,
                    t.is_light ? 0.7 : 0.45,
                    t.is_light ? 0.5 : 0.75,
                    0.65);
                cairo_move_to(cr, badge_x, badge_y);
                pango_cairo_show_layout(cr, layout);
                pango_layout_set_font_description(layout, fnt);
            }

            cairo_restore(cr); // end card clip

            // Card border - glowing accent when selected
            double border_alpha = sel ? 1.0 : (hov ? 0.40 : 0.14);
            double bw2 = sel ? 2.0 : 1.0;
            if (sel) {
                cairo_set_source_rgba(cr, t.accent_r, t.accent_g, t.accent_b, border_alpha);
            } else {
                cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, border_alpha);
            }
            cairo_set_line_width(cr, bw2);
            rrect(cr, card_x + bw2/2, card_y + bw2/2,
                  card_w - bw2, CARD_H - bw2, rr2);
            cairo_stroke(cr);

            // (name is drawn inside info bar - no label below card)
        }

        pango_font_description_free(name_fnt);
        pango_font_description_free(badge_fnt);
        cairo_restore(cr); // end grid clip

        // Scrollbar
        if (themes_total_height > themes_view_height) {
            double sp_t = g_animations_enabled ? 0.22 : 1.0;
            scroll_press_anim_t += (scroll_pressed ? 1.0 : 0.0 - scroll_press_anim_t) * sp_t;
            scrollbar(cr, cx + cw - 8, (double)(area_y + HEADER_H),
                      themes_view_height - HEADER_H,
                      themes_scroll, themes_total_height, tv,
                      scroll_press_anim_t, &themes_scroll);
        }
    }

void SettingsTab::draw_custom_picker(cairo_t *cr,PangoLayout *layout,PangoFontDescription *fnt,
                            double px,double py,double pw,double ph,
                            const ThemeVariant &tv) {
        // Field labels + swatches
        struct CField { const char *label; double *r,*g,*b; };
        ThemeVariant &ct = themes[selected_theme_idx]; // mutable ref
        CField fields[]={
            {"Background",  &ct.bg,        &ct.bg,        &ct.bg},
            {"Panel",       &ct.panel,     &ct.panel,     &ct.panel},
            {"Text",        &ct.text,      &ct.text,      &ct.text},
            {"Accent",      &ct.accent_r,  &ct.accent_g,  &ct.accent_b},
            {"Keyword",     &ct.keyword_r, &ct.keyword_g, &ct.keyword_b},
            {"Function",    &ct.function_r,&ct.function_g,&ct.function_b},
            {"String",      &ct.string_r,  &ct.string_g,  &ct.string_b},
            {"Comment",     &ct.comment_r, &ct.comment_g, &ct.comment_b},
            {"Cursor",      &ct.cursor_r,  &ct.cursor_g,  &ct.cursor_b},
            {"Selection",   &ct.selection_r,&ct.selection_g,&ct.selection_b},
        };
        int nf=10;
        double fw=pw*0.30, fh=28.0, fpad=6.0;
        double fy=py;

        // Left column: field list
        {
            PangoFontDescription *sf=pango_font_description_copy(fnt);
            pango_font_description_set_size(sf,9*PANGO_SCALE);
            pango_layout_set_font_description(layout,sf);
            for(int i=0;i<nf;i++){
                double iy=fy+i*(fh+fpad);
                bool sel=(i==custom_picker_field);
                // Row bg
                if(sel){
                    cairo_set_source_rgba(cr,tv.accent_r,tv.accent_g,tv.accent_b,0.18);
                    rrect(cr,px,iy,fw,fh,5); cairo_fill(cr);
                }
                // Colour swatch
                double sr=*fields[i].r, sg=*fields[i].g, sb=*fields[i].b;
                // For greyscale fields (bg, panel, text) use the grey value
                cairo_set_source_rgb(cr,sr,sg,sb);
                rrect(cr,px+4,iy+4,fh-8,fh-8,3); cairo_fill(cr);
                cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,0.3);
                rrect(cr,px+4,iy+4,fh-8,fh-8,3); cairo_stroke(cr);
                // Label
                pango_layout_set_text(layout,fields[i].label,-1);
                pango_layout_set_width(layout,-1);
                cairo_set_source_rgba(cr,tv.text,tv.text,tv.text,sel?1.0:0.75);
                cairo_move_to(cr,px+fh+4,iy+(fh-10)/2.0);
                pango_cairo_show_layout(cr,layout);
            }
            pango_font_description_free(sf);
            pango_layout_set_font_description(layout,fnt);
        }

        // Right: Picker (SV square + Hue strip)
        double rx=px+fw+12, rw=pw-fw-12;
        double sq_size=std::min(rw-28.0, ph-40.0);
        sq_size=std::max(sq_size,80.0);
        double sq_x=rx, sq_y=py;
        double hue_x=rx+sq_size+8, hue_w=14.0, hue_h=sq_size;

        // Cache picker rects for hit-testing
        cp_px=sq_x; cp_py=sq_y; cp_pw=sq_size; cp_ph=sq_size;
        cp_hx=hue_x; cp_hy=sq_y; cp_hw=hue_w; cp_hh=hue_h;

        // Hue bar (vertical gradient via segments)
        int hsegs=60;
        for(int hi=0;hi<hsegs;hi++){
            double hf=(double)hi/hsegs;
            double r2,g2,b2; hsv_to_rgb(hf,1,1,r2,g2,b2);
            cairo_set_source_rgb(cr,r2,g2,b2);
            double sy2=sq_y+hf*hue_h, sh2=hue_h/hsegs+1;
            cairo_rectangle(cr,hue_x,sy2,hue_w,sh2); cairo_fill(cr);
        }
        // Hue indicator
        double hind_y=sq_y+custom_picker_hue*hue_h;
        cairo_set_source_rgb(cr,1,1,1);
        cairo_set_line_width(cr,1.5);
        cairo_rectangle(cr,hue_x-2,hind_y-3,hue_w+4,6); cairo_stroke(cr);

        // SV square (drawn as gradient mesh via rows)
        if(sq_size>10){
            int rows=40,cols=40;
            double cw2=sq_size/cols, ch2=sq_size/rows;
            for(int ri=0;ri<rows;ri++){
                for(int ci=0;ci<cols;ci++){
                    double s2=(double)ci/cols, v2=1.0-(double)ri/rows;
                    double r2,g2,b2; hsv_to_rgb(custom_picker_hue,s2,v2,r2,g2,b2);
                    cairo_set_source_rgb(cr,r2,g2,b2);
                    cairo_rectangle(cr,sq_x+ci*cw2,sq_y+ri*ch2,cw2+1,ch2+1);
                    cairo_fill(cr);
                }
            }
            // Crosshair
            double cx2=sq_x+custom_picker_sat*sq_size;
            double cy2=sq_y+(1.0-custom_picker_val)*sq_size;
            cairo_set_source_rgb(cr,1,1,1);
            cairo_set_line_width(cr,1.5);
            shape::disc(cr, cx2, cy2, 6); cairo_stroke(cr);
            cairo_set_source_rgb(cr,0,0,0);
            shape::disc(cr, cx2, cy2, 4); cairo_stroke(cr);
        }

        // Border around square
        cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,0.25);
        cairo_set_line_width(cr,1.0);
        cairo_rectangle(cr,sq_x,sq_y,sq_size,sq_size); cairo_stroke(cr);

        // Current colour preview chip
        if(custom_picker_field>=0 && custom_picker_field<nf){
            double pr=0,pg=0,pb=0;
            hsv_to_rgb(custom_picker_hue,custom_picker_sat,custom_picker_val,pr,pg,pb);
            double chip_y=sq_y+sq_size+6;
            cairo_set_source_rgb(cr,pr,pg,pb);
            rrect(cr,sq_x,chip_y,sq_size/2-4,18,4); cairo_fill(cr);
            // Hex label
            char hex[16];
            snprintf(hex,sizeof(hex),"#%02X%02X%02X",(int)(pr*255),(int)(pg*255),(int)(pb*255));
            PangoFontDescription *sf=pango_font_description_copy(fnt);
            pango_font_description_set_size(sf,8*PANGO_SCALE);
            pango_layout_set_font_description(layout,sf);
            pango_layout_set_text(layout,hex,-1);
            cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,0.7);
            cairo_move_to(cr,sq_x+sq_size/2+4,chip_y+3);
            pango_cairo_show_layout(cr,layout);
            pango_font_description_free(sf);
            pango_layout_set_font_description(layout,fnt);
        }

        // Hint
        {
            PangoFontDescription *sf=pango_font_description_copy(fnt);
            pango_font_description_set_size(sf,8*PANGO_SCALE);
            pango_layout_set_font_description(layout,sf);
            pango_layout_set_text(layout,"Click a field, then pick colour in square",-1);
            cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,0.4);
            cairo_move_to(cr,px,py+ph-12);
            pango_cairo_show_layout(cr,layout);
            pango_font_description_free(sf);
            pango_layout_set_font_description(layout,fnt);
        }
    }

bool SettingsTab::handle_custom_picker_click(int mx,int my,int cx,int area_y,int cw,int /*ch*/) {
        if(themes[selected_theme_idx].id!="custom") return false;
        ThemeVariant &ct=themes[selected_theme_idx];

        struct CField{double*r,*g,*b;};
        CField fields[]={
            {&ct.bg,        &ct.bg,        &ct.bg},
            {&ct.panel,     &ct.panel,     &ct.panel},
            {&ct.text,      &ct.text,      &ct.text},
            {&ct.accent_r,  &ct.accent_g,  &ct.accent_b},
            {&ct.keyword_r, &ct.keyword_g, &ct.keyword_b},
            {&ct.function_r,&ct.function_g,&ct.function_b},
            {&ct.string_r,  &ct.string_g,  &ct.string_b},
            {&ct.comment_r, &ct.comment_g, &ct.comment_b},
            {&ct.cursor_r,  &ct.cursor_g,  &ct.cursor_b},
            {&ct.selection_r,&ct.selection_g,&ct.selection_b},
        };
        int nf=10;

        // Field list click - uses cp_px as anchor; field list is to the left of picker
        double fw=cp_pw*0.42+12; // rough width mirror of draw layout
        double fh=28.0, fpad=6.0;
        double field_x=cp_px-fw-12; // left of SV square
        for(int i=0;i<nf;i++){
            double iy=cp_py+i*(fh+fpad);
            if(mx>=(int)field_x && mx<=(int)(field_x+fw) && my>=(int)iy && my<=(int)(iy+fh)){
                custom_picker_field=i;
                rgb_to_hsv(*fields[i].r,*fields[i].g,*fields[i].b,
                           custom_picker_hue,custom_picker_sat,custom_picker_val);
                return true;
            }
        }

        // SV square
        if(cp_pw>0 && mx>=cp_px && mx<=cp_px+cp_pw && my>=cp_py && my<=cp_py+cp_ph){
            custom_picker_sat=std::clamp((mx-cp_px)/cp_pw,0.0,1.0);
            custom_picker_val=1.0-std::clamp((my-cp_py)/cp_ph,0.0,1.0);
            if(custom_picker_field>=0 && custom_picker_field<nf){
                double r2,g2,b2;
                hsv_to_rgb(custom_picker_hue,custom_picker_sat,custom_picker_val,r2,g2,b2);
                if(custom_picker_field<=2){
                    double grey=(r2*0.299+g2*0.587+b2*0.114);
                    *fields[custom_picker_field].r=grey;
                    *fields[custom_picker_field].g=grey;
                    *fields[custom_picker_field].b=grey;
                } else {
                    *fields[custom_picker_field].r=r2;
                    *fields[custom_picker_field].g=g2;
                    *fields[custom_picker_field].b=b2;
                }
                ct.gutter_bg=ct.bg*0.95; ct.surface=ct.bg+0.03;
                ct.elevated=ct.panel+0.03;
                ct.icon_r=ct.text; ct.icon_g=ct.text; ct.icon_b=ct.text;
            }
            return true;
        }

        // Hue bar
        if(cp_hh>0 && mx>=(int)cp_hx && mx<=(int)(cp_hx+cp_hw) && my>=(int)cp_hy && my<=(int)(cp_hy+cp_hh)){
            custom_picker_hue=std::clamp((my-cp_hy)/cp_hh,0.0,1.0);
            if(custom_picker_field>=0 && custom_picker_field<nf){
                double r2,g2,b2;
                hsv_to_rgb(custom_picker_hue,custom_picker_sat,custom_picker_val,r2,g2,b2);
                if(custom_picker_field>2){
                    *fields[custom_picker_field].r=r2;
                    *fields[custom_picker_field].g=g2;
                    *fields[custom_picker_field].b=b2;
                }
            }
            return true;
        }
        (void)cx;(void)area_y;(void)cw;
        return false;
    }

#endif
