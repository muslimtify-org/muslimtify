#define _GNU_SOURCE
#include "cache.h"
#include "config.h"
#include "lib/muslimtify_internal.h"
#include "muslimtify_cycle.h"
#include "platform.h"
#include "prayer_checker.h"
#include "prayertimes.h"
#include "test_support.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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

static void setup(void) {
  if (!test_tmpdir(tmpdir, sizeof(tmpdir), "cycletest")) {
    fprintf(stderr, "FATAL: cannot create a temporary directory\n");
    exit(1);
  }
  test_set_config_home(tmpdir);
  test_set_cache_home(tmpdir);
}

static void teardown(void) {
  test_remove_tree(tmpdir);
}

typedef struct {
  char kind; /* 'p' for a prayer notification, 'a' for an adhan */
  char prayer[32];
  char time_str[32];
  int minutes_before;
  char urgency[32];
  bool has_preset;
  char preset[32];
  char path[MAX_ADHAN_PATH];
} Sent;

#define SENT_MAX 16
static Sent sent_log[SENT_MAX];
static int sent_count;
static int init_calls;
static int cleanup_calls;
static bool init_ok;
static int detect_calls;
static bool detect_ok;
static double detect_latitude;
static double detect_longitude;
static time_t detect_stamp;
static bool cache_delete_during_notify;

static int rec_init(const char *app_name) {
  (void)app_name;
  init_calls++;
  return init_ok ? 1 : 0;
}

static void rec_prayer(const char *prayer_name, const char *time_str, int minutes_before,
                       const char *urgency, const char *sound_preset) {
  if (cache_delete_during_notify)
    cache_invalidate();
  if (sent_count >= SENT_MAX)
    return;
  Sent *s = &sent_log[sent_count++];
  memset(s, 0, sizeof(*s));
  s->kind = 'p';
  snprintf(s->prayer, sizeof(s->prayer), "%s", prayer_name);
  snprintf(s->time_str, sizeof(s->time_str), "%s", time_str);
  s->minutes_before = minutes_before;
  snprintf(s->urgency, sizeof(s->urgency), "%s", urgency);
  s->has_preset = sound_preset != NULL;
  if (sound_preset)
    snprintf(s->preset, sizeof(s->preset), "%s", sound_preset);
}

static void rec_adhan(const char *prayer_name, const char *time_str, const char *path) {
  if (sent_count >= SENT_MAX)
    return;
  Sent *s = &sent_log[sent_count++];
  memset(s, 0, sizeof(*s));
  s->kind = 'a';
  snprintf(s->prayer, sizeof(s->prayer), "%s", prayer_name);
  snprintf(s->time_str, sizeof(s->time_str), "%s", time_str);
  snprintf(s->path, sizeof(s->path), "%s", path);
}

static void rec_cleanup(void) {
  cleanup_calls++;
}

// A lookup that finds Jakarta, or one that scribbles on its copy and fails.
static int rec_detect(Config *cfg, GpsStatus *status) {
  detect_calls++;
  *status = GPS_OK;
  if (!detect_ok) {
    cfg->latitude = 55.0;
    return -1;
  }
  cfg->latitude = detect_latitude;
  cfg->longitude = detect_longitude;
  snprintf(cfg->timezone, sizeof(cfg->timezone), "Asia/Jakarta");
  cfg->timezone_offset = 7.0;
  cfg->updated_at = (int64_t)detect_stamp;
  return 0;
}

static const MuslimtifyCycleHooks HOOKS = {rec_detect, rec_init, rec_prayer, rec_adhan,
                                           rec_cleanup};

#define JAKARTA_LAT (-6.2088)
#define TEST_YEAR 2026
#define TEST_MONTH 3
#define TEST_DAY 10
// An arbitrary instant. Only its distance from the stored updated_at matters.
#define TEST_EPOCH ((time_t)1000000000)

