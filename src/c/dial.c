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
