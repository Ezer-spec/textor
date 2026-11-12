#ifndef SETTINGS_HELP_ABOUT_HXX
#define SETTINGS_HELP_ABOUT_HXX
// settings_help_about.hxx - out-of-line SettingsTab method definitions,
// split out of the former single ~2300-line settings_tab.hxx for readability.! Yes Yes.!
// Included at the bottom of settings_tab.hxx, after the class body (which still
// declares every one of these as a plain prototype) - same pattern as a normal
// .cxx would use, just kept header-only like the rest of this codebase.
// Do not include this file directly; it depends on SettingsTab already being
// fully declared.

void SettingsTab::draw_help(cairo_t *cr,PangoLayout *layout,PangoFontDescription *fnt,
                    int cx,int cy,int cw,int ch,const ThemeVariant &tv) {
        const int pad=36;
        draw_heading(cr,layout,fnt,"Keyboard Shortcuts",cx+pad,cy+28,tv);
        div(cr,cx+pad,cy+62,cw-pad*2,tv);
        struct Group{const char*n;struct E{const char*k;const char*d;};std::vector<E>e;};
        std::vector<Group> groups={
            {"File",{{"Ctrl+S","Save file"},{"Ctrl+W","Close tab"}}},
            {"Tabs",{{"Ctrl+Tab","Next tab"},{"Ctrl+PgDn","Next tab"},{"Ctrl+PgUp","Prev tab"}}},
            {"Edit",{{"Ctrl+A","Select all"},{"Ctrl+C","Copy"},{"Ctrl+X","Cut"},{"Ctrl+V","Paste"},{"Ctrl+D","Duplicate line"},{"Tab","Indent"},{"Enter","New line"}}},
            {"Navigate",{{"Arrows","Move cursor"},{"Home","Line start / first non-ws"},{"End","Line end"}}},
            {"Select",{{"Shift+Arrows","Extend selection"},{"Shift+Click","Extend to click"},{"Ctrl+Click","Add extra cursor"},{"Escape","Clear extra cursors"}}},
            {"Scroll",{{"Wheel","Scroll vertically"},{"Shift+Wheel","Scroll horizontally"},{"Wheel on Console","Scroll console"},{"Wheel on Sidebar","Scroll file list"}}},
            {"Console",{{"Up/Down","Command history"},{"PgUp","Scroll up"},{"PgDn","Scroll down"},{"Ctrl+C","Interrupt"},{"Ctrl+L","Clear"},{"Escape","Close"}}},
            {"Interface",{{"Settings","Open settings"},{"Console btn","Toggle terminal"},{"Sidebar btn","Toggle files"},{"Open Folder","Navigate to path"}}},
        };
        const double GG=24,RH=34,GH=32;
        double total_h=8; for(auto&g:groups) total_h+=GH+g.e.size()*RH+GG;
        help_total_height = total_h;
        double view_h=ch-72;
        help_scroll=std::clamp(help_scroll,0.0,std::max(0.0,total_h-view_h));
        cairo_save(cr); cairo_rectangle(cr,cx,cy+70,cw,view_h); cairo_clip(cr);
        double ry=cy+74-help_scroll;
        PangoFontDescription *sf=pango_font_description_copy(fnt);
        pango_font_description_set_size(sf,(int)(tv.font_size*0.9)*PANGO_SCALE);
        for(auto&g:groups){
            pango_font_description_set_weight(fnt,PANGO_WEIGHT_SEMIBOLD);
            pango_font_description_set_size(fnt,(tv.font_size+1)*PANGO_SCALE);
            pango_layout_set_font_description(layout,fnt);
            cairo_set_source_rgba(cr,tv.accent_r,tv.accent_g,tv.accent_b,0.85);
            pango_layout_set_text(layout,g.n,-1);
            cairo_move_to(cr,cx+pad,ry+6); pango_cairo_show_layout(cr,layout);
            cairo_set_source_rgba(cr,tv.accent_r,tv.accent_g,tv.accent_b,0.28);
            cairo_rectangle(cr,cx+pad,ry+GH-4,cw-pad*2,1); cairo_fill(cr);
            pango_font_description_set_weight(fnt,PANGO_WEIGHT_NORMAL);
            pango_font_description_set_size(fnt,tv.font_size*PANGO_SCALE);
            pango_layout_set_font_description(layout,fnt); ry+=GH;
            for(int ei=0;ei<(int)g.e.size();++ei){
                if(ei%2==0){cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,0.04); cairo_rectangle(cr,cx+pad-8,ry,cw-pad*2+16,RH); cairo_fill(cr);}
                pango_layout_set_font_description(layout,sf);
                pango_font_description_set_weight(sf,PANGO_WEIGHT_MEDIUM);
                pango_layout_set_text(layout,g.e[ei].k,-1);
                int kw,kh; pango_layout_get_pixel_size(layout,&kw,&kh);
                double bp=8,bw2=kw+bp*2,bh2=kh+6,bx=cx+pad+210-bw2,by2=ry+(RH-bh2)/2.0;
                cairo_set_source_rgba(cr,tv.elevated*1.5,tv.elevated*1.5,tv.elevated*1.5,0.9);
                rrect(cr,bx,by2,bw2,bh2,5); cairo_fill(cr);
                cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,0.25); cairo_set_line_width(cr,1.0);
                rrect(cr,bx,by2,bw2,bh2,5); cairo_stroke(cr);
                cairo_set_source_rgba(cr,tv.accent_r,tv.accent_g,tv.accent_b,0.9);
                cairo_move_to(cr,bx+bp,by2+3); pango_cairo_show_layout(cr,layout);
                pango_font_description_set_weight(sf,PANGO_WEIGHT_NORMAL);
                pango_layout_set_font_description(layout,sf);
                cairo_set_source_rgba(cr,tv.text,tv.text,tv.text,0.8);
                pango_layout_set_text(layout,g.e[ei].d,-1);
                cairo_move_to(cr,cx+pad+230,ry+(RH-kh)/2.0); pango_cairo_show_layout(cr,layout);
                ry+=RH;
            }
            ry+=GG;
        }
        pango_font_description_free(sf); cairo_restore(cr);
        if(total_h>view_h){
            double sp_h = g_animations_enabled ? 0.22 : 1.0;
            scroll_press_anim_h += (scroll_pressed ? 1.0 : 0.0 - scroll_press_anim_h) * sp_h;
            scrollbar(cr,cx+cw-10,(double)(cy+70),view_h,help_scroll,total_h,tv, scroll_press_anim_h, &help_scroll);
        }
    }

