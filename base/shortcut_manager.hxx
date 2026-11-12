#ifndef SHORTCUT_MANAGER_HXX
#define SHORTCUT_MANAGER_HXX

//  ShortcutManager  -  configurable keybindings for IDE
//
//  * Each action has a name, a default chord, and a display string.
//  * Bindings are stored in shortcuts.conf as:
//       action_name=CTRL+SHIFT+S
//  * Parsing: tokens split by '+', last token is key name (KeySym lookup).

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include "config.hxx"

struct Shortcut {
    std::string action;      // internal name, e.g. "save"
    std::string display;     // e.g. "Save File"
    bool ctrl  = false;
    bool shift = false;
    bool alt   = false;
    KeySym key = 0;          // X11 KeySym
    std::string key_name;    // e.g. "S"

    // Human-readable chord string, e.g. "Ctrl+S"
    std::string chord() const {
        std::string s;
        if (ctrl)  s += "Ctrl+";
        if (shift) s += "Shift+";
        if (alt)   s += "Alt+";
        s += key_name;
        return s;
    }
};

// Parse a chord string like "CTRL+SHIFT+S" into booleans + KeySym
static inline bool parse_chord(const std::string &chord,
                                bool &ctrl, bool &shift, bool &alt,
                                KeySym &ks, std::string &key_name)
{
    ctrl = shift = alt = false;
    ks = 0;
    key_name.clear();

    std::vector<std::string> tokens;
    std::stringstream ss(chord);
    std::string tok;
    while (std::getline(ss, tok, '+')) {
        std::string u = tok;
        for (auto &c : u) c = (char)std::toupper((unsigned char)c);
        tokens.push_back(u);
    }
    if (tokens.empty()) return false;

    for (int i = 0; i < (int)tokens.size() - 1; ++i) {
        if (tokens[i] == "CTRL"  || tokens[i] == "CONTROL") ctrl  = true;
        if (tokens[i] == "SHIFT")                            shift = true;
        if (tokens[i] == "ALT")                              alt   = true;
    }

    key_name = tokens.back();
    // Map common names to KeySyms
    static const std::map<std::string, KeySym> kmap = {
        {"S",           XK_s},
        {"W",           XK_w},
        {"Q",           XK_q},
        {"A",           XK_a},
        {"C",           XK_c},
        {"X",           XK_x},
        {"V",           XK_v},
        {"D",           XK_d},
        {"F",           XK_f},
        {"G",           XK_g},
        {"H",           XK_h},
        {"N",           XK_n},
        {"O",           XK_o},
        {"P",           XK_p},
        {"R",           XK_r},
        {"T",           XK_t},
        {"U",           XK_u},
        {"Y",           XK_y},
        {"Z",           XK_z},
        {"TAB",         XK_Tab},
        {"RETURN",      XK_Return},
        {"ENTER",       XK_Return},
        {"ESCAPE",      XK_Escape},
        {"ESC",         XK_Escape},
        {"F1",          XK_F1},
        {"F2",          XK_F2},
        {"F3",          XK_F3},
        {"F4",          XK_F4},
        {"F5",          XK_F5},
        {"F6",          XK_F6},
        {"F7",          XK_F7},
        {"F8",          XK_F8},
        {"F9",          XK_F9},
        {"F10",         XK_F10},
        {"F11",         XK_F11},
        {"F12",         XK_F12},
        {"PAGEUP",      XK_Prior},
        {"PAGEDOWN",    XK_Next},
        {"HOME",        XK_Home},
        {"END",         XK_End},
        {"DELETE",      XK_Delete},
        {"BACKSPACE",   XK_BackSpace},
        {"INSERT",      XK_Insert},
        {"UP",          XK_Up},
        {"DOWN",        XK_Down},
        {"LEFT",        XK_Left},
        {"RIGHT",       XK_Right},
        {"SPACE",       XK_space},
        {"/",           XK_slash},
        {"\\",          XK_backslash},
        {",",           XK_comma},
        {".",           XK_period},
        {";",           XK_semicolon},
        {"'",           XK_apostrophe},
        {"`",           XK_grave},
        {"-",           XK_minus},
        {"=",           XK_equal},
        {"[",           XK_bracketleft},
        {"]",           XK_bracketright},
    };

    auto it = kmap.find(key_name);
    if (it != kmap.end()) { ks = it->second; return true; }
    // Single lowercase letter
    if (key_name.size() == 1) {
        ks = (KeySym)std::tolower((unsigned char)key_name[0]);
        return ks != 0;
    }
    return false;
}

struct ShortcutManager {

    std::vector<Shortcut> shortcuts;

    // ............ Default bindings ............
    ShortcutManager() {
        defaults();
    }

