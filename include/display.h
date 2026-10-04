#ifndef DISPLAY_H
#define DISPLAY_H

// vibekit: config.h is here only for the location and notification settings
// renderers below. Piece 2 gives them library structs and drops this include.
#include "config.h"
#include "muslimtify.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * One day as a table, one row per prayer. The Date column carries the day each
 * prayer actually falls on. The row at day->next is highlighted.
 */
void display_day_table(const MuslimtifyDay *day, int time_format);

/** One day as lowercase key=value lines, enabled prayers only. */
void display_day_plain(const MuslimtifyDay *day, int time_format);

/** One day as a JSON object { "date", "prayers": { ... } }. */
void display_day_json(const MuslimtifyDay *day, int time_format);

/**
 * A run of days as a combined table, one row per day and one column per
 * enabled prayer. A time on another day carries a '+' or '-' marker.
 */
void display_range_table(const MuslimtifyDay *days, size_t count, int time_format);

/** A run of days as `date=` blocks of key=value lines, blank-line separated. */
void display_range_plain(const MuslimtifyDay *days, size_t count, int time_format);

/** A run of days as a JSON array of { "date", "prayers": { ... } } objects. */
void display_range_json(const MuslimtifyDay *days, size_t count, int time_format);

/** The next prayer as a table. */
void display_next_table(const MuslimtifyNext *next, int time_format);

/** The next prayer as lowercase key=value lines. */
void display_next_plain(const MuslimtifyNext *next, int time_format);

/** The next prayer as a JSON object {date, prayer, time, remaining}. */
void display_next_json(const MuslimtifyNext *next, int time_format);

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
