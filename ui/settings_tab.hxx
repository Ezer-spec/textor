#ifndef SETTINGS_TAB_HXX
#define SETTINGS_TAB_HXX

#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <string>
#include <vector>
#include <cmath>
#include <chrono>
#include <algorithm>
#include <map>
#include <functional>
#include "svg_icons.hxx"
#include "../color/themes.hxx"
#include "morphing.hxx"
#include "icon_anim.hxx"
#include "../base/shortcut_manager.hxx"
#include "../base/globals.hxx"
#include "../base/addon_loader.hxx"

struct SettingsTab {

    enum Section {
        SEC_GENERAL=0, SEC_PERF=1, SEC_THEMES=2,
        SEC_KEYS=3, SEC_HELP=4, SEC_ABOUT=5
    };
    int current_section  = 0;
    int hovered_section  = -1;
    const int NAV_W      = 220;

    std::function<void()> on_addon_reload;
    std::function<void()> on_show_onboarding;  // wired by Editor - re-arms first-run setup
    double show_onboarding_link_x = 0, show_onboarding_link_y = 0;
    double show_onboarding_link_w = 0, show_onboarding_link_h = 0;
    bool   show_onboarding_link_hovered = false;
    AddonLoader *addon_loader = nullptr;

    bool   show_line_numbers  = true;
    bool   show_splash        = true;
    bool   autocomplete_on    = true;
    bool   auto_bracket       = true;
    // ............ Performance flags ............
    bool   animations_enabled = true;
    bool   rounded_corners    = true;
    bool   text_optimized     = true;  // fast Cairo hint mode, reduced AA
    bool   line_scroll        = false;  // line-by-line scroll (console-style)
    bool   smart_render       = true;  // virtual viewport: skip off-screen lines
    int    indent_size        = 4;
    double indent_size_anim   = 4.0;
    bool   dragging_indent    = false;
    double gutter_density     = 1.0;
    double line_spacing       = 1.0;

    // ............ Editor hints / suggestions ............
    bool   word_hints_on             = true;   // word-match suggestions from buffer
    bool   inline_hints_on           = true;   // inline type/param hints while typing
    bool   double_click_word_select  = true;   // double-tap selects the word under cursor

    // ............ FPS limits ............
    int    fps_max            = 60;
    int    fps_avg            = 30;
    int    fps_min            =  5;
    double fps_max_anim       = 60.0;
    double fps_avg_anim       = 30.0;
    double fps_min_anim       =  5.0;
    bool   dragging_fps_max   = false;
    bool   dragging_fps_avg   = false;
    bool   dragging_fps_min   = false;
    // Scrollbar thumb press animation (0=normal, 1=pressed->shrink)
    double scroll_press_anim_g = 0.0;   // general
    double scroll_press_anim_p = 0.0;   // perf
    double scroll_press_anim_t = 0.0;   // themes
    double scroll_press_anim_k = 0.0;   // keys
    double scroll_press_anim_h = 0.0;   // help
    bool   scroll_pressed = false;

    // ............ Real drag-to-scroll for the settings scrollbar ............
    // scrollbar() below used to be purely decorative: it drew a thumb but
    // nothing ever hit-tested the mouse against it, so it could never
    // actually be dragged (scroll_pressed was only ever set to *false*).
    // These fields cache the track geometry + which section's scroll
    // variable is currently bound, updated every time scrollbar() draws,
    // so a later mouse-down/move/up can do a real drag against the exact
    // same geometry that was just rendered.
    double  scroll_track_x = 0.0, scroll_track_y = 0.0, scroll_track_h = 0.0;
    double  scroll_total_h = 0.0;
    double *scroll_bound   = nullptr;
    bool    dragging_settings_scroll = false;
    double  drag_start_my_settings = 0.0, drag_start_val_settings = 0.0;

    // Also cache the true measured content height per section (replaces
    // hand-maintained magic-number formulas that had drifted out of sync
    // with what draw_general/draw_perf/draw_keys/draw_help actually laid
    // out - which is why mouse-wheel scrolling could stop short of, or
    // overshoot, the real bottom of the content).
    double general_total_height = 0.0;
    double perf_total_height    = 0.0;
    double keys_total_height    = 0.0;
    double help_total_height    = 0.0;

    std::vector<std::string> font_families = {
        "JetBrains Mono","Fira Code","Source Code Pro",
        "Cascadia Code","Hack","Inconsolata","Ubuntu Mono","Monospace",
        "DejaVu Sans Mono","Liberation Mono","Noto Sans Mono","Roboto Mono",
        "Anonymous Pro","IBM Plex Mono","Space Mono","Courier New"
    };
    int    font_family_idx  = 0;
    int    font_size_pt     = 12;
    double font_size_anim   = 12.0;
    bool   dragging_font_size = false;
    // Slider thumb squeeze animation (0=idle, 1=dragging)
    double drag_anim_font_size = 0.0;
    double drag_anim_spacing   = 0.0;
    double drag_anim_indent    = 0.0;
    double drag_anim_gutter    = 0.0;
    double drag_anim_fps_max   = 0.0;
    double drag_anim_fps_avg   = 0.0;
    double drag_anim_fps_min   = 0.0;

    ShortcutManager shortcuts;
    int    editing_key_idx  = -1;
    bool   waiting_for_key  = false;
    int    hovered_key_idx  = -1;
    double keys_scroll      = 0.0;

    std::vector<ThemeVariant> themes = default_themes();
    int selected_theme_idx  = 0;

    ThemeVariant get_selected_theme() const {
        if (selected_theme_idx >= 0 && selected_theme_idx < (int)themes.size())
            return themes[selected_theme_idx];
        return themes.empty() ? ThemeVariant{} : themes[0];
    }

    bool   dragging_spacing = false;
    bool   dragging_gutter  = false;
    double slider_base_x    = 0;
    double slider_base_w    = 0;
    // Cached slider geometry (set once at mouse_down, not in draw)
    double cached_sl_x      = 0;
    double cached_sl_w      = 0;

    // Cached slider Y positions and x/w (set during draw, used in hit-test)
    double cached_font_size_y   = 0;
    double cached_font_family_y = 0;
    double cached_spacing_y     = 0;
    double cached_indent_y      = 0;
    double cached_gutter_y      = 0;
    double cached_fps_max_y     = 0;
    double cached_fps_avg_y     = 0;
    double cached_fps_min_y     = 0;
    double cached_sl_x_g        = 0;  // General sl_x
    double cached_sl_w_g        = 0;  // General sl_w
    double cached_sl_x_p        = 0;  // Perf sl_x
    double cached_sl_w_p        = 0;  // Perf sl_w