// Jakarta, every prayer enabled, no reminders, no adhan, and a location the
// user set by hand, so nothing detects or refreshes unless a test asks for it.
static Config base_config(void) {
  Config cfg = config_default();
  cfg.latitude = JAKARTA_LAT;
  cfg.longitude = 106.8456;
  snprintf(cfg.timezone, sizeof(cfg.timezone), "Asia/Jakarta");
  cfg.timezone_offset = 7.0;
  cfg.auto_detect = false;
  PrayerConfig *all[] = {&cfg.fajr, &cfg.dhuhr, &cfg.asr, &cfg.maghrib, &cfg.isha};
  for (int i = 0; i < PRAYER_COUNT; i++) {
    all[i]->enabled = true;
    all[i]->adhan_enabled = false;
    all[i]->reminder_count = 0;
    all[i]->offset = 0;
    all[i]->adhan[0] = '\0';
  }
  return cfg;
}

// The minute of the test day on which dhuhr falls for this config, computed
// the way cache_build_triggers computes it.
static int dhuhr_minute(const Config *cfg) {
  struct PrayerTimes t = prayer_times_for_config(cfg, TEST_YEAR, TEST_MONTH, TEST_DAY);
  return (int)ceil(t.dhuhr * 60.0);
}

static struct tm at_minute(int minute) {
  struct tm now = {0};
  now.tm_year = TEST_YEAR - 1900;
  now.tm_mon = TEST_MONTH - 1;
  now.tm_mday = TEST_DAY;
  now.tm_hour = minute / 60;
  now.tm_min = minute % 60;
  return now;
}

// Start a test: save the config, drop the trigger cache, and forget every
// recorded call.
static void begin(const Config *cfg) {
  check_bool("config saved", config_save(cfg) == 0);
  cache_invalidate();
  memset(sent_log, 0, sizeof(sent_log));
  sent_count = 0;
  init_calls = 0;
  cleanup_calls = 0;
  init_ok = true;
  detect_calls = 0;
  detect_ok = true;
  detect_latitude = JAKARTA_LAT;
  detect_longitude = 106.8456;
  detect_stamp = TEST_EPOCH;
  cache_delete_during_notify = false;
}

static MuslimtifyError cycle_at(int minute, time_t epoch, MuslimtifyCycle *out) {
  struct tm now = at_minute(minute);
  return muslimtify_run_cycle_at(&HOOKS, &now, epoch, out);
}

static void test_fires_once(void) {
  printf("  a due prayer is announced once...\n");

  Config cfg = base_config();
  begin(&cfg);
  int dhuhr = dhuhr_minute(&cfg);
  struct PrayerTimes t = prayer_times_for_config(&cfg, TEST_YEAR, TEST_MONTH, TEST_DAY);
  char expected_time[16];
  format_time_cfg(&cfg, t.dhuhr, expected_time, sizeof(expected_time));

  MuslimtifyCycle cycle;
  check_bool("cycle succeeds", cycle_at(dhuhr, TEST_EPOCH, &cycle) == MUSLIMTIFY_OK);
  check_bool("one notification", sent_count == 1 && cycle.notifications == 1);
  check_bool("it is a prayer notification", sent_log[0].kind == 'p');
  check_bool("for dhuhr", strcmp(sent_log[0].prayer, prayer_get_name(PRAYER_DHUHR)) == 0);
  check_bool("at the prayer itself", sent_log[0].minutes_before == 0);
  check_bool("with the formatted time", strcmp(sent_log[0].time_str, expected_time) == 0);
  check_bool("notifications opened and closed once", init_calls == 1 && cleanup_calls == 1);
  check_bool("nothing detected",
             !cycle.detected && !cycle.refreshed && !cycle.refresh_failed && detect_calls == 0);

  check_bool("second cycle succeeds", cycle_at(dhuhr, TEST_EPOCH, &cycle) == MUSLIMTIFY_OK);
  check_bool("the same minute sends nothing more", sent_count == 1 && cycle.notifications == 0);

  check_bool("a NULL result is accepted", cycle_at(dhuhr + 1, TEST_EPOCH, NULL) == MUSLIMTIFY_OK);
  check_bool("NULL hooks",
             muslimtify_run_cycle_at(NULL, NULL, TEST_EPOCH, &cycle) == MUSLIMTIFY_ERR_INVALID_ARG);
}

