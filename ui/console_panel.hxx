#ifndef CONSOLE_PANEL_HXX
#define CONSOLE_PANEL_HXX

#include <string>
#include "../base/keytext.hxx"
#include <vector>
#include <deque>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <chrono>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#if defined(__linux__)
#include <pty.h>
#elif defined(__FreeBSD__) || defined(__APPLE__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <libutil.h>
#else
#include <pty.h>
#endif
#include <termios.h>
#include <signal.h>
#include <pango/pangocairo.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include "svg_icons.hxx"
#include "../color/themes.hxx"

//  ANSI colour palette
struct Rgb { float r,g,b; };

static const Rgb ANSI_PALETTE[256] = {
    {0.12f,0.12f,0.12f},{0.80f,0.21f,0.21f},{0.21f,0.72f,0.29f},{0.85f,0.75f,0.18f},
    {0.28f,0.48f,0.90f},{0.70f,0.28f,0.82f},{0.20f,0.76f,0.84f},{0.82f,0.82f,0.82f},
    {0.40f,0.40f,0.40f},{1.0f,0.35f,0.35f},{0.35f,0.95f,0.40f},{1.0f,0.95f,0.35f},
    {0.45f,0.65f,1.0f},{0.90f,0.45f,1.0f},{0.35f,0.95f,1.0f},{1.0f,1.0f,1.0f},
};

static Rgb ansi_color(int idx) {
    if (idx < 0) return {0,0,0};
    if (idx < 16) return ANSI_PALETTE[idx];
    if (idx < 232) {
        idx -= 16;
        int b = idx%6, g = (idx/6)%6, r = idx/36;
        auto v=[](int x)->float{return x?0.137f+x*0.137f:0.0f;};
        return {v(r),v(g),v(b)};
    }
    float g2 = 0.078f + (idx-232)*0.039f;
    return {g2,g2,g2};
}

//  Cell: one terminal character
struct Cell {
    char32_t ch      = U' ';
    int      fg      = -1;
    int      bg      = -1;
    bool     bold    = false;
    bool     italic  = false;
    bool     underline = false;
    bool     reverse = false;
    bool     dirty   = true;
};

//  TermScreen: fixed-size grid of cells
struct TermScreen {
    int cols = 80, rows = 24;
    std::vector<std::vector<Cell>> cells;
    int cur_col = 0, cur_row = 0;
    bool cur_visible = true;
    int  attr_fg = -1, attr_bg = -1;
    bool attr_bold = false, attr_italic = false;
    bool attr_underline = false, attr_reverse = false;
    int saved_col = 0, saved_row = 0;
    int scroll_top = 0, scroll_bot = -1;

    void resize(int c, int r) {
        cols = std::max(1,c); rows = std::max(1,r);
        cells.assign(rows, std::vector<Cell>(cols));
        cur_col = std::min(cur_col, cols-1);
        cur_row = std::min(cur_row, rows-1);
        scroll_top = 0; scroll_bot = rows-1;
    }
    void clear_all() {
        for (auto &row : cells)
            for (auto &c : row) { c.ch=U' '; c.fg=-1; c.bg=-1; c.bold=c.italic=c.underline=c.reverse=false; c.dirty=true; }
    }
    Cell &at(int r, int c) {
        r = std::clamp(r,0,rows-1); c = std::clamp(c,0,cols-1);
        return cells[r][c];
    }
    void set_cell(int r, int c, char32_t ch) {
        auto &cell = at(r,c);
        cell.ch = ch; cell.fg = attr_fg; cell.bg = attr_bg;
        cell.bold = attr_bold; cell.italic = attr_italic;
        cell.underline = attr_underline; cell.reverse = attr_reverse;
        cell.dirty = true;
    }
    void scroll_up(int top, int bot, int n=1) {
        for (int i = 0; i < n; i++) {
            for (int r = top; r < bot; r++) cells[r] = cells[r+1];
            cells[bot].assign(cols, Cell{});
        }
    }
    void scroll_down_region(int top, int bot, int n=1) {
        for (int i = 0; i < n; i++) {
            for (int r = bot; r > top; r--) cells[r] = cells[r-1];
            cells[top].assign(cols, Cell{});
        }
    }
    int sbot() const { return scroll_bot < 0 ? rows-1 : std::min(scroll_bot, rows-1); }
};

//  TermInstance: a single PTY-backed terminal (one console tab)
struct TermInstance {
    std::string title = "Terminal";
    int  pty_master = -1;
    pid_t pty_pid   = -1;
    bool pty_alive  = false;

    TermScreen screen;
    int cell_w = 8, cell_h = 16;
    bool cell_metrics_dirty = true;

    static const int SCROLLBACK_MAX = 4000;
    std::deque<std::vector<Cell>> scrollback;
    double scroll_offset   = 0.0;
    double scroll_vy       = 0.0;   // inertial velocity (lines/frame)

    std::string esc_buf;
    enum class ParseState { Normal, Esc, CSI, OSC, DCS } parse_state = ParseState::Normal;

    bool sel_active = false;
    int sel_r0=-1, sel_c0=-1, sel_r1=-1, sel_c1=-1;

    std::chrono::steady_clock::time_point last_blink = std::chrono::steady_clock::now();
    bool cursor_blink_on = true;
    std::string last_font_spec;

    TermInstance() { screen.resize(80, 24); }
    ~TermInstance() { kill_pty(); }