    double general_scroll       = 0.0;
    double perf_scroll          = 0.0;
    double themes_scroll        = 0.0;
    double themes_total_height  = 0.0;
    double themes_view_height   = 0.0;
    int    hovered_theme_idx    = -1;
    double help_scroll          = 0.0;
    static constexpr int THEMES_LIST_W = 212;

    ToggleButton toggle_line_numbers;
    ToggleButton toggle_splash;
    ToggleButton toggle_autocomplete;
    ToggleButton toggle_auto_bracket;
    // Performance tab toggles
    ToggleButton toggle_animations;
    ToggleButton toggle_rounded;
    ToggleButton toggle_text_optimized;
    ToggleButton toggle_line_scroll;
    ToggleButton toggle_smart_render;

    // General new toggles
    ToggleButton toggle_word_hints;
    ToggleButton toggle_inline_hints;
    ToggleButton toggle_double_click_word;

    // ............ Nav animation state (fade cross-fade active indicator) ............
    double nav_item_alpha[6] = {1,0,0,0,0,0}; // per-item active fill alpha
    double nav_hover_a[6] = {0,0,0,0,0,0};  // per-item hover glow 0..1
    bool   nav_bar_init   = false; // first frame flag


    // Custom theme color picker state
    int  custom_picker_field = -1;   // which field is being edited (-1=none)
    double custom_picker_hue = 0.0;
    double custom_picker_sat = 0.8;
    double custom_picker_val = 0.8;
    // Picker rect cache (set during draw, used for hit-test)
    double cp_px=0,cp_py=0,cp_pw=0,cp_ph=0; // SV square
    double cp_hx=0,cp_hy=0,cp_hw=0,cp_hh=0; // Hue bar

    double spacing_anim = 1.0;
    double gutter_anim  = 1.0;
    int mouse_x = 0, mouse_y = 0;

    SettingsTab() {
        toggle_line_numbers.value   = show_line_numbers;
        toggle_splash.value         = show_splash;
        toggle_autocomplete.value   = autocomplete_on;
        toggle_auto_bracket.value   = auto_bracket;
        toggle_animations.value     = !animations_enabled;  // ON = animations disabled
        toggle_rounded.value        = !rounded_corners;     // ON = flat UI
        toggle_text_optimized.value = text_optimized;
        toggle_line_scroll.value    = line_scroll;
        toggle_smart_render.value   = smart_render;
        toggle_word_hints.value          = word_hints_on;
        toggle_inline_hints.value        = inline_hints_on;
        toggle_double_click_word.value   = double_click_word_select;
        spacing_anim = line_spacing;
        gutter_anim  = gutter_density;
        shortcuts.load();
    }

    std::map<std::string,std::string> to_map() const {
        std::map<std::string,std::string> m;
        m["theme"]             = themes[selected_theme_idx].id;
        m["show_line_numbers"] = show_line_numbers ? "1" : "0";
        m["show_splash"]       = show_splash       ? "1" : "0";
        m["autocomplete"]      = autocomplete_on   ? "1" : "0";
        m["auto_bracket"]      = auto_bracket      ? "1" : "0";
        m["animations"]        = animations_enabled ? "1" : "0";
        m["rounded_corners"]   = rounded_corners    ? "1" : "0";
        m["text_optimized"]    = text_optimized     ? "1" : "0";
        m["line_scroll"]       = line_scroll        ? "1" : "0";
        m["smart_render"]      = smart_render       ? "1" : "0";
        m["indent_size"]       = std::to_string(indent_size);
        m["indent_size_anim"]  = std::to_string(indent_size);
        m["gutter_density"]    = std::to_string(gutter_density);
        m["line_spacing"]      = std::to_string(line_spacing);
        m["font_family"]       = font_families[font_family_idx];
        m["font_size"]         = std::to_string(font_size_pt);
        m["word_hints"]              = word_hints_on            ? "1" : "0";
        m["inline_hints"]            = inline_hints_on          ? "1" : "0";
        m["double_click_word_select"]= double_click_word_select ? "1" : "0";
        m["fps_max"]           = std::to_string(fps_max);
        m["fps_avg"]           = std::to_string(fps_avg);
        m["fps_min"]           = std::to_string(fps_min);
        return m;
    }

    void load_from_map(const std::map<std::string,std::string> &m) {
        if (m.count("theme")) for (int i=0;i<(int)themes.size();++i)
            if (themes[i].id==m.at("theme")) selected_theme_idx=i;
        if (m.count("show_line_numbers")) show_line_numbers=(m.at("show_line_numbers")=="1");
        if (m.count("show_splash"))       show_splash      =(m.at("show_splash")=="1");
        if (m.count("autocomplete"))      autocomplete_on  =(m.at("autocomplete")=="1");
        if (m.count("animations"))        animations_enabled=(m.at("animations")=="1");
        if (m.count("rounded_corners"))   rounded_corners  =(m.at("rounded_corners")=="1");
        if (m.count("text_optimized"))    text_optimized   =(m.at("text_optimized")=="1");
        if (m.count("line_scroll"))       line_scroll      =(m.at("line_scroll")=="1");
        if (m.count("smart_render"))      smart_render     =(m.at("smart_render")=="1");
        if (m.count("word_hints"))               word_hints_on           =(m.at("word_hints")=="1");
        if (m.count("inline_hints"))             inline_hints_on         =(m.at("inline_hints")=="1");
        if (m.count("double_click_word_select")) double_click_word_select=(m.at("double_click_word_select")=="1");
        if (m.count("fps_max"))           fps_max    = std::stoi(m.at("fps_max"));
        if (m.count("fps_avg"))           fps_avg    = std::stoi(m.at("fps_avg"));
        if (m.count("fps_min"))           fps_min    = std::clamp(std::stoi(m.at("fps_min")), 5, 120);
        if (m.count("indent_size"))    try{indent_size   =std::clamp(std::stoi(m.at("indent_size")),1,8);}catch(...){}
        indent_size_anim = indent_size;
        if (m.count("gutter_density")) try{gutter_density=std::stod(m.at("gutter_density")); gutter_anim=gutter_density;}catch(...){}
        if (m.count("line_spacing"))   try{line_spacing  =std::stod(m.at("line_spacing"));   spacing_anim=line_spacing;}catch(...){}
        if (m.count("font_family")) {
            const std::string &ff=m.at("font_family");
            for (int i=0;i<(int)font_families.size();++i)
                if (font_families[i]==ff){font_family_idx=i;break;}
        }
        if (m.count("font_size")) try{
            font_size_pt=std::clamp(std::stoi(m.at("font_size")),8,24);
            font_size_anim=font_size_pt;
        }catch(...){}
        if (m.count("auto_bracket")) auto_bracket=(m.at("auto_bracket")=="1");
        toggle_line_numbers.value   = show_line_numbers;
        toggle_splash.value         = show_splash;
        toggle_autocomplete.value   = autocomplete_on;
        toggle_auto_bracket.value   = auto_bracket;
        toggle_animations.value     = !animations_enabled; // inverted: ON = no anim
        toggle_rounded.value        = !rounded_corners;    // inverted: ON = flat UI
        toggle_text_optimized.value = text_optimized;
        toggle_line_scroll.value    = line_scroll;
        toggle_smart_render.value   = smart_render;
    }