void SettingsTab::draw_about(cairo_t *cr,PangoLayout *layout,PangoFontDescription *fnt,
                     int cx,int cy,int cw,int ch,const ThemeVariant &tv) {
        const int pad=36;
        draw_heading(cr,layout,fnt,"About Textor",cx+pad,cy+28,tv);
        div(cr,cx+pad,cy+62,cw-pad*2,tv);
        double ry=cy+90,logo_r=36,lx=cx+pad+logo_r,ly=ry+logo_r;
        cairo_set_source_rgba(cr,tv.accent_r,tv.accent_g,tv.accent_b,0.18);
        shape::disc(cr, lx, ly, logo_r); cairo_fill(cr);
        cairo_set_source_rgb(cr,tv.accent_r,tv.accent_g,tv.accent_b);
        shape::disc(cr, lx, ly, logo_r*0.6); cairo_fill(cr);
        cairo_set_source_rgb(cr,tv.bg,tv.bg,tv.bg);
        pango_font_description_set_weight(fnt,PANGO_WEIGHT_BOLD);
        pango_font_description_set_size(fnt,22*PANGO_SCALE);
        pango_layout_set_font_description(layout,fnt);
        pango_layout_set_text(layout,"T",-1);
        int tw,th_; pango_layout_get_pixel_size(layout,&tw,&th_);
        cairo_move_to(cr,lx-tw/2.0,ly-th_/2.0); pango_cairo_show_layout(cr,layout);
        pango_font_description_set_weight(fnt,PANGO_WEIGHT_NORMAL);
        pango_font_description_set_size(fnt,tv.font_size*PANGO_SCALE);
        pango_layout_set_font_description(layout,fnt);
        double tx2=cx+pad+logo_r*2+20;
        pango_font_description_set_weight(fnt,PANGO_WEIGHT_BOLD);
        pango_font_description_set_size(fnt,(tv.font_size+6)*PANGO_SCALE);
        pango_layout_set_font_description(layout,fnt);
        cairo_set_source_rgb(cr,tv.text,tv.text,tv.text);
        pango_layout_set_text(layout,"Textor IDE",-1);
        cairo_move_to(cr,tx2,ry+8); pango_cairo_show_layout(cr,layout);
        pango_font_description_set_weight(fnt,PANGO_WEIGHT_NORMAL);
        pango_font_description_set_size(fnt,tv.font_size*PANGO_SCALE);
        pango_layout_set_font_description(layout,fnt);
        cairo_set_source_rgba(cr,tv.accent_r,tv.accent_g,tv.accent_b,0.85);
                  pango_layout_set_text(layout,"v2.0.0",-1);
        cairo_move_to(cr,tx2,ry+46); pango_cairo_show_layout(cr,layout);
        ry+=logo_r*2+30; div(cr,cx+pad,(int)ry,cw-pad*2,tv); ry+=20;
        struct R{const char*l;const char*v;};
        R rows[]={{"Built with","C++17, X11, Cairo, Pango"},{"License","LSL"},
                  {"Description","Lightweight code editor built from scratch."}};
        for(auto&r:rows){
            PangoFontDescription *sm=pango_font_description_copy(fnt);
            pango_font_description_set_weight(sm,PANGO_WEIGHT_MEDIUM);
            pango_layout_set_font_description(layout,sm);
            cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,0.85);
            pango_layout_set_text(layout,r.l,-1);
            cairo_move_to(cr,cx+pad,ry); pango_cairo_show_layout(cr,layout);
            pango_font_description_free(sm); pango_layout_set_font_description(layout,fnt);
            cairo_set_source_rgb(cr,tv.text,tv.text,tv.text);
            pango_layout_set_text(layout,r.v,-1);
            cairo_move_to(cr,cx+pad+200,ry); pango_cairo_show_layout(cr,layout);
            int vw2,vh2; pango_layout_get_pixel_size(layout,&vw2,&vh2);
            ry+=std::max(28,vh2+10);
        }

        (void)cw;(void)ch;
    }

#endif
