#ifndef SETTINGS_MODAL_HXX
#define SETTINGS_MODAL_HXX

#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <map>
#include <chrono>
#include "svg_icons.hxx"
#include "../color/themes.hxx"
#include "morphing.hxx"

struct SettingsModal {
    bool visible = false;
    int width = 780, height = 570;
    int current_tab = 0;
    int sidebar_width = 226;

    int mouse_x = 0, mouse_y = 0;
    bool close_hovered = false;

    int hovered_theme_idx = -1;
    int hovered_tab_idx   = -1;

    bool show_line_numbers = true;
    bool show_splash       = true;
    int  indent_size       = 4;
    double gutter_density  = 1.0;
    double line_spacing    = 1.0;

    std::vector<ThemeVariant> themes = default_themes();
    int selected_theme_idx = 0;

    double themes_scroll       = 0.0;
    double themes_total_height = 0.0;
    double themes_view_height  = 0.0;

    bool   dragging_indent    = false;
    bool   dragging_gutter    = false;
    bool   dragging_scrollbar = false;
    double slider_base_x = 0.0, slider_base_w = 0.0;
    bool   mouse_down = false;
    double drag_start_y = 0, scroll_start_val = 0;

    AnimController  open_anim;
    ThemeTween      theme_tween;
    SlideIndicator  tab_indicator;   // flying pill animation

    ToggleButton toggle_line_numbers;
    ToggleButton toggle_splash;

    double indent_anim       = 4.0;
    double gutter_anim       = 1.0;
    double line_spacing_anim = 1.0;

    // Tab card hover spring per theme
    std::vector<SpringValue> card_hover;

    SettingsModal() {
        indent_anim = indent_size;
        gutter_anim = gutter_density;
        line_spacing_anim = line_spacing;
        toggle_line_numbers.value = show_line_numbers;
        toggle_splash.value = show_splash;
        card_hover.resize(themes.size());
        for(auto &s:card_hover){ s.stiffness=600; s.damping=49; }
    }

    // rrect -> morph::rrect (see morphing.hxx)
    void rrect(cairo_t *cr, double x, double y, double w, double h, double r) const {
        morph::rrect(cr, x, y, w, h, r);
    }

    ThemeVariant get_selected_theme() const {
        if (selected_theme_idx>=0&&selected_theme_idx<(int)themes.size()) return themes[selected_theme_idx];
        return themes.empty()?ThemeVariant():themes[0];
    }

    // ............ Tab geometry helpers ............
    static constexpr int TAB_COUNT = 3;
    static constexpr int TAB_H     = 54;
    static constexpr int TAB_PAD_Y = 108;  // y of first tab - space for SETTINGS header

    double tab_y(int i, int modal_y) const { return modal_y + TAB_PAD_Y + i*TAB_H; }

    // ............ Hover / Input ............
    void handle_hover(int mx, int my, int win_w, int win_h) {
        mouse_x=mx; mouse_y=my;
        int x=(win_w-width)/2, y=(win_h-height)/2;
        close_hovered = std::hypot(mx-(x+width-36), my-(y+36)) < 22;
        hovered_tab_idx=-1;
        for(int i=0;i<TAB_COUNT;++i){
            double ty=tab_y(i,y);
            if(mx>=x+14&&mx<=x+sidebar_width-14&&my>=(int)ty&&my<=(int)(ty+TAB_H-2))
                hovered_tab_idx=i;
        }
        hovered_theme_idx=-1;
        if(current_tab==1) compute_hovered_card(mx,my,x,y);
    }

    void compute_hovered_card(int mx,int my,int x,int y){
        int cx=x+sidebar_width+32;
        double av=x+width-56-cx, gap=18.0, mcw=230.0;
        int cols=std::max(1,(int)std::floor((av+gap)/(mcw+gap)));
        cols=std::min(cols,(int)std::max(1,(int)themes.size()));
        double cw=(av-(cols-1)*gap)/cols; cw=std::clamp(cw,140.0,600.0);
        double ch=130.0;
        double ly=my+themes_scroll;
        if(mx>=cx&&mx<=(int)(cx+cw*cols+gap*(cols-1))){
            for(int i=0;i<(int)themes.size();++i){
                int col=i%cols,row=i/cols;
                double tx=cx+col*(cw+gap), ty2=y+96+row*(ch+gap);
                if(mx>=tx&&mx<=tx+cw&&ly>=ty2&&ly<=ty2+ch){hovered_theme_idx=i;break;}
            }
        }
    }