    // Control column = 46% of content width, min 300px
    static int ctrl_col(int cw)  { return std::max(300, (int)(cw * 0.46)); }
    // Slider width = 42% of content width, min 200, max 300
    static int ctrl_slw(int cw)  { return std::clamp((int)(cw*0.42), 200, 300); }

    // .................................................................
    //  Input
    // .................................................................
    void handle_mouse_move(int mx, int my, int, int, int) {
        mouse_x = mx; mouse_y = my;
        show_onboarding_link_hovered =
            mx >= show_onboarding_link_x && mx <= show_onboarding_link_x + show_onboarding_link_w &&
            my >= show_onboarding_link_y && my <= show_onboarding_link_y + show_onboarding_link_h;
        if (dragging_settings_scroll) { update_scroll_drag(my); return; }
        if (dragging_spacing)   { update_spacing_from_mouse(mx);   return; }
        if (dragging_gutter)    { update_gutter_from_mouse(mx);    return; }
        if (dragging_font_size) { update_font_size_from_mouse(mx); return; }
        if (dragging_indent)    { update_indent_from_mouse(mx);    return; }
        if (dragging_fps_max)   { fps_max = 15 + (int)std::round(std::clamp((mx-slider_base_x)/slider_base_w,0.0,1.0)*225); fps_max_anim = fps_max_anim*0.85+fps_max*0.15; return; }
        if (dragging_fps_avg)   { fps_avg = 15 + (int)std::round(std::clamp((mx-slider_base_x)/slider_base_w,0.0,1.0)*225); fps_avg_anim = fps_avg_anim*0.85+fps_avg*0.15; return; }
        if (dragging_fps_min)   { fps_min =  5 + (int)std::round(std::clamp((mx-slider_base_x)/slider_base_w,0.0,1.0)*115); fps_min_anim = fps_min_anim*0.85+fps_min*0.15; return; }
        // Hover states
        toggle_line_numbers.hovered   = toggle_line_numbers.is_hit(mx,my);
        toggle_splash.hovered         = toggle_splash.is_hit(mx,my);
        toggle_autocomplete.hovered   = toggle_autocomplete.is_hit(mx,my);
        toggle_auto_bracket.hovered   = toggle_auto_bracket.is_hit(mx,my);
        toggle_word_hints.hovered         = toggle_word_hints.is_hit(mx,my);
        toggle_inline_hints.hovered       = toggle_inline_hints.is_hit(mx,my);
        toggle_double_click_word.hovered  = toggle_double_click_word.is_hit(mx,my);
        toggle_animations.hovered     = toggle_animations.is_hit(mx,my);
        toggle_rounded.hovered        = toggle_rounded.is_hit(mx,my);
        toggle_text_optimized.hovered = toggle_text_optimized.is_hit(mx,my);
        toggle_line_scroll.hovered    = toggle_line_scroll.is_hit(mx,my);
        toggle_smart_render.hovered   = toggle_smart_render.is_hit(mx,my);

        // If the mouse moves beyond toggle while it is pressed, remove pressed
        // (drag cancellation is standard button behavior)
        #define CLEAR_PRESS_IF_AWAY(t) if (t.pressed && !t.is_hit(mx,my)) t.pressed=false
        CLEAR_PRESS_IF_AWAY(toggle_line_numbers);
        CLEAR_PRESS_IF_AWAY(toggle_splash);
        CLEAR_PRESS_IF_AWAY(toggle_autocomplete);
        CLEAR_PRESS_IF_AWAY(toggle_auto_bracket);
        CLEAR_PRESS_IF_AWAY(toggle_word_hints);
        CLEAR_PRESS_IF_AWAY(toggle_inline_hints);
        CLEAR_PRESS_IF_AWAY(toggle_double_click_word);
        CLEAR_PRESS_IF_AWAY(toggle_animations);
        CLEAR_PRESS_IF_AWAY(toggle_rounded);
        CLEAR_PRESS_IF_AWAY(toggle_text_optimized);
        CLEAR_PRESS_IF_AWAY(toggle_line_scroll);
        CLEAR_PRESS_IF_AWAY(toggle_smart_render);
        #undef CLEAR_PRESS_IF_AWAY
    }

    void handle_mouse_up() {
        dragging_settings_scroll = false;
        dragging_spacing = dragging_gutter = dragging_font_size = dragging_indent = false;
        dragging_fps_max = dragging_fps_avg = dragging_fps_min = false;
        // Clamp FPS: ensure min <= avg <= max
        fps_min = std::min(fps_min, fps_avg);
        fps_avg = std::clamp(fps_avg, fps_min, fps_max);
        // Release pressed state on all toggles
        toggle_line_numbers.pressed = toggle_splash.pressed = toggle_autocomplete.pressed = false;
        toggle_auto_bracket.pressed = toggle_word_hints.pressed = toggle_inline_hints.pressed = toggle_double_click_word.pressed = false;
        toggle_animations.pressed   = toggle_rounded.pressed = toggle_text_optimized.pressed = false;
        toggle_line_scroll.pressed  = toggle_smart_render.pressed = false;
        scroll_pressed = false;
    }