    // Non-copyable, movable
    TermInstance(const TermInstance &) = delete;
    TermInstance &operator=(const TermInstance &) = delete;
    TermInstance(TermInstance &&o) noexcept
        : title(std::move(o.title)), pty_master(o.pty_master), pty_pid(o.pty_pid),
          pty_alive(o.pty_alive), screen(std::move(o.screen)),
          cell_w(o.cell_w), cell_h(o.cell_h), cell_metrics_dirty(o.cell_metrics_dirty),
          scrollback(std::move(o.scrollback)), scroll_offset(o.scroll_offset),
          esc_buf(std::move(o.esc_buf)), parse_state(o.parse_state),
          sel_active(o.sel_active), last_blink(o.last_blink),
          cursor_blink_on(o.cursor_blink_on), last_font_spec(std::move(o.last_font_spec))
    { o.pty_master = -1; o.pty_pid = -1; o.pty_alive = false; }

    TermInstance &operator=(TermInstance &&o) noexcept {
        if (this == &o) return *this;
        kill_pty();
        title              = std::move(o.title);
        pty_master         = o.pty_master;         o.pty_master = -1;
        pty_pid            = o.pty_pid;             o.pty_pid    = -1;
        pty_alive          = o.pty_alive;           o.pty_alive  = false;
        screen             = std::move(o.screen);
        cell_w             = o.cell_w;
        cell_h             = o.cell_h;
        cell_metrics_dirty = o.cell_metrics_dirty;
        scrollback         = std::move(o.scrollback);
        scroll_offset      = o.scroll_offset;
        esc_buf            = std::move(o.esc_buf);
        parse_state        = o.parse_state;
        sel_active         = o.sel_active;
        last_blink         = o.last_blink;
        cursor_blink_on    = o.cursor_blink_on;
        last_font_spec     = std::move(o.last_font_spec);
        return *this;
    }

    void spawn_shell() {
        if (pty_alive) return;
        struct winsize ws{}; ws.ws_col = screen.cols; ws.ws_row = screen.rows;
        ws.ws_xpixel = screen.cols * cell_w; ws.ws_ypixel = screen.rows * cell_h;
        pty_pid = forkpty(&pty_master, nullptr, nullptr, &ws);
        if (pty_pid == 0) {
            const char *sh = getenv("SHELL"); if (!sh) sh = "/bin/bash";
            setenv("TERM", "xterm-256color", 1);
            setenv("COLORTERM", "truecolor", 1);
            setenv("TERM_PROGRAM", "Textor", 1);
            execl(sh, sh, nullptr); _exit(1);
        } else if (pty_pid > 0) {
            int flags = fcntl(pty_master, F_GETFL, 0);
            fcntl(pty_master, F_SETFL, flags | O_NONBLOCK);
            pty_alive = true;
            screen.clear_all(); screen.cur_col = screen.cur_row = 0;
            scrollback.clear(); scroll_offset = 0;
        }
    }

    void kill_pty() {
        if (pty_pid > 0) { kill(pty_pid, SIGHUP); int st; waitpid(pty_pid, &st, WNOHANG); pty_pid = -1; }
        if (pty_master >= 0) { close(pty_master); pty_master = -1; }
        pty_alive = false;
    }

    void resize_pty(int cols, int rows) {
        if (screen.cols == cols && screen.rows == rows) return;
        screen.resize(cols, rows);
        if (pty_master >= 0) {
            struct winsize ws{}; ws.ws_col = cols; ws.ws_row = rows;
            ws.ws_xpixel = cols * cell_w; ws.ws_ypixel = rows * cell_h;
            ioctl(pty_master, TIOCSWINSZ, &ws);
        }
    }

    void poll_pty() {
        if (!pty_alive || pty_master < 0) return;
        int status; pid_t r = waitpid(pty_pid, &status, WNOHANG);
        if (r == pty_pid) { pty_alive = false; return; }
        char buf[4096]; ssize_t n; int iters = 0;
        while ((n = read(pty_master, buf, sizeof(buf))) > 0 && iters < 32) {
            process_output(buf, (int)n); iters++;
        }
    }

    void pty_write(const char *data, int len) {
        if (!pty_alive || pty_master < 0) return;
        int written = 0;
        while (written < len) {
            ssize_t r = write(pty_master, data+written, len-written);
            if (r <= 0) break; written += (int)r;
        }
    }

