#ifndef PRAYER_CHECKER_H
#define PRAYER_CHECKER_H

#include "config.h"
#include "prayertimes.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  PRAYER_FAJR,
  PRAYER_SUNRISE,
  PRAYER_DHUHA,
  PRAYER_DHUHR,
  PRAYER_ASR,
  PRAYER_MAGHRIB,
  PRAYER_ISHA,
  PRAYER_NONE
} PrayerType;

/**
 * Get human-readable prayer name
 */
const char *prayer_get_name(PrayerType type);

/**
 * Get prayer time from PrayerTimes struct by type
 */
double prayer_get_time(const struct PrayerTimes *times, PrayerType type);

/**
 * Check if prayer is enabled
 */
bool prayer_is_enabled(const Config *cfg, PrayerType type);

/**
 * Get prayer config by type
 */
const PrayerConfig *prayer_get_config(const Config *cfg, PrayerType type);

/**
 * Get next prayer type and time remaining in minutes
 */
PrayerType prayer_get_next(const Config *cfg, struct tm *now, struct PrayerTimes *times,
                           int *minutes_until);

#ifdef __cplusplus
}
#endif

#endif // PRAYER_CHECKER_H