    bool handle_mouse_down(int mx, int my, int area_x, int area_y, int area_w, int area_h,
                            const ThemeVariant &tv)
    {
        // NAV_HEADER_H=52 + gap=6 -> первый item = area_y+58, шаг=48 (44+4)
        for (int i = 0; i < 6; ++i) {
            double ny = area_y + 58.0 + i * 48.0;
            if (mx >= area_x + 8 && mx < area_x + NAV_W - 8
                && my >= (int)ny && my < (int)(ny + 44.0)) {
                icon_anim::trigger(&nav_hover_a[i]);
                current_section=i; editing_key_idx=-1; waiting_for_key=false; return true;
            }
        }

        int cx=area_x+NAV_W+1, cy=area_y, cw=area_w-NAV_W-1;
        const int PAD = 20;

        if (current_section==SEC_GENERAL) {
            if (show_onboarding_link_hovered &&
                mx >= show_onboarding_link_x && mx <= show_onboarding_link_x + show_onboarding_link_w &&
                my >= show_onboarding_link_y && my <= show_onboarding_link_y + show_onboarding_link_h) {
                if (on_show_onboarding) on_show_onboarding();
                return true;
            }
            // Use cached geometry set by draw_general each frame - exact match guaranteed
            double sl_x = cached_sl_x_g;
            double sl_w = cached_sl_w_g;
            // Slider thumb y-positions cached by draw_general
            double r0 = cached_font_size_y;    // Font Size thumb y
            double r1 = cached_font_family_y;  // Font Family buttons y (top of button)
            double r2 = cached_spacing_y;      // Line Spacing thumb y
            double r3 = cached_indent_y;       // Indent thumb y
            double rg = cached_gutter_y;       // Gutter Width thumb y
            if (mx>=sl_x && mx<=sl_x+sl_w && my>=r0-18 && my<=r0+18)
                { dragging_font_size=true; slider_base_x=sl_x; slider_base_w=sl_w; update_font_size_from_mouse(mx); return true; }
            double btn_sz = 24, pill_w = sl_w - btn_sz*2 - 10;
            if (my >= r1 && my <= r1 + btn_sz) {
                if (mx >= sl_x && mx < sl_x + btn_sz)
                    { font_family_idx=(font_family_idx-1+(int)font_families.size())%(int)font_families.size(); return true; }
                double rx2 = sl_x + btn_sz + 5 + pill_w + 5;
                if (mx >= rx2 && mx < rx2 + btn_sz)
                    { font_family_idx=(font_family_idx+1)%(int)font_families.size(); return true; }
            }
            if (mx>=sl_x && mx<=sl_x+sl_w && my>=r2-18 && my<=r2+18)
                { dragging_spacing=true; slider_base_x=sl_x; slider_base_w=sl_w; update_spacing_from_mouse(mx); return true; }
            if (mx>=sl_x && mx<=sl_x+sl_w && my>=r3-18 && my<=r3+18)
                { dragging_indent=true; slider_base_x=sl_x; slider_base_w=sl_w; update_indent_from_mouse(mx); return true; }
            if (mx>=sl_x && mx<=sl_x+sl_w && my>=rg-18 && my<=rg+18)
                { dragging_gutter=true; slider_base_x=sl_x; slider_base_w=sl_w; update_gutter_from_mouse(mx); return true; }
            if (toggle_line_numbers.is_hit(mx,my))  { toggle_line_numbers.pressed=true;  toggle_line_numbers.toggle();  show_line_numbers=toggle_line_numbers.value; return true; }
            if (toggle_splash.is_hit(mx,my))         { toggle_splash.pressed=true;        toggle_splash.toggle();        show_splash=toggle_splash.value;             return true; }
            if (toggle_autocomplete.is_hit(mx,my))   { toggle_autocomplete.pressed=true;  toggle_autocomplete.toggle();  autocomplete_on=toggle_autocomplete.value;   return true; }
            if (toggle_auto_bracket.is_hit(mx,my))   { toggle_auto_bracket.pressed=true;  toggle_auto_bracket.toggle();  auto_bracket=toggle_auto_bracket.value;      return true; }
            if (toggle_word_hints.is_hit(mx,my))     { toggle_word_hints.pressed=true;    toggle_word_hints.toggle();    word_hints_on=toggle_word_hints.value;       return true; }
            if (toggle_inline_hints.is_hit(mx,my))   { toggle_inline_hints.pressed=true;  toggle_inline_hints.toggle();  inline_hints_on=toggle_inline_hints.value;   return true; }
            if (toggle_double_click_word.is_hit(mx,my)) { toggle_double_click_word.pressed=true; toggle_double_click_word.toggle(); double_click_word_select=toggle_double_click_word.value; return true; }
        }

        if (current_section==SEC_PERF) {
            // Use cached geometry set by draw_perf each frame - exact match guaranteed
            double sl_x = cached_sl_x_p;
            double sl_w = cached_sl_w_p;
            double rf0 = cached_fps_max_y;
            double rf1 = cached_fps_avg_y;
            double rf2 = cached_fps_min_y;
            if (mx>=sl_x && mx<=sl_x+sl_w && my>=rf0-18 && my<=rf0+18)
                { dragging_fps_max=true; slider_base_x=sl_x; slider_base_w=sl_w; return true; }
            if (mx>=sl_x && mx<=sl_x+sl_w && my>=rf1-18 && my<=rf1+18)
                { dragging_fps_avg=true; slider_base_x=sl_x; slider_base_w=sl_w; return true; }
            if (mx>=sl_x && mx<=sl_x+sl_w && my>=rf2-18 && my<=rf2+18)
                { dragging_fps_min=true; slider_base_x=sl_x; slider_base_w=sl_w; return true; }
            if (toggle_animations.is_hit(mx,my)) {
                toggle_animations.pressed=true; toggle_animations.toggle();
                animations_enabled=!toggle_animations.value; g_animations_enabled=animations_enabled; return true; }
            if (toggle_rounded.is_hit(mx,my)) {
                toggle_rounded.pressed=true; toggle_rounded.toggle();
                rounded_corners=!toggle_rounded.value; g_rounded_corners=rounded_corners; return true; }
            if (toggle_text_optimized.is_hit(mx,my)) { toggle_text_optimized.pressed=true; toggle_text_optimized.toggle(); text_optimized=toggle_text_optimized.value;  g_text_optimized=text_optimized; return true; }
            if (toggle_line_scroll.is_hit(mx,my))    { toggle_line_scroll.pressed=true;    toggle_line_scroll.toggle();    line_scroll=toggle_line_scroll.value;        g_line_scroll=line_scroll;       return true; }
            if (toggle_smart_render.is_hit(mx,my))   { toggle_smart_render.pressed=true;   toggle_smart_render.toggle();   smart_render=toggle_smart_render.value;      g_smart_render=smart_render;     return true; }
        }

        if (current_section==SEC_THEMES) {
            // Grid layout click detection
            const int COLS   = 3;
            const int CARD_H = 130;
            const int CARD_GAP  = 18;
            const int HEADER_H  = 52;
            const int SIDE_PAD  = 16;
            int card_w = (cw - SIDE_PAD * 2 - CARD_GAP * (COLS - 1)) / COLS;
            if (card_w < 80) card_w = 80;
            int n = (int)themes.size();
            for (int i = 0; i < n; i++) {
                int col = i % COLS;
                int row = i / COLS;
                double card_x = cx + SIDE_PAD + col * (card_w + CARD_GAP);
                double card_y = area_y + HEADER_H + row * (CARD_H + CARD_GAP)
                                - themes_scroll;
                // Hit card + name label below
                if (mx >= (int)card_x && mx <= (int)(card_x + card_w)
                    && my >= (int)card_y && my <= (int)(card_y + CARD_H + 18)) {
                    selected_theme_idx = i;
                    if (themes[i].id != "custom") custom_picker_field = -1;
                    return true;
                }
            }
        }

        if (current_section==SEC_KEYS) {
            const int ROW_H=44; int row_y=cy+80;
            const int pad=36;   // padding constant (was missing in this scope)
            double rb_x=cx+cw-pad-110, rb_y=cy+28;
            if (mx>=rb_x && mx<rb_x+110 && my>=rb_y && my<rb_y+32) {
                shortcuts.defaults(); shortcuts.save(); editing_key_idx=-1; waiting_for_key=false; return true;
            }
            // Only respond to clicks within the content column
            if (mx >= cx && mx < cx+cw) {
                for (int i=0;i<(int)shortcuts.shortcuts.size();++i) {
                    double ry2=row_y+i*ROW_H-keys_scroll;
                    if (ry2<cy+70 || ry2>cy+area_h-20) continue;
                    if (my>=ry2 && my<ry2+ROW_H) {
                        if (editing_key_idx==i) { editing_key_idx=-1; waiting_for_key=false; }
                        else { editing_key_idx=i; waiting_for_key=true; }
                        return true;
                    }
                }
                // Click on empty space in content area cancels editing
                editing_key_idx=-1; waiting_for_key=false;
            }
        }

        (void)tv;
        // Fallback: did the click land on the scrollbar thumb for whichever
        // section is currently showing one? (cached by scrollbar() during
        // the last draw() call.) This is what makes the settings scrollbar
        // actually draggable, matching the code editor's scrollbar.
        if (try_start_scroll_drag(mx, my)) return false; // no "setting changed", just started a drag
        return false;
    }


