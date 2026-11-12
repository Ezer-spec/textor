#ifndef TEXTOR_ADCO_HXX
#define TEXTOR_ADCO_HXX

// ............................................................................
//  Adco - the language Textor uses for themes and addons  (*.adco)
//
//  Adco looks like Swift: `let`/`var`, `func`, `if`/`else`, `key: value`
//  members, trailing-less blocks, `//` and nestable `/* */` comments, string
//  interpolation `"\(expr)"` and `#rrggbb` colour literals.  One file may hold
//  any number of `theme`, `addon` and `language` declarations.
//
//      import adco.std
//
//      theme "Ocean Night" {
//          let base = rgb(0.24, 0.62, 0.90)
//          dark: true
//          colors {
//              bg:        gray(0.062)
//              accent:    base
//              selection: mix(base, #000000, 0.6)
//          }
//      }
//
//      addon "Lua Support" {
//          version: "1.0"
//          language "Lua" {
//              extensions: [".lua"]
//              keywords:   ["function", "end", "local"]
//              comment { line: "--" }
//          }
//      }
//
//  This header is the whole engine: it turns source text into a tree of
//  `Node`s (kind / label / properties / children) plus diagnostics.  What the
//  nodes *mean* (theme colours, language tables) is decided by the callers in
//  color/themes.hxx and base/addon_loader.hxx.
// ............................................................................

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <algorithm>

namespace adco {

struct Color { double r = 0, g = 0, b = 0, a = 1; };

struct Value {
    enum T { Nil, Num, Boolean, Str, Col, Arr } t = Nil;
    double n = 0;
    bool   b = false;
    std::string s;
    Color  c;
    std::vector<Value> a;

    static Value num(double v)               { Value x; x.t = Num;  x.n = v; return x; }
    static Value boolean(bool v)             { Value x; x.t = Boolean; x.b = v; return x; }
    static Value str(const std::string &v)   { Value x; x.t = Str;  x.s = v; return x; }
    static Value col(const Color &v)         { Value x; x.t = Col;  x.c = v; return x; }
    static Value arr(std::vector<Value> v)   { Value x; x.t = Arr;  x.a = std::move(v); return x; }

    bool truthy() const {
        switch (t) {
            case Boolean: return b;
            case Num:  return n != 0;
            case Str:  return !s.empty();
            case Arr:  return !a.empty();
            case Col:  return true;
            default:   return false;
        }
    }
};

struct Diag { int line = 0, col = 0; std::string msg; };

// Result of evaluating one declaration/block.
struct Node {
    std::string kind;      // "theme", "addon", "language", "colors", ...
    std::string label;     // the string after the keyword:  theme "Label"
    std::string extends;   // theme "A" extends "B"
    std::vector<std::string> attrs;   // @hidden etc.
    int line = 0;
    std::vector<std::pair<std::string, Value>> props;
    std::vector<Node> kids;

    const Value *get(const std::string &k) const {
        for (auto it = props.rbegin(); it != props.rend(); ++it)
            if (it->first == k) return &it->second;
        return nullptr;
    }
    const Node *kid(const std::string &k) const {
        for (auto &n : kids) if (n.kind == k) return &n;
        return nullptr;
    }
    bool has_attr(const std::string &a) const {
        return std::find(attrs.begin(), attrs.end(), a) != attrs.end();
    }
};

struct Document {
    std::vector<Node> decls;
    std::vector<std::string> imports;
    std::vector<Diag> diags;
};

// ............................................................................
//  Lexer
// ............................................................................
enum class Tk { End, Ident, Number, String, HexColor, Punct, Attr };

struct Token {
    Tk type = Tk::End;
    std::string text;       // ident name / punct / string body (raw, with escapes unresolved) / hex digits
    double num = 0;
    int line = 1, col = 1;
};

struct Lexer {
    const std::string &src;
    size_t i = 0;
    int line = 1, col = 1;
    std::vector<Diag> &diags;

    Lexer(const std::string &s, std::vector<Diag> &d) : src(s), diags(d) {}

    char cur(size_t o = 0) const { return i + o < src.size() ? src[i + o] : '\0'; }
    void adv() { if (cur() == '\n') { line++; col = 1; } else col++; i++; }

