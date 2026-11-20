#include "test.h"
#include <math.h>
#include "dial.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double screen_angle_of(PointQ10 s) {
  double a = atan2(s.x, -s.y) * 180.0 / M_PI;
  if (a < 0) a += 360.0;
  return a;
}

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

  /* --- step 3.3: visibility, projection, hull, style tip --- */
  Vec3q16 uz_edge = {0, 0, 2287};
  CHECK(dial_shadow_visible(uz_edge));
  Vec3q16 uz_below = {0, 0, 2286};
  CHECK(!dial_shadow_visible(uz_below));
  Vec3q16 uz_neg = {0, 0, -100};
  CHECK(!dial_shadow_visible(uz_neg));

  /* six projected points at a daytime sun are finite and hull-able */
  dial_init(&d, 514779);
  SolarState sn;
  solar_state(1782043200, -15, &sn);
  Vec3q16 uday = sun_vector(&sn, 514779);
  PointQ10 pts[6], hull[6];
  dial_shadow_points(&d, uday, pts);
  for (int i = 0; i < 6; i++) {
    CHECK(pts[i].x > -1000000 && pts[i].x < 1000000);
    CHECK(pts[i].y > -1000000 && pts[i].y < 1000000);
  }
  int nh = convex_hull(pts, hull);
  CHECK(nh >= 3 && nh <= 6);

  /* hull: convex order, no duplicates, contains all inputs */
  for (int i = 0; i < nh; i++) {
    int j = (i + 1) % nh, k = (i + 2) % nh;
    int64_t cross = (int64_t)(hull[j].x - hull[i].x) * (hull[k].y - hull[i].y)
                  - (int64_t)(hull[j].y - hull[i].y) * (hull[k].x - hull[i].x);
    CHECK(cross > 0);   /* strictly convex, CCW */
  }
  for (int i = 0; i < 6; i++) {
    int inside = 1;
    for (int j = 0; j < nh; j++) {
      int k = (j + 1) % nh;
      int64_t cross = (int64_t)(hull[k].x - hull[j].x) * (pts[i].y - hull[j].y)
                    - (int64_t)(hull[k].y - hull[j].y) * (pts[i].x - hull[j].x);
      if (cross < -1) inside = 0;
    }
    CHECK(inside);
  }

  /* night sun: outputs zeroed, still safe */
  dial_shadow_points(&d, uz_neg, pts);
  int all_zero = 1;
  for (int i = 0; i < 6; i++)
    if (pts[i].x != 0 || pts[i].y != 0) all_zero = 0;
  CHECK(all_zero);

  /* style tip at solar noon points up in both hemispheres */
  SolarState noon = {2343, 0, 72000, 0};
  Vec3q16 un = sun_vector(&noon, 514779);
  PointQ10 tip = dial_style_tip(&d, un);
  CHECK_NEAR(screen_angle_of(dial_to_screen(&d, tip)), 0.0, 0.2);

  Dial dsyd;
  dial_init(&dsyd, -338688);
  Vec3q16 un_s = sun_vector(&noon, -338688);
  tip = dial_style_tip(&dsyd, un_s);
  CHECK_NEAR(screen_angle_of(dial_to_screen(&dsyd, tip)), 0.0, 0.2);

  /* tip direction matches the hour_dir entry at 9am solar (H = -45 deg) */
  SolarState am = {2343, 0, 54000, -8192};
  Vec3q16 uam = sun_vector(&am, 514779);
  tip = dial_style_tip(&d, uam);
  PointQ10 hd = d.hour_dir[3];
  double tl = sqrt((double)tip.x * tip.x + (double)tip.y * tip.y);
  double hl = sqrt((double)hd.x * hd.x + (double)hd.y * hd.y);
  double dot = ((double)tip.x * hd.x + (double)tip.y * hd.y) / (tl * hl);
  CHECK(dot > 0.9995);

  /* reading-edge distance: winter noon > summer noon > 0 */
  SolarState wnoon = {-2343, 0, 72000, 0};
  Vec3q16 uw = sun_vector(&wnoon, 514779);
  PointQ10 tip_summer = dial_style_tip(&d, un);
  PointQ10 tip_winter = dial_style_tip(&d, uw);
  int64_t sum2 = (int64_t)tip_summer.x * tip_summer.x + (int64_t)tip_summer.y * tip_summer.y;
  int64_t win2 = (int64_t)tip_winter.x * tip_winter.x + (int64_t)tip_winter.y * tip_winter.y;
  CHECK(win2 > sum2 && sum2 > 0);

  /* anchor screen angles */
  SolarState syd;
  solar_state(1782007200, 1512093, &syd);   /* Sydney 2026-06-21 02:00 */
  Vec3q16 usyd = sun_vector(&syd, -338688);
  tip = dial_style_tip(&dsyd, usyd);
  CHECK_NEAR(screen_angle_of(dial_to_screen(&dsyd, tip)), 359.6, 0.3);

  Vec3q16 uam_s = sun_vector(&am, -338688);
  tip = dial_style_tip(&dsyd, uam_s);
  CHECK_NEAR(screen_angle_of(dial_to_screen(&dsyd, tip)), 29.1, 0.5);

  tip = dial_style_tip(&d, uam);
  CHECK_NEAR(screen_angle_of(dial_to_screen(&d, tip)), 322.0, 0.5);

  /* --- step 3.4: penumbra band scale (all args Q10; 1024 == R == 94 px) --- */
  CHECK(band_scale_q16(600, 0) == 65536);
  CHECK(band_scale_q16(0, 0) == 65536);
  int32_t prev = -1;
  for (int32_t px = 0; px <= 6; px++) {
    int32_t s = band_scale_q16(654, px * 11);   /* 60 px reach, 0..6 px offset */
    CHECK(s >= prev);
    CHECK(s >= 65536 && s <= 131072);
    prev = s;
  }
  /* 0..6 px over a 60 px reach spans ~1.0..~1.1 */
  CHECK_NEAR(band_scale_q16(654, 66), 72144, 60);
  /* extreme offset stays capped */
  CHECK(band_scale_q16(1, 100000) <= 131072);

  return test_summary("test_dial");
}
