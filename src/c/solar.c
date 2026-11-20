#include <time.h>
#include "solar.h"
#include "fixmath.h"
#include "solar_table.h"

/* month start indices in the 366-day leap table */
static const uint16_t k_month_start_leap[12] = {
  0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335
};

/* a + (b-a)*frac/65536, symmetric rounding */
static int32_t interp(int32_t a, int32_t b, int32_t frac_q16) {
  return a + (int32_t)(((int64_t)(b - a) * frac_q16) / 65536);
}

void solar_state(time_t utc, int32_t lon_e4, SolarState *out) {
  struct tm *tm_utc = gmtime(&utc);

  int idx = k_month_start_leap[tm_utc->tm_mon] + tm_utc->tm_mday - 1;
  int next = (idx + 1) % SOLAR_TABLE_DAYS;

  int32_t sod = tm_utc->tm_hour * 3600 + tm_utc->tm_min * 60 + tm_utc->tm_sec;
  int32_t frac_q16 = (int32_t)(((int64_t)sod << 16) / 86400);

  out->decl_cdeg = (int16_t)interp(k_decl_cdeg[idx], k_decl_cdeg[next], frac_q16);
  out->eot_cmin = (int16_t)interp(k_eot_cmin[idx], k_eot_cmin[next], frac_q16);

  int32_t ast = sod * 100 / 60 + lon_e4 / 25 + out->eot_cmin;
  ast %= 144000;
  if (ast < 0) ast += 144000;
  out->ast_cmin = ast;

  out->hour_angle = (int32_t)(((int64_t)(ast - 72000) * 65536) / 144000);
}
