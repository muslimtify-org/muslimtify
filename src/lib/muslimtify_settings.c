#include "muslimtify.h"

#include "cache.h"
#include "config.h"
#include "country.h"
#include "lib/muslimtify_internal.h"
#include "location.h"
#include "platform.h"
#include "prayer_checker.h"
#include "prayertimes.h"
#include "string_util.h"
#include "util.h"

#include <ctype.h>
#include <string.h>
#include <time.h>

_Static_assert(MUSLIMTIFY_TIMEZONE_SIZE == sizeof(((Config *)0)->timezone), "timezone size");
_Static_assert(MUSLIMTIFY_CITY_SIZE == sizeof(((Config *)0)->city), "city size");
_Static_assert(MUSLIMTIFY_COUNTRY_SIZE == sizeof(((Config *)0)->country), "country size");
_Static_assert(MUSLIMTIFY_PATH_SIZE == MAX_ADHAN_PATH, "adhan path size");

// The words the config file stores, indexed by the public enum values.
static const char *const MADHAB_KEYS[] = {"shafi", "hanafi"};
static const char *const MADHAB_NAMES[] = {"Shafi'i", "Hanafi"};
static const char *const URGENCY_KEYS[] = {"low", "normal", "critical"};
static const char *const SOUND_KEYS[] = {"adhan", "default", "off"};

static const char *key_at(const char *const *keys, size_t count, int value) {
  return (value >= 0 && (size_t)value < count) ? keys[value] : "unknown";
}

static bool key_find(const char *const *keys, size_t count, const char *key, int *out) {
  for (size_t i = 0; i < count; i++) {
    if (strcmp(keys[i], key) == 0) {
      *out = (int)i;
      return true;
    }
  }
  return false;
}

static bool equals_ignore_case(const char *a, const char *b) {
  while (*a && *b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
      return false;
    a++;
    b++;
  }
  return *a == *b;
}

static PrayerConfig *prayer_settings(Muslimtify *mt, MuslimtifyPrayerType prayer) {
  PrayerConfig *all[] = {&mt->cfg.fajr, &mt->cfg.dhuhr, &mt->cfg.asr, &mt->cfg.maghrib,
                         &mt->cfg.isha};
  if ((int)prayer < 0 || (int)prayer >= PRAYER_COUNT)
    return NULL;
  return all[prayer];
}

/* -- Lifecycle ------------------------------------------------------------- */

MuslimtifyError muslimtify_reload(Muslimtify *mt) {
  if (!mt)
    return MUSLIMTIFY_ERR_INVALID_ARG;
  Config cfg;
  if (config_load(&cfg) != 0)
    return MUSLIMTIFY_ERR_CONFIG_LOAD;
  mt->cfg = cfg;
  return MUSLIMTIFY_OK;
}

MuslimtifyError muslimtify_save(Muslimtify *mt) {
  if (!mt)
    return MUSLIMTIFY_ERR_INVALID_ARG;
  if (config_save(&mt->cfg) != 0)
    return MUSLIMTIFY_ERR_CONFIG_SAVE;
  cache_invalidate();
  return MUSLIMTIFY_OK;
}

/* -- Names and parsing ------------------------------------------------------ */

const char *muslimtify_madhab_key(MuslimtifyMadhab madhab) {
  return key_at(MADHAB_KEYS, ARRAY_LEN(MADHAB_KEYS), (int)madhab);
}

const char *muslimtify_madhab_name(MuslimtifyMadhab madhab) {
  return key_at(MADHAB_NAMES, ARRAY_LEN(MADHAB_NAMES), (int)madhab);
}

const char *muslimtify_urgency_key(MuslimtifyUrgency urgency) {
  return key_at(URGENCY_KEYS, ARRAY_LEN(URGENCY_KEYS), (int)urgency);
}

const char *muslimtify_sound_mode_key(MuslimtifySoundMode mode) {
  return key_at(SOUND_KEYS, ARRAY_LEN(SOUND_KEYS), (int)mode);
}

