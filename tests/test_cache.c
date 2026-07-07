#define _GNU_SOURCE
#include "cache.h"
#include "config.h"
#include "platform.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef _WIN32
#include <sys/stat.h>
#endif

static int passed = 0;
static int failed = 0;

static void check_bool(const char *test, bool cond) {
  if (cond) {
    passed++;
  } else {
    failed++;
    fprintf(stderr, "FAIL [%s]\n", test);
  }
}

// Jakarta prayer times (same as test_prayer_checker.c)
static struct PrayerTimes jakarta_times(void) {
  return (struct PrayerTimes){
      .fajr = 4.0 + 26.0 / 60.0,
      .sunrise = 5.0 + 46.0 / 60.0,
      .dhuha = 6.0 + 14.0 / 60.0,
      .dhuhr = 12.0 + 4.0 / 60.0,
      .asr = 15.0 + 29.0 / 60.0,
      .maghrib = 18.0 + 17.0 / 60.0,
      .isha = 19.0 + 32.0 / 60.0,
  };
}

static Config test_config(void) {
  Config cfg = config_default();
  cfg.latitude = -6.2088;
  cfg.longitude = 106.8456;
  cfg.timezone_offset = 7.0;
  cfg.auto_detect = false;
  return cfg;
}

static void test_build_triggers_includes_future(void) {
  printf("  build triggers includes future only...\n");
  Config cfg = test_config();
  struct PrayerTimes times = jakarta_times();
  PrayerCache cache = {0};

  // At 12:00 (minute 720), should include dhuhr (12:04=724) and later,
  // plus their reminders. Should NOT include fajr, sunrise, dhuha.
  int count = cache_build_triggers(&cache, &cfg, &times, 720, "2026-03-22");

  check_bool("has triggers", count > 0);
  check_bool("date set", strcmp(cache.date, "2026-03-22") == 0);

  // First trigger should be at or after minute 720
  for (int i = 0; i < cache.trigger_count; i++) {
    check_bool("trigger >= current", cache.triggers[i].minute >= 720);
  }

  // Should include dhuhr exact (minute 724 = ceil(12.0667*60))
  bool found_dhuhr = false;
  for (int i = 0; i < cache.trigger_count; i++) {
    if (strcmp(cache.triggers[i].prayer, "Dhuhr") == 0 && cache.triggers[i].minutes_before == 0) {
      found_dhuhr = true;
    }
  }
  check_bool("includes dhuhr exact", found_dhuhr);
}

static void test_build_triggers_sorted(void) {
  printf("  build triggers sorted ascending...\n");
  Config cfg = test_config();
  struct PrayerTimes times = jakarta_times();
  PrayerCache cache = {0};

  cache_build_triggers(&cache, &cfg, &times, 0, "2026-03-22");

  for (int i = 1; i < cache.trigger_count; i++) {
    check_bool("sorted ascending", cache.triggers[i].minute >= cache.triggers[i - 1].minute);
  }
}

static void test_build_triggers_skips_disabled(void) {
  printf("  build triggers skips disabled prayers...\n");
  Config cfg = test_config();
  cfg.fajr.enabled = false;
  struct PrayerTimes times = jakarta_times();
  PrayerCache cache = {0};

  cache_build_triggers(&cache, &cfg, &times, 0, "2026-03-22");

  for (int i = 0; i < cache.trigger_count; i++) {
    check_bool("no fajr trigger", strcmp(cache.triggers[i].prayer, "Fajr") != 0);
  }
}

static void test_build_triggers_includes_reminders(void) {
  printf("  build triggers includes reminders...\n");
  Config cfg = test_config();
  struct PrayerTimes times = jakarta_times();
  PrayerCache cache = {0};

  // At minute 0, should include fajr reminders (30, 15, 5 min before)
  cache_build_triggers(&cache, &cfg, &times, 0, "2026-03-22");

  int fajr_reminder_count = 0;
  for (int i = 0; i < cache.trigger_count; i++) {
    if (strcmp(cache.triggers[i].prayer, "Fajr") == 0 && cache.triggers[i].minutes_before > 0) {
      fajr_reminder_count++;
    }
  }
  check_bool("fajr has 3 reminders", fajr_reminder_count == 3);
}

