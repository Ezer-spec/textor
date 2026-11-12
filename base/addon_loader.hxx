#ifndef ADDON_LOADER_HXX
#define ADDON_LOADER_HXX

// .............................................................................
//  AddonLoader - loads themes and language support from  *.adco  files
//
//  Everything an addon can provide is written in Adco (base/adco.hxx), the
//  Swift-like language that Textor uses for themes and addons:
//
//      addon "Lua Support" {
//          version: "1.0"
//          author:  "Textor Community"
//
//          language "Lua" {
//              extensions: [".lua"]
//              keywords:   ["function", "end", "local", "if", "then"]
//              types:      ["table", "string"]
//              builtins:   ["print", "pairs", "ipairs"]
//              completions: ["table.insert", "string.format"]
//              strings:    ["\"", "'"]
//              comment { line: "--"; block: ["--[[", "]]"] }
//              colors { keyword: #8CA6FF; type: #4DD9D9; builtin: #8CD9B3 }
//              tokenColors { "self": #FFB86C }
//              icon { color: #7388F2; svg: "lua.svg" }
//          }
//
//          theme "Lua Night" { ... }      // addons may bundle themes too
//      }
//
//  See docs/ADCO.md for the full reference.  A bare `theme` or `language`
//  declaration at the top level of a file works as well.
//
//  Every addon lives in its own folder named after it; the icon is in svg/:
//
//      addons/lua/lua.adco          the addon (names the icon: icon { svg: "lua.svg" })
//      addons/lua/svg/lua.svg       the file icon
//
//  A single .adco file directly inside addons/ or themes/ still works too.
//
//  Files are read from the addons/ and themes/ folders next to the program,
//  from ./addons and ./themes in the current folder, and from
//  ~/.config/textor/addons and ~/.config/textor/themes (these win).
// .............................................................................

#include <string>
#include <vector>
#include <map>
#include <set>
#include <array>
#include <unordered_set>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <iostream>
#include "config.hxx"
#include "adco.hxx"
#include "../color/themes.hxx"

namespace fs = std::filesystem;

// One language definition (the unit the highlighter, the file icons and the
// completion list work with).  Several of these can come out of one addon.
struct LangAddon {
    // addon metadata
    std::string addon_name;
    std::string version;
    std::string author;
    std::string description;
    std::string source_file;    // full path of the .adco file ("" for built-in)
    bool        builtin = false;

    // language
    std::string lang_name;
    std::vector<std::string> extensions;   // e.g. {".lua"}

    // token sets
    std::unordered_set<std::string> keywords;
    std::unordered_set<std::string> types;
    std::unordered_set<std::string> builtins;
    std::vector<std::string> completions;

    // comments / strings
    std::string comment_line;
    std::string comment_block_start;
    std::string comment_block_end;
    std::string doc_comment;
    std::string string_chars = "\"'";

    // highlighter switches (used by the Adco language itself, available to addons)
    bool hex_colors     = false;   // paint #rrggbb literals in their own colour
    bool highlight_keys = false;   // `name:` member keys get their own colour
    char attr_prefix    = 0;       // '@' -> @attribute tokens

    // colours (0..1).  -1 = "unset": falls back to the active theme.
    double kw_r = 0.40, kw_g = 0.70, kw_b = 1.00;
    double ty_r = 0.30, ty_g = 0.90, ty_b = 0.90;
    double bi_r = -1.0, bi_g = -1.0, bi_b = -1.0;
    double op_r = -1.0, op_g = -1.0, op_b = -1.0;

    // file icon (sidebar)
    double icon_r = -1.0, icon_g = -1.0, icon_b = -1.0;
    std::string icon_symbol;
    std::string icon_svg_path;   // SVG icon from the addon folder (full path, "" = none)
    std::string icon_svg;        // text of that SVG file

    std::map<std::string, std::array<double,3>> token_colors;   // word -> rgb

