#define _GNU_SOURCE
#include "cache.h"
#include "config.h"
#include "country.h"
#include "lib/muslimtify_internal.h"
#include "location.h"
#include "muslimtify.h"
#include "prayertimes.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int passed = 0;
static int failed = 0;

static char tmpdir[256];

static void check_bool(const char *test, bool cond) {
  if (cond) {
    passed++;
  } else {
    failed++;
    fprintf(stderr, "FAIL [%s]\n", test);
  }
}

// True when every byte of the object still holds the fill value, which is how
// the tests prove a function left its out parameter alone.
static bool all_bytes(const void *p, size_t n, unsigned char v) {
  const unsigned char *b = p;
  for (size_t i = 0; i < n; i++) {
    if (b[i] != v)
      return false;
  }
  return true;
}

static void setup(void) {
  snprintf(tmpdir, sizeof(tmpdir), "/tmp/mt_libtest_XXXXXX");
  if (!mkdtemp(tmpdir)) {
    fprintf(stderr, "FATAL: mkdtemp failed\n");
    exit(1);
  }
  setenv("XDG_CONFIG_HOME", tmpdir, 1);
  setenv("XDG_CACHE_HOME", tmpdir, 1);
}

static void teardown(void) {
  char cmd[1024];
  snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
  if (system(cmd) != 0) { /* best-effort cleanup */
  }
}

// -- Fixtures -----------------------------------------------------------------

typedef struct {
  const char *name;
  double latitude;
  double longitude;
  double offset;
} Site;

// The same high-latitude sites tests/test_display.c uses, all under CALC_MWL.
static const Site SITES[] = {
    {"Reykjavik", 64.1466, -21.9426, 0.0},
    {"Anchorage", 61.2181, -149.9003, -9.0},
    {"Murmansk", 68.9585, 33.0827, 3.0},
    {"Tromso", 69.6492, 18.9553, 1.0},
};
#define SITE_COUNT (sizeof(SITES) / sizeof(SITES[0]))

static Config site_config(const Site *s) {
  Config cfg = config_default();
  cfg.latitude = s->latitude;
  cfg.longitude = s->longitude;
  cfg.timezone_offset = s->offset;
  // Use the stored fixed offset directly rather than a named zone lookup.
  cfg.timezone[0] = '\0';
  snprintf(cfg.calculation_method, sizeof(cfg.calculation_method), "mwl");
  return cfg;
}

static Config zone_config(double lat, double lon, const char *tz, double offset) {
  Config cfg = config_default();
  cfg.latitude = lat;
  cfg.longitude = lon;
  snprintf(cfg.timezone, sizeof(cfg.timezone), "%s", tz);
  cfg.timezone_offset = offset;
  cfg.auto_detect = false;
  return cfg;
}

static Config jakarta_config(void) {
  return zone_config(-6.2088, 106.8456, "Asia/Jakarta", 7.0);
}

// A "now" that matches none of the dates under test, so `next` stays -1.
static struct tm far_now(void) {
  struct tm now = {0};
  now.tm_year = 100;
  now.tm_mday = 1;
  return now;
}

static bool same_location(const MuslimtifyLocation *a, const MuslimtifyLocation *b) {
  return a->is_set == b->is_set && a->latitude == b->latitude && a->longitude == b->longitude &&
         strcmp(a->timezone, b->timezone) == 0 && a->utc_offset == b->utc_offset &&
         strcmp(a->city, b->city) == 0 && strcmp(a->country, b->country) == 0 &&
         a->auto_detect == b->auto_detect && a->gps == b->gps &&
         a->refresh_interval == b->refresh_interval;
}

static bool same_notification(const MuslimtifyNotification *a, const MuslimtifyNotification *b) {
  if (a->urgency != b->urgency || a->sound_mode != b->sound_mode)
    return false;
  for (int i = 0; i < MUSLIMTIFY_PRAYER_COUNT; i++) {
    const MuslimtifyPrayerSettings *x = &a->prayers[i];
    const MuslimtifyPrayerSettings *y = &b->prayers[i];
    if (x->enabled != y->enabled || x->adhan_enabled != y->adhan_enabled ||
        x->offset != y->offset || x->reminder_count != y->reminder_count ||
        strcmp(x->adhan_file, y->adhan_file) != 0)
      return false;
    for (int j = 0; j < x->reminder_count; j++) {
      if (x->reminders[j] != y->reminders[j])
        return false;
    }
  }
  return true;
}

// -- muslimtify_open ----------------------------------------------------------

// Must run first: the config and cache paths are resolved once per process.
// A fresh config is auto_detect at 0,0. The handle opens, the queries report
// that there is no location, and setting one on the handle makes them work.
static void test_open_from_disk(void) {
  printf("  open from disk...\n");

  Muslimtify *mt = NULL;
  check_bool("fresh config opens", muslimtify_open(&mt) == MUSLIMTIFY_OK && mt != NULL);
  if (!mt)
    return;

  MuslimtifyLocation loc;
  check_bool("location reads", muslimtify_get_location(mt, &loc) == MUSLIMTIFY_OK);
  check_bool("fresh config has no location", !loc.is_set);

  MuslimtifyDay day;
  MuslimtifyDay week[2];
  size_t count = 0;
  MuslimtifyNext next;
  check_bool("day without location",
             muslimtify_day(mt, 2026, 3, 10, &day) == MUSLIMTIFY_ERR_NO_LOCATION);
  check_bool("today without location", muslimtify_today(mt, 0, &day) == MUSLIMTIFY_ERR_NO_LOCATION);
  check_bool("range without location", muslimtify_range(mt, 2026, 3, 10, 2026, 3, 11, week, 2,
                                                        &count) == MUSLIMTIFY_ERR_NO_LOCATION);
  check_bool("next without location", muslimtify_next(mt, &next) == MUSLIMTIFY_ERR_NO_LOCATION);

  check_bool("set coordinates", muslimtify_set_coordinates(mt, -6.2088, 106.8456) == MUSLIMTIFY_OK);
  check_bool("location reads again", muslimtify_get_location(mt, &loc) == MUSLIMTIFY_OK);
  check_bool("location is set", loc.is_set);
  check_bool("day with location", muslimtify_day(mt, 2026, 3, 10, &day) == MUSLIMTIFY_OK);
  check_bool("today with location", muslimtify_today(mt, 0, &day) == MUSLIMTIFY_OK);
  check_bool("range with location",
             muslimtify_range(mt, 2026, 3, 10, 2026, 3, 11, week, 2, &count) == MUSLIMTIFY_OK);
  check_bool("next with location", muslimtify_next(mt, &next) == MUSLIMTIFY_OK);
  muslimtify_close(mt);

  check_bool("open rejects NULL out", muslimtify_open(NULL) == MUSLIMTIFY_ERR_INVALID_ARG);
  muslimtify_close(NULL);
}

// Setters change memory only until muslimtify_save, and muslimtify_reload
// drops what was not saved.
static void test_save_reload(void) {
  printf("  save and reload...\n");

  Muslimtify *a = NULL;
  Muslimtify *b = NULL;
  check_bool("open a", muslimtify_open(&a) == MUSLIMTIFY_OK);
  if (!a)
    return;
  check_bool("starts at 24", muslimtify_time_format(a) == 24);
  check_bool("set 12", muslimtify_set_time_format(a, 12) == MUSLIMTIFY_OK);
  check_bool("handle sees its own change", muslimtify_time_format(a) == 12);

  check_bool("open b before save", muslimtify_open(&b) == MUSLIMTIFY_OK);
  check_bool("unsaved change is not on disk", muslimtify_time_format(b) == 24);
  muslimtify_close(b);

  check_bool("save", muslimtify_save(a) == MUSLIMTIFY_OK);
  check_bool("open b after save", muslimtify_open(&b) == MUSLIMTIFY_OK);
  check_bool("saved change is on disk", muslimtify_time_format(b) == 12);
  muslimtify_close(b);

  check_bool("set 24 without saving", muslimtify_set_time_format(a, 24) == MUSLIMTIFY_OK);
  check_bool("reload", muslimtify_reload(a) == MUSLIMTIFY_OK);
  check_bool("reload drops the unsaved change", muslimtify_time_format(a) == 12);

  check_bool("restore 24", muslimtify_set_time_format(a, 24) == MUSLIMTIFY_OK &&
                               muslimtify_save(a) == MUSLIMTIFY_OK);
  muslimtify_close(a);

  check_bool("reload NULL", muslimtify_reload(NULL) == MUSLIMTIFY_ERR_INVALID_ARG);
  check_bool("save NULL", muslimtify_save(NULL) == MUSLIMTIFY_ERR_INVALID_ARG);
}