    void process_output(const char *data, int len) {
        for (int i = 0; i < len; ) {
            unsigned char ch = (unsigned char)data[i];
            switch (parse_state) {
            case ParseState::Normal:
                if (ch == 0x1B) { parse_state = ParseState::Esc; i++; break; }
                if (ch == 0x0D) { screen.cur_col = 0; i++; break; }
                if (ch == 0x0A) { do_newline(); i++; break; }
                if (ch == 0x08) { if (screen.cur_col>0) screen.cur_col--; i++; break; }
                if (ch == 0x09) { screen.cur_col = (screen.cur_col/8+1)*8; if(screen.cur_col>=screen.cols)screen.cur_col=screen.cols-1; i++; break; }
                if (ch == 0x07) { i++; break; }
                if (ch >= 0x20 || ch == 0) {
                    char32_t codepoint = ch; int extra = 0;
                    if ((ch&0xE0)==0xC0)      { codepoint=ch&0x1F; extra=1; }
                    else if ((ch&0xF0)==0xE0)  { codepoint=ch&0x0F; extra=2; }
                    else if ((ch&0xF8)==0xF0)  { codepoint=ch&0x07; extra=3; }
                    i++;
                    for (int e=0;e<extra&&i<len;e++,i++) codepoint=(codepoint<<6)|((unsigned char)data[i]&0x3F);
                    if (screen.cur_col >= screen.cols) { screen.cur_col=0; do_newline(); }
                    screen.set_cell(screen.cur_row, screen.cur_col, codepoint);
                    screen.cur_col++;
                } else { i++; }
                break;
            case ParseState::Esc:
                if (ch == '[') { parse_state=ParseState::CSI; esc_buf.clear(); i++; break; }
                if (ch == ']') { parse_state=ParseState::OSC; esc_buf.clear(); i++; break; }
                if (ch == 'P') { parse_state=ParseState::DCS; esc_buf.clear(); i++; break; }
                if (ch == 'M') { if(screen.cur_row==screen.scroll_top) screen.scroll_down_region(screen.scroll_top,screen.sbot()); else if(screen.cur_row>0) screen.cur_row--; parse_state=ParseState::Normal; i++; break; }
                if (ch == '7') { screen.saved_col=screen.cur_col; screen.saved_row=screen.cur_row; parse_state=ParseState::Normal; i++; break; }
                if (ch == '8') { screen.cur_col=screen.saved_col; screen.cur_row=screen.saved_row; parse_state=ParseState::Normal; i++; break; }
                if (ch == 'c') { screen.clear_all(); screen.cur_col=screen.cur_row=0; parse_state=ParseState::Normal; i++; break; }
                parse_state=ParseState::Normal; i++; break;
            case ParseState::CSI:
                if (ch >= 0x40 && ch <= 0x7E) { process_csi(esc_buf,(char)ch); parse_state=ParseState::Normal; esc_buf.clear(); }
                else esc_buf += (char)ch;
                i++; break;
            case ParseState::OSC:
                if (ch==0x07||(ch==0x1B&&i+1<len&&data[i+1]=='\\')) {
                    // Handle OSC title set
                    if (esc_buf.size()>2 && esc_buf[0]=='0' && esc_buf[1]==';')
                        title = esc_buf.substr(2);
                    if(ch==0x1B) i++;
                    parse_state=ParseState::Normal; esc_buf.clear();
                } else esc_buf += (char)ch;
                i++; break;
            case ParseState::DCS:
                if (ch==0x1B) parse_state=ParseState::Normal; i++; break;
            }
        }
    }

    void do_newline() {
        int bot = screen.sbot();
        if (screen.cur_row < bot) { screen.cur_row++; }
        else {
            scrollback.push_back(screen.cells[screen.scroll_top]);
            if ((int)scrollback.size() > SCROLLBACK_MAX) scrollback.pop_front();
            screen.scroll_up(screen.scroll_top, bot);
        }
    }

    std::vector<int> parse_params(const std::string &s, int def=0) {
        std::vector<int> p;
        if (s.empty()) { p.push_back(def); return p; }
        std::istringstream ss(s); std::string tok;
        while (std::getline(ss, tok, ';')) {
            try { p.push_back(tok.empty()?def:std::stoi(tok)); } catch(...){p.push_back(def);}
        }
        return p;
    }

    void process_csi(const std::string &ps_str, char fc) {
        std::string ps = ps_str;
        bool priv = (!ps.empty() && ps[0]=='?'); if (priv) ps=ps.substr(1);
        auto P = parse_params(ps);
        auto p = [&](int i, int def=0){ return (i<(int)P.size()&&P[i]>0)?P[i]:def; };
        int bot = screen.sbot();
        switch (fc) {
        case 'A': screen.cur_row=std::max(0,screen.cur_row-p(0,1)); break;
        case 'B': screen.cur_row=std::min(screen.rows-1,screen.cur_row+p(0,1)); break;
        case 'C': screen.cur_col=std::min(screen.cols-1,screen.cur_col+p(0,1)); break;
        case 'D': screen.cur_col=std::max(0,screen.cur_col-p(0,1)); break;
        case 'G': screen.cur_col=std::clamp(p(0,1)-1,0,screen.cols-1); break;
        case 'H': case 'f': screen.cur_row=std::clamp(p(0,1)-1,0,screen.rows-1); screen.cur_col=std::clamp(p(1,1)-1,0,screen.cols-1); break;
        case 'J': { int mode=P.empty()?0:P[0];
            if(mode==0){for(int c=screen.cur_col;c<screen.cols;c++)screen.at(screen.cur_row,c).ch=U' ';for(int r=screen.cur_row+1;r<screen.rows;r++)screen.cells[r].assign(screen.cols,Cell{});}
            else if(mode==1){for(int r=0;r<screen.cur_row;r++)screen.cells[r].assign(screen.cols,Cell{});for(int c=0;c<=screen.cur_col;c++)screen.at(screen.cur_row,c).ch=U' ';}
            else if(mode==2||mode==3){screen.clear_all();screen.cur_col=screen.cur_row=0;}
            break;}
        case 'K': { int mode=P.empty()?0:P[0];
            if(mode==0)for(int c=screen.cur_col;c<screen.cols;c++)screen.at(screen.cur_row,c)=Cell{};
            else if(mode==1)for(int c=0;c<=screen.cur_col;c++)screen.at(screen.cur_row,c)=Cell{};
            else for(int c=0;c<screen.cols;c++)screen.at(screen.cur_row,c)=Cell{};
            break;}
        case 'L': screen.scroll_down_region(screen.cur_row,bot,p(0,1)); break;
        case 'M': screen.scroll_up(screen.cur_row,bot,p(0,1)); break;
        case 'P': { int n=p(0,1); for(int c=screen.cur_col;c<screen.cols-n;c++)screen.at(screen.cur_row,c)=screen.at(screen.cur_row,c+n); for(int c=screen.cols-n;c<screen.cols;c++)screen.at(screen.cur_row,c)=Cell{}; break;}
        case 'S': screen.scroll_up(screen.scroll_top,bot,p(0,1)); break;
        case 'T': screen.scroll_down_region(screen.scroll_top,bot,p(0,1)); break;
        case 'r': screen.scroll_top=std::clamp(p(0,1)-1,0,screen.rows-1); screen.scroll_bot=std::clamp(p(1,screen.rows)-1,screen.scroll_top,screen.rows-1); screen.cur_col=0;screen.cur_row=0; break;
        case 's': screen.saved_col=screen.cur_col;screen.saved_row=screen.cur_row; break;
        case 'u': screen.cur_col=screen.saved_col;screen.cur_row=screen.saved_row; break;
        case 'm': process_sgr(P); break;
        default: break;
        }
    }

