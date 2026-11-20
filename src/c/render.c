#include "render.h"
#include "fixmath.h"
#include "theme.h"
#include "trig.h"

/* ---- scanline helpers (all in screen px) ---- */

static void draw_stipple_run(GContext *ctx, int32_t y, int32_t x, int32_t n,
                             GColor color, uint8_t density) {
  graphics_context_set_fill_color(ctx, color);
  int32_t run_start = -1;
  for (int32_t i = 0; i <= n; i++) {
    bool on = i < n && stipple_on(x + i, y, density);
    if (on && run_start < 0) run_start = i;
    if (!on && run_start >= 0) {
      graphics_fill_rect(ctx, ((GRect){{x + run_start, y}, {i - run_start, 1}}), 0,
                         GCornerNone);
      run_start = -1;
    }
  }
}

/* stipple-filled circle, screen-anchored pattern */
static void circle_stipple(GContext *ctx, int32_t cx, int32_t cy, int32_t r,
                           GColor color, uint8_t density) {
  for (int32_t dy = -r; dy <= r; dy++) {
    uint32_t dx = isqrt_u64((uint64_t)r * r - (uint64_t)dy * dy);
    draw_stipple_run(ctx, cy + dy, cx - dx, 2 * dx + 1, color, density);
  }
}

/* annulus between inner and outer radius (outer >= inner >= 0) */
static void annulus_stipple(GContext *ctx, int32_t cx, int32_t cy, int32_t r_in,
                            int32_t r_out, GColor color, uint8_t density) {
  for (int32_t dy = -r_out; dy <= r_out; dy++) {
    uint32_t dx_out = isqrt_u64((uint64_t)r_out * r_out - (uint64_t)dy * dy);
    uint32_t dx_in = 0;
    if (r_in > 0 && dy <= r_in && dy >= -r_in) {
      int64_t v = (int64_t)r_in * r_in - (int64_t)dy * dy;
      dx_in = v > 0 ? isqrt_u64((uint64_t)v) + 1 : 0;  /* +1 keeps inside out */
    }
    if ((int32_t)dx_in > (int32_t)dx_out) continue;
    if (dx_in == 0) {
      draw_stipple_run(ctx, cy + dy, cx - dx_out, 2 * dx_out + 1, color,
                       density);
    } else {
      draw_stipple_run(ctx, cy + dy, cx - dx_out, dx_out - dx_in + 1, color,
                       density);
      draw_stipple_run(ctx, cy + dy, cx + dx_in, dx_out - dx_in + 1, color,
                       density);
    }
  }
}

/* ---- engraving (step 5.2) ---- */

#define NUM_H   PBL_PLATFORM_SWITCH(PBL_PLATFORM_TYPE_CURRENT, 0, 0, 0, 0, 12, 0, 16)
#define STROKE  PBL_PLATFORM_SWITCH(PBL_PLATFORM_TYPE_CURRENT, 0, 0, 0, 0, 1, 0, 2)

static void stroke_line(GContext *ctx, GPoint a, GPoint b, GColor color,
                        int width) {
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, width);
  graphics_draw_line(ctx, a, b);
}

/* two-tone engraved stroke: dark core, 1 px light highlight below-right */
static void engraved_line(GContext *ctx, GPoint a, GPoint b, GColor core,
                          GColor light, int width) {
  stroke_line(ctx, (GPoint){a.x + 1, a.y + 1}, (GPoint){b.x + 1, b.y + 1},
              light, width);
  stroke_line(ctx, a, b, core, width);
}

/* vector Roman glyphs: I, V, X strokes, upright, centred in a char cell.
 * Narrow advances so XI/XII/I fit the compressed noon arc at 51 deg. */
