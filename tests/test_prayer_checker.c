#include "config.h"
#include "prayer_checker.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

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

// Helper: build a struct tm for a given hour:minute
static struct tm make_time(int hour, int min) {
  struct tm t = {0};
  t.tm_hour = hour;
  t.tm_min = min;
  return t;
}

// Known prayer times for Jakarta, used across all tests.
// Values in decimal hours: fajr=04:26, sunrise=05:46, dhuha=06:14,
// dhuhr=12:04, asr=15:29, maghrib=18:17, isha=19:32
static struct PrayerTimes jakarta_times(void) {
  return (struct PrayerTimes){
      .fajr = 4.0 + 26.0 / 60.0,     // 4.4333
      .sunrise = 5.0 + 46.0 / 60.0,  // 5.7667
      .dhuha = 6.0 + 14.0 / 60.0,    // 6.2333
      .dhuhr = 12.0 + 4.0 / 60.0,    // 12.0667
      .asr = 15.0 + 29.0 / 60.0,     // 15.4833
      .maghrib = 18.0 + 17.0 / 60.0, // 18.2833
      .isha = 19.0 + 32.0 / 60.0,    // 19.5333
  };
}

// Default config with Jakarta location, all standard prayers enabled
static Config test_config(void) {
  Config cfg = config_default();
  cfg.latitude = -6.2088;
  cfg.longitude = 106.8456;
  cfg.timezone_offset = 7.0;
  cfg.auto_detect = false;
  return cfg;
}

// -- prayer_get_next tests ---------------------------------------------------

static void test_next_upcoming(void) {
  printf("  next upcoming...\n");
  Config cfg = test_config();
  struct PrayerTimes times = jakarta_times();

  int mins = 0;

  // At 03:00, next enabled prayer is fajr (04:26)
  struct tm now = make_time(3, 0);
  PrayerType next = prayer_get_next(&cfg, &now, &times, &mins);
  check_bool("next@03:00 is fajr", next == PRAYER_FAJR);
  check_bool("next@03:00 ~86min", mins > 80 && mins < 92);

  // At 13:00, next enabled is asr (15:29)
  now = make_time(13, 0);
  next = prayer_get_next(&cfg, &now, &times, &mins);
  check_bool("next@13:00 is asr", next == PRAYER_ASR);
  check_bool("next@13:00 ~149min", mins > 140 && mins < 155);
}

static void test_next_wraps_to_tomorrow(void) {
  printf("  next wraps to tomorrow...\n");
  Config cfg = test_config();
  struct PrayerTimes times = jakarta_times();

  int mins = 0;

  // At 20:00, all today's prayers have passed.
  // Next should be fajr (04:26 tomorrow) ≈ 506 minutes away.
  struct tm now = make_time(20, 0);
  PrayerType next = prayer_get_next(&cfg, &now, &times, &mins);
  check_bool("next@20:00 is fajr", next == PRAYER_FAJR);
  check_bool("next@20:00 ~506min", mins > 500 && mins < 515);
}

static void test_next_all_disabled(void) {
  printf("  next all disabled...\n");
  Config cfg = test_config();
  struct PrayerTimes times = jakarta_times();

  cfg.fajr.enabled = false;
  cfg.sunrise.enabled = false;
  cfg.dhuha.enabled = false;
  cfg.dhuhr.enabled = false;
  cfg.asr.enabled = false;
  cfg.maghrib.enabled = false;
  cfg.isha.enabled = false;

  int mins = 0;
  struct tm now = make_time(10, 0);
  PrayerType next = prayer_get_next(&cfg, &now, &times, &mins);
  check_bool("next all disabled", next == PRAYER_NONE);
}

static void test_next_skips_disabled(void) {
  printf("  next skips disabled...\n");
  Config cfg = test_config();
  struct PrayerTimes times = jakarta_times();

  // Disable dhuhr, at 11:00 next should be asr (not dhuhr)
  cfg.dhuhr.enabled = false;
  int mins = 0;
  struct tm now = make_time(11, 0);
  PrayerType next = prayer_get_next(&cfg, &now, &times, &mins);
  check_bool("next skips disabled dhuhr", next == PRAYER_ASR);
}

// -- helper function tests ---------------------------------------------------

static void test_prayer_get_name(void) {
  printf("  prayer_get_name...\n");
  check_bool("name fajr", strcmp(prayer_get_name(PRAYER_FAJR), "Fajr") == 0);
  check_bool("name isha", strcmp(prayer_get_name(PRAYER_ISHA), "Isha") == 0);
  check_bool("name none", strcmp(prayer_get_name(PRAYER_NONE), "Unknown") == 0);
}

static void test_prayer_is_enabled(void) {
  printf("  prayer_is_enabled...\n");
  Config cfg = test_config();
  check_bool("fajr enabled", prayer_is_enabled(&cfg, PRAYER_FAJR));
  check_bool("sunrise disabled", !prayer_is_enabled(&cfg, PRAYER_SUNRISE));
  check_bool("dhuha disabled", !prayer_is_enabled(&cfg, PRAYER_DHUHA));
  check_bool("dhuhr enabled", prayer_is_enabled(&cfg, PRAYER_DHUHR));
}

static void test_prayer_get_time(void) {
  printf("  prayer_get_time...\n");
  struct PrayerTimes times = jakarta_times();
  check_bool("get fajr time", fabs(prayer_get_time(&times, PRAYER_FAJR) - times.fajr) < 0.001);
  check_bool("get isha time", fabs(prayer_get_time(&times, PRAYER_ISHA) - times.isha) < 0.001);
  check_bool("get none time", prayer_get_time(&times, PRAYER_NONE) == 0.0);
}

// -- main ---------------------------------------------------------------------

int main(void) {
  printf("Running prayer checker tests...\n");

  test_next_upcoming();
  test_next_wraps_to_tomorrow();
  test_next_all_disabled();
  test_next_skips_disabled();
  test_prayer_get_name();
  test_prayer_is_enabled();
  test_prayer_get_time();

  printf("\nResults: %d passed, %d failed\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
