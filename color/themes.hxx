#ifndef THEMES_HXX
#define THEMES_HXX

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cmath>
#include <cctype>

struct ThemeVariant {
    std::string id, name;
    bool is_light = false;

    double bg=0.05, surface=0.08, panel=0.10, elevated=0.12;
    double text=0.95, muted=0.6;
    double gutter_bg=0.06, gutter_text=0.45;
    short  base_gutter_width=50;   // pixel width, always small - narrowed from int
    double icon_r=0.95, icon_g=0.95, icon_b=0.95;
    double accent_r=0.2,  accent_g=0.5,  accent_b=1.0;
    double accent2_r=0.8, accent2_g=0.45,accent2_b=0.6;
    double success_r=0.18,success_g=0.8, success_b=0.4;
    double warning_r=0.95,warning_g=0.7, warning_b=0.15;
    double error_r=0.9,   error_g=0.2,   error_b=0.3;
    double keyword_r=0.7,  keyword_g=0.4,  keyword_b=0.8;
    double function_r=0.4, function_g=0.6, function_b=0.9;
    double string_r=0.6,   string_g=0.8,   string_b=0.4;
    double comment_r=0.4,  comment_g=0.4,  comment_b=0.4;
    double cursor_r=0.2,    cursor_g=0.6,    cursor_b=1.0;
    double selection_r=0.2, selection_g=0.4, selection_b=0.8;
    double line_highlight=0.02;
    std::string font_family="JetBrains Mono";
    short font_size=12, line_height=20, indent_size=4; // small bounded UI values - narrowed from int

    std::map<std::string,std::string> to_map() const {
        std::map<std::string,std::string> m;
        m["id"]=id; m["name"]=name; m["is_light"]=is_light?"1":"0";
        m["bg"]=std::to_string(bg); m["surface"]=std::to_string(surface);
        m["panel"]=std::to_string(panel); m["elevated"]=std::to_string(elevated);
        m["text"]=std::to_string(text); m["muted"]=std::to_string(muted);
        m["gutter_bg"]=std::to_string(gutter_bg); m["gutter_text"]=std::to_string(gutter_text);
        m["base_gutter_width"]=std::to_string(base_gutter_width);
        m["icon_r"]=std::to_string(icon_r); m["icon_g"]=std::to_string(icon_g); m["icon_b"]=std::to_string(icon_b);
        m["accent_r"]=std::to_string(accent_r); m["accent_g"]=std::to_string(accent_g); m["accent_b"]=std::to_string(accent_b);
        m["accent2_r"]=std::to_string(accent2_r); m["accent2_g"]=std::to_string(accent2_g); m["accent2_b"]=std::to_string(accent2_b);
        m["success_r"]=std::to_string(success_r); m["success_g"]=std::to_string(success_g); m["success_b"]=std::to_string(success_b);
        m["warning_r"]=std::to_string(warning_r); m["warning_g"]=std::to_string(warning_g); m["warning_b"]=std::to_string(warning_b);
        m["error_r"]=std::to_string(error_r); m["error_g"]=std::to_string(error_g); m["error_b"]=std::to_string(error_b);
        m["kw_r"]=std::to_string(keyword_r); m["kw_g"]=std::to_string(keyword_g); m["kw_b"]=std::to_string(keyword_b);
        m["fn_r"]=std::to_string(function_r); m["fn_g"]=std::to_string(function_g); m["fn_b"]=std::to_string(function_b);
        m["str_r"]=std::to_string(string_r); m["str_g"]=std::to_string(string_g); m["str_b"]=std::to_string(string_b);
        m["cmt_r"]=std::to_string(comment_r); m["cmt_g"]=std::to_string(comment_g); m["cmt_b"]=std::to_string(comment_b);
        m["cursor_r"]=std::to_string(cursor_r); m["cursor_g"]=std::to_string(cursor_g); m["cursor_b"]=std::to_string(cursor_b);
        m["selection_r"]=std::to_string(selection_r); m["selection_g"]=std::to_string(selection_g); m["selection_b"]=std::to_string(selection_b);
        m["line_highlight"]=std::to_string(line_highlight);
        m["font_family"]=font_family; m["font_size"]=std::to_string(font_size);
        m["line_height"]=std::to_string(line_height); m["indent_size"]=std::to_string(indent_size);
        return m;
    }