    bool loaded = false;
};

namespace adco_sem {

static inline std::vector<std::string> strings_of(const adco::Value *v) {
    std::vector<std::string> out;
    if (!v) return out;
    if (v->t == adco::Value::Str) { out.push_back(v->s); return out; }
    if (v->t == adco::Value::Arr)
        for (auto &x : v->a) if (x.t == adco::Value::Str) out.push_back(x.s);
    return out;
}

static inline bool color_of(const adco::Value *v, double &r, double &g, double &b) {
    if (!v) return false;
    return adco_rgb(*v, r, g, b);
}

// Reads the SVG named in `icon { svg: "name.svg" }`: first from the addon's own
// svg/ folder, then from the addon folder itself.
static inline void load_icon_svg(LangAddon &L, const std::string &name, std::vector<std::string> &notes) {
    if (name.find("..") != std::string::npos || (!name.empty() && name[0] == '/')) {
        notes.push_back("language \"" + L.lang_name + "\": icon svg must be a file inside the addon folder");
        return;
    }
    fs::path dir = fs::path(L.source_file).parent_path();
    for (const fs::path &p : {dir / "svg" / name, dir / name}) {
        std::error_code ec;
        if (!fs::is_regular_file(p, ec)) continue;
        std::ifstream ifs(p, std::ios::binary);
        std::stringstream ss; ss << ifs.rdbuf();
        L.icon_svg = ss.str(); L.icon_svg_path = p.string();
        return;
    }
    notes.push_back("language \"" + L.lang_name + "\": icon svg '" + name + "' not found (looked in svg/ of the addon folder)");
}

// Turns one `language "Name" { ... }` node into a LangAddon.
static inline LangAddon language_from_node(const adco::Node &n, const LangAddon &meta,
                                           std::vector<std::string> &notes) {
    LangAddon L;
    L.addon_name = meta.addon_name; L.version = meta.version; L.author = meta.author;
    L.description = meta.description; L.source_file = meta.source_file;
    L.lang_name = n.label;
    if (L.lang_name.empty()) { notes.push_back("language without a name was skipped"); return L; }

    for (auto e : strings_of(n.get("extensions"))) {
        if (e.empty()) continue;
        if (e[0] != '.') e = "." + e;
        L.extensions.push_back(e);
    }
    for (auto &w : strings_of(n.get("keywords")))    L.keywords.insert(w);
    for (auto &w : strings_of(n.get("types")))       L.types.insert(w);
    for (auto &w : strings_of(n.get("builtins")))    L.builtins.insert(w);
    L.completions = strings_of(n.get("completions"));

    if (const adco::Value *v = n.get("strings")) {
        L.string_chars.clear();
        for (auto &s : strings_of(v)) if (!s.empty()) L.string_chars += s[0];
    }
    if (const adco::Node *c = n.kid("comment")) {
        if (const adco::Value *v = c->get("line"); v && v->t == adco::Value::Str) L.comment_line = v->s;
        if (const adco::Value *v = c->get("doc");  v && v->t == adco::Value::Str) L.doc_comment  = v->s;
        auto blk = strings_of(c->get("block"));
        if (blk.size() == 2) { L.comment_block_start = blk[0]; L.comment_block_end = blk[1]; }
        else if (!blk.empty()) notes.push_back("language \"" + L.lang_name + "\": comment.block needs [start, end]");
    }
    if (const adco::Node *c = n.kid("colors")) {
        color_of(c->get("keyword"),  L.kw_r, L.kw_g, L.kw_b);
        color_of(c->get("type"),     L.ty_r, L.ty_g, L.ty_b);
        color_of(c->get("builtin"),  L.bi_r, L.bi_g, L.bi_b);
        color_of(c->get("operator"), L.op_r, L.op_g, L.op_b);
    }
    if (const adco::Node *c = n.kid("tokenColors")) {
        for (auto &p : c->props) {
            double r, g, b;
            if (color_of(&p.second, r, g, b)) L.token_colors[p.first] = {r, g, b};
        }
    }
    if (const adco::Node *c = n.kid("icon")) {
        color_of(c->get("color"), L.icon_r, L.icon_g, L.icon_b);
        if (const adco::Value *v = c->get("symbol"); v && v->t == adco::Value::Str) L.icon_symbol = v->s;
        if (const adco::Value *v = c->get("svg");    v && v->t == adco::Value::Str && !v->s.empty())
            load_icon_svg(L, v->s, notes);
    }
    if (const adco::Value *v = n.get("hexColors"))       L.hex_colors     = v->truthy();
    if (const adco::Value *v = n.get("highlightKeys"))   L.highlight_keys = v->truthy();
    if (const adco::Value *v = n.get("attributePrefix"); v && v->t == adco::Value::Str && !v->s.empty())
        L.attr_prefix = v->s[0];

    L.loaded = true;
    return L;
}

// The Adco language itself - built in, so its own files are always highlighted, I think that’s logical... -_-
static inline LangAddon builtin_adco_language() {
    LangAddon L;
    L.addon_name = "Adco"; L.version = "1.0"; L.author = "Textor";
    L.description = "Built-in support for the Adco language (themes and addons)";
    L.builtin = true;
    L.lang_name = "Adco";
    L.extensions = {".adco"};
    L.keywords = {"theme","addon","language","import","let","var","func","return","if","else",
                  "extends","true","false","nil"};
    L.types    = {"Color","Number","String","Bool","Array",
                  "colors","font","comment","icon","tokenColors"};
    L.builtins = {"rgb","rgb8","gray","hex","hsl","mix","lighten","darken","alpha",
                  "min","max","clamp","abs","round","count","upper","lower"};
    L.comment_line = "//"; L.comment_block_start = "/*"; L.comment_block_end = "*/"; L.doc_comment = "///";
    L.string_chars = "\"";
    L.hex_colors = true; L.highlight_keys = true; L.attr_prefix = '@';
    L.kw_r = 0.78; L.kw_g = 0.52; L.kw_b = 0.95;
    L.ty_r = 0.36; L.ty_g = 0.82; L.ty_b = 0.82;
    L.icon_r = 0.70; L.icon_g = 0.52; L.icon_b = 0.95; L.icon_symbol = "adco";
    for (auto *w : {"theme","addon","language","import","let","var","func","return","if","else","extends",
                    "colors","font","comment","icon","tokenColors","version","author","description","extensions",
                    "keywords","types","builtins","completions","strings","dark","light","family","size",
                    "lineHeight","indent","line","block","doc","symbol","svg","color","hexColors","highlightKeys",
                    "attributePrefix","bg","surface","panel","elevated","text","muted","gutterBg","gutterText",
                    "accent","accent2","keyword","function","string","cursor","selection","error","warning",
                    "success","lineHighlight","type","builtin","operator"})
        L.completions.push_back(w);
    for (const auto &w : L.builtins) L.completions.push_back(w + "(");
    L.loaded = true;
    return L;
}

}

