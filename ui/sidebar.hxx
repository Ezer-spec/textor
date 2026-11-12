#ifndef SIDEBAR_HXX
#define SIDEBAR_HXX

#include <vector>
#include "../base/keytext.hxx"
#include <string>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <set>
#include <map>
#include <chrono>
#include <functional>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include "../base/globals.hxx"
#include "morphing.hxx"
#include "../base/addon_loader.hxx"
#include <X11/keysym.h>
#include "../color/themes.hxx"
#include "../base/keymap.hxx"
#include "../util/dispatch.hxx"
#include "svg_icons.hxx"

namespace fs = std::filesystem;

// Tree node
struct TreeNode {
    std::string name, ext;
    fs::path    path;
    bool        is_dir      = false;
    bool        is_hidden   = false;
    bool        expanded    = false;
    int         depth       = 0;
    uintmax_t   size_bytes  = 0;
    // children loaded lazily when expanded
    std::vector<TreeNode> children;
    bool        children_loaded = false;
    // Animation: expand arrow rotation
    double      arrow_anim  = 0.0; // 0=closed, 1=open
};

static inline std::string fmt_size(uintmax_t b){
    if(b<1024) return std::to_string(b)+" B";
    if(b<1024*1024) return std::to_string(b/1024)+" KB";
    if(b<(uintmax_t)1024*1024*1024) return std::to_string(b/(1024*1024))+" MB";
    return std::to_string(b/(1024*1024*1024))+" GB";
}

static inline void load_children(TreeNode &node){
    node.children.clear();
    node.children_loaded=true;
    try{
        std::vector<TreeNode> dirs,files;
        for(auto &p:fs::directory_iterator(node.path)){
            TreeNode c;
            c.name=p.path().filename().string();
            c.path=p.path();
            c.is_dir=p.is_directory();
            c.is_hidden=(!c.name.empty()&&c.name[0]=='.');
            c.depth=node.depth+1;
            if(!c.is_dir){
                c.ext=svg_icons::icon_ext(c.name);
                try{c.size_bytes=fs::file_size(p);}catch(...){}
            }
            if(c.is_dir) dirs.push_back(c);
            else files.push_back(c);
        }
        auto cmp=[](const TreeNode&a,const TreeNode&b){
            std::string na=a.name,nb=b.name;
            for(auto&ch:na)ch=(char)tolower((unsigned char)ch);
            for(auto&ch:nb)ch=(char)tolower((unsigned char)ch);
            return na<nb;
        };
        std::sort(dirs.begin(),dirs.end(),cmp);
        std::sort(files.begin(),files.end(),cmp);
        for(auto&d:dirs) node.children.push_back(d);
        for(auto&f:files) node.children.push_back(f);
    }catch(...){}
}

struct Sidebar {
    fs::path current_path = fs::current_path();
    int  width       = 240;
    int  hovered_idx = -1;
    bool visible     = true;
    int  selected_idx= -1;

    // Pointer to addon loader for icon overrides (set by Editor after construction)
    const AddonLoader *addon_loader = nullptr;

    std::string filter;
    bool  search_active = false;
    bool  show_hidden   = false;

    // New Explorer header: search starts collapsed behind a small icon
    // button next to the big "New File" button; clicking it expands into
    // the filter input, sliding the New File button into a small icon.
    bool   search_expanded    = false;
    double search_expand_anim = 0.0; // 0=collapsed, 1=expanded

    // Global project search
    enum SearchMode { SEARCH_FILENAME=0, SEARCH_CONTENT=1, SEARCH_REGEX=2 };
    SearchMode search_mode = SEARCH_FILENAME;
    bool  search_mode_popup = false;  // show mode dropdown
    std::set<fs::path> global_match_paths;  // files that match content search
    double scroll_y     = 0.0;
    bool   follow_selection = false;   // keyboard moved the selection: scroll it into view on the next draw

    // History (used by editor.hxx back/forward buttons)
    std::vector<fs::path> history;
    int                   history_idx = -1;

    // Flat display list (built from tree, reset on refresh)
    std::vector<TreeNode*> flat;   // non-owning pointers into root.children tree

    // Compatibility alias: editor.hxx uses sidebar.files directly
    // We expose a thin wrapper that returns the same data as flat but as FileEntry-like view
    // Instead, we keep a parallel flat FileEntry list for editor.hxx compatibility
    struct FileEntry {
        std::string name, ext;
        bool is_directory = false;
        uintmax_t size_bytes = 0;
        bool is_hidden = false;
        fs::path path;
    };
    std::vector<FileEntry> files; // synced with flat on every build_flat()

    // Root node (virtual, its children = top-level entries of current_path)
    TreeNode root;

    enum ActionMode { ACT_NONE=0, ACT_NEW_FILE, ACT_NEW_FOLDER, ACT_DELETE };
    ActionMode  action_mode = ACT_NONE;
    std::string input_text;
    std::set<int> multi_sel;
    int hovered_btn = -1;

    // Row hover animation
    std::map<int,double> row_hover_anim;
    std::chrono::steady_clock::time_point last_tick = std::chrono::steady_clock::now();

    static constexpr int ROW_H    = 28;
    static constexpr int HEADER_BASE = 72;   // label+toolbar+margin
    static constexpr int INPUT_H     = 26;   // height of inline input row

    // Dynamic header height - grows when input field is shown.
    // Single-row layout now (no separate caption line above the box).
    int header_h() const {
        return (action_mode != ACT_NONE) ? HEADER_BASE + 24 : HEADER_BASE;
    }

    Sidebar(){ refresh(); }

    // Build flat display list from tree
    void build_flat(){
        flat.clear();
        std::function<void(std::vector<TreeNode>&)> walk=[&](std::vector<TreeNode>&nodes){
            for(auto &n:nodes){
                if(!show_hidden&&n.is_hidden) continue;
                if(!filter.empty()){
                    std::string ln=n.name,lf=filter;
                    for(auto&c:ln)c=(char)tolower((unsigned char)c);
                    for(auto&c:lf)c=(char)tolower((unsigned char)c);
                    if(ln.find(lf)==std::string::npos&&!n.is_dir) continue;
                }
                flat.push_back(&n);
                if(n.is_dir&&n.expanded&&n.children_loaded) walk(n.children);
            }
        };
        walk(root.children);
        // Sync compat 'files' list
        files.clear();
        files.reserve(flat.size());
        for(auto *n:flat){
            FileEntry fe;
            fe.name=n->name; fe.ext=n->ext;
            fe.is_directory=n->is_dir; fe.is_hidden=n->is_hidden;
            fe.size_bytes=n->size_bytes; fe.path=n->path;
            files.push_back(fe);
        }
    }

