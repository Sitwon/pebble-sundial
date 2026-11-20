#include "location.h"

bool location_valid(int32_t lat_e4, int32_t lon_e4) {
  return lat_e4 >= -900000 && lat_e4 <= 900000 &&
         lon_e4 >= -1800000 && lon_e4 <= 1800000;
}
