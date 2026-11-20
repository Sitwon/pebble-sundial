#ifndef SUNDIAL_THEME_H
#define SUNDIAL_THEME_H

#include <stdbool.h>
#include <stdint.h>

/* Scene selection thresholds in Q16 (SPEC 3.5): day at sin 6 deg,
 * shadow fade at sin 4 / sin 2, night below sin(-1 deg). */
#define SIN6_Q16   6850
#define SIN4_Q16   4572
#define SIN2_Q16   2287
#define SIN_M1_Q16 (-1144)

typedef enum { THEME_DAY, THEME_GOLDEN, THEME_NIGHT } Theme;
typedef struct { Theme theme; uint8_t shadow; } Scene;

Scene scene_for_sun(int32_t uz_q16);
uint8_t bayer4(int32_t x, int32_t y);
bool stipple_on(int32_t x, int32_t y, uint8_t density);

#endif