    void refresh(){
        root.path=current_path; root.is_dir=true; root.depth=-1;
        load_children(root);
        build_flat();
        scroll_y=0.0; selected_idx=-1; multi_sel.clear();
        dir_stamps=read_stamps();
    }

    // ............ Keeping the tree in sync with the disk ............
    // Files can change outside the editor (the terminal, a file manager). We look
    // at the change time of every shown folder twice a second, and reload the tree
    // when one of them changed.
    std::map<fs::path,fs::file_time_type> dir_stamps;   // folder -> change time at the last load
    std::chrono::steady_clock::time_point last_fs_check=std::chrono::steady_clock::now();

    // The folders whose content is on screen: the root and every opened folder
    static void shown_dirs(const TreeNode &n,std::vector<fs::path> &out){
        for(auto &c:n.children)
            if(c.is_dir&&c.expanded&&c.children_loaded){ out.push_back(c.path); shown_dirs(c,out); }
    }
    std::map<fs::path,fs::file_time_type> read_stamps() const {
        std::vector<fs::path> dirs{root.path};
        shown_dirs(root,dirs);
        std::map<fs::path,fs::file_time_type> stamps;
        for(auto &p:dirs){
            std::error_code ec;
            auto t=fs::last_write_time(p,ec);
            stamps[p]=ec?fs::file_time_type::min():t;   // a missing folder counts as changed
        }
        return stamps;
    }

    // Reads the tree from disk again, but keeps opened folders, the selection and the scroll
    void reload_keep_state(){
        std::set<fs::path> opened;
        std::function<void(const TreeNode&)> collect=[&](const TreeNode &n){
            for(auto &c:n.children) if(c.is_dir&&c.expanded){ opened.insert(c.path); collect(c); }
        };
        collect(root);
        fs::path selected;
        if(selected_idx>=0&&selected_idx<(int)flat.size()) selected=flat[selected_idx]->path;

        root.path=current_path; root.is_dir=true; root.depth=-1;
        load_children(root);
        std::function<void(TreeNode&)> reopen=[&](TreeNode &n){
            for(auto &c:n.children) if(c.is_dir&&opened.count(c.path)){
                c.expanded=true; c.arrow_anim=1.0;
                load_children(c); reopen(c);
            }
        };
        reopen(root);
        build_flat();

        selected_idx=-1; hovered_idx=-1; multi_sel.clear();
        for(int i=0;i<(int)flat.size();++i) if(flat[i]->path==selected){ selected_idx=i; break; }
        dir_stamps=read_stamps();
    }

    // Called every frame; does the work about twice a second
    void poll_filesystem(){
        auto now=std::chrono::steady_clock::now();
        if(std::chrono::duration<double>(now-last_fs_check).count()<0.5) return;
        last_fs_check=now;
        if(action_mode!=ACT_NONE) return;   // do not move the list while the user types or picks files to delete
        if(read_stamps()!=dir_stamps) reload_keep_state();
    }

    void apply_filter(){ build_flat(); }

    // panel_h = height of the whole sidebar panel (the header is part of it)
    double max_scroll(int panel_h) const {
        return std::max(0.0,(double)flat.size()*ROW_H-(panel_h-header_h()));
    }

    // Keep scroll_y inside the list (the list gets shorter when folders are closed)
    void clamp_scroll(int panel_h){
        scroll_y=std::clamp(scroll_y,0.0,max_scroll(panel_h));
    }

    // Scroll just enough to show the selected row
    void scroll_to_selected(int panel_h){
        if(selected_idx<0||selected_idx>=(int)flat.size()) return;
        double view_h=(double)(panel_h-header_h());
        double top=(double)selected_idx*ROW_H, bottom=top+ROW_H;
        if(top<scroll_y) scroll_y=top;
        else if(bottom>scroll_y+view_h) scroll_y=bottom-view_h;
    }

    // Shared geometry for the Explorer toolbar row: New File button (left,
    // shrinks to icon-only when search is expanded) + Search button/input
    // (right, grows to fill when expanded). Both derived from the same
    // search_expand_anim so the two edges move in lockstep.
    void explorer_toolbar_rects(int sx, double &nf_x, double &nf_w,
                                 double &sr_x, double &sr_w) const {
        double total_w = (double)(width - 16);
        double icon_w  = 32.0;
        double gap     = 6.0;
        double e = std::clamp(search_expand_anim, 0.0, 1.0);
        nf_x = sx + 8.0;
        nf_w = (total_w - icon_w - gap) * (1.0 - e) + icon_w * e;
        sr_x = nf_x + nf_w + gap;
        sr_w = total_w - nf_w - gap;
    }
    static constexpr int TOOLBAR_Y = 32, TOOLBAR_H = 32;

    bool new_file_btn_hit(int mx,int my,int sx) const {
        if (my < TOOLBAR_Y || my > TOOLBAR_Y+TOOLBAR_H) return false;
        double nf_x,nf_w,sr_x,sr_w; explorer_toolbar_rects(sx,nf_x,nf_w,sr_x,sr_w);
        return mx>=nf_x && mx<=nf_x+nf_w;
    }
    bool search_region_hit(int mx,int my,int sx) const {
        if (my < TOOLBAR_Y || my > TOOLBAR_Y+TOOLBAR_H) return false;
        double nf_x,nf_w,sr_x,sr_w; explorer_toolbar_rects(sx,nf_x,nf_w,sr_x,sr_w);
        return mx>=sr_x && mx<=sr_x+sr_w;
    }
    // The magnifier glyph itself sits at the left edge of the search region
    // (whether collapsed to a 32px button or expanded into the input)
    // clicking exactly there toggles expand/collapse; clicking elsewhere in
    // an already-expanded input just focuses it for typing.
    bool search_icon_glyph_hit(int mx,int my,int sx) const {
        if (my < TOOLBAR_Y || my > TOOLBAR_Y+TOOLBAR_H) return false;
        double nf_x,nf_w,sr_x,sr_w; explorer_toolbar_rects(sx,nf_x,nf_w,sr_x,sr_w);
        double glyph_w = std::min(32.0, sr_w);
        return mx>=sr_x && mx<=sr_x+glyph_w;
    }

