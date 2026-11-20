#include <pebble.h>
#include "trig.h"

int32_t tri_sin(int32_t trig_angle) {
  return sin_lookup(trig_angle);
}

int32_t tri_cos(int32_t trig_angle) {
  return cos_lookup(trig_angle);
}
