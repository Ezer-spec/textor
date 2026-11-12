#ifndef SETTINGS_KEYS_HXX
#define SETTINGS_KEYS_HXX
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

void SettingsTab::draw_keys(cairo_t *cr,PangoLayout *layout,PangoFontDescription *fnt,
                   int cx,int cy,int cw,int ch,const ThemeVariant &tv) {
        const int pad=36;
        draw_heading(cr,layout,fnt,"Keybindings",cx+pad,cy+28,tv);
        div(cr,cx+pad,cy+62,cw-pad*2,tv);

        // Reset All button
        {
            double rb_x=cx+cw-pad-110,rb_y=cy+28;
            bool hov=(mouse_x>=rb_x&&mouse_x<rb_x+110&&mouse_y>=rb_y&&mouse_y<rb_y+32);
            cairo_set_source_rgba(cr,tv.accent_r,tv.accent_g,tv.accent_b,hov?0.18:0.08);
            rrect(cr,rb_x,rb_y,110,32,8); cairo_fill(cr);
            cairo_set_source_rgb(cr,tv.accent_r,tv.accent_g,tv.accent_b);
            PangoFontDescription *sf2=pango_font_description_copy(fnt);
            pango_font_description_set_size(sf2,10*PANGO_SCALE);
            pango_layout_set_font_description(layout,sf2);
            pango_layout_set_text(layout,"Reset All",-1);
            int pw2,ph2; pango_layout_get_pixel_size(layout,&pw2,&ph2);
            cairo_move_to(cr,rb_x+(110-pw2)/2.0,rb_y+(32-ph2)/2.0); pango_cairo_show_layout(cr,layout);
            pango_font_description_free(sf2); pango_layout_set_font_description(layout,fnt);
        }

        const int ROW_H=44; int row_y=cy+80; double view_h=ch-80;

        if(waiting_for_key&&editing_key_idx>=0){
            std::string hint="Press new key for: "+shortcuts.shortcuts[editing_key_idx].display+"  (Esc=cancel)";
            PangoFontDescription *sf2=pango_font_description_copy(fnt);
            pango_font_description_set_size(sf2,10*PANGO_SCALE);
            pango_layout_set_font_description(layout,sf2);
            cairo_set_source_rgba(cr,tv.warning_r,tv.warning_g,tv.warning_b,0.9);
            pango_layout_set_text(layout,hint.c_str(),-1);
            cairo_move_to(cr,cx+pad,cy+66); pango_cairo_show_layout(cr,layout);
            pango_font_description_free(sf2); pango_layout_set_font_description(layout,fnt);
        }

        cairo_save(cr); cairo_rectangle(cr,cx,row_y,cw,view_h); cairo_clip(cr);
        PangoFontDescription *sf=pango_font_description_copy(fnt);
        pango_font_description_set_size(sf,10*PANGO_SCALE);

        for(int i=0;i<(int)shortcuts.shortcuts.size();++i){
            const auto &sc=shortcuts.shortcuts[i];
            double ry2=row_y+i*ROW_H-keys_scroll;
            if(ry2+ROW_H<row_y||ry2>row_y+view_h) continue;
            bool ie=(i==editing_key_idx),ih=(i==hovered_key_idx);

            if(ie){
                cairo_set_source_rgba(cr,tv.accent_r,tv.accent_g,tv.accent_b,0.12);
                cairo_rectangle(cr,cx+pad-8,ry2,cw-pad+8,ROW_H-1); cairo_fill(cr);
                double t2=std::fmod(std::chrono::duration_cast<std::chrono::duration<double>>(
                    std::chrono::steady_clock::now().time_since_epoch()).count(),1.0);
                cairo_set_source_rgba(cr,tv.accent_r,tv.accent_g,tv.accent_b,0.3+0.4*std::abs(std::sin(t2*M_PI*2)));
                cairo_set_line_width(cr,1.5);
                cairo_rectangle(cr,cx+pad-7,ry2+1,cw-pad+6,ROW_H-3); cairo_stroke(cr);
            } else if(i%2==0){
                cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,0.04);
                cairo_rectangle(cr,cx+pad-8,ry2,cw-pad+8,ROW_H-1); cairo_fill(cr);
            }
            if(ih&&!ie){ cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,0.07); cairo_rectangle(cr,cx+pad-8,ry2,cw-pad+8,ROW_H-1); cairo_fill(cr); }

            pango_layout_set_font_description(layout,sf);
            cairo_set_source_rgba(cr,tv.text,tv.text,tv.text,0.85);
            pango_layout_set_text(layout,sc.display.c_str(),-1);
            cairo_move_to(cr,cx+pad,ry2+(ROW_H-16)/2.0); pango_cairo_show_layout(cr,layout);

            std::string chord=sc.chord(); if(ie&&waiting_for_key) chord="...";
            int kw2,kh2; pango_layout_set_text(layout,chord.c_str(),-1); pango_layout_get_pixel_size(layout,&kw2,&kh2);
            double bp=9,bw2=kw2+bp*2,bh2=kh2+6,bx=cx+cw-pad-bw2-14,by2=ry2+(ROW_H-bh2)/2.0;
            if(ie) cairo_set_source_rgba(cr,tv.accent_r,tv.accent_g,tv.accent_b,0.22);
            else   cairo_set_source_rgba(cr,tv.elevated*1.6,tv.elevated*1.6,tv.elevated*1.6,0.9);
            rrect(cr,bx,by2,bw2,bh2,5); cairo_fill(cr);
            cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,0.2); cairo_set_line_width(cr,1.0);
            rrect(cr,bx,by2,bw2,bh2,5); cairo_stroke(cr);
            cairo_set_source_rgba(cr,tv.accent_r,tv.accent_g,tv.accent_b,0.9);
            cairo_move_to(cr,bx+bp,by2+3); pango_cairo_show_layout(cr,layout);
            if(ih&&!ie){
                PangoFontDescription *sf3=pango_font_description_copy(fnt);
                pango_font_description_set_size(sf3,9*PANGO_SCALE);
                pango_layout_set_font_description(layout,sf3);
                cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,0.5);
                pango_layout_set_text(layout,"click to rebind",-1);
                int hw,hh; pango_layout_get_pixel_size(layout,&hw,&hh);
                cairo_move_to(cr,bx-hw-12,ry2+(ROW_H-hh)/2.0); pango_cairo_show_layout(cr,layout);
                pango_font_description_free(sf3); pango_layout_set_font_description(layout,fnt);
            }
        }
        pango_font_description_free(sf); pango_layout_set_font_description(layout,fnt);
        cairo_restore(cr);

        double total_h=shortcuts.shortcuts.size()*ROW_H;
        keys_total_height = total_h;
        if(total_h>view_h){
            double sp_k = g_animations_enabled ? 0.22 : 1.0;
            scroll_press_anim_k += (scroll_pressed ? 1.0 : 0.0 - scroll_press_anim_k) * sp_k;
            scrollbar(cr,cx+cw-10,(double)row_y,view_h,keys_scroll,total_h,tv, scroll_press_anim_k, &keys_scroll);
        }
    }

