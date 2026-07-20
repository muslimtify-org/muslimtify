#ifndef DISPLAY_H
#define DISPLAY_H

#include "config.h"
#include "prayertimes.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Display prayer times in table format
 */
void display_prayer_times_table(const struct PrayerTimes *times, const Config *cfg,
                                struct tm *date);

/**
 * Display prayer times in plain key=value format (only enabled prayers)
 */
void display_prayer_times_plain(const struct PrayerTimes *times, const Config *cfg,
                                struct tm *date);

/**
 * Display prayer times in JSON format
 */
void display_prayer_times_json(const struct PrayerTimes *times, const Config *cfg, struct tm *date);

/**
 * Display prayer times for an inclusive date range as a combined table
 * (columns: Date | Prayer | Time), all 7 prayers per day.
 * Precondition: the caller must bound the range; this function loops one day at
 * a time and does not cap the span itself (see MAX_RANGE_DAYS in cmd_show.c).
 */
void display_prayer_times_range_table(const Config *cfg, int sy, int sm, int sd, int ey, int em,
                                      int ed);

/**
 * Display prayer times for an inclusive date range as `date=` blocks of
 * lowercase key=value lines (enabled prayers only), blank-line separated.
 * Precondition: the caller must bound the range; this function loops one day at
 * a time and does not cap the span itself (see MAX_RANGE_DAYS in cmd_show.c).
 */
void display_prayer_times_range_plain(const Config *cfg, int sy, int sm, int sd, int ey, int em,
                                      int ed);

/**
 * Display prayer times for an inclusive date range as a JSON array of
 * { "date": "YYYY-MM-DD", "prayers": { ... } } objects.
 * Precondition: the caller must bound the range; this function loops one day at
 * a time and does not cap the span itself (see MAX_RANGE_DAYS in cmd_show.c).
 */
void display_prayer_times_range_json(const Config *cfg, int sy, int sm, int sd, int ey, int em,
                                     int ed);

/**
 * Display next prayer info
 */
void display_next_prayer(const struct PrayerTimes *times, const Config *cfg,
                         struct tm *current_time);

/**
 * Display next prayer in lowercase key=value form
 */
void display_next_prayer_headless(const struct PrayerTimes *times, const Config *cfg,
                                  struct tm *current_time);

/**
 * Display next prayer as a JSON object {prayer, time, remaining}
 */
void display_next_prayer_json(const struct PrayerTimes *times, const Config *cfg,
                              struct tm *current_time);

/**
 * Display location info
 */
void display_location(const Config *cfg);

/**
 * Display location info as JSON
 */
void display_location_json(const Config *cfg);

/**
 * Display location info as lowercase key=value
 */
void display_location_headless(const Config *cfg);

/** Display notification settings as a table */
void display_notification_settings(const Config *cfg);
/** Display notification settings as JSON */
void display_notification_settings_json(const Config *cfg);
/** Display notification settings as lowercase key=value */
void display_notification_settings_headless(const Config *cfg);

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_H