static void test_save_invalidates_cache(void) {
  printf("  save invalidates the trigger cache...\n");

  static PrayerCache cache;
  memset(&cache, 0, sizeof(cache));
  snprintf(cache.date, sizeof(cache.date), "2026-01-01");
  check_bool("cache written", cache_save(&cache) == 0);
  FILE *f = fopen(cache_get_path(), "r");
  check_bool("cache file exists", f != NULL);
  if (f)
    fclose(f);

  Muslimtify *mt = NULL;
  check_bool("open", muslimtify_open(&mt) == MUSLIMTIFY_OK);
  check_bool("save", muslimtify_save(mt) == MUSLIMTIFY_OK);
  muslimtify_close(mt);

  f = fopen(cache_get_path(), "r");
  check_bool("cache file is gone", f == NULL);
  if (f)
    fclose(f);
}

static void test_open_config(void) {
  printf("  open from memory...\n");

  Config cfg = jakarta_config();
  Muslimtify *mt = NULL;
  check_bool("NULL config", muslimtify_open_config(NULL, &mt) == MUSLIMTIFY_ERR_INVALID_ARG);
  check_bool("NULL out", muslimtify_open_config(&cfg, NULL) == MUSLIMTIFY_ERR_INVALID_ARG);

  // An unusable coordinate still opens. The queries are what refuse it.
  Config bad = jakarta_config();
  bad.latitude = NAN;
  struct tm now = far_now();
  MuslimtifyDay day;
  MuslimtifyLocation loc;
  check_bool("NaN latitude opens", muslimtify_open_config(&bad, &mt) == MUSLIMTIFY_OK);
  check_bool("NaN latitude has no location",
             muslimtify_day_at(mt, &now, 2026, 3, 10, &day) == MUSLIMTIFY_ERR_NO_LOCATION);
  check_bool("NaN latitude is not set",
             muslimtify_get_location(mt, &loc) == MUSLIMTIFY_OK && !loc.is_set);
  muslimtify_close(mt);

  check_bool("24 by default", muslimtify_open_config(&cfg, &mt) == MUSLIMTIFY_OK &&
                                  muslimtify_time_format(mt) == 24);
  muslimtify_close(mt);
  cfg.time_format = 12;
  check_bool("12 when configured", muslimtify_open_config(&cfg, &mt) == MUSLIMTIFY_OK &&
                                       muslimtify_time_format(mt) == 12);
  muslimtify_close(mt);
  check_bool("NULL handle is 24", muslimtify_time_format(NULL) == 24);
}

// -- Settings -----------------------------------------------------------------

static void test_location_setters(void) {
  printf("  location setters...\n");

  Config cfg = jakarta_config();
  cfg.auto_detect = true;
  snprintf(cfg.city, sizeof(cfg.city), "Jakarta");
  snprintf(cfg.country, sizeof(cfg.country), "ID");
  Muslimtify *mt = NULL;
  check_bool("opens", muslimtify_open_config(&cfg, &mt) == MUSLIMTIFY_OK);
  if (!mt)
    return;

  MuslimtifyLocation before;
  MuslimtifyLocation loc;
  check_bool("read before", muslimtify_get_location(mt, &before) == MUSLIMTIFY_OK);
  check_bool("city and country read back",
             strcmp(before.city, "Jakarta") == 0 && strcmp(before.country, "ID") == 0);

  check_bool("latitude over 90",
             muslimtify_set_coordinates(mt, 91.0, 0.0) == MUSLIMTIFY_ERR_INVALID_LATITUDE);
  check_bool("latitude NaN",
             muslimtify_set_coordinates(mt, NAN, 0.0) == MUSLIMTIFY_ERR_INVALID_LATITUDE);
  check_bool("longitude over 180",
             muslimtify_set_coordinates(mt, 0.0, 181.0) == MUSLIMTIFY_ERR_INVALID_LONGITUDE);
  check_bool("longitude NaN",
             muslimtify_set_coordinates(mt, 0.0, NAN) == MUSLIMTIFY_ERR_INVALID_LONGITUDE);
  check_bool("unknown timezone",
             muslimtify_set_timezone(mt, "No/Such_Zone") == MUSLIMTIFY_ERR_UNKNOWN_TIMEZONE);
  check_bool("NULL timezone", muslimtify_set_timezone(mt, NULL) == MUSLIMTIFY_ERR_INVALID_ARG);
  check_bool("three letter country",
             muslimtify_set_country(mt, "ZZZ") == MUSLIMTIFY_ERR_INVALID_COUNTRY);
  check_bool("one character country",
             muslimtify_set_country(mt, "1") == MUSLIMTIFY_ERR_INVALID_COUNTRY);
  check_bool("NULL country", muslimtify_set_country(mt, NULL) == MUSLIMTIFY_ERR_INVALID_ARG);
  check_bool("refresh below the minimum",
             muslimtify_set_refresh_interval(mt, 10) == MUSLIMTIFY_ERR_INVALID_REFRESH_INTERVAL);
  check_bool("negative refresh",
             muslimtify_set_refresh_interval(mt, -1) == MUSLIMTIFY_ERR_INVALID_REFRESH_INTERVAL);
  check_bool("NULL city", muslimtify_set_city(mt, NULL) == MUSLIMTIFY_ERR_INVALID_ARG);
  check_bool("read after rejections", muslimtify_get_location(mt, &loc) == MUSLIMTIFY_OK);
  check_bool("rejections changed nothing", same_location(&before, &loc));

  // Moving the coordinates makes the labels and the zone stale.
  check_bool("set coordinates", muslimtify_set_coordinates(mt, -6.9, 107.6) == MUSLIMTIFY_OK);
  check_bool("read moved", muslimtify_get_location(mt, &loc) == MUSLIMTIFY_OK);
  check_bool("coordinates stored", loc.latitude == -6.9 && loc.longitude == 107.6);
  check_bool("labels cleared", loc.city[0] == '\0' && loc.country[0] == '\0');
  check_bool("auto detect off", !loc.auto_detect);
  check_bool("a timezone is set", loc.timezone[0] != '\0');

  check_bool("set city", muslimtify_set_city(mt, "Bandung") == MUSLIMTIFY_OK);
  check_bool("set country lowercase", muslimtify_set_country(mt, "id") == MUSLIMTIFY_OK);
  check_bool("refresh disabled", muslimtify_set_refresh_interval(mt, 0) == MUSLIMTIFY_OK);
  check_bool("read labels", muslimtify_get_location(mt, &loc) == MUSLIMTIFY_OK);
  check_bool("city stored", strcmp(loc.city, "Bandung") == 0);
  check_bool("country uppercased", strcmp(loc.country, "ID") == 0);
  check_bool("refresh is 0", loc.refresh_interval == 0);
  check_bool("refresh 7200", muslimtify_set_refresh_interval(mt, 7200) == MUSLIMTIFY_OK &&
                                 muslimtify_get_location(mt, &loc) == MUSLIMTIFY_OK &&
                                 loc.refresh_interval == 7200);

  char long_city[200];
  memset(long_city, 'a', sizeof(long_city) - 1);
  long_city[sizeof(long_city) - 1] = '\0';
  check_bool("long city is accepted", muslimtify_set_city(mt, long_city) == MUSLIMTIFY_OK);
  check_bool("long city is truncated", muslimtify_get_location(mt, &loc) == MUSLIMTIFY_OK &&
                                           strlen(loc.city) == MUSLIMTIFY_CITY_SIZE - 1);

  // Central Indonesia Time is UTC+8 all year.
  if (timezone_exists("Asia/Makassar")) {
    check_bool("set timezone", muslimtify_set_timezone(mt, "Asia/Makassar") == MUSLIMTIFY_OK);
    check_bool("read timezone", muslimtify_get_location(mt, &loc) == MUSLIMTIFY_OK);
    check_bool("timezone stored", strcmp(loc.timezone, "Asia/Makassar") == 0);
    check_bool("offset follows the zone", loc.utc_offset == 8.0);
  }
  muslimtify_close(mt);

  check_bool("coordinates NULL handle",
             muslimtify_set_coordinates(NULL, 0.0, 0.0) == MUSLIMTIFY_ERR_INVALID_ARG);
  check_bool("location NULL handle",
             muslimtify_get_location(NULL, &loc) == MUSLIMTIFY_ERR_INVALID_ARG);
}