    void handle_scroll(int dy){
        if(current_tab!=1) return;
        themes_scroll+=dy*28;
        double ms=std::max(0.0,themes_total_height-themes_view_height);
        themes_scroll=std::clamp(themes_scroll,0.0,ms);
    }

    bool handle_mouse_down(int mx,int my,int win_w,int win_h){
        mouse_down=true;
        int x=(win_w-width)/2, y=(win_h-height)/2;
        int cx=x+sidebar_width+32;
        if(close_hovered){visible=false;open_anim.start(false,0.20);return true;}
        if(current_tab==0){
            double sl_x=cx+180,sl_w=220;
            if(mx>=sl_x&&mx<=sl_x+sl_w&&my>=y+100&&my<=y+132){
                dragging_indent=true;slider_base_x=sl_x;slider_base_w=sl_w;
                update_line_spacing_from_mouse(mx);return true;
            }
            if(mx>=sl_x&&mx<=sl_x+sl_w&&my>=y+160&&my<=y+192){
                dragging_gutter=true;slider_base_x=sl_x;slider_base_w=sl_w;
                update_gutter_from_mouse(mx);return true;
            }
            if(toggle_line_numbers.is_hit(mx,my)){toggle_line_numbers.toggle();show_line_numbers=toggle_line_numbers.value;return true;}
            if(toggle_splash.is_hit(mx,my)){toggle_splash.toggle();show_splash=toggle_splash.value;return true;}
        }
        if(current_tab==1){
            if(mx>x+width-20&&mx<x+width){dragging_scrollbar=true;drag_start_y=my;scroll_start_val=themes_scroll;return true;}
            double av=x+width-56-cx,gap=18.0,mcw=230.0;
            int cols=std::max(1,(int)std::floor((av+gap)/(mcw+gap)));
            cols=std::min(cols,(int)std::max(1,(int)themes.size()));
            double cw=(av-(cols-1)*gap)/cols;cw=std::clamp(cw,140.0,600.0);
            double ch=130.0;
            double ly=my+themes_scroll;
            if(mx>=cx){
                for(int i=0;i<(int)themes.size();++i){
                    int col=i%cols,row=i/cols;
                    double tx=cx+col*(cw+gap),ty=y+96+row*(ch+gap);
                    if(mx>=tx&&mx<=tx+cw&&ly>=ty&&ly<=ty+ch){
                        if(i!=selected_theme_idx){
                            theme_tween.start(themes[selected_theme_idx],themes[i],0.32);
                            selected_theme_idx=i;
                        }
                        return true;
                    }
                }
            }
        }
        return false;
    }

    bool handle_mouse_move(int mx,int my,int win_w,int win_h){
        mouse_x=mx;mouse_y=my;
        handle_hover(mx,my,win_w,win_h);
        if(dragging_indent){update_line_spacing_from_mouse(mx);return true;}
        if(dragging_gutter){update_gutter_from_mouse(mx);return true;}
        if(dragging_scrollbar&&themes_total_height>themes_view_height){
            double th2=themes_view_height,thumb_r=th2/themes_total_height;
            double thr=th2*thumb_r,scr=th2-thr;
            double ms=themes_total_height-themes_view_height;
            if(scr>0) themes_scroll=std::clamp(scroll_start_val+(my-drag_start_y)*(ms/scr),0.0,ms);
            return true;
        }
        toggle_line_numbers.hovered=toggle_line_numbers.is_hit(mx,my);
        toggle_splash.hovered=toggle_splash.is_hit(mx,my);
        return false;
    }

    bool handle_mouse_up(int,int,int,int){
        bool w=dragging_indent||dragging_gutter||dragging_scrollbar;
        mouse_down=dragging_indent=dragging_gutter=dragging_scrollbar=false;return w;
    }

    bool handle_click(int mx,int my,int win_w,int win_h){
        int x=(win_w-width)/2,y=(win_h-height)/2;
        for(int i=0;i<TAB_COUNT;++i){
            double ty=tab_y(i,y);
            if(mx>=x+14&&mx<=x+sidebar_width-14&&my>=(int)ty&&my<=(int)(ty+TAB_H-2)){
                current_tab=i;
                if(current_tab==1)themes_scroll=0.0;
                return true;
            }
        }
        return false;
    }