    static ThemeVariant from_map(const std::map<std::string,std::string> &m) {
        ThemeVariant t;
        auto getd=[&](const std::string&k,double d)->double{ if(!m.count(k))return d; try{return std::stod(m.at(k));}catch(...){return d;} };
        auto geti=[&](const std::string&k,int d)->int{ if(!m.count(k))return d; try{return std::stoi(m.at(k));}catch(...){return d;} };
        if(m.count("id"))t.id=m.at("id"); if(m.count("name"))t.name=m.at("name");
        if(m.count("is_light"))t.is_light=(m.at("is_light")=="1");
        t.bg=getd("bg",t.bg); t.surface=getd("surface",t.surface); t.panel=getd("panel",t.panel); t.elevated=getd("elevated",t.elevated);
        t.text=getd("text",t.text); t.muted=getd("muted",t.muted);
        t.gutter_bg=getd("gutter_bg",t.gutter_bg); t.gutter_text=getd("gutter_text",t.gutter_text);
        t.base_gutter_width=geti("base_gutter_width",t.base_gutter_width);
        t.icon_r=getd("icon_r",t.icon_r); t.icon_g=getd("icon_g",t.icon_g); t.icon_b=getd("icon_b",t.icon_b);
        t.accent_r=getd("accent_r",t.accent_r); t.accent_g=getd("accent_g",t.accent_g); t.accent_b=getd("accent_b",t.accent_b);
        t.accent2_r=getd("accent2_r",t.accent2_r); t.accent2_g=getd("accent2_g",t.accent2_g); t.accent2_b=getd("accent2_b",t.accent2_b);
        t.success_r=getd("success_r",t.success_r); t.success_g=getd("success_g",t.success_g); t.success_b=getd("success_b",t.success_b);
        t.warning_r=getd("warning_r",t.warning_r); t.warning_g=getd("warning_g",t.warning_g); t.warning_b=getd("warning_b",t.warning_b);
        t.error_r=getd("error_r",t.error_r); t.error_g=getd("error_g",t.error_g); t.error_b=getd("error_b",t.error_b);
        t.keyword_r=getd("kw_r",t.keyword_r); t.keyword_g=getd("kw_g",t.keyword_g); t.keyword_b=getd("kw_b",t.keyword_b);
        t.function_r=getd("fn_r",t.function_r); t.function_g=getd("fn_g",t.function_g); t.function_b=getd("fn_b",t.function_b);
        t.string_r=getd("str_r",t.string_r); t.string_g=getd("str_g",t.string_g); t.string_b=getd("str_b",t.string_b);
        t.comment_r=getd("cmt_r",t.comment_r); t.comment_g=getd("cmt_g",t.comment_g); t.comment_b=getd("cmt_b",t.comment_b);
        t.cursor_r=getd("cursor_r",t.cursor_r); t.cursor_g=getd("cursor_g",t.cursor_g); t.cursor_b=getd("cursor_b",t.cursor_b);
        t.selection_r=getd("selection_r",t.selection_r); t.selection_g=getd("selection_g",t.selection_g); t.selection_b=getd("selection_b",t.selection_b);
        t.line_highlight=getd("line_highlight",t.line_highlight);
        if(m.count("font_family"))t.font_family=m.at("font_family");
        t.font_size=geti("font_size",t.font_size); t.line_height=geti("line_height",t.line_height); t.indent_size=geti("indent_size",t.indent_size);
        return t;
    }
};

static inline double lerp(double a,double b,double t){return a+(b-a)*t;}

