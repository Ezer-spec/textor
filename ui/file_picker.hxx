#ifndef FILE_PICKER_HXX
#define FILE_PICKER_HXX

#include <string>
#include "../base/keytext.hxx"
#include <vector>
#include <filesystem>
#include <algorithm>
#include <functional>
#include <cmath>
#include <cstdlib>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <X11/keysym.h>
#include "svg_icons.hxx"
#include "../color/themes.hxx"
#include "morphing.hxx"

namespace fs = std::filesystem;

struct FilePicker {
    bool visible     = false;
    bool mode_folder = false;
    AnimController anim;

    fs::path current_path;
    std::vector<fs::path> history;
    int history_idx = 0;

    struct Entry {
        std::string name;
        bool        is_dir;
        uintmax_t   size;
        bool        is_hidden;
        std::string ext;   // lowercase, no dot
    };
    std::vector<Entry> entries;

    int    selected_idx = -1;
    int    hovered_idx  = -1;
    double scroll_y     = 0.0;
    double scroll_vel   = 0.0;

    bool confirmed    = false;
    std::string result_path;
    std::string result_name;

    std::string path_input;
    bool path_input_active = false;

    std::string filter_text;
    std::vector<int> filtered;   // indices into entries[]

    // ............ Quick-access sidebar ............
    // A short list of frequently used locations (Home folder, Desktop, Documents…)
    struct QuickLoc {
        std::string label;
        fs::path    path;
        int         icon;   // 0=home 1=root/drive 2=desktop 3=documents 4=downloads
    };
    std::vector<QuickLoc> quick_locs;
    int quick_hovered_idx = -1;
    int footer_hover = -1;   // 0 = Cancel, 1 = Confirm, -1 = neither

    // ............ Breadcrumb path bar ............
    // Segment hit-boxes are recomputed every draw() call (draw() runs every
    // frame the picker is visible) and consulted by handle_click()/
    // handle_move(), since those don't have a cairo context to re-measure
    // text with - same idea as caching a layout pass for hit-testing.
    struct BreadcrumbSeg { double x, w; fs::path path; };
    std::vector<BreadcrumbSeg> breadcrumb_segs;
    int breadcrumb_hover = -1;

    static constexpr int ROW_H       = 28;
    static constexpr int HEADER_H    = 84;   // title + path bar + filter bar
    static constexpr int FOOTER_H    = 54;
    static constexpr int SIDEBAR_W   = 140;  // quick-access column width

    // ............ Extension colour ............
    static std::tuple<float,float,float> ext_color(const std::string &x, double muted) {
        if (x=="cpp"||x=="cxx"||x=="cc"||x=="c"||x=="h"||x=="hxx"||x=="hpp") return {0.38f,0.72f,1.00f};
        if (x=="py")                        return {0.99f,0.85f,0.30f};
        if (x=="js"||x=="ts"||x=="jsx"||x=="tsx") return {1.00f,0.78f,0.18f};
        if (x=="rs")                        return {0.95f,0.50f,0.22f};
        if (x=="swift")                     return {0.94f,0.36f,0.24f};
        if (x=="nim")                       return {0.96f,0.85f,0.28f};
        if (x=="kt"||x=="kts")              return {0.72f,0.45f,0.95f};
        if (x=="lua")                       return {0.45f,0.55f,0.95f};
        if (x=="rb")                        return {0.88f,0.25f,0.30f};
        if (x=="mek")                       return {0.70f,0.45f,0.90f};
        if (x=="go")                        return {0.40f,0.85f,0.90f};
        if (x=="json"||x=="yaml"||x=="yml"||x=="toml") return {0.55f,0.88f,0.55f};
        if (x=="html"||x=="htm"||x=="css"||x=="scss") return {0.90f,0.50f,0.35f};
        if (x=="sh"||x=="bash"||x=="zsh")   return {0.55f,0.90f,0.55f};
        if (x=="md"||x=="txt"||x=="rst")    return {0.72f,0.72f,0.72f};
        if (x=="png"||x=="jpg"||x=="jpeg"||x=="gif"||x=="svg"||x=="webp") return {0.88f,0.50f,0.88f};
        if (x=="pdf")                       return {0.90f,0.28f,0.28f};
        if (x=="zip"||x=="tar"||x=="gz"||x=="xz"||x=="7z"||x=="rar") return {0.90f,0.75f,0.30f};
        return {(float)muted,(float)muted,(float)muted};
    }

    // ............ Lifecycle ............
    void build_quick_locs() {
        quick_locs.clear();
        const char *home_c = std::getenv("HOME");
        fs::path home = home_c ? fs::path(home_c) : fs::path("/");
        quick_locs.push_back({"Home", home, 0});
        auto add_if_exists = [&](const char *label, const fs::path &p, int icon) {
            std::error_code ec;
            if (fs::exists(p, ec) && fs::is_directory(p, ec))
                quick_locs.push_back({label, p, icon});
        };
        add_if_exists("Desktop",   home / "Desktop",   2);
        add_if_exists("Documents", home / "Documents", 3);
        add_if_exists("Downloads", home / "Downloads", 4);
        quick_locs.push_back({"Filesystem", fs::path("/"), 1});
    }

