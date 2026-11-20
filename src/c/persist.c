#include "persist.h"

#define PERSIST_KEY_LAT    0x10
#define PERSIST_KEY_LON    0x11
#define PERSIST_KEY_SRC    0x12
#define PERSIST_KEY_VALID  0x13

void persist_location_save(const Location *loc) {
  persist_write_int(PERSIST_KEY_LAT, loc->lat_e4);
  persist_write_int(PERSIST_KEY_LON, loc->lon_e4);
  persist_write_int(PERSIST_KEY_SRC, loc->source);
  persist_write_bool(PERSIST_KEY_VALID, loc->valid);
}

bool persist_location_load(Location *loc) {
  if (!persist_exists(PERSIST_KEY_LAT) || !persist_exists(PERSIST_KEY_LON)) {
    return false;
  }
  loc->lat_e4 = persist_read_int(PERSIST_KEY_LAT);
  loc->lon_e4 = persist_read_int(PERSIST_KEY_LON);
  loc->source = persist_exists(PERSIST_KEY_SRC)
    ? (uint8_t)persist_read_int(PERSIST_KEY_SRC) : 0;
  loc->valid = persist_read_bool(PERSIST_KEY_VALID);
  return true;
}