static void glyph_strokes(char c, int h, GPoint a[2], GPoint b[2]) {
  int h2 = h / 2;
  int w2 = (h - 3) / 2;
  if (c == 'I') {
    a[0] = (GPoint){0, -h2}; b[0] = (GPoint){0, h2};
  } else if (c == 'V') {
    a[0] = (GPoint){-w2, -h2}; b[0] = (GPoint){0, h2 - 1};
    a[1] = (GPoint){w2, -h2};  b[1] = (GPoint){0, h2 - 1};
  } else {  /* X */
    a[0] = (GPoint){-w2, -h2}; b[0] = (GPoint){w2, h2};
    a[1] = (GPoint){-w2, h2};  b[1] = (GPoint){w2, -h2};
  }
}

static int glyph_width(char c, int h) {
  return c == 'I' ? 2 : (h - 3) / 2 * 2 + 1;
}

static void draw_glyph(GContext *ctx, char c, int x, int y, int h, int stroke,
                       GColor core, GColor light) {
  GPoint a[2], b[2] = {{0, 0}, {0, 0}};
  glyph_strokes(c, h, a, b);
  int n = c == 'I' ? 1 : 2;
  for (int s = 0; s < 2; s++) {   /* light pass first, core over it */
    GColor color = s ? core : light;
    int off = s ? 0 : 1;
    for (int i = 0; i < n; i++)
      stroke_line(ctx, (GPoint){x + a[i].x + off, y + a[i].y + off},
                      (GPoint){x + b[i].x + off, y + b[i].y + off}, color,
                  stroke);
  }
}

static void draw_label(GContext *ctx, const char *label, int cx, int cy,
                       GColor core, GColor light) {
  int h = NUM_H, w = 0;
  for (const char *p = label; *p; p++) w += glyph_width(*p, h) + 1;
  w -= 1;
  int x = cx - w / 2;
  for (const char *p = label; *p; p++) {
    draw_glyph(ctx, *p, x + glyph_width(*p, h) / 2, cy, h, STROKE, core, light);
    x += glyph_width(*p, h);
  }
}

static void draw_engraving(GContext *ctx, const Dial *dial, int32_t cx,
                           int32_t cy, int32_t R, bool calibrated) {
  static const char *k_labels[13] = {
    "VI", "VII", "VIII", "IX", "X", "XI", "XII", "I", "II", "III", "IV", "V",
    "VI"
  };

  GColor core = calibrated ? GColorArmyGreen : GColorDarkGray;
  GColor light = calibrated ? GColorChromeYellow : GColorLightGray;

  /* hour lines 0.14R..0.70R; XII is the 2 px substyle */
  for (int i = 0; i < 13; i++) {
    PointQ10 s = dial_to_screen(dial, dial->hour_dir[i]);
    GPoint a = {cx + (int16_t)((int64_t)s.x * 143 * R / (1024 * 1024)),
                cy + (int16_t)((int64_t)s.y * 143 * R / (1024 * 1024))};
    GPoint b = {cx + (int16_t)((int64_t)s.x * 717 * R / (1024 * 1024)),
                cy + (int16_t)((int64_t)s.y * 717 * R / (1024 * 1024))};
    engraved_line(ctx, a, b, core, light, i == 6 ? STROKE + 1 : STROKE);
  }

  /* numerals at 0.82R on each hour line */
  for (int i = 0; i < 13; i++) {
    PointQ10 s = dial_to_screen(dial, dial->hour_dir[i]);
    int nx = cx + (int)((int64_t)s.x * NUM_R_Q10 * R / (1024 * 1024));
    int ny = cy + (int)((int64_t)s.y * NUM_R_Q10 * R / (1024 * 1024));
    draw_label(ctx, k_labels[i], nx, ny, core, light);
  }

  /* hub: dark circle with a light edge offset below-right */
  int hr = R * 123 / 1024;   /* 0.12R */
  graphics_context_set_fill_color(ctx, light);
  graphics_fill_circle(ctx, GPoint(cx + 1, cy + 1), hr);
  graphics_context_set_fill_color(ctx, core);
  graphics_fill_circle(ctx, GPoint(cx, cy), hr);

  /* pole index: 3/4 px dot at 0.97R above centre */
  int r_dot = STROKE + 2;
  if (calibrated) {
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_circle(ctx, GPoint(cx, cy - (int32_t)R * 993 / 1024), r_dot);
  } else {
    graphics_context_set_stroke_color(ctx, GColorDarkGray);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_circle(ctx, GPoint(cx, cy - (int32_t)R * 993 / 1024), r_dot);
  }
}

