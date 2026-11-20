#include "test.h"
#include "location.h"

int main(void) {
  /* valid coordinates */
  CHECK(location_valid(0, 0));
  CHECK(location_valid(900000, 1800000));      /* 90N 180E */
  CHECK(location_valid(-900000, -1800000));    /* 90S 180W */
  CHECK(location_valid(514779, -15));

  /* invalid */
  CHECK(!location_valid(900001, 0));
  CHECK(!location_valid(-900001, 0));
  CHECK(!location_valid(0, 1800001));
  CHECK(!location_valid(0, -1800001));
  CHECK(!location_valid(-2147483647 - 1, -2147483647 - 1));
  CHECK(!location_valid(2147483647, 2147483647));

  return test_summary("test_location");
}