static void test_calculation_setters(void) {
  printf("  method, madhab and time format...\n");

  Config cfg = jakarta_config();
  Muslimtify *mt = NULL;
  check_bool("opens", muslimtify_open_config(&cfg, &mt) == MUSLIMTIFY_OK);
  if (!mt)
    return;

  size_t count = muslimtify_method_count();
  check_bool("there is a method", count >= 1);
  MuslimtifyMethodInfo info;
  MuslimtifyMethodInfo current;
  char last_key[64] = "";
  for (size_t i = 0; i < count; i++) {
    check_bool("method at index", muslimtify_method_at(i, &info) == MUSLIMTIFY_OK);
    check_bool("method has a key", info.key != NULL && info.key[0] != '\0');
    check_bool("method has a name", info.name != NULL);
    check_bool("listed method is accepted", muslimtify_set_method(mt, info.key) == MUSLIMTIFY_OK);
    check_bool("method reads back", muslimtify_get_method(mt, &current) == MUSLIMTIFY_OK &&
                                        strcmp(current.key, info.key) == 0 &&
                                        strcmp(current.name, info.name) == 0);
    snprintf(last_key, sizeof(last_key), "%s", info.key);
  }
  check_bool("index at the count",
             muslimtify_method_at(count, &info) == MUSLIMTIFY_ERR_INVALID_ARG);
  check_bool("method_at NULL out", muslimtify_method_at(0, NULL) == MUSLIMTIFY_ERR_INVALID_ARG);

  check_bool("custom is not selectable",
             muslimtify_set_method(mt, "custom") == MUSLIMTIFY_ERR_UNKNOWN_METHOD);
  check_bool("unknown method",
             muslimtify_set_method(mt, "nosuch") == MUSLIMTIFY_ERR_UNKNOWN_METHOD);
  check_bool("NULL method", muslimtify_set_method(mt, NULL) == MUSLIMTIFY_ERR_INVALID_ARG);
  check_bool("rejected method changed nothing",
             muslimtify_get_method(mt, &current) == MUSLIMTIFY_OK &&
                 strcmp(current.key, last_key) == 0);

  check_bool("set hanafi", muslimtify_set_madhab(mt, MUSLIMTIFY_MADHAB_HANAFI) == MUSLIMTIFY_OK);
  check_bool("hanafi reads back", muslimtify_get_madhab(mt) == MUSLIMTIFY_MADHAB_HANAFI);
  check_bool("madhab out of range",
             muslimtify_set_madhab(mt, (MuslimtifyMadhab)7) == MUSLIMTIFY_ERR_INVALID_VALUE);
  check_bool("rejected madhab changed nothing",
             muslimtify_get_madhab(mt) == MUSLIMTIFY_MADHAB_HANAFI);
  check_bool("set shafi", muslimtify_set_madhab(mt, MUSLIMTIFY_MADHAB_SHAFI) == MUSLIMTIFY_OK &&
                              muslimtify_get_madhab(mt) == MUSLIMTIFY_MADHAB_SHAFI);
  check_bool("NULL handle is shafi", muslimtify_get_madhab(NULL) == MUSLIMTIFY_MADHAB_SHAFI);

  check_bool("set 12", muslimtify_set_time_format(mt, 12) == MUSLIMTIFY_OK &&
                           muslimtify_time_format(mt) == 12);
  check_bool("time format 13", muslimtify_set_time_format(mt, 13) == MUSLIMTIFY_ERR_INVALID_VALUE);
  check_bool("rejected time format changed nothing", muslimtify_time_format(mt) == 12);
  muslimtify_close(mt);
}

static void test_notification_setters(void) {
  printf("  prayer and notification setters...\n");

  Config cfg = jakarta_config();
  Muslimtify *mt = NULL;
  check_bool("opens", muslimtify_open_config(&cfg, &mt) == MUSLIMTIFY_OK);
  if (!mt)
    return;

  MuslimtifyNotification before;
  MuslimtifyNotification n;
  check_bool("read before", muslimtify_get_notification(mt, &before) == MUSLIMTIFY_OK);

  const int eleven[11] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
  const int zero[1] = {0};
  const int too_far[1] = {1441};
  check_bool("unknown prayer", muslimtify_set_prayer_enabled(mt, MUSLIMTIFY_PRAYER_COUNT, true) ==
                                   MUSLIMTIFY_ERR_UNKNOWN_PRAYER);
  check_bool("offset 61", muslimtify_set_prayer_offset(mt, MUSLIMTIFY_FAJR, 61) ==
                              MUSLIMTIFY_ERR_INVALID_OFFSET);
  check_bool("offset -61", muslimtify_set_prayer_offset(mt, MUSLIMTIFY_FAJR, -61) ==
                               MUSLIMTIFY_ERR_INVALID_OFFSET);
  check_bool("eleven reminders", muslimtify_set_prayer_reminders(mt, MUSLIMTIFY_FAJR, eleven, 11) ==
                                     MUSLIMTIFY_ERR_TOO_MANY_REMINDERS);
  check_bool("reminder of 0", muslimtify_set_prayer_reminders(mt, MUSLIMTIFY_FAJR, zero, 1) ==
                                  MUSLIMTIFY_ERR_INVALID_REMINDER);
  check_bool("reminder of 1441", muslimtify_set_prayer_reminders(mt, MUSLIMTIFY_FAJR, too_far, 1) ==
                                     MUSLIMTIFY_ERR_INVALID_REMINDER);
  check_bool("reminders NULL with a count",
             muslimtify_set_prayer_reminders(mt, MUSLIMTIFY_FAJR, NULL, 2) ==
                 MUSLIMTIFY_ERR_INVALID_ARG);
  check_bool("adhan file NULL", muslimtify_set_prayer_adhan_file(mt, MUSLIMTIFY_FAJR, NULL) ==
                                    MUSLIMTIFY_ERR_INVALID_ARG);
  check_bool("urgency out of range",
             muslimtify_set_urgency(mt, (MuslimtifyUrgency)7) == MUSLIMTIFY_ERR_INVALID_VALUE);
  check_bool("sound out of range",
             muslimtify_set_sound_mode(mt, (MuslimtifySoundMode)7) == MUSLIMTIFY_ERR_INVALID_VALUE);
  check_bool("read after rejections", muslimtify_get_notification(mt, &n) == MUSLIMTIFY_OK);
  check_bool("rejections changed nothing", same_notification(&before, &n));

  const int three[3] = {30, 15, 5};
  check_bool("disable asr",
             muslimtify_set_prayer_enabled(mt, MUSLIMTIFY_ASR, false) == MUSLIMTIFY_OK);
  check_bool("offset -60", muslimtify_set_prayer_offset(mt, MUSLIMTIFY_FAJR, -60) == MUSLIMTIFY_OK);
  check_bool("offset 60", muslimtify_set_prayer_offset(mt, MUSLIMTIFY_DHUHR, 60) == MUSLIMTIFY_OK);
  check_bool("three reminders",
             muslimtify_set_prayer_reminders(mt, MUSLIMTIFY_FAJR, three, 3) == MUSLIMTIFY_OK);
  check_bool("clear reminders",
             muslimtify_set_prayer_reminders(mt, MUSLIMTIFY_ISHA, NULL, 0) == MUSLIMTIFY_OK);
  check_bool("adhan on",
             muslimtify_set_prayer_adhan(mt, MUSLIMTIFY_MAGHRIB, true) == MUSLIMTIFY_OK);
  check_bool("urgency critical",
             muslimtify_set_urgency(mt, MUSLIMTIFY_URGENCY_CRITICAL) == MUSLIMTIFY_OK);
  check_bool("sound adhan", muslimtify_set_sound_mode(mt, MUSLIMTIFY_SOUND_ADHAN) == MUSLIMTIFY_OK);

  check_bool("read accepted", muslimtify_get_notification(mt, &n) == MUSLIMTIFY_OK);
  check_bool("asr disabled", !n.prayers[MUSLIMTIFY_ASR].enabled);
  check_bool("fajr offset", n.prayers[MUSLIMTIFY_FAJR].offset == -60);
  check_bool("dhuhr offset", n.prayers[MUSLIMTIFY_DHUHR].offset == 60);
  check_bool("fajr reminders", n.prayers[MUSLIMTIFY_FAJR].reminder_count == 3 &&
                                   n.prayers[MUSLIMTIFY_FAJR].reminders[0] == 30 &&
                                   n.prayers[MUSLIMTIFY_FAJR].reminders[2] == 5);
  check_bool("isha reminders cleared", n.prayers[MUSLIMTIFY_ISHA].reminder_count == 0);
  check_bool("maghrib adhan", n.prayers[MUSLIMTIFY_MAGHRIB].adhan_enabled);
  check_bool("urgency read back", n.urgency == MUSLIMTIFY_URGENCY_CRITICAL);
  check_bool("sound read back", n.sound_mode == MUSLIMTIFY_SOUND_ADHAN);

  check_bool("notification NULL handle",
             muslimtify_get_notification(NULL, &n) == MUSLIMTIFY_ERR_INVALID_ARG);
  muslimtify_close(mt);
}

