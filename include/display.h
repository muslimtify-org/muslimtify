#ifndef DISPLAY_H
#define DISPLAY_H

#include "config.h"
#include "prayertimes.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Day the event falls on relative to the date it was computed for: +1 when the
 * clock time is on the next calendar day, -1 when it is on the previous one,
 * and 0 otherwise. A non-finite time has no day, so it returns 0.
 *
 * The rounding matches format_time_hm, which rounds the minute up, so the day
 * and the clock string this returns a day for can never disagree. A time of
 * 23.999 rounds up to 24:00, which is the next day, and this reports it as one.
 */
int prayer_time_day_offset(double hours);

/**
 * Format a decimal-hours time into "HH:MM" followed by '+' when the event falls
 * on the next calendar day and '-' when it falls on the previous one, so that a
 * row of five prayers reads in the order they occur. A non-finite time renders
 * as "--:--" with no marker. Seven bytes are enough for either.
 */
void format_time_hm_day(double hours, char *outBuffer, size_t bufSize);

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
 * Display next prayer as a JSON object {date, prayer, time, remaining}, where date
 * is the calendar day the prayer actually falls on
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
