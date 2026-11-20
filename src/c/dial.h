#ifndef SUNDIAL_DIAL_H
#define SUNDIAL_DIAL_H

#include <stdbool.h>
#include <stdint.h>
#include "solar.h"

/* Q10 fixed point: 1024 == dial radius R. */
typedef struct { int32_t x, y; } PointQ10;

typedef struct {
  int32_t pole;           /* +1 north, -1 south */
  int32_t lat_abs_e4;     /* clamped to 150000..750000 */
  int32_t h_q10, L_q10, w_q10;
  int32_t sin_lat, cos_lat;   /* Q16 of clamped |lat| */
  PointQ10 hour_dir[13];      /* local shadow direction, |v| ~= 1024 */
} Dial;

void dial_init(Dial *d, int32_t lat_e4);
/* true when |lat| is inside the accurate 15..75 deg band (before clamping) */
bool dial_in_band(int32_t lat_e4);
PointQ10 dial_to_screen(const Dial *d, PointQ10 local);
bool dial_shadow_visible(Vec3q16 sun);
void dial_shadow_points(const Dial *d, Vec3q16 sun, PointQ10 out[6]);
PointQ10 dial_style_tip(const Dial *d, Vec3q16 sun);
int convex_hull(PointQ10 in[6], PointQ10 out[6]);
int32_t band_scale_q16(int32_t max_dist_q10, int32_t offset_px_q10);

#endif
