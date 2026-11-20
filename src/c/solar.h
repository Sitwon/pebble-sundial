#ifndef SUNDIAL_SOLAR_H
#define SUNDIAL_SOLAR_H

#include <stdint.h>
#include <time.h>

/* Apparent solar state at a UTC instant. */
typedef struct {
  int16_t decl_cdeg;
  int16_t eot_cmin;
  int32_t ast_cmin;     /* apparent solar time, 0..143999 centiminutes */
  int32_t hour_angle;   /* TRIG angle units, -32768..32767 */
} SolarState;

/* Local sun vector: Q16.0 components, x east, y north, z up. */
typedef struct { int32_t x, y, z; } Vec3q16;

void solar_state(time_t utc, int32_t lon_e4, SolarState *out);
Vec3q16 sun_vector(const SolarState *s, int32_t lat_e4);

#endif
