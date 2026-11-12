#ifndef KEYMAP_HXX
#define KEYMAP_HXX

// ............ Native keyboard / shortcut helpers ............
// That crate
// existed only to (a) map raw X11/XKB keysyms to Latin/Cyrillic layout text
// when the platform didn't already resolve a printable string, (b) classify
// Ctrl/Shift/Alt chords into editor shortcuts, and (c) compute the edge-scroll
// speed while drag-selecting near the top/bottom of the viewport. None of that
// needs a separate language runtime - it's pure, allocation-light logic, so it
// lives here as ordinary inline C++ with no FFI boundary, no CString round
// trips

#include <algorithm>
#include <cstdint>
#include <string>

namespace textor_keymap {

enum class LayoutMode : uint8_t { Latin, Cyrillic };

// Classified editor shortcuts
enum class ShortcutAction : uint32_t {
    NoAction     = 0,  // renamed from None: X11/X.h #defines None as 0L
    InsertText   = 1,
    DeleteBackward = 2,
    DeleteForward  = 3,
    Enter        = 4,
    Tab          = 5,
    MoveLeft     = 6,
    MoveRight    = 7,
    MoveUp       = 8,
    MoveDown     = 9,
    SelectAll    = 10,
    Copy         = 11,
    Cut          = 12,
    Paste        = 13,
    Undo         = 14,
    Redo         = 15,
    Find         = 16,
    FindProject  = 17,
    Save         = 18,
};

// Maps a base-layout (Latin) keysym to the corresponding character for the
// requested layout, applying Shift where relevant. Returns an empty string if
// the keysym has no mapped text (e.g. function keys, arrows)
inline std::string map_key_to_text(uint64_t keysym, uint64_t state, LayoutMode mode) {
    const bool shift = (state & 0x1) != 0;

    const char *latin = nullptr;
    switch (keysym) {
        case 0x0061: latin = "a"; break; case 0x0062: latin = "b"; break;
        case 0x0063: latin = "c"; break; case 0x0064: latin = "d"; break;
        case 0x0065: latin = "e"; break; case 0x0066: latin = "f"; break;
        case 0x0067: latin = "g"; break; case 0x0068: latin = "h"; break;
        case 0x0069: latin = "i"; break; case 0x006a: latin = "j"; break;
        case 0x006b: latin = "k"; break; case 0x006c: latin = "l"; break;
        case 0x006d: latin = "m"; break; case 0x006e: latin = "n"; break;
        case 0x006f: latin = "o"; break; case 0x0070: latin = "p"; break;
        case 0x0071: latin = "q"; break; case 0x0072: latin = "r"; break;
        case 0x0073: latin = "s"; break; case 0x0074: latin = "t"; break;
        case 0x0075: latin = "u"; break; case 0x0076: latin = "v"; break;
        case 0x0077: latin = "w"; break; case 0x0078: latin = "x"; break;
        case 0x0079: latin = "y"; break; case 0x007a: latin = "z"; break;
        case 0x005b: latin = "["; break; case 0x005d: latin = "]"; break;
        case 0x003b: latin = ";"; break; case 0x0027: latin = "'"; break;
        case 0x002c: latin = ","; break; case 0x002e: latin = "."; break;
        case 0x002f: latin = "/"; break; case 0x0030: latin = "0"; break;
        case 0x0031: latin = "1"; break; case 0x0032: latin = "2"; break;
        case 0x0033: latin = "3"; break; case 0x0034: latin = "4"; break;
        case 0x0035: latin = "5"; break; case 0x0036: latin = "6"; break;
        case 0x0037: latin = "7"; break; case 0x0038: latin = "8"; break;
        case 0x0039: latin = "9"; break; case 0x002d: latin = "-"; break;
        case 0x003d: latin = "="; break; case 0x005c: latin = "\\"; break;
        default: return {};
    }

    static const char *const kCyrLower[26] = {
        "ф","и","с","в","у","а","п","р","ш","о","л","д","ь",
        "т","щ","з","й","к","ы","е","г","м","ц","ч","н","я"
    };
    static const char *const kLatinUpper[26] = {
        "A","B","C","D","E","F","G","H","I","J","K","L","M",
        "N","O","P","Q","R","S","T","U","V","W","X","Y","Z"
    };
    static const char *const kCyrUpper[26] = {
        "Ф","И","С","В","У","А","П","Р","Ш","О","Л","Д","Ь",
        "Т","Щ","З","Й","К","Ы","Е","Г","М","Ц","Ч","Н","Я"
    };
    // Extra punctuation that swaps under the Cyrillic (ЙЦУКЕН) layout
    struct PunctMap { const char *lat; const char *cyr; };
    static const PunctMap kPunct[] = {
        {"[", "х"}, {"]", "ъ"}, {";", "ж"}, {"'", "э"}, {",", "б"}, {".", "ю"},
    };
    static const PunctMap kPunctShift[] = {
        {"х", "Х"}, {"ъ", "Ъ"}, {"ж", "Ж"}, {"э", "Э"}, {"б", "Б"}, {"ю", "Ю"},
    };

    std::string out = latin;
    if (mode == LayoutMode::Cyrillic) {
        if (latin[0] >= 'a' && latin[0] <= 'z' && latin[1] == '\0') {
            out = kCyrLower[latin[0] - 'a'];
        } else {
            for (auto &p : kPunct) {
                if (out == p.lat) { out = p.cyr; break; }
            }
        }
    }

    if (shift) {
        if (mode == LayoutMode::Cyrillic) {
            for (int i = 0; i < 26; ++i) {
                if (out == kCyrLower[i]) { out = kCyrUpper[i]; break; }
            }
            for (auto &p : kPunctShift) {
                if (out == p.lat) { out = p.cyr; break; }
            }
        } else {
            if (out.size() == 1 && out[0] >= 'a' && out[0] <= 'z') {
                out = kLatinUpper[out[0] - 'a'];
            }
        }
    }
    return out;
}

// Classifies a Ctrl/Shift/Alt chord into an editor-level shortcut action
// `state` bit 0x1 = ctrl, 0x2 = shift, 0x4 = alt (matches call sites)
inline ShortcutAction classify_shortcut(uint64_t keysym, uint64_t state) {
    const bool ctrl  = (state & 0x1) != 0;
    const bool shift = (state & 0x2) != 0;
    const bool alt   = (state & 0x4) != 0;

    if (alt || (!ctrl && !shift)) return ShortcutAction::NoAction;

    // Keysyms may arrive either as the plain ASCII code point or with the
    // legacy high-word tagging some callers used historically; accept both
    auto is_key = [&](uint64_t lower) {
        return keysym == lower || keysym == (lower << 32);
    };

    if (ctrl && !shift) {
        if (is_key(0x0066)) return ShortcutAction::Find;
        if (is_key(0x0061)) return ShortcutAction::SelectAll;
        if (is_key(0x0063)) return ShortcutAction::Copy;
        if (is_key(0x0078)) return ShortcutAction::Cut;
        if (is_key(0x0076)) return ShortcutAction::Paste;
        if (is_key(0x007a)) return ShortcutAction::Undo;
        if (is_key(0x0079)) return ShortcutAction::Redo;
        if (is_key(0x0073)) return ShortcutAction::Save;
    }
    if (ctrl && shift) {
        if (is_key(0x0066)) return ShortcutAction::FindProject;
        if (is_key(0x007a)) return ShortcutAction::Redo;
    }
    return ShortcutAction::NoAction;
}

// Returns the scroll delta (pixels/frame) to apply while the pointer is
// within `edge_margin` pixels of the top/bottom of [viewport_top, viewport_bottom]
// during a drag-select, easing from 0 at the margin edge to `edge_speed` at
// the very edge of the viewport
inline double edge_scroll_delta(int32_t pointer_y, int32_t viewport_top,
                                 int32_t viewport_bottom, int32_t edge_margin,
                                 double edge_speed) {
    const int32_t viewport_h = std::max(viewport_bottom - viewport_top, 1);
    int32_t y = pointer_y - viewport_top;
    if (y < 0) y = 0;

    if (y < edge_margin) {
        double factor = 1.0 - std::clamp((double)y / (double)edge_margin, 0.0, 1.0);
        return -edge_speed * factor;
    }
    if (y > viewport_h - edge_margin) {
        double factor = std::clamp(
            (double)(y - (viewport_h - edge_margin)) / (double)edge_margin, 0.0, 1.0);
        return edge_speed * factor;
    }
    return 0.0;
}

}

// ............ Thin free-function aliases ............
// Kept at global scope (rather than requiring `textor_keymap::` everywhere)
inline std::string textor_map_key_to_text(uint64_t keysym, uint64_t state, uint32_t mode) {
    return textor_keymap::map_key_to_text(
        keysym, state,
        mode == 1 ? textor_keymap::LayoutMode::Cyrillic : textor_keymap::LayoutMode::Latin);
}

inline uint32_t textor_classify_shortcut(uint64_t keysym, uint64_t state) {
    return static_cast<uint32_t>(textor_keymap::classify_shortcut(keysym, state));
}

inline double textor_edge_scroll_delta(int32_t pointer_y, int32_t viewport_top,
                                        int32_t viewport_bottom, int32_t edge_margin,
                                        double edge_speed) {
    return textor_keymap::edge_scroll_delta(pointer_y, viewport_top, viewport_bottom,
                                             edge_margin, edge_speed);
}

#endif
