#include "svg_icons.hxx"

#include <cctype>
#include <map>

#define NANOSVG_IMPLEMENTATION
#include "../third_party/nanosvg/nanosvg.h"
#include "file_icons_data.hxx"

namespace svg_icons {

// Icon name for a file extension (or addon symbol); nullptr = no SVG icon for it yet.
const char *name_for(const std::string &ext) {
    static const std::map<std::string, const char *> names = {
        {"c", "c"},          {"h", "c"},
        {"cpp", "cpp"},      {"cxx", "cpp"},   {"cc", "cpp"},
        {"hpp", "cpp"},      {"hxx", "cpp"},   {"hh", "cpp"},
        {"sh", "bash"},      {"bash", "bash"}, {"zsh", "bash"},
        {"js", "javascript"},{"jsx", "javascript"},
        {"mjs", "javascript"},{"cjs", "javascript"},
        {"kt", "kotlin"},    {"kts", "kotlin"},
        {"lua", "lua"},      {"mek", "mekanism"},
        {"nim", "nim"},      {"py", "python"},
        {"rb", "ruby"},      {"rs", "rust"},
        {"swift", "swift"},
        {"png", "image"},    {"jpg", "image"}, {"jpeg", "image"}, {"gif", "image"},
        {"bmp", "image"},    {"ico", "image"}, {"webp", "image"}, {"svg", "image"},
        {"xml", "xml"},      {"xsd", "xml"},   {"xsl", "xml"},    {"xslt", "xml"},
        {"cmake", "cmake"},
        {"txt", "file"},     {"log", "file"},
        {"ts", "typescript"},{"tsx", "typescript"},
        {"go", "go"},        {"java", "java"}, {"d", "d"},
        {"html", "html"},    {"htm", "html"},
        {"css", "css"},      {"scss", "css"},
        {"json", "json"},    {"jsonc", "json"},
        {"yaml", "yaml"},    {"yml", "yaml"},  {"toml", "toml"},
        {"md", "markdown"},  {"markdown", "markdown"},
        {"adco", "adco"},
    };
    auto it = names.find(ext);
    return it == names.end() ? nullptr : it->second;
}

// Parses SVG text once per key and keeps the result (nullptr if the SVG is broken).
// Used for the built-in icons and for icons that come from addons.
static NSVGimage *parse_cached(const std::string &key, const std::string &svg_text) {
    static std::map<std::string, NSVGimage *> cache;
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;

    std::string text = svg_text;                      // nanosvg edits the text while parsing
    NSVGimage *img = nsvgParse(&text[0], "px", 96.0f);
    if (img && (img->width <= 0 || img->height <= 0)) { nsvgDelete(img); img = nullptr; }
    cache[key] = img;
    return img;
}

// Built-in icon by name (see tools/embed_icons.lua)
static NSVGimage *load(const char *name) {
    for (const EmbeddedIcon *e = k_embedded_icons; e->name; ++e)
        if (std::string(e->name) == name) return parse_cached(std::string("builtin:") + name, e->svg);
    return nullptr;
}

// Draws a parsed icon into the square (x, y, size, size) in one colour
static void render(cairo_t *cr, NSVGimage *img, double x, double y, double size,
                   double r, double g, double b, double alpha, bool flip_x = false) {
    double scale = size / std::max(img->width, img->height);
    double ox = x + (size - img->width  * scale) / 2.0;   // centre inside the square
    double oy = y + (size - img->height * scale) / 2.0;

    cairo_save(cr);
    cairo_translate(cr, ox, oy);
    cairo_scale(cr, scale, scale);
    if (flip_x) { cairo_translate(cr, img->width, 0); cairo_scale(cr, -1, 1); }   // mirror left-right

    for (NSVGshape *s = img->shapes; s; s = s->next) {
        if (!(s->flags & NSVG_FLAGS_VISIBLE)) continue;

        cairo_new_path(cr);
        for (NSVGpath *p = s->paths; p; p = p->next) {
            cairo_move_to(cr, p->pts[0], p->pts[1]);
            for (int i = 0; i < p->npts - 1; i += 3) {
                const float *c = &p->pts[i * 2];
                cairo_curve_to(cr, c[2], c[3], c[4], c[5], c[6], c[7]);
            }
            if (p->closed) cairo_close_path(cr);
        }

        cairo_set_source_rgba(cr, r, g, b, alpha * s->opacity);
        if (s->fill.type != NSVG_PAINT_NONE) {
            cairo_set_fill_rule(cr, s->fillRule == NSVG_FILLRULE_EVENODD
                                    ? CAIRO_FILL_RULE_EVEN_ODD : CAIRO_FILL_RULE_WINDING);
            if (s->stroke.type != NSVG_PAINT_NONE) cairo_fill_preserve(cr);
            else                                    cairo_fill(cr);
        }
        if (s->stroke.type != NSVG_PAINT_NONE) {
            cairo_set_line_width(cr, s->strokeWidth);
            cairo_set_line_cap(cr, s->strokeLineCap == NSVG_CAP_ROUND  ? CAIRO_LINE_CAP_ROUND
                                 : s->strokeLineCap == NSVG_CAP_SQUARE ? CAIRO_LINE_CAP_SQUARE
                                                                       : CAIRO_LINE_CAP_BUTT);
            cairo_set_line_join(cr, s->strokeLineJoin == NSVG_JOIN_ROUND ? CAIRO_LINE_JOIN_ROUND
                                  : s->strokeLineJoin == NSVG_JOIN_BEVEL ? CAIRO_LINE_JOIN_BEVEL
                                                                         : CAIRO_LINE_JOIN_MITER);
            cairo_stroke(cr);
        }
    }
    cairo_restore(cr);
}

// Built-in icon of a file extension. false = no SVG icon for it (caller draws its own).
bool draw(cairo_t *cr, const std::string &ext, double x, double y, double size,
                 double r, double g, double b, double alpha) {
    const char *name = name_for(ext);
    NSVGimage *img = name ? load(name) : nullptr;
    if (!img) return false;
    render(cr, img, x, y, size, r, g, b, alpha);
    return true;
}

// Extension used to pick an icon: lower case, no dot ("Main.CPP" -> "cpp").
// CMakeLists.txt gets the CMake icon although its extension is txt.
std::string icon_ext(const std::string &file_name) {
    if (file_name == "CMakeLists.txt") return "cmake";
    size_t dot = file_name.find_last_of('.');
    if (dot == std::string::npos || dot == 0) return "";
    std::string ext = file_name.substr(dot + 1);
    for (char &c : ext) c = (char)std::tolower((unsigned char)c);
    return ext;
}

// Plain file icon: for every file that has no icon of its own
bool draw_plain_file(cairo_t *cr, double x, double y, double size,
                     double r, double g, double b, double alpha) {
    NSVGimage *img = load("file");
    if (!img) return false;
    render(cr, img, x, y, size, r, g, b, alpha);
    return true;
}

// Icon from SVG text (an addon's icon). `key` must be unique per icon, e.g. its file path.
bool draw_text(cairo_t *cr, const std::string &key, const std::string &svg_text,
                      double x, double y, double size,
                      double r, double g, double b, double alpha) {
    if (svg_text.empty()) return false;
    NSVGimage *img = parse_cached(key, svg_text);
    if (!img) return false;
    render(cr, img, x, y, size, r, g, b, alpha);
    return true;
}

// Icon of the editor interface (assets/icons/ui/<name>.svg), by its short name, e.g. "search"
bool draw_ui(cairo_t *cr, const char *name, double x, double y, double size,
             double r, double g, double b, double alpha, bool flip_x) {
    NSVGimage *img = load((std::string("ui_") + name).c_str());
    if (!img) return false;
    render(cr, img, x, y, size, r, g, b, alpha, flip_x);
    return true;
}

}
