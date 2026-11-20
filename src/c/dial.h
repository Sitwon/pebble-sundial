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
PointQ10 dial_to_screen(const Dial *d, PointQ10 local);

#endif