    void update_line_spacing_from_mouse(int mx){
        double rel=std::clamp((mx-slider_base_x)/slider_base_w,0.0,1.0);
        line_spacing=0.5+rel*1.5;
        line_spacing_anim=line_spacing_anim*0.85+line_spacing*0.15;
    }
    void update_gutter_from_mouse(int mx){
        double rel=std::clamp((mx-slider_base_x)/slider_base_w,0.0,1.0);
        gutter_density=0.5+rel*1.5;
        gutter_anim=gutter_anim*0.82+gutter_density*0.18;
    }

    // ............ Theme preview miniature ............
    void draw_theme_preview(cairo_t *cr,double x,double y,double w,double h,const ThemeVariant &t) const {
        cairo_set_source_rgb(cr,t.surface,t.surface,t.surface);
        rrect(cr,x,y,w,h,6);cairo_fill(cr);
        cairo_set_source_rgba(cr,t.panel,t.panel,t.panel,0.62);
        rrect(cr,x+5,y+5,w-10,h-10,5);cairo_fill(cr);
        double gw=std::round(w*0.08);
        cairo_set_source_rgba(cr,t.gutter_bg,t.gutter_bg,t.gutter_bg,0.92);
        rrect(cr,x+8,y+10,std::max(5.0,gw-10),h-24,3);cairo_fill(cr);
        double bx=x+gw+16,by2=y+14,lh=13;
        auto bar=[&](double ox,double len,double alpha,double r,double g,double b){
            cairo_set_source_rgba(cr,r,g,b,alpha);
            rrect(cr,bx+ox,by2,len,5,2.5);cairo_fill(cr);
        };
        bar(0,w*0.28,0.95,t.accent_r,t.accent_g,t.accent_b);by2+=lh;
        bar(0,w*0.50,0.72,t.text,t.text,t.text);by2+=lh;
        bar(8,w*0.42,0.46,t.muted,t.muted,t.muted);by2+=lh;
        bar(0,w*0.36,0.72,t.text,t.text,t.text);
    }

