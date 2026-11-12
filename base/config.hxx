#ifndef CONFIG_HXX
#define CONFIG_HXX

#include <string>
#include <map>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

// ............ Config directory: ~/.config/textor/ ............
static inline std::string textor_config_dir() {
    const char *home = std::getenv("HOME");
    if (!home || !home[0]) home = "/tmp";
    return std::string(home) + "/.config/textor";
}

// Ensure ~/.config/textor/ (and its sub-dirs) exist.
// Called once at editor startup.
static inline void ensure_textor_dirs() {
    std::error_code ec;
    std::string base = textor_config_dir();
    fs::create_directories(base,              ec);
    fs::create_directories(base + "/addons",  ec);
    fs::create_directories(base + "/themes",  ec);
}

static inline std::string textor_settings_path() {
    return textor_config_dir() + "/settings.conf";
}

static inline std::string textor_shortcuts_path() {
    return textor_config_dir() + "/shortcuts.conf";
}

static inline std::string textor_addons_dir() {
    return textor_config_dir() + "/addons";
}

static inline std::string textor_themes_dir() {
    return textor_config_dir() + "/themes";
}

// Simple key=value config loader/saver
static inline std::map<std::string, std::string> load_config(const std::string &path = "") {
    std::string p = path.empty() ? textor_settings_path() : path;
    std::map<std::string, std::string> m;
    std::ifstream ifs(p);
    if (!ifs) return m;
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        if (line[0] == '#') continue;
        auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        std::string k = line.substr(0, pos);
        std::string v = line.substr(pos + 1);
        auto trim = [](std::string &s) {
            while (!s.empty() && isspace((unsigned char)s.front())) s.erase(s.begin());
            while (!s.empty() && isspace((unsigned char)s.back())) s.pop_back();
        };
        trim(k); trim(v);
        if (!k.empty()) m[k] = v;
    }
    return m;
}

static inline bool save_config(const std::map<std::string, std::string> &m,
                                 const std::string &path = "") {
    std::string p = path.empty() ? textor_settings_path() : path;
    ensure_textor_dirs();

    std::vector<std::string> lines;
    std::map<std::string, size_t> key_line_index;
    {
        std::ifstream ifs(p);
        if (ifs) {
            std::string line;
            size_t idx = 0;
            while (std::getline(ifs, line)) {
                lines.push_back(line);
                std::string tmp = line;
                while (!tmp.empty() && isspace((unsigned char)tmp.front())) tmp.erase(tmp.begin());
                if (tmp.empty() || tmp[0] == '#') { ++idx; continue; }
                auto pos = tmp.find('=');
                if (pos != std::string::npos) {
                    std::string k = tmp.substr(0, pos);
                    while (!k.empty() && isspace((unsigned char)k.back())) k.pop_back();
                    while (!k.empty() && isspace((unsigned char)k.front())) k.erase(k.begin());
                    if (!k.empty()) key_line_index[k] = idx;
                }
                ++idx;
            }
        }
    }

    std::vector<std::string> out_lines = lines;
    for (const auto &kv : m) {
        const std::string &k = kv.first;
        const std::string &v = kv.second;
        auto it = key_line_index.find(k);
        std::string new_line = k + "=" + v;
        if (it != key_line_index.end()) {
            out_lines[it->second] = new_line;
        }
    }
    for (const auto &kv : m) {
        const std::string &k = kv.first;
        if (key_line_index.find(k) == key_line_index.end()) {
            out_lines.push_back(k + "=" + kv.second);
        }
    }
    if (out_lines.empty()) {
        for (const auto &kv : m) out_lines.push_back(kv.first + "=" + kv.second);
    }

    std::ofstream ofs(p, std::ofstream::trunc);
    if (!ofs) return false;
    for (const auto &l : out_lines) ofs << l << "\n";
    return true;
}

#endif