    bool handle_key_for_rebind(XKeyEvent *kev);

    void handle_scroll(int delta, int, int, int, int area_h) {
        if (current_section==SEC_THEMES)
            themes_scroll=std::clamp(themes_scroll+delta,0.0,std::max(0.0,themes_total_height-themes_view_height));
        if (current_section==SEC_HELP)
            help_scroll=std::clamp(help_scroll+delta,0.0,std::max(0.0,help_total_height-(double)(area_h-72)));
        if (current_section==SEC_KEYS)
            keys_scroll=std::clamp(keys_scroll+delta,0.0,std::max(0.0,keys_total_height-(double)(area_h-80)));
        if (current_section==SEC_GENERAL)
            general_scroll=std::clamp(general_scroll+delta,0.0,std::max(0.0,general_total_height-(double)(area_h-64)));
        if (current_section==SEC_PERF)
            perf_scroll=std::clamp(perf_scroll+delta,0.0,std::max(0.0,perf_total_height-(double)(area_h-64)));
    }

    void update_hover(int mx, int my, int area_x, int area_y, int area_w) {
        hovered_section=-1;
        // Same coordinates as in handle_mouse_down
        for (int i = 0; i < 6; ++i) {
            double ny = area_y + 58.0 + i * 48.0;
            if (mx >= area_x + 8 && mx < area_x + NAV_W - 8
                && my >= (int)ny && my < (int)(ny + 44.0))
                hovered_section = i;
        }
        hovered_key_idx=-1;
        if (current_section==SEC_KEYS) {
            int cx2=area_x+NAV_W+1;
            int cw2=area_w-NAV_W-1;
            // Only highlight if mouse is actually inside the content column
            if (mx >= cx2 && mx < cx2+cw2) {
                int cy=area_y; const int ROW_H=44;
                for (int i=0;i<(int)shortcuts.shortcuts.size();++i) {
                    double ry2=cy+80+i*ROW_H-keys_scroll;
                    if (my>=ry2 && my<ry2+ROW_H) { hovered_key_idx=i; break; }
                }
            }
        }
        hovered_theme_idx=-1;
        if (current_section==SEC_THEMES) {
            int cx2 = area_x + NAV_W + 1;
            const int COLS   = 3;
            const int CARD_H = 130;
            const int CARD_GAP  = 18;
            const int HEADER_H  = 52;
            const int SIDE_PAD  = 16;
            int card_w = (area_w - NAV_W - SIDE_PAD * 2 - CARD_GAP * (COLS-1)) / COLS;
            if (card_w < 80) card_w = 80;
            for (int i = 0; i < (int)themes.size(); i++) {
                int col = i % COLS;
                int row = i / COLS;
                double card_x = cx2 + SIDE_PAD + col * (card_w + CARD_GAP);
                double card_y = area_y + HEADER_H + row * (CARD_H + CARD_GAP)
                                - themes_scroll;
                if (mx >= (int)card_x && mx <= (int)(card_x + card_w)
                    && my >= (int)card_y && my <= (int)(card_y + CARD_H + 18))
                    hovered_theme_idx = i;
            }
        }
    }

    //  Main draw
    void draw(cairo_t *cr, int area_x, int area_y, int area_w, int area_h,
              const ThemeVariant &tv)
    {
        if (!cr) return;
        cairo_set_source_rgb(cr,tv.bg,tv.bg,tv.bg);
        cairo_rectangle(cr,area_x,area_y,area_w,area_h); cairo_fill(cr);

        PangoLayout *layout=pango_cairo_create_layout(cr);
        std::string fs=tv.font_family+" "+std::to_string(tv.font_size);
        PangoFontDescription *fnt=pango_font_description_from_string(fs.c_str());
        pango_layout_set_font_description(layout,fnt);

        // ............ Navigation sidebar - Navigation Drawer + Mica ............
        const double NAV_ITEM_H   = 44.0;
        const double NAV_ITEM_GAP = 4.0;
        const double NAV_HEADER_H = 52.0; // height of SETTINGS header area
        const double NAV_FIRST_Y = area_y + NAV_HEADER_H + 6.0; // first tab below the header
        const double NAV_ITEM_X   = area_x + 8.0;
        const double NAV_ITEM_W   = NAV_W - 16.0;

        // ............ Nav background - flat surface, no depth gradient ............
        {
            double ng = tv.panel + 0.018;
            cairo_set_source_rgb(cr, ng, ng, ng);
            cairo_rectangle(cr, area_x, area_y, NAV_W, area_h);
            cairo_fill(cr);
        }
        // Right separator - flat hairline
        {
            cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, 0.14);
            cairo_rectangle(cr, area_x + NAV_W - 1, area_y, 1, area_h);
            cairo_fill(cr);
        }
        // ............ "SETTINGS" header at the top of the sidebar ............
        {
        // Accent vertical stripe
            cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.85);
            rrect(cr, area_x + 14, area_y + 16, 3.0, 22.0, 1.5);
            cairo_fill(cr);