    // ............ Main draw ............
    void draw(cairo_t *cr,int win_w,int win_h,double progress_override=-1.0){
        double progress=(progress_override>=0.0)?std::clamp(progress_override,0.0,1.0):open_anim.progress();
        if(progress<=0.001)return;

        ThemeVariant chrome=theme_tween.is_running()?theme_tween.current():themes[selected_theme_idx];

        // Scale + fade modal in with spring feel
        double p=morph::ease_in_out_cubic(progress);
        double scale=1.0;
        int cwx=win_w/2,cwy=win_h/2;
        cairo_save(cr);
        cairo_translate(cr,cwx,cwy);
        cairo_scale(cr,scale,scale);
        cairo_translate(cr,-cwx,-cwy);

        // Backdrop
        cairo_set_source_rgba(cr,0,0,0,0.55*p);
        cairo_rectangle(cr,0,0,win_w,win_h);cairo_fill(cr);

        int x=(win_w-width)/2,y=(win_h-height)/2;

        // Drop shadow через morph::shadow (MD elevation-24)

        // Modal body
        cairo_save(cr);
        double surf=chrome.surface;
        cairo_set_source_rgb(cr,surf,surf,surf+0.01);
        rrect(cr,x,y,width,height,24);cairo_fill(cr);
        // Hairline border
        cairo_set_source_rgba(cr,1,1,1,0.05);
        cairo_set_line_width(cr,0.7);
        rrect(cr,x+0.35,y+0.35,width-0.7,height-0.7,24);cairo_stroke(cr);

        // Sidebar
        double sp=chrome.panel*0.97;
        cairo_set_source_rgb(cr,sp,sp,sp);
        rrect(cr,x,y,sidebar_width,height,24);cairo_clip(cr);
        cairo_rectangle(cr,x,y,sidebar_width,height);cairo_fill(cr);
        cairo_reset_clip(cr);
        cairo_set_source_rgba(cr,0,0,0,0.11);
        cairo_rectangle(cr,x+sidebar_width-1,y,1,height);cairo_fill(cr);
        cairo_restore(cr);

        // Pango setup
        PangoLayout *layout=pango_cairo_create_layout(cr);
        PangoFontDescription *fnt=pango_font_description_from_string((chrome.font_family+" "+std::to_string(chrome.font_size)).c_str());
        pango_layout_set_font_description(layout,fnt);

        // ............ Title "SETTINGS" ............
        {
            // Accent vertical stripe to the left of the title
            cairo_set_source_rgba(cr,chrome.accent_r,chrome.accent_g,chrome.accent_b,0.90);
            morph::rrect(cr, x+16, y+30, 3, 28, 1.5);
            cairo_fill(cr);

            // Text "SETTINGS" - bold uppercase
            PangoFontDescription *hf2 = pango_font_description_copy(fnt);
            pango_font_description_set_size(hf2, (chrome.font_size + 2) * PANGO_SCALE);
            pango_font_description_set_weight(hf2, PANGO_WEIGHT_BOLD);
            pango_layout_set_font_description(layout, hf2);
            pango_layout_set_attributes(layout, NULL);
            pango_layout_set_text(layout, "SETTINGS", -1);
            int htw2, hth2; pango_layout_get_pixel_size(layout, &htw2, &hth2);
            cairo_set_source_rgb(cr, chrome.text, chrome.text, chrome.text);
            cairo_move_to(cr, x+26, y + (TAB_PAD_Y - hth2) / 2.0 - 4);
            pango_cairo_show_layout(cr, layout);
            pango_font_description_free(hf2);
            pango_layout_set_font_description(layout, fnt);

            // Thin separator under the title
            cairo_set_source_rgba(cr, chrome.muted, chrome.muted, chrome.muted, 0.15);
            cairo_set_line_width(cr, 1.0);
            cairo_move_to(cr, x+12, y + TAB_PAD_Y - 8);
            cairo_line_to(cr, x + sidebar_width - 12, y + TAB_PAD_Y - 8);
            cairo_stroke(cr);
        }

        // ............ Flying tab indicator ............
        {
            // Update indicator target
            double target_y=tab_y(current_tab,y)+4.0; // vertical center in TAB_H=54
            double target_h=TAB_H-8; // 46px - fills the line
            tab_indicator.target_at(target_y,target_h);
            tab_indicator.tick();

            // Draw pill/track behind tabs
            double ind_y=tab_indicator.pos_y.value;
            double ind_h=tab_indicator.height.value;
            if(std::abs(ind_h)>1.0){
                // Background pill
                cairo_set_source_rgb(cr,chrome.elevated,chrome.elevated,chrome.elevated);
                rrect(cr,x+14,ind_y,sidebar_width-28,ind_h,10);cairo_fill(cr);
                // Left accent stripe - animated via indicator
                cairo_set_source_rgba(cr,chrome.accent_r,chrome.accent_g,chrome.accent_b,0.95);
                rrect(cr,x+14,ind_y+6,4,ind_h-12,2);cairo_fill(cr);
            }
        }

        // ............ Sidebar tabs ............
        std::vector<std::string> tabs={"General","Themes","About"};
        int icon_x=x+42, label_x=x+76;
        for(int i=0;i<TAB_COUNT;++i){
            double ty=tab_y(i,y);
            bool active=(i==current_tab);
            bool hov=(i==hovered_tab_idx);

            if(!active&&hov){
                // Hover pill - identical to active, but with opacity 0.45
                // Background pill (elevated color, like active)
                cairo_set_source_rgba(cr,
                    chrome.elevated, chrome.elevated, chrome.elevated, 0.55);
                rrect(cr, x+14, ty+4, sidebar_width-28, TAB_H-8, 10);
                cairo_fill(cr);
                // Left accent stripe (like active, but a little thinner/more transparent)
                cairo_set_source_rgba(cr,
                    chrome.accent_r, chrome.accent_g, chrome.accent_b, 0.55);
                rrect(cr, x+14, ty+10, 4, TAB_H-20, 2);
                cairo_fill(cr);
            }

            // Icon
            double ir=active?chrome.accent_r:chrome.icon_r;
            double ig=active?chrome.accent_g:chrome.icon_g;
            double ib=active?chrome.accent_b:chrome.icon_b;
            double icx=icon_x, icy=ty+TAB_H/2.0;
            cairo_save(cr);
            cairo_translate(cr,icx,icy);
            double sc=20.0/24.0;cairo_scale(cr,sc,sc);
            cairo_set_source_rgb(cr,ir,ig,ib);
            cairo_set_line_width(cr,1.3);
            if(i==0){ // General - grid
                for(int r2=0;r2<3;++r2)for(int c2=0;c2<3;++c2){
                    double gx=-10+c2*7,gy=-7+r2*7;
                    cairo_rectangle(cr,gx,gy,4,4);cairo_fill(cr);
                }
            } else if(i==1){ // Themes - palette blob
                cairo_move_to(cr,-9,-4);
                cairo_curve_to(cr,-9,-9,9,-9,9,-4);
                cairo_curve_to(cr,9,1,4,8,0,8);
                cairo_curve_to(cr,-4,8,-9,4,-9,-4);
                cairo_close_path(cr);cairo_set_line_width(cr,1.1);cairo_stroke(cr);
                shape::disc(cr, -3, 0, 1.2);cairo_fill(cr);
                shape::disc(cr, 3, -1, 1.2);cairo_fill(cr);
                shape::disc(cr, 5, 4, 1.2);cairo_fill(cr);
            } else { // About - info circle
                shape::disc(cr, 0, 0, 8);cairo_stroke(cr);
                shape::disc(cr, 0, -3.5, 1.1);cairo_fill(cr);
                cairo_move_to(cr,0,-1);cairo_line_to(cr,0,4.5);cairo_stroke(cr);
            }
            cairo_restore(cr);

            // Label
            double la=active?1.0:0.78;
            cairo_set_source_rgba(cr,chrome.text,chrome.text,chrome.text,la);
            pango_font_description_set_weight(fnt,active?PANGO_WEIGHT_SEMIBOLD:PANGO_WEIGHT_NORMAL);
            pango_layout_set_font_description(layout,fnt);
            pango_layout_set_attributes(layout,NULL);
            pango_layout_set_text(layout,tabs[i].c_str(),-1);
            int lw_tab,lh_tab; pango_layout_get_pixel_size(layout,&lw_tab,&lh_tab);
            // Center inside pill: pill starts at ty+4, height TAB_H-8
            double pill_center_y = ty + 4.0 + (TAB_H - 8 - lh_tab) / 2.0;
            cairo_move_to(cr, label_x, pill_center_y);
            pango_cairo_show_layout(cr,layout);
        }

        int content_x=x+sidebar_width+32;

        // ............ Content area ............
        cairo_save(cr);
        cairo_rectangle(cr,content_x-8,y+1,width-sidebar_width-8,height-2);cairo_clip(cr);

        if(current_tab==0){
            // ............ General ............
            pango_layout_set_markup(layout,"<span size='15000' weight='bold'>General</span>",-1);
            cairo_set_source_rgb(cr,chrome.text,chrome.text,chrome.text);
            cairo_move_to(cr,content_x,y+42);pango_cairo_show_layout(cr,layout);

            pango_font_description_set_weight(fnt,PANGO_WEIGHT_NORMAL);
            pango_layout_set_font_description(layout,fnt);

            auto draw_slider=[&](int sy,double val_anim,double min_v,double max_v,const char*label,const char*vtxt){
                cairo_set_source_rgb(cr,chrome.text,chrome.text,chrome.text);
                pango_layout_set_attributes(layout,NULL);
                pango_layout_set_text(layout,label,-1);
                cairo_move_to(cr,content_x,sy+8);pango_cairo_show_layout(cr,layout);

                double sl_x=content_x+180,sl_w=220,sl_y=sy+20;
                slider_base_x=sl_x;slider_base_w=sl_w;

                // Same look as the sliders of the Welcome screen
                double prog=std::clamp((val_anim-min_v)/(max_v-min_v),0.0,1.0);
                morph::draw_slider_bar(cr,sl_x,sl_y,sl_w,prog,chrome);
                // Value label
                cairo_set_source_rgba(cr,chrome.text,chrome.text,chrome.text,0.70);
                pango_layout_set_text(layout,vtxt,-1);
                cairo_move_to(cr,sl_x+sl_w+22,sy+8);pango_cairo_show_layout(cr,layout);
            };

            char buf[64];
            snprintf(buf,sizeof(buf),"%.2fx",line_spacing);
            if(!dragging_indent)line_spacing_anim=line_spacing_anim*0.86+line_spacing*0.14;
            draw_slider(y+102,line_spacing_anim,0.5,2.0,"Line Spacing",buf);

            snprintf(buf,sizeof(buf),"%.1fx",gutter_density);
            if(!dragging_gutter)gutter_anim=gutter_anim*0.84+gutter_density*0.16;
            draw_slider(y+162,gutter_anim,0.5,2.0,"Gutter Width",buf);

            // Section heading
            cairo_set_source_rgba(cr,chrome.muted,chrome.muted,chrome.muted,0.5);
            cairo_rectangle(cr,content_x,y+218,width-sidebar_width-64,1);cairo_fill(cr);

            // Toggles
            auto draw_toggle_row=[&](ToggleButton &tb,const char *label,double ly2){
                pango_layout_set_attributes(layout,NULL);
                pango_layout_set_text(layout,label,-1);
                cairo_set_source_rgb(cr,chrome.text,chrome.text,chrome.text);
                cairo_move_to(cr,content_x,ly2);pango_cairo_show_layout(cr,layout);
                tb.x=content_x+300;tb.y=ly2-10;tb.w=48;tb.h=28;
                tb.tick();tb.draw(cr,chrome);
            };
            draw_toggle_row(toggle_line_numbers,"Show Line Numbers",y+228);
            draw_toggle_row(toggle_splash,"Show Splash Screen",y+270);

        } else if(current_tab==1){
            // ............ Themes ............
            pango_layout_set_markup(layout,"<span size='15000' weight='bold'>Themes</span>",-1);
            cairo_set_source_rgb(cr,chrome.text,chrome.text,chrome.text);
            cairo_move_to(cr,content_x,y+42);pango_cairo_show_layout(cr,layout);

            // Light/Dark filter pills (visual label only)
            {
                PangoFontDescription *sf2=pango_font_description_from_string((chrome.font_family+" 8").c_str());
                pango_layout_set_font_description(layout,sf2);
                int nd=0,nl=0;
                for(auto &t2:themes){if(t2.is_light)nl++;else nd++;}
                char lb[64];snprintf(lb,sizeof(lb),"%d dark  -  %d light",nd,nl);
                pango_layout_set_text(layout,lb,-1);
                cairo_set_source_rgba(cr,chrome.muted,chrome.muted,chrome.muted,0.5);
                cairo_move_to(cr,content_x,y+62);pango_cairo_show_layout(cr,layout);
                pango_font_description_free(sf2);
                pango_layout_set_font_description(layout,fnt);
            }

            int grid_x=content_x;
            int grid_y=y+92;
            double av=(double)(x+width-56-grid_x),gap=18.0,mcw=230.0;
            int cols=std::max(1,(int)std::floor((av+gap)/(mcw+gap)));
            cols=std::min(cols,(int)std::max(1,(int)themes.size()));
            double cw=(av-(cols-1)*gap)/cols;cw=std::clamp(cw,140.0,600.0);
            double ch=138.0;

            themes_view_height=height-132;
            themes_total_height=std::ceil((double)themes.size()/cols)*(ch+gap);

            cairo_rectangle(cr,grid_x-8,grid_y-8,av+16,themes_view_height+16);cairo_clip(cr);

            // Update spring anims
            double dt2=1.0/60.0;
            if((int)card_hover.size()<(int)themes.size()) card_hover.resize(themes.size());

            for(int i=0;i<(int)themes.size();++i){
                int col=i%cols,row=i/cols;
                double tx=grid_x+col*(cw+gap);
                double ty=grid_y+row*(ch+gap)-themes_scroll;

                if(ty+ch<grid_y||ty>grid_y+themes_view_height)continue;

                bool is_sel=(i==selected_theme_idx);
                bool is_hov=(i==hovered_theme_idx);
                card_hover[i].set_target(is_hov?1.0:0.0);
                card_hover[i].update(dt2);
                double hv=card_hover[i].value;

                // Card scale spring hover
                double scl=1.0;
                double ccx=tx+cw/2.0,ccy=ty+ch/2.0;
                double dw=cw*scl,dh=ch*scl;
                double dx=ccx-dw/2.0,dy=ccy-dh/2.0;


                // Card background
                double cbg=chrome.surface+0.008*hv;
                cairo_set_source_rgb(cr,cbg,cbg,cbg+0.004);
                rrect(cr,dx,dy,dw,dh,11);cairo_fill(cr);

                // Left color stripe from palette
                double stripe_h=dh-24;
                cairo_set_source_rgba(cr,themes[i].accent_r,themes[i].accent_g,themes[i].accent_b,0.55+0.25*hv);
                rrect(cr,dx+6,dy+12,3.5,stripe_h,1.5);cairo_fill(cr);

                // Preview
                double pw2=dw*0.44,ph=dh-28;
                draw_theme_preview(cr,dx+16,dy+10,pw2-20,ph,themes[i]);

                // Right info column
                double rx=dx+pw2+12,rw=dw-pw2-20;
                double sw_y=dy+18,sw_h=9;
                // Accent swatch
                cairo_set_source_rgb(cr,themes[i].accent_r,themes[i].accent_g,themes[i].accent_b);
                rrect(cr,rx,sw_y,rw*0.58,sw_h,3);cairo_fill(cr);
                // Muted swatch
                cairo_set_source_rgba(cr,themes[i].muted,themes[i].muted,themes[i].muted,0.85);
                rrect(cr,rx,sw_y+sw_h+5,rw*0.38,sw_h,3);cairo_fill(cr);
                // Light/dark badge
                {
                    PangoFontDescription *bf=pango_font_description_from_string((chrome.font_family+" 7").c_str());
                    pango_layout_set_font_description(layout,bf);
                    const char *badge=themes[i].is_light?"LIGHT":"DARK";
                    pango_layout_set_text(layout,badge,-1);
                    int bw2,bh2;pango_layout_get_pixel_size(layout,&bw2,&bh2);
                    double bx2=rx,by2=dy+ch-28;
                    cairo_set_source_rgba(cr,themes[i].accent_r,themes[i].accent_g,themes[i].accent_b,0.15);
                    rrect(cr,bx2,by2,bw2+8,bh2+4,3);cairo_fill(cr);
                    cairo_set_source_rgba(cr,themes[i].accent_r,themes[i].accent_g,themes[i].accent_b,0.9);
                    cairo_move_to(cr,bx2+4,by2+2);pango_cairo_show_layout(cr,layout);
                    pango_font_description_free(bf);
                    pango_layout_set_font_description(layout,fnt);
                }

                // Name footer
                double ftr_y=dy+dh-34;
                cairo_set_source_rgba(cr,chrome.panel,chrome.panel,chrome.panel,0.07);
                rrect(cr,dx+6,ftr_y,dw-12,30,8);cairo_fill(cr);
                pango_font_description_set_weight(fnt,PANGO_WEIGHT_BOLD);
                pango_layout_set_font_description(layout,fnt);
                pango_layout_set_attributes(layout,NULL);
                pango_layout_set_text(layout,themes[i].name.c_str(),-1);
                int pw3,ph3;pango_layout_get_pixel_size(layout,&pw3,&ph3);
                cairo_set_source_rgb(cr,chrome.text,chrome.text,chrome.text);
                cairo_move_to(cr,dx+(dw-pw3)/2.0,ftr_y+(30-ph3)/2.0);pango_cairo_show_layout(cr,layout);
                pango_font_description_set_weight(fnt,PANGO_WEIGHT_NORMAL);

                // Selection ring
                if(is_sel){
                    cairo_set_source_rgba(cr,chrome.accent_r,chrome.accent_g,chrome.accent_b,0.85);
                    cairo_set_line_width(cr,2.0);
                    rrect(cr,dx+1.5,dy+1.5,dw-3,dh-3,11);cairo_stroke(cr);
                    // Checkmark badge
                    cairo_set_source_rgb(cr,chrome.accent_r,chrome.accent_g,chrome.accent_b);
                    shape::disc(cr, dx+dw-14, dy+14, 10);cairo_fill(cr);
                    cairo_set_source_rgb(cr,1,1,1);
                    cairo_set_line_width(cr,1.8);
                    cairo_move_to(cr,dx+dw-19,dy+14);
                    cairo_line_to(cr,dx+dw-13.5,dy+18.5);
                    cairo_line_to(cr,dx+dw-8,dy+9);
                    cairo_stroke(cr);
                } else if(hv>0.02){
                    cairo_set_source_rgba(cr,chrome.accent_r,chrome.accent_g,chrome.accent_b,0.4*hv);
                    cairo_set_line_width(cr,1.5);
                    rrect(cr,dx+1,dy+1,dw-2,dh-2,11);cairo_stroke(cr);
                }
            }

            // Scrollbar via MorphScrollBar
            if(themes_total_height>themes_view_height){
                double trk=themes_view_height,ratio=trk/themes_total_height;
                double thr=std::max(40.0,trk*ratio);
                double ms=themes_total_height-themes_view_height;
                double thr_y=grid_y+(themes_scroll/std::max(1.0,ms))*(trk-thr);
                // Track
                cairo_set_source_rgba(cr,chrome.muted,chrome.muted,chrome.muted,0.07);
                morph::rrect(cr,(double)(x+width-16),(double)grid_y,8.0,trk,4.0);
                cairo_fill(cr);
                // Thumb - flat fill, no glare
                cairo_set_source_rgba(cr,chrome.accent_r,chrome.accent_g,chrome.accent_b,0.85);
                morph::rrect(cr,(double)(x+width-16),thr_y,8.0,thr,4.0);
                cairo_fill(cr);
            }

        } else if(current_tab==2){
            // ............ About ............
            // Logo ring
            cairo_set_source_rgba(cr,chrome.accent_r,chrome.accent_g,chrome.accent_b,0.18);
            shape::disc(cr, content_x+30, y+76, 26);cairo_fill(cr);
            cairo_set_source_rgba(cr,chrome.accent_r,chrome.accent_g,chrome.accent_b,0.8);
            cairo_set_line_width(cr,2.0);
            shape::disc(cr, content_x+30, y+76, 26);cairo_stroke(cr);
            cairo_set_source_rgb(cr,chrome.accent_r,chrome.accent_g,chrome.accent_b);
            pango_font_description_set_weight(fnt,PANGO_WEIGHT_BOLD);
            pango_layout_set_font_description(layout,fnt);
            pango_layout_set_text(layout,"T",-1);
            int lw2,lh2;pango_layout_get_pixel_size(layout,&lw2,&lh2);
            cairo_move_to(cr,content_x+30-lw2/2.0,y+76-lh2/2.0);pango_cairo_show_layout(cr,layout);
            pango_font_description_set_weight(fnt,PANGO_WEIGHT_NORMAL);

            pango_layout_set_markup(layout,
                "<span size='20000' weight='bold'>Textor</span>\n"
                "<span size='9000' foreground='#888888'>v1.5.8  -  a lightweight C++ code editor</span>\n\n"
                "<span size='9500'>Built with X11, Cairo, Pango.\n"
                "Tree file manager, syntax highlighting,\n"
                "multi-theme, animated UI.\n\n"
                "</span>"
                "<span size='9000' foreground='#666666'>Designed to be fast and fluid.</span>",
                -1);
            cairo_set_source_rgb(cr,chrome.text,chrome.text,chrome.text);
            cairo_move_to(cr,content_x+68,y+52);pango_cairo_show_layout(cr,layout);
        }

        cairo_restore(cr); // content clip

        // Close button
        double cbx=x+width-36,cby=y+36;
        if(close_hovered){
            cairo_set_source_rgba(cr,chrome.error_r,chrome.error_g,chrome.error_b,0.14);
            shape::disc(cr, cbx, cby, 16);cairo_fill(cr);
            svg_icons::draw_ui(cr,"cross",cbx-7,cby-7,14,chrome.error_r,chrome.error_g,chrome.error_b,1.0);
        } else {
            svg_icons::draw_ui(cr,"cross",cbx-7,cby-7,14,chrome.text,chrome.text,chrome.text,0.4);
        }

        pango_font_description_free(fnt);
        g_object_unref(layout);
        cairo_restore(cr); // scale
    }

