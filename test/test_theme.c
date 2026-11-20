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
  /* --- step 4.2: Bayer stipple --- */
  uint8_t seen[16] = {0};
  for (int y = 0; y < 4; y++)
    for (int x = 0; x < 4; x++)
      seen[bayer4(x, y)]++;
  for (int v = 0; v < 16; v++) CHECK(seen[v] == 1);

  /* period 4 in both axes, deterministic */
  for (int y = -8; y < 8; y++)
    for (int x = -8; x < 8; x++)
      CHECK(bayer4(x, y) == bayer4(x + 4, y) && bayer4(x, y) == bayer4(x, y + 4));

  /* density d turns on exactly d of 16 cells per tile */
  for (uint8_t density = 0; density <= 16; density++) {
    int count = 0;
    for (int y = 100; y < 104; y++)
      for (int x = 200; x < 204; x++)
        if (stipple_on(x, y, density)) count++;
    CHECK(count == density);
    if (density == 0) {
      for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
          CHECK(!stipple_on(x, y, 0));
    }
  }
  /* monotone: a cell on at density d is on at d+1 */
  for (uint8_t d = 0; d < 16; d++)
    for (int y = 0; y < 16; y++)
      for (int x = 0; x < 16; x++)
        if (stipple_on(x, y, d)) CHECK(stipple_on(x, y, d + 1));

  return test_summary("test_theme");
}
