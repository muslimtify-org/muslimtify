#include "muslimtify.h"

#include "config.h"
#include "lib/muslimtify_internal.h"
#include "log.h"
#include "platform.h"
#include "prayer_checker.h"
#include "prayertimes.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

_Static_assert((int)MUSLIMTIFY_PRAYER_COUNT == PRAYER_COUNT, "prayer enums must match");
_Static_assert(MUSLIMTIFY_MAX_REMINDERS == MAX_REMINDERS, "reminder capacity must match");
_Static_assert((int)MUSLIMTIFY_LOG_WARNING == (int)MT_LOG_WARNING &&
                   (int)MUSLIMTIFY_LOG_ERROR == (int)MT_LOG_ERROR,
               "log levels must match");

// Largest day shift muslimtify_today accepts: more days than years 1-9999 span,
// which keeps the day serial far from overflowing.
#define MAX_DAY_SHIFT (9999L * 366L)

const char *muslimtify_get_error(MuslimtifyError err) {
  switch (err) {
  case MUSLIMTIFY_OK:
    return "Success";
  case MUSLIMTIFY_ERR_INVALID_ARG:
    return "Invalid argument";
  case MUSLIMTIFY_ERR_NO_MEMORY:
    return "Out of memory";
  case MUSLIMTIFY_ERR_CONFIG_LOAD:
    return "Failed to load config";
  case MUSLIMTIFY_ERR_NO_LOCATION:
    return "Location is not set";
  case MUSLIMTIFY_ERR_INVALID_DATE:
    return "Invalid date";
  case MUSLIMTIFY_ERR_DATE_ORDER:
    return "End date is before start date";
  case MUSLIMTIFY_ERR_RANGE_TOO_LONG:
    return "Date range too long (maximum 366 days)";
  case MUSLIMTIFY_ERR_CONFIG_SAVE:
    return "Failed to save config";
  case MUSLIMTIFY_ERR_INVALID_VALUE:
    return "Invalid value";
  case MUSLIMTIFY_ERR_VALUE_TOO_LONG:
    return "Value is too long";
  case MUSLIMTIFY_ERR_INVALID_LATITUDE:
    return "Invalid latitude";
  case MUSLIMTIFY_ERR_INVALID_LONGITUDE:
    return "Invalid longitude";
  case MUSLIMTIFY_ERR_UNKNOWN_TIMEZONE:
    return "Unknown timezone";
  case MUSLIMTIFY_ERR_INVALID_COUNTRY:
    return "Invalid country code";
  case MUSLIMTIFY_ERR_INVALID_REFRESH_INTERVAL:
    return "Refresh interval must be 0 or at least 3600 seconds";
  case MUSLIMTIFY_ERR_UNKNOWN_METHOD:
    return "Unknown method";
  case MUSLIMTIFY_ERR_UNKNOWN_PRAYER:
    return "Unknown prayer";
  case MUSLIMTIFY_ERR_INVALID_OFFSET:
    return "Offset must be from -60 to 60 minutes";
  case MUSLIMTIFY_ERR_INVALID_REMINDER:
    return "Reminder minutes must be from 1 to 1440";
  case MUSLIMTIFY_ERR_TOO_MANY_REMINDERS:
    return "At most 10 reminder values are allowed";
  case MUSLIMTIFY_ERR_FILE_NOT_FOUND:
    return "File not found";
  case MUSLIMTIFY_ERR_FILE_NOT_REGULAR:
    return "Not a regular file";
  case MUSLIMTIFY_ERR_FILE_IS_SYMLINK:
    return "File must not be a symlink";
  case MUSLIMTIFY_ERR_FILE_NOT_READABLE:
    return "File is not readable";
  case MUSLIMTIFY_ERR_FILE_RESOLVE:
    return "Cannot resolve file path";
  case MUSLIMTIFY_ERR_DETECT_FAILED:
    return "Failed to detect location";
  case MUSLIMTIFY_ERR_GPS_NO_DAEMON:
    return "Cannot reach gpsd";
  case MUSLIMTIFY_ERR_GPS_NO_DEVICE:
    return "No GPS device detected";
  case MUSLIMTIFY_ERR_GPS_NO_PERMISSION:
    return "Location access is turned off";
  case MUSLIMTIFY_ERR_GPS_UNAVAILABLE:
    return "GPS is not available in this build";
  case MUSLIMTIFY_ERR_NOTIFY_INIT:
    return "Failed to initialize notification system";
  case MUSLIMTIFY_ERR_NO_UPCOMING_PRAYER:
    return "No upcoming prayers enabled";
  case MUSLIMTIFY_ERR_ADHAN_NOT_PLAYING:
    return "No adhan is currently playing";
  case MUSLIMTIFY_ERR_UNSUPPORTED:
    return "Not supported on this platform";
  case MUSLIMTIFY_ERR_DAEMON_BINARY:
    return "Cannot find the muslimtify program to run as the daemon";
  case MUSLIMTIFY_ERR_NO_HOME:
    return "Cannot determine home directory";
  case MUSLIMTIFY_ERR_DAEMON_UNIT:
    return "Cannot write the service file";
  case MUSLIMTIFY_ERR_DAEMON_RELOAD:
    return "systemctl daemon-reload failed";
  case MUSLIMTIFY_ERR_DAEMON_ENABLE:
    return "Failed to enable muslimtify.service";
  case MUSLIMTIFY_ERR_CACHE_SAVE:
    return "Cannot write the trigger cache, so notifications may repeat";
  default:
    return "Unknown error";
  }
}

