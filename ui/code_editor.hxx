#ifndef CODE_EDITOR_HXX
#define CODE_EDITOR_HXX

#include <string>
#include "../base/keytext.hxx"
#include <vector>
#include <deque>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <unordered_set>
#include <future>
#include <mutex>
#include <atomic>
#include <pango/pangocairo.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include "../util/dispatch.hxx"

struct ThemeVariant;
// ScrollBar forward-declaration removed: defined in morphing.hxx
struct SettingsModal;
#include "morphing.hxx"
#include "autocomplete.hxx"
#include "../base/addon_loader.hxx"
#include "../base/globals.hxx"
#include "../util/rope.hxx"
#include "../base/keymap.hxx"

//  SyntaxHighlighter - tokenize a line and produce PangoAttrList
enum class TokenKind { Normal, Keyword, Type, Function, String, Number, Comment, Preproc, Operator };

struct SyntaxHighlighter {

    // Language enum (declared first - used in scan_ml_state default arg)
    enum class Lang { Unknown, C, Cpp, D, Rust, Python, Shell, Lua, Mekanism, Kotlin, Config, Markdown };

    // Human-readable label for the language-bar / status text.
    static const char* display_name(Lang l) {
        switch (l) {
            case Lang::C:        return "C";
            case Lang::Cpp:      return "C++";
            case Lang::D:        return "D";
            case Lang::Rust:     return "Rust";
            case Lang::Python:   return "Python";
            case Lang::Shell:    return "Shell";
            case Lang::Lua:      return "Lua";
            case Lang::Mekanism: return "Mekanism";
            case Lang::Kotlin:   return "Kotlin";
            case Lang::Config:   return "Config";
            case Lang::Markdown: return "Markdown";
            default:             return "Text";
        }
    }

    // Keyword sets
    static const std::unordered_set<std::string>& cpp_keywords() {
        static std::unordered_set<std::string> s {
            "auto","break","case","catch","class","const","constexpr","continue",
            "default","delete","do","else","enum","explicit","extern","false","final",
            "for","friend","goto","if","inline","mutable","namespace","new","noexcept",
            "nullptr","operator","override","private","protected","public","register",
            "return","sizeof","static","static_assert","static_cast","struct","switch",
            "template","this","throw","true","try","typedef","typeid","typename",
            "union","using","virtual","volatile","while","__attribute__"
        };
        return s;
    }

    static const std::unordered_set<std::string>& cpp_types() {
        static std::unordered_set<std::string> s {
            "bool","char","double","float","int","long","short","signed","unsigned",
            "void","wchar_t","char8_t","char16_t","char32_t","size_t","ptrdiff_t",
            "int8_t","int16_t","int32_t","int64_t","uint8_t","uint16_t","uint32_t",
            "uint64_t","string","wstring","u16string","u32string","vector","map",
            "unordered_map","set","unordered_set","pair","tuple","optional","variant",
            "shared_ptr","unique_ptr","weak_ptr","array","list","deque"
        };
        return s;
    }

    static const std::unordered_set<std::string>& d_keywords() {
        static std::unordered_set<std::string> s {
            "abstract","alias","align","asm","assert","auto","body","break","case",
            "cast","catch","class","const","continue","debug","default","delegate",
            "delete","deprecated","do","else","enum","export","extern","false","final",
            "finally","for","foreach","foreach_reverse","function","goto","if","immutable",
            "import","in","inout","interface","invariant","is","lazy","mixin","module",
            "new","nothrow","null","out","override","package","pragma","private",
            "protected","public","pure","ref","return","scope","shared","static",
            "struct","super","switch","synchronized","template","this","throw","true",
            "try","typedef","typeid","typeof","union","unittest","version","void",
            "volatile","while","with","__FILE__","__LINE__","__DATE__","__TIME__",
            "__FUNCTION__","__PRETTY_FUNCTION__","@safe","@trusted","@system",
            "@nogc","@property","@disable","@live"
        };
        return s;
    }

    static const std::unordered_set<std::string>& d_types() {
        static std::unordered_set<std::string> s {
            "bool","byte","ubyte","short","ushort","int","uint","long","ulong",
            "cent","ucent","float","double","real","ifloat","idouble","ireal",
            "cfloat","cdouble","creal","char","wchar","dchar","string","wstring",
            "dstring","size_t","ptrdiff_t","hash_t","equals_t"
        };
        return s;
    }

    static const std::unordered_set<std::string>& rust_keywords() {
        static std::unordered_set<std::string> s {
            "as","async","await","break","const","continue","crate","dyn","else",
            "enum","extern","false","fn","for","if","impl","in","let","loop","match",
            "mod","move","mut","pub","ref","return","self","Self","static","struct",
            "super","trait","true","type","union","unsafe","use","where","while",
            "abstract","become","box","do","final","macro","override","priv",
            "try","typeof","unsized","virtual","yield"
        };
        return s;
    }

    static const std::unordered_set<std::string>& rust_types() {
        static std::unordered_set<std::string> s {
            "i8","i16","i32","i64","i128","isize","u8","u16","u32","u64","u128",
            "usize","f32","f64","bool","char","str","String","Vec","Box","Option",
            "Result","Ok","Err","Some","None","HashMap","HashSet","Arc","Rc","Cell",
            "RefCell","Mutex","RwLock"
        };
        return s;
    }

    static const std::unordered_set<std::string>& py_keywords() {
        static std::unordered_set<std::string> s {
            "False","None","True","and","as","assert","async","await","break","class",
            "continue","def","del","elif","else","except","finally","for","from",
            "global","if","import","in","is","lambda","nonlocal","not","or","pass",
            "raise","return","try","while","with","yield","print","len","range",
            "int","float","str","list","dict","set","tuple","bool","type"
        };
        return s;
    }

    static const std::unordered_set<std::string>& lua_keywords() {
        static std::unordered_set<std::string> s {
            "and","break","do","else","elseif","end","false","for","function",
            "goto","if","in","local","nil","not","or","repeat","return","then",
            "true","until","while"
        };
        return s;
    }

    static const std::unordered_set<std::string>& lua_types() {
        static std::unordered_set<std::string> s {
            "print","pairs","ipairs","next","type","tostring","tonumber","unpack",
            "rawget","rawset","rawequal","rawlen","select","error","assert",
            "pcall","xpcall","setmetatable","getmetatable","require","load",
            "loadfile","dofile","collectgarbage","table","string","math","io",
            "os","coroutine","package","debug","utf8","bit32",
            "insert","remove","sort","concat","format","gsub","match","gmatch",
            "find","byte","char","sub","len","upper","lower","rep","reverse",
            "abs","ceil","floor","sqrt","sin","cos","tan","max","min","random",
            "huge","pi"
        };
        return s;
    }

    static const std::unordered_set<std::string>& kotlin_keywords() {
        static std::unordered_set<std::string> s {
            "abstract","actual","annotation","as","break","by","catch","class",
            "companion","const","constructor","continue","crossinline","data",
            "delegate","do","dynamic","else","enum","expect","external","false",
            "field","file","final","finally","for","fun","get","if","import",
            "in","infix","init","inline","inner","interface","internal",
            "is","it","lateinit","noinline","null","object","open","operator",
            "out","override","package","param","private","property","protected",
            "public","receiver","reified","return","sealed","set","setparam",
            "super","suspend","tailrec","this","throw","true","try","typealias",
            "typeof","val","value","var","vararg","when","where","while"
        };
        return s;
    }

    static const std::unordered_set<std::string>& kotlin_types() {
        static std::unordered_set<std::string> s {
            "Any","Array","Boolean","BooleanArray","Byte","ByteArray","Char",
            "CharArray","Comparable","Double","DoubleArray","Float","FloatArray",
            "Function","Int","IntArray","Iterable","Iterator","List","Long",
            "LongArray","Map","MutableList","MutableMap","MutableSet","Nothing",
            "Number","Pair","Set","Short","ShortArray","String","Triple","Unit",
            "println","print","readLine","arrayOf","listOf","mapOf","setOf",
            "mutableListOf","mutableMapOf","mutableSetOf","emptyList","emptyMap",
            "emptySet","run","let","also","apply","with","takeIf","takeUnless",
            "lazy","TODO","error","check","require","assert"
        };
        return s;
    }

    static const std::unordered_set<std::string>& shell_keywords() {
        static std::unordered_set<std::string> s {
            "if","then","else","elif","fi","for","while","do","done","case",
            "esac","in","function","return","exit","export","local","readonly",
            "declare","unset","shift","source","alias","true","false","break",
            "continue","until","select","time","coproc","test"
        };
        return s;
    }

    static const std::unordered_set<std::string>& shell_types() {
        static std::unordered_set<std::string> s {
            "echo","printf","read","cd","ls","mkdir","rm","cp","mv","cat","grep",
            "sed","awk","find","sort","uniq","wc","cut","tr","head","tail","curl",
            "wget","chmod","chown","kill","ps","pwd","date","env","set","eval",
            "exec","which","type","hash","command","builtin","enable"
        };
        return s;
    }

    // Fast ML-comment state scanner - no Pango, thread-safe
    // Returns the multi-line comment state AFTER scanning `line`
    // Used by the background thread to rebuild ml_comment_state cheaply
    static bool scan_ml_state(const std::string &line, bool in_ml,
                               const LangAddon *addon = nullptr,
                               Lang lang = Lang::Unknown)
    {
        const int n = (int)line.size();
        bool out_ml = in_ml;

        if (addon && !addon->comment_block_start.empty() && !addon->comment_block_end.empty()) {
            const std::string &cbs = addon->comment_block_start;
            const std::string &cbe = addon->comment_block_end;
            for (int i = 0; i < n; ) {
                if (out_ml) {
                    if (i+(int)cbe.size() <= n &&
                        line.compare(i,(int)cbe.size(),cbe)==0)
                    { i += (int)cbe.size(); out_ml = false; }
                    else ++i;
                } else {
                    if (i+(int)cbs.size() <= n &&
                        line.compare(i,(int)cbs.size(),cbs)==0)
                    { out_ml = true; i += (int)cbs.size(); }
                    else ++i;
                }
            }
            return out_ml;
        }

        // Lua: --[[ ... ]]  block comment
        if (lang == Lang::Lua) {
            for (int i = 0; i < n; ) {
                if (out_ml) {
                    if (i+1<n && line[i]==']' && line[i+1]==']') { out_ml=false; i+=2; }
                    else ++i;
                } else {
                    if (i+3<n && line[i]=='-'&&line[i+1]=='-'&&line[i+2]=='['&&line[i+3]=='[')
                    { out_ml=true; i+=4; }
                    else {
                        // line comment --
                        if (i+1<n && line[i]=='-' && line[i+1]=='-') break;
                        ++i;
                    }
                }
            }
            return out_ml;
        }

        // C-style /* ... */
        bool in_str = false; char str_ch = 0;
        for (int i = 0; i < n; ) {
            char c = line[i];
            if (in_str) {
                if (c == str_ch && (i==0 || line[i-1] != '\\')) in_str = false;
                ++i; continue;
            }
            if (out_ml) {
                if (i+1<n && c=='*' && line[i+1]=='/') { out_ml=false; i+=2; }
                else ++i;
                continue;
            }
            if (c == '"' || c == '\'') { in_str=true; str_ch=c; ++i; continue; }
            if (c=='/' && i+1<n) {
                if (line[i+1]=='/') break;
                if (line[i+1]=='*') { out_ml=true; i+=2; continue; }
            }
            ++i;
        }
        return out_ml;
    }

    // Language detection from filename
    static Lang detect(const std::string &filename) {
        auto ext = [&]() -> std::string {
            auto p = filename.rfind('.');
            if (p == std::string::npos) return "";
            std::string e = filename.substr(p+1);
            for (auto &c : e) c = (char)std::tolower((unsigned char)c);
            return e;
        }();

        // Extension -> language. Built once; every subsequent call is a
        // single hash lookup instead of walking an if/else-if ladder
        static const dispatch::FastDispatcher<std::string, Lang> by_ext = [] {
            dispatch::FastDispatcher<std::string, Lang> t(Lang::Unknown);
            t.add_case("c",    Lang::C)  .add_case("h",    Lang::C);
            t.add_case("cpp",  Lang::Cpp).add_case("cxx",  Lang::Cpp)
             .add_case("cc",   Lang::Cpp).add_case("hxx",  Lang::Cpp)
             .add_case("hpp",  Lang::Cpp).add_case("hh",   Lang::Cpp);
            t.add_case("d",    Lang::D);
            t.add_case("rs",   Lang::Rust);
            t.add_case("py",   Lang::Python);
            t.add_case("sh",   Lang::Shell).add_case("bash", Lang::Shell)
             .add_case("zsh",  Lang::Shell);
            t.add_case("lua",  Lang::Lua);
            t.add_case("kt",   Lang::Kotlin).add_case("kts", Lang::Kotlin);
            t.add_case("adn",  Lang::Config).add_case("conf", Lang::Config)
             .add_case("cool", Lang::Config); // Textor's own theme-file format, not the COOL language
            t.add_case("mek",  Lang::Mekanism);
            t.add_case("md",   Lang::Markdown).add_case("markdown", Lang::Markdown);
            return t;
        }();

        Lang found = by_ext.dispatch(ext);
        if (found != Lang::Unknown) return found;

        // Also detect BUILD.mek by full filename (no extension involved).
        {
            auto slash = filename.rfind('/');
            std::string base = (slash != std::string::npos) ? filename.substr(slash+1) : filename;
            if (base == "BUILD.mek") return Lang::Mekanism;
        }
        // Files with no recognized extension (including extensionless
        // scripts, e.g. a shebang-only "myscript") fall back to Shell/bash
        // highlighting by default rather than plain text.
        if (ext.empty()) return Lang::Shell;
        return Lang::Unknown;
    }