static void test_adhan_file(void) {
  printf("  adhan file...\n");

  char file[512];
  char link[512];
  char missing[512];
  snprintf(file, sizeof(file), "%s/adhan.mp3", tmpdir);
  snprintf(link, sizeof(link), "%s/adhan-link.mp3", tmpdir);
  snprintf(missing, sizeof(missing), "%s/no-such-file.mp3", tmpdir);
  FILE *f = fopen(file, "w");
  check_bool("temp file created", f != NULL);
  if (!f)
    return;
  fputs("x", f);
  fclose(f);
  check_bool("symlink created", symlink(file, link) == 0);

  Config cfg = jakarta_config();
  Muslimtify *mt = NULL;
  MuslimtifyNotification n;
  check_bool("opens", muslimtify_open_config(&cfg, &mt) == MUSLIMTIFY_OK);
  if (!mt)
    return;

  check_bool("regular file accepted",
             muslimtify_set_prayer_adhan_file(mt, MUSLIMTIFY_FAJR, file) == MUSLIMTIFY_OK);
  check_bool("read", muslimtify_get_notification(mt, &n) == MUSLIMTIFY_OK);
  check_bool("a path is stored",
             strstr(n.prayers[MUSLIMTIFY_FAJR].adhan_file, "adhan.mp3") != NULL);

  check_bool("missing file", muslimtify_set_prayer_adhan_file(mt, MUSLIMTIFY_FAJR, missing) ==
                                 MUSLIMTIFY_ERR_FILE_NOT_FOUND);
  check_bool("symlink", muslimtify_set_prayer_adhan_file(mt, MUSLIMTIFY_FAJR, link) ==
                            MUSLIMTIFY_ERR_FILE_IS_SYMLINK);
  check_bool("directory is refused",
             muslimtify_set_prayer_adhan_file(mt, MUSLIMTIFY_FAJR, tmpdir) != MUSLIMTIFY_OK);

  MuslimtifyNotification after;
  check_bool("read after rejections", muslimtify_get_notification(mt, &after) == MUSLIMTIFY_OK);
  check_bool("rejections kept the stored path", same_notification(&n, &after));
  muslimtify_close(mt);
}

static void test_names_and_parsing(void) {
  printf("  names and parsing...\n");

  for (int m = MUSLIMTIFY_MADHAB_SHAFI; m <= MUSLIMTIFY_MADHAB_HANAFI; m++) {
    MuslimtifyMadhab out = (MuslimtifyMadhab)99;
    check_bool("madhab key round trips",
               muslimtify_parse_madhab(muslimtify_madhab_key((MuslimtifyMadhab)m), &out) ==
                       MUSLIMTIFY_OK &&
                   (int)out == m);
    check_bool("madhab has a name", muslimtify_madhab_name((MuslimtifyMadhab)m)[0] != '\0');
  }
  for (int u = MUSLIMTIFY_URGENCY_LOW; u <= MUSLIMTIFY_URGENCY_CRITICAL; u++) {
    MuslimtifyUrgency out = (MuslimtifyUrgency)99;
    check_bool("urgency key round trips",
               muslimtify_parse_urgency(muslimtify_urgency_key((MuslimtifyUrgency)u), &out) ==
                       MUSLIMTIFY_OK &&
                   (int)out == u);
  }
  for (int s = MUSLIMTIFY_SOUND_ADHAN; s <= MUSLIMTIFY_SOUND_OFF; s++) {
    MuslimtifySoundMode out = (MuslimtifySoundMode)99;
    check_bool("sound key round trips",
               muslimtify_parse_sound_mode(muslimtify_sound_mode_key((MuslimtifySoundMode)s),
                                           &out) == MUSLIMTIFY_OK &&
                   (int)out == s);
  }
  for (int p = 0; p < MUSLIMTIFY_PRAYER_COUNT; p++) {
    MuslimtifyPrayerType out = MUSLIMTIFY_PRAYER_COUNT;
    check_bool("prayer name round trips",
               muslimtify_parse_prayer(muslimtify_prayer_name((MuslimtifyPrayerType)p), &out) ==
                       MUSLIMTIFY_OK &&
                   (int)out == p);
  }

  MuslimtifyPrayerType prayer = MUSLIMTIFY_PRAYER_COUNT;
  check_bool("lowercase prayer", muslimtify_parse_prayer("fajr", &prayer) == MUSLIMTIFY_OK &&
                                     prayer == MUSLIMTIFY_FAJR);
  check_bool("uppercase prayer", muslimtify_parse_prayer("ISHA", &prayer) == MUSLIMTIFY_OK &&
                                     prayer == MUSLIMTIFY_ISHA);
  check_bool("dhur alias", muslimtify_parse_prayer("dhur", &prayer) == MUSLIMTIFY_OK &&
                               prayer == MUSLIMTIFY_DHUHR);
  check_bool("dhur alias uppercase", muslimtify_parse_prayer("DHUR", &prayer) == MUSLIMTIFY_OK &&
                                         prayer == MUSLIMTIFY_DHUHR);
  prayer = MUSLIMTIFY_ASR;
  check_bool("unknown prayer",
             muslimtify_parse_prayer("nosuch", &prayer) == MUSLIMTIFY_ERR_UNKNOWN_PRAYER);
  check_bool("unknown prayer leaves out alone", prayer == MUSLIMTIFY_ASR);
  check_bool("NULL prayer name",
             muslimtify_parse_prayer(NULL, &prayer) == MUSLIMTIFY_ERR_INVALID_ARG);

  MuslimtifyMadhab madhab = MUSLIMTIFY_MADHAB_HANAFI;
  MuslimtifyUrgency urgency = MUSLIMTIFY_URGENCY_LOW;
  MuslimtifySoundMode sound = MUSLIMTIFY_SOUND_OFF;
  check_bool("unknown madhab",
             muslimtify_parse_madhab("nosuch", &madhab) == MUSLIMTIFY_ERR_INVALID_VALUE &&
                 madhab == MUSLIMTIFY_MADHAB_HANAFI);
  check_bool("unknown urgency",
             muslimtify_parse_urgency("loud", &urgency) == MUSLIMTIFY_ERR_INVALID_VALUE &&
                 urgency == MUSLIMTIFY_URGENCY_LOW);
  check_bool("unknown sound",
             muslimtify_parse_sound_mode("loud", &sound) == MUSLIMTIFY_ERR_INVALID_VALUE &&
                 sound == MUSLIMTIFY_SOUND_OFF);
  check_bool("NULL madhab key",
             muslimtify_parse_madhab(NULL, &madhab) == MUSLIMTIFY_ERR_INVALID_ARG);

  check_bool("out of range madhab key",
             strcmp(muslimtify_madhab_key((MuslimtifyMadhab)7), "unknown") == 0);
  check_bool("out of range urgency key",
             strcmp(muslimtify_urgency_key((MuslimtifyUrgency)7), "unknown") == 0);
  check_bool("out of range sound key",
             strcmp(muslimtify_sound_mode_key((MuslimtifySoundMode)7), "unknown") == 0);
}

