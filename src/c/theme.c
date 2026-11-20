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