static inline ThemeVariant interpolate_theme(const ThemeVariant &a,const ThemeVariant &b,double t){
    ThemeVariant o; o.id=b.id; o.name=b.name; o.is_light=b.is_light;
    o.bg=lerp(a.bg,b.bg,t); o.surface=lerp(a.surface,b.surface,t);
    o.panel=lerp(a.panel,b.panel,t); o.elevated=lerp(a.elevated,b.elevated,t);
    o.text=lerp(a.text,b.text,t); o.muted=lerp(a.muted,b.muted,t);
    o.gutter_bg=lerp(a.gutter_bg,b.gutter_bg,t); o.gutter_text=lerp(a.gutter_text,b.gutter_text,t);
    o.base_gutter_width=(int)std::round(lerp(a.base_gutter_width,b.base_gutter_width,t));
    o.icon_r=lerp(a.icon_r,b.icon_r,t); o.icon_g=lerp(a.icon_g,b.icon_g,t); o.icon_b=lerp(a.icon_b,b.icon_b,t);
    o.accent_r=lerp(a.accent_r,b.accent_r,t); o.accent_g=lerp(a.accent_g,b.accent_g,t); o.accent_b=lerp(a.accent_b,b.accent_b,t);
    o.accent2_r=lerp(a.accent2_r,b.accent2_r,t); o.accent2_g=lerp(a.accent2_g,b.accent2_g,t); o.accent2_b=lerp(a.accent2_b,b.accent2_b,t);
    o.success_r=lerp(a.success_r,b.success_r,t); o.success_g=lerp(a.success_g,b.success_g,t); o.success_b=lerp(a.success_b,b.success_b,t);
    o.warning_r=lerp(a.warning_r,b.warning_r,t); o.warning_g=lerp(a.warning_g,b.warning_g,t); o.warning_b=lerp(a.warning_b,b.warning_b,t);
    o.error_r=lerp(a.error_r,b.error_r,t); o.error_g=lerp(a.error_g,b.error_g,t); o.error_b=lerp(a.error_b,b.error_b,t);
    o.keyword_r=lerp(a.keyword_r,b.keyword_r,t); o.keyword_g=lerp(a.keyword_g,b.keyword_g,t); o.keyword_b=lerp(a.keyword_b,b.keyword_b,t);
    o.function_r=lerp(a.function_r,b.function_r,t); o.function_g=lerp(a.function_g,b.function_g,t); o.function_b=lerp(a.function_b,b.function_b,t);
    o.string_r=lerp(a.string_r,b.string_r,t); o.string_g=lerp(a.string_g,b.string_g,t); o.string_b=lerp(a.string_b,b.string_b,t);
    o.comment_r=lerp(a.comment_r,b.comment_r,t); o.comment_g=lerp(a.comment_g,b.comment_g,t); o.comment_b=lerp(a.comment_b,b.comment_b,t);
    o.cursor_r=lerp(a.cursor_r,b.cursor_r,t); o.cursor_g=lerp(a.cursor_g,b.cursor_g,t); o.cursor_b=lerp(a.cursor_b,b.cursor_b,t);
    o.selection_r=lerp(a.selection_r,b.selection_r,t); o.selection_g=lerp(a.selection_g,b.selection_g,t); o.selection_b=lerp(a.selection_b,b.selection_b,t);
    o.line_highlight=lerp(a.line_highlight,b.line_highlight,t);
    o.font_family=b.font_family;
    o.font_size=(int)std::round(lerp(a.font_size,b.font_size,t));
    o.line_height=(int)std::round(lerp(a.line_height,b.line_height,t));
    o.indent_size=(int)std::round(lerp(a.indent_size,b.indent_size,t));
    return o;
}