    // Colour helpers: pack RGBA into Pango guint16
    static PangoAttrList* build_attrs(const std::string &line, Lang lang,
                                      const ThemeVariant &tv,
                                      bool in_ml_comment, bool &out_ml_comment,
                                      const LangAddon *addon = nullptr)
    {
        PangoAttrList *al = pango_attr_list_new();
        out_ml_comment = in_ml_comment;

        if (lang == Lang::Unknown) return al;

        // BUILD.mek syntax
        if (lang == Lang::Mekanism) {
            const int n = (int)line.size();
            auto add_color = [&](int s, int e, double r, double g, double b) {
                if (s >= e) return;
                auto *a = pango_attr_foreground_new(
                    (guint16)(r*65535),(guint16)(g*65535),(guint16)(b*65535));
                a->start_index=(guint)s; a->end_index=(guint)e;
                pango_attr_list_insert(al,a);
            };
            // Keyword sets (v1.3+)
            // Control-flow: var func target switch case default
            static const std::unordered_set<std::string> mek_kw={
                "var","func","target",
                "switch","case","default"
            };
            // Built-in calls: console(...)
            static const std::unordered_set<std::string> mek_builtins={
                "console"
            };
            // Property keys (function colour when followed by colon)
            static const std::unordered_set<std::string> mek_props={
                "sources","outputs","repo","use","build_env","filelist",
                "git_cmds","deps","flags","cc","cxx","ar","ld"
            };
            int i=0;
            // Comment fast-path
            while(i<n && (line[i]==' '||line[i]=='	')) ++i;
            if(i<n && line[i]=='#'){
                add_color(0,n,tv.comment_r,tv.comment_g,tv.comment_b); return al;
            }
            // Full scan
            i=0;
            while(i<n){
                char c=line[i];
                // comment
                if(c=='#'){add_color(i,n,tv.comment_r,tv.comment_g,tv.comment_b);break;}
                // string literal
                if(c=='"'){
                    int j=i+1;
                    while(j<n && line[j]!='"'){if(line[j]=='\\')j++;j++;}
                    if(j<n)j++;
                    add_color(i,j,tv.string_r,tv.string_g,tv.string_b);
                    i=j; continue;
                }
                // interpolation {var:xxx} {filelist} {outputN}
                if(c=='{'){
                    int j=i+1;
                    while(j<n && line[j]!='}') j++;
                    if(j<n)j++;
                    add_color(i,j,tv.accent_r,tv.accent_g,tv.accent_b);
                    i=j; continue;
                }
                // identifier / keyword
                if(std::isalpha((unsigned char)c)||c=='_'){
                    int j=i;
                    // allow dot for cxx.detect style tokens
                    while(j<n&&(std::isalnum((unsigned char)line[j])||line[j]=='_'||line[j]=='.'))j++;
                    std::string tok=line.substr(i,j-i);
                    int k=j; while(k<n&&line[k]==' ')k++;
                    bool followed_by_colon=(k<n&&line[k]==':');
                    bool followed_by_paren=(k<n&&line[k]=='(');
                    if(mek_kw.count(tok))
                        add_color(i,j,tv.keyword_r,tv.keyword_g,tv.keyword_b);
                    else if(mek_builtins.count(tok)&&followed_by_paren)
                        add_color(i,j,tv.function_r,tv.function_g,tv.function_b);
                    else if(mek_props.count(tok)&&followed_by_colon)
                        add_color(i,j,tv.function_r,tv.function_g,tv.function_b);
                    else if(followed_by_colon)
                        add_color(i,j,tv.accent_r*0.8+0.1,tv.accent_g*0.9,tv.accent_b);
                    i=j; continue;
                }
                // parentheses (muted)
                if(c=='('||c==')'){
                    double ov=std::min(1.0,tv.muted*1.5);
                    add_color(i,i+1,ov,ov,ov);
                }
                // colon (muted)
                if(c==':'){
                    double ov=std::min(1.0,tv.muted*1.4);
                    add_color(i,i+1,ov,ov,ov);
                }
                ++i;
            }
            return al;
        }

        // Config (.adn / .conf) - simple INI-style "[section]" / "key=value"
        // files, used by Textor's own addon manifests and config files
        if (lang == Lang::Config) {
            const int n = (int)line.size();
            auto add_color = [&](int s, int e, double r, double g, double b) {
                if (s >= e) return;
                auto *a = pango_attr_foreground_new(
                    (guint16)(r*65535),(guint16)(g*65535),(guint16)(b*65535));
                a->start_index=(guint)s; a->end_index=(guint)e;
                pango_attr_list_insert(al,a);
            };
            int i = 0;
            while (i < n && (line[i]==' ' || line[i]=='\t')) ++i;
            // Comment: '#' or ';' as the first non-blank character
            if (i < n && (line[i]=='#' || line[i]==';')) {
                add_color(0, n, tv.comment_r, tv.comment_g, tv.comment_b);
                return al;
            }
            // Section header: [section]
            if (i < n && line[i]=='[') {
                auto j = line.find(']', (size_t)i);
                int end = (j == std::string::npos) ? n : (int)j+1;
                add_color(i, end, tv.keyword_r, tv.keyword_g, tv.keyword_b);
                return al;
            }
            // key = value (also "key=value" with no spaces)
            auto eq_pos = line.find('=', (size_t)i);
            if (eq_pos != std::string::npos) {
                int eq = (int)eq_pos;
                int key_end = eq;
                while (key_end > i && (line[key_end-1]==' ' || line[key_end-1]=='\t')) --key_end;
                add_color(i, key_end, tv.function_r, tv.function_g, tv.function_b);
                add_color(eq, eq+1, tv.muted, tv.muted, tv.muted);
                int val_start = eq+1;
                while (val_start < n && (line[val_start]==' ' || line[val_start]=='\t')) ++val_start;
                if (val_start < n) add_color(val_start, n, tv.string_r, tv.string_g, tv.string_b);
            }
            return al;
        }

        // Markdown (.md): headings, quotes, lists, `code`, **bold**, [link](url)
        if (lang == Lang::Markdown) {
            const int n = (int)line.size();
            auto add_color = [&](int s, int e, double r, double g, double b) {
                if (s >= e) return;
                auto *a = pango_attr_foreground_new(
                    (guint16)(r*65535),(guint16)(g*65535),(guint16)(b*65535));
                a->start_index=(guint)s; a->end_index=(guint)e;
                pango_attr_list_insert(al,a);
            };
            int i = 0;
            while (i < n && (line[i]==' ' || line[i]=='\t')) ++i;
            if (i >= n) return al;

            // Heading: # ... ######
            if (line[i]=='#') {
                int j = i;
                while (j < n && line[j]=='#') ++j;
                if (j - i <= 6 && (j == n || line[j]==' ')) {
                    add_color(i, n, tv.keyword_r, tv.keyword_g, tv.keyword_b);
                    return al;
                }
            }
            // Code fence line
            if (line.compare(i, 3, "```") == 0 || line.compare(i, 3, "~~~") == 0) {
                add_color(i, n, tv.string_r, tv.string_g, tv.string_b);
                return al;
            }
            // Quote
            if (line[i]=='>') {
                add_color(i, n, tv.comment_r, tv.comment_g, tv.comment_b);
                return al;
            }
            // Horizontal rule: --- *** ___
            if (line[i]=='-' || line[i]=='*' || line[i]=='_') {
                int count = 0; bool only = true;
                for (int k = i; k < n; ++k) {
                    if (line[k]==line[i]) ++count;
                    else if (line[k]!=' ') { only = false; break; }
                }
                if (only && count >= 3) {
                    add_color(i, n, tv.muted, tv.muted, tv.muted);
                    return al;
                }
            }
            // List marker: "- " "* " "+ " "1. "
            if ((line[i]=='-' || line[i]=='*' || line[i]=='+') && i+1 < n && line[i+1]==' ') {
                add_color(i, i+1, tv.accent_r, tv.accent_g, tv.accent_b);
                i += 2;
            } else if (std::isdigit((unsigned char)line[i])) {
                int j = i;
                while (j < n && std::isdigit((unsigned char)line[j])) ++j;
                if (j+1 < n && (line[j]=='.' || line[j]==')') && line[j+1]==' ') {
                    add_color(i, j+1, tv.accent_r, tv.accent_g, tv.accent_b);
                    i = j + 2;
                }
            }
            // Inline: `code`, **bold**, [text](url)
            while (i < n) {
                char c = line[i];
                if (c == '`') {
                    size_t j = line.find('`', (size_t)i + 1);
                    if (j != std::string::npos) {
                        add_color(i, (int)j + 1, tv.string_r, tv.string_g, tv.string_b);
                        i = (int)j + 1; continue;
                    }
                }
                if ((c=='*' || c=='_') && i+1 < n && line[i+1]==c) {
                    size_t j = line.find(std::string(2, c), (size_t)i + 2);
                    if (j != std::string::npos && (int)j > i + 2) {
                        add_color(i, (int)j + 2, tv.function_r, tv.function_g, tv.function_b);
                        i = (int)j + 2; continue;
                    }
                }
                if (c == '[') {
                    size_t j = line.find(']', (size_t)i + 1);
                    if (j != std::string::npos && j + 1 < (size_t)n && line[j+1]=='(') {
                        size_t k = line.find(')', j + 2);
                        if (k != std::string::npos) {
                            add_color(i, (int)j + 1, tv.accent_r, tv.accent_g, tv.accent_b);
                            add_color((int)j + 1, (int)k + 1, tv.comment_r, tv.comment_g, tv.comment_b);
                            i = (int)k + 1; continue;
                        }
                    }
                }
                ++i;
            }
            return al;
        }

        const int n = (int)line.size();

        // Helper: add colour span [s, e)
        auto add_color = [&](int s, int e, double r, double g, double b) {
            if (s >= e) return;
            auto *a = pango_attr_foreground_new(
                (guint16)(r * 65535), (guint16)(g * 65535), (guint16)(b * 65535));
            a->start_index = (guint)s;
            a->end_index   = (guint)e;
            pango_attr_list_insert(al, a);
        };

        /*
            Keyword/type set per language - was a 7-deep ternary ladder
            (twice: once for keywords, once for types), now a single array
            lookup built once. Falls back to the C/C++ sets, same as the
            ladder's final `: cpp_keywords()` / `: cpp_types()` did.
            reference_wrapper instead of a raw pointer - same "just an
            address" cost, but the table can't accidentally end up holding
            a null entry
        */
        using KwSetRef = std::reference_wrapper<const std::unordered_set<std::string>>;
        static const dispatch::JumpDispatcher<KwSetRef, 11> kw_table = [] {
            dispatch::JumpDispatcher<KwSetRef, 11> t(cpp_keywords());
            t.set_case(Lang::D,      d_keywords())
             .set_case(Lang::Rust,   rust_keywords())
             .set_case(Lang::Python, py_keywords())
             .set_case(Lang::Lua,    lua_keywords())
             .set_case(Lang::Kotlin, kotlin_keywords())
             .set_case(Lang::Shell,  shell_keywords());
            return t;
        }();
        static const dispatch::JumpDispatcher<KwSetRef, 11> ty_table = [] {
            dispatch::JumpDispatcher<KwSetRef, 11> t(cpp_types());
            t.set_case(Lang::D,      d_types())
             .set_case(Lang::Rust,   rust_types())
             .set_case(Lang::Lua,    lua_types())
             .set_case(Lang::Kotlin, kotlin_types())
             .set_case(Lang::Shell,  shell_types());
            return t;
        }();
        const auto &kws   = kw_table.dispatch(lang).get();
        const auto &types = ty_table.dispatch(lang).get();

        // State machine
        int i = 0;

        // If we're inside a block comment, scan until */ (or ]] for Lua)
        if (out_ml_comment) {
            int j = 0;
            bool is_lua_bc = (lang == Lang::Lua);
            while (j < n) {
                bool end_found = false;
                if (is_lua_bc) {
                    if (j+1 < n && line[j]==']' && line[j+1]==']') { j += 2; end_found = true; }
                } else {
                    if (j+1 < n && line[j]=='*' && line[j+1]=='/') { j += 2; end_found = true; }
                }
                if (end_found) {
                    out_ml_comment = false;
                    add_color(0, j, tv.comment_r, tv.comment_g, tv.comment_b);
                    i = j;
                    break;
                }
                ++j;
            }
            if (out_ml_comment) {
                add_color(0, n, tv.comment_r, tv.comment_g, tv.comment_b);
                return al;
            }
        }

        while (i < n) {
            char c = line[i];

            // Line comment  // or # or
            bool is_line_cmt = false;
            if (lang == Lang::Python || lang == Lang::Shell) {
                if (c == '#') is_line_cmt = true;
            } else if (lang == Lang::Lua) {
                // must NOT start [[ (handled as block comment below)
                if (i+1 < n && c=='-' && line[i+1]=='-') {
                    // peek: is it [[ ?
                    bool is_block = (i+3 < n && line[i+2]=='[' && line[i+3]=='[');
                    if (!is_block) is_line_cmt = true;
                }
            } else {
                if (i+1 < n && c=='/' && line[i+1]=='/') is_line_cmt = true;
            }
            if (is_line_cmt) {
                add_color(i, n, tv.comment_r, tv.comment_g, tv.comment_b);
                break;
            }

            // Lua block comment [[ ... ]]
            if (lang == Lang::Lua && i+3 < n && c=='-' && line[i+1]=='-' &&
                line[i+2]=='[' && line[i+3]=='[')
            {
                int j = i+4;
                bool closed = false;
                while (j+1 < n) {
                    if (line[j]==']' && line[j+1]==']') { j+=2; closed=true; break; }
                    ++j;
                }
                if (!closed) {
                    add_color(i, n, tv.comment_r, tv.comment_g, tv.comment_b);
                    out_ml_comment = true; break;
                }
                add_color(i, j, tv.comment_r, tv.comment_g, tv.comment_b);
                i = j; continue;
            }

            // Block comment /* ... */
            if (lang != Lang::Python && lang != Lang::Shell && lang != Lang::Lua &&
                i+1 < n && c=='/' && line[i+1]=='*')
            {
                int j = i+2;
                bool closed = false;
                while (j+1 < n) {
                    if (line[j]=='*' && line[j+1]=='/') { j+=2; closed=true; break; }
                    ++j;
                }
                if (!closed) { add_color(i, n, tv.comment_r, tv.comment_g, tv.comment_b); out_ml_comment = true; break; }
                add_color(i, j, tv.comment_r, tv.comment_g, tv.comment_b);
                i = j; continue;
            }

            // D nested /+ ... +/
            if (lang == Lang::D && i+1 < n && c=='/' && line[i+1]=='+') {
                int j = i+2, depth=1;
                while (j+1 < n && depth > 0) {
                    if (line[j]=='/' && line[j+1]=='+') { depth++; j+=2; }
                    else if (line[j]=='+' && line[j+1]=='/') { depth--; j+=2; }
                    else ++j;
                }
                add_color(i, std::min(j,n), tv.comment_r, tv.comment_g, tv.comment_b);
                i = j; continue;
            }

            // Preprocessor # (C/C++)
            if ((lang==Lang::C||lang==Lang::Cpp) && c=='#' && i==0) {
                // color entire line as preproc
                add_color(0, n, tv.keyword_r, tv.keyword_g * 0.85, tv.keyword_b * 0.5);
                break;
            }

            // String literals " or '
            if (c == '"' || c == '\'' || (lang==Lang::Python && c=='`') ||
                (lang!=Lang::Python && c=='`') || c=='`')
            {
                char q = c;
                // Python triple-quote - skip for simplicity, handle as single
                int j = i+1;
                while (j < n) {
                    if (line[j] == '\\') { j+=2; continue; }
                    if (line[j] == q)    { ++j; break; }
                    ++j;
                }
                add_color(i, j, tv.string_r, tv.string_g, tv.string_b);
                i = j; continue;
            }

            // D string r"..." or q"..."
            if (lang==Lang::D && (c=='r'||c=='q') && i+1<n && line[i+1]=='"') {
                int j = i+2;
                while (j < n && line[j] != '"') ++j;
                if (j < n) ++j;
                add_color(i, j, tv.string_r, tv.string_g, tv.string_b);
                i = j; continue;
            }

            // Numbers
            if (std::isdigit((unsigned char)c) ||
                (c=='0' && i+1<n && (line[i+1]=='x'||line[i+1]=='b')))
            {
                int j = i;
                while (j < n && (std::isalnum((unsigned char)line[j]) || line[j]=='.'||line[j]=='_')) ++j;
                add_color(i, j, tv.warning_r * 0.95, tv.warning_g * 0.85, tv.warning_b * 0.5);
                i = j; continue;
            }

            // Identifiers / keywords
            if (std::isalpha((unsigned char)c) || c=='_' ||
                (lang==Lang::D && c=='@'))
            {
                int j = i;
                // Skip '@' so the while loop reaches the identifier chars after it
                if (lang==Lang::D && j < n && line[j]=='@') ++j;
                while (j < n && (std::isalnum((unsigned char)line[j]) || line[j]=='_')) ++j;
                // Bare '@' with no identifier after it - just advance to avoid infinite loop
                if (j == i) { ++i; continue; }
                // tok spans '@' + identifier, matching entries like "@safe" in d_keywords
                std::string tok = line.substr(i, j - i);

                bool is_kw   = kws.count(tok) > 0;
                bool is_type = types.count(tok) > 0;

                // Heuristic: identifier followed by '(' -> function
                bool is_func = false;
                if (!is_kw && !is_type) {
                    int k = j;
                    while (k < n && line[k] == ' ') ++k;
                    if (k < n && line[k] == '(') is_func = true;
                }

                if (is_kw) {
                    double r = addon ? addon->kw_r : tv.keyword_r;
                    double g = addon ? addon->kw_g : tv.keyword_g;
                    double b = addon ? addon->kw_b : tv.keyword_b;
                    add_color(i, j, r, g, b);
                } else if (is_type) {
                    double r = addon ? addon->ty_r : tv.function_r * 0.85;
                    double g = addon ? addon->ty_g : tv.function_g;
                    double b = addon ? addon->ty_b : tv.function_b * 0.9;
                    add_color(i, j, r, g, b);
                } else if (is_func) {
                    add_color(i, j, tv.function_r, tv.function_g, tv.function_b);
                }
                // else normal text colour (no attribute = default)
                i = j; continue;
            }

            // Operators (highlight lightly)
            if (c=='+'||c=='-'||c=='*'||c=='/'||c=='='||c=='<'||c=='>'||
                c=='!'||c=='&'||c=='|'||c=='^'||c=='~'||c=='%'||c==':'||c=='?')
            {
                // subtle: slightly brighter than muted
                double ov = tv.muted * 1.3 > 1.0 ? 1.0 : tv.muted * 1.3;
                add_color(i, i+1, ov, ov, ov);
            }

            ++i;
        }

        return al;
    }