    void skip_trivia() {
        for (;;) {
            char c = cur();
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { adv(); continue; }
            if (c == '/' && cur(1) == '/') { while (cur() && cur() != '\n') adv(); continue; }
            if (c == '/' && cur(1) == '*') {
                int depth = 0; int sl = line, sc = col;
                do {
                    if (cur() == '/' && cur(1) == '*') { depth++; adv(); adv(); }
                    else if (cur() == '*' && cur(1) == '/') { depth--; adv(); adv(); }
                    else adv();
                } while (depth > 0 && cur());
                if (depth > 0) diags.push_back({sl, sc, "unterminated block comment"});
                continue;
            }
            break;
        }
    }

    std::vector<Token> run() {
        std::vector<Token> out;
        for (;;) {
            skip_trivia();
            Token t; t.line = line; t.col = col;
            char c = cur();
            if (!c) { t.type = Tk::End; out.push_back(t); break; }

            if (std::isalpha((unsigned char)c) || c == '_') {
                size_t s = i;
                while (std::isalnum((unsigned char)cur()) || cur() == '_') adv();
                t.type = Tk::Ident; t.text = src.substr(s, i - s);
            }
            else if (std::isdigit((unsigned char)c) || (c == '.' && std::isdigit((unsigned char)cur(1)))) {
                size_t s = i; bool dot = false;
                while (std::isdigit((unsigned char)cur()) || cur() == '_' || (cur() == '.' && !dot && std::isdigit((unsigned char)cur(1)))) {
                    if (cur() == '.') dot = true;
                    adv();
                }
                std::string raw; for (size_t k = s; k < i; k++) if (src[k] != '_') raw += src[k];
                t.type = Tk::Number; t.num = std::atof(raw.c_str()); t.text = raw;
            }
            else if (c == '"') {
                adv();
                std::string body; bool closed = false;
                while (cur()) {
                    if (cur() == '\\' && cur(1)) {
                        body += cur(); adv(); body += cur(); adv();
                        // keep an interpolation `\( ... )` intact, including nested parens
                        if (body.back() == '(') {
                            int d = 1;
                            while (cur() && d > 0) {
                                if (cur() == '(') d++;
                                else if (cur() == ')') d--;
                                body += cur(); adv();
                            }
                        }
                        continue;
                    }
                    if (cur() == '"') { adv(); closed = true; break; }
                    if (cur() == '\n') break;
                    body += cur(); adv();
                }
                if (!closed) diags.push_back({t.line, t.col, "unterminated string"});
                t.type = Tk::String; t.text = body;
            }
            else if (c == '#') {
                adv();
                size_t s = i;
                while (std::isxdigit((unsigned char)cur())) adv();
                t.type = Tk::HexColor; t.text = src.substr(s, i - s);
                size_t n = t.text.size();
                if (n != 3 && n != 6 && n != 8)
                    diags.push_back({t.line, t.col, "colour literal must be #rgb, #rrggbb or #rrggbbaa"});
            }
            else if (c == '@') {
                adv(); size_t s = i;
                while (std::isalnum((unsigned char)cur()) || cur() == '_') adv();
                t.type = Tk::Attr; t.text = src.substr(s, i - s);
            }
            else {
                t.type = Tk::Punct;
                char d = cur(1);
                if ((c == '-' && d == '>') || (c == '=' && d == '=') || (c == '!' && d == '=') ||
                    (c == '<' && d == '=') || (c == '>' && d == '=') || (c == '&' && d == '&') ||
                    (c == '|' && d == '|')) {
                    t.text = std::string() + c + d; adv(); adv();
                } else { t.text = std::string(1, c); adv(); }
            }
            out.push_back(t);
        }
        return out;
    }
};

// ............................................................................
//  AST
// ............................................................................
struct Expr; struct Stmt;
using ExprP = std::shared_ptr<Expr>;
using StmtP = std::shared_ptr<Stmt>;

struct Expr {
    enum K { Lit, Id, Call, Member, Array, Unary, Binary, Ternary, Interp } k = Lit;
    Value lit;                      // Lit
    std::string name;               // Id / Call / Member / operator
    std::vector<ExprP> args;        // Call args / Array items / Interp parts
    ExprP a, b, c;                  // Unary(a) Binary(a,b) Ternary(a,b,c) Member(a)
    int line = 0, col = 0;
};

struct Stmt {
    enum K { Let, Prop, Block, Func, If, Return, Import } k = Let;
    std::string name;               // let/prop name, block kind, func name, import path
    std::string label, extends;     // block
    std::vector<std::string> attrs;
    std::vector<std::string> params;
    ExprP e;                        // let/prop value, if condition, return value
    std::vector<StmtP> body, elseBody;
    int line = 0, col = 0;
};

// ............................................................................
//  Parser
// ............................................................................
struct Parser {
    std::vector<Token> t;
    size_t p = 0;
    std::vector<Diag> &diags;
    int depth = 0;