MuslimtifyError muslimtify_parse_prayer(const char *name, MuslimtifyPrayerType *out) {
  if (!name || !out)
    return MUSLIMTIFY_ERR_INVALID_ARG;
  for (int i = 0; i < PRAYER_COUNT; i++) {
    if (equals_ignore_case(name, prayer_get_name((PrayerType)i))) {
      *out = (MuslimtifyPrayerType)i;
      return MUSLIMTIFY_OK;
    }
  }
  // A common shorter spelling, accepted for as long as the CLI has existed.
  if (equals_ignore_case(name, "dhur")) {
    *out = MUSLIMTIFY_DHUHR;
    return MUSLIMTIFY_OK;
  }
  return MUSLIMTIFY_ERR_UNKNOWN_PRAYER;
}

MuslimtifyError muslimtify_parse_madhab(const char *key, MuslimtifyMadhab *out) {
  int value;
  if (!key || !out)
    return MUSLIMTIFY_ERR_INVALID_ARG;
  if (!key_find(MADHAB_KEYS, ARRAY_LEN(MADHAB_KEYS), key, &value))
    return MUSLIMTIFY_ERR_INVALID_VALUE;
  *out = (MuslimtifyMadhab)value;
  return MUSLIMTIFY_OK;
}

MuslimtifyError muslimtify_parse_urgency(const char *key, MuslimtifyUrgency *out) {
  int value;
  if (!key || !out)
    return MUSLIMTIFY_ERR_INVALID_ARG;
  if (!key_find(URGENCY_KEYS, ARRAY_LEN(URGENCY_KEYS), key, &value))
    return MUSLIMTIFY_ERR_INVALID_VALUE;
  *out = (MuslimtifyUrgency)value;
  return MUSLIMTIFY_OK;
}

MuslimtifyError muslimtify_parse_sound_mode(const char *key, MuslimtifySoundMode *out) {
  int value;
  if (!key || !out)
    return MUSLIMTIFY_ERR_INVALID_ARG;
  if (!key_find(SOUND_KEYS, ARRAY_LEN(SOUND_KEYS), key, &value))
    return MUSLIMTIFY_ERR_INVALID_VALUE;
  *out = (MuslimtifySoundMode)value;
  return MUSLIMTIFY_OK;
}

/* -- Getters ---------------------------------------------------------------- */