static void test_reminder(void) {
  printf("  a reminder is announced ahead of the prayer...\n");

  Config cfg = base_config();
  cfg.dhuhr.reminder_count = 1;
  cfg.dhuhr.reminders[0] = 10;
  begin(&cfg);
  int dhuhr = dhuhr_minute(&cfg);

  MuslimtifyCycle cycle;
  check_bool("cycle succeeds", cycle_at(dhuhr - 10, TEST_EPOCH, &cycle) == MUSLIMTIFY_OK);
  check_bool("one reminder", sent_count == 1 && sent_log[0].kind == 'p');
  check_bool("ten minutes ahead", sent_log[0].minutes_before == 10);
  check_bool("for dhuhr", strcmp(sent_log[0].prayer, prayer_get_name(PRAYER_DHUHR)) == 0);
}

static void test_adhan(void) {
  printf("  the adhan plays when it is enabled...\n");

  // A real file, in case loading the config ever checks that the path exists.
  char adhan_file[512];
  snprintf(adhan_file, sizeof(adhan_file), "%s/adhan.mp3", tmpdir);
  FILE *f = fopen(adhan_file, "w");
  check_bool("adhan file created", f != NULL);
  if (!f)
    return;
  fputs("x", f);
  fclose(f);

  Config cfg = base_config();
  cfg.dhuhr.adhan_enabled = true;
  snprintf(cfg.dhuhr.adhan, sizeof(cfg.dhuhr.adhan), "%s", adhan_file);
  begin(&cfg);
  int dhuhr = dhuhr_minute(&cfg);

  // Compare against what the cycle itself will load, not against the literal.
  Config loaded;
  check_bool("config reloads", config_load(&loaded) == 0);
  check_bool("the adhan path survives a reload", loaded.dhuhr.adhan[0] != '\0');

  MuslimtifyCycle cycle;
  check_bool("cycle succeeds", cycle_at(dhuhr, TEST_EPOCH, &cycle) == MUSLIMTIFY_OK);
  check_bool("one call, and it is the adhan", sent_count == 1 && sent_log[0].kind == 'a');
  check_bool("with the configured file", strcmp(sent_log[0].path, loaded.dhuhr.adhan) == 0);
  check_bool("counted as a notification", cycle.notifications == 1);
}

static void test_catch_up(void) {
  printf("  catch-up after a missed minute...\n");

  Config cfg = base_config();
  int dhuhr = dhuhr_minute(&cfg);
  MuslimtifyCycle cycle;

  // Ten minutes late is inside the window, from a fresh cache or an existing one.
  begin(&cfg);
  check_bool("late cycle succeeds", cycle_at(dhuhr + 10, TEST_EPOCH, &cycle) == MUSLIMTIFY_OK);
  check_bool("ten minutes late still fires", sent_count == 1);

  begin(&cfg);
  check_bool("early cycle succeeds", cycle_at(dhuhr - 1, TEST_EPOCH, &cycle) == MUSLIMTIFY_OK);
  check_bool("nothing is due yet", sent_count == 0);
  check_bool("late cycle on that cache", cycle_at(dhuhr + 10, TEST_EPOCH, &cycle) == MUSLIMTIFY_OK);
  check_bool("ten minutes late fires from the cache", sent_count == 1);

  // Twenty minutes late is outside it, so the trigger is dropped unannounced.
  begin(&cfg);
  check_bool("early cycle again", cycle_at(dhuhr - 1, TEST_EPOCH, &cycle) == MUSLIMTIFY_OK);
  check_bool("very late cycle", cycle_at(dhuhr + 20, TEST_EPOCH, &cycle) == MUSLIMTIFY_OK);
  check_bool("twenty minutes late is dropped", sent_count == 0);
  check_bool("and stays dropped",
             cycle_at(dhuhr + 21, TEST_EPOCH, &cycle) == MUSLIMTIFY_OK && sent_count == 0);

  begin(&cfg);
  check_bool("very late on a fresh cache",
             cycle_at(dhuhr + 20, TEST_EPOCH, &cycle) == MUSLIMTIFY_OK);
  check_bool("a fresh cache does not revive it", sent_count == 0);
}