    Parser(std::vector<Token> toks, std::vector<Diag> &d) : t(std::move(toks)), diags(d) {}

    const Token &cur(size_t o = 0) const { return t[std::min(p + o, t.size() - 1)]; }
    bool at_end() const { return cur().type == Tk::End; }
    bool is_p(const char *s, size_t o = 0) const { return cur(o).type == Tk::Punct && cur(o).text == s; }
    bool is_id(const char *s, size_t o = 0) const { return cur(o).type == Tk::Ident && cur(o).text == s; }
    void err(const std::string &m) { diags.push_back({cur().line, cur().col, m}); }
    bool eat_p(const char *s) { if (is_p(s)) { p++; return true; } return false; }
    bool expect_p(const char *s) {
        if (eat_p(s)) return true;
        err(std::string("expected '") + s + "'"); return false;
    }

    // ............ expressions ............
    ExprP mk(Expr::K k) { auto e = std::make_shared<Expr>(); e->k = k; e->line = cur().line; e->col = cur().col; return e; }

    static Color parse_hex(const std::string &h) {
        Color c; auto hv = [](char ch) { return std::isdigit((unsigned char)ch) ? ch - '0' : (std::tolower(ch) - 'a' + 10); };
        if (h.size() == 3) {
            c.r = hv(h[0]) * 17 / 255.0; c.g = hv(h[1]) * 17 / 255.0; c.b = hv(h[2]) * 17 / 255.0;
        } else if (h.size() == 6 || h.size() == 8) {
            c.r = (hv(h[0]) * 16 + hv(h[1])) / 255.0;
            c.g = (hv(h[2]) * 16 + hv(h[3])) / 255.0;
            c.b = (hv(h[4]) * 16 + hv(h[5])) / 255.0;
            if (h.size() == 8) c.a = (hv(h[6]) * 16 + hv(h[7])) / 255.0;
        }
        return c;
    }

    // Plain-text strings (block labels, keys, import paths): resolve escapes only.
    static std::string unescape(const std::string &s) {
        std::string o;
        for (size_t i = 0; i < s.size(); i++) {
            if (s[i] != '\\' || i + 1 >= s.size()) { o += s[i]; continue; }
            char n = s[++i];
            o += n == 'n' ? '\n' : (n == 't' ? '\t' : n);
        }
        return o;
    }

    ExprP parse_string_expr(const Token &tok) {
        // Resolve escapes; split out \( ... ) interpolations.
        auto e = std::make_shared<Expr>(); e->line = tok.line; e->col = tok.col;
        std::vector<ExprP> parts; std::string lit;
        auto flush = [&]() {
            if (lit.empty()) return;
            auto l = std::make_shared<Expr>(); l->k = Expr::Lit; l->lit = Value::str(lit);
            parts.push_back(l); lit.clear();
        };
        const std::string &s = tok.text;
        for (size_t i = 0; i < s.size(); i++) {
            if (s[i] != '\\' || i + 1 >= s.size()) { lit += s[i]; continue; }
            char n = s[++i];
            if (n == 'n') lit += '\n';
            else if (n == 't') lit += '\t';
            else if (n == '(') {
                int d = 1; size_t st = i + 1, j = st;
                while (j < s.size() && d > 0) { if (s[j] == '(') d++; else if (s[j] == ')') d--; j++; }
                std::string inner = s.substr(st, j - 1 - st);
                flush();
                Lexer lx(inner, diags);
                // positions inside interpolations are relative; good enough for diagnostics
                Parser sub(lx.run(), diags); sub.depth = depth + 1;
                if (sub.depth < 16) parts.push_back(sub.parse_expr());
                i = j - 1;
            }
            else lit += n;   // \" \\ and unknown escapes
        }
        flush();
        if (parts.empty()) { e->k = Expr::Lit; e->lit = Value::str(""); return e; }
        if (parts.size() == 1 && parts[0]->k == Expr::Lit) return parts[0];
        e->k = Expr::Interp; e->args = std::move(parts);
        return e;
    }

