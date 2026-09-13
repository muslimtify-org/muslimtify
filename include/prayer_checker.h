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
  PRAYER_DHUHR,
  PRAYER_ASR,
  PRAYER_MAGHRIB,
  PRAYER_ISHA,
  PRAYER_NONE
} PrayerType;

/* The number of real prayers, which is every value before PRAYER_NONE. Loops
   over the parallel prayer arrays must use this rather than a literal, so that
   adding or removing a prayer cannot leave a stale bound behind. */
#define PRAYER_COUNT ((int)PRAYER_NONE)

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
 * The next enabled prayer at or after `now`, to the minute.
 *
 * `day_delta` says which day's times `time` was taken from, relative to the
 * date in `now`: -1 for the previous day, 0 for the same day, +1 for the next.
 * `time` is in decimal hours on that source day, so it can be below 0 or at or
 * above 24 when the prayer crosses midnight. `minutes_until` is rounded the way
 * format_time_hm rounds, so it agrees with the printed time. `type` is
 * PRAYER_NONE when no enabled prayer has a time in the window.
 */
typedef struct {
  PrayerType type;
  int minutes_until;
  int day_delta;
  double time;
} NextPrayer;

/**
 * Pick the next prayer from the previous, same and next day's times, in that
 * order in `days`. Three days are needed because a prayer can cross midnight in
 * either direction: just after midnight the previous day's isha can still be
 * ahead, and it is not in the same day's times at all.
 */
NextPrayer prayer_next_from_days(const Config *cfg, const struct tm *now,
                                 const struct PrayerTimes days[3]);

/**
 * prayer_next_from_days for `now`, given the times for now's date. The
 * neighbouring days are computed from `cfg`.
 */
NextPrayer prayer_get_next(const Config *cfg, const struct tm *now,
                           const struct PrayerTimes *today);

#ifdef __cplusplus
}
#endif

#endif // PRAYER_CHECKER_H
