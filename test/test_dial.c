#include "test.h"
#include "dial.h"

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

  return test_summary("test_dial");
}