// A config edited by hand to hold a word the library does not know reads back
// as the default, which is what the daemon does with it.
static void test_unknown_words(void) {
  printf("  unknown words in the config...\n");

  Config cfg = jakarta_config();
  snprintf(cfg.notification_urgency, sizeof(cfg.notification_urgency), "loud");
  snprintf(cfg.notification_sound, sizeof(cfg.notification_sound), "loud");
  snprintf(cfg.madhab, sizeof(cfg.madhab), "nosuch");
  Muslimtify *mt = NULL;
  MuslimtifyNotification n;
  check_bool("opens", muslimtify_open_config(&cfg, &mt) == MUSLIMTIFY_OK);
  if (!mt)
    return;
  check_bool("read", muslimtify_get_notification(mt, &n) == MUSLIMTIFY_OK);
  check_bool("unknown urgency is normal", n.urgency == MUSLIMTIFY_URGENCY_NORMAL);
  check_bool("unknown sound is default", n.sound_mode == MUSLIMTIFY_SOUND_DEFAULT);
  check_bool("unknown madhab is shafi", muslimtify_get_madhab(mt) == MUSLIMTIFY_MADHAB_SHAFI);
  muslimtify_close(mt);
}

// -- Location detection -------------------------------------------------------

// Stand-ins for the receiver and the network. The GPS stub returns whatever
// status the test sets, and the detect stub runs the real fallback rule in
// location_fetch_core over the two of them.
static GpsStatus stub_gps_status;
static int stub_gps_calls;

static GpsStatus stub_gps(Config *cfg) {
  stub_gps_calls++;
  if (stub_gps_status == GPS_OK) {
    cfg->latitude = 1.5;
    cfg->longitude = 2.5;
  }
  return stub_gps_status;
}

static int stub_ipinfo(Config *cfg) {
  cfg->latitude = -7.25;
  cfg->longitude = 112.75;
  snprintf(cfg->timezone, sizeof(cfg->timezone), "Asia/Jakarta");
  snprintf(cfg->city, sizeof(cfg->city), "Surabaya");
  snprintf(cfg->country, sizeof(cfg->country), "ID");
  return 0;
}

static int stub_detect(Config *cfg, GpsStatus *status) {
  return location_fetch_core(cfg, stub_gps, stub_ipinfo, status);
}

// Scribbles on the config before failing, as a lookup that died half way would.
static int stub_detect_fail(Config *cfg, GpsStatus *status) {
  cfg->latitude = 55.0;
  cfg->use_gps = false;
  snprintf(cfg->city, sizeof(cfg->city), "Nowhere");
  *status = GPS_NO_DAEMON;
  return -1;
}

static void test_needs_detect(void) {
  printf("  needs detect...\n");

  Config fresh = config_default();
  Muslimtify *mt = NULL;
  check_bool("opens", muslimtify_open_config(&fresh, &mt) == MUSLIMTIFY_OK);
  if (!mt)
    return;
  check_bool("fresh config needs detection", muslimtify_location_needs_detect(mt));
  check_bool("set coordinates", muslimtify_set_coordinates(mt, -6.2, 106.8) == MUSLIMTIFY_OK);
  check_bool("a set location does not", !muslimtify_location_needs_detect(mt));
  muslimtify_close(mt);

  // Auto-detect off at 0,0 is a location the user chose.
  Config manual = config_default();
  manual.auto_detect = false;
  check_bool("opens manual", muslimtify_open_config(&manual, &mt) == MUSLIMTIFY_OK);
  check_bool("auto-detect off does not", !muslimtify_location_needs_detect(mt));
  muslimtify_close(mt);

  check_bool("NULL does not", !muslimtify_location_needs_detect(NULL));
}

static void test_detect_location(void) {
  printf("  detect location...\n");

  Config cfg = jakarta_config();
  Muslimtify *mt = NULL;
  check_bool("opens", muslimtify_open_config(&cfg, &mt) == MUSLIMTIFY_OK);
  if (!mt)
    return;

  MuslimtifyLocation before;
  MuslimtifyLocation loc;
  MuslimtifyDetection detection;
  check_bool("read before", muslimtify_get_location(mt, &before) == MUSLIMTIFY_OK);
  check_bool("starts with auto-detect off", !before.auto_detect);

  // A failed lookup changes nothing, whatever it did to its working copy.
  memset(&detection, 0x5a, sizeof(detection));
  check_bool("failure is reported",
             muslimtify_detect_location_with(mt, stub_detect_fail, &detection) ==
                 MUSLIMTIFY_ERR_DETECT_FAILED);
  check_bool("failure leaves the result alone", all_bytes(&detection, sizeof(detection), 0x5a));
  check_bool("read after failure", muslimtify_get_location(mt, &loc) == MUSLIMTIFY_OK);
  check_bool("failure leaves the location alone", same_location(&before, &loc));

  // A successful lookup commits and turns auto-detect on.
  check_bool("success",
             muslimtify_detect_location_with(mt, stub_detect, &detection) == MUSLIMTIFY_OK);
  check_bool("read after success", muslimtify_get_location(mt, &loc) == MUSLIMTIFY_OK);
  check_bool("coordinates committed", loc.latitude == -7.25 && loc.longitude == 112.75);
  check_bool("labels committed",
             strcmp(loc.city, "Surabaya") == 0 && strcmp(loc.country, "ID") == 0);
  check_bool("auto-detect on", loc.auto_detect);
  check_bool("source is the IP lookup", detection.source == MUSLIMTIFY_SOURCE_IP);
  check_bool("no GPS problem", detection.gps == MUSLIMTIFY_OK && !detection.gps_disabled);

  check_bool("result is optional",
             muslimtify_detect_location_with(mt, stub_detect, NULL) == MUSLIMTIFY_OK);
  check_bool("NULL handle",
             muslimtify_detect_location(NULL, &detection) == MUSLIMTIFY_ERR_INVALID_ARG);
  check_bool("NULL source",
             muslimtify_detect_location_with(mt, NULL, &detection) == MUSLIMTIFY_ERR_INVALID_ARG);
  muslimtify_close(mt);
}

// Every GPS outcome, with GPS enabled on the handle.
static void test_detect_gps_matrix(void) {
  printf("  detect with GPS enabled...\n");

  const struct {
    GpsStatus status;
    MuslimtifyLocationSource source;
    MuslimtifyError gps;
    bool disabled;
  } cases[] = {
      {GPS_OK, MUSLIMTIFY_SOURCE_GPS, MUSLIMTIFY_OK, false},
      {GPS_NO_FIX, MUSLIMTIFY_SOURCE_IP, MUSLIMTIFY_OK, false},
      {GPS_NO_DAEMON, MUSLIMTIFY_SOURCE_IP, MUSLIMTIFY_ERR_GPS_NO_DAEMON, true},
      {GPS_NO_DEVICE, MUSLIMTIFY_SOURCE_IP, MUSLIMTIFY_ERR_GPS_NO_DEVICE, true},
      {GPS_UNAVAILABLE, MUSLIMTIFY_SOURCE_IP, MUSLIMTIFY_ERR_GPS_UNAVAILABLE, true},
      {GPS_NO_PERMISSION, MUSLIMTIFY_SOURCE_IP, MUSLIMTIFY_ERR_GPS_NO_PERMISSION, false},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    Config cfg = jakarta_config();
    cfg.use_gps = true;
    Muslimtify *mt = NULL;
    MuslimtifyDetection detection;
    MuslimtifyLocation loc;
    if (muslimtify_open_config(&cfg, &mt) != MUSLIMTIFY_OK) {
      check_bool("opens", false);
      continue;
    }
    stub_gps_status = cases[i].status;
    check_bool("detects",
               muslimtify_detect_location_with(mt, stub_detect, &detection) == MUSLIMTIFY_OK);
    check_bool("source", detection.source == cases[i].source);
    check_bool("gps code", detection.gps == cases[i].gps);
    check_bool("gps disabled", detection.gps_disabled == cases[i].disabled);
    // A warning exists exactly when there is a GPS problem to report.
    check_bool("warning matches the code", (muslimtify_detection_warning(&detection) != NULL) ==
                                               (cases[i].gps != MUSLIMTIFY_OK));
    check_bool("read", muslimtify_get_location(mt, &loc) == MUSLIMTIFY_OK);
    check_bool("stored GPS flag", loc.gps == !cases[i].disabled);
    check_bool("coordinates follow the source",
               loc.latitude == (cases[i].source == MUSLIMTIFY_SOURCE_GPS ? 1.5 : -7.25));
    muslimtify_close(mt);
  }

  // With GPS off the receiver is never asked.
  Config off = jakarta_config();
  Muslimtify *mt = NULL;
  MuslimtifyDetection detection;
  check_bool("opens with GPS off", muslimtify_open_config(&off, &mt) == MUSLIMTIFY_OK);
  stub_gps_status = GPS_NO_DAEMON;
  stub_gps_calls = 0;
  check_bool("detects with GPS off",
             muslimtify_detect_location_with(mt, stub_detect, &detection) == MUSLIMTIFY_OK);
  check_bool("receiver not asked", stub_gps_calls == 0);
  check_bool("nothing to report", detection.gps == MUSLIMTIFY_OK && !detection.gps_disabled &&
                                      detection.source == MUSLIMTIFY_SOURCE_IP);
  check_bool("no warning with GPS off", muslimtify_detection_warning(&detection) == NULL);
  check_bool("no warning for NULL", muslimtify_detection_warning(NULL) == NULL);
  muslimtify_close(mt);
}

