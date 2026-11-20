#include "solar.h"
#include "fixmath.h"
#include "trig.h"
#include "solar_table.h"

/* month start indices in the 366-day leap table */
static const uint16_t k_month_start_leap[12] = {
  0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335
};

/* a + (b-a)*frac/65536, symmetric rounding */
static int32_t interp(int32_t a, int32_t b, int32_t frac_q16) {
  return a + (int32_t)(((int64_t)(b - a) * frac_q16) / 65536);
}

/* no SDK time dependency: break the UTC epoch down with the civil-date
 * algorithm (equivalent to gmtime, TZ-independent by construction) */
static void utc_broken(int64_t utc, int *idx, int32_t *sod) {
  int32_t days = (int32_t)(utc / 86400);
  *sod = (int32_t)(utc % 86400);

  int32_t z = days + 719468;
  int32_t era = (z >= 0 ? z : z - 146096) / 146097;
  int32_t doe = z - era * 146097;
  int32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  int32_t mp = (5 * doy + 2) / 153;
  int32_t d = doy - (153 * mp + 2) / 5 + 1;
  int32_t m = mp + (mp < 10 ? 3 : -9);

  *idx = k_month_start_leap[m - 1] + d - 1;
}

void solar_state(time_t utc, int32_t lon_e4, SolarState *out) {
  int idx;
  int32_t sod;
  utc_broken((int64_t)utc, &idx, &sod);
  int next = (idx + 1) % SOLAR_TABLE_DAYS;
  int32_t frac_q16 = (int32_t)(((int64_t)sod << 16) / 86400);

  out->decl_cdeg = (int16_t)interp(k_decl_cdeg[idx], k_decl_cdeg[next], frac_q16);
  out->eot_cmin = (int16_t)interp(k_eot_cmin[idx], k_eot_cmin[next], frac_q16);

  int32_t ast = sod * 100 / 60 + lon_e4 / 25 + out->eot_cmin;
  ast %= 144000;
  if (ast < 0) ast += 144000;
  out->ast_cmin = ast;

  out->hour_angle = (int32_t)(((int64_t)(ast - 72000) * 65536) / 144000);
}

Vec3q16 sun_vector(const SolarState *s, int32_t lat_e4) {
  int32_t sin_lat = tri_sin(e4_to_trig(lat_e4));
  int32_t cos_lat = tri_cos(e4_to_trig(lat_e4));
  int32_t sin_d = tri_sin(cdeg_to_trig(s->decl_cdeg));
  int32_t cos_d = tri_cos(cdeg_to_trig(s->decl_cdeg));
  int32_t sin_H = tri_sin(s->hour_angle);
  int32_t cos_H = tri_cos(s->hour_angle);

  Vec3q16 u;
  u.x = -q16_mul(cos_d, sin_H);
  u.y = q16_mul(sin_d, cos_lat) - q16_mul(cos_d, q16_mul(sin_lat, cos_H));
  u.z = q16_mul(sin_d, sin_lat) + q16_mul(cos_d, q16_mul(cos_lat, cos_H));
  return u;
}
