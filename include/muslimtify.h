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
  MUSLIMTIFY_ERR_RANGE_TOO_LONG,
  MUSLIMTIFY_ERR_CONFIG_SAVE,
  MUSLIMTIFY_ERR_INVALID_VALUE,
  MUSLIMTIFY_ERR_VALUE_TOO_LONG,
  MUSLIMTIFY_ERR_INVALID_LATITUDE,
  MUSLIMTIFY_ERR_INVALID_LONGITUDE,
  MUSLIMTIFY_ERR_UNKNOWN_TIMEZONE,
  MUSLIMTIFY_ERR_INVALID_COUNTRY,
  MUSLIMTIFY_ERR_INVALID_REFRESH_INTERVAL,
  MUSLIMTIFY_ERR_UNKNOWN_METHOD,
  MUSLIMTIFY_ERR_UNKNOWN_PRAYER,
  MUSLIMTIFY_ERR_INVALID_OFFSET,
  MUSLIMTIFY_ERR_INVALID_REMINDER,
  MUSLIMTIFY_ERR_TOO_MANY_REMINDERS,
  MUSLIMTIFY_ERR_FILE_NOT_FOUND,
  MUSLIMTIFY_ERR_FILE_NOT_REGULAR,
  MUSLIMTIFY_ERR_FILE_IS_SYMLINK,
  MUSLIMTIFY_ERR_FILE_NOT_READABLE,
  MUSLIMTIFY_ERR_FILE_RESOLVE,
  MUSLIMTIFY_ERR_DETECT_FAILED,
  MUSLIMTIFY_ERR_GPS_NO_DAEMON,
  MUSLIMTIFY_ERR_GPS_NO_DEVICE,
  MUSLIMTIFY_ERR_GPS_NO_PERMISSION,
  MUSLIMTIFY_ERR_GPS_UNAVAILABLE,
  MUSLIMTIFY_ERR_NOTIFY_INIT,
  MUSLIMTIFY_ERR_NO_UPCOMING_PRAYER,
  MUSLIMTIFY_ERR_ADHAN_NOT_PLAYING
} MuslimtifyError;

typedef enum {
  MUSLIMTIFY_FAJR,
  MUSLIMTIFY_DHUHR,
  MUSLIMTIFY_ASR,
  MUSLIMTIFY_MAGHRIB,
  MUSLIMTIFY_ISHA,
  MUSLIMTIFY_PRAYER_COUNT
} MuslimtifyPrayerType;

typedef enum { MUSLIMTIFY_MADHAB_SHAFI, MUSLIMTIFY_MADHAB_HANAFI } MuslimtifyMadhab;

typedef enum {
  MUSLIMTIFY_URGENCY_LOW,
  MUSLIMTIFY_URGENCY_NORMAL,
  MUSLIMTIFY_URGENCY_CRITICAL
} MuslimtifyUrgency;

typedef enum {
  MUSLIMTIFY_SOUND_ADHAN,
  MUSLIMTIFY_SOUND_DEFAULT,
  MUSLIMTIFY_SOUND_OFF
} MuslimtifySoundMode;

typedef enum { MUSLIMTIFY_SOURCE_IP, MUSLIMTIFY_SOURCE_GPS } MuslimtifyLocationSource;

#define MUSLIMTIFY_MAX_REMINDERS 10
#define MUSLIMTIFY_MAX_RANGE_DAYS 366
#define MUSLIMTIFY_TIME_STR_SIZE 9
#define MUSLIMTIFY_TIMEZONE_SIZE 64
#define MUSLIMTIFY_CITY_SIZE 128
#define MUSLIMTIFY_COUNTRY_SIZE 64
#define MUSLIMTIFY_PATH_SIZE 512

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

typedef struct {
  bool is_set; /* false when there is no usable location */
  double latitude;
  double longitude;
  char timezone[MUSLIMTIFY_TIMEZONE_SIZE];
  double utc_offset; /* hours in effect today, follows DST */
  char city[MUSLIMTIFY_CITY_SIZE];
  char country[MUSLIMTIFY_COUNTRY_SIZE];
  bool auto_detect;
  bool gps;
  long long refresh_interval; /* seconds, 0 means disabled */
} MuslimtifyLocation;