static void test_set_gps(void) {
  printf("  GPS toggle...\n");

  Config cfg = jakarta_config();
  Muslimtify *mt = NULL;
  check_bool("opens", muslimtify_open_config(&cfg, &mt) == MUSLIMTIFY_OK);
  if (!mt)
    return;

  MuslimtifyLocation before;
  MuslimtifyLocation loc;
  bool has_fix = true;
  check_bool("read before", muslimtify_get_location(mt, &before) == MUSLIMTIFY_OK);

  // Each structural problem refuses to enable and changes nothing.
  const struct {
    GpsStatus status;
    MuslimtifyError err;
  } failures[] = {
      {GPS_NO_DAEMON, MUSLIMTIFY_ERR_GPS_NO_DAEMON},
      {GPS_NO_DEVICE, MUSLIMTIFY_ERR_GPS_NO_DEVICE},
      {GPS_NO_PERMISSION, MUSLIMTIFY_ERR_GPS_NO_PERMISSION},
      {GPS_UNAVAILABLE, MUSLIMTIFY_ERR_GPS_UNAVAILABLE},
  };
  for (size_t i = 0; i < sizeof(failures) / sizeof(failures[0]); i++) {
    stub_gps_status = failures[i].status;
    check_bool("enable is refused",
               muslimtify_set_gps_with(mt, true, stub_gps, &has_fix) == failures[i].err);
    check_bool("read after refusal", muslimtify_get_location(mt, &loc) == MUSLIMTIFY_OK);
    check_bool("refusal changes nothing", same_location(&before, &loc));
  }

  // A receiver with no fix yet still enables, and keeps the stored coordinates.
  stub_gps_status = GPS_NO_FIX;
  has_fix = true;
  check_bool("enable without a fix",
             muslimtify_set_gps_with(mt, true, stub_gps, &has_fix) == MUSLIMTIFY_OK);
  check_bool("no fix reported", !has_fix);
  check_bool("read", muslimtify_get_location(mt, &loc) == MUSLIMTIFY_OK);
  check_bool("GPS on, coordinates kept", loc.gps && loc.latitude == before.latitude);

  // Turning it off never asks the receiver.
  stub_gps_calls = 0;
  has_fix = true;
  check_bool("disable", muslimtify_set_gps_with(mt, false, stub_gps, &has_fix) == MUSLIMTIFY_OK);
  check_bool("disable does not probe", stub_gps_calls == 0 && !has_fix);
  check_bool("read off", muslimtify_get_location(mt, &loc) == MUSLIMTIFY_OK && !loc.gps);

  // A fix enables and stores where the receiver says it is.
  stub_gps_status = GPS_OK;
  has_fix = false;
  check_bool("enable with a fix",
             muslimtify_set_gps_with(mt, true, stub_gps, &has_fix) == MUSLIMTIFY_OK);
  check_bool("fix reported", has_fix);
  check_bool("read fix", muslimtify_get_location(mt, &loc) == MUSLIMTIFY_OK);
  check_bool("GPS on with the fix stored", loc.gps && loc.latitude == 1.5 && loc.longitude == 2.5);

  check_bool("has_fix is optional",
             muslimtify_set_gps_with(mt, true, stub_gps, NULL) == MUSLIMTIFY_OK);
  check_bool("NULL handle", muslimtify_set_gps(NULL, true, NULL) == MUSLIMTIFY_ERR_INVALID_ARG);
  muslimtify_close(mt);
}

static void test_method_from_country(void) {
  printf("  method from country...\n");

  Config cfg = jakarta_config();
  snprintf(cfg.country, sizeof(cfg.country), "ID");
  Muslimtify *mt = NULL;
  MuslimtifyMethodInfo info;
  check_bool("opens", muslimtify_open_config(&cfg, &mt) == MUSLIMTIFY_OK);
  if (!mt)
    return;
  check_bool("from a country", muslimtify_set_method_from_country(mt) == MUSLIMTIFY_OK);
  check_bool("read", muslimtify_get_method(mt, &info) == MUSLIMTIFY_OK);
  check_bool("the country's default",
             strcmp(info.key, method_to_string(country_default_method("ID"))) == 0);
  muslimtify_close(mt);

  // No country falls back to the engine's default, as method --auto relies on.
  cfg.country[0] = '\0';
  check_bool("opens without a country", muslimtify_open_config(&cfg, &mt) == MUSLIMTIFY_OK);
  check_bool("from no country", muslimtify_set_method_from_country(mt) == MUSLIMTIFY_OK);
  check_bool("read fallback", muslimtify_get_method(mt, &info) == MUSLIMTIFY_OK);
  check_bool("the fallback method",
             strcmp(info.key, method_to_string(country_default_method(""))) == 0);
  muslimtify_close(mt);

  check_bool("NULL handle", muslimtify_set_method_from_country(NULL) == MUSLIMTIFY_ERR_INVALID_ARG);

  check_bool("uppercase code", muslimtify_check_country("ID") == MUSLIMTIFY_OK);
  check_bool("lowercase code", muslimtify_check_country("id") == MUSLIMTIFY_OK);
  check_bool("three letters", muslimtify_check_country("ZZZ") == MUSLIMTIFY_ERR_INVALID_COUNTRY);
  check_bool("one character", muslimtify_check_country("1") == MUSLIMTIFY_ERR_INVALID_COUNTRY);
  check_bool("empty", muslimtify_check_country("") == MUSLIMTIFY_ERR_INVALID_COUNTRY);
  check_bool("NULL code", muslimtify_check_country(NULL) == MUSLIMTIFY_ERR_INVALID_ARG);
}

// -- Agreement with the engine ------------------------------------------------