            // text "SETTINGS"
            PangoFontDescription *hfnt = pango_font_description_copy(fnt);
            pango_font_description_set_weight(hfnt, PANGO_WEIGHT_BOLD);
            pango_font_description_set_size(hfnt, (int)(tv.font_size * PANGO_SCALE));
            pango_layout_set_font_description(layout, hfnt);
            pango_layout_set_attributes(layout, NULL);
            pango_layout_set_text(layout, "SETTINGS", -1);
            int sh_w, sh_h; pango_layout_get_pixel_size(layout, &sh_w, &sh_h);
            cairo_set_source_rgba(cr, tv.text, tv.text, tv.text, 0.90);
            cairo_move_to(cr, area_x + 24, area_y + (NAV_HEADER_H - sh_h) / 2.0);
            pango_cairo_show_layout(cr, layout);
            pango_font_description_free(hfnt);
            pango_layout_set_font_description(layout, fnt);

            // Separator under the title
            cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, 0.18);
            cairo_set_line_width(cr, 1.0);
            cairo_move_to(cr, area_x + 10, area_y + NAV_HEADER_H);
            cairo_line_to(cr, area_x + NAV_W - 10, area_y + NAV_HEADER_H);
            cairo_stroke(cr);
        }

        // ............ Animated active indicator - fade cross-fade per item ............
        if (!nav_bar_init) {
            for (int i = 0; i < 6; ++i)
                nav_item_alpha[i] = (i == current_section) ? 1.0 : 0.0;
            nav_bar_init = true;
        }
        // Animate alpha per item toward target
        double aspd = g_animations_enabled ? 0.14 : 1.0;
        for (int i = 0; i < 6; ++i) {
            double target_a = (i == current_section) ? 1.0 : 0.0;
            nav_item_alpha[i] += (target_a - nav_item_alpha[i]) * aspd;
        }

        // Draw indicator behind items - squared with r=4
        for (int i = 0; i < 6; ++i) {
            if (nav_item_alpha[i] < 0.005) continue;
            double ny  = NAV_FIRST_Y + i * (NAV_ITEM_H + NAV_ITEM_GAP);
            double bar_h = NAV_ITEM_H - 10;
            double bar_y = ny + 5;
            double rr2   = g_rounded_corners ? 4.0 : 1.0;  // subtle squarish corners

            // Tonal container (active indicator) - flat fill, no edge glow
            double af = tv.accent_r * 0.14 + (tv.panel + 0.018) * 0.86;
            cairo_set_source_rgba(cr, af, af + (tv.accent_g - tv.accent_r) * 0.14,
                                  af + (tv.accent_b - tv.accent_r) * 0.14,
                                  nav_item_alpha[i]);
            rrect(cr, NAV_ITEM_X, bar_y, NAV_ITEM_W, bar_h, rr2);
            cairo_fill(cr);

            // Left accent stripe (3px)
            double pill_h = bar_h * 0.55;
            double pill_y = ny + 5 + (bar_h - pill_h) * 0.5;
            double pill_r = g_rounded_corners ? 1.5 : 0;
            cairo_set_source_rgba(cr,
                tv.accent_r, tv.accent_g, tv.accent_b, nav_item_alpha[i]);
            rrect(cr, NAV_ITEM_X, pill_y, 3.0, pill_h, pill_r);
            cairo_fill(cr);
        }

        // ............ Nav items ............
        const char *labels[6] = {"General","Performance","Themes","Keybindings","Help","About"};

        for (int i = 0; i < 6; ++i) {
            double ny  = NAV_FIRST_Y + i * (NAV_ITEM_H + NAV_ITEM_GAP);
            bool active  = (i == current_section);
            bool hovered = (i == hovered_section);
            // rr2 is the same for hover and active - so that they look identical
            double rr2   = g_rounded_corners ? 4.0 : 1.0;

            // Per-item hover animation
            double h_target = hovered && !active ? 1.0 : 0.0;
            nav_hover_a[i] += (h_target - nav_hover_a[i]) * (g_animations_enabled ? 0.20 : 1.0);

            // Hover overlay - the same pill as active, but with alpha 0.45
            if (nav_hover_a[i] > 0.005) {
                // Tonal container (like active but more transparent)
                double haf = tv.accent_r * 0.14 + (tv.panel + 0.018) * 0.86;
                cairo_set_source_rgba(cr, haf,
                    haf + (tv.accent_g - tv.accent_r) * 0.14,
                    haf + (tv.accent_b - tv.accent_r) * 0.14,
                    0.55 * nav_hover_a[i]);
                rrect(cr, NAV_ITEM_X, ny + 5, NAV_ITEM_W, NAV_ITEM_H - 10, rr2);
                cairo_fill(cr);
                // Left bar (as active)
                double hpill_h = (NAV_ITEM_H - 10) * 0.55;
                double hpill_y = ny + 5 + ((NAV_ITEM_H - 10) - hpill_h) * 0.5;
                cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b,
                                      0.60 * nav_hover_a[i]);
                rrect(cr, NAV_ITEM_X, hpill_y, 3.0, hpill_h,
                      g_rounded_corners ? 1.5 : 0);
                cairo_fill(cr);
            }

            // Icon
            double icon_alpha = active ? 1.0 : (0.55 + 0.35 * nav_hover_a[i]);
            double ir = tv.accent_r * (active ? 1.0 : 0.0) + tv.muted * 1.4 * (active ? 0.0 : 1.0);
            double ig = tv.accent_g * (active ? 1.0 : 0.0) + tv.muted * 1.4 * (active ? 0.0 : 1.0);
            double ib = tv.accent_b * (active ? 1.0 : 0.0) + tv.muted * 1.4 * (active ? 0.0 : 1.0);
            cairo_save(cr);
            cairo_set_source_rgba(cr, ir, ig, ib, icon_alpha);
            // Icon center = bar center
            int icon_bar_y = (int)(ny + 5.0 + (NAV_ITEM_H - 10.0 - 20.0) / 2.0);
            {
                double nic_cx = NAV_ITEM_X + 22 + 10.0;  // draw_nav_icon takes a
                double nic_cy = icon_bar_y + 10.0;       // top-left + size(20)/2
                icon_anim::Scope pop(cr, &nav_hover_a[i], nic_cx, nic_cy);
                draw_nav_icon(cr, i, (int)(NAV_ITEM_X + 22), icon_bar_y, 20, ir, ig, ib);
            }
            cairo_restore(cr);

            // Label
            double label_alpha = active ? 1.0 : (0.55 + 0.35 * nav_hover_a[i]);
            pango_font_description_set_weight(fnt, active ? PANGO_WEIGHT_SEMIBOLD : PANGO_WEIGHT_NORMAL);
            pango_layout_set_font_description(layout, fnt);
            pango_layout_set_text(layout, labels[i], -1);
            pango_layout_set_width(layout, (int)(NAV_ITEM_W - 56) * PANGO_SCALE);
            pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
            int lw2, lh2; pango_layout_get_pixel_size(layout, &lw2, &lh2);
            double label_r = active ? tv.text : tv.text;
            cairo_set_source_rgba(cr, label_r, label_r, label_r, label_alpha);
            // Text center = center bar (ny+5 .. ny+5+(NAV_ITEM_H-10))
            double bar_center_y = ny + 5.0 + (NAV_ITEM_H - 10.0 - lh2) / 2.0;
            cairo_move_to(cr, NAV_ITEM_X + 50, bar_center_y);
            pango_cairo_show_layout(cr, layout);
            pango_font_description_set_weight(fnt, PANGO_WEIGHT_NORMAL);
            pango_layout_set_width(layout, -1);
            pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_NONE);
        }

        int cx=area_x+NAV_W+1, cw=area_w-NAV_W-1;
        cairo_save(cr);
        cairo_rectangle(cr,cx,area_y,cw,area_h); cairo_clip(cr);
        switch(current_section){
            case SEC_GENERAL: draw_general(cr,layout,fnt,cx,area_y,cw,area_h,tv); break;
            case SEC_PERF:    draw_perf   (cr,layout,fnt,cx,area_y,cw,area_h,tv); break;
            case SEC_THEMES:  draw_themes (cr,layout,fnt,cx,area_y,cw,area_h,area_x,area_w,tv); break;
            case SEC_KEYS:    draw_keys   (cr,layout,fnt,cx,area_y,cw,area_h,tv); break;
            case SEC_HELP:    draw_help   (cr,layout,fnt,cx,area_y,cw,area_h,tv); break;
            case SEC_ABOUT:   draw_about  (cr,layout,fnt,cx,area_y,cw,area_h,tv); break;
        }
        cairo_restore(cr);
        pango_font_description_free(fnt);
        g_object_unref(layout);
    }

