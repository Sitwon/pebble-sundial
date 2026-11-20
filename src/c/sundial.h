#ifndef SUNDIAL_SUNDIAL_H
#define SUNDIAL_SUNDIAL_H

#include <stdbool.h>
#include <stdint.h>

/* Shared app state: location and its provenance. */
typedef struct {
  int32_t lat_e4, lon_e4;
  uint8_t source;   /* 0 default, 1 manual, 2 GPS */
  bool valid;
} Location;

#endif