// Walk every site through every day of 2026. Each valid time must print the
// same clock string as format_time_hm on the raw double, carry the day offset
// of the minute-rounded value, the calendar date that offset implies, and an
// instant built from those. Each row must be in non-decreasing order.
static void test_engine_agreement(void) {
  printf("  agreement with the engine...\n");

  long start = mt_days_from_civil(2026, 1, 1);
  long end = mt_days_from_civil(2026, 12, 31);
  struct tm now = far_now();
  int invalid_seen = 0;

  for (size_t s = 0; s < SITE_COUNT; s++) {
    Config cfg = site_config(&SITES[s]);
    Muslimtify *mt = NULL;
    if (muslimtify_open_config(&cfg, &mt) != MUSLIMTIFY_OK) {
      check_bool("site opens", false);
      continue;
    }

    int seen_plus = 0;
    for (long z = start; z <= end; z++) {
      int y, m, d;
      mt_civil_from_days(z, &y, &m, &d);

      MuslimtifyDay day;
      check_bool("day succeeds", muslimtify_day_at(mt, &now, y, m, d, &day) == MUSLIMTIFY_OK);
      check_bool("day carries its date", day.year == y && day.month == m && day.day == d);
      check_bool("no next on another day", day.next == -1);

      struct PrayerTimes t = prayer_times_for_config(&cfg, y, m, d);
      double values[MUSLIMTIFY_PRAYER_COUNT] = {t.fajr, t.dhuhr, t.asr, t.maghrib, t.isha};

      long long prev_key = LLONG_MIN;
      for (int i = 0; i < MUSLIMTIFY_PRAYER_COUNT; i++) {
        const MuslimtifyTime *pt = &day.prayers[i].time;
        char via[MUSLIMTIFY_TIME_STR_SIZE];
        muslimtify_format_time(pt, 24, via, sizeof(via));

        if (!isfinite(values[i])) {
          invalid_seen++;
          check_bool("non-finite time is invalid", !pt->valid);
          check_bool("invalid time renders --:--", strcmp(via, "--:--") == 0);
          continue;
        }
        check_bool("finite time is valid", pt->valid);

        char raw[6];
        format_time_hm(values[i], raw, sizeof(raw));
        check_bool("clock matches format_time_hm", strcmp(raw, via) == 0);

        long total = (long)ceil(values[i] * 60.0);
        int expected = total < 0 ? -1 : (total >= 24L * 60 ? 1 : 0);
        check_bool("day offset matches minute-rounded rule", pt->day_offset == expected);
        if (expected > 0)
          seen_plus++;

        int ey, em, ed;
        mt_civil_from_days(z + expected, &ey, &em, &ed);
        check_bool("date is the day the time falls on",
                   pt->year == ey && pt->month == em && pt->day == ed);

        long long instant = (long long)(z + expected) * 86400LL + pt->hour * 3600LL +
                            pt->minute * 60LL - llround(SITES[s].offset * 3600.0);
        check_bool("instant matches date, clock and offset", (long long)pt->instant == instant);

        long long key = (long long)(z + expected) * 1440LL + pt->hour * 60LL + pt->minute;
        check_bool("row is in order", key >= prev_key);
        prev_key = key;
      }
    }

    // Reykjavik has days whose isha falls after midnight, so the next-day
    // branch must be exercised or the checks above pass vacuously.
    if (strcmp(SITES[s].name, "Reykjavik") == 0)
      check_bool("Reykjavik: some time on the next day", seen_plus > 0);

    muslimtify_close(mt);
  }

  printf("    non-finite times seen: %d\n", invalid_seen);
}

// -- Instants -----------------------------------------------------------------

static void test_instant(void) {
  printf("  instant...\n");

  Config cfg = jakarta_config();
  Muslimtify *mt = NULL;
  struct tm now = far_now();
  MuslimtifyDay day;
  check_bool("jakarta opens", muslimtify_open_config(&cfg, &mt) == MUSLIMTIFY_OK);
  check_bool("jakarta day", muslimtify_day_at(mt, &now, 2026, 3, 10, &day) == MUSLIMTIFY_OK);
  for (int i = 0; i < MUSLIMTIFY_PRAYER_COUNT; i++) {
    const MuslimtifyTime *pt = &day.prayers[i].time;
    long long expect = (long long)mt_days_from_civil(pt->year, pt->month, pt->day) * 86400LL +
                       pt->hour * 3600LL + pt->minute * 60LL - 7LL * 3600LL;
    check_bool("jakarta instant is UTC+7", pt->valid && (long long)pt->instant == expect);
    check_bool("jakarta time is on the same day", pt->day_offset == 0 && pt->day == 10);
  }
  muslimtify_close(mt);

  // British Summer Time starts on 2026-03-29, so the wall clock is on UTC the
  // day before and on UTC+1 the day after.
  if (timezone_exists("Europe/London")) {
    Config london = zone_config(51.5074, -0.1278, "Europe/London", 0.0);
    check_bool("london opens", muslimtify_open_config(&london, &mt) == MUSLIMTIFY_OK);
    const int days[2] = {28, 30};
    const long long shift[2] = {0LL, 3600LL};
    for (int k = 0; k < 2; k++) {
      check_bool("london day",
                 muslimtify_day_at(mt, &now, 2026, 3, days[k], &day) == MUSLIMTIFY_OK);
      const MuslimtifyTime *pt = &day.prayers[MUSLIMTIFY_DHUHR].time;
      long long wall = (long long)mt_days_from_civil(pt->year, pt->month, pt->day) * 86400LL +
                       pt->hour * 3600LL + pt->minute * 60LL;
      check_bool("london instant follows DST", wall - (long long)pt->instant == shift[k]);
    }
    muslimtify_close(mt);
  }
}

// -- Errors -------------------------------------------------------------------

static void test_errors(void) {
  printf("  errors...\n");

  Config cfg = jakarta_config();
  Muslimtify *mt = NULL;
  struct tm now = far_now();
  check_bool("opens", muslimtify_open_config(&cfg, &mt) == MUSLIMTIFY_OK);

  MuslimtifyDay day;
  memset(&day, 0x5a, sizeof(day));

  check_bool("day NULL handle",
             muslimtify_day(NULL, 2026, 1, 1, &day) == MUSLIMTIFY_ERR_INVALID_ARG);
  check_bool("day NULL out", muslimtify_day(mt, 2026, 1, 1, NULL) == MUSLIMTIFY_ERR_INVALID_ARG);
  check_bool("today NULL handle", muslimtify_today(NULL, 0, &day) == MUSLIMTIFY_ERR_INVALID_ARG);

  const int bad[][3] = {{2026, 13, 1}, {2026, 0, 1},  {2026, 2, 30}, {2025, 2, 29},
                        {0, 1, 1},     {10000, 1, 1}, {2026, 1, 0},  {2026, 4, 31}};
  for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
    check_bool("bad date through day", muslimtify_day(mt, bad[i][0], bad[i][1], bad[i][2], &day) ==
                                           MUSLIMTIFY_ERR_INVALID_DATE);
    check_bool("bad date through check_range",
               muslimtify_check_range(bad[i][0], bad[i][1], bad[i][2], bad[i][0], bad[i][1],
                                      bad[i][2], NULL) == MUSLIMTIFY_ERR_INVALID_DATE);
  }
  check_bool("out untouched on error", all_bytes(&day, sizeof(day), 0x5a));

  check_bool("leap day is valid",
             muslimtify_check_range(2024, 2, 29, 2024, 2, 29, NULL) == MUSLIMTIFY_OK);
  check_bool("today shift too large",
             muslimtify_today(mt, 9999L * 366L + 1, &day) == MUSLIMTIFY_ERR_INVALID_DATE);
  check_bool("today shift before year 1",
             muslimtify_today(mt, -9999L * 366L, &day) == MUSLIMTIFY_ERR_INVALID_DATE);
  check_bool("out untouched after today errors", all_bytes(&day, sizeof(day), 0x5a));

  size_t days = 99;
  check_bool("reversed range",
             muslimtify_check_range(2022, 1, 3, 2022, 1, 1, &days) == MUSLIMTIFY_ERR_DATE_ORDER);
  check_bool("367 days is too long", muslimtify_check_range(2024, 1, 1, 2025, 1, 1, &days) ==
                                         MUSLIMTIFY_ERR_RANGE_TOO_LONG);
  check_bool("days untouched on error", days == 99);
  check_bool("366 days is allowed",
             muslimtify_check_range(2024, 1, 1, 2024, 12, 31, &days) == MUSLIMTIFY_OK);
  check_bool("366 days is counted", days == MUSLIMTIFY_MAX_RANGE_DAYS);

  MuslimtifyDay week[7];
  size_t count = 99;
  check_bool("range NULL count", muslimtify_range(mt, 2026, 3, 1, 2026, 3, 7, week, 7, NULL) ==
                                     MUSLIMTIFY_ERR_INVALID_ARG);
  check_bool("range over cap", muslimtify_range(mt, 2026, 3, 1, 2026, 3, 7, week, 3, &count) ==
                                   MUSLIMTIFY_ERR_RANGE_TOO_LONG);
  check_bool("range reversed", muslimtify_range(mt, 2026, 3, 7, 2026, 3, 1, week, 7, &count) ==
                                   MUSLIMTIFY_ERR_DATE_ORDER);
  check_bool("count untouched on error", count == 99);

  check_bool("range of a week",
             muslimtify_range(mt, 2026, 3, 1, 2026, 3, 7, week, 7, &count) == MUSLIMTIFY_OK);
  check_bool("week has seven days", count == 7);
  for (size_t i = 0; i < count && i < 7; i++) {
    MuslimtifyDay single;
    check_bool("single day",
               muslimtify_day_at(mt, &now, 2026, 3, 1 + (int)i, &single) == MUSLIMTIFY_OK);
    check_bool("range element has the date", week[i].day == 1 + (int)i && week[i].month == 3);
    for (int k = 0; k < MUSLIMTIFY_PRAYER_COUNT; k++)
      check_bool("range element equals the single day",
                 week[i].prayers[k].time.instant == single.prayers[k].time.instant);
  }

  check_bool("today succeeds", muslimtify_today(mt, 0, &day) == MUSLIMTIFY_OK);
  check_bool("another date has no next",
             muslimtify_day(mt, 2000, 1, 1, &day) == MUSLIMTIFY_OK && day.next == -1);

  muslimtify_close(mt);
}