    bool search_bar_hit(int mx,int my,int sx) const {
        if(my<40||my>62) return false;
        double mode_pill_w=52.0;
        double search_w=(width-16)-mode_pill_w-4;
        return mx>=sx+8 && mx<=sx+8+search_w;
    }
    bool search_mode_pill_hit(int mx,int my,int sx) const {
        if (search_expand_anim < 0.98) return false; // only clickable once fully expanded
        double nf_x,nf_w,sr_x,sr_w; explorer_toolbar_rects(sx,nf_x,nf_w,sr_x,sr_w);
        double mode_pill_w=52.0;
        double pill_x = sr_x + sr_w - mode_pill_w;
        return mx>=pill_x && my>=TOOLBAR_Y && my<=TOOLBAR_Y+TOOLBAR_H && mx<=pill_x+mode_pill_w;
    }
    // hovered_btn==10 for mode pill, ==20 for new-file, ==21 for search
    void update_hover_btn(int mx,int my,int sx){
        if(search_mode_pill_hit(mx,my,sx)) hovered_btn=10;
        else if(new_file_btn_hit(mx,my,sx)) hovered_btn=20;
        else if(search_region_hit(mx,my,sx)) hovered_btn=21;
        else hovered_btn=-1;
    }

    // 3 action buttons - fill full width evenly (new file, new folder, delete)
    int action_btn_hit(int mx,int my,int sx) const {
        if(my<66||my>98) return -1;
        double bw=(double)(width-16)/3.0;
        int rel=mx-(sx+8);
        if(rel<0||rel>(width-16)) return -1;
        return std::min(2,(int)(rel/bw));
    }

    int hit_test_row(int mx,int my,int sx) const {
        if(!visible) return -1;
        if(mx<sx||mx>sx+width) return -1;
        int ry=my-header_h();
        if(ry<0) return -1;
        int idx=(int)((ry+scroll_y)/ROW_H);
        return (idx>=0&&idx<(int)flat.size())?idx:-1;
    }

    void toggle_expand(int idx){
        if(idx<0||idx>=(int)flat.size()) return;
        TreeNode *n=flat[idx];
        if(!n->is_dir) return;
        n->expanded=!n->expanded;
        if(n->expanded&&!n->children_loaded) load_children(*n);
        build_flat();
        dir_stamps=read_stamps();
    }

    void navigate(const fs::path &p){
        if(history_idx<(int)history.size()-1)
            history.erase(history.begin()+history_idx+1,history.end());
        history.push_back(p); history_idx=(int)history.size()-1;
        current_path=p; cancel_action(); refresh();
    }

    void begin_new_file()    { action_mode=ACT_NEW_FILE;   input_text.clear(); search_active=false; multi_sel.clear(); }
    void begin_new_folder()  { action_mode=ACT_NEW_FOLDER; input_text.clear(); search_active=false; multi_sel.clear(); }
    void begin_delete()      {
        if(action_mode==ACT_DELETE){cancel_action();return;}
        action_mode=ACT_DELETE; multi_sel.clear(); search_active=false; input_text.clear();
        if(selected_idx>=0&&selected_idx<(int)flat.size()) multi_sel.insert(selected_idx);
    }
    void toggle_multi_sel(int idx){
        if(idx<0||idx>=(int)flat.size())return;
        if(multi_sel.count(idx))multi_sel.erase(idx);else multi_sel.insert(idx);
    }
    void cancel_action(){ action_mode=ACT_NONE; input_text.clear(); multi_sel.clear(); }

    bool commit_create(){
        if(input_text.empty()){cancel_action();return false;}
        try{
            fs::path t=current_path/input_text;
            if(action_mode==ACT_NEW_FILE){std::ofstream f(t);f.close();}
            else fs::create_directory(t);
            cancel_action(); reload_keep_state(); return true;
        }catch(...){cancel_action();return false;}
    }

    bool commit_delete(){
        if(multi_sel.empty()){cancel_action();return false;}
        std::vector<int> idxs(multi_sel.rbegin(),multi_sel.rend());
        bool any=false;
        for(int idx:idxs){
            if(idx<0||idx>=(int)flat.size())continue;
            try{fs::remove_all(flat[idx]->path);any=true;}catch(...){}
        }
        cancel_action(); reload_keep_state(); return any;
    }

    bool handle_action_key(KeySym ks,const char *buf,int len){
        if(action_mode==ACT_NONE)return false;
        if(ks==XK_Escape){cancel_action();return true;}
        if(ks==XK_Return||ks==XK_KP_Enter){
            if(action_mode==ACT_DELETE) commit_delete();
            else commit_create();
            return true;
        }
        if(action_mode==ACT_NEW_FILE||action_mode==ACT_NEW_FOLDER){
            if(ks==XK_BackSpace){if(!input_text.empty())textor_pop_utf8(input_text);return true;}
            std::string text;
            if (len > 0 && buf[0] >= 0x20 && buf[0] != 0x7f) {
                text.assign(buf, len);
            } else if (ks != 0) {
                text = textor_map_key_to_text(static_cast<uint64_t>(ks), 0, 1u);
            }
            if (!text.empty()) { input_text.append(text); return true; }
        }
        return true;
    }

    // Ctrl+V in the New File / New Folder field: the first line of the text is inserted
    void paste_text(const std::string &text){
        if(action_mode!=ACT_NEW_FILE && action_mode!=ACT_NEW_FOLDER) return;
        std::string line=text.substr(0,text.find_first_of("\r\n"));
        input_text+=line;
    }

