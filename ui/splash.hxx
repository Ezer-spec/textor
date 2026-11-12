#ifndef TEXTOR_SPLASH_HXX
#define TEXTOR_SPLASH_HXX

#include <chrono>
#include <cmath>
#include <cstring>
#include <algorithm>

// .................................................................
// Splash screen: the word TEXTOR
// .................................................................

static const char  *SPLASH_WORD  = "TEXTOR";
static const double LETTER_DELAY = 0.06;   // seconds between two letters
static const double LETTER_TIME  = 0.20;   // how long one letter takes to appear
static const double SPLASH_HOLD_END = 1.1; // start of the fade out
static const double SPLASH_TOTAL    = 1.4; // end of the splash

static inline double splash_clamp01(double v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

// Slow at the end: 0 -> 1
static inline double splash_ease_out(double t) { t = splash_clamp01(t); return 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t); }

static bool draw_splash(
    cairo_t *cr,
    int w,
    int h,
    const std::chrono::steady_clock::time_point &start,
    const std::chrono::steady_clock::time_point &now)
{
    double t = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() / 1000.0;
    if (t >= SPLASH_TOTAL) return true;

    // Background
    cairo_set_source_rgb(cr, 0.025, 0.035, 0.030);
    cairo_paint(cr);

    // Everything fades out at the end
    double fade_out = 1.0;
    if (t > SPLASH_HOLD_END) fade_out = 1.0 - splash_clamp01((t - SPLASH_HOLD_END) / (SPLASH_TOTAL - SPLASH_HOLD_END));

    // Font: big, bold, with a little space between the letters
    double size    = std::min(w, h) * 0.14;
    double spacing = size * 0.18;
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, size);

    // Width of every letter, so the whole word can be centered
    int n = (int)std::strlen(SPLASH_WORD);
    double advance[16];
    double total_w = 0;
    for (int i = 0; i < n; i++) {
        char one[2] = { SPLASH_WORD[i], 0 };
        cairo_text_extents_t ext;
        cairo_text_extents(cr, one, &ext);
        advance[i] = ext.x_advance;
        total_w += advance[i] + (i + 1 < n ? spacing : 0);
    }

    double x = (w - total_w) / 2.0;
    double baseline = h / 2.0 + size * 0.35;

    for (int i = 0; i < n; i++) {
        // Progress of this letter: 0 = not started, 1 = in place
        double p = splash_ease_out((t - i * LETTER_DELAY) / LETTER_TIME);

        double alpha = p * fade_out;
        double rise  = 0.0;                         // no movement, fade only

        if (alpha > 0.001) {
            char one[2] = { SPLASH_WORD[i], 0 };
            cairo_set_source_rgba(cr, 0.25, 0.90, 0.48, alpha);
            cairo_move_to(cr, x, baseline + rise);
            cairo_show_text(cr, one);
        }
        x += advance[i] + spacing;
    }

    return false;
}

#endif