static void test_new_day(void) {
  printf("  a cache from another day is rebuilt...\n");

  Config cfg = base_config();
  begin(&cfg);
  static PrayerCache cache;
  memset(&cache, 0, sizeof(cache));
  snprintf(cache.date, sizeof(cache.date), "2000-01-01");
  check_bool("old cache written", cache_save(&cache) == 0);

  MuslimtifyCycle cycle;
  check_bool("cycle succeeds",
             cycle_at(dhuhr_minute(&cfg) - 1, TEST_EPOCH, &cycle) == MUSLIMTIFY_OK);
  check_bool("cache reloads", cache_load(&cache) == 0);
  check_bool("cache is for the test day", strcmp(cache.date, "2026-03-10") == 0);
  check_bool("with triggers still ahead", cache.trigger_count >= 1);
}

static void test_first_run(void) {
  printf("  first-run detection...\n");

  // No location and auto-detect on: the cycle detects and saves.
  Config cfg = config_default();
  begin(&cfg);
  MuslimtifyCycle cycle;
  Config loaded;
  check_bool("cycle succeeds", cycle_at(60, TEST_EPOCH, &cycle) == MUSLIMTIFY_OK);
  check_bool("detected once", cycle.detected && detect_calls == 1);
  check_bool("not counted as a refresh", !cycle.refreshed && !cycle.refresh_failed);
  check_bool("config reloads", config_load(&loaded) == 0);
  check_bool("the location reached the disk", loaded.latitude == JAKARTA_LAT);

  // A failed lookup stops the cycle and stores nothing.
  begin(&cfg);
  detect_ok = false;
  check_bool("failure is reported",
             cycle_at(60, TEST_EPOCH, &cycle) == MUSLIMTIFY_ERR_DETECT_FAILED);
  check_bool("nothing sent", sent_count == 0 && !cycle.detected);
  check_bool("config reloads after failure", config_load(&loaded) == 0);
  check_bool("the disk still has no location", config_location_needs_detect(&loaded));
}

// Auto-detected an hour interval ago plus a bit, so the next cycle refreshes.
static Config stale_config(void) {
  Config cfg = base_config();
  cfg.auto_detect = true;
  cfg.refresh_interval = 3600;
  cfg.updated_at = (int64_t)TEST_EPOCH - 7200;
  return cfg;
}

static void test_stale_refresh(void) {
  printf("  a stale location is refreshed...\n");

  Config cfg = stale_config();
  int dhuhr = dhuhr_minute(&cfg);
  MuslimtifyCycle cycle;
  Config loaded;

  begin(&cfg);
  detect_latitude = -6.3;
  check_bool("cycle succeeds", cycle_at(60, TEST_EPOCH, &cycle) == MUSLIMTIFY_OK);
  check_bool("refreshed once", cycle.refreshed && !cycle.refresh_failed && detect_calls == 1);
  check_bool("not counted as a first run", !cycle.detected);
  check_bool("config reloads", config_load(&loaded) == 0);
  check_bool("the new location reached the disk", loaded.latitude == -6.3);
  check_bool("with its timestamp", loaded.updated_at == (int64_t)TEST_EPOCH);

  // A failed refresh keeps the stored location and the cycle still announces.
  begin(&cfg);
  detect_ok = false;
  check_bool("cycle still succeeds", cycle_at(dhuhr, TEST_EPOCH, &cycle) == MUSLIMTIFY_OK);
  check_bool("failure is flagged", cycle.refresh_failed && !cycle.refreshed);
  check_bool("the due prayer is still announced", sent_count == 1);
  check_bool("config reloads after failure", config_load(&loaded) == 0);
  check_bool("the stored location is untouched",
             loaded.latitude == JAKARTA_LAT && loaded.updated_at == (int64_t)TEST_EPOCH - 7200);
}