struct AddonLoader {
    std::vector<LangAddon>    addons;     // one entry per language (built-in Adco first)
    std::vector<ThemeVariant> themes;     // themes found in .adco files
    std::vector<std::string>  problems;   // "file:line:col: message" - shown in the Addons panel
    int files_loaded = 0;

    // Themes wait here until every file is read, so `extends` works no matter
    // which file comes first.  (file name, theme block)
    std::vector<std::pair<std::string, adco::Node>> pending_themes;

    // Folder of the running program (empty if it cannot be found).
    static std::string exe_dir() {
        std::error_code ec;
        for (const char *link : {"/proc/self/exe", "/proc/curproc/file"}) {
            fs::path p = fs::read_symlink(link, ec);
            if (!ec && !p.empty()) return p.parent_path().string();
            ec.clear();
        }
        return "";
    }

    // Where .adco files are read from. Later folders win, so the user's own
    // folders come last and can override the bundled files.
    static std::vector<std::string> search_dirs() {
        std::vector<std::string> d;
        std::string exe = exe_dir();
        if (!exe.empty()) {
            d.push_back(exe + "/addons");                   // next to the program
            d.push_back(exe + "/themes");
            d.push_back(exe + "/../addons");                // program in build/
            d.push_back(exe + "/../themes");
            d.push_back(exe + "/../share/textor/addons");   // installed (see flake.nix)
        }
        d.push_back("addons");                              // current folder
        d.push_back("themes");
        d.push_back(textor_addons_dir());                   // ~/.config/textor/...
        d.push_back(textor_themes_dir());
        return d;
    }

