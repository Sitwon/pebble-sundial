#include "test.h"
#include <stdlib.h>
#include <time.h>
#include "solar.h"
#include "solar_table.h"

/* unix timestamps, all UTC */
#define T2026_SUMMER_NOON 1782043200   /* 2026-06-21 12:00 */
#define T2026_WINTER_NOON 1797854400   /* 2026-12-21 12:00 */
#define T2026_EQUINOX_9AM 1773997200   /* 2026-03-20 09:00 */

static void check_anchor(time_t utc, int32_t lon_e4,
                         int want_decl, int want_eot, int want_ast, int want_H) {
  SolarState s;
  solar_state(utc, lon_e4, &s);
  CHECK_NEAR(s.decl_cdeg, want_decl, 25);    /* 0.25 deg: year-drift, see comment */
  CHECK_NEAR(s.eot_cmin, want_eot, 30);      /* 0.3 min: year-drift */
  CHECK_NEAR(s.ast_cmin, want_ast, 30);      /* 0.3 min: year-drift */
  CHECK_NEAR(s.hour_angle, want_H, 18);      /* 0.1 deg in TRIG units */
}

int main(void) {
  /* --- anchors (SPEC.md 7.1); lon given in e4 ---
   * Tolerances are wider than the plan's 0.1 deg / 0.2 min: the spec anchors
   * are real-2026-ephemeris values, while the table is date-indexed from 2028
   * (DECISIONS.md #8). At steep-slope dates (equinox declination, December
   * EoT) the ~12 h equinox phase shift over two years costs up to ~0.25 deg
   * declination and ~0.25 min EoT/AST. */
  check_anchor(T2026_SUMMER_NOON, -15, 2343, -186, 71810, -86);    /* 11:58.1, -0.47 deg */
  check_anchor(T2026_WINTER_NOON, -15, -2343, 191, 72190, 86);     /* 12:01.9, +0.47 deg */
  check_anchor(T2026_EQUINOX_9AM, -15, -4, -748, 53240, -8538);    /* 8:52.4, -46.9 deg */
  check_anchor(T2026_SUMMER_NOON - 10 * 3600, 1512093,             /* Sydney 02:00 */
               2343, -176, 72300, 137);
  /* --- longitude sign: 1 deg east = +4 min = +400 cmin --- */
  SolarState a, b;
  solar_state(T2026_SUMMER_NOON, 10000, &a);    /* +1 deg */
  solar_state(T2026_SUMMER_NOON, 0, &b);
  CHECK(a.ast_cmin - b.ast_cmin == 400);
  solar_state(T2026_SUMMER_NOON, -10000, &a);   /* -1 deg */
  CHECK(a.ast_cmin - b.ast_cmin == -400);

  /* --- wrap: ast stays in [0, 144000) near midnight --- */
  solar_state(T2026_SUMMER_NOON + 43140, 0, &a);   /* 23:59 */
  CHECK(a.ast_cmin >= 0 && a.ast_cmin < 144000);
  solar_state(1782086460, 0, &a);   /* 00:01 next day */
  CHECK(a.ast_cmin >= 0 && a.ast_cmin < 144000);
  /* far-east longitude pushes ast past 144000 and must wrap */
  solar_state(T2026_SUMMER_NOON, 1700000, &a);
  CHECK(a.ast_cmin >= 0 && a.ast_cmin < 144000);

  /* --- leap-day indexing via exact table reads (frac = 0) --- */
  solar_state(1835395200, 0, &a);   /* 2028-02-29 00:00 */
  CHECK(a.decl_cdeg == k_decl_cdeg[59] && a.eot_cmin == k_eot_cmin[59]);
  solar_state(1835481600, 0, &a);   /* 2028-03-01 00:00 */
  CHECK(a.decl_cdeg == k_decl_cdeg[60] && a.eot_cmin == k_eot_cmin[60]);
  solar_state(1772323200, 0, &a);   /* 2026-03-01 00:00 (non-leap) */
  CHECK(a.decl_cdeg == k_decl_cdeg[60] && a.eot_cmin == k_eot_cmin[60]);
  solar_state(1772236800, 0, &a);   /* 2026-02-28 00:00 */
  CHECK(a.decl_cdeg == k_decl_cdeg[58] && a.eot_cmin == k_eot_cmin[58]);

  /* --- year wrap: Dec 31 23:59 interpolates 365 -> 0, no out-of-bounds --- */
  solar_state(1798761540, 0, &a);   /* 2026-12-31 23:59 */
  CHECK(a.ast_cmin >= 0 && a.ast_cmin < 144000);
  CHECK(a.decl_cdeg >= k_decl_cdeg[365] - 50 && a.decl_cdeg <= k_decl_cdeg[365] + 50);
  /* interp must move toward day 0, not garbage */
  CHECK((a.decl_cdeg - k_decl_cdeg[365]) * (k_decl_cdeg[0] - k_decl_cdeg[365]) >= 0);

  /* --- time-zone independence --- */
  SolarState tz_a, tz_b;
  solar_state(T2026_SUMMER_NOON, 1512093, &tz_a);
  setenv("TZ", "Pacific/Kiritimati", 1);
  tzset();
  solar_state(T2026_SUMMER_NOON, 1512093, &tz_b);
  CHECK(tz_b.decl_cdeg == tz_a.decl_cdeg && tz_b.eot_cmin == tz_a.eot_cmin);
  CHECK(tz_b.ast_cmin == tz_a.ast_cmin && tz_b.hour_angle == tz_a.hour_angle);
  setenv("TZ", "UTC0", 1);
  tzset();

  /* --- hour_angle in TRIG units: choose lon so AST lands exactly on the hour --- */
  static const struct { int hour, want; } hours[] = {
    {6, -16384}, {12, 0}, {18, 16384},
  };
  for (unsigned i = 0; i < sizeof(hours) / sizeof(hours[0]); i++) {
    time_t utc = T2026_SUMMER_NOON - 12 * 3600 + hours[i].hour * 3600;
    SolarState s0, s1;
    solar_state(utc, 0, &s0);
    int32_t lon = -(int32_t)s0.eot_cmin * 25;
    solar_state(utc, lon, &s1);
    CHECK_NEAR(s1.hour_angle, hours[i].want, 2);
  }

  return test_summary("test_solar");
}