static inline std::vector<ThemeVariant> default_themes() {
    std::vector<ThemeVariant> v;

    // ............ 0. Emerald Editor -- Deep neutral dark + vivid green accent ............
    // The new default look
    {ThemeVariant t; t.id="emerald_editor"; t.name="Emerald Editor";
    t.bg=0.055; t.surface=0.075; t.panel=0.095; t.elevated=0.120;
    t.gutter_bg=0.065; t.gutter_text=0.42; t.text=0.920; t.muted=0.520;
    t.accent_r=0.180; t.accent_g=0.820; t.accent_b=0.460;
    t.accent2_r=0.420; t.accent2_g=0.880; t.accent2_b=0.580;
    t.keyword_r=0.560; t.keyword_g=0.560; t.keyword_b=0.940;
    t.function_r=0.380; t.function_g=0.660; t.function_b=0.950;
    t.string_r=0.860; t.string_g=0.760; t.string_b=0.360;
    t.comment_r=0.400; t.comment_g=0.450; t.comment_b=0.420;
    t.cursor_r=t.accent_r; t.cursor_g=t.accent_g; t.cursor_b=t.accent_b;
    t.selection_r=0.100; t.selection_g=0.260; t.selection_b=0.170;
    t.success_r=t.accent_r; t.success_g=t.accent_g; t.success_b=t.accent_b;
    t.warning_r=0.950; t.warning_g=0.700; t.warning_b=0.200;
    t.error_r=0.900; t.error_g=0.320; t.error_b=0.320;
    t.line_highlight=0.022;
    t.icon_r=t.text; t.icon_g=t.text; t.icon_b=t.text; v.push_back(t);}


    // ............ 1. Tangerine Night -- Dark + citrus orange ............
    {ThemeVariant t; t.id="tangerine_night"; t.name="Tangerine Night";
    t.bg=0.085; t.surface=0.110; t.panel=0.135; t.elevated=0.160;
    t.gutter_bg=0.095; t.gutter_text=0.42; t.text=0.930; t.muted=0.520;
    t.accent_r=0.940; t.accent_g=0.420; t.accent_b=0.120;
    t.accent2_r=0.980; t.accent2_g=0.620; t.accent2_b=0.180;
    t.keyword_r=0.900; t.keyword_g=0.360; t.keyword_b=0.200;
    t.function_r=0.980; t.function_g=0.620; t.function_b=0.180;
    t.string_r=0.620; t.string_g=0.780; t.string_b=0.420;
    t.comment_r=0.320; t.comment_g=0.360; t.comment_b=0.420;
    t.cursor_r=t.accent_r; t.cursor_g=t.accent_g; t.cursor_b=t.accent_b;
    t.selection_r=0.240; t.selection_g=0.140; t.selection_b=0.060;
    t.success_r=0.620; t.success_g=0.780; t.success_b=0.420;
    t.warning_r=0.980; t.warning_g=0.620; t.warning_b=0.180;
    t.error_r=0.920; t.error_g=0.250; t.error_b=0.220;
    t.line_highlight=0.028;
    t.icon_r=t.text; t.icon_g=t.text; t.icon_b=t.text; v.push_back(t);}


    // ............ 2. Deep Bloom -- Teal + magenta flowers ............
    {ThemeVariant t; t.id="deep_bloom"; t.name="Deep Bloom";
    t.bg=0.070; t.surface=0.095; t.panel=0.120; t.elevated=0.145;
    t.gutter_bg=0.080; t.gutter_text=0.40; t.text=0.900; t.muted=0.500;
    t.accent_r=0.180; t.accent_g=0.600; t.accent_b=0.560;
    t.accent2_r=0.780; t.accent2_g=0.240; t.accent2_b=0.520;
    t.keyword_r=0.820; t.keyword_g=0.250; t.keyword_b=0.520;
    t.function_r=0.180; t.function_g=0.600; t.function_b=0.560;
    t.string_r=0.560; t.string_g=0.800; t.string_b=0.500;
    t.comment_r=0.300; t.comment_g=0.320; t.comment_b=0.420;
    t.cursor_r=t.accent_r; t.cursor_g=t.accent_g; t.cursor_b=t.accent_b;
    t.selection_r=0.120; t.selection_g=0.180; t.selection_b=0.250;
    t.success_r=0.560; t.success_g=0.800; t.success_b=0.500;
    t.warning_r=0.900; t.warning_g=0.400; t.warning_b=0.600;
    t.error_r=0.900; t.error_g=0.200; t.error_b=0.300;
    t.line_highlight=0.022;
    t.icon_r=t.text; t.icon_g=t.text; t.icon_b=t.text; v.push_back(t);}


    // ............ 3. Violet Petals ............
    {ThemeVariant t; t.id="violet_petals"; t.name="Violet Petals";
    t.bg=0.072; t.surface=0.096; t.panel=0.118; t.elevated=0.140;
    t.gutter_bg=0.082; t.gutter_text=0.40; t.text=0.900; t.muted=0.480;
    t.accent_r=0.450; t.accent_g=0.320; t.accent_b=0.700;
    t.accent2_r=0.820; t.accent2_g=0.700; t.accent2_b=0.900;
    t.keyword_r=0.720; t.keyword_g=0.420; t.keyword_b=0.850;
    t.function_r=0.480; t.function_g=0.560; t.function_b=0.900;
    t.string_r=0.620; t.string_g=0.780; t.string_b=0.500;
    t.comment_r=0.300; t.comment_g=0.320; t.comment_b=0.420;
    t.cursor_r=t.accent_r; t.cursor_g=t.accent_g; t.cursor_b=t.accent_b;
    t.selection_r=0.160; t.selection_g=0.120; t.selection_b=0.260;
    t.success_r=0.560; t.success_g=0.800; t.success_b=0.500;
    t.warning_r=0.940; t.warning_g=0.700; t.warning_b=0.300;
    t.error_r=0.900; t.error_g=0.300; t.error_b=0.400;
    t.line_highlight=0.020;
    t.icon_r=t.text; t.icon_g=t.text; t.icon_b=t.text; v.push_back(t);}


    // ............ 4. Forest Veil ............
    {ThemeVariant t; t.id="forest_veil"; t.name="Forest Veil";
    t.bg=0.068; t.surface=0.092; t.panel=0.115; t.elevated=0.138;
    t.gutter_bg=0.078; t.gutter_text=0.40; t.text=0.910; t.muted=0.490;
    t.accent_r=0.120; t.accent_g=0.500; t.accent_b=0.300;
    t.accent2_r=0.560; t.accent2_g=0.750; t.accent2_b=0.520;
    t.keyword_r=0.300; t.keyword_g=0.600; t.keyword_b=0.400;
    t.function_r=0.200; t.function_g=0.700; t.function_b=0.500;
    t.string_r=0.500; t.string_g=0.800; t.string_b=0.450;
    t.comment_r=0.280; t.comment_g=0.320; t.comment_b=0.380;
    t.cursor_r=t.accent_r; t.cursor_g=t.accent_g; t.cursor_b=t.accent_b;
    t.selection_r=0.120; t.selection_g=0.180; t.selection_b=0.120;
    t.success_r=0.500; t.success_g=0.800; t.success_b=0.450;
    t.warning_r=0.900; t.warning_g=0.700; t.warning_b=0.300;
    t.error_r=0.850; t.error_g=0.250; t.error_b=0.250;
    t.line_highlight=0.020;
    t.icon_r=t.text; t.icon_g=t.text; t.icon_b=t.text; v.push_back(t);}


    // ............ 5. Rowan Berry ............
    {ThemeVariant t; t.id="rowan_berry"; t.name="Rowan Berry";
    t.bg=0.075; t.surface=0.100; t.panel=0.125; t.elevated=0.150;
    t.gutter_bg=0.085; t.gutter_text=0.42; t.text=0.920; t.muted=0.500;
    t.accent_r=0.850; t.accent_g=0.250; t.accent_b=0.200;
    t.accent2_r=0.200; t.accent2_g=0.600; t.accent2_b=0.400;
    t.keyword_r=0.900; t.keyword_g=0.300; t.keyword_b=0.250;
    t.function_r=0.850; t.function_g=0.250; t.function_b=0.200;
    t.string_r=0.450; t.string_g=0.750; t.string_b=0.400;
    t.comment_r=0.320; t.comment_g=0.340; t.comment_b=0.420;
    t.cursor_r=t.accent_r; t.cursor_g=t.accent_g; t.cursor_b=t.accent_b;
    t.selection_r=0.200; t.selection_g=0.120; t.selection_b=0.100;
    t.success_r=0.450; t.success_g=0.750; t.success_b=0.400;
    t.warning_r=0.950; t.warning_g=0.650; t.warning_b=0.200;
    t.error_r=0.950; t.error_g=0.200; t.error_b=0.200;
    t.line_highlight=0.022;
    t.icon_r=t.text; t.icon_g=t.text; t.icon_b=t.text; v.push_back(t);}


    // ............ 6. Stormlight ............
    {ThemeVariant t; t.id="stormlight"; t.name="Stormlight";
    t.bg=0.065; t.surface=0.088; t.panel=0.110; t.elevated=0.135;
    t.gutter_bg=0.075; t.gutter_text=0.38; t.text=0.880; t.muted=0.470;
    t.accent_r=0.400; t.accent_g=0.700; t.accent_b=0.850;
    t.accent2_r=0.900; t.accent2_g=0.900; t.accent2_b=0.820;
    t.keyword_r=0.500; t.keyword_g=0.650; t.keyword_b=0.800;
    t.function_r=0.600; t.function_g=0.800; t.function_b=0.900;
    t.string_r=0.500; t.string_g=0.800; t.string_b=0.700;
    t.comment_r=0.300; t.comment_g=0.350; t.comment_b=0.450;
    t.cursor_r=t.accent_r; t.cursor_g=t.accent_g; t.cursor_b=t.accent_b;
    t.selection_r=0.140; t.selection_g=0.200; t.selection_b=0.260;
    t.success_r=0.500; t.success_g=0.800; t.success_b=0.700;
    t.warning_r=0.950; t.warning_g=0.800; t.warning_b=0.400;
    t.error_r=0.900; t.error_g=0.300; t.error_b=0.300;
    t.line_highlight=0.020;
    t.icon_r=t.text; t.icon_g=t.text; t.icon_b=t.text; v.push_back(t);}


    // ............ 7. Mistwood ............
    {ThemeVariant t; t.id="mistwood"; t.name="Mistwood";
    t.bg=0.070; t.surface=0.095; t.panel=0.118; t.elevated=0.140;
    t.gutter_bg=0.080; t.gutter_text=0.40; t.text=0.900; t.muted=0.480;
    t.accent_r=0.400; t.accent_g=0.500; t.accent_b=0.450;
    t.accent2_r=0.700; t.accent2_g=0.750; t.accent2_b=0.650;
    t.keyword_r=0.600; t.keyword_g=0.650; t.keyword_b=0.550;
    t.function_r=0.500; t.function_g=0.700; t.function_b=0.600;
    t.string_r=0.600; t.string_g=0.750; t.string_b=0.500;
    t.comment_r=0.320; t.comment_g=0.340; t.comment_b=0.420;
    t.cursor_r=t.accent_r; t.cursor_g=t.accent_g; t.cursor_b=t.accent_b;
    t.selection_r=0.140; t.selection_g=0.160; t.selection_b=0.140;
    t.success_r=0.600; t.success_g=0.750; t.success_b=0.500;
    t.warning_r=0.900; t.warning_g=0.700; t.warning_b=0.300;
    t.error_r=0.850; t.error_g=0.300; t.error_b=0.250;
    t.line_highlight=0.020;
    t.icon_r=t.text; t.icon_g=t.text; t.icon_b=t.text; v.push_back(t);}

    return v;
}