typedef struct {
  bool enabled;
  bool adhan_enabled;
  int offset; /* minutes added to the calculated time */
  int reminders[MUSLIMTIFY_MAX_REMINDERS];
  int reminder_count;
  char adhan_file[MUSLIMTIFY_PATH_SIZE]; /* "" means the bundled adhan */
} MuslimtifyPrayerSettings;

typedef struct {
  MuslimtifyUrgency urgency;
  MuslimtifySoundMode sound_mode;
  MuslimtifyPrayerSettings prayers[MUSLIMTIFY_PRAYER_COUNT];
} MuslimtifyNotification;

typedef struct {
  const char *key;  /* such as "kemenag" */
  const char *name; /* display name, "" when the method has none */
} MuslimtifyMethodInfo;

/* What muslimtify_detect_location did. */
typedef struct {
  MuslimtifyLocationSource source; /* where the stored coordinates came from */
  MuslimtifyError gps;             /* MUSLIMTIFY_OK, or why GPS was not used */
  bool gps_disabled;               /* true when detection switched GPS off */
} MuslimtifyDetection;

/* What one muslimtify_run_cycle call did. */
typedef struct {
  bool detected;                 /* a first-run detection ran and was saved */
  bool refreshed;                /* a stale location was re-detected and saved */
  bool refresh_failed;           /* a stale refresh failed, the stored location was kept */
  MuslimtifyDetection detection; /* outcome of that detection or refresh, when one ran */
  int notifications;             /* notifications sent in this cycle */
} MuslimtifyCycle;

/**
 * Load the user's config into a new handle. Performs no network access and
 * succeeds even when no location is configured. On error *out is NULL. Free
 * the handle with muslimtify_close.
 */
MuslimtifyError muslimtify_open(Muslimtify **out);

/**
 * Reread the config file into the handle, dropping unsaved changes. On failure
 * the handle keeps its contents. A long-lived frontend calls this before
 * showing a settings screen, because the daemon rewrites the config when it
 * refreshes the location.
 */
MuslimtifyError muslimtify_reload(Muslimtify *mt);

/**
 * Write the handle's settings to the config file and invalidate the daemon's
 * trigger cache. Setters change the handle in memory only until this is called.
 */
MuslimtifyError muslimtify_save(Muslimtify *mt);

/** Free a handle, discarding unsaved changes. NULL is a no-op. */
void muslimtify_close(Muslimtify *mt);

/* The prayer time queries below that take a handle return
   MUSLIMTIFY_ERR_NO_LOCATION while it has no location. */

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

MuslimtifyError muslimtify_get_location(const Muslimtify *mt, MuslimtifyLocation *out);
MuslimtifyError muslimtify_get_notification(const Muslimtify *mt, MuslimtifyNotification *out);

/**
 * The configured calculation method. out->key points into the handle and is
 * valid until the next setter, reload or close. out->name is static.
 */
MuslimtifyError muslimtify_get_method(const Muslimtify *mt, MuslimtifyMethodInfo *out);

/** The configured madhab. A NULL handle or an unknown stored word gives shafi. */
MuslimtifyMadhab muslimtify_get_madhab(const Muslimtify *mt);

/** The configured display format, 12 or 24. A NULL handle gives 24. */
int muslimtify_time_format(const Muslimtify *mt);

/** Path of the config file. Static string, never NULL. */
const char *muslimtify_config_path(void);

/** Number of selectable calculation methods. */
size_t muslimtify_method_count(void);

/** The method at index, with static strings. index must be below the count. */
MuslimtifyError muslimtify_method_at(size_t index, MuslimtifyMethodInfo *out);

/* Every setter below validates before it writes, so a failed call changes
   nothing. Changes stay in memory until muslimtify_save. */

