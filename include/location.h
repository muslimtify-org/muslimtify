#ifndef LOCATION_H
#define LOCATION_H

#include "config.h"
#include "platform.h"

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
 * Return true iff `tz_name` is a safe IANA-style timezone string: non-NULL,
 * non-empty, at most 64 chars, no leading ':', and every character is ASCII
 * alphanumeric or one of '_', '+', '-', '/'. Guards the setenv("TZ")/tzset()
 * path against hostile ipinfo.io responses and corrupted config values.
 */
bool timezone_name_is_valid(const char *tz_name);

/**
 * Write the host system's IANA timezone name (e.g. "Asia/Jakarta") into
 * `buf` (capacity `cap`, NUL-terminated). Used by `location set` to refresh
 * the timezone after the user supplies coordinates without going through
 * the ipinfo.io geolocation path.
 *
 * On Linux, reads /etc/localtime (symlink) with /etc/timezone fallback.
 * On Windows, reverses GetDynamicTimeZoneInformation()'s TimeZoneKeyName
 * against the IANA<->Windows table.
 *
 * Returns 0 on success, -1 on failure. On failure `buf` is set to "UTC"
 * when cap allows. `buf` must be non-NULL and `cap >= 2`.
 */
int get_system_timezone(char *buf, size_t cap);

/**
 * Fetch location information from ipinfo.io and update config.
 * Returns: 0 on success, -1 on failure.
 */
int location_fetch(Config *cfg);

/**
 * Try to read coordinates from a local gpsd receiver. On GPS_OK, writes
 * latitude/longitude and derives the timezone from the host system (GPS carries
 * no timezone); country is left unchanged. Returns a GpsStatus describing the
 * outcome. Exposed so `location gps on` can probe before enabling.
 */
GpsStatus location_fetch_gps(Config *cfg);

/**
 * Orchestrator core with injected sources (test seam, mirrors
 * location_refresh_with). When cfg->use_gps is set it calls gps() first: a
 * GPS_OK returns immediately; a structural failure (GPS_NO_DAEMON /
 * GPS_NO_DEVICE / GPS_UNAVAILABLE) warns once on stderr and sets use_gps=false
 * so the next fetch stops trying; a GPS_NO_FIX is silent and keeps use_gps on.
 * Any non-OK GPS outcome falls through to ipinfo(). Returns the 0/-1 of the
 * source that produced the result.
 */
int location_fetch_core(Config *cfg, GpsStatus (*gps)(Config *), int (*ipinfo)(Config *));

/**
 * Quiet helper that ensures location data exists.
 * Returns: 0 on success, -1 on failure.
 * This function does not print user-facing status lines.
 */
int location_prepare(Config *cfg);

/**
 * Force a fresh location lookup from ipinfo.io, ignoring any cached
 * coordinates, and persist the result. Unlike location_prepare(), this does
 * NOT skip when coordinates are already set — it always re-fetches. Intended
 * to run once at daemon startup so a machine that moved between boots picks
 * up its new location.
 *
 * Fail-safe by design:
 *   - If auto_detect is disabled, this is a no-op (returns 0), leaving a
 *     manually-set location untouched. Note the converse: with auto_detect
 *     enabled, any hand-edited coordinates are replaced on each refresh — set
 *     auto_detect=false (e.g. via `location set`) to pin coordinates.
 *   - If the network fetch fails, the passed-in config is left unmodified
 *     and -1 is returned, so a boot with no network keeps the last known
 *     good location instead of wiping it to 0,0.
 *
 * On success `*cfg` is updated (latitude/longitude/timezone/country) and
 * saved to disk. The calculation_method is intentionally left as-is.
 *
 * Returns 0 on success or when auto_detect is off, -1 on fetch/save failure.
 */
int location_refresh(Config *cfg);

/**
 * Pure, network-free staleness check for the daemon check cycle. Returns true
 * iff auto-detect is on, the interval is enabled (> 0), and the saved location
 * is at least `refresh_interval` seconds old. `updated_at == 0` (never fetched)
 * with a positive interval is always stale.
 */
bool location_is_stale(const Config *cfg, int64_t now);

/**
 * CLI-facing wrapper around location preparation.
 * Preserves interactive status output when auto-detect runs.
 */
int ensure_location(Config *cfg);

/**
 * Auto-detect: fetch location via ipinfo and set calculation_method from the
 * detected country (via country_default_method). Mutates *cfg only — does NOT
 * save config or print. Returns 0 on success, -1 on fetch failure.
 *
 * Used by `daemon enable` (Linux + Windows).
 */
int config_auto_detect(Config *cfg);

#ifdef __cplusplus
}
#endif

#endif // LOCATION_H
