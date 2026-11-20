/* Host-only implementation of trig.h for the test runner.
 * Deliberately shares no code with the watch implementation (src/c/trig.c). */
#include <math.h>
#include "trig.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define TRIG_MAX_ANGLE 65536
#define TRIG_MAX_RATIO 65535

static int32_t ratio(double rad) {
  return (int32_t)llround(sin(rad) * TRIG_MAX_RATIO);
}

int32_t tri_sin(int32_t trig_angle) {
  /* normalize into [0, TRIG_MAX_ANGLE) so llround(-1e-17) cannot give -0 */
  int32_t a = ((trig_angle % TRIG_MAX_ANGLE) + TRIG_MAX_ANGLE) % TRIG_MAX_ANGLE;
  return ratio(a * 2.0 * M_PI / TRIG_MAX_ANGLE);
}

int32_t tri_cos(int32_t trig_angle) {
  int32_t a = ((trig_angle % TRIG_MAX_ANGLE) + TRIG_MAX_ANGLE) % TRIG_MAX_ANGLE;
  return (int32_t)llround(cos(a * 2.0 * M_PI / TRIG_MAX_ANGLE) * TRIG_MAX_RATIO);
}
