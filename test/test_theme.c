#include "test.h"
#include "theme.h"

static const struct {
  int32_t uz; Theme theme; uint8_t shadow;
} cases[] = {
  {65535, THEME_DAY, 3},
  {6850, THEME_DAY, 3},
  {6849, THEME_GOLDEN, 2},
  {4572, THEME_GOLDEN, 2},
  {4571, THEME_GOLDEN, 1},
  {2287, THEME_GOLDEN, 1},
  {2286, THEME_GOLDEN, 0},
  {0, THEME_GOLDEN, 0},
  {-1144, THEME_GOLDEN, 0},
  {-1145, THEME_NIGHT, 0},
  {-65535, THEME_NIGHT, 0},
};

int main(void) {
  for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    Scene s = scene_for_sun(cases[i].uz);
    CHECK(s.theme == cases[i].theme);
    CHECK(s.shadow == cases[i].shadow);
  }
  return test_summary("test_theme");
}