const char *muslimtify_prayer_name(MuslimtifyPrayerType type) {
  if ((int)type < 0 || (int)type >= PRAYER_COUNT)
    return "Unknown";
  return prayer_get_name((PrayerType)type);
}

int muslimtify_time_format(const Muslimtify *mt) {
  return (mt && mt->cfg.time_format == 12) ? 12 : 24;
}

void muslimtify_format_time(const MuslimtifyTime *time, int time_format, char *out, size_t cap) {
  if (!out || cap == 0)
    return;
  if (!time || !time->valid) {
    snprintf(out, cap, "--:--");
    return;
  }
  clock_format(time->hour, time->minute, time_format, out, cap);
}

static bool date_is_valid(int year, int month, int day) {
  static const int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (year < 1 || year > 9999 || month < 1 || month > 12 || day < 1)
    return false;
  int len = days_in_month[month - 1];
  if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0))
    len = 29;
  return day <= len;
}

MuslimtifyError muslimtify_check_range(int start_year, int start_month, int start_day, int end_year,
                                       int end_month, int end_day, size_t *days) {
  if (!date_is_valid(start_year, start_month, start_day) ||
      !date_is_valid(end_year, end_month, end_day))
    return MUSLIMTIFY_ERR_INVALID_DATE;
  long span = mt_days_from_civil(end_year, end_month, end_day) -
              mt_days_from_civil(start_year, start_month, start_day) + 1;
  if (span < 1)
    return MUSLIMTIFY_ERR_DATE_ORDER;
  if (span > MUSLIMTIFY_MAX_RANGE_DAYS)
    return MUSLIMTIFY_ERR_RANGE_TOO_LONG;
  if (days)
    *days = (size_t)span;
  return MUSLIMTIFY_OK;
}

bool muslimtify_has_location(const Muslimtify *mt) {
  const Config *cfg = &mt->cfg;
  return !config_location_needs_detect(cfg) && config_latitude_is_valid(cfg->latitude) &&
         config_longitude_is_valid(cfg->longitude);
}

MuslimtifyError muslimtify_open_config(const Config *cfg, Muslimtify **out) {
  if (!out)
    return MUSLIMTIFY_ERR_INVALID_ARG;
  *out = NULL;
  if (!cfg)
    return MUSLIMTIFY_ERR_INVALID_ARG;

  Muslimtify *mt = malloc(sizeof(*mt));
  if (!mt)
    return MUSLIMTIFY_ERR_NO_MEMORY;
  mt->cfg = *cfg;
  *out = mt;
  return MUSLIMTIFY_OK;
}