static void draw_motto(GContext *ctx, int32_t cx, int32_t cy, int32_t R,
                       bool calibrated) {
  const char *motto = "HORAS NON NUMERO NISI SERENAS";
  GFont font = PBL_PLATFORM_SWITCH(PBL_PLATFORM_TYPE_CURRENT, 0, 0, 0, 0,
      fonts_get_system_font(FONT_KEY_GOTHIC_09),
      0, fonts_get_system_font(FONT_KEY_GOTHIC_14));

  GSize size = graphics_text_layout_get_content_size(
      motto, font, ((GRect){{0, 0}, {2 * R, 2 * R}}), GTextOverflowModeWordWrap,
      GTextAlignmentCenter);
  /* chord width at the motto's y (0.63R below centre) */
  int32_t my = cy + (int32_t)R * MOTTO_R_Q10 / 1024;
  int64_t half_chord = (int64_t)R * 777 / 1024;
  if ((int32_t)size.w > 2 * half_chord) motto = "TEMPUS FUGIT";

  graphics_context_set_text_color(
      ctx, calibrated ? GColorArmyGreen : GColorDarkGray);
  graphics_draw_text(ctx, motto, font,
                     ((GRect){{cx - R, my - size.h / 2}, {2 * R, size.h + 2}}),
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);}

static void draw_case(GContext *ctx, GRect bounds) {
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
}

static void draw_dial(GContext *ctx, int32_t cx, int32_t cy, int32_t R,
                      bool calibrated) {
  (void)calibrated;

  /* rim band: ArmyGreen / DarkGray dither */
  annulus_stipple(ctx, cx, cy, R - RIM_W, R, GColorArmyGreen, 8);
  circle_stipple(ctx, cx, cy, R - RIM_W, GColorDarkGray, 16);

  /* brass base */
  graphics_context_set_fill_color(ctx, GColorBrass);
  graphics_fill_circle(ctx, GPoint(cx, cy), R - RIM_W);

  /* 4 concentric bands, slightly darker toward the rim:
     ArmyGreen stipple density rises outward across dithered boundaries */
  static const uint8_t k_band_dens[4] = {1, 3, 6, 9};
  static const uint16_t k_bounds_q10[5] = {0, 449, 619, 800, 940};
  for (int b = 0; b < 4; b++) {
    annulus_stipple(ctx, cx, cy,
                    R * k_bounds_q10[b] / 1024, R * k_bounds_q10[b + 1] / 1024,
                    GColorArmyGreen, k_band_dens[b]);
  }
  /* dithered transitions at each boundary */
  for (int b = 1; b < 4; b++) {
    int32_t rb = R * k_bounds_q10[b] / 1024;
    annulus_stipple(ctx, cx, cy, rb - 1, rb + 1, GColorArmyGreen, 8);
  }

  /* 3 fixed patina stipples near the rim */
  static const int k_patina_deg[3] = {30, 140, 250};
  for (int i = 0; i < 3; i++) {
    int32_t a = k_patina_deg[i] * 65536 / 360;
    int32_t px = cx + (int32_t)((int64_t)tri_sin(a) * R / 65536 * 93 / 100);
    int32_t py = cy - (int32_t)((int64_t)tri_cos(a) * R / 65536 * 93 / 100);
    circle_stipple(ctx, px, py, 8, GColorArmyGreen, 5);
  }
}

/* ---- cast shadow (step 5.4) ---- */

