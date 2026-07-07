#ifndef CACHE_H
#define CACHE_H

#include "config.h"
#include "prayertimes.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_TRIGGERS 64

typedef struct {
  char prayer[16];            // Prayer name (e.g., "fajr")
  char adhan[MAX_ADHAN_PATH]; // path to adhan sound
  int minute;                 // Absolute minute of day (hour*60 + min)
  int minutes_before;         // 0 = exact time, >0 = reminder
  double prayer_time;         // Prayer time in decimal hours
  bool adhan_enabled;         // true if adhan is enabled
} CacheTrigger;

typedef struct {
  char date[16]; // "YYYY-MM-DD"
  CacheTrigger triggers[MAX_TRIGGERS];
  int trigger_count;
} PrayerCache;

/**
 * Get cache file path (~/.cache/muslimtify/next_prayer.json)
 */
const char *cache_get_path(void);

/**
 * Load cache from disk.
 * Returns: 0 on success, -1 on error (missing/corrupt/wrong date)
 */
int cache_load(PrayerCache *cache);

/**
 * Save cache to disk.
 * Returns: 0 on success, -1 on error
 */
int cache_save(const PrayerCache *cache);

/**
 * Delete cache file (invalidate).
 */
void cache_invalidate(void);

/**
 * Build trigger list from current time and prayer times.
 * Only includes triggers at or after current_minute.
 * Returns: number of triggers added
 */
int cache_build_triggers(PrayerCache *cache, const Config *cfg, const struct PrayerTimes *times,
                         int current_minute, const char *date_str);

/**
 * Remove a trigger by index (caller must call cache_save afterward).
 */
void cache_remove_trigger(PrayerCache *cache, int index);

/**
 * Reset cached path (for testing with different XDG_CACHE_HOME).
 */
void cache_reset_path(void);

#ifdef __cplusplus
}
#endif

#endif // CACHE_H