    // Highlighter for languages that come from an .adco addon (and for Adco itself):
    // one scanner driven by the tables of the LangAddon.
    static PangoAttrList* build_attrs_addon(const std::string &line, const LangAddon &addon,
                                            const ThemeVariant &tv,
                                            bool in_ml_comment, bool &out_ml_comment)
    {
        return build_attrs_generic(line, &addon, tv, in_ml_comment, out_ml_comment);
    }

    static PangoAttrList* build_attrs_generic(const std::string &line, const LangAddon *spec,
                                      const ThemeVariant &tv,
                                      bool in_ml_comment, bool &out_ml_comment)
    {
        PangoAttrList *al = pango_attr_list_new();
        out_ml_comment = false;
        const int n = (int)line.size();
        if (n == 0) return al;

        auto add_color = [&](int s, int e, double r, double g, double b) {
            if (s >= e) return;
            auto *a = pango_attr_foreground_new((guint16)(r*65535), (guint16)(g*65535), (guint16)(b*65535));
            a->start_index = (guint)s; a->end_index = (guint)e;
            pango_attr_list_insert(al, a);
        };

        // Base colour for everything the rules below don't touch.
        add_color(0, n, tv.text, tv.text, tv.text);
        if (!spec) return al;
        const LangAddon &L = *spec;

        auto is_ident_start = [](unsigned char c) { return std::isalpha(c) || c == '_'; };
        auto is_ident = [](unsigned char c) { return std::isalnum(c) || c == '_'; };

        const std::string &cline = L.comment_line;
        const std::string &cbs   = L.comment_block_start;
        const std::string &cbe   = L.comment_block_end;
        const bool has_block = !cbs.empty() && !cbe.empty();
        out_ml_comment = in_ml_comment;

        int i = 0;

        // Continue a block comment from the previous line
        if (out_ml_comment && has_block) {
            int j = 0; bool closed = false;
            while (j <= n - (int)cbe.size()) {
                if (line.compare(j, cbe.size(), cbe) == 0) { j += (int)cbe.size(); closed = true; break; }
                ++j;
            }
            if (!closed) { add_color(0, n, tv.comment_r, tv.comment_g, tv.comment_b); return al; }
            out_ml_comment = false;
            add_color(0, j, tv.comment_r, tv.comment_g, tv.comment_b);
            i = j;
        } else out_ml_comment = false;

        while (i < n) {
            const unsigned char c = (unsigned char)line[i];

            // Line comment
            if (!cline.empty() && line.compare(i, cline.size(), cline) == 0) {
                add_color(i, n, tv.comment_r, tv.comment_g, tv.comment_b);
                break;
            }
            // Block comment
            if (has_block && line.compare(i, cbs.size(), cbs) == 0) {
                int j = i + (int)cbs.size(); bool closed = false;
                while (j <= n - (int)cbe.size()) {
                    if (line.compare(j, cbe.size(), cbe) == 0) { j += (int)cbe.size(); closed = true; break; }
                    ++j;
                }
                if (!closed) { add_color(i, n, tv.comment_r, tv.comment_g, tv.comment_b); out_ml_comment = true; break; }
                add_color(i, j, tv.comment_r, tv.comment_g, tv.comment_b);
                i = j; continue;
            }
            // String literal (\( ... ) interpolation is highlighted for languages that use it)
            if (!L.string_chars.empty() && L.string_chars.find((char)c) != std::string::npos) {
                const char q = (char)c; int j = i + 1; int end = n;
                std::vector<std::pair<int,int>> interps;
                while (j < n) {
                    if (line[j] == '\\' && j + 1 < n) {
                        if (L.hex_colors && line[j + 1] == '(') {
                            int d = 1, k = j + 2;
                            while (k < n && d > 0) { if (line[k] == '(') d++; else if (line[k] == ')') d--; k++; }
                            interps.push_back({j, k});
                            j = k; continue;
                        }
                        j += 2; continue;
                    }
                    if (line[j] == q) { ++j; end = j; break; }
                    ++j;
                }
                add_color(i, end, tv.string_r, tv.string_g, tv.string_b);
                for (auto &ip : interps)
                    add_color(ip.first, std::min(ip.second, end), tv.accent2_r, tv.accent2_g, tv.accent2_b);
                i = end; continue;
            }
            // Colour literal  #rgb / #rrggbb / #rrggbbaa  -> drawn in its own colour
            if (L.hex_colors && c == '#') {
                int j = i + 1;
                while (j < n && std::isxdigit((unsigned char)line[j])) ++j;
                int len = j - i - 1;
                if ((len == 3 || len == 6 || len == 8) && (j >= n || !is_ident((unsigned char)line[j]))) {
                    auto hv = [](char ch) { return std::isdigit((unsigned char)ch) ? ch - '0' : (std::tolower(ch) - 'a' + 10); };
                    double r, g, b;
                    if (len == 3) { r = hv(line[i+1])*17/255.0; g = hv(line[i+2])*17/255.0; b = hv(line[i+3])*17/255.0; }
                    else { r = (hv(line[i+1])*16+hv(line[i+2]))/255.0; g = (hv(line[i+3])*16+hv(line[i+4]))/255.0; b = (hv(line[i+5])*16+hv(line[i+6]))/255.0; }
                    // keep the literal readable: nudge colours that vanish into the background toward the text colour
                    double luma = 0.2126*r + 0.7152*g + 0.0722*b;
                    if (std::fabs(luma - tv.bg) < 0.18) {
                        r = r*0.5 + tv.text*0.5; g = g*0.5 + tv.text*0.5; b = b*0.5 + tv.text*0.5;
                    }
                    add_color(i, j, r, g, b);
                    i = j; continue;
                }
            }
            // @attribute
            if (L.attr_prefix && c == (unsigned char)L.attr_prefix && i + 1 < n && is_ident_start((unsigned char)line[i+1])) {
                int j = i + 1; while (j < n && is_ident((unsigned char)line[j])) ++j;
                add_color(i, j, tv.accent2_r, tv.accent2_g, tv.accent2_b);
                i = j; continue;
            }
            // Number
            if (std::isdigit(c)) {
                int j = i;
                while (j < n && (std::isalnum((unsigned char)line[j]) || line[j] == '.' || line[j] == '_')) ++j;
                add_color(i, j, tv.warning_r*0.95, tv.warning_g*0.85, tv.warning_b*0.5);
                i = j; continue;
            }
            // Identifier
            if (is_ident_start(c)) {
                int j = i; while (j < n && is_ident((unsigned char)line[j])) ++j;
                std::string tok = line.substr(i, j - i);
                auto tc = L.token_colors.find(tok);
                if (tc != L.token_colors.end()) {
                    add_color(i, j, tc->second[0], tc->second[1], tc->second[2]);
                } else if (L.keywords.count(tok)) {
                    add_color(i, j, L.kw_r, L.kw_g, L.kw_b);
                } else if (L.types.count(tok)) {
                    add_color(i, j, L.ty_r, L.ty_g, L.ty_b);
                } else if (L.builtins.count(tok)) {
                    add_color(i, j, L.bi_r >= 0 ? L.bi_r : tv.function_r,
                                    L.bi_g >= 0 ? L.bi_g : tv.function_g,
                                    L.bi_b >= 0 ? L.bi_b : tv.function_b);
                } else {
                    int k = j; while (k < n && line[k] == ' ') ++k;
                    if (L.highlight_keys && k < n && line[k] == ':' && !(k + 1 < n && line[k+1] == ':')) {
                        add_color(i, j, tv.accent2_r, tv.accent2_g, tv.accent2_b);
                    } else if (k < n && line[k] == '(') {
                        add_color(i, j, tv.function_r, tv.function_g, tv.function_b);
                    }
                }
                i = j; continue;
            }
            // Operators
            if (c=='+'||c=='-'||c=='*'||c=='='||c=='<'||c=='>'||c=='!'||c=='&'||c=='|'||c=='^'||c=='%'||c==':'||c=='?') {
                if (L.op_r >= 0) add_color(i, i+1, L.op_r, L.op_g, L.op_b);
                else { double ov = std::min(1.0, tv.muted * 1.3); add_color(i, i+1, ov, ov, ov); }
            }
            ++i;
        }
        return al;
    }
};

//  CodeEditor - text buffer + rendering + input handling
struct CodeEditor {

    // Buffer & file
    RopeBuffer  buffer; // O(log n) edits via rope skip-list
    std::string filename = "untitled.cpp";
    bool dirty = false;

    // Syntax highlighting state
    SyntaxHighlighter::Lang syn_lang = SyntaxHighlighter::Lang::Cpp;
    std::vector<bool> ml_comment_state; // per-line multi-line comment tracking
    const LangAddon  *active_addon      = nullptr; // non-null when addon handles rendering
    const LangAddon  *completions_addon = nullptr; // non-null for completions (any lang)

    // Name shown in the top bar and status bar: an addon's language for files
    // without a built-in highlighter, otherwise the built-in language name
    const char* language_name() const {
        if (syn_lang == SyntaxHighlighter::Lang::Unknown && active_addon && !active_addon->lang_name.empty())
            return active_addon->lang_name.c_str();
        return SyntaxHighlighter::display_name(syn_lang);
    }

    void update_syn_lang() {
        syn_lang          = SyntaxHighlighter::detect(filename);
        active_addon      = nullptr;
        completions_addon = nullptr;

        if (!addon_loader) { ml_comment_state.clear(); highlight_dirty = true; return; }

        // Extract file extension
        std::string ext;
        auto p = filename.rfind('.');
        if (p != std::string::npos) {
            ext = filename.substr(p);
            for (auto &c : ext) c = (char)std::tolower((unsigned char)c);
        }

        if (!ext.empty()) {
            for (const auto &a : addon_loader->addons) {
                for (const auto &ae : a.extensions) {
                    if (ae == ext) {
                        // Use addon for completions always
                        completions_addon = &a;
                        // Use addon for rendering:
                        // always if no built-in highlighter (Unknown lang)
                        // also if addon defines custom colors (kw_r/ty_r changed from defaults)
                        if (syn_lang == SyntaxHighlighter::Lang::Unknown) {
                            active_addon = &a;
                        } else {
                            // Addon can override keyword/type colors even for built-in langs
                            // We store it so build_attrs can use its color overrides
                            active_addon = &a;
                        }
                        break;
                    }
                }
                if (completions_addon) break;
            }
        }

        ml_comment_state.clear();
        highlight_dirty = true;
    }

    // Internal clipboard (Ctrl+C/X/V) + X11 system clipboard
    std::string clipboard;

    // Auto-bracket insertion: enabled via General settings
    bool auto_bracket = true;

    // Undo / Redo stack
    struct UndoRecord {
        std::string buffer_before;
        int         cursor_before = 0;
        std::string buffer_after;
        int         cursor_after  = 0;
    };
    // MEMORY OPTIMIZATION:
    // Use deque instead of vector - erase(begin()) O(1) instead of O(n)
    // MAX_UNDO limits the depth. For files > 100KB additionally
    // reduce the limit so as not to keep hundreds of copies of a large buffer
    std::deque<UndoRecord> undo_stack;
    std::deque<UndoRecord> redo_stack;
    static constexpr int MAX_UNDO         = 200;   // for files < 100KB
    static constexpr int MAX_UNDO_LARGE   = 50;    // for files >= 100KB
    static constexpr size_t LARGE_FILE_BYTES = 100 * 1024;

    int effective_max_undo() const {
        return (buffer.size() >= LARGE_FILE_BYTES) ? MAX_UNDO_LARGE : MAX_UNDO;
    }

    // Snapshot before a mutating operation; call BEFORE the change
    void push_undo(const std::string &buf_before, int cur_before) {
        undo_stack.push_back({buf_before, cur_before, buf_before, cur_before});
        // Обрезаем стек спереди (O(1) для deque)
        while ((int)undo_stack.size() > effective_max_undo())
            undo_stack.pop_front();
        redo_stack.clear();
    }
    // Finalise the last undo record AFTER the change (sets after-state)
    void commit_undo() {
        if (!undo_stack.empty()) {
            undo_stack.back().buffer_after = get_buffer_str();  // rebuilds cache after change
            undo_stack.back().cursor_after = cursor_pos;
        }
        invalidate_buffer_cache(); // buffer has changed
    }
    void do_undo() {
        if (undo_stack.empty()) return;
        UndoRecord r = undo_stack.back(); undo_stack.pop_back();
        r.buffer_after = get_buffer_str(); r.cursor_after = cursor_pos;  // cache
        redo_stack.push_back(r);
        buffer = r.buffer_before; cursor_pos = r.cursor_before;
        invalidate_buffer_cache(); // restored old buffer
        sel_active = false; extra_cursors.clear();
        rebuild_lines(); dirty = true; preferred_col = -1; reset_blink();
    }
    void do_redo() {
        if (redo_stack.empty()) return;
        UndoRecord r = redo_stack.back(); redo_stack.pop_back();
        undo_stack.push_back(r);
        buffer = r.buffer_after; cursor_pos = r.cursor_after;
        sel_active = false; extra_cursors.clear();
        invalidate_buffer_cache(); //restored new buffer
        rebuild_lines(); dirty = true; preferred_col = -1; reset_blink();
    }


    // X11 system clipboard - native XConvertSelection, no external tools
    // g_x11_dpy / g_x11_win are set by base.cxx immediately after window creation
    static std::string x11_clipboard_get() {
        // Pull globals declared in globals.hxx
        extern Display     *g_x11_dpy;
        extern Window       g_x11_win;

        if (!g_x11_dpy || !g_x11_win) return {};

        Display *dpy = g_x11_dpy;
        Window   win = g_x11_win;

        Atom clip   = XInternAtom(dpy, "CLIPBOARD", False);
        Atom utf8   = XInternAtom(dpy, "UTF8_STRING", False);
        Atom prop   = XInternAtom(dpy, "TEXTOR_CLIP_IN", False);

        // Ask the current CLIPBOARD owner to convert to UTF-8
        XConvertSelection(dpy, clip, utf8, prop, win, CurrentTime);
        XFlush(dpy);

        // Wait for SelectionNotify (up to ~200 ms)
        XEvent ev;
        for (int tries = 0; tries < 200; ++tries) {
            if (XCheckTypedWindowEvent(dpy, win, SelectionNotify, &ev)) break;
            struct timespec ts = {0, 1000000}; // 1 ms
            nanosleep(&ts, nullptr);
        }

        if (ev.type != SelectionNotify || ev.xselection.property == None)
            return {};

        // Read the property
        Atom            actual_type;
        int             actual_fmt;
        unsigned long   n_items, bytes_left;
        unsigned char  *data = nullptr;
        XGetWindowProperty(dpy, win, prop,
                           0, 0x7FFFFFFF, True,
                           AnyPropertyType,
                           &actual_type, &actual_fmt,
                           &n_items, &bytes_left, &data);
        std::string result;
        if (data) {
            result.assign(reinterpret_cast<char*>(data), n_items);
            XFree(data);
        }
        return result;
    }

    static void x11_clipboard_set(const std::string &text) {
        extern Display     *g_x11_dpy;
        extern Window       g_x11_win;
        extern std::string  g_x11_clipboard;

        if (!g_x11_dpy || !g_x11_win) {
            // Fallback: try xclip/xsel so at least something works
            FILE *p = popen("xclip -selection clipboard 2>/dev/null", "w");
            if (!p) p = popen("xsel --clipboard --input 2>/dev/null", "w");
            if (p) { fwrite(text.data(), 1, text.size(), p); pclose(p); }
            return;
        }

        g_x11_clipboard = text;

        Atom clip = XInternAtom(g_x11_dpy, "CLIPBOARD", False);
        // Claim ownership - base.cxx handles SelectionRequest to serve it
        XSetSelectionOwner(g_x11_dpy, clip, g_x11_win, CurrentTime);
        XFlush(g_x11_dpy);
    }

    // Cursor & selection
    int cursor_pos  = 0;   // byte offset in buffer (moving end)
    int sel_anchor  = 0;   // byte offset of the fixed selection end
    bool sel_active = false;

    // Multi-cursor support
    struct ExtraCursor {
        int  pos       = 0;
        int  anchor    = 0;
        bool sel_active = false;
        bool has_selection() const { return sel_active && anchor != pos; }
        int  sel_lo()  const { return std::min(anchor, pos); }
        int  sel_hi()  const { return std::max(anchor, pos); }
    };
    std::vector<ExtraCursor> extra_cursors;

    // Helpers
    bool has_selection() const { return sel_active && (sel_anchor != cursor_pos); }
    int  sel_lo()        const { return std::min(sel_anchor, cursor_pos); }
    int  sel_hi()        const { return std::max(sel_anchor, cursor_pos); }