// The case the config_save rule exists for: a refresh shortly after a prayer
// was announced must not announce it again.
static void test_refresh_does_not_repeat(void) {
  printf("  a refresh does not repeat an announcement...\n");

  Config cfg = base_config();
  cfg.auto_detect = true;
  cfg.refresh_interval = 3600;
  cfg.updated_at = (int64_t)TEST_EPOCH;
  begin(&cfg);
  int dhuhr = dhuhr_minute(&cfg);

  MuslimtifyCycle cycle;
  check_bool("first cycle succeeds", cycle_at(dhuhr, TEST_EPOCH, &cycle) == MUSLIMTIFY_OK);
  check_bool("announced, no refresh yet", sent_count == 1 && !cycle.refreshed);

  // Five minutes on the wall clock, but far enough in real time to be stale.
  detect_stamp = TEST_EPOCH + 4000;
  check_bool("second cycle succeeds",
             cycle_at(dhuhr + 5, TEST_EPOCH + 4000, &cycle) == MUSLIMTIFY_OK);
  check_bool("it refreshed", cycle.refreshed && detect_calls == 1);
  check_bool("and announced nothing again", sent_count == 1 && cycle.notifications == 0);
}

// A refresh that moves the location rebuilds today's triggers for the new
// place: the old dhuhr minute goes quiet and the new one is announced.
static void test_refresh_moves_triggers(void) {
  printf("  a refresh moves today's triggers...\n");

  Config cfg = base_config();
  cfg.auto_detect = true;
  cfg.refresh_interval = 3600;
  cfg.updated_at = (int64_t)TEST_EPOCH;
  begin(&cfg);
  int old_dhuhr = dhuhr_minute(&cfg);

  // Build and save today's cache at the old location, well before dhuhr.
  MuslimtifyCycle cycle;
  check_bool("first cycle succeeds",
             cycle_at(old_dhuhr - 120, TEST_EPOCH, &cycle) == MUSLIMTIFY_OK);
  check_bool("nothing due yet", sent_count == 0);

  // Jakarta's latitude but Kathmandu's longitude: dhuhr moves by over an hour.
  Config moved = cfg;
  moved.longitude = 85.3;
  int new_dhuhr = dhuhr_minute(&moved);
  check_bool("the two dhuhr minutes differ", new_dhuhr != old_dhuhr);

  detect_stamp = TEST_EPOCH + 4000;
  detect_longitude = 85.3;
  check_bool("refresh cycle succeeds",
             cycle_at(old_dhuhr - 100, TEST_EPOCH + 4000, &cycle) == MUSLIMTIFY_OK);
  check_bool("it refreshed", cycle.refreshed && detect_calls == 1);

  check_bool("old dhuhr minute is quiet",
             cycle_at(old_dhuhr, TEST_EPOCH + 4000, &cycle) == MUSLIMTIFY_OK && sent_count == 0);
  check_bool("new dhuhr minute announces",
             cycle_at(new_dhuhr, TEST_EPOCH + 4000, &cycle) == MUSLIMTIFY_OK && sent_count == 1);
}

// A failed refresh is not retried on the very next cycle.
static void test_refresh_backs_off(void) {
  printf("  a failed refresh backs off...\n");

  Config cfg = stale_config();
  begin(&cfg);
  detect_ok = false;
  MuslimtifyCycle cycle;
  check_bool("first attempt fails",
             cycle_at(60, TEST_EPOCH + 100000, &cycle) == MUSLIMTIFY_OK && cycle.refresh_failed);
  check_bool("next minute does not retry",
             cycle_at(61, TEST_EPOCH + 100060, &cycle) == MUSLIMTIFY_OK && !cycle.refresh_failed &&
                 detect_calls == 1);
  detect_ok = true;
  check_bool("it retries later", cycle_at(75, TEST_EPOCH + 100900, &cycle) == MUSLIMTIFY_OK &&
                                     cycle.refreshed && detect_calls == 2);
}