    void defaults() {
        shortcuts.clear();
        auto add = [&](const char *action, const char *display,
                       bool ctrl, bool shift, bool alt, KeySym ks, const char *kn) {
            Shortcut sc;
            sc.action = action; sc.display = display;
            sc.ctrl = ctrl; sc.shift = shift; sc.alt = alt;
            sc.key = ks; sc.key_name = kn;
            shortcuts.push_back(sc);
        };
        add("save",            "Save File",             true,  false, false, XK_s,     "S");
        add("close_tab",       "Close Tab",             true,  false, false, XK_w,     "W");
        add("next_tab",        "Next Tab",              true,  false, false, XK_Tab,   "Tab");
        add("prev_tab",        "Previous Tab",          true,  true,  false, XK_Tab,   "Tab");
        add("select_all",      "Select All",            true,  false, false, XK_a,     "A");
        add("copy",            "Copy",                  true,  false, false, XK_c,     "C");
        add("cut",             "Cut",                   true,  false, false, XK_x,     "X");
        add("paste",           "Paste",                 true,  false, false, XK_v,     "V");
        add("duplicate_line",  "Duplicate Line",        true,  false, false, XK_d,     "D");
        add("toggle_console",  "Toggle Console",        true,  false, false, XK_grave, "`");
        add("settings",        "Open Settings",         true,  false, false, XK_comma, ",");
        add("find",            "Find in File",          true,  false, false, XK_f,     "F");
        add("toggle_sidebar",  "Toggle Sidebar",        true,  false, false, XK_b,     "B");
        add("new_file",        "New File",              true,  false, false, XK_n,     "N");
        add("open_file",       "Open File",             true,  false, false, XK_o,     "O");
        add("quit",            "Quit",                  true,  false, false, XK_q,     "Q");
        add("indent",          "Indent Line",           false, false, false, XK_Tab,   "Tab");
        add("autocomplete",    "Trigger Autocomplete",  false, false, false, XK_space, "Space");
        add("scroll_to_cursor","Scroll to Cursor",      true,  false, false, XK_l,     "L");
        add("command_palette", "Command Palette",       true,  true,  false, XK_p,     "P");
        add("quick_open",      "Go to Open File",       true,  false, false, XK_p,     "P");

        // ............ Full keyboard control ............
        add("focus_mode",      "Toggle Focus Mode",     false, false, false, XK_F11,   "F11");
        add("focus_sidebar",   "Focus Sidebar",         true,  false, false, XK_1,     "1");
        add("focus_editor",    "Focus Editor",          true,  false, false, XK_2,     "2");
        add("focus_tabs",      "Focus Tab Panel",       true,  false, false, XK_3,     "3");
        add("focus_console",   "Focus Console",         true,  false, false, XK_4,     "4");
        add("move_focus_next", "Next Panel",            true,  false, true,  XK_Right, "Right");
        add("move_focus_prev", "Previous Panel",        true,  false, true,  XK_Left,  "Left");
        add("activate",        "Activate / Open",       false, false, false, XK_Return,"Return");
        add("collapse",        "Collapse",              false, false, false, XK_Left,  "Left");
        add("expand",          "Expand",                false, false, false, XK_Right, "Right");
        add("navigate_up",     "Navigate Up",           false, false, false, XK_Up,    "Up");
        add("navigate_down",   "Navigate Down",         false, false, false, XK_Down,  "Down");
    }

    Shortcut* find(const std::string &action) {
        for (auto &sc : shortcuts)
            if (sc.action == action) return &sc;
        return nullptr;
    }

    // ............ Match test ............
    bool matches(const std::string &action, XKeyEvent *kev) const {
        if (!kev) return false;
        bool ctrl  = (kev->state & ControlMask) != 0;
        bool shift = (kev->state & ShiftMask)   != 0;
        bool alt   = (kev->state & Mod1Mask)    != 0;
        KeySym ks  = XLookupKeysym(kev, 0);
        // Also check lowercase variant
        KeySym ksl = (ks >= 'A' && ks <= 'Z') ? (KeySym)(ks + 32) : ks;

        for (const auto &sc : shortcuts) {
            if (sc.action != action) continue;
            if (sc.ctrl  != ctrl)  continue;
            if (sc.shift != shift) continue;
            if (sc.alt   != alt)   continue;
            if (sc.key == ks || sc.key == ksl) return true;
        }
        return false;
    }

    // Matches without requiring exact shift state (for letters that have both)
    bool matches_key(const std::string &action, bool ctrl, bool shift, KeySym ks) const {
        KeySym ksl = (ks >= 'A' && ks <= 'Z') ? (KeySym)(ks + 32) : ks;
        for (const auto &sc : shortcuts) {
            if (sc.action != action) continue;
            if (sc.ctrl  != ctrl)  continue;
            if (sc.alt   != false) continue;
            if (sc.key == ks || sc.key == ksl) return true;
        }
        return false;
    }

    // ............ Persistence ............
    void load(const std::string &path = "") {
        std::string p = path.empty() ? textor_shortcuts_path() : path;
        // Delegate to internal impl
        load_from(p);
    }
    void load_from(const std::string &path) {
        std::ifstream ifs(path);
        if (!ifs) return;
        std::string line;
        while (std::getline(ifs, line)) {
            if (line.empty() || line[0] == '#') continue;
            auto pos = line.find('=');
            if (pos == std::string::npos) continue;
            std::string k = line.substr(0, pos);
            std::string v = line.substr(pos + 1);
            // trim
            while (!k.empty() && isspace((unsigned char)k.back()))  k.pop_back();
            while (!k.empty() && isspace((unsigned char)k.front())) k.erase(k.begin());
            while (!v.empty() && isspace((unsigned char)v.back()))  v.pop_back();
            while (!v.empty() && isspace((unsigned char)v.front())) v.erase(v.begin());

            for (auto &sc : shortcuts) {
                if (sc.action == k) {
                    bool c2, s2, a2; KeySym ks2; std::string kn2;
                    if (parse_chord(v, c2, s2, a2, ks2, kn2)) {
                        sc.ctrl = c2; sc.shift = s2; sc.alt = a2;
                        sc.key = ks2; sc.key_name = kn2;
                    }
                    break;
                }
            }
        }
    }

    void save(const std::string &path = "") const {
        std::string p = path.empty() ? textor_shortcuts_path() : path;
        ensure_textor_dirs();
        std::ofstream ofs(p, std::ofstream::trunc);
        if (!ofs) return;
        ofs << "# Textor IDE - Keyboard Shortcuts\n";
        ofs << "# Format: action_name=CTRL+SHIFT+KEY\n\n";
        for (const auto &sc : shortcuts) {
            ofs << sc.action << "=" << sc.chord() << "\n";
        }
    }
};

#endif