    void process_sgr(const std::vector<int> &P) {
        if (P.empty()) { reset_attrs(); return; }
        for (int i = 0; i < (int)P.size(); ) {
            int v = P[i];
            if(v==0){reset_attrs();i++;continue;}
            if(v==1){screen.attr_bold=true;i++;continue;}
            if(v==3){screen.attr_italic=true;i++;continue;}
            if(v==4){screen.attr_underline=true;i++;continue;}
            if(v==7){screen.attr_reverse=true;i++;continue;}
            if(v==22){screen.attr_bold=false;i++;continue;}
            if(v==23){screen.attr_italic=false;i++;continue;}
            if(v==24){screen.attr_underline=false;i++;continue;}
            if(v==27){screen.attr_reverse=false;i++;continue;}
            if(v>=30&&v<=37){screen.attr_fg=v-30;i++;continue;}
            if(v==38){
                if(i+2<(int)P.size()&&P[i+1]==5){screen.attr_fg=P[i+2];i+=3;continue;}
                if(i+4<(int)P.size()&&P[i+1]==2){int r=(int)(P[i+2]/255.0f*5),g=(int)(P[i+3]/255.0f*5),b=(int)(P[i+4]/255.0f*5);screen.attr_fg=16+36*r+6*g+b;i+=5;continue;}
                i++;continue;}
            if(v==39){screen.attr_fg=-1;i++;continue;}
            if(v>=40&&v<=47){screen.attr_bg=v-40;i++;continue;}
            if(v==48){
                if(i+2<(int)P.size()&&P[i+1]==5){screen.attr_bg=P[i+2];i+=3;continue;}
                if(i+4<(int)P.size()&&P[i+1]==2){int r=(int)(P[i+2]/255.0f*5),g=(int)(P[i+3]/255.0f*5),b=(int)(P[i+4]/255.0f*5);screen.attr_bg=16+36*r+6*g+b;i+=5;continue;}
                i++;continue;}
            if(v==49){screen.attr_bg=-1;i++;continue;}
            if(v>=90&&v<=97){screen.attr_fg=v-90+8;i++;continue;}
            if(v>=100&&v<=107){screen.attr_bg=v-100+8;i++;continue;}
            i++;
        }
    }

    void reset_attrs() {
        screen.attr_fg=-1;screen.attr_bg=-1;
        screen.attr_bold=false;screen.attr_italic=false;
        screen.attr_underline=false;screen.attr_reverse=false;
    }

    void update_cell_metrics(PangoLayout *layout, PangoFontDescription *fnt) {
        if (!cell_metrics_dirty) return;
        pango_layout_set_font_description(layout, fnt);
        pango_layout_set_text(layout, "M", -1);
        PangoRectangle ink, log;
        pango_layout_get_extents(layout, &ink, &log);
        cell_w = std::max(1, (int)(log.width  / PANGO_SCALE));
        cell_h = std::max(1, (int)(log.height / PANGO_SCALE));
        cell_metrics_dirty = false;
    }

    void fit_to_area(int pixel_w, int pixel_h) {
        if (cell_w < 1 || cell_h < 1) return;
        int new_cols = std::max(10, pixel_w / cell_w);
        int new_rows = std::max(3,  pixel_h / cell_h);
        resize_pty(new_cols, new_rows);
    }