// Settings saved while a cycle runs delete the cache. The cycle must not write
// its stale copy back, and an unwritable cache is reported.
static void test_cache_gone_and_unwritable(void) {
  printf("  a cache deleted during the cycle, and one that cannot be written...\n");

  Config cfg = base_config();
  begin(&cfg);
  int dhuhr = dhuhr_minute(&cfg);

  // An adhan hook stands in for the moment another process saves settings.
  MuslimtifyCycle cycle;
  cache_delete_during_notify = true;
  check_bool("cycle succeeds", cycle_at(dhuhr, TEST_EPOCH, &cycle) == MUSLIMTIFY_OK);
  check_bool("announced once", sent_count == 1);
  check_bool("the deleted cache stays deleted", !platform_file_exists(cache_get_path()));
  cache_delete_during_notify = false;

  // The cache directory is read-only, so the cache cannot be written. (Root
  // ignores directory modes, so under root this part proves nothing.)
  if (test_has_posix_modes()) {
    begin(&cfg);
    char cache_dir[512];
    snprintf(cache_dir, sizeof(cache_dir), "%s/muslimtify", tmpdir);
    check_bool("cache dir made read-only", test_chmod(cache_dir, 0500));
    check_bool("the failure is reported",
               cycle_at(dhuhr, TEST_EPOCH, &cycle) == MUSLIMTIFY_ERR_CACHE_SAVE);
    check_bool("the prayer was still announced", sent_count == 1 && cycle.notifications == 1);
    test_chmod(cache_dir, 0700);
  } else {
    printf("  SKIP: file modes mean nothing here\n");
  }
}

static void test_init_failure(void) {
  printf("  a notification system that will not start...\n");

  Config cfg = base_config();
  begin(&cfg);
  int dhuhr = dhuhr_minute(&cfg);

  MuslimtifyCycle cycle;
  init_ok = false;
  check_bool("failure is reported",
             cycle_at(dhuhr, TEST_EPOCH, &cycle) == MUSLIMTIFY_ERR_NOTIFY_INIT);
  check_bool("nothing sent", sent_count == 0 && cycle.notifications == 0);

  init_ok = true;
  check_bool("next cycle succeeds", cycle_at(dhuhr, TEST_EPOCH, &cycle) == MUSLIMTIFY_OK);
  check_bool("the trigger was kept and now fires", sent_count == 1);
}

static void test_sound_and_urgency(void) {
  printf("  sound preset and urgency...\n");

  Config cfg = base_config();
  cfg.dhuhr.reminder_count = 1;
  cfg.dhuhr.reminders[0] = 10;
  snprintf(cfg.notification_sound, sizeof(cfg.notification_sound), "default");
  snprintf(cfg.notification_urgency, sizeof(cfg.notification_urgency), "critical");
  begin(&cfg);
  int dhuhr = dhuhr_minute(&cfg);

  // Compare against what the cycle itself will load, not against literals.
  Config loaded;
  check_bool("config reloads", config_load(&loaded) == 0);

  MuslimtifyCycle cycle;
  check_bool("reminder cycle", cycle_at(dhuhr - 10, TEST_EPOCH, &cycle) == MUSLIMTIFY_OK);
  check_bool("prayer cycle", cycle_at(dhuhr, TEST_EPOCH, &cycle) == MUSLIMTIFY_OK);
  check_bool("two notifications", sent_count == 2);
  check_bool("the reminder uses the reminder preset",
             sent_log[0].has_preset &&
                 strcmp(sent_log[0].preset, loaded.notification_sound_reminder) == 0);
  check_bool("the prayer uses the alarm preset",
             sent_log[1].has_preset &&
                 strcmp(sent_log[1].preset, loaded.notification_sound_alarm) == 0);
  check_bool("the urgency is passed through",
             strcmp(sent_log[1].urgency, loaded.notification_urgency) == 0);

  snprintf(cfg.notification_sound, sizeof(cfg.notification_sound), "off");
  begin(&cfg);
  check_bool("silent cycle", cycle_at(dhuhr, TEST_EPOCH, &cycle) == MUSLIMTIFY_OK);
  check_bool("sound off passes no preset", sent_count == 1 && !sent_log[0].has_preset);
}