MuslimtifyError muslimtify_get_location(const Muslimtify *mt, MuslimtifyLocation *out) {
  if (!mt || !out)
    return MUSLIMTIFY_ERR_INVALID_ARG;
  const Config *cfg = &mt->cfg;

  // The offset in effect today, so it tracks DST rather than the value frozen
  // in the config when the location was last set.
  time_t now = time(NULL);
  struct tm lt;
  platform_localtime(&now, &lt);

  MuslimtifyLocation loc;
  memset(&loc, 0, sizeof(loc));
  loc.is_set = muslimtify_has_location(mt);
  loc.latitude = cfg->latitude;
  loc.longitude = cfg->longitude;
  copy_string(loc.timezone, sizeof(loc.timezone), cfg->timezone);
  loc.utc_offset = effective_tz_offset(cfg, lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
  copy_string(loc.city, sizeof(loc.city), cfg->city);
  copy_string(loc.country, sizeof(loc.country), cfg->country);
  loc.auto_detect = cfg->auto_detect;
  loc.gps = cfg->use_gps;
  loc.refresh_interval = (long long)cfg->refresh_interval;
  *out = loc;
  return MUSLIMTIFY_OK;
}

MuslimtifyError muslimtify_get_notification(const Muslimtify *mt, MuslimtifyNotification *out) {
  if (!mt || !out)
    return MUSLIMTIFY_ERR_INVALID_ARG;
  const Config *cfg = &mt->cfg;

  MuslimtifyNotification n;
  memset(&n, 0, sizeof(n));
  int value;
  n.urgency = key_find(URGENCY_KEYS, ARRAY_LEN(URGENCY_KEYS), cfg->notification_urgency, &value)
                  ? (MuslimtifyUrgency)value
                  : MUSLIMTIFY_URGENCY_NORMAL;
  n.sound_mode = key_find(SOUND_KEYS, ARRAY_LEN(SOUND_KEYS), cfg->notification_sound, &value)
                     ? (MuslimtifySoundMode)value
                     : MUSLIMTIFY_SOUND_DEFAULT;

  for (int i = 0; i < PRAYER_COUNT; i++) {
    const PrayerConfig *pcfg = prayer_get_config(cfg, (PrayerType)i);
    MuslimtifyPrayerSettings *p = &n.prayers[i];
    p->enabled = pcfg->enabled;
    p->adhan_enabled = pcfg->adhan_enabled;
    p->offset = pcfg->offset;
    for (int j = 0; j < pcfg->reminder_count && j < MUSLIMTIFY_MAX_REMINDERS; j++)
      p->reminders[p->reminder_count++] = pcfg->reminders[j];
    copy_string(p->adhan_file, sizeof(p->adhan_file), pcfg->adhan);
  }

  *out = n;
  return MUSLIMTIFY_OK;
}

static const char *method_name(CalcMethod method) {
  const MethodParams *params = method_params_get(method);
  return params ? params->name : "";
}

MuslimtifyError muslimtify_get_method(const Muslimtify *mt, MuslimtifyMethodInfo *out) {
  if (!mt || !out)
    return MUSLIMTIFY_ERR_INVALID_ARG;
  out->key = mt->cfg.calculation_method;
  out->name = method_name(method_from_string(mt->cfg.calculation_method));
  return MUSLIMTIFY_OK;
}

size_t muslimtify_method_count(void) {
  return (size_t)CALC_CUSTOM;
}

MuslimtifyError muslimtify_method_at(size_t index, MuslimtifyMethodInfo *out) {
  if (!out || index >= (size_t)CALC_CUSTOM)
    return MUSLIMTIFY_ERR_INVALID_ARG;
  out->key = method_to_string((CalcMethod)index);
  out->name = method_name((CalcMethod)index);
  return MUSLIMTIFY_OK;
}

MuslimtifyMadhab muslimtify_get_madhab(const Muslimtify *mt) {
  int value;
  if (mt && key_find(MADHAB_KEYS, ARRAY_LEN(MADHAB_KEYS), mt->cfg.madhab, &value))
    return (MuslimtifyMadhab)value;
  return MUSLIMTIFY_MADHAB_SHAFI;
}

/* -- Location setters ------------------------------------------------------- */

MuslimtifyError muslimtify_set_coordinates(Muslimtify *mt, double latitude, double longitude) {
  if (!mt)
    return MUSLIMTIFY_ERR_INVALID_ARG;
  if (!config_latitude_is_valid(latitude))
    return MUSLIMTIFY_ERR_INVALID_LATITUDE;
  if (!config_longitude_is_valid(longitude))
    return MUSLIMTIFY_ERR_INVALID_LONGITUDE;

  Config *cfg = &mt->cfg;
  cfg->latitude = latitude;
  cfg->longitude = longitude;
  cfg->auto_detect = false;
  // The labels and the zone described the old place. The zone is re-derived
  // from the host so the offset stays correct until the caller sets one.
  // get_system_timezone leaves "UTC" behind when it cannot read the zone.
  cfg->city[0] = '\0';
  cfg->country[0] = '\0';
  (void)get_system_timezone(cfg->timezone, sizeof(cfg->timezone));
  cfg->timezone_offset = parse_timezone_offset(cfg->timezone, time(NULL));
  return MUSLIMTIFY_OK;
}

MuslimtifyError muslimtify_set_timezone(Muslimtify *mt, const char *iana_name) {
  if (!mt || !iana_name)
    return MUSLIMTIFY_ERR_INVALID_ARG;
  // A zone that does not resolve on this system would silently compute as UTC.
  if (!timezone_exists(iana_name))
    return MUSLIMTIFY_ERR_UNKNOWN_TIMEZONE;
  size_t len = strlen(iana_name);
  if (len + 1 > sizeof(mt->cfg.timezone))
    return MUSLIMTIFY_ERR_VALUE_TOO_LONG;
  memcpy(mt->cfg.timezone, iana_name, len + 1);
  mt->cfg.timezone_offset = parse_timezone_offset(iana_name, time(NULL));
  return MUSLIMTIFY_OK;
}

MuslimtifyError muslimtify_set_city(Muslimtify *mt, const char *city) {
  if (!mt || !city)
    return MUSLIMTIFY_ERR_INVALID_ARG;
  size_t cap = sizeof(mt->cfg.city);
  size_t len = strlen(city);
  if (len >= cap)
    len = cap - 1;
  memcpy(mt->cfg.city, city, len);
  mt->cfg.city[len] = '\0';
  return MUSLIMTIFY_OK;
}

MuslimtifyError muslimtify_set_country(Muslimtify *mt, const char *iso2) {
  if (!mt || !iso2)
    return MUSLIMTIFY_ERR_INVALID_ARG;
  if (!country_is_valid_alpha2(iso2))
    return MUSLIMTIFY_ERR_INVALID_COUNTRY;
  mt->cfg.country[0] = (char)toupper((unsigned char)iso2[0]);
  mt->cfg.country[1] = (char)toupper((unsigned char)iso2[1]);
  mt->cfg.country[2] = '\0';
  return MUSLIMTIFY_OK;
}

MuslimtifyError muslimtify_set_refresh_interval(Muslimtify *mt, long long seconds) {
  if (!mt)
    return MUSLIMTIFY_ERR_INVALID_ARG;
  if (seconds < 0 || (seconds > 0 && seconds < LOCATION_MIN_REFRESH_SECONDS))
    return MUSLIMTIFY_ERR_INVALID_REFRESH_INTERVAL;
  mt->cfg.refresh_interval = (int64_t)seconds;
  return MUSLIMTIFY_OK;
}

/* -- Calculation and display setters ---------------------------------------- */

MuslimtifyError muslimtify_set_method(Muslimtify *mt, const char *key) {
  if (!mt || !key)
    return MUSLIMTIFY_ERR_INVALID_ARG;
  // method_from_string falls back to CALC_CUSTOM for unknown names, and
  // "custom" itself is not selectable: it needs fajr/isha angles from the
  // config file.
  CalcMethod method = method_from_string(key);
  if (method == CALC_CUSTOM || strcmp(key, method_to_string(method)) != 0)
    return MUSLIMTIFY_ERR_UNKNOWN_METHOD;
  copy_string(mt->cfg.calculation_method, sizeof(mt->cfg.calculation_method), key);
  return MUSLIMTIFY_OK;
}

MuslimtifyError muslimtify_set_madhab(Muslimtify *mt, MuslimtifyMadhab madhab) {
  if (!mt)
    return MUSLIMTIFY_ERR_INVALID_ARG;
  if ((int)madhab < 0 || (size_t)madhab >= ARRAY_LEN(MADHAB_KEYS))
    return MUSLIMTIFY_ERR_INVALID_VALUE;
  copy_string(mt->cfg.madhab, sizeof(mt->cfg.madhab), MADHAB_KEYS[madhab]);
  return MUSLIMTIFY_OK;
}

MuslimtifyError muslimtify_set_time_format(Muslimtify *mt, int time_format) {
  if (!mt)
    return MUSLIMTIFY_ERR_INVALID_ARG;
  if (time_format != 12 && time_format != 24)
    return MUSLIMTIFY_ERR_INVALID_VALUE;
  mt->cfg.time_format = time_format;
  return MUSLIMTIFY_OK;
}

/* -- Per-prayer setters ----------------------------------------------------- */

MuslimtifyError muslimtify_set_prayer_enabled(Muslimtify *mt, MuslimtifyPrayerType prayer,
                                              bool enabled) {
  if (!mt)
    return MUSLIMTIFY_ERR_INVALID_ARG;
  PrayerConfig *pcfg = prayer_settings(mt, prayer);
  if (!pcfg)
    return MUSLIMTIFY_ERR_UNKNOWN_PRAYER;
  pcfg->enabled = enabled;
  return MUSLIMTIFY_OK;
}

MuslimtifyError muslimtify_set_prayer_offset(Muslimtify *mt, MuslimtifyPrayerType prayer,
                                             int minutes) {
  if (!mt)
    return MUSLIMTIFY_ERR_INVALID_ARG;
  PrayerConfig *pcfg = prayer_settings(mt, prayer);
  if (!pcfg)
    return MUSLIMTIFY_ERR_UNKNOWN_PRAYER;
  if (minutes < PRAYER_OFFSET_MIN || minutes > PRAYER_OFFSET_MAX)
    return MUSLIMTIFY_ERR_INVALID_OFFSET;
  pcfg->offset = minutes;
  return MUSLIMTIFY_OK;
}

MuslimtifyError muslimtify_set_prayer_reminders(Muslimtify *mt, MuslimtifyPrayerType prayer,
                                                const int *minutes, size_t count) {
  if (!mt || (count > 0 && !minutes))
    return MUSLIMTIFY_ERR_INVALID_ARG;
  PrayerConfig *pcfg = prayer_settings(mt, prayer);
  if (!pcfg)
    return MUSLIMTIFY_ERR_UNKNOWN_PRAYER;
  if (count > MUSLIMTIFY_MAX_REMINDERS)
    return MUSLIMTIFY_ERR_TOO_MANY_REMINDERS;
  for (size_t i = 0; i < count; i++) {
    if (minutes[i] < 1 || minutes[i] > 1440)
      return MUSLIMTIFY_ERR_INVALID_REMINDER;
  }
  for (size_t i = 0; i < count; i++)
    pcfg->reminders[i] = minutes[i];
  pcfg->reminder_count = (int)count;
  return MUSLIMTIFY_OK;
}

MuslimtifyError muslimtify_set_prayer_adhan(Muslimtify *mt, MuslimtifyPrayerType prayer,
                                            bool enabled) {
  if (!mt)
    return MUSLIMTIFY_ERR_INVALID_ARG;
  PrayerConfig *pcfg = prayer_settings(mt, prayer);
  if (!pcfg)
    return MUSLIMTIFY_ERR_UNKNOWN_PRAYER;
  pcfg->adhan_enabled = enabled;
  return MUSLIMTIFY_OK;
}

MuslimtifyError muslimtify_set_prayer_adhan_file(Muslimtify *mt, MuslimtifyPrayerType prayer,
                                                 const char *path) {
  if (!mt || !path)
    return MUSLIMTIFY_ERR_INVALID_ARG;
  PrayerConfig *pcfg = prayer_settings(mt, prayer);
  if (!pcfg)
    return MUSLIMTIFY_ERR_UNKNOWN_PRAYER;

  char resolved[MAX_ADHAN_PATH];
  switch (platform_resolve_regular_file(path, resolved, sizeof(resolved))) {
  case PATH_FILE_OK:
    break;
  case PATH_FILE_NOT_FOUND:
    return MUSLIMTIFY_ERR_FILE_NOT_FOUND;
  case PATH_FILE_NOT_REGULAR:
    return MUSLIMTIFY_ERR_FILE_NOT_REGULAR;
  case PATH_FILE_IS_SYMLINK:
    return MUSLIMTIFY_ERR_FILE_IS_SYMLINK;
  case PATH_FILE_NOT_READABLE:
    return MUSLIMTIFY_ERR_FILE_NOT_READABLE;
  case PATH_FILE_TOO_LONG:
    return MUSLIMTIFY_ERR_VALUE_TOO_LONG;
  default:
    return MUSLIMTIFY_ERR_FILE_RESOLVE;
  }
  copy_string(pcfg->adhan, sizeof(pcfg->adhan), resolved);
  return MUSLIMTIFY_OK;
}

/* -- Notification setters --------------------------------------------------- */

MuslimtifyError muslimtify_set_urgency(Muslimtify *mt, MuslimtifyUrgency urgency) {
  if (!mt)
    return MUSLIMTIFY_ERR_INVALID_ARG;
  if ((int)urgency < 0 || (size_t)urgency >= ARRAY_LEN(URGENCY_KEYS))
    return MUSLIMTIFY_ERR_INVALID_VALUE;
  copy_string(mt->cfg.notification_urgency, sizeof(mt->cfg.notification_urgency),
              URGENCY_KEYS[urgency]);
  return MUSLIMTIFY_OK;
}

MuslimtifyError muslimtify_set_sound_mode(Muslimtify *mt, MuslimtifySoundMode mode) {
  if (!mt)
    return MUSLIMTIFY_ERR_INVALID_ARG;
  if ((int)mode < 0 || (size_t)mode >= ARRAY_LEN(SOUND_KEYS))
    return MUSLIMTIFY_ERR_INVALID_VALUE;
  copy_string(mt->cfg.notification_sound, sizeof(mt->cfg.notification_sound), SOUND_KEYS[mode]);
  return MUSLIMTIFY_OK;
}