    // Draw this terminal into a given pixel rect
    void draw(cairo_t *cr, PangoLayout *layout, PangoFontDescription *fnt_arg,
              const ThemeVariant &tv,
              int x, int y, int w, int h, bool focused)
    {
        if (!pty_alive) spawn_shell();
        poll_pty();

        std::string fspec = tv.font_family + " " + std::to_string(tv.font_size);
        if (fspec != last_font_spec) { cell_metrics_dirty = true; last_font_spec = fspec; }

        PangoFontDescription *mono_fnt = pango_font_description_from_string(
            (std::string("Monospace ") + std::to_string(tv.font_size)).c_str());
        pango_layout_set_font_description(layout, mono_fnt);
        update_cell_metrics(layout, mono_fnt);

        int pad = 4;
        fit_to_area(w - pad*2 - 6, h - pad);

        // Background - slightly distinct from editor
        cairo_set_source_rgb(cr, tv.bg*0.90, tv.bg*0.90, tv.bg*0.94);
        cairo_rectangle(cr, x, y, w, h); cairo_fill(cr);

        cairo_save(cr);
        cairo_rectangle(cr, x+pad, y, w-pad*2-6, h-pad);
        cairo_clip(cr);

        auto now = std::chrono::steady_clock::now();
        double dt = std::chrono::duration<double>(now - last_blink).count();
        if (dt > 0.5) { cursor_blink_on = !cursor_blink_on; last_blink = now; }

        int total_lines = (int)scrollback.size() + screen.rows;
        int view_start  = total_lines - screen.rows - (int)scroll_offset;
        view_start = std::max(0, view_start);

        pango_layout_set_font_description(layout, mono_fnt);

        for (int vr = 0; vr < screen.rows; vr++) {
            int abs_line = view_start + vr;
            const std::vector<Cell> *row_ptr = nullptr;
            std::vector<Cell> empty_row(screen.cols);
            if (abs_line < (int)scrollback.size()) row_ptr = &scrollback[abs_line];
            else {
                int sr = abs_line - (int)scrollback.size();
                row_ptr = (sr>=0&&sr<screen.rows) ? &screen.cells[sr] : &empty_row;
            }
            const auto &row = *row_ptr;
            double ry = y + vr * cell_h;
            bool is_screen_row = (abs_line >= (int)scrollback.size());
            int screen_row = abs_line - (int)scrollback.size();

            for (int col = 0; col < screen.cols; col++) {
                const Cell &cell = (col<(int)row.size())?row[col]:empty_row[0];
                double cx2 = x + pad + col * cell_w;
                bool is_cursor = is_screen_row && scroll_offset<0.5
                              && screen_row==screen.cur_row && col==screen.cur_col
                              && cursor_blink_on && screen.cur_visible && focused;

                bool rev = cell.reverse;
                int fg_idx = rev ? cell.bg : cell.fg;
                int bg_idx = rev ? cell.fg : cell.bg;

                if (bg_idx >= 0) {
                    Rgb bg = ansi_color(bg_idx);
                    cairo_set_source_rgb(cr, bg.r, bg.g, bg.b);
                    cairo_rectangle(cr, cx2, ry, cell_w+1, cell_h); cairo_fill(cr);
                }
                if (is_cursor) {
                    cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, 0.85);
                    cairo_rectangle(cr, cx2, ry, cell_w, cell_h); cairo_fill(cr);
                }
                if (cell.ch != U' ' && cell.ch != 0) {
                    Rgb fg_col;
                    if (is_cursor) fg_col = {(float)tv.bg,(float)tv.bg,(float)tv.bg};
                    else if (fg_idx<0) fg_col = {(float)tv.text,(float)tv.text,(float)tv.text};
                    else { fg_col = ansi_color(fg_idx); if(cell.bold&&fg_idx<8)fg_col=ansi_color(fg_idx+8); }

                    cairo_set_source_rgb(cr, fg_col.r, fg_col.g, fg_col.b);

                    char utf8[8]={}; int utf8_len=0;
                    char32_t cp = cell.ch;
                    if(cp<0x80){utf8[0]=(char)cp;utf8_len=1;}
                    else if(cp<0x800){utf8[0]=0xC0|(cp>>6);utf8[1]=0x80|(cp&0x3F);utf8_len=2;}
                    else if(cp<0x10000){utf8[0]=0xE0|(cp>>12);utf8[1]=0x80|((cp>>6)&0x3F);utf8[2]=0x80|(cp&0x3F);utf8_len=3;}
                    else{utf8[0]=0xF0|(cp>>18);utf8[1]=0x80|((cp>>12)&0x3F);utf8[2]=0x80|((cp>>6)&0x3F);utf8[3]=0x80|(cp&0x3F);utf8_len=4;}

                    PangoFontDescription *cf = pango_font_description_copy(mono_fnt);
                    if(cell.bold) pango_font_description_set_weight(cf,PANGO_WEIGHT_BOLD);
                    if(cell.italic) pango_font_description_set_style(cf,PANGO_STYLE_ITALIC);
                    pango_layout_set_font_description(layout,cf);
                    pango_layout_set_text(layout,utf8,utf8_len);
                    cairo_move_to(cr,cx2,ry);
                    pango_cairo_show_layout(cr,layout);
                    pango_font_description_free(cf);
                    if(cell.underline){cairo_set_line_width(cr,1.0);cairo_move_to(cr,cx2,ry+cell_h-2);cairo_line_to(cr,cx2+cell_w,ry+cell_h-2);cairo_stroke(cr);}
                }
            }
        }

        pango_layout_set_font_description(layout, fnt_arg);

        cairo_restore(cr);

        // scrollbar for terminal
        {
            int total = (int)scrollback.size() + screen.rows;
            int vis   = screen.rows;
            if (total > vis && h > 20) {
                double sb_w_rest = 4.0;
                double sb_w_full = 8.0;
                // hover detection via scroll_offset > 0 as proxy for "user scrolled"
                double hover = scroll_offset > 0.5 ? 1.0 : 0.3;
                double sb_w  = sb_w_rest + (sb_w_full - sb_w_rest) * hover;
                double sb_pad = 3.0;
                double sb_x  = x + w - sb_w - sb_pad;
                double sb_y  = y + 6.0;
                double sb_h  = h - 12.0;
                double thumb_r = sb_w / 2.0;

                // Track background
                cairo_set_source_rgba(cr, tv.muted, tv.muted, tv.muted, 0.10 * hover);
                // rounded track
                shape::rrect(cr, sb_x, sb_y, sb_w, sb_h, thumb_r); cairo_fill(cr);

                // Thumb
                double ratio  = std::clamp((double)vis / total, 0.04, 1.0);
                double th     = std::max(20.0, sb_h * ratio);
                double sf     = scroll_offset / std::max(1.0, (double)scrollback.size());
                double ty     = sb_y + (1.0 - sf) * (sb_h - th);

                // Thumb fill - accent with opacity
                double alpha = 0.45 + 0.45 * hover;
                cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, alpha);
                shape::rrect(cr, sb_x, ty, sb_w, th, thumb_r); cairo_fill(cr);
            }
        }
        pango_font_description_free(mono_fnt);
    }

    void handle_scroll(int delta) {
        // Apply impulse to velocity - inertial scroll like editor
        scroll_vy -= delta * 2.5;
    }

    // Call each frame to apply friction and update scroll_offset
    void tick_scroll() {
        if (std::abs(scroll_vy) < 0.05) { scroll_vy = 0.0; return; }
        scroll_offset = std::clamp(scroll_offset + scroll_vy,
                                   0.0, (double)scrollback.size());
        scroll_vy *= 0.78;  // friction - ~120ms coast
    }
};