    void clear_selection() { sel_anchor = cursor_pos; sel_active = false; }
    void clear_extra_cursors() { extra_cursors.clear(); }

    // Moves every extra cursor (arrows, Home, End): `move` gets the old position, returns the new one.
    // Cursors that end up on the same place are merged into one.
    template <class F> void move_extra_cursors(F move) {
        for (auto &ec : extra_cursors) {
            ec.pos = std::clamp(move(ec.pos), 0, (int)buffer.size());
            ec.anchor = ec.pos; ec.sel_active = false;
        }
        std::vector<ExtraCursor> keep;
        for (const auto &ec : extra_cursors) {
            if (ec.pos == cursor_pos) continue;
            bool same = false;
            for (const auto &k : keep) if (k.pos == ec.pos) { same = true; break; }
            if (!same) keep.push_back(ec);
        }
        extra_cursors.swap(keep);
    }

    // Add/toggle extra cursor at position (Ctrl+Click)
    void add_cursor_at(int pos) {
        if (pos == cursor_pos) return; // already main cursor
        for (int i = 0; i < (int)extra_cursors.size(); i++) {
            if (extra_cursors[i].pos == pos) {
                extra_cursors.erase(extra_cursors.begin() + i); // toggle off
                return;
            }
        }
        ExtraCursor ec; ec.pos = ec.anchor = pos; ec.sel_active = false;
        extra_cursors.push_back(ec);
    }

    // Insert text at all cursor positions (ascending offset method)
    void multi_insert(const char *text, int len) {
        if (extra_cursors.empty()) {
            buffer.insert(cursor_pos, text, len);
            cursor_pos += len; return;
        }
        struct CP { int orig; int *ptr; };
        std::vector<CP> cps;
        cps.push_back({cursor_pos, &cursor_pos});
        for (auto &ec : extra_cursors) cps.push_back({ec.pos, &ec.pos});
        std::sort(cps.begin(), cps.end(), [](const CP &a, const CP &b){ return a.orig < b.orig; });
        int offset = 0;
        for (auto &cp : cps) {
            int actual = cp.orig + offset;
            // Guard against out-of-range insert
            actual = std::clamp(actual, 0, (int)buffer.size());
            buffer.insert(actual, text, len);
            *cp.ptr = actual + len;
            offset += len;
        }
        clamp_cursors();
    }

    // Clamp all cursor positions to valid buffer range
    void clamp_cursors() {
        int bsz = (int)buffer.size();
        cursor_pos = std::clamp(cursor_pos, 0, bsz);
        sel_anchor = std::clamp(sel_anchor, 0, bsz);
        for (auto &ec : extra_cursors) {
            ec.pos    = std::clamp(ec.pos,    0, bsz);
            ec.anchor = std::clamp(ec.anchor, 0, bsz);
        }
    }

    /*
        Erase one char before each cursor - sort descending so high-pos deletions
        never shift the byte offsets of lower cursors (no cumulative adjustment needed).
        UTF-8 codepoint boundaries
        Backspace/Delete used to erase exactly one *byte*. For any multi-byte
        UTF-8 character (e.g. Cyrillic, 2 bytes each) that left the other byte
        of the pair behind as an orphaned, invalid UTF-8 fragment - which is
        exactly what rendered as garbled/mismatched glyphs after deleting a
        character. These helpers find the real codepoint boundary so a whole
        character is removed atomically.
    */
    int utf8_char_len(unsigned char lead) const {
        if ((lead & 0x80) == 0x00) return 1;   // 0xxxxxxx  (ASCII)
        if ((lead & 0xE0) == 0xC0) return 2;   // 110xxxxx
        if ((lead & 0xF0) == 0xE0) return 3;   // 1110xxxx
        if ((lead & 0xF8) == 0xF0) return 4;   // 11110xxx
        return 1; // stray continuation byte / invalid - fall back to 1 so we
                  // always make forward progress instead of looping forever
    }
    // Byte length of the UTF-8 codepoint ending immediately before `pos`.
    int utf8_backward_len(int pos) const {
        if (pos <= 0) return 0;
        int i = pos - 1, back = 1;
        while (i > 0 && (static_cast<unsigned char>(buffer[i]) & 0xC0) == 0x80) {
            --i; ++back;
            if (back >= 4) break; // malformed input safety cap
        }
        return back;
    }
    // Byte length of the UTF-8 codepoint starting at `pos`.
    int utf8_forward_len(int pos) const {
        if (pos < 0 || pos >= (int)buffer.size()) return 0;
        int len = utf8_char_len(static_cast<unsigned char>(buffer[pos]));
        // Clamp so we never claim to read past a lead byte that turned out
        // to be followed by fewer continuation bytes than expected.
        int avail = (int)buffer.size() - pos;
        return std::min(len, avail);
    }

    // Backspace at every cursor. The cursors are handled from the last one to the first, so
    // erasing never moves a cursor that is still waiting, the cursors already handled
    // (the ones behind the erased character) are moved back by its length.
    void multi_backspace() {
        if (!extra_cursors.empty()) move_extra_cursors([](int p) { return p; });   // drop doubles
        if (extra_cursors.empty()) {
            if (cursor_pos > 0) {
                int blen = utf8_backward_len(cursor_pos);
                if (blen <= 0) blen = 1;
                cursor_pos -= blen;
                buffer.erase(cursor_pos, blen);
            }
            return;
        }
        clamp_cursors();
        struct CP { int orig; int *ptr; };
        std::vector<CP> cps;
        cps.push_back({cursor_pos, &cursor_pos});
        for (auto &ec : extra_cursors) cps.push_back({ec.pos, &ec.pos});
        std::sort(cps.begin(), cps.end(), [](const CP &a, const CP &b){ return a.orig > b.orig; });
        for (size_t k = 0; k < cps.size(); ++k) {
            int blen = utf8_backward_len(cps[k].orig);
            if (blen <= 0) continue;                           // start of the file: nothing to erase
            int at = cps[k].orig - blen;
            buffer.erase(at, blen);
            *cps[k].ptr = at;
            for (size_t j = 0; j < k; ++j) *cps[j].ptr -= blen;
        }
        for (auto &ec : extra_cursors) { ec.anchor = ec.pos; ec.sel_active = false; }
        move_extra_cursors([](int p) { return p; });           // cursors that met become one
    }

    // Delete key at every cursor - same order and same shifting as multi_backspace
    void multi_delete() {
        if (!extra_cursors.empty()) move_extra_cursors([](int p) { return p; });
        if (extra_cursors.empty()) {
            if (cursor_pos < (int)buffer.size()) {
                int flen = utf8_forward_len(cursor_pos);
                if (flen <= 0) flen = 1;
                buffer.erase(cursor_pos, flen);
            }
            return;
        }
        clamp_cursors();
        struct CP { int orig; int *ptr; };
        std::vector<CP> cps;
        cps.push_back({cursor_pos, &cursor_pos});
        for (auto &ec : extra_cursors) cps.push_back({ec.pos, &ec.pos});
        std::sort(cps.begin(), cps.end(), [](const CP &a, const CP &b){ return a.orig > b.orig; });
        for (size_t k = 0; k < cps.size(); ++k) {
            int at = cps[k].orig;
            if (at < 0 || at >= (int)buffer.size()) continue;  // end of file: nothing to delete, just emptiness )
            int flen = utf8_forward_len(at);
            if (flen <= 0) flen = 1;
            buffer.erase(at, flen);
            for (size_t j = 0; j < k; ++j) *cps[j].ptr -= flen;
        }
        for (auto &ec : extra_cursors) { ec.anchor = ec.pos; ec.sel_active = false; }
        move_extra_cursors([](int p) { return p; });
    }

    std::string get_selection_text() const {
        if (!has_selection()) return "";
        return buffer.substr(sel_lo(), sel_hi() - sel_lo());
    }

    void delete_selection() {
        if (!has_selection()) return;
        int lo = sel_lo(), hi = sel_hi();
        buffer.erase(lo, hi - lo);
        cursor_pos = lo;
        // extra cursors behind the removed text move back with it
        auto shift_back = [&](int &p) { if (p >= hi) p -= hi - lo; else if (p > lo) p = lo; };
        for (auto &ec : extra_cursors) { shift_back(ec.pos); shift_back(ec.anchor); }
        move_extra_cursors([](int p) { return p; });      // merge cursors that now share a place
        clear_selection();
        invalidate_buffer_cache();
        rebuild_lines();
        dirty = true;
    }

    // Scroll
    double scroll_y = 0.0;
    double scroll_x = 0.0;
    double scroll_vy = 0.0;  // inertial velocity Y (pixels/frame)
    double scroll_vx = 0.0;  // inertial velocity X
    double total_text_height = 0.0;
    double total_text_width  = 0.0;

    // Buffer line cache
    // buffer.to_string() is an expensive operation (copies the entire file).
    // We invalidate the cache on every edit (dirty=true) and rebuild lazily.
    // Use get_buffer_str() instead of buffer.to_string() everywhere except saving.
    mutable std::string  buffer_str_cache;
    mutable bool         buffer_str_dirty = true;  // whether it is necessary to rebuild the cache

    // Returns a buffer string (from cache or rebuilds)
    const std::string &get_buffer_str() const {
        if (buffer_str_dirty) {
            buffer_str_cache = buffer.to_string();
            buffer_str_dirty = false;
        }
        return buffer_str_cache;
    }

    // Invalidate cache on any edit (called from insert_text/delete_selection, etc.)
    void invalidate_buffer_cache() {
        buffer_str_dirty = true;
    }

    // Cursor blink
    bool show_cursor    = true;
    bool cursor_visible = true;
    std::chrono::steady_clock::time_point last_blink = std::chrono::steady_clock::now();

    // Smooth cursor animation
    double cursor_anim_x  = -1.0;   // animated pixel X (-1 = uninitialised)
    double cursor_anim_y  = -1.0;   // animated pixel Y
    double cursor_target_x = -1.0;  // matches cursor_anim_x's "uninitialised" sentinel
    double cursor_target_y = -1.0;  // see any_anim_running() in base.cxx for why this matters

    // Bracket highlight
    // Bracket pair under the cursor, as (line, column) pairs. It is searched only when the
    // cursor moves or the text changes - never once per frame (see update_bracket_pair).
    struct BracketPair { bool valid = false; int l1 = 0, c1 = 0, l2 = 0, c2 = 0; };
    BracketPair bracket_pair;
    int      bracket_key_cursor = -1;   // cursor position the pair was searched for
    unsigned bracket_key_rev    = ~0u;  // text revision the pair was searched for
    unsigned buffer_rev         = 0;    // +1 on every change of the text (rebuild_lines)

    // Lines cache
    std::vector<std::string> lines;
    int line_count = 1;

    // Per-line width cache (avoids re-measuring all N lines each edit)
    // width_cache[i] == 0 -> not yet measured for that line.
    // Invalidated on rebuild_lines(); individual entries re-measured lazily.
    std::vector<double> width_cache;
    // Running max of all measured widths. Shrinks only on a full rescan.
    double width_cache_max     = 0.0;
    // Epoch counter: bumped on every rebuild_lines so draw knows to reset.
    int    width_cache_epoch   = 0;

    // Settings
    int    indent_size    = 4;
    double gutter_density = 1.0;
    double line_spacing   = 1.0;   // multiplier
    void set_line_spacing(double v) { line_spacing = v; font_cache.invalidate(); }

    // External deps
    ScrollBar    *scrollbar   = nullptr;
    SettingsModal *modal       = nullptr;
    ThemeTween   *theme_tween = nullptr;

    // Autocomplete
    Autocomplete autocomplete;
    bool *word_hints_on_ptr = nullptr;   // points to settings_tab.word_hints_on
    // Pointer to addon loader (set by Editor after construction)
    const AddonLoader *addon_loader = nullptr;

    void init_autocomplete() {
        std::vector<std::string> extra;
        bool is_d = (syn_lang == SyntaxHighlighter::Lang::D);

        // completions_addon supplies keywords for Rust and any other lang with .adn
        if (completions_addon && !completions_addon->completions.empty()) {
            extra = completions_addon->completions;
        } else if (addon_loader) {
            std::string lname;
            switch (syn_lang) {
                case SyntaxHighlighter::Lang::Rust:   lname = "Rust";   break;
                case SyntaxHighlighter::Lang::Python: lname = "Python"; break;
                case SyntaxHighlighter::Lang::Cpp:    lname = "C++";    break;
                case SyntaxHighlighter::Lang::C:      lname = "C";      break;
                case SyntaxHighlighter::Lang::Kotlin: lname = "Kotlin"; break;
                default: break;
            }
            if (!lname.empty())
                extra = addon_loader->get_completions(lname);
        }
        // D uses built-in builtins; all other langs use only their own items
        autocomplete.set_items(extra, is_d);
    }

    // Virtual preferred column (for Up/Down navigation)
    int preferred_col = -1;

    // Auto-scroll to cursor (disabled by default, enabled on key nav)
    bool auto_scroll_to_cursor = false;

    // Pending click (resolved in draw when layout is available)
    double pending_mouse_x  = 0.0;
    double pending_mouse_y  = 0.0;
    bool   needs_cursor_update = false;

    // Cached line height (written during draw, read for drag)
    double cached_line_h = 18.0;
    double view_h_cached = 0.0;   // height of the text view in the last frame (for scroll_to_cursor_now)

    // Dirty flags
    // highlight_dirty: buffer changed -> rebuild ml_comment_state & text-width cache
    bool highlight_dirty = true;
    bool render_dirty    = true;   // set whenever something visual changed

    // Find-bar overlay: suppress editor current-line highlight
    bool suppress_line_highlight = false;

    // Track last text-opt state so we invalidate Pango context on toggle.
    // Without this, changing hint mode mid-session corrupts cached glyph metrics
    // and produces random/garbled characters in the editor.
    bool last_text_optimized_ = false;

    // Background ML-comment state thread
    // When the buffer changes we launch an async task that scans all lines
    // using the lightweight scan_ml_state (no Pango - thread-safe).
    // The main thread picks up the result on the next frame.
    std::future<std::vector<bool>> ml_bg_future;
    std::mutex                     ml_bg_mutex;
    std::vector<bool>              ml_bg_result;   // written by bg, read by main
    bool                           ml_bg_ready = false;

    // Snapshot passed to background thread (immutable copy of lines)
    void kick_ml_background(const LangAddon *addon_snap,
                             SyntaxHighlighter::Lang lang_snap)
    {
        auto lines_snap = lines;  // copy
        bool has_addon  = (addon_snap != nullptr);
        LangAddon addon_copy;
        if (has_addon) addon_copy = *addon_snap;

        ml_bg_future = std::async(std::launch::async,
            [lines_snap, has_addon, addon_copy, lang_snap]() -> std::vector<bool>
        {
            int n = (int)lines_snap.size();
            std::vector<bool> state(n + 1, false);
            bool in_ml = false;
            for (int i = 0; i < n; ++i) {
                state[i] = in_ml;
                const LangAddon *ap = has_addon ? &addon_copy : nullptr;
                in_ml = SyntaxHighlighter::scan_ml_state(lines_snap[i], in_ml, ap, lang_snap);
            }
            state[n] = in_ml;
            return state;
        });
        ml_bg_ready = false;
    }