    ExprP parse_primary() {
        const Token &tk = cur();
        if (tk.type == Tk::Number) { auto e = mk(Expr::Lit); e->lit = Value::num(tk.num); p++; return e; }
        if (tk.type == Tk::String) { Token c = tk; p++; return parse_string_expr(c); }
        if (tk.type == Tk::HexColor) { auto e = mk(Expr::Lit); e->lit = Value::col(parse_hex(tk.text)); p++; return e; }
        if (tk.type == Tk::Ident) {
            if (tk.text == "true" || tk.text == "false") { auto e = mk(Expr::Lit); e->lit = Value::boolean(tk.text == "true"); p++; return e; }
            if (tk.text == "nil") { auto e = mk(Expr::Lit); p++; return e; }
            auto e = mk(Expr::Id); e->name = tk.text; p++;
            if (is_p("(")) {
                p++; e->k = Expr::Call;
                while (!at_end() && !is_p(")")) {
                    // optional Swift-style argument label:  mix(a, b, t: 0.5)
                    if (cur().type == Tk::Ident && is_p(":", 1)) p += 2;
                    e->args.push_back(parse_expr());
                    if (!eat_p(",")) break;
                }
                expect_p(")");
            }
            return e;
        }
        if (is_p("(")) { p++; auto e = parse_expr(); expect_p(")"); return e; }
        if (is_p("[")) {
            auto e = mk(Expr::Array); p++;
            while (!at_end() && !is_p("]")) {
                e->args.push_back(parse_expr());
                if (!eat_p(",")) break;
            }
            expect_p("]");
            return e;
        }
        err("expected an expression");
        if (!at_end() && !is_p("}") && !is_p(")") && !is_p("]")) p++;
        return mk(Expr::Lit);
    }

    ExprP parse_postfix() {
        ExprP e = parse_primary();
        while (is_p(".") && cur(1).type == Tk::Ident) {
            auto m = mk(Expr::Member); p++;
            m->name = cur().text; p++; m->a = e; e = m;
        }
        return e;
    }

    ExprP parse_unary() {
        if (is_p("-") || is_p("!")) {
            auto e = mk(Expr::Unary); e->name = cur().text; p++;
            e->a = parse_unary(); return e;
        }
        return parse_postfix();
    }

    static int prec(const std::string &op) {
        if (op == "||") return 1;
        if (op == "&&") return 2;
        if (op == "==" || op == "!=") return 3;
        if (op == "<" || op == ">" || op == "<=" || op == ">=") return 4;
        if (op == "+" || op == "-") return 5;
        if (op == "*" || op == "/" || op == "%") return 6;
        return -1;
    }

    ExprP parse_bin(int minp) {
        ExprP l = parse_unary();
        for (;;) {
            if (cur().type != Tk::Punct) break;
            int pr = prec(cur().text);
            if (pr < minp) break;
            auto e = mk(Expr::Binary); e->name = cur().text; p++;
            e->a = l; e->b = parse_bin(pr + 1); l = e;
        }
        return l;
    }

    ExprP parse_expr() {
        if (++depth > 64) { err("expression nested too deeply"); depth--; return mk(Expr::Lit); }
        ExprP c = parse_bin(1);
        if (is_p("?")) {
            auto e = mk(Expr::Ternary); p++;
            e->a = c; e->b = parse_expr(); expect_p(":"); e->c = parse_expr();
            c = e;
        }
        depth--;
        return c;
    }

    // ............ statements ............
    std::vector<StmtP> parse_block_body() {
        std::vector<StmtP> out;
        while (!at_end() && !is_p("}")) {
            size_t before = p;
            if (eat_p(";") || eat_p(",")) continue;
            StmtP s = parse_stmt();
            if (s) out.push_back(s);
            if (p == before) p++;   // always make progress
        }
        return out;
    }