private:
    static void rrect(cairo_t *cr,double x,double y,double w,double h,double r){
        shape::rrect(cr,x,y,w,h,r);
    }

    // Icon of a settings section: one SVG file per section (assets/icons/ui)
    static void draw_nav_icon(cairo_t *cr,int id,double ox,double oy,double size,double r,double g,double b){
        static const char *svg_names[7]={"general","performance","themes","keybindings","help","about","plugin"};
        if(id<0||id>=7) return;
        svg_icons::draw_ui(cr,svg_names[id],ox+size*0.075,oy+size*0.075,size*0.85,r,g,b,1.0);
    }

    // Display title for settings sections
    void draw_heading(cairo_t *cr, PangoLayout *layout, PangoFontDescription *fnt,
                      const char *title, int cx, int cy, const ThemeVariant &tv);

    // divider - hairline, fades at edges (card separator style)
    void div(cairo_t *cr,int x,int y,int w,const ThemeVariant &tv){
        cairo_pattern_t *pg = cairo_pattern_create_linear(x, y, x+w, y);
        cairo_pattern_add_color_stop_rgba(pg, 0.0, tv.muted, tv.muted, tv.muted, 0.0);
        cairo_pattern_add_color_stop_rgba(pg, 0.1, tv.muted, tv.muted, tv.muted, 0.10);
        cairo_pattern_add_color_stop_rgba(pg, 0.9, tv.muted, tv.muted, tv.muted, 0.10);
        cairo_pattern_add_color_stop_rgba(pg, 1.0, tv.muted, tv.muted, tv.muted, 0.0);
        cairo_set_source(cr, pg);
        cairo_rectangle(cr, x, y, w, 1);
        cairo_fill(cr);
        cairo_pattern_destroy(pg);
    }

    void row_label(cairo_t *cr,PangoLayout *layout,PangoFontDescription *fnt,
                    const char *label,const char *desc,int cx,int cy,int max_w,const ThemeVariant &tv);

