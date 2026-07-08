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

/**
 * Display config
 */
void display_config(const Config *cfg);

/**
 * Display prayer list with status
 */
void display_prayer_list(const Config *cfg);

/**
 * Display reminders configuration
 */
void display_reminders(const Config *cfg);

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_H