    StmtP parse_stmt() {
        auto s = std::make_shared<Stmt>(); s->line = cur().line; s->col = cur().col;

        while (cur().type == Tk::Attr) {
            s->attrs.push_back(cur().text); p++;
            if (is_p("(")) { int d = 0; do { if (is_p("(")) d++; if (is_p(")")) d--; p++; } while (d > 0 && !at_end()); }
        }

        if (is_id("import")) {
            p++; s->k = Stmt::Import;
            if (cur().type == Tk::String) { s->name = unescape(cur().text); p++; }
            else {
                while (cur().type == Tk::Ident) {
                    s->name += cur().text; p++;
                    if (is_p(".") && cur(1).type == Tk::Ident) { s->name += "."; p++; } else break;
                }
            }
            return s;
        }
        if (is_id("let") || is_id("var")) {
            p++; s->k = Stmt::Let;
            if (cur().type != Tk::Ident) { err("expected a name after let/var"); return nullptr; }
            s->name = cur().text; p++;
            if (eat_p(":")) { if (cur().type == Tk::Ident) p++; }   // optional type annotation
            if (!expect_p("=")) return nullptr;
            s->e = parse_expr();
            return s;
        }
        if (is_id("func")) {
            p++; s->k = Stmt::Func;
            if (cur().type != Tk::Ident) { err("expected a function name"); return nullptr; }
            s->name = cur().text; p++;
            expect_p("(");
            while (!at_end() && !is_p(")")) {
                if (cur().type == Tk::Ident) {
                    // `_ name: Type` or `name: Type` or `name`
                    std::string nm = cur().text; p++;
                    if (cur().type == Tk::Ident && !is_p(":")) { nm = cur().text; p++; }
                    if (eat_p(":")) { if (cur().type == Tk::Ident) p++; }
                    s->params.push_back(nm);
                } else p++;
                if (!eat_p(",")) break;
            }
            expect_p(")");
            if (eat_p("->")) { if (cur().type == Tk::Ident) p++; }
            if (!expect_p("{")) return nullptr;
            s->body = parse_block_body();
            expect_p("}");
            return s;
        }
        if (is_id("return")) { p++; s->k = Stmt::Return; s->e = parse_expr(); return s; }
        if (is_id("if")) {
            p++; s->k = Stmt::If; s->e = parse_expr();
            if (!expect_p("{")) return nullptr;
            s->body = parse_block_body(); expect_p("}");
            if (is_id("else")) {
                p++;
                if (is_id("if")) { auto e = parse_stmt(); if (e) s->elseBody.push_back(e); }
                else if (expect_p("{")) { s->elseBody = parse_block_body(); expect_p("}"); }
            }
            return s;
        }

        // key: value   |   key = value   |   "key": value
        if ((cur().type == Tk::Ident || cur().type == Tk::String) &&
            (is_p(":", 1) || is_p("=", 1))) {
            s->k = Stmt::Prop; s->name = cur().type == Tk::String ? unescape(cur().text) : cur().text; p += 2;
            s->e = parse_expr();
            return s;
        }
        // kind ["label"] [extends "parent"] { ... }
        if (cur().type == Tk::Ident) {
            s->k = Stmt::Block; s->name = cur().text; p++;
            if (cur().type == Tk::String) { s->label = unescape(cur().text); p++; }
            if (is_id("extends")) { p++; if (cur().type == Tk::String) { s->extends = unescape(cur().text); p++; } else err("expected a name after extends"); }
            if (!expect_p("{")) return nullptr;
            s->body = parse_block_body();
            expect_p("}");
            return s;
        }
        err("unexpected '" + cur().text + "'");
        return nullptr;
    }

    std::vector<StmtP> parse_file() {
        std::vector<StmtP> out;
        while (!at_end()) {
            size_t before = p;
            if (eat_p(";")) continue;
            if (is_p("}")) { err("unexpected '}'"); p++; continue; }
            StmtP s = parse_stmt();
            if (s) out.push_back(s);
            if (p == before) p++;
        }
        return out;
    }
};

// .............................................................................
//  Evaluator
// .............................................................................
struct Env {
    Env *parent = nullptr;
    std::map<std::string, Value> vars;
    std::map<std::string, StmtP> funcs;
    bool returned = false;
    Value ret;

    explicit Env(Env *p = nullptr) : parent(p) {}
    const Value *find(const std::string &n) const {
        for (const Env *e = this; e; e = e->parent) {
            auto it = e->vars.find(n); if (it != e->vars.end()) return &it->second;
        }
        return nullptr;
    }
    StmtP find_func(const std::string &n) const {
        for (const Env *e = this; e; e = e->parent) {
            auto it = e->funcs.find(n); if (it != e->funcs.end()) return it->second;
        }
        return nullptr;
    }
};

struct Evaluator {
    Document &doc;
    int call_depth = 0;
    Evaluator(Document &d) : doc(d) {}

    void err(int line, int col, const std::string &m) { doc.diags.push_back({line, col, m}); }

    static double clamp01(double v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
    static Color mixc(const Color &a, const Color &b, double t) {
        t = clamp01(t);
        return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t };
    }