    // Call once per frame: copy result if bg thread is done.
    void poll_ml_background() {
        if (!ml_bg_future.valid()) return;
        if (ml_bg_future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            return;
        std::lock_guard<std::mutex> lk(ml_bg_mutex);
        ml_bg_result = ml_bg_future.get();
        ml_bg_ready  = true;
    }

    // Cached Pango font metrics (avoid re-measuring "Ag" every frame)
    struct FontMetricsCache {
        PangoFontDescription *last_fnt = nullptr;
        double line_h = 0.0;
        bool valid    = false;
        void invalidate() { valid = false; }
    } font_cache;

    CodeEditor() { rebuild_lines(); }

    // Lines management
    void rebuild_lines() {
        // The buffer has changed - invalidate the line cache.
        // This covers all edit paths (insert, delete, undo/redo).
        invalidate_buffer_cache();
        ++buffer_rev;
        lines.clear();
        // Iterate rope chunks - avoids materialising a full std::string copy.
        std::string cur;
        cur.reserve(128);
        buffer.for_each_chunk([&](const char *data, size_t len) {
            for (size_t i = 0; i < len; ++i) {
                if (data[i] == '\n') { lines.push_back(std::move(cur)); cur.clear(); }
                else                   cur.push_back(data[i]);
            }
        });
        lines.push_back(std::move(cur));
        if (lines.empty()) lines.push_back(std::string());
        line_count = (int)lines.size();

        // Invalidate caches
        ml_comment_state.clear();
        highlight_dirty = true;
        font_cache.invalidate();
        render_dirty    = true;

        // Invalidate per-line width cache - line indices may have shifted.
        width_cache.assign(line_count, 0.0);
        width_cache_max = 0.0;
        ++width_cache_epoch;

        // Shrink backing storage so switching away from a large file actually
        // returns memory to the OS rather than holding oversized capacity.
        if ((int)lines.capacity() > line_count * 2 + 64)
            lines.shrink_to_fit();
    }

    void update_line_count() { rebuild_lines(); }

    // Index (line, col)
    void index_to_line_col(int idx, int &out_line, int &out_col) const {
        idx = std::clamp(idx, 0, (int)buffer.size());
        int pos = 0;
        for (int i = 0; i < (int)lines.size(); ++i) {
            int lnlen   = (int)lines[i].size();
            int seg_len = lnlen + (i < (int)lines.size() - 1 ? 1 : 0);
            if (idx <= pos + lnlen || i == (int)lines.size() - 1) {
                out_line = i;
                out_col  = std::clamp(idx - pos, 0, lnlen);
                return;
            }
            pos += seg_len;
        }
        out_line = (int)lines.size() - 1;
        out_col  = (int)lines.back().size();
    }

    int line_col_to_index(int line, int col) const {
        line = std::clamp(line, 0, (int)lines.size() - 1);
        col  = std::clamp(col,  0, (int)lines[line].size());
        int idx = 0;
        for (int i = 0; i < line; ++i) idx += (int)lines[i].size() + 1;
        return idx + col;
    }

    // Resolve click -> cursor position using Pango
    // local_x / local_y are already in content-space (scroll subtracted, origin = top-left of text)
    void resolve_click_to_cursor(double local_x, double local_y,
                                  PangoLayout *layout, PangoFontDescription *fnt)
    {
        if (!layout || !fnt) { needs_cursor_update = true; return; }

        // lines[] is already current; rebuild only happens on buffer mutation.
        if (lines.empty()) { needs_cursor_update = true; return; }

        // Use cached line height - avoid re-measuring every drag event.
        double line_h = cached_line_h > 1.0 ? cached_line_h : [&]() -> double {
            pango_layout_set_font_description(layout, fnt);
            pango_layout_set_text(layout, "Ag", -1);
            PangoRectangle ink, logical;
            pango_layout_get_extents(layout, &ink, &logical);
            double h = (double)logical.height / PANGO_SCALE;
            if (h <= 0) h = (double)pango_font_description_get_size(fnt) / PANGO_SCALE * 1.2;
            return h * std::max(0.5, line_spacing);
        }();

        int approx_line = std::clamp((int)std::floor(local_y / line_h),
                                     0, (int)lines.size() - 1);

        const std::string &ln = lines[approx_line];
        if (ln.empty()) {
            cursor_pos    = line_col_to_index(approx_line, 0);
            preferred_col = 0;
            return;
        }

        // O(log N) column lookup via Pango
        // pango_layout_xy_to_index maps a pixel position -> byte index in O(log N)
        // using Pango's internal binary-search on glyph extents.
        // This replaces the old O(N) for(col=0..len) loop.
        pango_layout_set_font_description(layout, fnt);
        pango_layout_set_text(layout, ln.c_str(), (int)ln.size());

        int byte_idx  = 0;
        int trailing  = 0;
        // Convert pixel X to Pango units (PANGO_SCALE = 1024)
        int pango_x = (int)(local_x * PANGO_SCALE);
        int pango_y = (int)((local_y - approx_line * line_h) * PANGO_SCALE);
        pango_layout_xy_to_index(layout, pango_x, pango_y, &byte_idx, &trailing);

        // 'trailing' == 1 means the click was past the midpoint of the glyph
        // advance one grapheme cluster so the cursor lands after it.
        if (trailing) {
            // Move byte_idx forward by one UTF-8 character
            const char *p = ln.c_str() + byte_idx;
            const char *e = ln.c_str() + ln.size();
            if (p < e) {
                // Skip one UTF-8 byte sequence
                unsigned char uc = (unsigned char)*p;
                if      (uc < 0x80) byte_idx += 1;
                else if (uc < 0xE0) byte_idx += 2;
                else if (uc < 0xF0) byte_idx += 3;
                else                byte_idx += 4;
                byte_idx = std::min(byte_idx, (int)ln.size());
            }
        }

        byte_idx      = std::clamp(byte_idx, 0, (int)ln.size());
        cursor_pos    = line_col_to_index(approx_line, byte_idx);
        preferred_col = byte_idx;
    }

    // Mouse click
    // Resolves immediately if layout/fnt supplied.
    // text_area_x = ed_x + gutter_width  (the +15 text-padding is added inside)
    void handle_mouse_click(int x, int y,
                             int text_area_x, int text_area_y, int /*gutter_w*/,
                             PangoLayout *layout, PangoFontDescription *fnt,
                             bool shift_held = false, bool ctrl_held = false)
    {
        double local_x = (double)x - (text_area_x + 15.0) + scroll_x;
        double local_y = (double)y - (text_area_y + 15.0) + scroll_y;

        needs_cursor_update = false;
        reset_blink();

        if (ctrl_held && !shift_held) {
            // Ctrl+Click: add / remove extra cursor
            int saved = cursor_pos;
            resolve_click_to_cursor(local_x, local_y, layout, fnt);
            int new_pos = cursor_pos;
            cursor_pos = saved; // restore main cursor
            add_cursor_at(new_pos);
        } else if (shift_held) {
            if (!sel_active) { sel_anchor = cursor_pos; sel_active = true; }
            resolve_click_to_cursor(local_x, local_y, layout, fnt);
            if (cursor_pos == sel_anchor) sel_active = false;
            clear_extra_cursors();
        } else {
            resolve_click_to_cursor(local_x, local_y, layout, fnt);
            sel_anchor = cursor_pos;
            sel_active = false;
            clear_extra_cursors();
        }
        preferred_col = -1;
    }

    // Overload without layout (deferred to next draw frame)
    void handle_mouse_click(int x, int y, int text_area_x, int text_area_y, int gutter_w,
                             bool shift_held = false)
    {
        pending_mouse_x    = (double)x - (text_area_x + 15.0) + scroll_x;
        pending_mouse_y    = (double)y - (text_area_y + 15.0) + scroll_y;
        needs_cursor_update = true;
        reset_blink();
        if (!shift_held) { sel_anchor = cursor_pos; sel_active = false; }
        preferred_col = -1;
        (void)gutter_w;
    }

    // Mouse drag
    void handle_mouse_drag(int x, int y,
                            int text_area_x, int text_area_y,
                            PangoLayout *layout, PangoFontDescription *fnt)
    {
        double local_x = (double)x - (text_area_x + 15.0) + scroll_x;
        double local_y = (double)y - (text_area_y + 15.0) + scroll_y;

        // Clamp local_y so dragging above/below the content still works
        local_y = std::max(0.0, local_y);

        resolve_click_to_cursor(local_x, local_y, layout, fnt);

        sel_active = (cursor_pos != sel_anchor);
        reset_blink();
        preferred_col = -1;
    }

    void handle_mouse_move(int /*x*/, int /*y*/) {}

    // Select the word under the cursor (used on double-click)
    void select_word_at_cursor() {
        const std::string &buf = get_buffer_str();
        int n = (int)buf.size();
        if (n == 0) return;
        int pos = std::clamp(cursor_pos, 0, n - 1);

        auto is_word = [](char c) {
            return std::isalnum((unsigned char)c) || c == '_';
        };

        // If not on a word char, nothing to select
        if (!is_word(buf[pos])) return;

        int lo = pos;
        while (lo > 0 && is_word(buf[lo - 1])) --lo;
        int hi = pos;
        while (hi < n && is_word(buf[hi])) ++hi;

        if (hi > lo) {
            sel_anchor = lo;
            cursor_pos = hi;
            sel_active = true;
            clear_extra_cursors();
            auto_scroll_to_cursor = true;
            reset_blink();
        }
    }


    // Keyboard: handle the key, then at once scroll so the cursor line stays in view
    void handle_key(XKeyEvent *kev) {
        handle_key_impl(kev);
        scroll_to_cursor_now();
    }

    // Vertical tracking is implemented within the key handler (using the viewport height from the most recent
    // frame), ensuring the cursor never leaves the window boundaries, even if a frame is delayed. Horizontal
    // tracking and the final check are still performed within the draw_text_area function.
    void scroll_to_cursor_now() {
        if (!auto_scroll_to_cursor || cached_line_h <= 1.0 || view_h_cached <= 0.0) return;
        rebuild_lines();
        int line, col; index_to_line_col(cursor_pos, line, col);
        double cy = (double)line * cached_line_h, margin = 14.0;
        if (cy < scroll_y + margin)
            scroll_y = std::max(0.0, cy - margin);
        else if (cy + cached_line_h > scroll_y + view_h_cached - margin)
            scroll_y = std::max(0.0, cy + cached_line_h - (view_h_cached - margin));
    }

    void handle_key_impl(XKeyEvent *kev) {
        if (!kev) return;
        auto_scroll_to_cursor = true;

        char buf[32]; KeySym ks;
        int  len  = textor_lookup_key(kev, buf, sizeof(buf), &ks);
        bool ctrl  = (kev->state & ControlMask) != 0;
        bool shift = (kev->state & ShiftMask)   != 0;

        std::string layout_text;
        if (len <= 0 && ks != 0) {
            layout_text = textor_map_key_to_text(static_cast<uint64_t>(ks), static_cast<uint64_t>(kev->state), 1u);
        }
        if (!layout_text.empty()) {
            len = (int)layout_text.size();
            std::memcpy(buf, layout_text.data(), std::min<size_t>(len, sizeof(buf) - 1));
            buf[len] = '\0';
        }

        // Autocomplete intercept
        if (autocomplete.visible && autocomplete.enabled) {
            std::string ac_insert;
            if (autocomplete.handle_key(kev, ac_insert)) {
                if (!ac_insert.empty()) {
                    multi_insert(ac_insert.c_str(), (int)ac_insert.size());
                    rebuild_lines(); dirty = true;
                    preferred_col = -1; reset_blink();
                }
                return;
            }
        }

        // Ctrl shortcuts
        if (ctrl) {
            // Ctrl+/ : Toggle line comment
            if (ks == XK_slash) {
                push_undo(get_buffer_str(), cursor_pos);  // cache - do not copy the entire buffer
                int line, col; index_to_line_col(cursor_pos, line, col);
                // Determine comment prefix
                std::string cmt = "// ";
                if (syn_lang == SyntaxHighlighter::Lang::Python ||
                    syn_lang == SyntaxHighlighter::Lang::Shell)  cmt = "# ";
                if (syn_lang == SyntaxHighlighter::Lang::Mekanism) cmt = "# ";
                if (syn_lang == SyntaxHighlighter::Lang::Config)   cmt = "# ";
                if (syn_lang == SyntaxHighlighter::Lang::Lua)      cmt = "-- ";

                auto toggle_comment = [&](int ln) {
                    const std::string &l = lines[ln];
                    int idx_s = line_col_to_index(ln, 0);
                    // Strip leading whitespace offset
                    int ws = 0; while (ws < (int)l.size() && (l[ws]==' '||l[ws]=='\t')) ws++;
                    bool has_cmt = ((int)l.size() >= ws + (int)cmt.size() &&
                                    l.substr(ws, cmt.size()) == cmt);
                    if (has_cmt) {
                        buffer.erase(idx_s + ws, (int)cmt.size());
                    } else {
                        buffer.insert(idx_s + ws, cmt.c_str(), (int)cmt.size());
                    }
                };

                if (has_selection()) {
                    int lo_ln, lo_cl, hi_ln, hi_cl;
                    index_to_line_col(sel_lo(), lo_ln, lo_cl);
                    index_to_line_col(sel_hi(), hi_ln, hi_cl);
                    // Iterate in reverse so insertions don't shift earlier offsets
                    for (int i = hi_ln; i >= lo_ln; --i) toggle_comment(i);
                } else {
                    toggle_comment(line);
                }
                commit_undo(); rebuild_lines(); dirty = true;
                sel_active = false; preferred_col = -1; reset_blink(); return;
            }

            // Ctrl+D : Duplicate line or selection
            if (ks == XK_d || ks == XK_D) {
                push_undo(get_buffer_str(), cursor_pos);  // cache - do not copy the entire buffer
                if (has_selection()) {
                    std::string sel = get_selection_text();
                    int hi = sel_hi();
                    buffer.insert(hi, sel.c_str(), (int)sel.size());
                    cursor_pos = hi + (int)sel.size();
                    sel_anchor = hi;
                } else {
                    int ln, col_; index_to_line_col(cursor_pos, ln, col_);
                    std::string dup = "\n" + lines[ln];
                    int end_idx = line_col_to_index(ln, (int)lines[ln].size());
                    buffer.insert(end_idx, dup.c_str(), (int)dup.size());
                    cursor_pos = end_idx + 1 + col_;
                }
                commit_undo(); rebuild_lines(); dirty = true;
                preferred_col = -1; reset_blink(); return;
            }

            // Ctrl+Shift+K : Delete line
            if (shift && (ks == XK_k || ks == XK_K)) {
                push_undo(get_buffer_str(), cursor_pos);  // cache - do not copy the entire buffer
                int ln, col_; index_to_line_col(cursor_pos, ln, col_);
                int s = line_col_to_index(ln, 0);
                int e = (ln + 1 < (int)lines.size())
                    ? line_col_to_index(ln + 1, 0)
                    : line_col_to_index(ln, (int)lines[ln].size());
                buffer.erase(s, e - s);
                cursor_pos = std::clamp(s, 0, (int)buffer.size());
                commit_undo(); rebuild_lines(); dirty = true;
                sel_active = false; preferred_col = -1; reset_blink(); return;
            }

            // Ctrl+] : Indent selection
            if (ks == XK_bracketright) {
                push_undo(get_buffer_str(), cursor_pos);  // cache - do not copy the entire buffer
                int lo_ln, lo_cl, hi_ln, hi_cl;
                index_to_line_col(has_selection() ? sel_lo() : cursor_pos, lo_ln, lo_cl);
                index_to_line_col(has_selection() ? sel_hi() : cursor_pos, hi_ln, hi_cl);
                std::string pad(indent_size, ' ');
                for (int i = hi_ln; i >= lo_ln; --i) {
                    int si = line_col_to_index(i, 0);
                    buffer.insert(si, pad.c_str(), indent_size);
                }
                cursor_pos += indent_size;
                commit_undo(); rebuild_lines(); dirty = true;
                preferred_col = -1; reset_blink(); return;
            }

            // Ctrl+[ : Dedent selection
            if (ks == XK_bracketleft) {
                push_undo(get_buffer_str(), cursor_pos);  // cache - do not copy the entire buffer
                int lo_ln, lo_cl, hi_ln, hi_cl;
                index_to_line_col(has_selection() ? sel_lo() : cursor_pos, lo_ln, lo_cl);
                index_to_line_col(has_selection() ? sel_hi() : cursor_pos, hi_ln, hi_cl);
                for (int i = hi_ln; i >= lo_ln; --i) {
                    const std::string &l = lines[i];
                    int rm = 0;
                    while (rm < indent_size && rm < (int)l.size() && l[rm] == ' ') rm++;
                    if (rm > 0) {
                        int si = line_col_to_index(i, 0);
                        buffer.erase(si, rm);
                        if (i == lo_ln) cursor_pos = std::max(0, cursor_pos - rm);
                    }
                }
                commit_undo(); rebuild_lines(); dirty = true;
                preferred_col = -1; reset_blink(); return;
            }

            // Ctrl+J : Join line with next
            if (ks == XK_j || ks == XK_J) {
                push_undo(get_buffer_str(), cursor_pos);  // cache - do not copy the entire buffer
                int ln, col_; index_to_line_col(cursor_pos, ln, col_);
                if (ln + 1 < (int)lines.size()) {
                    int end_cur = line_col_to_index(ln, (int)lines[ln].size());
                    // Remove newline and trim leading whitespace on next line
                    int skip = 0;
                    while (skip < (int)lines[ln+1].size() &&
                           (lines[ln+1][skip]==' '||lines[ln+1][skip]=='\t')) skip++;
                    buffer.erase(end_cur, 1 + skip);
                    buffer.insert(end_cur, " ", 1);
                    cursor_pos = end_cur;
                }
                commit_undo(); rebuild_lines(); dirty = true;
                sel_active = false; preferred_col = -1; reset_blink(); return;
            }

            if (ks == XK_a || ks == XK_A) {
                sel_anchor = 0; cursor_pos = (int)buffer.size();
                sel_active = true; preferred_col = -1; reset_blink();
                autocomplete.hide(); return;
            }
            if (ks == XK_c || ks == XK_C) {
                clipboard = get_selection_text();
                x11_clipboard_set(clipboard);
                return;
            }
            if (ks == XK_x || ks == XK_X) {
                clipboard = get_selection_text();
                x11_clipboard_set(clipboard);
                if (has_selection()) { push_undo(get_buffer_str(), cursor_pos); delete_selection(); commit_undo(); }
                preferred_col = -1; reset_blink(); autocomplete.hide(); return;
            }
            if (ks == XK_v || ks == XK_V) {
                // Prefer X11 clipboard; fall back to internal
                std::string sys = x11_clipboard_get();
                if (!sys.empty()) clipboard = sys;
                push_undo(get_buffer_str(), cursor_pos);  // cache - do not copy the entire buffer
                if (has_selection()) delete_selection();
                if (!clipboard.empty()) {
                    buffer.insert(cursor_pos, clipboard);
                    cursor_pos += (int)clipboard.size();
                    rebuild_lines(); dirty = true;
                }
                commit_undo();
                preferred_col = -1; reset_blink(); autocomplete.hide(); return;
            }
            if (ks == XK_d || ks == XK_D) {
                rebuild_lines();
                push_undo(get_buffer_str(), cursor_pos);  // cache - do not copy the entire buffer
                int line, col; index_to_line_col(cursor_pos, line, col);
                std::string dup = "\n" + lines[line];
                int ins = line_col_to_index(line, (int)lines[line].size());
                buffer.insert(ins, dup);
                cursor_pos += (int)dup.size();
                rebuild_lines(); dirty = true;
                commit_undo();
                preferred_col = -1; reset_blink(); autocomplete.hide(); return;
            }
            // Ctrl+Z: undo
            if (ks == XK_z || ks == XK_Z) {
                if (shift) do_redo(); else do_undo();
                return;
            }
            // Ctrl+Y: redo
            if (ks == XK_y || ks == XK_Y) {
                do_redo(); return;
            }
            // Ctrl+Space -> force autocomplete
            if (ks == XK_space) {
                if (autocomplete.enabled) {
                    std::string w = Autocomplete::word_at(get_buffer_str(), cursor_pos);
                    if (w.empty()) w = "_";
                    autocomplete.update_with_buffer_words(w, buffer.to_string(),
                        word_hints_on_ptr && *word_hints_on_ptr);
                }
                return;
            }
            return; // other ctrl handled by Editor
        }

        // Arrow keys
        if (ks == XK_Left && !extra_cursors.empty())
            move_extra_cursors([&](int p) { return p - (p > 0 ? std::max(1, utf8_backward_len(p)) : 0); });
        if (ks == XK_Right && !extra_cursors.empty())
            move_extra_cursors([&](int p) { return p + (p < (int)buffer.size() ? std::max(1, utf8_forward_len(p)) : 0); });
        if ((ks == XK_Up || ks == XK_Down) && !extra_cursors.empty()) {
            rebuild_lines();
            move_extra_cursors([&](int p) {
                int l, c; index_to_line_col(p, l, c);
                int tl = (ks == XK_Up) ? std::max(0, l - 1) : std::min((int)lines.size() - 1, l + 1);
                return line_col_to_index(tl, std::min((int)lines[tl].size(), c));
            });
        }
        if ((ks == XK_Home || ks == XK_End) && !extra_cursors.empty()) {
            rebuild_lines();
            move_extra_cursors([&](int p) {
                int l, c; index_to_line_col(p, l, c);
                if (ks == XK_End) return line_col_to_index(l, (int)lines[l].size());
                int f = 0;
                while (f < (int)lines[l].size() && (lines[l][f] == ' ' || lines[l][f] == '\t')) f++;
                return line_col_to_index(l, c != f ? f : 0);
            });
        }
        if (ks == XK_Left) {
            if (has_selection() && !shift) { cursor_pos = sel_lo(); clear_selection(); }
            else { begin_selection_if(shift); if (cursor_pos > 0) cursor_pos -= std::max(1, utf8_backward_len(cursor_pos)); update_selection_after(shift); }
            autocomplete.hide(); preferred_col = -1; reset_blink(); return;
        }
        if (ks == XK_Right) {
            if (has_selection() && !shift) { cursor_pos = sel_hi(); clear_selection(); }
            else { begin_selection_if(shift); if (cursor_pos < (int)buffer.size()) cursor_pos += std::max(1, utf8_forward_len(cursor_pos)); update_selection_after(shift); }
            autocomplete.hide(); preferred_col = -1; reset_blink(); return;
        }
        if (ks == XK_Up || ks == XK_Down) {
            rebuild_lines();
            int line, col; index_to_line_col(cursor_pos, line, col);
            if (preferred_col < 0) preferred_col = col;
            if (!shift && has_selection()) {
                cursor_pos = (ks == XK_Up) ? sel_lo() : sel_hi();
                clear_selection(); index_to_line_col(cursor_pos, line, col);
            } else { begin_selection_if(shift); }
            int tl = (ks == XK_Up) ? std::max(0, line-1) : std::min((int)lines.size()-1, line+1);
            int tc = std::min((int)lines[tl].size(), preferred_col);
            cursor_pos = line_col_to_index(tl, tc);
            update_selection_after(shift);
            autocomplete.hide(); reset_blink(); return;
        }

        // Home / End
        if (ks == XK_Home) {
            begin_selection_if(shift); rebuild_lines();
            int line, col; index_to_line_col(cursor_pos, line, col);
            int fnws = 0;
            while (fnws < (int)lines[line].size() && (lines[line][fnws]==' '||lines[line][fnws]=='\t')) fnws++;
            cursor_pos = (col != fnws) ? line_col_to_index(line, fnws) : line_col_to_index(line, 0);
            update_selection_after(shift);
            autocomplete.hide(); preferred_col = -1; reset_blink(); return;
        }
        if (ks == XK_End) {
            begin_selection_if(shift); rebuild_lines();
            int line, col; index_to_line_col(cursor_pos, line, col);
            cursor_pos = line_col_to_index(line, (int)lines[line].size());
            update_selection_after(shift);
            autocomplete.hide(); preferred_col = -1; reset_blink(); return;
        }

        // Editing keys
        if (ks == XK_Escape) {
            if (!extra_cursors.empty()) { clear_extra_cursors(); reset_blink(); return; }
            autocomplete.hide(); return;
        }
        if (ks == XK_BackSpace) {
            push_undo(get_buffer_str(), cursor_pos);  // cache - do not copy the entire buffer
            if (has_selection()) { delete_selection(); }
            else {
                // Auto-bracket: if cursor is between a pair, delete both
                if (auto_bracket && !extra_cursors.empty() == false &&
                    cursor_pos > 0 && cursor_pos < (int)buffer.size())
                {
                    char prev = buffer[cursor_pos-1];
                    char next = buffer[cursor_pos];
                    bool is_pair = (prev=='(' && next==')') ||
                                   (prev=='{' && next=='}') ||
                                   (prev=='[' && next==']') ||
                                   (prev=='"' && next=='"') ||
                                   (prev=='\'' && next=='\'');
                    if (is_pair) {
                        buffer.erase(cursor_pos, 1);
                        buffer.erase(cursor_pos-1, 1);
                        cursor_pos--;
                        rebuild_lines(); dirty = true;
                        commit_undo();
                        if (autocomplete.enabled) {
                            std::string w = Autocomplete::word_at(get_buffer_str(), cursor_pos);
                            autocomplete.update_with_buffer_words(w, buffer.to_string(),
                        word_hints_on_ptr && *word_hints_on_ptr);
                        }
                        preferred_col = -1; reset_blink(); return;
                    }
                }
                multi_backspace(); rebuild_lines(); dirty = true;
            }
            commit_undo();
            if (autocomplete.enabled) {
                std::string w = Autocomplete::word_at(get_buffer_str(), cursor_pos);
                autocomplete.update_with_buffer_words(w, buffer.to_string(),
                        word_hints_on_ptr && *word_hints_on_ptr);
            }
            preferred_col = -1; reset_blink(); return;
        }
        if (ks == XK_Delete) {
            push_undo(get_buffer_str(), cursor_pos);  // cache - do not copy the entire buffer
            if (has_selection()) { delete_selection(); }
            else { multi_delete(); rebuild_lines(); dirty = true; }
            commit_undo();
            autocomplete.hide(); preferred_col = -1; reset_blink(); return;
        }
        if (ks == XK_Return || ks == XK_KP_Enter) {
            if (has_selection()) delete_selection();
            rebuild_lines();
            int line, col; index_to_line_col(cursor_pos, line, col);
            std::string indent;
            for (char c : lines[line]) { if (c==' '||c=='\t') indent+=c; else break; }
            std::string ins = "\n" + indent;
            multi_insert(ins.c_str(), (int)ins.size());
            rebuild_lines(); dirty = true;
            autocomplete.hide(); preferred_col = -1; reset_blink(); return;
        }
        if (ks == XK_Tab) {
            // If selection spans multiple lines -> indent block
            if (has_selection() && !shift) {
                int lo_ln, lo_cl, hi_ln, hi_cl;
                index_to_line_col(sel_lo(), lo_ln, lo_cl);
                index_to_line_col(sel_hi(), hi_ln, hi_cl);
                if (hi_ln > lo_ln) {
                    push_undo(get_buffer_str(), cursor_pos);  // cache - do not copy the entire buffer
                    std::string pad(indent_size, ' ');
                    for (int i = hi_ln; i >= lo_ln; --i) {
                        int si = line_col_to_index(i, 0);
                        buffer.insert(si, pad.c_str(), indent_size);
                    }
                    cursor_pos += indent_size;
                    commit_undo(); rebuild_lines(); dirty = true;
                    preferred_col = -1; reset_blink();
                    autocomplete.hide(); return;
                }
            }
            if (has_selection()) delete_selection();
            std::string s(indent_size, ' ');
            multi_insert(s.c_str(), (int)s.size());
            rebuild_lines(); dirty = true;
            autocomplete.hide(); preferred_col = -1; reset_blink(); return;
        }

        // Printable characters  (with auto-bracket insertion)
        if (len > 0 && !ctrl) {
            char ch0 = buf[0];

            // Auto-bracket: skip-over closing bracket
            if (auto_bracket && len == 1 &&
                (ch0 == ')' || ch0 == '}' || ch0 == ']') &&
                cursor_pos < (int)buffer.size() && buffer[cursor_pos] == ch0)
            {
                // Just advance past the existing bracket instead of inserting
                cursor_pos++;
                rebuild_lines(); preferred_col = -1; reset_blink();
                autocomplete.hide(); return;
            }

            // Auto-bracket: skip-over closing quote
            if (auto_bracket && len == 1 &&
                (ch0 == '"' || ch0 == '\'') &&
                cursor_pos < (int)buffer.size() && buffer[cursor_pos] == ch0)
            {
                // Check we're actually inside a pair (prev char == ch0 too)
                bool inside = (cursor_pos > 0 && buffer[cursor_pos-1] != '\\');
                if (inside) {
                    cursor_pos++;
                    rebuild_lines(); preferred_col = -1; reset_blink();
                    autocomplete.hide(); return;
                }
            }

            if (has_selection()) delete_selection();

            // Auto-bracket: insert pair
            if (auto_bracket && len == 1 && extra_cursors.empty()) {
                const char *pair = nullptr;
                if      (ch0 == '(') pair = ")";
                else if (ch0 == '{') pair = "}";
                else if (ch0 == '[') pair = "]";
                else if (ch0 == '"') pair = "\"";
                else if (ch0 == '\'') pair = "'";

                if (pair) {
                    // Insert opening char
                    buffer.insert(cursor_pos, buf, 1);
                    cursor_pos += 1;
                    // Insert closing char after cursor
                    buffer.insert(cursor_pos, pair, 1);
                    // cursor stays between the pair
                    rebuild_lines(); dirty = true;
                    preferred_col = -1; reset_blink();
                    if (autocomplete.enabled) {
                        std::string w = Autocomplete::word_at(get_buffer_str(), cursor_pos);
                        autocomplete.update_with_buffer_words(w, buffer.to_string(),
                        word_hints_on_ptr && *word_hints_on_ptr);
                    } else { autocomplete.hide(); }
                    return;
                }
            }

            // Auto-bracket: smart backspace
            // (handled in XK_BackSpace below, but we need the pair info here)

            // Normal insert
            push_undo(get_buffer_str(), cursor_pos);  // cache - do not copy the entire buffer
            multi_insert(buf, len);
            commit_undo();
            rebuild_lines(); dirty = true;
            preferred_col = -1; reset_blink();
            // Trigger autocomplete on every keystroke - both from loaded language
            // items AND from buffer word-hints, exactly like the backspace path
            // below does. Previously this required all_items to be non-empty,
            // which meant plain text / languages without a loaded item list
            // would never show suggestions while typing (only while deleting).
            if (autocomplete.enabled) {
                std::string w = Autocomplete::word_at(get_buffer_str(), cursor_pos);
                if (w.size() >= 1) autocomplete.update_with_buffer_words(w, buffer.to_string(),
                        word_hints_on_ptr && *word_hints_on_ptr);
                else autocomplete.hide();
            } else {
                autocomplete.hide();
            }
        }
        // Alt+Up / Alt+Down : Move line(s)
        bool alt = (kev->state & Mod1Mask) != 0;
        if (alt && !ctrl) {
            if (ks == XK_Up || ks == XK_Down) {
                push_undo(get_buffer_str(), cursor_pos);  // cache - do not copy the entire buffer
                int ln, col_; index_to_line_col(cursor_pos, ln, col_);
                if (ks == XK_Up && ln > 0) {
                    // Swap current line with line above
                    std::string cur_line_txt = lines[ln];
                    std::string prev_line_txt = lines[ln-1];
                    int prev_s = line_col_to_index(ln-1, 0);
                    int prev_e = line_col_to_index(ln-1, (int)prev_line_txt.size());
                    int cur_s  = line_col_to_index(ln,   0);
                    int cur_e  = line_col_to_index(ln,   (int)cur_line_txt.size());
                    // Replace both lines (cur_e >= prev_s so replace from bottom)
                    buffer.erase(cur_s, cur_e - cur_s);
                    buffer.insert(cur_s, prev_line_txt.c_str(), (int)prev_line_txt.size());
                    buffer.erase(prev_s, prev_e - prev_s);
                    buffer.insert(prev_s, cur_line_txt.c_str(), (int)cur_line_txt.size());
                    cursor_pos = line_col_to_index(ln-1, std::min(col_, (int)cur_line_txt.size()));
                } else if (ks == XK_Down && ln < (int)lines.size()-1) {
                    std::string cur_line_txt  = lines[ln];
                    std::string next_line_txt = lines[ln+1];
                    int cur_s  = line_col_to_index(ln,   0);
                    int cur_e  = line_col_to_index(ln,   (int)cur_line_txt.size());
                    int next_s = line_col_to_index(ln+1, 0);
                    int next_e = line_col_to_index(ln+1, (int)next_line_txt.size());
                    buffer.erase(next_s, next_e - next_s);
                    buffer.insert(next_s, cur_line_txt.c_str(), (int)cur_line_txt.size());
                    buffer.erase(cur_s, cur_e - cur_s);
                    buffer.insert(cur_s, next_line_txt.c_str(), (int)next_line_txt.size());
                    cursor_pos = line_col_to_index(ln+1, std::min(col_, (int)cur_line_txt.size()));
                }
                commit_undo(); rebuild_lines(); dirty = true;
                sel_active = false; preferred_col = -1; reset_blink(); return;
            }
        }
    }

    void handle_key_release(XKeyEvent *) {}

    // Ensure cursor is visible (auto-scroll)
    void ensure_cursor_visible(double view_w, double view_h,
                                PangoLayout *layout, PangoFontDescription *fnt)
    {
        if (!layout || !fnt || !auto_scroll_to_cursor) return;
        rebuild_lines();

        int line, col; index_to_line_col(cursor_pos, line, col);

        // Measure line height
        pango_layout_set_font_description(layout, fnt);
        pango_layout_set_text(layout, "Ag", -1);
        PangoRectangle ink, logical;
        pango_layout_get_extents(layout, &ink, &logical);
        double base_h = (double)logical.height / PANGO_SCALE;
        if (base_h <= 0) base_h = (double)pango_font_description_get_size(fnt) / PANGO_SCALE * 1.2;
        double line_h = base_h * std::max(0.5, line_spacing);
        cached_line_h = line_h;

        // Cursor pixel X
        pango_layout_set_text(layout, lines[line].c_str(), (int)lines[line].size());
        PangoRectangle pos;
        pango_layout_get_cursor_pos(layout, col, &pos, nullptr);
        double cx = (double)pos.x / PANGO_SCALE;
        double cy = (double)line * line_h;

        double margin = 14.0;

        // Vertical
        if (cy < scroll_y + margin)
            scroll_y = std::max(0.0, cy - margin);
        else if (cy + line_h > scroll_y + view_h - margin)
            scroll_y = std::max(0.0, cy + line_h - (view_h - margin));

        // Horizontal
        double hm = 24.0;
        if (cx < scroll_x + hm)
            scroll_x = std::max(0.0, cx - hm);
        else if (cx > scroll_x + view_w - hm)
            scroll_x = std::max(0.0, cx - (view_w - hm));
    }

    // Main draw
    // Forward declaration of GPU overlay struct (defined in editor.hxx)
    struct GpuOverlay {
        struct Rect { double x, y, w, h; };
        Rect  gpu_sel_rects[3] = {};
        int   gpu_sel_rect_count = 0;
        bool  gpu_sel_active = false;
        float gpu_sel_r = 0, gpu_sel_g = 0, gpu_sel_b = 0;
    };

    void draw_text_area(cairo_t *cr, PangoLayout *layout, PangoFontDescription *fnt,
                         const ThemeVariant &tv,
                         int text_area_x, int text_area_y,
                         int width, int height,
                         GpuOverlay *gpu_overlay = nullptr)
    {
        if (!cr || !layout || !fnt) return;

        // Text optimisation: fast hint mode, no sub-pixel AA
        // Cuts Pango/Cairo text CPU cost roughly in half on old hardware.
        // Applied per-frame so toggling in Settings takes effect immediately.
        //
        // IMPORTANT: when the mode flips we must invalidate Pango's internal
        // glyph/metrics cache; otherwise stale hinting metrics produce corrupted
        // (random) glyphs on screen.
        if (last_text_optimized_ != g_text_optimized) {
            last_text_optimized_ = g_text_optimized;
            font_cache.invalidate();    // line_h must be remeasured
            pango_layout_context_changed(layout); // flush Pango glyph cache
            highlight_dirty = true; // force full re-highlight
        }

        cairo_font_options_t *fo = cairo_font_options_create();
        if (g_text_optimized) {
            cairo_font_options_set_antialias   (fo, CAIRO_ANTIALIAS_GRAY);
            cairo_font_options_set_hint_style  (fo, CAIRO_HINT_STYLE_FULL);
            cairo_font_options_set_hint_metrics(fo, CAIRO_HINT_METRICS_ON);
        } else {
            cairo_font_options_set_antialias   (fo, CAIRO_ANTIALIAS_SUBPIXEL);
            cairo_font_options_set_hint_style  (fo, CAIRO_HINT_STYLE_SLIGHT);
            cairo_font_options_set_hint_metrics(fo, CAIRO_HINT_METRICS_ON);
        }
        pango_cairo_context_set_font_options(
            pango_layout_get_context(layout), fo);
        cairo_font_options_destroy(fo);

        // Only rebuild if buffer was modified (highlight_dirty acts as the dirty flag)
        if (highlight_dirty || lines.empty())
            rebuild_lines();
        // Note: rebuild_lines sets highlight_dirty=true; we handle width/highlight below
        pango_layout_set_font_description(layout, fnt);

        // Measure line height - cached to avoid Pango call every frame
        double line_h;
        if (font_cache.valid && font_cache.last_fnt == fnt) {
            line_h = font_cache.line_h;
        } else {
            pango_layout_set_text(layout, "Ag", -1);
            PangoRectangle ink, logical;
            pango_layout_get_extents(layout, &ink, &logical);
            double base_h = (double)logical.height / PANGO_SCALE;
            if (base_h <= 0) base_h = (double)pango_font_description_get_size(fnt) / PANGO_SCALE * 1.2;
            line_h = base_h * std::max(0.5, line_spacing);
            font_cache.line_h    = line_h;
            font_cache.last_fnt  = fnt;
            font_cache.valid     = true;
        }
        cached_line_h = line_h;

        // Total content extents
        total_text_height = line_h * (double)lines.size();

        view_h_cached = (double)(height - text_area_y - 30);

        // Scroll to the cursor BEFORE drawing, so this very frame already shows it
        // (it used to run after drawing, and the frame with the new scroll never came) :(
        ensure_cursor_visible((double)(width - text_area_x - 30),
                              (double)(height - text_area_y - 30),
                              layout, fnt);
        // total_text_width is filled after visible line range is known (see below).

        // Setup clip & translate
        cairo_save(cr);
        cairo_rectangle(cr, text_area_x, text_area_y,
                        width  - text_area_x,
                        height - text_area_y - 12);
        cairo_clip(cr);
        cairo_translate(cr, text_area_x + 15.0 - scroll_x,
                             text_area_y + 15.0 - scroll_y);

        // Resolve pending click
        if (needs_cursor_update) {
            resolve_click_to_cursor(pending_mouse_x, pending_mouse_y, layout, fnt);
            needs_cursor_update = false;
        }

        // Current cursor position
        int cur_line = 0, cur_col = 0;
        index_to_line_col(cursor_pos, cur_line, cur_col);

        // Visible line range
        double view_top    = scroll_y;
        double view_bottom = scroll_y + (height - text_area_y - 30.0);
        int    start_ln    = std::max(0, (int)std::floor(view_top    / line_h) - 1);
        int    end_ln      = std::min((int)lines.size()-1,
                                      (int)std::ceil(view_bottom / line_h) + 1);

        // Per-line width cache (must be after start_ln/end_ln are known)
        // Only measures visible lines each frame - O(visible) not O(N).
        // Full rescan every ~120 frames to keep scrollbar width accurate.
        {
            if ((int)width_cache.size() != line_count)
                width_cache.assign(line_count, 0.0);

            double visible_max = 0.0;
            int ws = std::max(0, start_ln - 2);
            int we = std::min(line_count - 1, end_ln + 2);
            for (int i = ws; i <= we; ++i) {
                if (width_cache[i] == 0.0 && !lines[i].empty()) {
                    pango_layout_set_text(layout, lines[i].c_str(), (int)lines[i].size());
                    int pw, ph; pango_layout_get_pixel_size(layout, &pw, &ph);
                    width_cache[i] = (double)pw;
                    if (width_cache[i] > width_cache_max)
                        width_cache_max = width_cache[i];
                }
                visible_max = std::max(visible_max, width_cache[i]);
            }

            static int rescan_counter = 0;
            if (highlight_dirty || ++rescan_counter % 120 == 0) {
                rescan_counter = 0;
                double full_max = 0.0;
                for (int i = 0; i < line_count; ++i) {
                    if (width_cache[i] == 0.0 && !lines[i].empty()) {
                        pango_layout_set_text(layout, lines[i].c_str(), (int)lines[i].size());
                        int pw, ph; pango_layout_get_pixel_size(layout, &pw, &ph);
                        width_cache[i] = (double)pw;
                    }
                    full_max = std::max(full_max, width_cache[i]);
                }
                width_cache_max = full_max;
            }

            total_text_width = std::max(width_cache_max, visible_max);
        }

        update_bracket_pair();

        // Indent guides (VS Code-style subtle vertical lines)
        if (indent_size > 0) {
            // Measure width of indent_size spaces
            std::string sp(indent_size, ' ');
            pango_layout_set_text(layout, sp.c_str(), indent_size);
            int sp_w, sp_h; pango_layout_get_pixel_size(layout, &sp_w, &sp_h);
            double indent_px = (double)sp_w;

            cairo_set_line_width(cr, 1.0);
            for (int i = start_ln; i <= end_ln; ++i) {
                const std::string &ln = lines[i];
                // Count leading spaces
                int spaces = 0;
                for (char ch : ln) {
                    if (ch == ' ') ++spaces;
                    else if (ch == '\t') spaces += indent_size;
                    else break;
                }
                int guides = spaces / indent_size;
                if (guides < 1) continue;
                double ry = (double)i * line_h;
                for (int g = 1; g < guides; ++g) {
                    double gx = g * indent_px;
                    // Active guide (same indent level as cursor line) slightly brighter
                    bool active = (i == cur_line);
                    cairo_set_source_rgba(cr,
                        tv.muted, tv.muted, tv.muted, active ? 0.20 : 0.10);
                    cairo_move_to(cr, gx + 0.5, ry);
                    cairo_line_to(cr, gx + 0.5, ry + line_h);
                    cairo_stroke(cr);
                }
            }
            // Reset layout
            pango_layout_set_font_description(layout, fnt);
        }

        // Trailing-whitespace markers
        // Subtle dots over spaces/tabs left dangling at the end of a line
        // easy to miss otherwise, and a common source of noisy diffs. :(
        {
            cairo_set_line_width(cr, 1.0);
            for (int i = start_ln; i <= end_ln; ++i) {
                const std::string &ln = lines[i];
                if (ln.empty()) continue;
                size_t last_nonspace = ln.find_last_not_of(" \t");
                size_t ws_start = (last_nonspace == std::string::npos) ? 0 : last_nonspace + 1;
                if (ws_start >= ln.size()) continue; // no trailing whitespace
                // Don't flag the line the cursor is currently editing - avoids
                // distracting dots appearing every time you type a space.
                if (i == cur_line) continue;

                pango_layout_set_attributes(layout, nullptr);
                pango_layout_set_text(layout, ln.c_str(), (int)ln.size());
                PangoRectangle p_start{}, p_end{};
                pango_layout_get_cursor_pos(layout, (int)ws_start, &p_start, nullptr);
                pango_layout_get_cursor_pos(layout, (int)ln.size(), &p_end, nullptr);
                double wx0 = (double)p_start.x / PANGO_SCALE;
                double wx1 = (double)p_end.x   / PANGO_SCALE;
                double wy  = (double)i * line_h + line_h * 0.5;

                cairo_set_source_rgba(cr, tv.warning_r, tv.warning_g, tv.warning_b, 0.35);
                for (double dxp = wx0 + 3.0; dxp < wx1; dxp += 6.0) {
                    shape::disc(cr, dxp, wy, 1.0);
                    cairo_fill(cr);
                }
            }
            pango_layout_set_font_description(layout, fnt);
        }

        // Current-line highlight (VS Code style)
        // Suppressed when find bar is visible to avoid double-highlight on matches.
        if (!has_selection() && !suppress_line_highlight) {
            double hl_y  = cur_line * line_h;
            double hl_rx = -8.0;
            double hl_rw = std::max(200.0, total_text_width + 20.0);
            // Subtle full-width background tint
            cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.055);
            cairo_rectangle(cr, hl_rx, hl_y, hl_rw, line_h);
            cairo_fill(cr);
            // Left border accent line (2px, accent colour)
            cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.70);
            cairo_rectangle(cr, hl_rx, hl_y, 2.0, line_h);
            cairo_fill(cr);
        }