//  ConsolePanel: multi-tab terminal with optional split-pane
struct ConsolePanel {
    bool visible  = false;
    int  height   = 300;

    static constexpr int TABBAR_H  = 34;   // same height as the editor tab strip
    static constexpr int TAB_MIN_W = 80;
    static constexpr int TAB_MAX_W = 160;
    static constexpr int BTN_W     = 26;  // width of + button
    static constexpr int BTN_H     = 20;
    static constexpr int BTN_GAP   = 4;

    std::vector<TermInstance> terms;
    int  active_term      = 0;
    int  close_hovered_tab = -1;
    bool add_tab_hovered   = false;

    // Scrollbar drag state
    bool   sb_drag_active     = false;
    double sb_drag_start_y    = 0.0;
    double sb_drag_start_off  = 0.0;

    ConsolePanel() { terms.emplace_back(); terms[0].title = "Terminal 1"; }
    ~ConsolePanel() {}

    int term_count() const { return (int)terms.size(); }

    TermInstance &focused_term() {
        return terms[std::clamp(active_term, 0, term_count()-1)];
    }

    int add_tab() {
        terms.emplace_back();
        int idx = (int)terms.size() - 1;
        terms[idx].title = "Terminal " + std::to_string(idx + 1);
        return idx;
    }

    void close_tab(int idx) {
        if (term_count() <= 1) return;
        terms.erase(terms.begin() + idx);
        if (active_term >= term_count()) active_term = term_count() - 1;
    }



    // Tab geometry helpers
    static int btn_area() { return BTN_W + BTN_GAP; }

    int tab_width(int panel_w) const {
        int avail = panel_w - btn_area();
        int n     = std::max(1, term_count());
        return std::clamp(avail / n, TAB_MIN_W, TAB_MAX_W);
    }

    // x of "+" button (absolute panel coords)
    int plus_btn_x(int panel_x, int panel_w) const {
        return panel_x + panel_w - btn_area();
    }


    // Draw helper: rounded rect
    static void rr(cairo_t *cr, double x, double y, double w, double h, double r2) {
        shape::rrect(cr, x, y, w, h, r2);
    }

    // keyboard input
    // Platform-neutral core: `ks` is an X11/XKB keysym (the two keysym spaces
    // are numerically identical), `buf`/`len` is the resolved printable text
    // for this key (may be empty for control/navigation keys).
    void handle_key_core(unsigned long ks, bool ctrl, bool shift,
                          const char *buf, int len) {
        if (!visible) return;

        if (ctrl && shift && ks == XK_t) { int i = add_tab(); active_term = i; return; }
        if (ctrl && shift && ks == XK_w) { close_tab(active_term); return; }
        if (ctrl && ks == XK_Page_Up)   { active_term = std::max(0, active_term-1); return; }
        if (ctrl && ks == XK_Page_Down) { active_term = std::min(term_count()-1, active_term+1); return; }

        auto &ft = focused_term();
        if (ks == XK_Page_Up   && shift) { ft.scroll_offset = std::min(ft.scroll_offset + ft.screen.rows/2.0, (double)ft.scrollback.size()); return; }
        if (ks == XK_Page_Down && shift) { ft.scroll_offset = std::max(0.0, ft.scroll_offset - ft.screen.rows/2.0); return; }
        ft.scroll_offset = 0;

        const char *up_seq    = ctrl ? "\x1b[1;5A" : "\x1b[A";
        const char *down_seq  = ctrl ? "\x1b[1;5B" : "\x1b[B";
        const char *right_seq = ctrl ? "\x1b[1;5C" : "\x1b[C";
        const char *left_seq  = ctrl ? "\x1b[1;5D" : "\x1b[D";
        struct { KeySym k; const char *seq; } vtmap[] = {
            {XK_Up,      up_seq},    {XK_Down,     down_seq},
            {XK_Right,   right_seq}, {XK_Left,     left_seq},
            {XK_Home,    "\x1b[H"},  {XK_End,      "\x1b[F"},
            {XK_Page_Up, "\x1b[5~"}, {XK_Page_Down,"\x1b[6~"},
            {XK_Delete,  "\x1b[3~"}, {XK_Insert,   "\x1b[2~"},
            {XK_F1,"\x1bOP"},{XK_F2,"\x1bOQ"},{XK_F3,"\x1bOR"},{XK_F4,"\x1bOS"},
            {XK_F5,"\x1b[15~"},{XK_F6,"\x1b[17~"},{XK_F7,"\x1b[18~"},
            {XK_F8,"\x1b[19~"},{XK_F9,"\x1b[20~"},{XK_F10,"\x1b[21~"},
            {XK_F11,"\x1b[23~"},{XK_F12,"\x1b[24~"},{0,nullptr}
        };
        for (auto &m : vtmap) {
            if (!m.k) break;
            if (ks == m.k) { ft.pty_write(m.seq, (int)strlen(m.seq)); return; }
        }
        if (ctrl) {
            if (ks>=XK_a&&ks<=XK_z){char c=(char)(ks-XK_a+1);ft.pty_write(&c,1);return;}
            if (ks>=XK_A&&ks<=XK_Z){char c=(char)(ks-XK_A+1);ft.pty_write(&c,1);return;}
        }
        if (ks==XK_BackSpace){char c=0x7f;ft.pty_write(&c,1);return;}
        if (ks==XK_Return)   {char c='\r';ft.pty_write(&c,1);return;}
        if (ks==XK_Tab)      {char c='\t';ft.pty_write(&c,1);return;}
        if (ks==XK_Escape)   {char c=0x1b;ft.pty_write(&c,1);return;}
        if (len > 0) ft.pty_write(buf, len);
    }