    static std::string to_str(const Value &v) {
        char buf[64];
        switch (v.t) {
            case Value::Str:  return v.s;
            case Value::Num:  std::snprintf(buf, sizeof buf, "%g", v.n); return buf;
            case Value::Boolean: return v.b ? "true" : "false";
            case Value::Col:
                std::snprintf(buf, sizeof buf, "#%02x%02x%02x",
                    (int)std::lround(clamp01(v.c.r) * 255), (int)std::lround(clamp01(v.c.g) * 255), (int)std::lround(clamp01(v.c.b) * 255));
                return buf;
            case Value::Arr: {
                std::string s = "[";
                for (size_t i = 0; i < v.a.size(); i++) { if (i) s += ", "; s += to_str(v.a[i]); }
                return s + "]";
            }
            default: return "nil";
        }
    }

    static bool equal(const Value &a, const Value &b) {
        if (a.t != b.t) return false;
        switch (a.t) {
            case Value::Num:  return a.n == b.n;
            case Value::Boolean: return a.b == b.b;
            case Value::Str:  return a.s == b.s;
            case Value::Col:  return a.c.r == b.c.r && a.c.g == b.c.g && a.c.b == b.c.b && a.c.a == b.c.a;
            case Value::Nil:  return true;
            case Value::Arr:
                if (a.a.size() != b.a.size()) return false;
                for (size_t i = 0; i < a.a.size(); i++) if (!equal(a.a[i], b.a[i])) return false;
                return true;
        }
        return false;
    }

    // Reads a colour-ish argument: Color, or hex string "#rrggbb".
    bool as_color(const Value &v, Color &out) {
        if (v.t == Value::Col) { out = v.c; return true; }
        if (v.t == Value::Str) {
            std::string h = v.s; if (!h.empty() && h[0] == '#') h.erase(0, 1);
            if ((h.size() == 3 || h.size() == 6 || h.size() == 8) &&
                std::all_of(h.begin(), h.end(), [](char c) { return std::isxdigit((unsigned char)c); })) {
                out = Parser::parse_hex(h); return true;
            }
        }
        if (v.t == Value::Num) { double g = clamp01(v.n); out = {g, g, g, 1}; return true; }
        return false;
    }

    Value eval(const ExprP &e, Env &env) {
        if (!e) return {};
        switch (e->k) {
            case Expr::Lit: return e->lit;
            case Expr::Id: {
                const Value *v = env.find(e->name);
                if (v) return *v;
                err(e->line, e->col, "unknown name '" + e->name + "'");
                return {};
            }
            case Expr::Array: {
                std::vector<Value> items; for (auto &a : e->args) items.push_back(eval(a, env));
                return Value::arr(std::move(items));
            }
            case Expr::Interp: {
                std::string s; for (auto &a : e->args) s += to_str(eval(a, env));
                return Value::str(s);
            }
            case Expr::Unary: {
                Value v = eval(e->a, env);
                if (e->name == "!") return Value::boolean(!v.truthy());
                if (v.t == Value::Num) return Value::num(-v.n);
                err(e->line, e->col, "cannot negate this value"); return {};
            }
            case Expr::Ternary: return eval(e->a, env).truthy() ? eval(e->b, env) : eval(e->c, env);
            case Expr::Member: {
                Value v = eval(e->a, env);
                const std::string &m = e->name;
                if (v.t == Value::Col) {
                    if (m == "r") return Value::num(v.c.r);
                    if (m == "g") return Value::num(v.c.g);
                    if (m == "b") return Value::num(v.c.b);
                    if (m == "a") return Value::num(v.c.a);
                    if (m == "luma") return Value::num(0.2126 * v.c.r + 0.7152 * v.c.g + 0.0722 * v.c.b);
                }
                if ((v.t == Value::Arr) && m == "count") return Value::num((double)v.a.size());
                if ((v.t == Value::Str) && m == "count") return Value::num((double)v.s.size());
                err(e->line, e->col, "no member '" + m + "'"); return {};
            }
            case Expr::Binary: return eval_binary(e, env);
            case Expr::Call:   return eval_call(e, env);
        }
        return {};
    }