        // Selection rectangles (drawn before text, like Sublime Text)
        if (has_selection()) {
            int slo = sel_lo(), shi = sel_hi();
            int lo_ln, lo_cl, hi_ln, hi_cl;
            index_to_line_col(slo, lo_ln, lo_cl);
            index_to_line_col(shi, hi_ln, hi_cl);

            double sel_r = tv.selection_r, sel_g = tv.selection_g, sel_b = tv.selection_b;
            if (theme_tween && theme_tween->is_running()) {
                ThemeVariant tc = theme_tween->current();
                sel_r = tc.selection_r; sel_g = tc.selection_g; sel_b = tc.selection_b;
            }

            // Ensure clean layout state for geometry measurements.
            // This is critical for D (and other langs) where syntax highlighting
            // may have left Pango attribute lists or a modified font on the layout,
            // which causes pango_layout_get_cursor_pos to return wrong pixel offsets.
            pango_layout_set_attributes(layout, nullptr);
            pango_layout_set_font_description(layout, fnt);

            // Width of a space char for "newline visual" extension
            pango_layout_set_text(layout, "m", 1);
            int sw, sh_; pango_layout_get_pixel_size(layout, &sw, &sh_);
            double nl_ext = (double)sw * 0.7;

            int sls = std::max(start_ln, lo_ln);
            int sle = std::min(end_ln,   hi_ln);

            // Fast selection rendering
            // Only the FIRST and LAST selected lines need Pango cursor-pos calls.
            // All middle lines (fully selected) are drawn as full-width rectangles -
            // this cuts Pango calls from O(visible_sel_lines) to O(1) per frame.
            // This matters most for 2000+ line files with large selections.

            // Measure start x of the first selected line (lo_ln)
            double first_xs = 0.0, first_xe = 0.0;
            if (lo_ln >= sls && lo_ln <= sle) {
                const std::string &fln = lines[lo_ln];
                pango_layout_set_attributes(layout, nullptr);
                pango_layout_set_text(layout, fln.c_str(), (int)fln.size());
                int cs = std::clamp(lo_cl, 0, (int)fln.size());
                PangoRectangle p; pango_layout_get_cursor_pos(layout, cs, &p, nullptr);
                first_xs = (double)p.x / PANGO_SCALE;
                if (lo_ln == hi_ln) {
                    // Single-line selection: also measure end
                    int ce = std::clamp(hi_cl, 0, (int)fln.size());
                    pango_layout_get_cursor_pos(layout, ce, &p, nullptr);
                    first_xe = (double)p.x / PANGO_SCALE;
                    if (first_xe - first_xs < 1.5) first_xe = first_xs + nl_ext;
                } else {
                    // Multi-line: first line extends to right edge
                    first_xe = std::max(total_text_width + nl_ext, 120.0);
                }
            }

            // Measure end x of the last selected line (hi_ln, only if multi-line)
            double last_xe = 0.0;
            if (lo_ln != hi_ln && hi_ln >= sls && hi_ln <= sle) {
                const std::string &lln = lines[hi_ln];
                pango_layout_set_attributes(layout, nullptr);
                pango_layout_set_text(layout, lln.c_str(), (int)lln.size());
                int ce = std::clamp(hi_cl, 0, (int)lln.size());
                PangoRectangle p; pango_layout_get_cursor_pos(layout, ce, &p, nullptr);
                last_xe = (double)p.x / PANGO_SCALE;
                if (last_xe < 1.5) last_xe = nl_ext;
            }

            // Export selection geometry to GPU overlay fields
            // Window-coord offset: cairo_translate was called with
            // (text_area_x + 15 - scroll_x,  text_area_y + 15 - scroll_y)
            // So a local (x,y) maps to window (x + off_wx, y + off_wy).
            double off_wx = text_area_x + 15.0 - scroll_x;
            double off_wy = text_area_y + 15.0 - scroll_y;
            double full_xe = std::max(total_text_width + nl_ext, 120.0);

            if (gpu_overlay) {
                // GPU path: skip Cairo fill, send rects to OpenGL overlay
                gpu_overlay->gpu_sel_active = true;
                gpu_overlay->gpu_sel_r = (float)sel_r;
                gpu_overlay->gpu_sel_g = (float)sel_g;
                gpu_overlay->gpu_sel_b = (float)sel_b;
                gpu_overlay->gpu_sel_rect_count = 0;

                // Clip bounds in window coordinates - prevents selection from
                // bleeding over gutter, sidebar, console or tab-bar.
                // IMPORTANT: clip_y0 = text_area_y (== TAB_BAR_H) to highlight
                // didn't layer on the top bar when scrolling down.
                double clip_x0 = (double)text_area_x;
                double clip_y0 = (double)text_area_y;  // = TAB_BAR_H
                double clip_x1 = (double)width;
                double clip_y1 = (double)height - 12.0;

                auto push_rect = [&](double xs, double ry, double xe) {
                    // Convert local coords -> window coords
                    double wx0 = xs + off_wx;
                    double wy0 = ry + off_wy;
                    double wx1 = xe + off_wx;
                    double wy1 = wy0 + line_h;
                    // Clamp to valid editor viewport
                    wx0 = std::max(wx0, clip_x0);
                    wy0 = std::max(wy0, clip_y0);
                    wx1 = std::min(wx1, clip_x1);
                    wy1 = std::min(wy1, clip_y1);
                    if (wx1 - wx0 < 0.5 || wy1 - wy0 < 0.5) return;
                    if (gpu_overlay->gpu_sel_rect_count >= 3) return;
                    auto &sr = gpu_overlay->gpu_sel_rects[gpu_overlay->gpu_sel_rect_count++];
                    sr.x = wx0; sr.y = wy0;
                    sr.w = wx1 - wx0; sr.h = wy1 - wy0;
                };

                if (lo_ln == hi_ln) {
                    push_rect(first_xs, (double)lo_ln * line_h, first_xe);
                } else {
                    push_rect(first_xs, (double)lo_ln * line_h, full_xe);
                    // Middle band (all lines between first and last)
                    if (hi_ln - lo_ln > 1) {
                        double band_y  = (double)(lo_ln + 1) * line_h;
                        double band_h  = (double)(hi_ln - lo_ln - 1) * line_h;
                        auto &sr = gpu_overlay->gpu_sel_rects[gpu_overlay->gpu_sel_rect_count++];
                        sr.x = 0 + off_wx; sr.y = band_y + off_wy;
                        sr.w = full_xe;    sr.h = band_h;
                    }
                    double last_xe2 = last_xe < 1.5 ? nl_ext : last_xe;
                    push_rect(0.0, (double)hi_ln * line_h, last_xe2);
                }
            } else {
                // CPU path: draw directly with Cairo
                cairo_set_source_rgba(cr, sel_r, sel_g, sel_b, 0.38);

                for (int i = sls; i <= sle; ++i) {
                    double ry = (double)i * line_h;
                    double xs, xe;
                    if (i == lo_ln && i == hi_ln) { xs = first_xs; xe = first_xe; }
                    else if (i == lo_ln) { xs = first_xs; xe = full_xe; }
                    else if (i == hi_ln) { xs = 0.0; xe = last_xe < 1.5 ? nl_ext : last_xe; }
                    else                 { xs = 0.0; xe = full_xe; }
                    if (xe - xs >= 0.5)
                        cairo_rectangle(cr, xs, ry, xe - xs, line_h);
                }
                cairo_fill(cr);
            }

            // Restore clean layout state for subsequent text rendering
            pango_layout_set_attributes(layout, nullptr);
            pango_layout_set_font_description(layout, fnt);
        }

