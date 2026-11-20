#include "dial.h"
#include "fixmath.h"
#include "trig.h"

/* Q10 constants: 1024 == R */
#define H_MAX_Q10 532    /* 0.52R */
#define W_Q10    61     /* 0.06R */

void dial_init(Dial *d, int32_t lat_e4) {
  d->pole = lat_e4 >= 0 ? 1 : -1;
  d->lat_abs_e4 = clamp_i32(lat_e4 < 0 ? -lat_e4 : lat_e4, 150000, 750000);

  int32_t trig = e4_to_trig(d->lat_abs_e4);
  d->sin_lat = tri_sin(trig);
  d->cos_lat = tri_cos(trig);

  /* tan in Q16 (cos_lat > 0 throughout the clamped band) */
  int32_t tan_q16 = (int32_t)(((int64_t)d->sin_lat << 16) / d->cos_lat);

  /* h = min(0.52R, 0.72R * tan); 737 = 0.72 * 1024 */
  int32_t h_tan = (int32_t)(((int64_t)737 * tan_q16 + 32768) >> 16);
  d->h_q10 = h_tan < H_MAX_Q10 ? h_tan : H_MAX_Q10;

  /* L = h / tan */
  d->L_q10 = (int32_t)(((int64_t)d->h_q10 << 16) / tan_q16);
  d->w_q10 = W_Q10;

  for (int i = 0; i < 13; i++) {
    /* H = -90 + 15*i deg in TRIG units; tan(theta) = sin|phi| * tan H */
    int32_t H = (int32_t)(((int64_t)i * 65536) / 24) - 16384;
    int32_t x = q16_mul(d->sin_lat, tri_sin(H));
    int32_t y = d->pole * tri_cos(H);
    uint32_t len = isqrt_u64((uint64_t)((int64_t)x * x + (int64_t)y * y));
    if (len == 0) len = 1;
    d->hour_dir[i].x = (int32_t)(((int64_t)x * 1024) / len);
    d->hour_dir[i].y = (int32_t)(((int64_t)y * 1024) / len);
  }
}

PointQ10 dial_to_screen(const Dial *d, PointQ10 local) {
  PointQ10 p;
  if (d->pole > 0) {
    p.x = local.x;
    p.y = -local.y;
  } else {
    p.x = -local.x;
    p.y = local.y;
  }
  return p;
}

/* no cast shadow below sin 2 deg (matches theme's SIN2 threshold) */
#define DIAL_MIN_UZ 2287

bool dial_shadow_visible(Vec3q16 sun) {
  return sun.z >= DIAL_MIN_UZ;
}

/* project dial-plane point p (Q10, z up) along the sun vector onto z = 0 */
static PointQ10 project(Vec3q16 sun, int32_t x, int32_t y, int32_t z) {
  PointQ10 p;
  p.x = (int32_t)(x - (int64_t)z * sun.x / sun.z);
  p.y = (int32_t)(y - (int64_t)z * sun.y / sun.z);
  return p;
}

void dial_shadow_points(const Dial *d, Vec3q16 sun, PointQ10 out[6]) {
  if (!dial_shadow_visible(sun)) {
    for (int i = 0; i < 6; i++) {
      out[i].x = 0;
      out[i].y = 0;
    }
    return;
  }
  int32_t x2 = d->w_q10 / 2;
  int32_t yend = d->pole * d->L_q10;
  int i = 0;
  for (int e = 0; e < 2; e++) {
    int32_t x = e ? x2 : -x2;
    out[i++] = project(sun, x, 0, 0);
    out[i++] = project(sun, x, yend, 0);
    out[i++] = project(sun, x, yend, d->h_q10);
  }
}

PointQ10 dial_style_tip(const Dial *d, Vec3q16 sun) {
  PointQ10 p = {0, 0};
  if (!dial_shadow_visible(sun)) return p;
  /* centre of the style's top edge; its shadow lies on the hour line */
  return project(sun, 0, d->pole * d->L_q10, d->h_q10);
}

static int64_t cross2(PointQ10 o, PointQ10 a, PointQ10 b) {
  return (int64_t)(a.x - o.x) * (b.y - o.y) - (int64_t)(a.y - o.y) * (b.x - o.x);
}

/* monotone chain hull, CCW, collinear points dropped */
int convex_hull(PointQ10 in[6], PointQ10 out[6]) {
  PointQ10 p[6];
  int n = 6;
  for (int i = 0; i < n; i++) p[i] = in[i];
  for (int i = 1; i < n; i++) {          /* insertion sort by (x, y) */
    PointQ10 v = p[i];
    int j = i - 1;
    while (j >= 0 && (p[j].x > v.x || (p[j].x == v.x && p[j].y > v.y))) {
      p[j + 1] = p[j];
      j--;
    }
    p[j + 1] = v;
  }
  n = 0;                                  /* drop duplicates */
  for (int i = 0; i < 6; i++)
    if (n == 0 || p[i].x != p[n - 1].x || p[i].y != p[n - 1].y)
      p[n++] = p[i];
  if (n == 1) {
    out[0] = p[0];
    return 1;
  }
  if (n == 2) {
    out[0] = p[0];
    out[1] = p[1];
    return 2;
  }
  PointQ10 h[12];
  int k = 0;
  for (int i = 0; i < n; i++) {           /* lower hull */
    while (k >= 2 && cross2(h[k - 2], h[k - 1], p[i]) <= 0) k--;
    h[k++] = p[i];
  }
  for (int i = n - 2, t = k + 1; i >= 0; i--) {  /* upper hull */
    while (k >= t && cross2(h[k - 2], h[k - 1], p[i]) <= 0) k--;
    h[k++] = p[i];
  }
  k--;                                    /* last point == first */
  for (int i = 0; i < k; i++) out[i] = h[i];
  return k;
}

/* penumbra bands widen with shadow length: 65536 == no widening */
int32_t band_scale_q16(int32_t max_dist_q10, int32_t offset_px_q10) {
  if (offset_px_q10 <= 0 || max_dist_q10 <= 0) return 65536;
  int32_t s = 65536 + (int32_t)(((int64_t)offset_px_q10 * 65536) / max_dist_q10);
  if (s > 131072) s = 131072;
  return s;
}