static inline int find_theme_index(const std::vector<ThemeVariant>&list,const std::string&id){
    for(size_t i=0;i<list.size();++i) if(list[i].id==id) return (int)i;
    return -1;
}
static inline ThemeVariant theme_by_id_or_default(const std::vector<ThemeVariant>&list,const std::string&id){
    int idx=find_theme_index(list,id);
    if(idx>=0) return list[idx];
    return list.empty()?ThemeVariant():list[0];
}

// ............ Adco themes ............
// A theme is a `theme "Name" { ... }` declaration in an .adco file (see
// base/adco.hxx and docs/ADCO.md).  Colours may be written as rgb(), gray(),
// hsl(), #rrggbb, mix(), lighten() ... - they are evaluated by the Adco
// engine and mapped onto ThemeVariant here.  `extends "Other"` starts from a
// theme that is already known (built-in or loaded earlier)

#include "../base/adco.hxx"

static inline std::string adco_theme_slug(const std::string &name) {
    std::string id;
    for (char c : name) {
        if (std::isalnum((unsigned char)c)) id += (char)std::tolower((unsigned char)c);
        else if (!id.empty() && id.back() != '_') id += '_';
    }
    while (!id.empty() && id.back() == '_') id.pop_back();
    return id.empty() ? "theme" : id;
}