        // Bracket highlight (VS Code-style box around matching pair)
        if (bracket_pair.valid) {
            auto draw_bracket_box = [&](int bl, int bc) {
                if (bl < start_ln || bl > end_ln) return;   // off screen: nothing to draw
                pango_layout_set_attributes(layout, nullptr);
                pango_layout_set_font_description(layout, fnt);
                pango_layout_set_text(layout, lines[bl].c_str(), (int)lines[bl].size());
                PangoRectangle r0, r1;
                pango_layout_get_cursor_pos(layout, bc,   &r0, nullptr);
                pango_layout_get_cursor_pos(layout, bc+1, &r1, nullptr);
                double bx = (double)r0.x / PANGO_SCALE;
                double bw = std::max(6.0, (double)(r1.x - r0.x) / PANGO_SCALE);
                double by = (double)bl * line_h + 1.0;
                double bh = line_h - 2.0;
                cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.55);
                cairo_set_line_width(cr, 1.5);
                cairo_rectangle(cr, bx, by, bw, bh);
                cairo_stroke(cr);
            };
            draw_bracket_box(bracket_pair.l1, bracket_pair.c1);
            draw_bracket_box(bracket_pair.l2, bracket_pair.c2);
            pango_layout_set_attributes(layout, nullptr);
            pango_layout_set_font_description(layout, fnt);
        }


        // Draw visible text lines (with syntax highlighting)
        cairo_set_source_rgb(cr, tv.text, tv.text, tv.text);
        pango_font_description_set_weight(fnt, PANGO_WEIGHT_NORMAL);
        pango_layout_set_font_description(layout, fnt);

        // ML-comment state: rebuilt asynchronously on buffer change
        // poll_ml_background() checks if the bg thread finished; if so, swap
        // the cached ml_comment_state.  This keeps the main thread O(visible)
        // instead of O(N) per frame for large files.
        poll_ml_background();

        if (highlight_dirty || (int)ml_comment_state.size() != line_count + 1) {
            if (ml_bg_ready) {
                // Background scan complete - adopt result immediately.
                std::lock_guard<std::mutex> lk(ml_bg_mutex);
                ml_comment_state = ml_bg_result;
                ml_bg_ready      = false;
                highlight_dirty  = false;
            } else {
                // Kick background thread if not already running.
                if (!ml_bg_future.valid() ||
                    ml_bg_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
                {
                    kick_ml_background(active_addon, syn_lang);
                }
                // Fallback for the very first draw: synchronously scan only the
                // visible window (fast even for huge files).
                if ((int)ml_comment_state.size() != line_count + 1) {
                    ml_comment_state.assign(line_count + 1, false);
                    // Quick forward-only scan up to start_ln
                    bool in_ml = false;
                    for (int i = 0; i < std::min(start_ln, line_count); ++i) {
                        ml_comment_state[i] = in_ml;
                        in_ml = SyntaxHighlighter::scan_ml_state(
                                    lines[i], in_ml, active_addon, syn_lang);
                    }
                    // Fill the rest with the last known state (approximate)
                    for (int i = start_ln; i <= line_count; ++i)
                        ml_comment_state[i] = in_ml;
                }
                // Don't clear highlight_dirty yet - wait for bg to finish.
            }
        } else {
            // Check if background result arrived this frame
            if (ml_bg_ready) {
                std::lock_guard<std::mutex> lk(ml_bg_mutex);
                ml_comment_state = ml_bg_result;
                ml_bg_ready      = false;
            }
        }

        // Smart Render: tighten visible range when enabled
        // With g_smart_render ON we add a smaller overdraw buffer (2 lines vs 4)
        // and skip Pango attribute building for lines outside a "highlight zone"
        // that is 8 lines narrower than the full visible window.
        // Lines in the overdraw fringe are drawn plain (no colour) - they are
        // usually clipped by the cairo_rectangle clip set above, so never seen.
        int highlight_start = start_ln;
        int highlight_end   = end_ln;
        if (g_smart_render) {
            // Tighten overdraw buffer: 1 line above/below instead of 4
            start_ln      = std::max(0, (int)std::floor(view_top    / line_h));
            end_ln        = std::min((int)lines.size()-1,
                                     (int)std::ceil(view_bottom / line_h));
            highlight_start = start_ln;
            highlight_end   = end_ln;
        }

        for (int i = start_ln; i <= end_ln; ++i) {
            const std::string &ln = lines[i];
            pango_layout_set_text(layout, ln.c_str(), (int)ln.size());

            // Smart render: skip expensive attribute building for fringe lines
            bool in_highlight_zone = (i >= highlight_start && i <= highlight_end);
            if (in_highlight_zone) {
                bool in_ml  = ml_comment_state[i];
                bool out_ml = false;
                PangoAttrList *attrs;
                if (active_addon && syn_lang == SyntaxHighlighter::Lang::Unknown) {
                    // Pure addon language - use addon-driven highlighter
                    attrs = SyntaxHighlighter::build_attrs_addon(ln, *active_addon, tv, in_ml, out_ml);
                } else {
                    // Built-in language - use built-in highlighter, pass addon for color overrides
                    attrs = SyntaxHighlighter::build_attrs(ln, syn_lang, tv, in_ml, out_ml, active_addon);
                }
                pango_layout_set_attributes(layout, attrs);
                pango_attr_list_unref(attrs);
            } else {
                pango_layout_set_attributes(layout, nullptr);
            }

            cairo_move_to(cr, 0.0, (double)i * line_h);
            pango_cairo_show_layout(cr, layout);
            pango_layout_set_attributes(layout, nullptr);
        }

        // Cursor blink + smooth animation
        {
            auto now = std::chrono::steady_clock::now();
            double ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_blink).count();
            if (ms > 530) { cursor_visible = !cursor_visible; last_blink = now; }

            // Compute true pixel target
            pango_layout_set_text(layout, lines[cur_line].c_str(), (int)lines[cur_line].size());
            PangoRectangle cpos_r;
            pango_layout_get_cursor_pos(layout, cur_col, &cpos_r, nullptr);
            double target_cx = (double)cpos_r.x / PANGO_SCALE;
            double target_cy = (double)cur_line * line_h;
            double ch        = (double)cpos_r.height / PANGO_SCALE;

            // Init on first frame
            if (cursor_anim_x < 0.0) { cursor_anim_x = target_cx; cursor_anim_y = target_cy; }

            // Store targets for any_anim_running query from the main loop
            cursor_target_x = target_cx;
            cursor_target_y = target_cy;

            // Smooth glide - fast spring (VS Code feel: ~80ms settle)
            bool animations_on = g_animations_enabled;
            if (animations_on) {
                double k = 0.30;  // lerp factor per frame (~60fps -> ~80ms settle)
                cursor_anim_x += (target_cx - cursor_anim_x) * k;
                cursor_anim_y += (target_cy - cursor_anim_y) * k;
                // Snap when very close to avoid infinite drift
                if (std::abs(cursor_anim_x - target_cx) < 0.5) cursor_anim_x = target_cx;
                if (std::abs(cursor_anim_y - target_cy) < 0.5) cursor_anim_y = target_cy;
            } else {
                cursor_anim_x = target_cx;
                cursor_anim_y = target_cy;
            }

            double crv = tv.cursor_r, cgv = tv.cursor_g, cbv = tv.cursor_b;
            if (theme_tween && theme_tween->is_running()) {
                ThemeVariant tc = theme_tween->current();
                crv = tc.cursor_r; cgv = tc.cursor_g; cbv = tc.cursor_b;
            }

            if (cursor_visible && show_cursor) {
                double draw_cx = cursor_anim_x;
                double draw_cy = cursor_anim_y;
                double cur_h   = std::max(ch, line_h * 0.88);

                // Cursor bar - clean, no glow/halo
                cairo_set_source_rgb(cr, crv, cgv, cbv);
                cairo_rectangle(cr, draw_cx - 1.0, draw_cy, 2.0, cur_h);
                cairo_fill(cr);
            }
        }

        if (preferred_col < 0) preferred_col = cur_col;

        // Reset so mouse-wheel scroll doesn't snap back to cursor
        auto_scroll_to_cursor = false;

        // Extra cursors (multi-cursor)
        if (cursor_visible && show_cursor && !extra_cursors.empty()) {
            double crv = tv.cursor_r, cgv = tv.cursor_g, cbv = tv.cursor_b;
            if (theme_tween && theme_tween->is_running()) {
                ThemeVariant tc = theme_tween->current();
                crv = tc.cursor_r; cgv = tc.cursor_g; cbv = tc.cursor_b;
            }
            for (const auto &ec : extra_cursors) {
                int ec_line = 0, ec_col = 0;
                index_to_line_col(ec.pos, ec_line, ec_col);
                if (ec_line < start_ln || ec_line > end_ln) continue;
                pango_layout_set_text(layout, lines[ec_line].c_str(), (int)lines[ec_line].size());
                PangoRectangle epos;
                pango_layout_get_cursor_pos(layout, ec_col, &epos, nullptr);
                double ecx = (double)epos.x / PANGO_SCALE;
                double ech = (double)epos.height / PANGO_SCALE;
                cairo_set_source_rgb(cr, crv, cgv, cbv);
                cairo_rectangle(cr, ecx - 1.0, (double)ec_line * line_h,
                                 2.5, std::max(ech, line_h * 0.88));
                cairo_fill(cr);
            }
        }

        cairo_restore(cr);

        // Draw autocomplete popup if visible (closed first if the cursor moved away)
        autocomplete.follow_cursor(cursor_pos);
        if (autocomplete.visible && autocomplete.enabled) {
            // Compute cursor pixel position in window coords
            int cur_line = 0, cur_col = 0;
            index_to_line_col(cursor_pos, cur_line, cur_col);
            pango_layout_set_text(layout, lines[cur_line].c_str(), (int)lines[cur_line].size());
            PangoRectangle cpos;
            pango_layout_get_cursor_pos(layout, cur_col, &cpos, nullptr);
            double cpx = text_area_x + 15.0 + (double)cpos.x / PANGO_SCALE - scroll_x;
            double cpy = text_area_y + (double)cur_line * line_h - scroll_y;
            autocomplete.draw(cr, cpx, cpy, width, height, tv, layout, fnt);
        }
    }