/* stipple-filled scanline polygon, rows clipped to the dial circle */
static void poly_fill(GContext *ctx, GPoint *pts, int n, int32_t cx,
                      int32_t cy, int32_t R, uint8_t density, bool core) {
  int32_t ymin = pts[0].y, ymax = pts[0].y;
  for (int i = 1; i < n; i++) {
    if (pts[i].y < ymin) ymin = pts[i].y;
    if (pts[i].y > ymax) ymax = pts[i].y;
  }
  if (ymin < cy - R + 1) ymin = cy - R + 1;
  if (ymax > cy + R - 1) ymax = cy + R - 1;

  for (int32_t y = ymin; y <= ymax; y++) {
    int32_t xs[8];
    int nx = 0;
    for (int i = 0; i < n; i++) {
      GPoint a = pts[i], b = pts[(i + 1) % n];
      if (a.y == b.y) continue;
      if ((y >= a.y && y < b.y) || (y >= b.y && y < a.y))
        xs[nx++] = a.x + (int32_t)(((int64_t)(y - a.y) * (b.x - a.x)) / (b.y - a.y));
    }
    for (int i = 1; i < nx; i++) {         /* sort crossings */
      int32_t v = xs[i];
      int j = i - 1;
      while (j >= 0 && xs[j] > v) { xs[j + 1] = xs[j]; j--; }
      xs[j + 1] = v;
    }
    /* clip the row to the dial circle */
    int64_t v = (int64_t)R * R - (int64_t)(y - cy) * (y - cy);
    if (v <= 0) continue;
    int32_t dx = isqrt_u64((uint64_t)v);
    for (int e = 0; e + 1 < nx; e += 2) {
      int32_t lo = xs[e] < cx - dx ? cx - dx : xs[e];
      int32_t hi = xs[e + 1] > cx + dx ? cx + dx : xs[e + 1];
      int32_t run_start = -1;
      GColor run_col = GColorBlack;
      for (int32_t x = lo; x <= hi + 1; x++) {
        bool on = x <= hi && stipple_on(x, y, density);
        GColor col = GColorArmyGreen;
        if (core && on && !((x & 1) || (y & 1))) col = GColorBlack;
        if (on && run_start < 0) {
          run_start = x;
          run_col = col;
        } else if (run_start >= 0 && (!on || col != run_col)) {
          graphics_context_set_fill_color(ctx, run_col);
          graphics_fill_rect(ctx, GRect(run_start, y, x - run_start, 1), 0,
                             GCornerNone);
          run_start = on ? x : -1;
          run_col = col;
        }
      }
    }
  }
}