MuslimtifyError muslimtify_open(Muslimtify **out) {
  if (!out)
    return MUSLIMTIFY_ERR_INVALID_ARG;
  *out = NULL;
  Config cfg;
  if (config_load(&cfg) != 0)
    return MUSLIMTIFY_ERR_CONFIG_LOAD;
  return muslimtify_open_config(&cfg, out);
}

void muslimtify_close(Muslimtify *mt) {
  free(mt);
}

// Convert the engine's decimal hours for the date at `day_serial` into the
// public struct. Hour and minute are read back from format_time_hm so they are
// the engine's own rounding. The day offset uses the minute-rounded value, so
// a time that rounds up to 24:00 counts as the next day.
static MuslimtifyTime time_from_hours(double hours, long day_serial, double utc_offset) {
  MuslimtifyTime t = {0};
  if (!isfinite(hours))
    return t;

  char hm[6];
  format_time_hm(hours, hm, sizeof(hm));
  t.hour = (hm[0] - '0') * 10 + (hm[1] - '0');
  t.minute = (hm[3] - '0') * 10 + (hm[4] - '0');

  long total = (long)ceil(hours * 60.0);
  t.day_offset = total < 0 ? -1 : (total >= 24L * 60 ? 1 : 0);

  long serial = day_serial + t.day_offset;
  mt_civil_from_days(serial, &t.year, &t.month, &t.day);
  t.instant = (time_t)((long long)serial * 86400LL + t.hour * 3600LL + t.minute * 60LL -
                       llround(utc_offset * 3600.0));
  t.valid = true;
  return t;
}

static void local_now(struct tm *out) {
  time_t now = time(NULL);
  platform_localtime(&now, out);
}

MuslimtifyError muslimtify_day_at(const Muslimtify *mt, const struct tm *now, int year, int month,
                                  int day, MuslimtifyDay *out) {
  if (!mt || !now || !out)
    return MUSLIMTIFY_ERR_INVALID_ARG;
  MuslimtifyError err = muslimtify_check_range(year, month, day, year, month, day, NULL);
  if (err != MUSLIMTIFY_OK)
    return err;
  if (!muslimtify_has_location(mt))
    return MUSLIMTIFY_ERR_NO_LOCATION;

  const Config *cfg = &mt->cfg;
  struct PrayerTimes times = prayer_times_for_config(cfg, year, month, day);
  long serial = mt_days_from_civil(year, month, day);
  double utc_offset = effective_tz_offset(cfg, year, month, day);

  MuslimtifyDay result = {0};
  result.year = year;
  result.month = month;
  result.day = day;
  result.next = -1;

  for (int i = 0; i < PRAYER_COUNT; i++) {
    PrayerType type = (PrayerType)i;
    const PrayerConfig *pcfg = prayer_get_config(cfg, type);
    MuslimtifyPrayer *p = &result.prayers[i];
    p->time = time_from_hours(prayer_get_time(&times, type), serial, utc_offset);
    p->enabled = pcfg->enabled;
    p->adhan_enabled = pcfg->adhan_enabled;
    p->offset = pcfg->offset;
    for (int j = 0; j < pcfg->reminder_count && j < MUSLIMTIFY_MAX_REMINDERS; j++)
      p->reminders[p->reminder_count++] = pcfg->reminders[j];
  }

  // Only a prayer taken from the day itself. After isha the next prayer is
  // tomorrow's fajr, and marking today's fajr, long passed, would be wrong.
  if (now->tm_year + 1900 == year && now->tm_mon + 1 == month && now->tm_mday == day) {
    NextPrayer next = prayer_get_next(cfg, now, &times);
    if (next.type != PRAYER_NONE && next.day_delta == 0)
      result.next = (int)next.type;
  }

  *out = result;
  return MUSLIMTIFY_OK;
}

MuslimtifyError muslimtify_day(const Muslimtify *mt, int year, int month, int day,
                               MuslimtifyDay *out) {
  struct tm now;
  local_now(&now);
  return muslimtify_day_at(mt, &now, year, month, day, out);
}