    // Global content search: scan all files under current_path for filter text
    void do_global_search(){
        global_match_paths.clear();
        if(filter.empty()) return;
        std::string lo_q=filter;
        for(auto&c:lo_q)c=(char)tolower((unsigned char)c);
        try{
            for(auto &de:fs::recursive_directory_iterator(current_path,
                    fs::directory_options::skip_permission_denied)){
                if(!de.is_regular_file()) continue;
                // Skip binary-likely extensions
                auto ext=de.path().extension().string();
                for(auto&c:ext)c=(char)tolower((unsigned char)c);
                static const std::set<std::string> bin_ext={
                    ".png",".jpg",".jpeg",".gif",".bmp",".ico",".svg",
                    ".mp3",".mp4",".avi",".mkv",".mov",".wav",".ogg",
                    ".zip",".tar",".gz",".xz",".7z",".rar",".bz2",
                    ".pdf",".o",".so",".a",".dylib",".exe",".bin",
                    ".ttf",".otf",".woff",".woff2"};
                if(bin_ext.count(ext)) continue;
                // Skip large files (>2MB)
                try{if(fs::file_size(de)>2*1024*1024)continue;}catch(...){}
                try{
                    std::ifstream f(de.path());
                    std::string line;
                    bool found=false;
                    while(std::getline(f,line)&&!found){
                        std::string lo=line;
                        for(auto&c:lo)c=(char)tolower((unsigned char)c);
                        if(lo.find(lo_q)!=std::string::npos) found=true;
                    }
                    if(found) global_match_paths.insert(de.path());
                }catch(...){}
            }
        }catch(...){}
    }

    // Draw helpers
    void rrect(cairo_t *cr,double x,double y,double w,double h,double r) const {
        shape::rrect(cr,x,y,w,h,r);
    }

    struct IconColor{double r,g,b;};
    static IconColor ext_color(const std::string &ext,const ThemeVariant &tv){
        // These few pull from the *current* theme, so they can't live in a
        // table built once and reused across themes - checked first, same
        // colors and precedence as before.
        if(ext=="cpp"||ext=="cxx"||ext=="cc"||ext=="c") return {tv.accent_r*0.8+0.1,tv.accent_g*0.9+0.05,tv.accent_b};
        if(ext=="hxx"||ext=="hpp"||ext=="hh"||ext=="h") return {tv.function_r,tv.function_g,tv.function_b*0.8};
        if(ext=="sh"||ext=="bash"||ext=="zsh") return {tv.success_r,tv.success_g,tv.success_b};
        if(ext=="md"||ext=="txt"||ext=="rst"){double m=std::min(1.0,tv.muted*1.3);return {m,m,m};}
        if(ext=="pdf") return {tv.error_r,tv.error_g*0.7,tv.error_b*0.5};
        if(ext=="zip"||ext=="tar"||ext=="gz"||ext=="xz"||ext=="7z") return {tv.warning_r,tv.warning_g*0.7,tv.warning_b*0.3};

        // Everything else is a fixed color regardless of theme - one hash
        // lookup instead of walking the rest of an if/else-if ladder.
        static const dispatch::FastDispatcher<std::string, IconColor> table = [] {
            dispatch::FastDispatcher<std::string, IconColor> t(IconColor{0,0,0}); // never used, see has_case() below
            t.add_case("d",    IconColor{0.9,0.55,0.2});
            t.add_case("rs",   IconColor{0.88,0.38,0.28});
            t.add_case("py",   IconColor{0.3,0.72,0.94});
            t.add_case("js",   IconColor{0.95,0.8,0.2}).add_case("ts",  IconColor{0.95,0.8,0.2})
             .add_case("jsx",  IconColor{0.95,0.8,0.2}).add_case("tsx", IconColor{0.95,0.8,0.2});
            t.add_case("lua",  IconColor{0.45,0.55,0.95});
            t.add_case("kt",   IconColor{0.72,0.45,0.95}).add_case("kts", IconColor{0.72,0.45,0.95});
            t.add_case("go",   IconColor{0.38,0.74,0.82});
            t.add_case("rb",   IconColor{0.88,0.25,0.3});
            t.add_case("java", IconColor{0.82,0.46,0.2});
            t.add_case("json", IconColor{0.75,0.75,0.5}).add_case("yaml", IconColor{0.75,0.75,0.5})
             .add_case("yml",  IconColor{0.75,0.75,0.5}).add_case("toml", IconColor{0.75,0.75,0.5});
            t.add_case("png",  IconColor{0.9,0.55,0.75}).add_case("jpg",  IconColor{0.9,0.55,0.75})
             .add_case("jpeg", IconColor{0.9,0.55,0.75}).add_case("gif",  IconColor{0.9,0.55,0.75})
             .add_case("svg",  IconColor{0.9,0.55,0.75});
            t.add_case("cmake",IconColor{0.7,0.45,0.9}).add_case("mk",   IconColor{0.7,0.45,0.9});
            t.add_case("html", IconColor{0.9,0.55,0.35}).add_case("css", IconColor{0.9,0.55,0.35})
             .add_case("xml",  IconColor{0.9,0.55,0.35});
            t.add_case("swift",IconColor{0.94,0.36,0.24});
            t.add_case("nim",  IconColor{0.96,0.85,0.28});
            t.add_case("mek",  IconColor{0.70,0.45,0.90});
            t.add_case("conf", IconColor{0.75,0.75,0.5})
             .add_case("adco", IconColor{0.70,0.52,0.95});
            return t;
        }();
        if (table.has_case(ext)) return table.dispatch(ext);
        return {tv.muted,tv.muted,tv.muted};
    }

    // Icon of a file: the addon's SVG, else the built-in SVG of the language, else a plain file shape
    void draw_lang_icon(cairo_t *cr, double x, double y, double sz,
                        const std::string &ext, const ThemeVariant &tv) const {
        std::string sym = ext;
        IconColor ic = ext_color(ext, tv);
        const LangAddon *addon = addon_loader ? addon_loader->find_by_ext(ext) : nullptr;
        if (addon) {
            if (addon->icon_r >= 0) ic = {addon->icon_r, addon->icon_g, addon->icon_b};
            if (!addon->icon_symbol.empty()) sym = addon->icon_symbol;
        }

        // The icon from the addon's own folder (svg/) wins over everything else
        if (addon && svg_icons::draw_text(cr, addon->icon_svg_path, addon->icon_svg,
                                          x, y, sz, ic.r, ic.g, ic.b, 0.95))
            return;

        // A language with a built-in SVG icon: just the icon in the language colour
        // (looked up by the addon symbol first, then by the extension)
        if (svg_icons::draw(cr, sym, x, y, sz, ic.r, ic.g, ic.b, 0.95) ||
            svg_icons::draw(cr, ext, x, y, sz, ic.r, ic.g, ic.b, 0.95))
            return;

        // Files without an icon of their own: the plain file icon in the language colour
        svg_icons::draw_plain_file(cr, x, y, sz, ic.r, ic.g, ic.b, 0.95);
    }