    void open(bool folder_mode, const fs::path &start) {
        mode_folder       = folder_mode;
        visible           = true;
        confirmed         = false;
        selected_idx      = hovered_idx = -1;
        scroll_y          = scroll_vel = 0;
        path_input_active = false;
        filter_text.clear();
        result_path.clear(); result_name.clear();
        anim.start(true, 0.20);
        current_path = start;
        history.clear();
        history.push_back(current_path);
        history_idx = 0;
        build_quick_locs();
        refresh();
    }

    void close() {
        visible = false;
        anim.start(false, 0.16);
        path_input_active = false;
        filter_text.clear();
    }

    void refresh() {
        entries.clear();
        scroll_y = scroll_vel = 0;
        selected_idx = -1;
        try {
            for (auto &de : fs::directory_iterator(current_path,
                    fs::directory_options::skip_permission_denied)) {
                Entry e;
                e.name      = de.path().filename().string();
                e.is_dir    = de.is_directory();
                e.is_hidden = (!e.name.empty() && e.name[0] == '.');
                try { e.size = e.is_dir ? 0 : de.file_size(); } catch(...) { e.size = 0; }
                if (!e.is_dir) {
                    e.ext = svg_icons::icon_ext(e.name);
                }
                entries.push_back(e);
            }
        } catch (...) {}
        std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b){
            if (a.is_dir != b.is_dir) return a.is_dir > b.is_dir;
            std::string an=a.name, bn=b.name;
            for (auto &c:an) c=(char)tolower((unsigned char)c);
            for (auto &c:bn) c=(char)tolower((unsigned char)c);
            return an < bn;
        });
        path_input = current_path.string();
        apply_filter();
    }

    void apply_filter() {
        filtered.clear();
        std::string lo = filter_text;
        for (auto &c:lo) c=(char)tolower((unsigned char)c);
        for (int i=0;i<(int)entries.size();i++) {
            if (lo.empty()) { filtered.push_back(i); continue; }
            std::string n=entries[i].name;
            for (auto &c:n) c=(char)tolower((unsigned char)c);
            if (n.find(lo)!=std::string::npos) filtered.push_back(i);
        }
    }

    const Entry *get(int fi) const {
        if (fi<0||fi>=(int)filtered.size()) return nullptr;
        int i=filtered[fi];
        return (i>=0&&i<(int)entries.size()) ? &entries[i] : nullptr;
    }

    void navigate(const fs::path &p) {
        if (history_idx<(int)history.size()-1)
            history.erase(history.begin()+history_idx+1, history.end());
        history.push_back(p); history_idx++;
        current_path = p; refresh();
    }

    void go_back() {
        if (history_idx>0) { history_idx--; current_path=history[history_idx]; refresh(); }
    }

    void go_up() {
        auto par = current_path.parent_path();
        if (par != current_path) navigate(par);
    }

    bool try_confirm() {
        if (mode_folder) {
            fs::path t = current_path;
            auto *e = get(selected_idx);
            if (e && e->is_dir) t = current_path / e->name;
            result_path=t.string(); result_name=t.filename().string();
            confirmed=true; visible=false; return true;
        }
        auto *e = get(selected_idx);
        if (!e||e->is_dir) return false;
        result_path=(current_path/e->name).string(); result_name=e->name;
        confirmed=true; visible=false; return true;
    }

    // ............ Input ............
    // Ctrl+V: the first line of the text goes into the path field (Ctrl+L) or the filter
    void paste_text(const std::string &text) {
        std::string line=text.substr(0,text.find_first_of("\r\n"));
        if (line.empty()) return;
        if (path_input_active) path_input+=line;
        else { filter_text+=line; apply_filter(); }
    }

    bool handle_key(XKeyEvent *kev) {
        if (!visible) return false;
        char buf[32]{}; KeySym ks;
        int len = textor_lookup_key(kev, buf, sizeof(buf), &ks);
        bool ctrl = (kev->state & ControlMask) != 0;

        if (ks==XK_Escape) {
            if (!filter_text.empty()) { filter_text.clear(); apply_filter(); }
            else close();
            return true;
        }
        if (ks==XK_Return||ks==XK_KP_Enter) {
            if (path_input_active) {
                try { fs::path p(path_input); if(fs::is_directory(p)){navigate(p);path_input_active=false;} }
                catch(...) {}
                return true;
            }
            auto *e=get(selected_idx);
            if (e&&e->is_dir) { navigate(current_path/e->name); return true; }
            try_confirm(); return true;
        }
        if (ks==XK_BackSpace) {
            if (path_input_active && !path_input.empty()) { textor_pop_utf8(path_input); return true; }
            if (!filter_text.empty()) { textor_pop_utf8(filter_text); apply_filter(); return true; }
            go_up(); return true;
        }
        if (ks==XK_Up)   { if(selected_idx>0) { selected_idx--; ensure_visible(selected_idx); } return true; }
        if (ks==XK_Down) { if(selected_idx<(int)filtered.size()-1) { selected_idx++; ensure_visible(selected_idx); } return true; }
        if (ks==XK_Left) { go_back(); return true; }
        if (ks==XK_Right) { auto *e=get(selected_idx); if(e&&e->is_dir) navigate(current_path/e->name); return true; }
        if (ctrl&&(ks==XK_l||ks==XK_L)) { path_input_active=true; return true; }
        if (path_input_active && len>0 && (unsigned char)buf[0]>=0x20) { path_input.append(buf,len); return true; }
        // Type-to-filter
        if (!path_input_active && !ctrl && len>0 && (unsigned char)buf[0]>=0x20) {
            filter_text.append(buf,len); apply_filter(); return true;
        }
        return true;
    }

    void handle_scroll(int delta) {
        if (!visible) return;
        scroll_vel += delta * 0.6;
    }

    void tick(double dt) {
        scroll_y += scroll_vel * dt * 60.0;
        scroll_vel *= std::pow(0.04, dt);
        if (std::abs(scroll_vel)<0.01) scroll_vel=0;
    }

    void ensure_visible(int fi, int view_h=400) {
        double top=fi*ROW_H, bot=top+ROW_H;
        if (top < scroll_y) scroll_y=top;
        if (bot > scroll_y+view_h) scroll_y=bot-view_h;
    }

    bool handle_click(int mx, int my, int win_w, int win_h) {
        if (!visible) return false;
        auto [px,py,pw,ph] = panel_rect(win_w, win_h);
        if (mx<px||mx>px+pw||my<py||my>py+ph) { close(); return true; }

        // Back (<-) and Up (^)
        if (my>=py+8&&my<py+34) {
            if (mx>=px+10&&mx<px+40) { go_back(); return true; }
            if (mx>=px+44&&mx<px+74) { go_up();   return true; }
        }
        // Path bar - a breadcrumb segment takes priority over the generic
        // "click to edit as text" fallback below.
        if (my>=py+40&&my<py+64) {
            if (!path_input_active) {
                for (auto &seg : breadcrumb_segs) {
                    if (mx>=seg.x && mx<seg.x+seg.w) { navigate(seg.path); return true; }
                }
            }
            path_input_active=true; return true;
        }
        // Filter bar
        if (my>=py+66&&my<py+HEADER_H-2) { path_input_active=false; return true; }

        // Confirm / Cancel
        int bw=116,bh=30,bx=px+pw-bw-12,by2=py+ph-bh-12;
        if (mx>=bx&&mx<bx+bw&&my>=by2&&my<by2+bh) { try_confirm(); return true; }
        int cx2=bx-82-8;
        if (mx>=cx2&&mx<cx2+82&&my>=by2&&my<by2+bh) { close(); return true; }

        int ly=py+HEADER_H, lh=ph-HEADER_H-FOOTER_H;

        // Quick-access sidebar
        if (mx>=px&&mx<px+SIDEBAR_W&&my>=ly&&my<ly+lh) {
            int row=(int)((my-ly)/ROW_H);
            if (row>=0&&row<(int)quick_locs.size()) {
                navigate(quick_locs[row].path);
            }
            return true;
        }

        // File list (now offset right, past the quick-access sidebar)
        int list_x = px + SIDEBAR_W + 1;
        if (my>=ly&&my<ly+lh&&mx>=list_x&&mx<px+pw) {
            int row=(int)((my-ly+scroll_y)/ROW_H);
            if (row>=0&&row<(int)filtered.size()) {
                if (selected_idx==row) {
                    auto *e=get(row);
                    if (e&&e->is_dir) navigate(current_path/e->name);
                    else try_confirm();
                } else {
                    selected_idx=row;
                    path_input_active=false;
                    auto *e=get(row);
                    if (e&&!e->is_dir&&!mode_folder) try_confirm();
                }
            }
        }
        return true;
    }

    bool handle_move(int mx, int my, int win_w, int win_h) {
        if (!visible) return false;
        auto [px,py,pw,ph] = panel_rect(win_w, win_h);
        int ly=py+HEADER_H, lh=ph-HEADER_H-FOOTER_H;
        hovered_idx=-1;
        quick_hovered_idx=-1;
        breadcrumb_hover=-1;
        footer_hover=-1;
        {
            int bw=116,bh=30,bx=px+pw-bw-12,by2=py+ph-bh-12;
            if (my>=by2&&my<by2+bh) {
                if (mx>=bx&&mx<bx+bw) footer_hover=1;
                else { int cx2=bx-82-8; if (mx>=cx2&&mx<cx2+82) footer_hover=0; }
            }
        }

        if (my>=py+40&&my<py+64&&!path_input_active) {
            for (int i=0;i<(int)breadcrumb_segs.size();i++) {
                auto &seg=breadcrumb_segs[i];
                if (mx>=seg.x&&mx<seg.x+seg.w) { breadcrumb_hover=i; break; }
            }
        }

        if (mx>=px&&mx<px+SIDEBAR_W&&my>=ly&&my<ly+lh) {
            int row=(int)((my-ly)/ROW_H);
            if (row>=0&&row<(int)quick_locs.size()) quick_hovered_idx=row;
        }

        int list_x = px + SIDEBAR_W + 1;
        if (mx>=list_x&&mx<px+pw&&my>=ly&&my<ly+lh) {
            int row=(int)((my-ly+scroll_y)/ROW_H);
            if (row>=0&&row<(int)filtered.size()) hovered_idx=row;
        }
        return false;
    }

    // ............ Draw ............
    std::tuple<int,int,int,int> panel_rect(int win_w, int win_h) const {
        int pw=std::min(760,win_w-60), ph=std::min(540,win_h-60);
        return {(win_w-pw)/2, (win_h-ph)/2, pw, ph};
    }

    static void rrect(cairo_t *cr, double x, double y, double w, double h, double r) {
        shape::rrect(cr, x, y, w, h, r);
    }

    static std::string fmt_size(uintmax_t b) {
        if (b<1024)    return std::to_string(b)+" B";
        if (b<1048576) return std::to_string(b/1024)+" KB";
        return std::to_string(b/1048576)+" MB";
    }

    // Small file/folder icon: SVG for folders and for languages that have one,
    // the plain file icon for every other file
    void draw_icon(cairo_t *cr, double cx, double cy, bool is_dir,
                   float r, float g, float b, float a, const std::string &ext = "") const {
        if (is_dir) { svg_icons::draw_ui(cr, "folder", cx - 8, cy - 8, 16, r, g, b, a); return; }
        if (!ext.empty() && svg_icons::draw(cr, ext, cx - 8, cy - 8, 16, r, g, b, a)) return;

        svg_icons::draw_plain_file(cr, cx - 8, cy - 8, 16, r, g, b, a);
    }

    void draw(cairo_t *cr, int win_w, int win_h, const ThemeVariant &tv) {
        double p = anim.progress();
        if (!visible && !anim.is_running()) return;
        if (p <= 0.001) return;
        double e3 = p*p*(3.0-2.0*p);

        auto [px,py,pw,ph] = panel_rect(win_w, win_h);
        double cx2=px+pw*0.5, cy2=py+ph*0.5;
        double scale=1.0;

        // Backdrop
        cairo_set_source_rgba(cr,0,0,0,0.48*e3);
        cairo_rectangle(cr,0,0,win_w,win_h); cairo_fill(cr);

        cairo_save(cr);
        cairo_translate(cr,cx2,cy2); cairo_scale(cr,scale,scale); cairo_translate(cr,-cx2,-cy2);


        // Panel group (for alpha animation)
        cairo_push_group(cr);
        rrect(cr,px,py,pw,ph,10); cairo_clip(cr);

        // Background
        double pbg=tv.elevated;
        cairo_set_source_rgb(cr,pbg,pbg,pbg+0.01); cairo_paint(cr);

        // Top accent line
        cairo_set_source_rgb(cr,tv.accent_r,tv.accent_g,tv.accent_b);
        cairo_rectangle(cr,px,py,pw,2.5); cairo_fill(cr);

        // Pango setup
        PangoLayout *layout=pango_cairo_create_layout(cr);
        std::string fspec=tv.font_family+" "+std::to_string(tv.font_size);
        PangoFontDescription *fnt=pango_font_description_from_string(fspec.c_str());
        pango_layout_set_font_description(layout,fnt);

        // ............ Title ............
        {
            PangoFontDescription *tf=pango_font_description_copy(fnt);
            pango_font_description_set_weight(tf,PANGO_WEIGHT_SEMIBOLD);
            pango_layout_set_font_description(layout,tf);
            pango_layout_set_text(layout,mode_folder?"Open Folder":"Open File",-1);
            int tw,th; pango_layout_get_pixel_size(layout,&tw,&th);
            cairo_set_source_rgb(cr,tv.text,tv.text,tv.text);
            cairo_move_to(cr,px+(pw-tw)/2.0,py+9);
            pango_cairo_show_layout(cr,layout);
            pango_font_description_free(tf);
            pango_layout_set_font_description(layout,fnt);
        }

        // ............ Nav buttons ............
        auto nav_btn=[&](int bx,int by2,bool on,const char *icon){
            cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,on?0.14:0.05);
            rrect(cr,bx,by2,28,24,5); cairo_fill(cr);
            // SVG arrow exactly in the middle of the button (28 x 24)
            svg_icons::draw_ui(cr,icon,bx+(28-20)/2.0,by2+(24-20)/2.0,20,tv.text,tv.text,tv.text,on?0.80:0.22);
        };
        nav_btn(px+10,py+8,history_idx>0,"arrow_left");
        nav_btn(px+42,py+8,current_path.has_parent_path()&&
                            current_path.parent_path()!=current_path,"arrow_up");

        // -- Path bar - breadcrumb of clickable segments, or raw text while
        //    editing (Ctrl+L, or clicking empty space in the bar) -----------
        breadcrumb_segs.clear();
        {
            int bx2=px+10,by2=py+40,bw2=pw-20,bh2=22;
            cairo_set_source_rgba(cr,tv.bg,tv.bg,tv.bg,0.55);
            rrect(cr,bx2,by2,bw2,bh2,4); cairo_fill(cr);
            if (path_input_active) {
                cairo_set_source_rgba(cr,tv.accent_r,tv.accent_g,tv.accent_b,0.50);
                cairo_set_line_width(cr,1.0);
                rrect(cr,bx2,by2,bw2,bh2,4); cairo_stroke(cr);
            }
            PangoFontDescription *sf=pango_font_description_copy(fnt);
            pango_font_description_set_size(sf,(int)(tv.font_size*0.82)*PANGO_SCALE);
            pango_layout_set_font_description(layout,sf);

            cairo_save(cr); cairo_rectangle(cr,bx2+5,by2,bw2-7,bh2); cairo_clip(cr);

            if (path_input_active) {
                std::string disp=path_input+"|";
                pango_layout_set_text(layout,disp.c_str(),-1);
                pango_layout_set_width(layout,(bw2-10)*PANGO_SCALE);
                pango_layout_set_ellipsize(layout,PANGO_ELLIPSIZE_START);
                int tw,th; pango_layout_get_pixel_size(layout,&tw,&th);
                cairo_set_source_rgba(cr,tv.text,tv.text,tv.text,0.78);
                cairo_move_to(cr,bx2+5,by2+(bh2-th)/2.0); pango_cairo_show_layout(cr,layout);
            } else {
                // Split the path into segments (root -> ... -> current dir),
                // each one independently clickable to jump straight there.
                std::vector<std::pair<std::string,fs::path>> parts;
                {
                    std::vector<fs::path> chain;
                    fs::path cur = current_path;
                    chain.push_back(cur);
                    while (cur.has_parent_path() && cur.parent_path()!=cur) {
                        cur = cur.parent_path();
                        chain.push_back(cur);
                    }
                    std::reverse(chain.begin(), chain.end());
                    for (auto &seg : chain) {
                        std::string label = seg.filename().string();
                        if (label.empty()) label = seg.string(); // root "/"
                        parts.push_back({label, seg});
                    }
                }

                pango_layout_set_width(layout,-1);
                double avail = bw2 - 10.0, sep_w = 14.0;
                std::vector<double> widths(parts.size());
                for (int i=0;i<(int)parts.size();i++) {
                    pango_layout_set_text(layout,parts[i].first.c_str(),-1);
                    int tw,th; pango_layout_get_pixel_size(layout,&tw,&th);
                    widths[i]=(double)tw;
                }
                // Keep as many trailing segments (nearest the current dir)
                // as fit; older ancestors collapse behind a "..." if needed.
                int first_shown=(int)parts.size()-1;
                double used=widths.empty()?0.0:widths.back();
                for (int i=(int)parts.size()-2;i>=0;i--) {
                    double add = widths[i] + sep_w;
                    if (used+add>avail) break;
                    used+=add; first_shown=i;
                }
                bool truncated = first_shown>0;

                double cxp = bx2+5;
                if (truncated) {
                    pango_layout_set_text(layout,"...",-1);
                    int tw,th; pango_layout_get_pixel_size(layout,&tw,&th);
                    cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,0.45);
                    cairo_move_to(cr,cxp,by2+(bh2-th)/2.0);
                    pango_cairo_show_layout(cr,layout);
                    cxp += tw + sep_w;
                }
                for (int i=first_shown;i<(int)parts.size();i++) {
                    bool is_last=(i==(int)parts.size()-1);
                    pango_layout_set_text(layout,parts[i].first.c_str(),-1);
                    int tw,th; pango_layout_get_pixel_size(layout,&tw,&th);
                    bool hov = (breadcrumb_hover==i);
                    if (hov || is_last) {
                        cairo_set_source_rgba(cr,
                            is_last?tv.accent_r:tv.muted,
                            is_last?tv.accent_g:tv.muted,
                            is_last?tv.accent_b:tv.muted,
                            is_last?0.16:0.10);
                        rrect(cr,cxp-4,by2+2,tw+8,bh2-4,4); cairo_fill(cr);
                    }
                    cairo_set_source_rgba(cr,tv.text,tv.text,tv.text,is_last?0.92:0.62);
                    cairo_move_to(cr,cxp,by2+(bh2-th)/2.0);
                    pango_cairo_show_layout(cr,layout);
                    breadcrumb_segs.push_back({cxp-4, tw+8.0, parts[i].second});
                    double seg_end = cxp + tw;
                    cxp = seg_end + sep_w;
                    if (!is_last) {
                        pango_layout_set_text(layout,">",-1);
                        int sw3,sh3; pango_layout_get_pixel_size(layout,&sw3,&sh3);
                        cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,0.35);
                        cairo_move_to(cr,seg_end+3,by2+(bh2-sh3)/2.0);
                        pango_cairo_show_layout(cr,layout);
                    }
                }
            }
            cairo_restore(cr);
            pango_font_description_free(sf);
            pango_layout_set_font_description(layout,fnt);
        }

        // ............ Filter bar ............
        {
            int fx2=px+10,fy2=py+66,fw2=pw-20,fh2=16;
            cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,!filter_text.empty()?0.14:0.07);
            rrect(cr,fx2,fy2,fw2,fh2,3); cairo_fill(cr);
            // Search icon
            cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,0.40);
            cairo_set_line_width(cr,1.0);
            shape::disc(cr, fx2+9, fy2+8, 3.8); cairo_stroke(cr);
            cairo_move_to(cr,fx2+12,fy2+11); cairo_line_to(cr,fx2+14.5,fy2+13.5); cairo_stroke(cr);
            PangoFontDescription *sf=pango_font_description_copy(fnt);
            pango_font_description_set_size(sf,(int)(tv.font_size*0.76)*PANGO_SCALE);
            pango_layout_set_font_description(layout,sf);
            if (filter_text.empty()) {
                pango_layout_set_text(layout,"Filter files...",-1);
                cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,0.32);
            } else {
                pango_layout_set_text(layout,filter_text.c_str(),-1);
                cairo_set_source_rgba(cr,tv.text,tv.text,tv.text,0.85);
            }
            pango_layout_set_width(layout,(fw2-24)*PANGO_SCALE);
            pango_layout_set_ellipsize(layout,PANGO_ELLIPSIZE_END);
            int fw3,fh3; pango_layout_get_pixel_size(layout,&fw3,&fh3);
            cairo_move_to(cr,fx2+18,fy2+(fh2-fh3)/2.0); pango_cairo_show_layout(cr,layout);
            if (!filter_text.empty()) {
                // match count badge
                std::string cnt=std::to_string(filtered.size());
                pango_layout_set_text(layout,cnt.c_str(),-1);
                pango_layout_set_width(layout,-1);
                int cw,ch; pango_layout_get_pixel_size(layout,&cw,&ch);
                cairo_set_source_rgba(cr,tv.accent_r,tv.accent_g,tv.accent_b,0.65);
                cairo_move_to(cr,fx2+fw2-cw-6,fy2+(fh2-ch)/2.0);
                pango_cairo_show_layout(cr,layout);
            }
            pango_font_description_free(sf);
            pango_layout_set_font_description(layout,fnt);
        }

        // ............ Header divider ............
        cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,0.12);
        cairo_rectangle(cr,px,py+HEADER_H-1,pw,1); cairo_fill(cr);

        int list_y=py+HEADER_H, list_h=ph-HEADER_H-FOOTER_H;
        int list_x=px+SIDEBAR_W+1, list_w=pw-SIDEBAR_W-1;

        // ............ Quick-access sidebar ............
        // Common locations (Home, Desktop, ...)
        {
            cairo_save(cr);
            cairo_rectangle(cr,px,list_y,SIDEBAR_W,list_h); cairo_clip(cr);
            cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,0.035);
            cairo_rectangle(cr,px,list_y,SIDEBAR_W,list_h); cairo_fill(cr);

            PangoFontDescription *qf=pango_font_description_copy(fnt);
            pango_font_description_set_size(qf,(int)(tv.font_size*0.84)*PANGO_SCALE);
            pango_layout_set_font_description(layout,qf);

            std::error_code qec;
            for (int i=0;i<(int)quick_locs.size();i++) {
                double ry = list_y + i*ROW_H;
                if (ry > list_y + list_h) break;
                bool active = fs::equivalent(quick_locs[i].path, current_path, qec) && !qec;
                bool hov    = (quick_hovered_idx==i);
                if (active) {
                    cairo_set_source_rgba(cr,tv.accent_r,tv.accent_g,tv.accent_b,0.14);
                    rrect(cr,px+6,ry+2,SIDEBAR_W-12,ROW_H-4,6); cairo_fill(cr);
                    cairo_set_source_rgba(cr,tv.accent_r,tv.accent_g,tv.accent_b,0.85);
                    rrect(cr,px+6,ry+6,3,ROW_H-12,1.5); cairo_fill(cr);
                } else if (hov) {
                    cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,0.07);
                    rrect(cr,px+6,ry+2,SIDEBAR_W-12,ROW_H-4,6); cairo_fill(cr);
                }
                float qr,qg,qb;
                switch (quick_locs[i].icon) {
                    case 0: qr=tv.accent_r; qg=tv.accent_g; qb=tv.accent_b; break; // home
                    default: qr=qg=qb=(float)tv.muted; break;
                }
                draw_icon(cr, px+22, ry+ROW_H/2.0, /*is_dir=*/true, qr,qg,qb,
                           active?1.0f:0.62f);
                pango_layout_set_text(layout, quick_locs[i].label.c_str(), -1);
                pango_layout_set_width(layout,(SIDEBAR_W-46)*PANGO_SCALE);
                pango_layout_set_ellipsize(layout,PANGO_ELLIPSIZE_END);
                int qw,qh; pango_layout_get_pixel_size(layout,&qw,&qh);
                cairo_set_source_rgba(cr,tv.text,tv.text,tv.text, active?0.92:0.68);
                cairo_move_to(cr,px+36,ry+(ROW_H-qh)/2.0);
                pango_cairo_show_layout(cr,layout);
            }
            pango_font_description_free(qf);
            pango_layout_set_font_description(layout,fnt);
            cairo_restore(cr);

            // Divider between sidebar and file list
            cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,0.12);
            cairo_rectangle(cr,px+SIDEBAR_W,list_y,1,list_h); cairo_fill(cr);
        }

        // ............ File list ............
        int n=(int)filtered.size();
        double max_s=std::max(0.0,(double)n*ROW_H-list_h);
        scroll_y=std::clamp(scroll_y,0.0,max_s);

        cairo_save(cr);
        cairo_rectangle(cr,list_x,list_y,list_w,list_h); cairo_clip(cr);

        if (n==0) {
            PangoFontDescription *sf=pango_font_description_copy(fnt);
            pango_font_description_set_size(sf,(int)(tv.font_size*0.88)*PANGO_SCALE);
            pango_layout_set_font_description(layout,sf);
            pango_layout_set_text(layout,filter_text.empty()?"Empty folder":"No matches",-1);
            int ew,eh; pango_layout_get_pixel_size(layout,&ew,&eh);
            cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,0.35);
            cairo_move_to(cr,list_x+(list_w-ew)/2.0,list_y+(list_h-eh)/2.0);
            pango_cairo_show_layout(cr,layout);
            pango_font_description_free(sf);
        }

        int vs=(int)(scroll_y/ROW_H)-1;
        int ve=std::min(n,vs+list_h/ROW_H+3);
        vs=std::max(0,vs);

        PangoFontDescription *sf=pango_font_description_copy(fnt);
        pango_font_description_set_size(sf,(int)(tv.font_size*0.88)*PANGO_SCALE);

        for (int fi=vs; fi<ve; fi++) {
            const auto *e=get(fi);
            if (!e) continue;
            double ry=list_y+fi*ROW_H-scroll_y;
            bool sel=(fi==selected_idx), hov=(fi==hovered_idx);

            // Row bg - rounded, matching the sidebar's own selection style
            if (sel) {
                cairo_set_source_rgba(cr,tv.accent_r,tv.accent_g,tv.accent_b,0.16);
                rrect(cr,list_x+4,ry+1,list_w-8,ROW_H-2,5); cairo_fill(cr);
                cairo_set_source_rgba(cr,tv.accent_r,tv.accent_g,tv.accent_b,0.85);
                rrect(cr,list_x+4,ry+4,3,ROW_H-8,1.5); cairo_fill(cr);
            } else if (hov) {
                cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,0.06);
                rrect(cr,list_x+4,ry+1,list_w-8,ROW_H-2,5); cairo_fill(cr);
            }

            // Icon
            float ir,ig,ib;
            if (e->is_dir) { ir=tv.accent_r; ig=tv.accent_g; ib=tv.accent_b; }
            else { auto [er,eg,eb]=ext_color(e->ext,tv.muted); ir=er;ig=eg;ib=eb; }
            draw_icon(cr,list_x+22,ry+ROW_H/2.0,e->is_dir,ir,ig,ib,sel?1.0f:0.68f,e->ext);

            // The parts on the right come first (size label, extension badge): the name
            // gets what is left, so it is cut with "..." and never runs into them.
            const double right_edge = list_x + list_w - 12;   // room for the scrollbar
            double name_right = right_edge;

            // File size, at the right edge
            if (!e->is_dir && e->size>0) {
                PangoFontDescription *tiny=pango_font_description_copy(fnt);
                pango_font_description_set_size(tiny,(int)(tv.font_size*0.72)*PANGO_SCALE);
                pango_layout_set_font_description(layout,tiny);
                std::string sz=fmt_size(e->size);
                pango_layout_set_text(layout,sz.c_str(),-1);
                pango_layout_set_width(layout,-1);
                int sw2,sh2; pango_layout_get_pixel_size(layout,&sw2,&sh2);
                cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,0.45);
                cairo_move_to(cr,right_edge-sw2,ry+(ROW_H-sh2)/2.0);
                pango_cairo_show_layout(cr,layout);
                pango_font_description_free(tiny);
                name_right = right_edge - sw2 - 10;
            }

            // Extension badge, left of the size
            if (!e->is_dir && !e->ext.empty()) {
                PangoFontDescription *ef=pango_font_description_copy(fnt);
                pango_font_description_set_size(ef,std::max(6,(int)(tv.font_size*0.68))*PANGO_SCALE);
                pango_font_description_set_weight(ef,PANGO_WEIGHT_BOLD);
                pango_layout_set_font_description(layout,ef);
                std::string et=e->ext; if(et.size()>5)et=et.substr(0,5);
                pango_layout_set_text(layout,et.c_str(),-1);
                pango_layout_set_width(layout,-1);
                int ew2,eh2; pango_layout_get_pixel_size(layout,&ew2,&eh2);
                double bx2=name_right-ew2-6, by2=ry+(ROW_H-eh2-2)/2.0;
                if (bx2 > list_x+36+60) {   // only when the name keeps at least some room
                    cairo_set_source_rgba(cr,ir,ig,ib,sel?0.28:0.18);
                    rrect(cr,bx2,by2-1,ew2+6,eh2+2,2); cairo_fill(cr);
                    cairo_set_source_rgba(cr,ir,ig,ib,sel?0.90:0.65);
                    cairo_move_to(cr,bx2+3,by2); pango_cairo_show_layout(cr,layout);
                    name_right = bx2 - 8;
                }
                pango_font_description_free(ef);
            }

            // Name
            pango_font_description_set_weight(sf,
                (sel||e->is_dir)?PANGO_WEIGHT_MEDIUM:PANGO_WEIGHT_NORMAL);
            pango_layout_set_font_description(layout,sf);
            pango_layout_set_text(layout,e->name.c_str(),-1);
            pango_layout_set_width(layout,(int)(std::max(40.0,name_right-(list_x+36))*PANGO_SCALE));
            pango_layout_set_ellipsize(layout,PANGO_ELLIPSIZE_END);
            int nw,nh; pango_layout_get_pixel_size(layout,&nw,&nh);
            double alpha=e->is_hidden?0.42:(sel?1.0:0.82);
            cairo_set_source_rgba(cr,tv.text,tv.text,tv.text,alpha);
            cairo_move_to(cr,list_x+36,ry+(ROW_H-nh)/2.0);
            pango_cairo_show_layout(cr,layout);
            pango_layout_set_ellipsize(layout,PANGO_ELLIPSIZE_NONE);
            pango_layout_set_font_description(layout,fnt);
        }
        pango_font_description_free(sf);
        cairo_restore(cr);

        // Scrollbar
        if (max_s>0) {
            double ratio=std::clamp((double)list_h/(list_h+max_s),0.05,1.0);
            double th2=std::max(24.0,list_h*ratio);
            double ms2=std::max(1.0,(double)list_h-th2);
            double tby=list_y+(scroll_y/max_s)*ms2;
            cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,0.08);
            rrect(cr,list_x+list_w-7,list_y,5,list_h,2); cairo_fill(cr);
            cairo_set_source_rgba(cr,tv.accent_r,tv.accent_g,tv.accent_b,0.50);
            rrect(cr,list_x+list_w-7,tby,5,th2,2); cairo_fill(cr);
        }

        // ............ Footer ............
        cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,0.10);
        cairo_rectangle(cr,px,py+ph-FOOTER_H,pw,1); cairo_fill(cr);

        // Selected item label
        {
            PangoFontDescription *sf2=pango_font_description_copy(fnt);
            pango_font_description_set_size(sf2,(int)(tv.font_size*0.80)*PANGO_SCALE);
            pango_layout_set_font_description(layout,sf2);
            std::string lbl;
            auto *se=get(selected_idx);
            if (mode_folder) lbl="Folder: "+(se&&se->is_dir?se->name:current_path.filename().string());
            else lbl=se&&!se->is_dir?"File: "+se->name:"No file selected";
            pango_layout_set_text(layout,lbl.c_str(),-1);
            pango_layout_set_width(layout,(pw-250)*PANGO_SCALE);
            pango_layout_set_ellipsize(layout,PANGO_ELLIPSIZE_MIDDLE);
            int lw2,lh2; pango_layout_get_pixel_size(layout,&lw2,&lh2);
            cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,0.58);
            cairo_move_to(cr,px+14,py+ph-FOOTER_H+(FOOTER_H-lh2)/2.0);
            pango_cairo_show_layout(cr,layout);
            pango_font_description_free(sf2);
            pango_layout_set_font_description(layout,fnt);
        }

        // Buttons - a touch of hover feedback, matching the rest of the UI's
        // interactive surfaces (nothing here reacted to the mouse before).
        int bh=30,bx=px+pw-116-12,by2=py+ph-bh-12;
        // Cancel
        cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,footer_hover==0?0.15:0.09);
        rrect(cr,bx-82-8,by2,82,bh,6); cairo_fill(cr);
        cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,footer_hover==0?0.28:0.18);
        cairo_set_line_width(cr,0.8);
        rrect(cr,bx-82-8,by2,82,bh,6); cairo_stroke(cr);
        {
            PangoFontDescription *bf=pango_font_description_copy(fnt);
            pango_font_description_set_size(bf,(int)(tv.font_size*0.86)*PANGO_SCALE);
            pango_layout_set_font_description(layout,bf);
            pango_layout_set_text(layout,"Cancel",-1);
            pango_layout_set_width(layout,-1);
            int cw,ch; pango_layout_get_pixel_size(layout,&cw,&ch);
            cairo_set_source_rgba(cr,tv.text,tv.text,tv.text,footer_hover==0?0.82:0.62);
            cairo_move_to(cr,bx-82-8+(82-cw)/2.0,by2+(bh-ch)/2.0);
            pango_cairo_show_layout(cr,layout);
            pango_font_description_free(bf);
            pango_layout_set_font_description(layout,fnt);
        }
        // Confirm
        auto *se=get(selected_idx);
        bool can=mode_folder||(se&&!se->is_dir);
        double confirm_a = can ? (footer_hover==1?1.0:0.88) : 0.28;
        cairo_set_source_rgba(cr,tv.accent_r,tv.accent_g,tv.accent_b,confirm_a);
        rrect(cr,bx,by2,116,bh,6); cairo_fill(cr);
        {
            PangoFontDescription *bf=pango_font_description_copy(fnt);
            pango_font_description_set_size(bf,(int)(tv.font_size*0.86)*PANGO_SCALE);
            pango_font_description_set_weight(bf,PANGO_WEIGHT_MEDIUM);
            pango_layout_set_font_description(layout,bf);
            pango_layout_set_text(layout,mode_folder?"Open Folder":"Open File",-1);
            pango_layout_set_width(layout,-1);
            int bw2,bh2; pango_layout_get_pixel_size(layout,&bw2,&bh2);
            cairo_set_source_rgba(cr,0.05,0.05,0.05,can?0.95:0.40);
            cairo_move_to(cr,bx+(116-bw2)/2.0,by2+(bh-bh2)/2.0);
            pango_cairo_show_layout(cr,layout);
            pango_font_description_free(bf);
        }

        pango_font_description_free(fnt);
        g_object_unref(layout);

        cairo_pop_group_to_source(cr);
        cairo_paint_with_alpha(cr,e3);
        cairo_restore(cr);
    }
};

#endif
