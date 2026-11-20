#include "test.h"
#include "trig.h"

int main(void) {
  CHECK(tri_sin(0) == 0);
  CHECK(tri_sin(16384) == 65535);        /* 90 deg: TRIG_MAX_RATIO */
  CHECK_NEAR(tri_sin(32768), 0, 1);      /* 180 deg */
  CHECK(tri_sin(49152) == -65535);       /* 270 deg */
  CHECK(tri_cos(0) == 65535);
  CHECK_NEAR(tri_cos(8192), 46341, 2);   /* 45 deg */

  /* odd symmetry for sin */
  static const int32_t angles[] = {1, 4096, 8192, 16384, 20000, 30000, -16384, -49152};
  for (unsigned i = 0; i < sizeof(angles) / sizeof(angles[0]); i++) {
    int32_t a = angles[i];
    CHECK(tri_sin(-a) == -tri_sin(a));
  }

  return test_summary("test_trig");
}
