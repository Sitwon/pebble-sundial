#include "test.h"
#include "fixmath.h"

int main(void) {
  /* q16_mul: positive, negative, zero, and no-overflow checks */
  CHECK_NEAR(q16_mul(65535, 65535), 65534, 1);
  CHECK_NEAR(q16_mul(-65535, 65535), -65534, 1);
  CHECK(q16_mul(0, 12345) == 0);
  CHECK(q16_mul(12345, 0) == 0);
  /* near full-scale product must not overflow into a negative */
  CHECK(q16_mul(65535, 65535) >= 0);
  CHECK(q16_mul(32768, 65536) == 32768);   /* 0.5 * 1.0 */

  /* clamp_i32 */
  CHECK(clamp_i32(5, 0, 10) == 5);
  CHECK(clamp_i32(-1, 0, 10) == 0);
  CHECK(clamp_i32(11, 0, 10) == 10);
  CHECK(clamp_i32(-2147483647 - 1, -100, 100) == -100);
  CHECK(clamp_i32(2147483647, -100, 100) == 100);
  CHECK(clamp_i32(0, 0, 0) == 0);

  /* cdeg_to_trig: 0.01 deg -> TRIG angle units (360 deg = 65536) */
  CHECK(cdeg_to_trig(9000) == 16384);   /* 90 deg */
  CHECK(cdeg_to_trig(-9000) == -16384);
  CHECK(cdeg_to_trig(0) == 0);
  CHECK_NEAR(cdeg_to_trig(5148), 9372, 1);    /* 51.48 deg */

  /* e4_to_trig: 1e-4 deg -> TRIG angle units */
  CHECK(e4_to_trig(900000) == 16384);
  CHECK(e4_to_trig(-900000) == -16384);
  CHECK(e4_to_trig(0) == 0);
  CHECK_NEAR(e4_to_trig(514779), 9372, 1);    /* 51.4779 deg */

  /* isqrt_u64: exact squares, floor behaviour, full scale */
  CHECK(isqrt_u64(0) == 0);
  CHECK(isqrt_u64(1) == 1);
  CHECK(isqrt_u64(2) == 1);
  CHECK(isqrt_u64(3) == 1);
  CHECK(isqrt_u64(4) == 2);
  CHECK(isqrt_u64(1024ull * 1024) == 1024);
  CHECK(isqrt_u64(1024ull * 1024 + 1) == 1024);
  CHECK(isqrt_u64(65535ull * 65535) == 65535);
  CHECK(isqrt_u64(65535ull * 65535 + 1) == 65535);
  CHECK(isqrt_u64(1ull << 62) == (1u << 31));

  return test_summary("test_fixmath");
}
