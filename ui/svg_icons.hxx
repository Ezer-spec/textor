#ifndef SVG_ICONS_HXX
#define SVG_ICONS_HXX

//  svg_icons.hxx - SVG icons (file icons and interface icons), drawn with cairo
//
//  The icons are the files from assets/icons/, packed into the binary by
//  tools/embed_icons.lua. nanosvg parses each icon once (see svg_icons.cxx);
//  every frame the parsed paths are simply handed to cairo.
//  All icons are single-colour: the colour is chosen by the caller, so the
//  icons follow the theme.

#include <cairo/cairo.h>
#include <string>

namespace svg_icons {

// Name of the built-in file icon for an extension (or addon symbol); nullptr = none yet
const char *name_for(const std::string &ext);

// Built-in file icon of `ext`, drawn into the square (x, y, size, size) in one colour.
// false = no SVG icon for this extension (the caller draws its own).
bool draw(cairo_t *cr, const std::string &ext, double x, double y, double size,
          double r, double g, double b, double alpha = 1.0);

// Extension that picks the icon of a file: lower case, no dot. CMakeLists.txt -> "cmake".
std::string icon_ext(const std::string &file_name);

// Plain file icon, for files that have no icon of their own
bool draw_plain_file(cairo_t *cr, double x, double y, double size,
                     double r, double g, double b, double alpha = 1.0);

// Icon from SVG text (an addon's icon). `key` must be unique per icon, e.g. its file path.
bool draw_text(cairo_t *cr, const std::string &key, const std::string &svg_text,
               double x, double y, double size,
               double r, double g, double b, double alpha = 1.0);

// Interface icon by short name: "explorer", "split", "console", "viewers", "plugin", "settings", "exit",
// "general", "performance", "themes", "keybindings", "help", "about", "folder", "search",
// "arrow_left", "arrow_right", "arrow_up", "arrow_down", "plus", "cross". flip_x mirrors it (arrow_left -> arrow right).
bool draw_ui(cairo_t *cr, const char *name, double x, double y, double size,
             double r, double g, double b, double alpha = 1.0, bool flip_x = false);

}

#endif