    Value eval_binary(const ExprP &e, Env &env) {
        const std::string &op = e->name;
        if (op == "&&") { Value l = eval(e->a, env); return l.truthy() ? Value::boolean(eval(e->b, env).truthy()) : Value::boolean(false); }
        if (op == "||") { Value l = eval(e->a, env); return l.truthy() ? Value::boolean(true) : Value::boolean(eval(e->b, env).truthy()); }
        Value l = eval(e->a, env), r = eval(e->b, env);
        if (op == "==") return Value::boolean(equal(l, r));
        if (op == "!=") return Value::boolean(!equal(l, r));
        if (l.t == Value::Num && r.t == Value::Num) {
            if (op == "+") return Value::num(l.n + r.n);
            if (op == "-") return Value::num(l.n - r.n);
            if (op == "*") return Value::num(l.n * r.n);
            if (op == "/") { if (r.n == 0) { err(e->line, e->col, "division by zero"); return {}; } return Value::num(l.n / r.n); }
            if (op == "%") { if (r.n == 0) { err(e->line, e->col, "division by zero"); return {}; } return Value::num(std::fmod(l.n, r.n)); }
            if (op == "<")  return Value::boolean(l.n < r.n);
            if (op == ">")  return Value::boolean(l.n > r.n);
            if (op == "<=") return Value::boolean(l.n <= r.n);
            if (op == ">=") return Value::boolean(l.n >= r.n);
        }
        if (op == "+" && (l.t == Value::Str || r.t == Value::Str)) return Value::str(to_str(l) + to_str(r));
        if (op == "+" && l.t == Value::Arr && r.t == Value::Arr) {
            Value o = l; o.a.insert(o.a.end(), r.a.begin(), r.a.end()); return o;
        }
        if (l.t == Value::Col && r.t == Value::Num && (op == "*" || op == "/")) {
            double k = op == "*" ? r.n : (r.n == 0 ? 0 : 1.0 / r.n);
            return Value::col({clamp01(l.c.r * k), clamp01(l.c.g * k), clamp01(l.c.b * k), l.c.a});
        }
        err(e->line, e->col, "operator '" + op + "' cannot be applied to these values");
        return {};
    }

    Value eval_call(const ExprP &e, Env &env) {
        std::vector<Value> a; for (auto &x : e->args) a.push_back(eval(x, env));
        const std::string &f = e->name;
        auto need = [&](size_t lo, size_t hi) {
            if (a.size() < lo || a.size() > hi) {
                err(e->line, e->col, "'" + f + "' expects " + std::to_string(lo) + (lo == hi ? "" : "-" + std::to_string(hi)) + " argument(s)");
                return false;
            }
            return true;
        };
        auto num = [&](size_t i, double d = 0) { return i < a.size() && a[i].t == Value::Num ? a[i].n : d; };

        // user-defined functions shadow builtins
        if (StmtP fn = env.find_func(f)) {
            if (++call_depth > 48) { err(e->line, e->col, "function '" + f + "' recurses too deeply"); call_depth--; return {}; }
            Env local(&env);
            for (size_t i = 0; i < fn->params.size(); i++) local.vars[fn->params[i]] = i < a.size() ? a[i] : Value();
            exec(fn->body, local, nullptr);
            call_depth--;
            return local.ret;
        }

        if (f == "rgb")  { if (!need(3, 4)) return {}; return Value::col({clamp01(num(0)), clamp01(num(1)), clamp01(num(2)), a.size() > 3 ? clamp01(num(3)) : 1.0}); }
        if (f == "rgb8") { if (!need(3, 4)) return {}; return Value::col({clamp01(num(0) / 255), clamp01(num(1) / 255), clamp01(num(2) / 255), a.size() > 3 ? clamp01(num(3)) : 1.0}); }
        if (f == "gray") { if (!need(1, 2)) return {}; double g = clamp01(num(0)); return Value::col({g, g, g, a.size() > 1 ? clamp01(num(1)) : 1.0}); }
        if (f == "hex") {
            if (!need(1, 1)) return {};
            Color c; if (as_color(a[0], c)) return Value::col(c);
            err(e->line, e->col, "hex() needs a string like \"#3d9ee6\""); return {};
        }
        if (f == "hsl") {
            if (!need(3, 4)) return {};
            double h = std::fmod(std::fmod(num(0), 360.0) + 360.0, 360.0) / 60.0, s = clamp01(num(1)), l = clamp01(num(2));
            double c = (1 - std::fabs(2 * l - 1)) * s, x = c * (1 - std::fabs(std::fmod(h, 2.0) - 1)), m = l - c / 2;
            double r = 0, g = 0, b = 0;
            if (h < 1) { r = c; g = x; } else if (h < 2) { r = x; g = c; } else if (h < 3) { g = c; b = x; }
            else if (h < 4) { g = x; b = c; } else if (h < 5) { r = x; b = c; } else { r = c; b = x; }
            return Value::col({r + m, g + m, b + m, a.size() > 3 ? clamp01(num(3)) : 1.0});
        }
        if (f == "mix") {
            if (!need(3, 3)) return {};
            Color x, y; if (!as_color(a[0], x) || !as_color(a[1], y)) { err(e->line, e->col, "mix() needs two colours"); return {}; }
            return Value::col(mixc(x, y, num(2)));
        }
        if (f == "lighten" || f == "darken") {
            if (!need(2, 2)) return {};
            Color x; if (!as_color(a[0], x)) { err(e->line, e->col, f + "() needs a colour"); return {}; }
            Color target = f == "lighten" ? Color{1, 1, 1, x.a} : Color{0, 0, 0, x.a};
            return Value::col(mixc(x, target, num(1)));
        }
        if (f == "alpha") {
            if (!need(2, 2)) return {};
            Color x; if (!as_color(a[0], x)) { err(e->line, e->col, "alpha() needs a colour"); return {}; }
            x.a = clamp01(num(1)); return Value::col(x);
        }
        if (f == "min")   { if (!need(2, 2)) return {}; return Value::num(std::min(num(0), num(1))); }
        if (f == "max")   { if (!need(2, 2)) return {}; return Value::num(std::max(num(0), num(1))); }
        if (f == "abs")   { if (!need(1, 1)) return {}; return Value::num(std::fabs(num(0))); }
        if (f == "round") { if (!need(1, 1)) return {}; return Value::num(std::round(num(0))); }
        if (f == "clamp") { if (!need(3, 3)) return {}; return Value::num(std::max(num(1), std::min(num(2), num(0)))); }
        if (f == "count") {
            if (!need(1, 1)) return {};
            return Value::num(a[0].t == Value::Arr ? (double)a[0].a.size() : (double)a[0].s.size());
        }
        if (f == "upper" || f == "lower") {
            if (!need(1, 1)) return {};
            std::string s = to_str(a[0]);
            for (auto &c : s) c = (char)(f == "upper" ? std::toupper((unsigned char)c) : std::tolower((unsigned char)c));
            return Value::str(s);
        }
        err(e->line, e->col, "unknown function '" + f + "'");
        return {};
    }

