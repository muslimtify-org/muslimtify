#ifndef LOCATION_H
#define LOCATION_H

#include "config.h"

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Compute the UTC offset (in hours) for IANA timezone `tz_name` at the
 * moment `when`. Reads the system tzdb via libc, so DST and historical
 * zone changes are honored. Returns 0.0 if `tz_name` is NULL.
 *
 * Not thread-safe: temporarily mutates the process-wide TZ env var.
 */
double parse_timezone_offset(const char *tz_name, time_t when);

/**
 * Fetch location information from ipinfo.io and update config.
 * Returns: 0 on success, -1 on failure.
 */
int location_fetch(Config *cfg);

/**
 * Quiet helper that ensures location data exists.
 * Returns: 0 on success, -1 on failure.
 * This function does not print user-facing status lines.
 */
int location_prepare(Config *cfg);

/**
 * CLI-facing wrapper around location preparation.
 * Preserves interactive status output when auto-detect runs.
 */
int ensure_location(Config *cfg);

/**
 * Alias for location_fetch (for clarity)
 */
int location_auto_detect(Config *cfg);

/**
 * Free any resources allocated by location module
 */
void location_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // LOCATION_H
