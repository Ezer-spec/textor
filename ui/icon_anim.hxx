#ifndef ICON_ANIM_HXX
#define ICON_ANIM_HXX
#pragma once

/*
    icon_anim.hxx / icon_anim.cxx - small, centralized "click pop" animation
    for every clickable icon in the app (activity-bar icons, the Explorer/
    Split/Addons/Console pills, the Settings/Exit icons, tab-close buttons,
    console-panel buttons, sidebar/file-picker icons, etc).

    Rather than adding a ScalarAnim/AnimController member to every single
    icon-holding struct across the codebase (BarIcon, sidebar rows, tab
    rows, console buttons, ...), this keeps one small out-of-line registry
    keyed by an arbitrary stable pointer - usually just `&some_button`, the
    same "one animation per widget instance" convention already used for
    ScalarAnim/AnimController elsewhere (see morphing.hxx), but without
    needing to touch the widget's own struct definition.

    HOW TO WIRE UP A NEW ICON:
    1) Where the click is handled, once the icon's action actually runs:
        icon_anim::trigger(&btn_settings);

    2) Where the icon glyph is drawn, wrap just the drawing calls for
      that one icon (nothing else - background pill, label text, etc.
      should stay put) in a Scope, centered on the icon's own midpoint:
        {
            icon_anim::Scope pop(cr, &btn_settings, icx, icy);
            // ... existing cairo calls that paint the icon glyph ...
        }

    The `key` just needs to be a stable, per-icon address - the button's
    own address is perfect since it lives as long as the icon does and is
    already unique per icon instance.

    Automatically respects Settings -> Performance -> "Animations enabled"
    (g_animations_enabled, the same global every other animation in this
    app already checks): trigger() becomes a no-op and Scope applies no
    transform at all while animations are turned off, so a disabled icon
    just changes state instantly with no pop, same as everything else.
 */

#include <cairo/cairo.h>

namespace icon_anim {

/* Call once when an icon is clicked/activated, to start its "pop".
   `key` must be the same stable address passed to Scope for that icon.
   No-op if animations are globally disabled or `key` is null. */
void trigger(const void *key) noexcept;

/* Current pop scale for `key`: 1.0 when idle, finished, or when
   animations are globally disabled, briefly dips below 1.0 and springs
   back past it right after trigger(). Exposed for callers that need the
   raw number, most call sites should just use Scope instead. */
double scale(const void *key) noexcept;

/* RAII helper: saves the cairo state, applies the current pop scale for
   `key` centered on (cx, cy) - the icon's own midpoint - and restores on
   scope exit. Wrap this around only the draw calls for one icon's glyph. */
struct Scope {
    cairo_t *cr;
    bool active;
    Scope(cairo_t *cr_, const void *key, double cx, double cy) noexcept;
    ~Scope() noexcept;
    Scope(const Scope &) = delete;
    Scope &operator=(const Scope &) = delete;
};

/* Drops finished/expired entries from the registry. Not required for
   correctness (scale()/Scope already treat an expired entry as idle) -
   just keeps long-running sessions that click many different icons from
   slowly accumulating dead map entries. Cheap; fine to call once a frame.
*/
void prune() noexcept;

}

#endif