// Scalar surface values (bg, panel, text ...) are single grey levels in
// ThemeVariant; colours are folded to their average so tinted input still works
static inline double adco_scalar(const adco::Value &v, double fallback) {
    if (v.t == adco::Value::Num) return v.n;
    if (v.t == adco::Value::Col) return (v.c.r + v.c.g + v.c.b) / 3.0;
    return fallback;
}

static inline bool adco_rgb(const adco::Value &v, double &r, double &g, double &b) {
    if (v.t == adco::Value::Col) { r = v.c.r; g = v.c.g; b = v.c.b; return true; }
    if (v.t == adco::Value::Num) { r = g = b = v.n; return true; }
    return false;
}

// Builds a ThemeVariant from a parsed `theme` node.  `known` is used to
// resolve `extends`.  Problems are appended to `notes` (one line each)
static inline ThemeVariant theme_from_adco(const adco::Node &n,
                                           const std::vector<ThemeVariant> &known,
                                           std::vector<std::string> *notes = nullptr) {
    ThemeVariant t;
    if (!n.extends.empty()) {
        bool found = false;
        for (auto &k : known)
            if (k.name == n.extends || k.id == n.extends || k.id == adco_theme_slug(n.extends)) { t = k; found = true; break; }
        if (!found && notes) notes->push_back("theme \"" + n.label + "\": unknown base theme \"" + n.extends + "\"");
    }
    t.name = n.label.empty() ? "Untitled" : n.label;
    t.id   = adco_theme_slug(t.name);

    if (const adco::Value *v = n.get("dark"))  t.is_light = !v->truthy();
    if (const adco::Value *v = n.get("light")) t.is_light =  v->truthy();

    if (const adco::Node *c = n.kid("colors")) {
        auto scalar = [&](const char *key, double &dst) {
            if (const adco::Value *v = c->get(key)) dst = adco_scalar(*v, dst);
        };
        auto rgb3 = [&](const char *key, double &r, double &g, double &b) {
            if (const adco::Value *v = c->get(key)) adco_rgb(*v, r, g, b);
        };
        if (const adco::Value *v = c->get("bg")) {
            double d = adco_scalar(*v, t.bg);
            t.bg = d; t.surface = d + 0.03; t.panel = d + 0.05; t.elevated = d + 0.07; t.gutter_bg = d + 0.01;
        }
        scalar("surface", t.surface);   scalar("panel", t.panel);   scalar("elevated", t.elevated);
        scalar("text", t.text);
        if (const adco::Value *v = c->get("muted")) { t.muted = adco_scalar(*v, t.muted); t.gutter_text = t.muted * 0.75; }
        scalar("gutterBg", t.gutter_bg); scalar("gutterText", t.gutter_text);
        if (const adco::Value *v = c->get("accent")) {
            double r, g, b;
            if (adco_rgb(*v, r, g, b)) { t.accent_r = t.cursor_r = r; t.accent_g = t.cursor_g = g; t.accent_b = t.cursor_b = b; }
        }
        rgb3("accent2",   t.accent2_r,   t.accent2_g,   t.accent2_b);
        rgb3("keyword",   t.keyword_r,   t.keyword_g,   t.keyword_b);
        rgb3("function",  t.function_r,  t.function_g,  t.function_b);
        rgb3("string",    t.string_r,    t.string_g,    t.string_b);
        rgb3("comment",   t.comment_r,   t.comment_g,   t.comment_b);
        rgb3("cursor",    t.cursor_r,    t.cursor_g,    t.cursor_b);
        rgb3("selection", t.selection_r, t.selection_g, t.selection_b);
        rgb3("error",     t.error_r,     t.error_g,     t.error_b);
        rgb3("warning",   t.warning_r,   t.warning_g,   t.warning_b);
        rgb3("success",   t.success_r,   t.success_g,   t.success_b);
        scalar("lineHighlight", t.line_highlight);
        for (auto &p : c->props) {
            static const char *known_keys[] = {"bg","surface","panel","elevated","text","muted","gutterBg","gutterText",
                "accent","accent2","keyword","function","string","comment","cursor","selection","error","warning",
                "success","lineHighlight"};
            bool ok = false; for (auto *k : known_keys) if (p.first == k) ok = true;
            if (!ok && notes) notes->push_back("theme \"" + t.name + "\": unknown colour '" + p.first + "'");
        }
    }
    if (const adco::Node *f = n.kid("font")) {
        if (const adco::Value *v = f->get("family"); v && v->t == adco::Value::Str) t.font_family = v->s;
        if (const adco::Value *v = f->get("size"))       t.font_size   = (short)std::lround(adco_scalar(*v, t.font_size));
        if (const adco::Value *v = f->get("lineHeight")) t.line_height = (short)std::lround(adco_scalar(*v, t.line_height));
        if (const adco::Value *v = f->get("indent"))     t.indent_size = (short)std::lround(adco_scalar(*v, t.indent_size));
    }
    t.icon_r = t.text; t.icon_g = t.text; t.icon_b = t.text;
    return t;
}

#endif
