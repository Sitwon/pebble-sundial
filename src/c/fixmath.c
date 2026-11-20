#include "fixmath.h"

int32_t q16_mul(int32_t a, int32_t b) {
  /* division (not >>) so negative products truncate symmetrically */
  return (int32_t)(((int64_t)a * b) / 65536);
}

int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static int32_t deg_to_trig(int64_t numer, int64_t denom) {
  /* round-half-away-from-zero to keep signs symmetric */
  int64_t half = denom / 2;
  if (numer >= 0) return (int32_t)((numer + half) / denom);
  return (int32_t)((numer - half) / denom);
}

int32_t cdeg_to_trig(int32_t cdeg) {
  return deg_to_trig((int64_t)cdeg * 65536, 36000);
}

int32_t e4_to_trig(int32_t e4) {
  return deg_to_trig((int64_t)e4 * 65536, 3600000);
}

uint32_t isqrt_u64(uint64_t v) {
  uint64_t res = 0;
  uint64_t bit = 1ull << 62;
  while (bit > v) bit >>= 2;
  while (bit) {
    if (v >= res + bit) {
      v -= res + bit;
      res = (res >> 1) + bit;
    } else {
      res >>= 1;
    }
    bit >>= 2;
  }
  return (uint32_t)res;
}