    // Persistence
    std::map<std::string,std::string> to_map() const {
        std::map<std::string,std::string> m;
        m["theme"]=themes[selected_theme_idx].id;
        m["show_line_numbers"]=show_line_numbers?"1":"0";
        m["show_splash"]=show_splash?"1":"0";
        m["indent_size"]=std::to_string(indent_size);
        m["gutter_density"]=std::to_string(gutter_density);
        m["line_spacing"]=std::to_string(line_spacing);
        return m;
    }

    void load_from_map(const std::map<std::string,std::string> &m){
        if(m.count("theme")) for(int i=0;i<(int)themes.size();++i) if(themes[i].id==m.at("theme"))selected_theme_idx=i;
        if(m.count("show_line_numbers"))show_line_numbers=(m.at("show_line_numbers")=="1");
        if(m.count("show_splash"))show_splash=(m.at("show_splash")=="1");
        try{if(m.count("indent_size")){indent_size=std::stoi(m.at("indent_size"));indent_anim=indent_size;}}catch(...){}
        try{if(m.count("gutter_density")){gutter_density=std::stod(m.at("gutter_density"));gutter_anim=gutter_density;}}catch(...){}
        try{if(m.count("line_spacing")){line_spacing=std::stod(m.at("line_spacing"));line_spacing_anim=line_spacing;}}catch(...){}
    }
};

#endif
