#ifndef MUSLIMTIFY_H
#define MUSLIMTIFY_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handle holding the loaded configuration. */
typedef struct Muslimtify Muslimtify;

/* New codes are appended. Existing values are never reordered. */
typedef enum {
  MUSLIMTIFY_OK = 0,
  MUSLIMTIFY_ERR_INVALID_ARG,
  MUSLIMTIFY_ERR_NO_MEMORY,
  MUSLIMTIFY_ERR_CONFIG_LOAD,
  MUSLIMTIFY_ERR_NO_LOCATION,
  MUSLIMTIFY_ERR_INVALID_DATE,
  MUSLIMTIFY_ERR_DATE_ORDER,
  MUSLIMTIFY_ERR_RANGE_TOO_LONG
} MuslimtifyError;

typedef enum {
  MUSLIMTIFY_FAJR,
  MUSLIMTIFY_DHUHR,
  MUSLIMTIFY_ASR,
  MUSLIMTIFY_MAGHRIB,
  MUSLIMTIFY_ISHA,
  MUSLIMTIFY_PRAYER_COUNT
} MuslimtifyPrayerType;

#define MUSLIMTIFY_MAX_REMINDERS 10
#define MUSLIMTIFY_MAX_RANGE_DAYS 366
#define MUSLIMTIFY_TIME_STR_SIZE 9

typedef struct {
  int year;       /* calendar day this time falls on */
  int month;      /* 1-12 */
  int day;        /* 1-31 */
  int hour;       /* 0-23, wall clock at the configured location */
  int minute;     /* 0-59 */
  int day_offset; /* -1, 0 or +1 relative to the date the time was computed for */
  time_t instant; /* UTC epoch seconds */
  bool valid;     /* false when the sun never reaches the angle at this latitude */
} MuslimtifyTime;

typedef struct {
  MuslimtifyTime time;
  bool enabled;
  bool adhan_enabled;
  int offset; /* configured shift in minutes, already applied to time */
  int reminders[MUSLIMTIFY_MAX_REMINDERS];
  int reminder_count;
} MuslimtifyPrayer;

typedef struct {
  int year;
  int month; /* 1-12 */
  int day;   /* 1-31 */
  MuslimtifyPrayer prayers[MUSLIMTIFY_PRAYER_COUNT];
  int next; /* index of the upcoming prayer when this is today, otherwise -1 */
} MuslimtifyDay;

typedef struct {
  MuslimtifyPrayerType prayer; /* MUSLIMTIFY_PRAYER_COUNT when none is upcoming */
  MuslimtifyTime time;         /* carries the calendar day the prayer falls on */
  int minutes_until;
} MuslimtifyNext;

/**
 * Load the user's config into a new handle. Performs no network access.
 * Returns MUSLIMTIFY_ERR_NO_LOCATION when no usable location is configured.
 * On error *out is NULL. Free the handle with muslimtify_close.
 */
MuslimtifyError muslimtify_open(Muslimtify **out);

/** Free a handle. NULL is a no-op. */
void muslimtify_close(Muslimtify *mt);

/**
 * Validate an inclusive date range without a handle. Years are 1-9999 and the
 * span is at most MUSLIMTIFY_MAX_RANGE_DAYS. On success the span is written to
 * *days when days is not NULL. Pass the same date twice to check one date.
 */
MuslimtifyError muslimtify_check_range(int start_year, int start_month, int start_day, int end_year,
                                       int end_month, int end_day, size_t *days);

/** Prayer times for one civil date. */
MuslimtifyError muslimtify_day(const Muslimtify *mt, int year, int month, int day,
                               MuslimtifyDay *out);

/** Prayer times for today on the system clock, shifted by day_offset days. */
MuslimtifyError muslimtify_today(const Muslimtify *mt, long day_offset, MuslimtifyDay *out);

/**
 * Prayer times for an inclusive date range, written to out[0..*count).
 * Returns MUSLIMTIFY_ERR_RANGE_TOO_LONG when the span exceeds cap.
 */
MuslimtifyError muslimtify_range(const Muslimtify *mt, int start_year, int start_month,
                                 int start_day, int end_year, int end_month, int end_day,
                                 MuslimtifyDay *out, size_t cap, size_t *count);

/**
 * The next enabled prayer from now on the system clock. When none is upcoming
 * the call still succeeds and out->prayer is MUSLIMTIFY_PRAYER_COUNT.
 */
MuslimtifyError muslimtify_next(const Muslimtify *mt, MuslimtifyNext *out);

/** Capitalized prayer name such as "Fajr". Static, never NULL. */
const char *muslimtify_prayer_name(MuslimtifyPrayerType type);

/** The configured display format, 12 or 24. A NULL handle gives 24. */
int muslimtify_time_format(const Muslimtify *mt);

/**
 * Write "HH:MM" (time_format other than 12) or "hh:MM AM" / "hh:MM PM"
 * (time_format 12) to out. An invalid or NULL time writes "--:--".
 * MUSLIMTIFY_TIME_STR_SIZE bytes are enough. No day marker is added.
 */
void muslimtify_format_time(const MuslimtifyTime *time, int time_format, char *out, size_t cap);

/** Message for an error code. Static, never NULL. */
const char *muslimtify_get_error(MuslimtifyError err);

#ifdef __cplusplus
}
#endif

#endif // MUSLIMTIFY_H