static void draw_shadow(GContext *ctx, const Dial *d, int32_t cx, int32_t cy,
                        int32_t R, Vec3q16 sun) {
  Scene sc = scene_for_sun(sun.z);
  if (sc.shadow == 0) return;

  PointQ10 local[6], hull[6];
  dial_shadow_points(d, sun, local);
  int n = convex_hull(local, hull);
  if (n < 3) return;

  /* hull in screen px, plus its reach for the band width */
  GPoint base[6];
  int32_t max_q10 = 0;
  for (int i = 0; i < n; i++) {
    PointQ10 s = dial_to_screen(d, hull[i]);
    base[i].x = (int16_t)(cx + (int32_t)s.x * R / 1024);
    base[i].y = (int16_t)(cy + (int32_t)s.y * R / 1024);
    int32_t len2 = s.x * s.x + s.y * s.y;
    if (len2 > max_q10) max_q10 = len2;
  }
  max_q10 = isqrt_u64((uint32_t)max_q10);
  int32_t max_px = (int32_t)((int64_t)max_q10 * R / 1024);

  int32_t band_px = 2 + max_px / 50;
  int32_t cap = PBL_PLATFORM_SWITCH(PBL_PLATFORM_TYPE_CURRENT, 0, 0, 0, 0, 6, 0, 8);
  if (band_px > cap) band_px = cap;

  /* three nested fills scaled about the style foot, outside in */
  int32_t scales[3] = {65536,
                       band_scale_q16(max_q10, band_px / 2),
                       band_scale_q16(max_q10, band_px)};
  uint8_t dens[3] = {12, 8, 4};
  for (int b = 2; b >= 0; b--) {
    GPoint poly[6];
    for (int i = 0; i < n; i++) {
      poly[i].x = (int16_t)(cx + ((int32_t)(base[i].x - cx) * scales[b]) / 65536);
      poly[i].y = (int16_t)(cy + ((int32_t)(base[i].y - cy) * scales[b]) / 65536);
    }
    poly_fill(ctx, poly, n, cx, cy, R, dens[b], b == 0);
  }

  /* crisp reading edge from centre to the style tip, clamped inside 0.97R */
  PointQ10 tip = dial_style_tip(d, sun);
  int64_t tpx = (int64_t)tip.x * R / 1024;
  int64_t tpy = (int64_t)tip.y * R / 1024;
  int64_t tlen2 = tpx * tpx + tpy * tpy;
  int64_t lim = (int64_t)R * 997 / 1024;
  if (tlen2 > lim * lim) {
    int64_t f = (int64_t)lim * 65536 / isqrt_u64((uint64_t)tlen2);
    tpx = tpx * f / 65536;
    tpy = tpy * f / 65536;
  }
  graphics_context_set_antialiased(ctx, false);
  stroke_line(ctx, GPoint(cx, cy), GPoint(cx + tpx, cy + tpy), GColorBlack, 1);
  graphics_context_set_antialiased(ctx, true);
}



static void draw_blade(GContext *ctx, const Dial *d, int32_t cx, int32_t cy,
                       int32_t R, Vec3q16 sun) {
  int32_t Lpx = (int32_t)((int64_t)d->L_q10 * R / 1024);
  int32_t wpx = (int32_t)((int64_t)d->w_q10 * R / 1024);
  if (wpx < 2) wpx = 2;
  int32_t half = wpx / 2;

  /* blade always points up toward the visible pole */
  graphics_context_set_fill_color(ctx, GColorWindsorTan);
  graphics_fill_rect(ctx, ((GRect){{cx - half, cy - Lpx}, {wpx, Lpx}}), 0,
                     GCornerNone);
  int32_t sunx = sun.x * d->pole;   /* sun's screen-x sign */
  graphics_context_set_fill_color(ctx, GColorRajah);   /* sunward half lit */
  graphics_fill_rect(ctx,
      ((GRect){{sunx > 0 ? cx : cx - half, cy - Lpx}, {half, Lpx}}), 0, GCornerNone);

  /* ridge line, sunward bevel, far-edge rim light */
  graphics_context_set_stroke_color(ctx, GColorChromeYellow);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(cx, cy - 1), GPoint(cx, cy - Lpx + 1));
  stroke_line(ctx, GPoint(cx + (sunx > 0 ? half : -half), cy - Lpx),
                   GPoint(cx + (sunx > 0 ? half : -half), cy), GColorChromeYellow, 1);
  graphics_draw_line(ctx, GPoint(cx - half, cy - Lpx),
                          GPoint(cx + half, cy - Lpx));

  /* 2 px contact shadow across the base */
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, ((GRect){{cx - half - 1, cy}, {wpx + 2, 2}}), 0, GCornerNone);
}

void render_draw(GContext *ctx, GRect bounds, const Dial *dial,
                 const SolarState *solar, Vec3q16 sun, bool calibrated) {
  (void)solar;
  int32_t cx = DIAL_CX, cy = DIAL_CY, R = DIAL_R;

  draw_case(ctx, bounds);
  draw_dial(ctx, cx, cy, R, calibrated);
  draw_engraving(ctx, dial, cx, cy, R, calibrated);
  draw_motto(ctx, cx, cy, R, calibrated);
  draw_shadow(ctx, dial, cx, cy, R, sun);
  draw_blade(ctx, dial, cx, cy, R, sun);
}
