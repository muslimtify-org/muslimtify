#ifndef DISPLAY_H
#define DISPLAY_H

#include "config.h"
#include "prayer_helper.h"
#include "prayertimes.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Display prayer times in table format
 */
void display_prayer_times_table(const PrayerSnapshot *snap);

/**
 * Display prayer times in plain key=value format (only enabled prayers)
 */
void display_prayer_times_plain(const PrayerSnapshot *snap);

/**
 * Display prayer times in JSON format
 */
void display_prayer_times_json(const PrayerSnapshot *snap);

/**
 * Display next prayer info
 */
void display_next_prayer(const PrayerSnapshot *snap);

/**
 * Display location info
 */
void display_location(const Config *cfg);

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
