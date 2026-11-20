#ifndef SUNDIAL_RENDER_H
#define SUNDIAL_RENDER_H

#include <pebble.h>
#include "dial.h"
#include "solar.h"

/* Platform layout constants (SPEC 2). Q10: 1024 == DIAL_R. */
#define DIAL_CX PBL_PLATFORM_SWITCH(PBL_PLATFORM_TYPE_CURRENT, 0, 0, 0, 0, 100, 0, 130)
#define DIAL_CY PBL_PLATFORM_SWITCH(PBL_PLATFORM_TYPE_CURRENT, 0, 0, 0, 0, 114, 0, 130)
#define DIAL_R  PBL_PLATFORM_SWITCH(PBL_PLATFORM_TYPE_CURRENT, 0, 0, 0, 0, 94, 0, 122)
#define RIM_W   PBL_PLATFORM_SWITCH(PBL_PLATFORM_TYPE_CURRENT, 0, 0, 0, 0, 5, 0, 6)
#define NUM_R_Q10 880                        /* 0.86R, widened from 0.82R so XI/XII/I clear each other */
#define MOTTO_R_Q10 645                      /* 0.63R */

void render_draw(GContext *ctx, GRect bounds, const Dial *dial,
                 const SolarState *solar, Vec3q16 sun, bool calibrated);

#endif