MuslimtifyError muslimtify_today(const Muslimtify *mt, long day_offset, MuslimtifyDay *out) {
  if (!mt || !out)
    return MUSLIMTIFY_ERR_INVALID_ARG;
  if (day_offset < -MAX_DAY_SHIFT || day_offset > MAX_DAY_SHIFT)
    return MUSLIMTIFY_ERR_INVALID_DATE;

  struct tm now;
  local_now(&now);
  int year, month, day;
  mt_civil_from_days(mt_days_from_civil(now.tm_year + 1900, now.tm_mon + 1, now.tm_mday) +
                         day_offset,
                     &year, &month, &day);
  return muslimtify_day_at(mt, &now, year, month, day, out);
}

MuslimtifyError muslimtify_range(const Muslimtify *mt, int start_year, int start_month,
                                 int start_day, int end_year, int end_month, int end_day,
                                 MuslimtifyDay *out, size_t cap, size_t *count) {
  if (!mt || !out || !count)
    return MUSLIMTIFY_ERR_INVALID_ARG;
  size_t days = 0;
  MuslimtifyError err = muslimtify_check_range(start_year, start_month, start_day, end_year,
                                               end_month, end_day, &days);
  if (err != MUSLIMTIFY_OK)
    return err;
  if (days > cap)
    return MUSLIMTIFY_ERR_RANGE_TOO_LONG;

  struct tm now;
  local_now(&now);
  long start = mt_days_from_civil(start_year, start_month, start_day);
  for (size_t i = 0; i < days; i++) {
    int year, month, day;
    mt_civil_from_days(start + (long)i, &year, &month, &day);
    err = muslimtify_day_at(mt, &now, year, month, day, &out[i]);
    if (err != MUSLIMTIFY_OK)
      return err;
  }
  *count = days;
  return MUSLIMTIFY_OK;
}

MuslimtifyError muslimtify_next_at(const Muslimtify *mt, const struct tm *now,
                                   MuslimtifyNext *out) {
  if (!mt || !now || !out)
    return MUSLIMTIFY_ERR_INVALID_ARG;
  if (!muslimtify_has_location(mt))
    return MUSLIMTIFY_ERR_NO_LOCATION;

  const Config *cfg = &mt->cfg;
  int year = now->tm_year + 1900;
  int month = now->tm_mon + 1;
  int day = now->tm_mday;
  struct PrayerTimes today = prayer_times_for_config(cfg, year, month, day);
  NextPrayer next = prayer_get_next(cfg, now, &today);

  MuslimtifyNext result = {0};
  result.prayer = MUSLIMTIFY_PRAYER_COUNT;
  if (next.type != PRAYER_NONE) {
    // The day the time was taken from. time_from_hours adds the day the time
    // itself crosses into.
    long serial = mt_days_from_civil(year, month, day) + next.day_delta;
    int sy, sm, sd;
    mt_civil_from_days(serial, &sy, &sm, &sd);
    result.prayer = (MuslimtifyPrayerType)next.type;
    result.time = time_from_hours(next.time, serial, effective_tz_offset(cfg, sy, sm, sd));
    result.minutes_until = next.minutes_until;
  }

  *out = result;
  return MUSLIMTIFY_OK;
}

MuslimtifyError muslimtify_next(const Muslimtify *mt, MuslimtifyNext *out) {
  struct tm now;
  local_now(&now);
  return muslimtify_next_at(mt, &now, out);
}

static MuslimtifyLogHandler log_handler = NULL;
static void *log_user_data = NULL;

// Core calls this with its own level type, and it passes the message on to the
// handler the frontend registered.
static void log_adapter(MtLogLevel level, const char *message, void *user_data) {
  (void)user_data;
  if (log_handler)
    log_handler((MuslimtifyLogLevel)level, message, log_user_data);
}

void muslimtify_set_log_handler(MuslimtifyLogHandler handler, void *user_data) {
  log_handler = handler;
  log_user_data = user_data;
  mt_log_set_handler(handler ? log_adapter : NULL, NULL);
}
