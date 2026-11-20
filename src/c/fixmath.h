#ifndef SUNDIAL_FIXMATH_H
#define SUNDIAL_FIXMATH_H

#include <stdint.h>

/* Q16.16 fixed point: one unit = 65536. Trig angle units: 360 deg = TRIG_MAX_ANGLE = 65536. */

int32_t q16_mul(int32_t a, int32_t b);       /* (int64)a*b / 65536, truncating */
int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi);
int32_t cdeg_to_trig(int32_t cdeg);          /* 0.01 deg -> TRIG angle units */
int32_t e4_to_trig(int32_t e4);              /* 1e-4 deg -> TRIG angle units */
uint32_t isqrt_u64(uint64_t v);              /* floor sqrt */

#endif
