#include "theme.h"

Scene scene_for_sun(int32_t uz_q16) {
  Scene s;
  if (uz_q16 < SIN_M1_Q16) {
    s.theme = THEME_NIGHT;
    s.shadow = 0;
    return s;
  }
  if (uz_q16 >= SIN6_Q16) {
    s.theme = THEME_DAY;
    s.shadow = 3;
    return s;
  }
  s.theme = THEME_GOLDEN;
  if (uz_q16 >= SIN4_Q16) s.shadow = 2;
  else if (uz_q16 >= SIN2_Q16) s.shadow = 1;
  else s.shadow = 0;
  return s;
}

static const uint8_t k_bayer[16] = {
   0,  8,  2, 10,
  12,  4, 14,  6,
   3, 11,  1,  9,
  15,  7, 13,  5
};

uint8_t bayer4(int32_t x, int32_t y) {
  return k_bayer[(y & 3) * 4 + (x & 3)];
}

bool stipple_on(int32_t x, int32_t y, uint8_t density) {
  if (density == 0) return false;
  if (density >= 16) return true;
  return bayer4(x, y) < density;
}
