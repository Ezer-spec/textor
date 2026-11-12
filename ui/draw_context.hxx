#ifndef DRAW_CONTEXT_HXX
#define DRAW_CONTEXT_HXX

/*
    draw_context.hxx - the shared per-frame state every piece of draw_ui
    needs. Introduced while splitting draw_ui (previously one ~1600-line
    free function in editor.hxx) into separate files under ui/render/, one
    per visual section (top bar, sidebar overlay, addons overlay, floating
    tab panel, ...).
    Rather than each extracted function taking cr/tv/layout/fnt/ed as five
    separate parameters (and every future one needing the same five), they
    all take a single `DrawContext &dc`. New sections that need more
    shared state later just add a field here instead of changing every
    function's signature - that's the "modularity for later edits" this
    was asked for.
    Each render/*.hxx function starts with a small alias preamble:
    cairo_t *cr = dc.cr; Editor &ed = dc.ed; const ThemeVariant &tv = dc.tv;
    PangoLayout *layout = dc.layout; PangoFontDescription *fnt = dc.fnt;
    so the body reads exactly like it did inline in draw_ui - this was a
    deliberate choice to make the split itself as low-risk as possible
    (pure move, no rewriting of every `cr`/`tv`/... reference). -_-
*/
#pragma once

#include <pango/pangocairo.h>

struct Editor;       // ui/editor.hxx
struct ThemeVariant;  // color/themes.hxx

struct DrawContext {
    cairo_t               *cr;
    Editor                 &ed;
    const ThemeVariant     &tv;
    PangoLayout            *layout;
    PangoFontDescription   *fnt;

    // Per-frame values several sections need, computed once at the top of
    // draw_ui so nobody recomputes them (that's also the "speed" half of
    // this pass - con_h/eff_height/topbar_reveal were each already single
    // per-frame values before the split; keeping them in one place here
    // instead of letting every new section reinvent its own copy is what
    // keeps it that way as draw_ui keeps growing).
    double con_h        = 0.0;   // current console height
    double eff_height   = 0.0;   // vertical space available to the code viewport
    double topbar_reveal = 1.0;  // Focus Mode: 1 = top bar fully shown, 0 = hidden
    int    line_height  = 0;     // current line height in px
    int    ed_x         = 0;     // left edge of the code viewport (past sidebar/activity bar);
                                  // set a little later than the rest, once the sidebar's
                                  // current slide-open animation width is known
    cairo_t *xlib_cr     = nullptr; // draw_ui's own xlib_cr param - null means GPU mode
                                     // (also set after construction, same as ed_x)
};

#endif
