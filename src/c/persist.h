#ifndef SUNDIAL_PERSIST_H
#define SUNDIAL_PERSIST_H

#include "sundial.h"

/* Save the current location to persistent storage. */
void persist_location_save(const Location *loc);

/* Load location from persistent storage; sets *loc. Returns true on success. */
bool persist_location_load(Location *loc);

#endif