// -- Per-prayer settings ------------------------------------------------------

static void test_prayer_settings(void) {
  printf("  prayer settings...\n");

  Config cfg = jakarta_config();
  cfg.asr.enabled = false;
  cfg.fajr.adhan_enabled = true;
  cfg.fajr.offset = 4;
  cfg.fajr.reminder_count = 2;
  cfg.fajr.reminders[0] = 30;
  cfg.fajr.reminders[1] = 10;

  Muslimtify *mt = NULL;
  struct tm now = far_now();
  MuslimtifyDay day;
  check_bool("opens", muslimtify_open_config(&cfg, &mt) == MUSLIMTIFY_OK);
  check_bool("day", muslimtify_day_at(mt, &now, 2026, 3, 10, &day) == MUSLIMTIFY_OK);

  const MuslimtifyPrayer *fajr = &day.prayers[MUSLIMTIFY_FAJR];
  check_bool("asr disabled", !day.prayers[MUSLIMTIFY_ASR].enabled);
  check_bool("fajr adhan", fajr->adhan_enabled);
  check_bool("fajr offset", fajr->offset == 4);
  check_bool("fajr reminders",
             fajr->reminder_count == 2 && fajr->reminders[0] == 30 && fajr->reminders[1] == 10);
  muslimtify_close(mt);
}

// -- Next prayer --------------------------------------------------------------

static void test_next(void) {
  printf("  next...\n");

  // 23:00 near the spring equinox in London: every prayer of the day has
  // passed, so the next one is the following day's fajr.
  Config cfg = zone_config(51.5074, -0.1278, "Europe/London", 0.0);
  Muslimtify *mt = NULL;
  check_bool("opens", muslimtify_open_config(&cfg, &mt) == MUSLIMTIFY_OK);

  struct tm now = {0};
  now.tm_year = 2022 - 1900;
  now.tm_mon = 3 - 1;
  now.tm_mday = 20;
  now.tm_hour = 23;

  MuslimtifyNext next;
  check_bool("next succeeds", muslimtify_next_at(mt, &now, &next) == MUSLIMTIFY_OK);
  check_bool("next is fajr", next.prayer == MUSLIMTIFY_FAJR);
  check_bool("next is tomorrow", next.time.valid && next.time.month == 3 && next.time.day == 21);
  check_bool("next is ahead", next.minutes_until > 0);

  MuslimtifyDay day;
  check_bool("today has no next after isha",
             muslimtify_day_at(mt, &now, 2022, 3, 20, &day) == MUSLIMTIFY_OK && day.next == -1);

  now.tm_hour = 0;
  now.tm_min = 1;
  check_bool("early morning marks a prayer of the day",
             muslimtify_day_at(mt, &now, 2022, 3, 20, &day) == MUSLIMTIFY_OK && day.next >= 0);

  check_bool("next NULL out", muslimtify_next_at(mt, &now, NULL) == MUSLIMTIFY_ERR_INVALID_ARG);
  check_bool("next NULL handle", muslimtify_next(NULL, &next) == MUSLIMTIFY_ERR_INVALID_ARG);
  muslimtify_close(mt);

  cfg.fajr.enabled = false;
  cfg.dhuhr.enabled = false;
  cfg.asr.enabled = false;
  cfg.maghrib.enabled = false;
  cfg.isha.enabled = false;
  check_bool("opens with all disabled", muslimtify_open_config(&cfg, &mt) == MUSLIMTIFY_OK);
  check_bool("nothing upcoming is a success", muslimtify_next_at(mt, &now, &next) == MUSLIMTIFY_OK);
  check_bool("nothing upcoming is reported", next.prayer == MUSLIMTIFY_PRAYER_COUNT);
  muslimtify_close(mt);
}

// -- Strings ------------------------------------------------------------------

static void test_strings(void) {
  printf("  strings...\n");

  for (int e = MUSLIMTIFY_OK; e <= MUSLIMTIFY_ERR_GPS_UNAVAILABLE; e++) {
    const char *msg = muslimtify_get_error((MuslimtifyError)e);
    check_bool("error message exists", msg != NULL && msg[0] != '\0');
    check_bool("error message is specific", strcmp(msg, "Unknown error") != 0);
  }
  check_bool("unknown code",
             strcmp(muslimtify_get_error((MuslimtifyError)999), "Unknown error") == 0);
  check_bool("success message", strcmp(muslimtify_get_error(MUSLIMTIFY_OK), "Success") == 0);

  check_bool("fajr name", strcmp(muslimtify_prayer_name(MUSLIMTIFY_FAJR), "Fajr") == 0);
  check_bool("isha name", strcmp(muslimtify_prayer_name(MUSLIMTIFY_ISHA), "Isha") == 0);
  check_bool("out of range name",
             strcmp(muslimtify_prayer_name(MUSLIMTIFY_PRAYER_COUNT), "Unknown") == 0);

  const struct {
    int hour;
    int minute;
    const char *h12;
    const char *h24;
  } cases[] = {
      {0, 0, "12:00 AM", "00:00"},  {0, 30, "12:30 AM", "00:30"},  {11, 59, "11:59 AM", "11:59"},
      {12, 0, "12:00 PM", "12:00"}, {12, 30, "12:30 PM", "12:30"}, {23, 59, "11:59 PM", "23:59"},
  };
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    MuslimtifyTime t = {0};
    t.hour = cases[i].hour;
    t.minute = cases[i].minute;
    t.valid = true;
    char buf[MUSLIMTIFY_TIME_STR_SIZE];
    muslimtify_format_time(&t, 12, buf, sizeof(buf));
    check_bool("12 hour form", strcmp(buf, cases[i].h12) == 0);
    muslimtify_format_time(&t, 24, buf, sizeof(buf));
    check_bool("24 hour form", strcmp(buf, cases[i].h24) == 0);
    muslimtify_format_time(&t, 7, buf, sizeof(buf));
    check_bool("any other format is 24 hour", strcmp(buf, cases[i].h24) == 0);
  }

  char buf[MUSLIMTIFY_TIME_STR_SIZE];
  MuslimtifyTime invalid = {0};
  muslimtify_format_time(&invalid, 12, buf, sizeof(buf));
  check_bool("invalid time in 12 hour", strcmp(buf, "--:--") == 0);
  muslimtify_format_time(NULL, 24, buf, sizeof(buf));
  check_bool("NULL time", strcmp(buf, "--:--") == 0);
}

// -- main ---------------------------------------------------------------------

int main(void) {
  printf("Running muslimtify library tests...\n");
  setup();
  test_open_from_disk();
  test_save_reload();
  test_save_invalidates_cache();
  test_open_config();
  test_location_setters();
  test_calculation_setters();
  test_notification_setters();
  test_adhan_file();
  test_names_and_parsing();
  test_unknown_words();
  test_needs_detect();
  test_detect_location();
  test_detect_gps_matrix();
  test_set_gps();
  test_method_from_country();
  test_engine_agreement();
  test_instant();
  test_errors();
  test_prayer_settings();
  test_next();
  test_strings();
  teardown();

  printf("\nResults: %d passed, %d failed\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