bool SettingsTab::handle_key_for_rebind(XKeyEvent *kev) {
        if (!waiting_for_key || editing_key_idx<0) return false;
        KeySym ks=XLookupKeysym(kev,0);
        if (ks==XK_Control_L||ks==XK_Control_R||ks==XK_Shift_L||ks==XK_Shift_R||
            ks==XK_Alt_L||ks==XK_Alt_R||ks==XK_Super_L||ks==XK_Super_R) return true;
        if (ks==XK_Escape) { editing_key_idx=-1; waiting_for_key=false; return true; }
        bool ctrl=(kev->state&ControlMask)!=0, shift=(kev->state&ShiftMask)!=0, alt=(kev->state&Mod1Mask)!=0;
        char buf[32]={}; int len=XLookupString(kev,buf,sizeof(buf)-1,nullptr,nullptr);
        std::string kn;
        if (len>0&&buf[0]>=0x20&&buf[0]<0x7f) kn+=(char)std::toupper((unsigned char)buf[0]);
        else { const char *k2=XKeysymToString(ks); kn=k2?k2:"?"; }
        if (kn=="Return") kn="Enter"; if (kn=="Prior") kn="PageUp"; if (kn=="Next") kn="PageDown";
        auto &sc=shortcuts.shortcuts[editing_key_idx];
        sc.ctrl=ctrl; sc.shift=shift; sc.alt=alt; sc.key=ks; sc.key_name=kn;
        editing_key_idx=-1; waiting_for_key=false; shortcuts.save(); return true;
    }

#endif