/**
 * Set both coordinates. Moving the location also turns auto-detect off, clears
 * the city and country, and sets the timezone from the system timezone. Call
 * muslimtify_set_city, muslimtify_set_country and muslimtify_set_timezone
 * after this, not before.
 */
MuslimtifyError muslimtify_set_coordinates(Muslimtify *mt, double latitude, double longitude);

/** Set the IANA timezone. It must exist on this system. */
MuslimtifyError muslimtify_set_timezone(Muslimtify *mt, const char *iana_name);

/** Set the city label. A name longer than the field is truncated. */
MuslimtifyError muslimtify_set_city(Muslimtify *mt, const char *city);

/** Set the country as an ISO 3166-1 alpha-2 code. Stored uppercased. */
MuslimtifyError muslimtify_set_country(Muslimtify *mt, const char *iso2);

/** Set the location auto-refresh interval: 0 to disable, otherwise at least 3600. */
MuslimtifyError muslimtify_set_refresh_interval(Muslimtify *mt, long long seconds);

/** Set the calculation method by a key from the method list. */
MuslimtifyError muslimtify_set_method(Muslimtify *mt, const char *key);

MuslimtifyError muslimtify_set_madhab(Muslimtify *mt, MuslimtifyMadhab madhab);

/** Set the display format, 12 or 24. */
MuslimtifyError muslimtify_set_time_format(Muslimtify *mt, int time_format);

MuslimtifyError muslimtify_set_prayer_enabled(Muslimtify *mt, MuslimtifyPrayerType prayer,
                                              bool enabled);

/** Shift one prayer's time by -60 to 60 minutes. */
MuslimtifyError muslimtify_set_prayer_offset(Muslimtify *mt, MuslimtifyPrayerType prayer,
                                             int minutes);

/**
 * Set one prayer's reminders: at most MUSLIMTIFY_MAX_REMINDERS values, each
 * 1 to 1440 minutes before the prayer. A count of 0 clears them, and minutes
 * may then be NULL.
 */
MuslimtifyError muslimtify_set_prayer_reminders(Muslimtify *mt, MuslimtifyPrayerType prayer,
                                                const int *minutes, size_t count);

MuslimtifyError muslimtify_set_prayer_adhan(Muslimtify *mt, MuslimtifyPrayerType prayer,
                                            bool enabled);

/**
 * Set one prayer's adhan audio file. The path must name an existing, readable,
 * regular file that is not a symlink. Its canonical path is stored.
 */
MuslimtifyError muslimtify_set_prayer_adhan_file(Muslimtify *mt, MuslimtifyPrayerType prayer,
                                                 const char *path);

MuslimtifyError muslimtify_set_urgency(Muslimtify *mt, MuslimtifyUrgency urgency);
MuslimtifyError muslimtify_set_sound_mode(Muslimtify *mt, MuslimtifySoundMode mode);

/* Like the setters, the location detection functions below change the handle in
   memory until muslimtify_save. */

/**
 * True when no usable location is stored and auto-detect is on, so a frontend
 * should call muslimtify_detect_location. Does no I/O.
 */
bool muslimtify_location_needs_detect(const Muslimtify *mt);

/**
 * Detect the location: GPS first when it is enabled, then an IP lookup. Blocks
 * on device and network I/O, so a frontend shows its own progress around it.
 * On failure returns MUSLIMTIFY_ERR_DETECT_FAILED and changes nothing. On
 * success auto-detect is on and *out, when not NULL, says where the location
 * came from and what happened with GPS. A GPS problem that cannot fix itself
 * switches GPS off, reported in out->gps_disabled.
 */
MuslimtifyError muslimtify_detect_location(Muslimtify *mt, MuslimtifyDetection *out);

/**
 * A ready-made English warning for a GPS problem met during detection, phrased
 * for the command line tool, or NULL when there is nothing to warn about. A
 * frontend that wants its own wording switches on detection->gps instead.
 * The string is static.
 */
const char *muslimtify_detection_warning(const MuslimtifyDetection *detection);