    // Executes statements. `out` collects properties/blocks (null inside functions).
    void exec(const std::vector<StmtP> &body, Env &env, Node *out) {
        for (auto &s : body) {
            if (env.returned) return;
            switch (s->k) {
                case Stmt::Import:
                    if (!out || out->kind.empty()) doc.imports.push_back(s->name);
                    break;
                case Stmt::Let:
                    env.vars[s->name] = eval(s->e, env);
                    break;
                case Stmt::Func:
                    env.funcs[s->name] = s;
                    break;
                case Stmt::Return:
                    env.ret = eval(s->e, env); env.returned = true;
                    // `return` inside an if-branch run in the same Env propagates naturally
                    break;
                case Stmt::If: {
                    bool c = eval(s->e, env).truthy();
                    exec(c ? s->body : s->elseBody, env, out);
                    break;
                }
                case Stmt::Prop: {
                    Value v = eval(s->e, env);
                    if (out) out->props.push_back({s->name, v});
                    env.vars[s->name] = v;
                    break;
                }
                case Stmt::Block: {
                    Node n; n.kind = s->name; n.label = s->label; n.extends = s->extends;
                    n.attrs = s->attrs; n.line = s->line;
                    Env scope(&env);
                    exec(s->body, scope, &n);
                    if (out) out->kids.push_back(std::move(n));
                    break;
                }
            }
        }
    }
};

// Parse + evaluate a whole .adco source text.
static inline Document evaluate(const std::string &src) {
    Document doc;
    Lexer lx(src, doc.diags);
    Parser ps(lx.run(), doc.diags);
    std::vector<StmtP> prog = ps.parse_file();

    Evaluator ev(doc);
    Env global;
    Node root;                       // kind == "" marks the file level
    ev.exec(prog, global, &root);
    doc.decls = std::move(root.kids);
    // Top-level `let`s are file-scoped; stray top-level properties are an error.
    for (auto &p : root.props)
        doc.diags.push_back({0, 0, "'" + p.first + "' must be inside a theme, addon or language"});
    return doc;
}

}

#endif