static void test_remove_trigger(void) {
  printf("  remove trigger...\n");
  PrayerCache cache = {0};
  strcpy(cache.date, "2026-03-22");
  cache.trigger_count = 3;
  strcpy(cache.triggers[0].prayer, "Fajr");
  cache.triggers[0].minute = 236;
  strcpy(cache.triggers[1].prayer, "Fajr");
  cache.triggers[1].minute = 251;
  strcpy(cache.triggers[2].prayer, "Dhuhr");
  cache.triggers[2].minute = 724;

  cache_remove_trigger(&cache, 0);

  check_bool("count decremented", cache.trigger_count == 2);
  check_bool("shifted correctly",
             strcmp(cache.triggers[0].prayer, "Fajr") == 0 && cache.triggers[0].minute == 251);
}

static void test_save_load_roundtrip(void) {
  printf("  save/load roundtrip...\n");

  // Redirect cache to a temp directory via XDG_CACHE_HOME
  char tmpdir[] = "/tmp/mt_cache_XXXXXX";
  if (!mkdtemp(tmpdir)) {
    fprintf(stderr, "FAIL [mkdtemp]\n");
    failed++;
    return;
  }
  setenv("XDG_CACHE_HOME", tmpdir, 1);
  cache_reset_path();
  check_bool("cache path starts in tmpdir", strncmp(cache_get_path(), tmpdir, strlen(tmpdir)) == 0);
  check_bool("cache path includes muslimtify dir",
             strstr(cache_get_path(), "/muslimtify/next_prayer.json") != NULL);

  {
    // Size cap: an oversize cache file must be refused by cache_load.
    // Write a valid cache first (creates the dir and gives cache_load
    // parseable JSON), then pad it past 1 MiB with trailing whitespace so the
    // only reason to reject it is the size cap, not a parse failure.
    PrayerCache seed = {0};
    strcpy(seed.date, "2026-03-22");
    check_bool("cache: initial save", cache_save(&seed) == 0);
    FILE *cf = fopen(cache_get_path(), "a");
    check_bool("cache: reopen for padding", cf != NULL);
    if (cf) {
      for (long i = 0; i < (1024L * 1024L) + 16L; i++)
        fputc(' ', cf);
      fclose(cf);
    }
    PrayerCache big;
    check_bool("cache: oversize file rejected", cache_load(&big) == -1);
  }

  // Build a cache
  PrayerCache original = {0};
  strcpy(original.date, "2026-03-22");
  original.trigger_count = 2;
  strcpy(original.triggers[0].prayer, "Fajr");
  original.triggers[0].minute = 266;
  original.triggers[0].minutes_before = 0;
  original.triggers[0].prayer_time = 4.4333;
  original.triggers[0].adhan_enabled = true;
  strcpy(original.triggers[0].adhan, "/tmp/fajr.mp3");
  strcpy(original.triggers[1].prayer, "Dhuhr");
  original.triggers[1].minute = 724;
  original.triggers[1].minutes_before = 0;
  original.triggers[1].prayer_time = 12.0667;

  // Save and reload
  int save_ok = cache_save(&original);
  check_bool("save succeeds", save_ok == 0);
  check_bool("cache file exists", platform_file_exists(cache_get_path()) == 1);

#ifndef _WIN32
  // Owner-only (0600): the cache mirrors config's fchmod hardening.
  struct stat cache_st;
  check_bool("cache: stat", stat(cache_get_path(), &cache_st) == 0);
  check_bool("cache: owner-only (0600)", (cache_st.st_mode & 077) == 0);
#endif

  PrayerCache loaded = {0};
  int load_ok = cache_load(&loaded);
  check_bool("load succeeds", load_ok == 0);
  check_bool("date matches", strcmp(loaded.date, "2026-03-22") == 0);
  check_bool("count matches", loaded.trigger_count == 2);
  check_bool("prayer[0] matches", strcmp(loaded.triggers[0].prayer, "Fajr") == 0);
  check_bool("minute[0] matches", loaded.triggers[0].minute == 266);
  check_bool("prayer[1] matches", strcmp(loaded.triggers[1].prayer, "Dhuhr") == 0);
  check_bool("prayer_time[1] close", fabs(loaded.triggers[1].prayer_time - 12.0667) < 0.01);
  check_bool("adhan[0] matches", strcmp(loaded.triggers[0].adhan, "/tmp/fajr.mp3") == 0);
  check_bool("adhan_enabled[0] matches", loaded.triggers[0].adhan_enabled == true);

  cache_invalidate();
  check_bool("cache file removed", platform_file_exists(cache_get_path()) == 0);

  char tmpdir2[] = "/tmp/mt_cache_reset_XXXXXX";
  if (!mkdtemp(tmpdir2)) {
    fprintf(stderr, "FAIL [mkdtemp reset]\n");
    failed++;
    unsetenv("XDG_CACHE_HOME");
    return;
  }
  setenv("XDG_CACHE_HOME", tmpdir2, 1);
  cache_reset_path();
  check_bool("cache path resets to new tmpdir",
             strncmp(cache_get_path(), tmpdir2, strlen(tmpdir2)) == 0);
  check_bool("cache path still includes muslimtify dir",
             strstr(cache_get_path(), "/muslimtify/next_prayer.json") != NULL);

  check_bool("save after reset succeeds", cache_save(&original) == 0);
  PrayerCache reloaded = {0};
  check_bool("load after reset succeeds", cache_load(&reloaded) == 0);
  check_bool("reset date matches", strcmp(reloaded.date, "2026-03-22") == 0);
  check_bool("reset count matches", reloaded.trigger_count == 2);
  check_bool("reset prayer matches", strcmp(reloaded.triggers[0].prayer, "Fajr") == 0);

  // Cleanup
  cache_invalidate();
  cache_reset_path();
  char dir1[PLATFORM_PATH_MAX];
  char dir2[PLATFORM_PATH_MAX];
  snprintf(dir1, sizeof(dir1), "%s/muslimtify", tmpdir);
  snprintf(dir2, sizeof(dir2), "%s/muslimtify", tmpdir2);
  (void)rmdir(dir1);
  (void)rmdir(tmpdir);
  (void)rmdir(dir2);
  (void)rmdir(tmpdir2);
  unsetenv("XDG_CACHE_HOME");
}