    void draw_folder_icon(cairo_t *cr,double x,double y,double sz,bool expanded_state,const ThemeVariant &tv) const {
        // SVG folder in the accent colour; an open folder is a bit brighter
        svg_icons::draw_ui(cr,"folder",x,y,sz,tv.accent_r,tv.accent_g,tv.accent_b,expanded_state?0.95:0.75);
    }

    // Arrow in front of a folder: it points right when closed and turns down when opened
    void draw_expand_arrow(cairo_t *cr,double cx,double cy,double anim,const ThemeVariant &tv) const {
        const double sz=10.0;
        cairo_save(cr);
        cairo_translate(cr,cx,cy);
        cairo_rotate(cr,anim*(M_PI/2.0));
        svg_icons::draw_ui(cr,"arrow_right",-sz/2,-sz/2,sz,tv.muted,tv.muted,tv.muted,0.6);
        cairo_restore(cr);
    }

    void draw_indent_line(cairo_t *cr,double x,double y_top,double y_bot,const ThemeVariant &tv) const {
        cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,0.12);
        cairo_set_line_width(cr,1.0);
        cairo_move_to(cr,x,y_top);cairo_line_to(cr,x,y_bot);cairo_stroke(cr);
    }

    void tick_anims(){
        auto now=std::chrono::steady_clock::now();
        double dt=std::chrono::duration_cast<std::chrono::duration<double>>(now-last_tick).count();
        last_tick=now;
        dt=std::min(dt,0.05);
        // Update arrow anims for visible nodes
        for(auto *n:flat){
            if(n->is_dir){
                double target=n->expanded?1.0:0.0;
                double speed=g_animations_enabled?14.0:1e9;
                n->arrow_anim+=( target-n->arrow_anim)*std::clamp(speed*dt,0.0,1.0);
            }
        }
        {
            double target = search_expanded ? 1.0 : 0.0;
            double speed  = g_animations_enabled ? 12.0 : 1e9;
            search_expand_anim += (target - search_expand_anim) * std::clamp(speed*dt, 0.0, 1.0);
        }
    }

    void draw(cairo_t *cr,int win_h,int sx,const ThemeVariant &tv){
        poll_filesystem();   // files created or deleted outside the editor show up here
        if(!visible||width<=0)return;
        tick_anims();

        // Background
        double sb_bg=tv.panel*0.98+tv.elevated*0.02;
        cairo_set_source_rgb(cr,sb_bg,sb_bg,sb_bg+0.008);
        cairo_rectangle(cr,sx,0,width,win_h);cairo_fill(cr);
        // Right edge - flat hairline, no gradient falloff
        cairo_set_source_rgba(cr,0,0,0,0.20);
        cairo_rectangle(cr,sx+width-1,0,1,win_h);cairo_fill(cr);

        PangoLayout *layout=pango_cairo_create_layout(cr);

        // Header - "EXPLORER" label
        {
            PangoFontDescription *hf=pango_font_description_from_string((tv.font_family+" Bold 10").c_str());
            pango_layout_set_font_description(layout,hf);
            pango_layout_set_text(layout,"EXPLORER",-1);
            cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,0.85);
            cairo_move_to(cr,sx+12,10);pango_cairo_show_layout(cr,layout);
            pango_font_description_free(hf);
        }

        // Explorer toolbar - New File (shrinks to icon) + Search (expands)
        {
            double nf_x,nf_w,sr_x,sr_w;
            explorer_toolbar_rects(sx,nf_x,nf_w,sr_x,sr_w);
            double by=TOOLBAR_Y, bh=TOOLBAR_H, br=7.0;
            double e = std::clamp(search_expand_anim, 0.0, 1.0);
            bool nf_hov = hovered_btn==20, sr_hov = hovered_btn==21;
            bool nf_active = action_mode==ACT_NEW_FILE;

            // New File button - solid accent fill, "+ New File" label that
            // fades out as it shrinks to an icon-only square
            {
                double fill_a = nf_active ? 1.0 : (nf_hov ? 0.92 : 0.85);
                cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, fill_a);
                rrect(cr, nf_x, by, nf_w, bh, br); cairo_fill(cr);

                double cx2 = nf_x + nf_w/2.0, cy2 = by + bh/2.0;
                // "+" glyph - stays centered whether icon-only or full width,
                // shifted left of center once there's room for the label.
                double plus_cx = (1.0 - e) * (nf_x + 18.0) + e * cx2;
                svg_icons::draw_ui(cr, "plus", plus_cx-6, cy2-6, 12, tv.bg, tv.bg, tv.bg, 1.0);

                // Label - only drawn once there's meaningfully more than
                // icon-width, and fades with the shrink.
                double label_room = nf_w - 34.0;
                if (label_room > 6.0) {
                    double label_a = std::clamp(label_room / 40.0, 0.0, 1.0);
                    PangoFontDescription *sf=pango_font_description_from_string((tv.font_family+" Bold 9.5").c_str());
                    pango_layout_set_font_description(layout,sf);
                    pango_layout_set_text(layout,"New File",-1);
                    cairo_set_source_rgba(cr, tv.bg, tv.bg, tv.bg, label_a);
                    // Center on the button's true measured text height
                    // instead of a fixed guess, so it stays aligned with
                    // the "+" glyph regardless of font/theme changes.
                    int lbl_w, lbl_h; pango_layout_get_pixel_size(layout, &lbl_w, &lbl_h);
                    cairo_move_to(cr, plus_cx+11, cy2 - lbl_h/2.0);
                    pango_cairo_show_layout(cr,layout);
                    pango_font_description_free(sf);
                }
            }

            // Search - 32px icon button when collapsed, expands into the
            // filter input (with mode pill) as e->1
            {
                cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted,
                                      (search_active?0.20:(sr_hov?0.16:0.10)));
                rrect(cr, sr_x, by, sr_w, bh, br); cairo_fill(cr);
                if (search_active) {
                    cairo_set_source_rgba(cr,tv.accent_r,tv.accent_g,tv.accent_b,0.48);
                    cairo_set_line_width(cr,1.0);
                    rrect(cr,sr_x+0.5,by+0.5,sr_w-1,bh-1,br); cairo_stroke(cr);
                }

                // Magnifier - sits at the left edge of the region always
                double ix2 = sr_x + std::min(16.0, sr_w*0.5), iy2 = by + bh/2.0;
                svg_icons::draw_ui(cr,"search",ix2-7,iy2-7,14,tv.muted,tv.muted,tv.muted,0.7);

                // Filter text / placeholder + mode pill - fade in once mostly expanded
                double mode_pill_w=52.0;
                if (e > 0.55) {
                    double text_a = std::clamp((e-0.55)/0.35, 0.0, 1.0);
                    double text_x = sr_x + 26.0;
                    double text_w = std::max(10.0, sr_w - 26.0 - mode_pill_w - 8.0);
                    PangoFontDescription *sf=pango_font_description_from_string((tv.font_family+" 9").c_str());
                    pango_layout_set_font_description(layout,sf);
                    const char *ph=(search_mode==SEARCH_FILENAME)?"Filter by name...":
                                   (search_mode==SEARCH_CONTENT)?"Search in files...":"Regex in files...";
                    if(filter.empty()){
                        pango_layout_set_text(layout,ph,-1);
                        cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,0.42*text_a);
                    } else {
                        pango_layout_set_text(layout,filter.c_str(),-1);
                        cairo_set_source_rgba(cr,tv.text,tv.text,tv.text,0.9*text_a);
                    }
                    pango_layout_set_width(layout,(int)(text_w*PANGO_SCALE));
                    pango_layout_set_ellipsize(layout,PANGO_ELLIPSIZE_END);
                    int fw,fh;pango_layout_get_pixel_size(layout,&fw,&fh);
                    cairo_move_to(cr,text_x,by+(bh-fh)/2.0); pango_cairo_show_layout(cr,layout);
                    if (search_active) {
                        cairo_set_source_rgba(cr,tv.cursor_r,tv.cursor_g,tv.cursor_b,0.85*text_a);
                        cairo_rectangle(cr,text_x+(filter.empty()?0:fw),by+(bh-fh)/2.0,1.5,fh);
                        cairo_fill(cr);
                    }
                    pango_font_description_free(sf);

                    // Mode pill
                    if (e > 0.9) {
                        double pill_a = std::clamp((e-0.9)/0.1, 0.0, 1.0);
                        double pill_x = sr_x + sr_w - mode_pill_w;
                        bool mode_hov=(hovered_btn==10);
                        cairo_set_source_rgba(cr,tv.accent_r,tv.accent_g,tv.accent_b,(mode_hov?0.28:0.16)*pill_a);
                        rrect(cr,pill_x,by+2,mode_pill_w,bh-4,5); cairo_fill(cr);
                        const char *mlbls[3]={"Name","Text","Regex"};
                        PangoFontDescription *pf=pango_font_description_from_string((tv.font_family+" Bold 8").c_str());
                        pango_layout_set_font_description(layout,pf);
                        pango_layout_set_text(layout,mlbls[(int)search_mode],-1);
                        int lw2,lh2;pango_layout_get_pixel_size(layout,&lw2,&lh2);
                        cairo_set_source_rgba(cr,tv.accent_r,tv.accent_g,tv.accent_b,0.95*pill_a);
                        cairo_move_to(cr,pill_x+(mode_pill_w-lw2)/2.0,by+(bh-lh2)/2.0);
                        pango_cairo_show_layout(cr,layout);
                        pango_font_description_free(pf);
                    }
                }
            }
        }

        // Inline input (shown below buttons, pushes list down)
        // No separate caption line above the box anymore - it used to read
        // "New file name:" / "Press Enter to confirm delete:" floating above
        // the field, which was requested removed outright. The purpose is
        // now conveyed by the input's own placeholder text (new file/folder)
        // or inside the confirm pill itself (delete) - one element, not two.
        if(action_mode!=ACT_NONE){
            double iby=HEADER_BASE-6;
            double ibx=sx+6,ibw=width-16,ibh=INPUT_H-6;
            double box_top=iby+4;
            if(action_mode==ACT_DELETE){
                cairo_set_source_rgba(cr,0.90,0.30,0.30,0.10);
                rrect(cr,ibx,box_top,ibw,ibh,5);cairo_fill(cr);
                cairo_set_source_rgba(cr,0.90,0.30,0.30,0.45);
                cairo_set_line_width(cr,1.0);rrect(cr,ibx+0.5,box_top+0.5,ibw-1,ibh-1,5);cairo_stroke(cr);
                PangoFontDescription *df=pango_font_description_from_string((tv.font_family+" 8.5").c_str());
                PangoLayout *dl=pango_cairo_create_layout(cr);
                pango_layout_set_font_description(dl,df);
                pango_layout_set_text(dl,"Press Enter to confirm delete",-1);
                cairo_set_source_rgba(cr,0.95,0.55,0.55,0.95);
                int dlw,dlh;pango_layout_get_pixel_size(dl,&dlw,&dlh);
                cairo_move_to(cr,ibx+(ibw-dlw)/2.0,box_top+(ibh-dlh)/2.0);
                pango_cairo_show_layout(cr,dl);
                g_object_unref(dl);pango_font_description_free(df);
            } else {
                cairo_set_source_rgba(cr,tv.accent_r,tv.accent_g,tv.accent_b,0.08);
                rrect(cr,ibx,box_top,ibw,ibh,5);cairo_fill(cr);
                cairo_set_source_rgba(cr,tv.accent_r,tv.accent_g,tv.accent_b,0.42);
                cairo_set_line_width(cr,1.0);rrect(cr,ibx+0.5,box_top+0.5,ibw-1,ibh-1,5);cairo_stroke(cr);
                PangoFontDescription *inf=pango_font_description_from_string((tv.font_family+" 9").c_str());
                PangoLayout *inl=pango_cairo_create_layout(cr);
                pango_layout_set_font_description(inl,inf);
                const char *placeholder = action_mode==ACT_NEW_FILE ? "New file name..." : "New folder name...";
                std::string disp=input_text.empty()?placeholder:input_text;
                pango_layout_set_text(inl,disp.c_str(),-1);
                cairo_set_source_rgba(cr,tv.text,tv.text,tv.text,input_text.empty()?0.28:0.90);
                int ilw,ilh;pango_layout_get_pixel_size(inl,&ilw,&ilh);
                cairo_move_to(cr,ibx+6,box_top+(ibh-ilh)/2.0);pango_cairo_show_layout(cr,inl);
                if(!input_text.empty()){
                    cairo_set_source_rgba(cr,tv.accent_r,tv.accent_g,tv.accent_b,0.85);
                    cairo_set_line_width(cr,1.2);
                    double cxp=ibx+6+ilw+1;
                    cairo_move_to(cr,cxp,box_top+2);cairo_line_to(cr,cxp,box_top+ibh-2);cairo_stroke(cr);
                }
                g_object_unref(inl);pango_font_description_free(inf);
            }
        }

        // Separator
        cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,0.1);
        cairo_rectangle(cr,sx+6,header_h()-2,width-12,1);cairo_fill(cr);

        // Scroll position is fixed here, with the real height of the panel
        if(follow_selection){ scroll_to_selected(win_h); follow_selection=false; }
        clamp_scroll(win_h);

        // File list
        cairo_save(cr);
        cairo_rectangle(cr,sx,header_h(),width,win_h-header_h());cairo_clip(cr);

        PangoFontDescription *fnt =pango_font_description_from_string((tv.font_family+" 9").c_str());
        PangoFontDescription *sfnt=pango_font_description_from_string((tv.font_family+" 8").c_str());
        pango_layout_set_font_description(layout,fnt);
        double icon_sz=17.0;

        for(int i=0;i<(int)flat.size();++i){
            double iy=header_h()+i*ROW_H-scroll_y;
            if(iy+ROW_H<header_h())continue;
            if(iy>win_h)break;

            TreeNode *fe=flat[i];
            bool hov=(i==hovered_idx);
            bool sel=(i==selected_idx);
            bool msel=multi_sel.count(i)>0;
            // depth=0 is top-level inside root, so no extra indent for depth -1 root
            int effective_depth=std::max(0,fe->depth);
            double depth_offset=effective_depth*14.0;

            // Global search match?
            bool gmatch=(search_mode!=SEARCH_FILENAME&&!filter.empty()&&
                         global_match_paths.count(fe->path)>0);

            // Row background
            if(action_mode==ACT_DELETE&&msel){
                cairo_set_source_rgba(cr,tv.error_r,tv.error_g,tv.error_b,0.14);
                rrect(cr,sx+4,iy+1,width-8,ROW_H-2,4);cairo_fill(cr);
                cairo_set_source_rgba(cr,tv.error_r,tv.error_g,tv.error_b,0.55);
                rrect(cr,sx+2,iy+5,3,ROW_H-10,2);cairo_fill(cr);
            } else if(sel){
                cairo_set_source_rgba(cr,tv.accent_r,tv.accent_g,tv.accent_b,0.16);
                rrect(cr,sx+4,iy+1,width-8,ROW_H-2,4);cairo_fill(cr);
                cairo_set_source_rgba(cr,tv.accent_r,tv.accent_g,tv.accent_b,0.85);
                rrect(cr,sx+2,iy+5,3,ROW_H-10,2);cairo_fill(cr);
            } else if(hov){
                cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,0.07);
                rrect(cr,sx+4,iy+1,width-8,ROW_H-2,4);cairo_fill(cr);
            }
            // Global content-search match glow
            if(gmatch){
                cairo_set_source_rgba(cr,tv.success_r,tv.success_g,tv.success_b,0.10);
                rrect(cr,sx+4,iy+1,width-8,ROW_H-2,4);cairo_fill(cr);
                cairo_set_source_rgba(cr,tv.success_r,tv.success_g,tv.success_b,0.65);
                rrect(cr,sx+2,iy+5,3,ROW_H-10,2);cairo_fill(cr);
            }

            // Tree indent guide lines - drawn within clip so they can't bleed
            for(int d=0;d<effective_depth;d++){
                double lx=sx+18.0+d*14.0;
                draw_indent_line(cr,lx,iy,iy+ROW_H,tv);
            }

            // Layout: [indent] [checkbox?] [arrow?] [icon] [name]
            double cur_x = sx + 10.0 + depth_offset;

            // Checkbox in delete mode - comes BEFORE arrow/icon
            if(action_mode==ACT_DELETE){
                double csz=11;
                double cx2=cur_x,cy2=iy+(ROW_H-csz)/2.0;
                if(msel){
                    cairo_set_source_rgba(cr,tv.error_r,tv.error_g,tv.error_b,0.85);
                    rrect(cr,cx2,cy2,csz,csz,2);cairo_fill(cr);
                    cairo_set_source_rgb(cr,1,1,1);cairo_set_line_width(cr,1.4);
                    cairo_move_to(cr,cx2+2,cy2+csz/2.0);
                    cairo_line_to(cr,cx2+csz/2.0-0.5,cy2+csz-3.0);
                    cairo_line_to(cr,cx2+csz-2,cy2+2.5);
                    cairo_stroke(cr);
                } else {
                    cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,0.18);
                    rrect(cr,cx2,cy2,csz,csz,2);cairo_fill(cr);
                    cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,0.40);
                    cairo_set_line_width(cr,1.0);
                    rrect(cr,cx2,cy2,csz,csz,2);cairo_stroke(cr);
                }
                cur_x += csz + 4.0;
            }

            // Expand arrow for dirs
            if(fe->is_dir){
                double ax=cur_x+5.0, ay=iy+ROW_H/2.0;
                draw_expand_arrow(cr,ax,ay,fe->arrow_anim,tv);
            }
            cur_x += 12.0;

            // Icon
            double fiy=iy+(ROW_H-icon_sz)/2.0;
            if(fe->is_dir) draw_folder_icon(cr,cur_x,fiy,icon_sz,fe->expanded,tv);
            else { draw_lang_icon(cr,cur_x,fiy,icon_sz,fe->ext,tv); }
            cur_x += icon_sz + 5.0;

            double tx2=cur_x;
            double avail_w=(double)(width-(int)(tx2-sx)-10);

            // The parts on the right come first: the size label and the extension badge.
            // The name gets what is left, so it can never run into them.
            int sw2=0,sh2=0;
            std::string size_text;
            if(!fe->is_dir&&fe->size_bytes>0){
                size_text=fmt_size(fe->size_bytes);
                pango_layout_set_font_description(layout,sfnt);
                pango_layout_set_text(layout,size_text.c_str(),-1);
                pango_layout_set_width(layout,-1);
                pango_layout_get_pixel_size(layout,&sw2,&sh2);
                avail_w-=sw2+8;
            }

            bool show_badge=!fe->is_dir&&!fe->ext.empty()&&(int)fe->ext.size()<=5&&action_mode!=ACT_DELETE;
            int ew=0,eh=0;
            PangoFontDescription *badge_font=pango_font_description_from_string((tv.font_family+" Bold 6").c_str());
            if(show_badge){
                pango_layout_set_font_description(layout,badge_font);
                pango_layout_set_text(layout,fe->ext.c_str(),-1);
                pango_layout_set_width(layout,-1);
                pango_layout_get_pixel_size(layout,&ew,&eh);
                if(avail_w-(ew+10)<50) show_badge=false;   // narrow sidebar: the name matters more
                else avail_w-=ew+10;
            }

            // Name: cut with "..." at the end when it does not fit
            pango_layout_set_font_description(layout,fnt);
            pango_layout_set_text(layout,fe->name.c_str(),-1);
            avail_w=std::max(20.0,avail_w);   // a negative width would switch the cutting off
            pango_layout_set_width(layout,(int)(avail_w*PANGO_SCALE));
            pango_layout_set_ellipsize(layout,PANGO_ELLIPSIZE_END);

            // Search highlight
            if(!filter.empty()){
                std::string ln=fe->name,lf=filter;
                for(auto&c:ln)c=(char)tolower((unsigned char)c);
                for(auto&c:lf)c=(char)tolower((unsigned char)c);
                size_t mp=ln.find(lf);
                if(mp!=std::string::npos){
                    PangoAttrList *al=pango_attr_list_new();
                    auto *a=pango_attr_foreground_new((guint16)(tv.accent_r*65535),(guint16)(tv.accent_g*65535),(guint16)(tv.accent_b*65535));
                    a->start_index=(guint)mp;a->end_index=(guint)(mp+filter.size());
                    pango_attr_list_insert(al,a);pango_layout_set_attributes(layout,al);pango_attr_list_unref(al);
                }
            }

            double tc;
            if(action_mode==ACT_DELETE&&msel) tc=tv.error_r*0.5+tv.text*0.5;
            else tc=sel?tv.text:(fe->is_dir?tv.text:std::min(1.0,tv.muted*1.15));
            cairo_set_source_rgb(cr,tc,tc,tc);
            int fw2,fh2;pango_layout_get_pixel_size(layout,&fw2,&fh2);
            cairo_move_to(cr,tx2,iy+(ROW_H-fh2)/2.0);pango_cairo_show_layout(cr,layout);
            pango_layout_set_attributes(layout,nullptr);
            pango_layout_set_ellipsize(layout,PANGO_ELLIPSIZE_NONE);

            // Extension badge: right after the name
            if(show_badge){
                auto ic=ext_color(fe->ext,tv);
                double px2=tx2+fw2+4,py2=iy+(ROW_H)/2.0-5,pw2=ew+6;
                cairo_set_source_rgba(cr,ic.r,ic.g,ic.b,0.18);
                rrect(cr,px2,py2,pw2,10,3);cairo_fill(cr);
                cairo_set_source_rgba(cr,ic.r,ic.g,ic.b,0.8);
                pango_layout_set_font_description(layout,badge_font);
                pango_layout_set_text(layout,fe->ext.c_str(),-1);pango_layout_set_width(layout,-1);
                cairo_move_to(cr,px2+3,py2+(10-eh)/2.0);pango_cairo_show_layout(cr,layout);
            }
            pango_font_description_free(badge_font);

            // Size label: at the right edge
            if(!size_text.empty()){
                pango_layout_set_font_description(layout,sfnt);
                pango_layout_set_text(layout,size_text.c_str(),-1);pango_layout_set_width(layout,-1);
                cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,0.45);
                cairo_move_to(cr,sx+width-sw2-8,iy+(ROW_H-sh2)/2.0);pango_cairo_show_layout(cr,layout);
            }
            pango_layout_set_font_description(layout,fnt);
        }

        if(flat.empty()){
            std::string msg=filter.empty()?"Folder is empty":"No matches for \""+filter+"\"";
            pango_layout_set_font_description(layout,fnt);
            pango_layout_set_text(layout,msg.c_str(),-1);pango_layout_set_width(layout,-1);
            int mw,mh;pango_layout_get_pixel_size(layout,&mw,&mh);
            cairo_set_source_rgba(cr,tv.muted,tv.muted,tv.muted,0.35);
            cairo_move_to(cr,sx+(width-mw)/2.0,header_h()+24);pango_cairo_show_layout(cr,layout);
        }

        cairo_restore(cr);

        // Scrollbar
        double content_h2=(double)flat.size()*ROW_H,view_h2=(double)(win_h-header_h());
        if(content_h2>view_h2){
            double th=std::max(18.0,view_h2*(view_h2/content_h2));
            double ms=std::max(0.0,content_h2-view_h2);
            double ty=header_h()+2+(ms>0?(scroll_y/ms)*(view_h2-4-th):0);
            cairo_set_source_rgba(cr,tv.accent_r,tv.accent_g,tv.accent_b,0.3);
            rrect(cr,sx+width-5,ty,3,th,1.5);cairo_fill(cr);
        }

        pango_font_description_free(fnt);
        pango_font_description_free(sfnt);
        g_object_unref(layout);

    }

    void draw(cairo_t *cr,int win_h,int x){
        ThemeVariant t=default_themes()[0];draw(cr,win_h,x,t);
    }
};

#endif
