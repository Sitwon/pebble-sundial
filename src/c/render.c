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

/* ---- static dial (step 5.1) ---- */

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
  (void)dial;
  (void)solar;
  (void)sun;
  int32_t cx = DIAL_CX, cy = DIAL_CY, R = DIAL_R;

  draw_case(ctx, bounds);
  draw_dial(ctx, cx, cy, R, calibrated);
}