    // .adco files of a folder: the ones lying directly in it, and the ones inside
    // its sub-folders (one folder per addon: addons/lua/lua.adco)
    static std::vector<fs::path> adco_files_in(const fs::path &dir) {
        std::vector<fs::path> out;
        std::error_code ec;
        for (auto &entry : fs::directory_iterator(dir, ec)) {
            if (entry.is_directory(ec)) {
                for (auto &inner : fs::directory_iterator(entry.path(), ec))
                    if (inner.is_regular_file(ec) && inner.path().extension() == ".adco")
                        out.push_back(inner.path());
            } else if (entry.path().extension() == ".adco") {
                out.push_back(entry.path());
            }
        }
        std::sort(out.begin(), out.end());
        return out;
    }

    // `base_themes` are the built-in palettes - used to resolve `extends "..."`.
    void load_all(const std::vector<ThemeVariant> &base_themes = {}) {
        addons.clear(); themes.clear(); problems.clear(); pending_themes.clear(); files_loaded = 0;
        addons.push_back(adco_sem::builtin_adco_language());

        std::set<std::string> seen_files;
        std::vector<ThemeVariant> known = base_themes;
        std::error_code ec;

        for (auto &dir : search_dirs()) {
            if (!fs::is_directory(dir, ec)) continue;
            std::vector<fs::path> paths = adco_files_in(dir);
            for (auto &p : paths) {
                std::string canon = fs::weakly_canonical(p, ec).string();
                if (!seen_files.insert(canon).second) continue;   // same file reached via two dirs
                load_file(p.string(), known);
            }
        }
        resolve_themes(known);

        std::cerr << "[Adco] " << files_loaded << " file(s), " << (addons.size() - 1)
                  << " language(s), " << themes.size() << " theme(s), "
                  << problems.size() << " problem(s)\n";
    }

    void load_file(const std::string &path, std::vector<ThemeVariant> &known) {
        std::ifstream ifs(path);
        if (!ifs) { problems.push_back(path + ": cannot open file"); return; }
        std::stringstream ss; ss << ifs.rdbuf();
        adco::Document doc = adco::evaluate(ss.str());
        files_loaded++;

        std::string base = fs::path(path).filename().string();
        for (auto &d : doc.diags) {
            std::string msg = base + ":" + std::to_string(d.line) + ":" + std::to_string(d.col) + ": " + d.msg;
            problems.push_back(msg);
            std::cerr << "[Adco] " << msg << "\n";
        }
        for (auto &imp : doc.imports)
            if (imp != "adco.std" && imp != "adco")
                problems.push_back(base + ": unknown import '" + imp + "'");

        std::vector<std::string> notes;
        for (auto &n : doc.decls) {
            if (n.has_attr("hidden")) continue;
            if (n.kind == "addon") {
                LangAddon meta;
                meta.addon_name = n.label.empty() ? base : n.label;
                meta.source_file = path;
                auto str_of = [&](const char *k) -> std::string {
                    const adco::Value *v = n.get(k);
                    return v && v->t == adco::Value::Str ? v->s : std::string();
                };
                meta.version = str_of("version"); meta.author = str_of("author"); meta.description = str_of("description");
                for (auto &k : n.kids) {
                    if (k.kind == "language")   add_language(k, meta, notes);
                    else if (k.kind == "theme") queue_theme(k, base);
                    else notes.push_back("addon \"" + meta.addon_name + "\": unknown block '" + k.kind + "'");
                }
            }
            else if (n.kind == "language") {
                LangAddon meta; meta.addon_name = n.label; meta.source_file = path;
                add_language(n, meta, notes);
            }
            else if (n.kind == "theme") queue_theme(n, base);
            else notes.push_back("unknown declaration '" + n.kind + "'");
        }
        for (auto &m : notes) { problems.push_back(base + ": " + m); std::cerr << "[Adco] " << base << ": " << m << "\n"; }
    }