    // X11 entry point (used by the Xlib backend).
    void handle_key(XKeyEvent *kev) {
        if (!visible || !kev) return;
        char buf[32] = {}; KeySym ks;
        int len = textor_lookup_key(kev, buf, sizeof(buf)-1, &ks);
        bool ctrl  = (kev->state & ControlMask) != 0;
        bool shift = (kev->state & ShiftMask)   != 0;
        handle_key_core(ks, ctrl, shift, buf, len);
    }

    void handle_scroll_event(int delta, int /*mx*/, int /*my*/,
                             int /*panel_x*/, int /*panel_y*/, int /*panel_w*/, int /*panel_h*/) {
        if (!visible) return;
        terms[std::clamp(active_term, 0, term_count()-1)].handle_scroll(delta);
    }

    // Call each frame to apply inertia to active terminal's scroll
    void tick(double /*dt*/) {
        if (!visible) return;
        for (auto &t : terms) t.tick_scroll();
    }

    // Draw
    void draw(cairo_t *cr, PangoLayout *layout, PangoFontDescription *fnt_arg,
              const ThemeVariant &tv, int x, int y, int w, int h)
    {
        if (!visible) return;

        // Panel body: slightly darker than the editor so the terminal reads as its own area
        cairo_set_source_rgb(cr, tv.bg * 0.78, tv.bg * 0.78, tv.bg * 0.82);
        cairo_rectangle(cr, x, y, w, h); cairo_fill(cr);

        // Tab bar: same colours as the editor tab strip (panel colour, thin bottom border)
        const int TB = TABBAR_H;
        cairo_set_source_rgb(cr, tv.panel, tv.panel, tv.panel);
        cairo_rectangle(cr, x, y, w, TB); cairo_fill(cr);
        cairo_set_source_rgba(cr, 1, 1, 1, 0.06);
        cairo_rectangle(cr, x, y + TB - 1, w, 1); cairo_fill(cr);
        // Thin line above the whole panel
        cairo_set_source_rgba(cr, 1, 1, 1, 0.10);
        cairo_rectangle(cr, x, y, w, 1); cairo_fill(cr);

        // Tabs
        int tw  = tab_width(w);
        int n   = term_count();

        PangoFontDescription *tf = pango_font_description_from_string(
            (tv.font_family + " " + std::to_string(std::max(8, (int)tv.font_size - 2))).c_str());

        for (int i = 0; i < n; i++) {
            int tx  = x + i * tw;
            bool act = (i == active_term);

            // Active tab: editor background and an accent line on top (like the editor tabs)
            if (act) {
                cairo_set_source_rgb(cr, tv.bg * 0.78, tv.bg * 0.78, tv.bg * 0.82);
                cairo_rectangle(cr, tx, y, tw, TB); cairo_fill(cr);
                cairo_set_source_rgb(cr, tv.accent_r, tv.accent_g, tv.accent_b);
                cairo_rectangle(cr, tx, y, tw, 2); cairo_fill(cr);
            }

            // Separator between tabs
            if (i > 0) {
                cairo_set_source_rgba(cr, 1, 1, 1, 0.06);
                cairo_set_line_width(cr, 1.0);
                cairo_move_to(cr, tx + 0.5, y + 6);
                cairo_line_to(cr, tx + 0.5, y + TB - 6);
                cairo_stroke(cr);
            }

            // PTY alive dot (3px rounded square)
            double dot_x = tx + 8, dot_y = y + TB/2.0 - 3;
            bool alive = terms[i].pty_alive;
            cairo_set_source_rgba(cr, tv.success_r, tv.success_g, tv.success_b, alive ? 0.85 : 0.30);
            rr(cr, dot_x, dot_y, 6, 6, 1.5);
            cairo_fill(cr);

            // Title
            pango_layout_set_font_description(layout, tf);
            std::string tname = terms[i].title;
            if ((int)tname.size() > 12) tname = tname.substr(0, 10) + "...";
            pango_layout_set_text(layout, tname.c_str(), -1);
            pango_layout_set_width(layout, (tw - 40) * PANGO_SCALE);
            pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
            int pw2, ph2; pango_layout_get_pixel_size(layout, &pw2, &ph2);
            cairo_set_source_rgba(cr, tv.text, tv.text, tv.text, act ? 0.92 : 0.48);
            cairo_move_to(cr, tx + 18, y + (TB - ph2) / 2.0);
            pango_cairo_show_layout(cr, layout);

            // Close button - small rounded square x
            int cx2 = tx + tw - 20, cy2 = y + TB/2 - 7;
            bool hov = (close_hovered_tab == i);
            if (hov) {
                cairo_set_source_rgba(cr, tv.error_r, tv.error_g, tv.error_b, 0.22);
                rr(cr, cx2, cy2, 14, 14, 3); cairo_fill(cr);
                svg_icons::draw_ui(cr, "cross", cx2 + 3, cy2 + 3, 8, tv.error_r, tv.error_g, tv.error_b, 0.90);
            } else {
                svg_icons::draw_ui(cr, "cross", cx2 + 3, cy2 + 3, 8, tv.muted, tv.muted, tv.muted, act ? 0.50 : 0.22);
            }
        }

        pango_font_description_free(tf);
        pango_layout_set_font_description(layout, fnt_arg);

        // "+" button (rounded square)
        {
            int bx = plus_btn_x(x, w);
            int by = y + (TB - BTN_H) / 2;
            bool hov = add_tab_hovered;
            cairo_set_source_rgba(cr, tv.accent_r, tv.accent_g, tv.accent_b, hov ? 0.22 : 0.10);
            rr(cr, bx, by, BTN_W, BTN_H, 4); cairo_fill(cr);
            double mid_x = bx + BTN_W / 2.0, mid_y = by + BTN_H / 2.0;
            svg_icons::draw_ui(cr, "plus", mid_x - 6, mid_y - 6, 12, tv.accent_r, tv.accent_g, tv.accent_b, hov ? 0.95 : 0.60);
        }

        // (console split button removed)

        // Single terminal pane
        int cy = y + TB;
        int ch = h - TB;
        if (ch <= 0) return;

        terms[std::clamp(active_term, 0, term_count()-1)]
            .draw(cr, layout, fnt_arg, tv, x, cy, w, ch, true);
    }