static void test_build_triggers_carries_adhan(void) {
  printf("  build triggers carries adhan...\n");
  Config cfg = test_config();
  strncpy(cfg.dhuhr.adhan, "/tmp/dhuhr.mp3", sizeof(cfg.dhuhr.adhan) - 1);
  cfg.dhuhr.adhan_enabled = true;
  struct PrayerTimes times = jakarta_times();
  PrayerCache cache = {0};

  cache_build_triggers(&cache, &cfg, &times, 0, "2026-03-22");

  bool found = false;
  for (int i = 0; i < cache.trigger_count; i++) {
    if (strcmp(cache.triggers[i].prayer, "Dhuhr") == 0 && cache.triggers[i].minutes_before == 0) {
      found = true;
      check_bool("trigger carries adhan path",
                 strcmp(cache.triggers[i].adhan, "/tmp/dhuhr.mp3") == 0);
      check_bool("trigger carries adhan_enabled", cache.triggers[i].adhan_enabled == true);
    }
  }
  check_bool("found dhuhr exact trigger", found);
}

int main(void) {
  printf("Running cache tests...\n");

  test_build_triggers_includes_future();
  test_build_triggers_sorted();
  test_build_triggers_skips_disabled();
  test_build_triggers_includes_reminders();
  test_remove_trigger();
  test_save_load_roundtrip();
  test_build_triggers_carries_adhan();

  printf("\nResults: %d passed, %d failed\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
