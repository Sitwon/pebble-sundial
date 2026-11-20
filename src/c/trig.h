#ifndef SUNDIAL_TRIG_H
#define SUNDIAL_TRIG_H

#include <stdint.h>

/* Trig abstraction. Angles in TRIG units (0..65535 = 0..360 deg);
 * results in Q16.0 ratio (-65535..65535). */

int32_t tri_sin(int32_t trig_angle);
int32_t tri_cos(int32_t trig_angle);

#endif
