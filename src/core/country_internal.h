#ifndef COUNTRY_INTERNAL_H
#define COUNTRY_INTERNAL_H

// Test-only view of the internal ISO 3166-1 alpha-2 table. NOT part of the
// public API (include/country.h); no production code includes this. Exposed so
// invariant tests can iterate the table (sort order, method completeness) after
// the public table accessor was removed from include/country.h.

#include "country.h"

#include <stddef.h>

extern const Country ISO_3166_1_ALPHA2[];
extern const size_t ISO_3166_1_ALPHA2_COUNT;

#endif // COUNTRY_INTERNAL_H