private:
    inline void reset_blink() {
        show_cursor = true; cursor_visible = true;
        last_blink = std::chrono::steady_clock::now();
    }

    // ............ Bracket matching ............
    // `buffer` is a rope: reading it character by character is slow. The search
    // therefore walks `lines` (plain strings) and only the cursor position is
    // converted once. Handles () [] {} with nesting; returns false if there is no match.
    bool find_matching_bracket_lc(int line, int col, int &out_line, int &out_col) const {
        if (line < 0 || line >= (int)lines.size()) return false;
        if (col < 0 || col >= (int)lines[line].size()) return false;
        char c = lines[line][col];
        char open_c = 0, close_c = 0;
        bool forward = true;
        if      (c == '(') { open_c = '('; close_c = ')'; forward = true;  }
        else if (c == ')') { open_c = '('; close_c = ')'; forward = false; }
        else if (c == '[') { open_c = '['; close_c = ']'; forward = true;  }
        else if (c == ']') { open_c = '['; close_c = ']'; forward = false; }
        else if (c == '{') { open_c = '{'; close_c = '}'; forward = true;  }
        else if (c == '}') { open_c = '{'; close_c = '}'; forward = false; }
        else return false;

        const long long MAX_SCAN = 20000000;   // safety limit for gigantic files
        long long scanned = 0;
        int depth = 0;
        if (forward) {
            for (int l = line; l < (int)lines.size(); ++l) {
                const std::string &t = lines[l];
                for (int i = (l == line ? col : 0); i < (int)t.size(); ++i) {
                    if (t[i] == open_c)  ++depth;
                    if (t[i] == close_c) --depth;
                    if (depth == 0) { out_line = l; out_col = i; return true; }
                }
                scanned += (long long)t.size();
                if (scanned > MAX_SCAN) return false;
            }
        } else {
            for (int l = line; l >= 0; --l) {
                const std::string &t = lines[l];
                for (int i = (l == line ? col : (int)t.size() - 1); i >= 0; --i) {
                    if (t[i] == close_c) ++depth;
                    if (t[i] == open_c)  --depth;
                    if (depth == 0) { out_line = l; out_col = i; return true; }
                }
                scanned += (long long)t.size();
                if (scanned > MAX_SCAN) return false;
            }
        }
        return false;
    }

    // Refreshes `bracket_pair` - only when the cursor moved or the text changed.
    // The bracket at the cursor is tried first, then the one just before it.
    void update_bracket_pair() {
        if (cursor_pos == bracket_key_cursor && buffer_rev == bracket_key_rev) return;
        bracket_key_cursor = cursor_pos;
        bracket_key_rev    = buffer_rev;
        bracket_pair.valid = false;

        int l, c;
        index_to_line_col(cursor_pos, l, c);
        int ml, mc;
        if (find_matching_bracket_lc(l, c, ml, mc)) {
            bracket_pair = {true, l, c, ml, mc};
        } else if (c > 0 && find_matching_bracket_lc(l, c - 1, ml, mc)) {
            bracket_pair = {true, l, c - 1, ml, mc};
        }
    }

    // Start selection anchoring if shift is held and no selection yet
    inline void begin_selection_if(bool shift) {
        if (shift && !sel_active) {
            sel_anchor = cursor_pos;
            sel_active = true;
        }
    }

    // After moving cursor: collapse or clear selection based on shift
    inline void update_selection_after(bool shift) {
        if (shift) {
            if (cursor_pos == sel_anchor) sel_active = false;
        } else {
            clear_selection();
        }
    }
};

#endif