    // Mouse click
    bool handle_mouse_click(int mx, int my, int panel_x, int panel_y, int panel_w, int panel_h) {
        if (!visible) return false;
        if (mx < panel_x || mx > panel_x + panel_w ||
            my < panel_y || my > panel_y + panel_h) return false;

        const int TB = TABBAR_H;

        // Tab bar
        if (my >= panel_y && my <= panel_y + TB) {
            int tw2 = tab_width(panel_w);
            int n2  = term_count();

            for (int i = 0; i < n2; i++) {
                int tx = panel_x + i * tw2;
                if (mx < tx || mx >= tx + tw2) continue;
                // Close button?
                int cx2 = tx + tw2 - 20, cy2 = panel_y + TB/2 - 7;
                if (mx >= cx2 && mx <= cx2 + 14 && my >= cy2 && my <= cy2 + 14) {
                    close_tab(i); return true;
                }
                active_term = i;
                return true;
            }

            // "+" button
            int px  = plus_btn_x(panel_x, panel_w);
            int by2 = panel_y + (TB - BTN_H) / 2;
            if (mx >= px && mx <= px + BTN_W && my >= by2 && my <= by2 + BTN_H) {
                active_term = add_tab();
                return true;
            }
            return true;
        }
        // Scrollbar click in content area
        {
            TermInstance &ft = focused_term();
            int cy2   = panel_y + TABBAR_H;
            int ch2   = panel_h - TABBAR_H;
            int total = (int)ft.scrollback.size() + ft.screen.rows;
            int vis   = ft.screen.rows;
            if (ch2 > 0 && total > vis) {
                double sb_w   = 8.0;
                double sb_pad = 3.0;
                double sb_x2  = panel_x + panel_w - sb_w - sb_pad;
                double sb_y2  = cy2 + 6.0;
                double sb_h2  = ch2 - 12.0;
                double ratio  = std::clamp((double)vis / total, 0.04, 1.0);
                double th     = std::max(20.0, sb_h2 * ratio);
                double sf     = ft.scroll_offset / std::max(1.0, (double)ft.scrollback.size());
                double ty     = sb_y2 + (1.0 - sf) * (sb_h2 - th);
                if (mx >= (int)sb_x2 && mx <= (int)(sb_x2 + sb_w + sb_pad * 2)
                    && my >= (int)ty && my <= (int)(ty + th)) {
                    sb_drag_active    = true;
                    sb_drag_start_y   = (double)my;
                    sb_drag_start_off = ft.scroll_offset;
                    ft.scroll_vy      = 0.0;
                    return true;
                }
            }
        }
        return true;
    }

    // Mouse release
    bool handle_mouse_release(int /*mx*/, int /*my*/) {
        if (sb_drag_active) { sb_drag_active = false; return true; }
        return false;
    }

    // Mouse motion
    bool handle_mouse_motion(int mx, int my, int panel_x, int panel_y, int panel_w, int panel_h) {
        if (!visible) return false;
        const int TB = TABBAR_H;
        close_hovered_tab = -1;
        add_tab_hovered   = false;

        if (my >= panel_y && my <= panel_y + TB) {
            int tw2 = tab_width(panel_w);
            for (int i = 0; i < term_count(); i++) {
                int tx  = panel_x + i * tw2;
                if (mx < tx || mx >= tx + tw2) continue;
                int cx2 = tx + tw2 - 20, cy2 = panel_y + TB/2 - 7;
                if (mx >= cx2 && mx <= cx2 + 14 && my >= cy2 && my <= cy2 + 14)
                    close_hovered_tab = i;
                break;
            }
            int by2 = panel_y + (TB - BTN_H) / 2;
            int px  = plus_btn_x(panel_x, panel_w);
            if (mx >= px && mx <= px + BTN_W && my >= by2 && my <= by2 + BTN_H)
                add_tab_hovered = true;
        }

        // Scrollbar drag
        if (sb_drag_active && !terms.empty()) {
            TermInstance &ft = focused_term();
            int ch2   = panel_h - TABBAR_H;
            int total = (int)ft.scrollback.size() + ft.screen.rows;
            int vis   = ft.screen.rows;
            if (ch2 > 0 && total > vis) {
                double sb_h2   = ch2 - 12.0;
                double ratio   = std::clamp((double)vis / total, 0.04, 1.0);
                double thumb_h = std::max(20.0, sb_h2 * ratio);
                double movable = sb_h2 - thumb_h;
                double scrollable = (double)ft.scrollback.size();
                if (movable > 0.0) {
                    double dy     = my - sb_drag_start_y;
                    // thumb goes DOWN -> we go UP in history (offset decreases)
                    double new_off = sb_drag_start_off - dy * (scrollable / movable);
                    ft.scroll_offset = std::clamp(new_off, 0.0, scrollable);
                    ft.scroll_vy     = 0.0;
                }
            }
            return true;
        }

        return false;
    }
};

#endif
