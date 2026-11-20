#include "test.h"
#include <math.h>
#include "dial.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const struct { int32_t lat_e4, pole, lat_abs; } clamps[] = {
  {0, 1, 150000}, {100000, 1, 150000}, {150000, 1, 150000},
  {514779, 1, 514779}, {750000, 1, 750000}, {890000, 1, 750000},
  {-100000, -1, 150000}, {-514779, -1, 514779}, {-890000, -1, 750000},
};

int main(void) {
  /* --- clamping and pole --- */
  for (unsigned i = 0; i < sizeof(clamps) / sizeof(clamps[0]); i++) {
    Dial d;
    dial_init(&d, clamps[i].lat_e4);
    CHECK(d.pole == clamps[i].pole);
    CHECK(d.lat_abs_e4 == clamps[i].lat_abs);
    CHECK(d.h_q10 > 0 && d.L_q10 > 0 && d.w_q10 > 0);  /* no zero division */
  }

  /* --- gnomon dimensions (SPEC 3.5) --- */
  Dial d;
  dial_init(&d, 514779);
  CHECK_NEAR(d.h_q10, 532, 1);
  CHECK_NEAR(d.L_q10, 423, 2);
  CHECK(d.w_q10 == 61);

  dial_init(&d, 250000);
  CHECK_NEAR(d.h_q10, 344, 2);
  CHECK_NEAR(d.L_q10, 737, 2);

  dial_init(&d, 750000);
  CHECK_NEAR(d.h_q10, 532, 1);
  CHECK_NEAR(d.L_q10, 142, 2);

  /* southern hemisphere: same absolute geometry */
  dial_init(&d, 514779);
  Dial ds;
  dial_init(&ds, -514779);
  CHECK(ds.h_q10 == d.h_q10 && ds.L_q10 == d.L_q10 && ds.w_q10 == d.w_q10);
  CHECK(ds.sin_lat == d.sin_lat && ds.cos_lat == d.cos_lat);

  /* clamped latitudes behave like their clamp value */
  Dial d15;
  dial_init(&d15, 150000);
  dial_init(&d, 0);
  CHECK(d.h_q10 == d15.h_q10 && d.L_q10 == d15.L_q10);
  dial_init(&d15, 750000);      /* -89 deg clamps to 75 */
  dial_init(&d, -890000);
  CHECK(d.h_q10 == d15.h_q10 && d.L_q10 == d15.L_q10);

  /* --- hour ring (step 3.2) --- */
  /* expected clockwise-from-up angles at 51.4779 N (SPEC 3.4) */
  static const double want[13] = {
    270.0, 288.9, 306.4, 322.0, 335.7, 348.2, 0.0,
    11.8, 24.3, 38.0, 53.6, 71.1, 90.0
  };
  dial_init(&d, 514779);
  for (int i = 0; i < 13; i++) {
    PointQ10 s = dial_to_screen(&d, d.hour_dir[i]);
    double ang = atan2(s.x, -s.y) * 180.0 / M_PI;
    if (ang < 0) ang += 360.0;
    CHECK_NEAR(ang, want[i], 0.15);
  }

  /* 9am sanity anchors (normalized) */
  PointQ10 s9 = dial_to_screen(&d, d.hour_dir[3]);
  CHECK_NEAR((double)s9.x / 1024, -0.616, 0.01);
  CHECK_NEAR((double)s9.y / 1024, -0.788, 0.01);

  /* 6am is due west locally, screen-left in NH */
  CHECK(d.hour_dir[0].y == 0);
  CHECK(d.hour_dir[0].x < 0);
  CHECK(s9.x < 0 && s9.y < 0);
  /* 6pm is due east locally, screen-right in NH */
  CHECK(d.hour_dir[12].y == 0);
  CHECK(d.hour_dir[12].x > 0);

  /* hour vectors have length ~1024 */
  for (int i = 0; i < 13; i++) {
    int64_t len2 = (int64_t)d.hour_dir[i].x * d.hour_dir[i].x
                 + (int64_t)d.hour_dir[i].y * d.hour_dir[i].y;
    /* Q10 truncation: up to ±1 per axis */
    CHECK_NEAR(len2, 1024ll * 1024, 2049);
  }

  /* southern ring is the exact east-west mirror of the northern ring */
  dial_init(&ds, -514779);
  for (int i = 0; i < 13; i++) {
    PointQ10 sn = dial_to_screen(&d, d.hour_dir[i]);
    PointQ10 ss = dial_to_screen(&ds, ds.hour_dir[i]);
    CHECK(ss.x == -sn.x);
    CHECK(ss.y == sn.y);
  }
  /* 6am screen-right in SH, 6pm screen-left */
  PointQ10 s6s = dial_to_screen(&ds, ds.hour_dir[0]);
  CHECK(s6s.x > 0 && s6s.y == 0);

  /* 0 deg latitude uses the clamped 15 deg geometry */
  Dial d0, d15b;
  dial_init(&d0, 0);
  dial_init(&d15b, 150000);
  for (int i = 0; i < 13; i++) {
    CHECK(d0.hour_dir[i].x == d15b.hour_dir[i].x);
    CHECK(d0.hour_dir[i].y == d15b.hour_dir[i].y);
  }

  /* dial_to_screen mapping */
  Dial dtmp_n, dtmp_s;
  dial_init(&dtmp_n, 514779);
  dial_init(&dtmp_s, -514779);
  PointQ10 loc = {17, -23};
  PointQ10 n = dial_to_screen(&dtmp_n, loc);
  CHECK(n.x == 17 && n.y == 23);
  PointQ10 sth = dial_to_screen(&dtmp_s, loc);
  CHECK(sth.x == -17 && sth.y == -23);

  return test_summary("test_dial");
}