static void test_notify_test(void) {
  printf("  test notification...\n");

  Config cfg = base_config();
  begin(&cfg);
  int dhuhr = dhuhr_minute(&cfg);
  struct tm now = at_minute(dhuhr - 30);

  Muslimtify *mt = NULL;
  MuslimtifyNext sent;
  check_bool("opens", muslimtify_open_config(&cfg, &mt) == MUSLIMTIFY_OK);
  if (!mt)
    return;

  memset(&sent, 0, sizeof(sent));
  check_bool("sends", muslimtify_notify_test_at(mt, &HOOKS, &now, false, &sent) == MUSLIMTIFY_OK);
  check_bool("one prayer notification", sent_count == 1 && sent_log[0].kind == 'p');
  check_bool("for the next prayer",
             strcmp(sent_log[0].prayer, prayer_get_name(PRAYER_DHUHR)) == 0 &&
                 sent.prayer == MUSLIMTIFY_DHUHR);
  check_bool("at the prayer itself", sent_log[0].minutes_before == 0);
  check_bool("the announced time is returned", sent.time.valid);
  check_bool("notifications opened and closed", init_calls == 1 && cleanup_calls == 1);

  check_bool("adhan", muslimtify_notify_test_at(mt, &HOOKS, &now, true, NULL) == MUSLIMTIFY_OK);
  check_bool("the second call is the adhan", sent_count == 2 && sent_log[1].kind == 'a');

  init_ok = false;
  check_bool("init failure", muslimtify_notify_test_at(mt, &HOOKS, &now, false, &sent) ==
                                 MUSLIMTIFY_ERR_NOTIFY_INIT);
  check_bool("init failure sends nothing", sent_count == 2);
  init_ok = true;
  check_bool("NULL handle", muslimtify_notify_test_at(NULL, &HOOKS, &now, false, &sent) ==
                                MUSLIMTIFY_ERR_INVALID_ARG);
  muslimtify_close(mt);

  // Nothing enabled means nothing is ahead.
  Config none = base_config();
  none.fajr.enabled = false;
  none.dhuhr.enabled = false;
  none.asr.enabled = false;
  none.maghrib.enabled = false;
  none.isha.enabled = false;
  check_bool("opens with nothing enabled", muslimtify_open_config(&none, &mt) == MUSLIMTIFY_OK);
  check_bool("nothing upcoming", muslimtify_notify_test_at(mt, &HOOKS, &now, false, &sent) ==
                                     MUSLIMTIFY_ERR_NO_UPCOMING_PRAYER);
  muslimtify_close(mt);

  Config nowhere = config_default();
  check_bool("opens without a location", muslimtify_open_config(&nowhere, &mt) == MUSLIMTIFY_OK);
  check_bool("no location", muslimtify_notify_test_at(mt, &HOOKS, &now, false, &sent) ==
                                MUSLIMTIFY_ERR_NO_LOCATION);
  muslimtify_close(mt);
  check_bool("the failures sent nothing", sent_count == 2);
}

int main(void) {
  printf("Running muslimtify cycle tests...\n");
  setup();
  test_fires_once();
  test_reminder();
  test_adhan();
  test_catch_up();
  test_new_day();
  test_first_run();
  test_stale_refresh();
  test_refresh_does_not_repeat();
  test_refresh_moves_triggers();
  test_refresh_backs_off();
  test_cache_gone_and_unwritable();
  test_init_failure();
  test_sound_and_urgency();
  test_notify_test();
  teardown();

  printf("\nResults: %d passed, %d failed\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