    void add_language(const adco::Node &n, const LangAddon &meta, std::vector<std::string> &notes) {
        LangAddon L = adco_sem::language_from_node(n, meta, notes);
        if (L.loaded) addons.push_back(std::move(L));
    }
    // Themes are queued; resolve_themes() builds them once all files are loaded.
    void queue_theme(const adco::Node &n, const std::string &file) { pending_themes.push_back({file, n}); }

    // A theme can be built when its base theme (extends) is known. Repeat until nothing changes,
    // so a theme may extend another .adco theme that is loaded later.
    void resolve_themes(std::vector<ThemeVariant> &known) {
        auto is_known = [&](const std::string &name) {
            for (auto &k : known)
                if (k.name == name || k.id == name || k.id == adco_theme_slug(name)) return true;
            return false;
        };
        bool progress = true;
        while (!pending_themes.empty() && progress) {
            progress = false;
            for (size_t i = 0; i < pending_themes.size(); ) {
                const adco::Node &n = pending_themes[i].second;
                if (!n.extends.empty() && !is_known(n.extends)) { i++; continue; }
                build_theme(n, pending_themes[i].first, known);
                pending_themes.erase(pending_themes.begin() + i);
                progress = true;
            }
        }
        // What is left extends a theme that does not exist: still build it, with a warning.
        for (auto &p : pending_themes) build_theme(p.second, p.first, known);
        pending_themes.clear();
    }

    void build_theme(const adco::Node &n, const std::string &file, std::vector<ThemeVariant> &known) {
        std::vector<std::string> notes;
        ThemeVariant t = theme_from_adco(n, known, &notes);
        for (auto &m : notes) { problems.push_back(file + ": " + m); std::cerr << "[Adco] " << file << ": " << m << "\n"; }

        // A later theme with the same id replaces the earlier one
        bool replaced = false;
        for (auto &x : themes) if (x.id == t.id) { x = t; replaced = true; }
        if (!replaced) themes.push_back(t);
        replaced = false;
        for (auto &k : known) if (k.id == t.id) { k = t; replaced = true; }
        if (!replaced) known.push_back(t);
    }

    const LangAddon *find_by_ext(const std::string &ext) const {
        std::string e = ext;
        if (!e.empty() && e[0] != '.') e = "." + e;
        for (auto it = addons.rbegin(); it != addons.rend(); ++it)   // later files win
            for (auto &x : it->extensions) if (x == e) return &*it;
        return nullptr;
    }

    const LangAddon *find_by_lang(const std::string &lang) const {
        std::string lu = lang;
        for (auto &c : lu) c = (char)std::tolower((unsigned char)c);
        for (auto &a : addons) {
            std::string ln = a.lang_name;
            for (auto &c : ln) c = (char)std::tolower((unsigned char)c);
            if (ln == lu) return &a;
        }
        return nullptr;
    }

    std::vector<std::string> get_completions(const std::string &lang) const {
        const LangAddon *a = find_by_lang(lang);
        return a ? a->completions : std::vector<std::string>{};
    }
};

#endif
