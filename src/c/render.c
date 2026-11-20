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
      graphics_fill_rect(ctx, GRect(x + run_start, y, i - run_start, 1), 0,
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
      motto, font, GRect(0, 0, 2 * R, 2 * R), GTextOverflowModeWordWrap,
      GTextAlignmentCenter);
  /* chord width at the motto's y (0.63R below centre) */
  int32_t my = cy + (int32_t)R * MOTTO_R_Q10 / 1024;
  int64_t half_chord = (int64_t)R * 777 / 1024;
  if ((int32_t)size.w > 2 * half_chord) motto = "TEMPUS FUGIT";

  graphics_context_set_text_color(
      ctx, calibrated ? GColorArmyGreen : GColorDarkGray);
  graphics_draw_text(ctx, motto, font,
                     GRect(cx - R, my - size.h / 2, 2 * R, size.h + 2),
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

void render_draw(GContext *ctx, GRect bounds, const Dial *dial,
                 const SolarState *solar, Vec3q16 sun, bool calibrated) {
  (void)solar;
  (void)sun;
  int32_t cx = DIAL_CX, cy = DIAL_CY, R = DIAL_R;

  draw_case(ctx, bounds);
  draw_dial(ctx, cx, cy, R, calibrated);
  draw_engraving(ctx, dial, cx, cy, R, calibrated);
  draw_motto(ctx, cx, cy, R, calibrated);
}