/**
 * Turn the GPS location source on or off. Turning it on probes the receiver
 * first: a fix stores its coordinates and sets *has_fix, a receiver with no fix
 * yet still enables, and any other outcome returns a MUSLIMTIFY_ERR_GPS_* code
 * and changes nothing. has_fix may be NULL.
 */
MuslimtifyError muslimtify_set_gps(Muslimtify *mt, bool enabled, bool *has_fix);

/**
 * Set the calculation method to the default for the stored country. An empty
 * or unknown country gives the engine's fallback method.
 */
MuslimtifyError muslimtify_set_method_from_country(Muslimtify *mt);

/**
 * Check an ISO 3166-1 alpha-2 country code without a handle. Either case is
 * accepted. Returns MUSLIMTIFY_ERR_INVALID_COUNTRY for anything else.
 */
MuslimtifyError muslimtify_check_country(const char *iso2);

/**
 * Run one notification check for the current minute: load the config, detect
 * the location on a first run, refresh it when it is older than the refresh
 * interval, then send every notification that is due and play the adhan where
 * it is enabled. Call it once a minute. Takes no handle, because it reads the
 * config from disk each time, which is what makes saved settings take effect.
 * Blocks while an adhan plays, so call it off the UI thread.
 *
 * *out, when not NULL, is filled even on an error return. A failed stale
 * refresh is not an error: the cycle continues and sets out->refresh_failed.
 */
MuslimtifyError muslimtify_run_cycle(MuslimtifyCycle *out);

/**
 * Send a notification now for the next prayer, or play its adhan when adhan is
 * true, which blocks while it plays. *sent, when not NULL, receives the prayer
 * and time that were announced. Returns MUSLIMTIFY_ERR_NO_UPCOMING_PRAYER when
 * no enabled prayer is ahead.
 */
MuslimtifyError muslimtify_notify_test(Muslimtify *mt, bool adhan, MuslimtifyNext *sent);

/**
 * Stop an adhan that is playing. Returns MUSLIMTIFY_ERR_ADHAN_NOT_PLAYING when
 * there is none to stop.
 */
MuslimtifyError muslimtify_adhan_stop(void);

/* The names and messages returned below are static strings and never NULL. */

/** Capitalized prayer name such as "Fajr". */
const char *muslimtify_prayer_name(MuslimtifyPrayerType type);

const char *muslimtify_madhab_key(MuslimtifyMadhab madhab);      /* "shafi", "hanafi" */
const char *muslimtify_madhab_name(MuslimtifyMadhab madhab);     /* "Shafi'i", "Hanafi" */
const char *muslimtify_urgency_key(MuslimtifyUrgency urgency);   /* "low", "normal", "critical" */
const char *muslimtify_sound_mode_key(MuslimtifySoundMode mode); /* "adhan", "default", "off" */

/** Case-insensitive, and "dhur" is accepted for dhuhr. MUSLIMTIFY_ERR_UNKNOWN_PRAYER when nothing
 * matches. */
MuslimtifyError muslimtify_parse_prayer(const char *name, MuslimtifyPrayerType *out);

/* Case-sensitive. MUSLIMTIFY_ERR_INVALID_VALUE when nothing matches. */
MuslimtifyError muslimtify_parse_madhab(const char *key, MuslimtifyMadhab *out);
MuslimtifyError muslimtify_parse_urgency(const char *key, MuslimtifyUrgency *out);
MuslimtifyError muslimtify_parse_sound_mode(const char *key, MuslimtifySoundMode *out);

/**
 * Write "HH:MM" (time_format other than 12) or "hh:MM AM" / "hh:MM PM"
 * (time_format 12) to out. An invalid or NULL time writes "--:--".
 * MUSLIMTIFY_TIME_STR_SIZE bytes are enough. No day marker is added.
 */
void muslimtify_format_time(const MuslimtifyTime *time, int time_format, char *out, size_t cap);

/** Message for an error code. */
const char *muslimtify_get_error(MuslimtifyError err);

#ifdef __cplusplus
}
#endif

#endif // MUSLIMTIFY_H