// ............ Slider + track style ............
    // Track: rounded pill, inactive part muted, active part accent-filled
    // Thumb: elevated disc with highlight streak, 20px diameter
    // Value label: accent colored, medium weight
    void slider(cairo_t *cr,PangoLayout *layout,PangoFontDescription *fnt,
                double sl_x,double sl_y,double sl_w,
                double anim_val,double min_v,double max_v,
                const char *val_str,const ThemeVariant &tv,
                double drag_anim=0.0);

    void theme_preview(cairo_t *cr,double x,double y,double w,double h,const ThemeVariant &t){
        cairo_set_source_rgb(cr,t.bg,t.bg,t.bg);
        rrect(cr,x,y,w,h,6); cairo_fill(cr);
        double gut=w*0.14;
        cairo_set_source_rgb(cr,t.gutter_bg,t.gutter_bg,t.gutter_bg);
        cairo_rectangle(cr,x,y,gut,h); cairo_fill(cr);
        double cx2=x+gut+8,cy2=y+10,lh=(h-20)/5.0;
        struct L{double ox,len,r,g,b,a;};
        L lines[5]={{0,w*0.40,t.keyword_r,t.keyword_g,t.keyword_b,0.9},
                    {12,w*0.55,t.text,t.text,t.text,0.7},
                    {20,w*0.38,t.string_r,t.string_g,t.string_b,0.8},
                    {12,w*0.50,t.function_r,t.function_g,t.function_b,0.75},
                    {0,w*0.28,t.comment_r,t.comment_g,t.comment_b,0.6}};
        for(int i=0;i<5;++i){
            cairo_set_source_rgba(cr,lines[i].r,lines[i].g,lines[i].b,lines[i].a);
            rrect(cr,cx2+lines[i].ox,cy2+i*lh,std::max(4.0,lines[i].len-gut-16),lh*0.55,2);
            cairo_fill(cr);
        }
    }
    // mini scrollbar - blank, press animation
    // press_anim: 0.0=normal, 1.0=pressed (thumb decreases)
    void scrollbar(cairo_t *cr, double track_x, double track_y, double track_h,
                   double scroll, double total, const ThemeVariant &tv,
                   double press_anim = 0.0, double *bind = nullptr)
    {
        // Cache geometry for drag hit-testing (see try_start_scroll_drag below).
        scroll_track_x = track_x; scroll_track_y = track_y; scroll_track_h = track_h;
        scroll_total_h = total;   scroll_bound   = bind;

        double ratio   = std::clamp(track_h / total, 0.05, 1.0);
        double thumb_h = std::max(28.0, track_h * ratio);
        double max_s   = std::max(1.0, total - track_h);
        double tby     = track_y + (scroll / max_s) * (track_h - thumb_h);

        // When you press thumb, it shrinks in width (press feedback)
        double base_w = 5.0;
        double press_w = 3.0; // compressed width
        double track_w = base_w - (base_w - press_w) * press_anim;
        double offset_x = (base_w - track_w) / 2.0; // center by track

        // Track background (thin line, no rounding)
        cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, 0.10);
        cairo_rectangle(cr, track_x + offset_x, track_y, track_w, track_h);
        cairo_fill(cr);

        // Thumb - a clean rectangle, no roundness, no point
        // When press: slightly smaller in height (+/-4px)
        double press_shrink = 4.0 * press_anim;
        double draw_tby = tby + press_shrink * 0.5;
        double draw_tbh = thumb_h - press_shrink;

                // Thumb fill - accent, without rounding
        double thumb_alpha = 0.65 + 0.25 * (1.0 - press_anim);  // darker when pressed
        cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, thumb_alpha);
        rrect(cr, track_x + offset_x, draw_tby, track_w, draw_tbh, shape::R);
        cairo_fill(cr);
    }

    // Hit-tests the mouse against the scrollbar thumb last drawn this frame
    // (with a few px of horizontal slop, same idea as the code editor's
    // scrollbar hit-test). Returns true and begins a drag if hit.
    bool try_start_scroll_drag(int mx, int my) {
        if (!scroll_bound || scroll_total_h <= scroll_track_h) return false;
        double ratio   = std::clamp(scroll_track_h / scroll_total_h, 0.05, 1.0);
        double thumb_h = std::max(28.0, scroll_track_h * ratio);
        double max_s   = std::max(1.0, scroll_total_h - scroll_track_h);
        double tby     = scroll_track_y + (*scroll_bound / max_s) * (scroll_track_h - thumb_h);
        if (mx >= scroll_track_x - 5 && mx <= scroll_track_x + 9 &&
            my >= tby && my <= tby + thumb_h) {
            dragging_settings_scroll = true;
            drag_start_my_settings   = my;
            drag_start_val_settings  = *scroll_bound;
            scroll_pressed = true;
            return true;
        }
        return false;
    }
    void update_scroll_drag(int my) {
        if (!dragging_settings_scroll || !scroll_bound) return;
        double ratio   = std::clamp(scroll_track_h / scroll_total_h, 0.05, 1.0);
        double thumb_h = std::max(28.0, scroll_track_h * ratio);
        double max_s   = std::max(1.0, scroll_total_h - scroll_track_h);
        double movable = scroll_track_h - thumb_h;
        if (movable > 0.0) {
            double ds = (my - drag_start_my_settings) * (max_s / movable);
            *scroll_bound = std::clamp(drag_start_val_settings + ds, 0.0, max_s);
        }
    }
    void end_scroll_drag() {
        dragging_settings_scroll = false;
        scroll_pressed = false;
    }

    //  General section - redesigned
    void draw_general(cairo_t *cr, PangoLayout *layout, PangoFontDescription *fnt,
                      int cx, int cy, int cw, int ch, const ThemeVariant &tv);
    void update_font_size_from_mouse(int mx){
        font_size_pt=8+(int)std::round(std::clamp((mx-slider_base_x)/slider_base_w,0.0,1.0)*16.0);
        font_size_anim=font_size_anim*0.85+font_size_pt*0.15;
    }

    void update_indent_from_mouse(int mx){
        indent_size=1+(int)std::round(std::clamp((mx-slider_base_x)/slider_base_w,0.0,1.0)*7.0);
        indent_size_anim=indent_size_anim*0.85+indent_size*0.15;
    }

    //  Performance section - redesigned with FPS sliders
    void draw_perf(cairo_t *cr, PangoLayout *layout, PangoFontDescription *fnt,
                   int cx, int cy, int cw, int ch, const ThemeVariant &tv);
    void draw_themes(cairo_t *cr,PangoLayout *layout,PangoFontDescription *fnt,
                      int cx,int area_y,int cw,int ch,int ax,int aw,
                      const ThemeVariant &tv);


    //  Keybindings section

    // ............ HSV<->RGB helpers (for custom theme picker) ............
    static void hsv_to_rgb(double h,double s,double v,double &r,double &g,double &b){
        h=std::fmod(h,1.0)*6.0; int i=(int)h; double f=h-i;
        double p=v*(1-s),q=v*(1-s*f),t=v*(1-s*(1-f));
        switch(i%6){case 0:r=v;g=t;b=p;break;case 1:r=q;g=v;b=p;break;
                     case 2:r=p;g=v;b=t;break;case 3:r=p;g=q;b=v;break;
                     case 4:r=t;g=p;b=v;break;default:r=v;g=p;b=q;break;}
    }
    static void rgb_to_hsv(double r,double g,double b,double &h,double &s,double &v){
        double mx=std::max({r,g,b}),mn=std::min({r,g,b}),d=mx-mn;
        v=mx; s=(mx>0)?d/mx:0;
        if(d<1e-9){h=0;return;}
        if(mx==r)h=(g-b)/d+(g<b?6:0);
        else if(mx==g)h=(b-r)/d+2; else h=(r-g)/d+4;
        h/=6.0;
    }

    // Draw a colour picker (SV square + Hue bar) for the custom theme
    void draw_custom_picker(cairo_t *cr,PangoLayout *layout,PangoFontDescription *fnt,
                            double px,double py,double pw,double ph,
                            const ThemeVariant &tv);

    // Handle mouse click inside custom picker - uses cached cp_* rects set during draw
    bool handle_custom_picker_click(int mx,int my,int cx,int area_y,int cw,int /*ch*/);

    void draw_keys(cairo_t *cr,PangoLayout *layout,PangoFontDescription *fnt,
                   int cx,int cy,int cw,int ch,const ThemeVariant &tv);

    // .................................................................
    //  Help section
    // .................................................................
    void draw_help(cairo_t *cr,PangoLayout *layout,PangoFontDescription *fnt,
                    int cx,int cy,int cw,int ch,const ThemeVariant &tv);

    // .................................................................
    //  About section
    // .................................................................
    void draw_about(cairo_t *cr,PangoLayout *layout,PangoFontDescription *fnt,
                     int cx,int cy,int cw,int ch,const ThemeVariant &tv);


    // .................................................................
    //  Addons section
    // .................................................................

    // Small-caps section label
    void update_spacing_from_mouse(int mx){ line_spacing=0.5+std::clamp((mx-slider_base_x)/slider_base_w,0.0,1.0)*1.5; }
    void update_gutter_from_mouse(int mx) { gutter_density=0.5+std::clamp((mx-slider_base_x)/slider_base_w,0.0,1.0)*1.5; }
};

// Out-of-line method definitions, split by section for readability - see
// each file's own header comment. Must come after the class body above,
// since they define SettingsTab::method(...) against it.
#include "settings/settings_shared.hxx"
#include "settings/settings_general.hxx"
#include "settings/settings_perf.hxx"
#include "settings/settings_themes.hxx"
#include "settings/settings_keys.hxx"
#include "settings/settings_help_about.hxx"

#endif
