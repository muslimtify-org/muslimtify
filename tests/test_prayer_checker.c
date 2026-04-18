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

// -- prayer_check_current tests ----------------------------------------------

static void test_exact_match(void) {
  printf("  exact match...\n");
  Config cfg = test_config();
  struct PrayerTimes times = jakarta_times();

  // Current time = fajr time (04:26)
  struct tm now = make_time(4, 26);
  PrayerMatch m = prayer_check_current(&cfg, &now, &times);
  check_bool("exact fajr type", m.type == PRAYER_FAJR);
  check_bool("exact fajr min_before", m.minutes_before == 0);

  // Current time = dhuhr time (12:04)
  now = make_time(12, 4);
  m = prayer_check_current(&cfg, &now, &times);
  check_bool("exact dhuhr type", m.type == PRAYER_DHUHR);
  check_bool("exact dhuhr min_before", m.minutes_before == 0);

  // Current time = isha time (19:32)
  now = make_time(19, 32);
  m = prayer_check_current(&cfg, &now, &times);
  check_bool("exact isha type", m.type == PRAYER_ISHA);
  check_bool("exact isha min_before", m.minutes_before == 0);
}

static void test_reminder_match(void) {
  printf("  reminder match...\n");
  Config cfg = test_config();
  struct PrayerTimes times = jakarta_times();

  // Default config has reminders [30, 15, 5] for fajr.
  // Fajr is at minute 266 (04:26). 30 min before = minute 236 (03:56).
  struct tm now = make_time(3, 56);
  PrayerMatch m = prayer_check_current(&cfg, &now, &times);
  check_bool("reminder 30 type", m.type == PRAYER_FAJR);
  check_bool("reminder 30 offset", m.minutes_before == 30);

  // 15 min before fajr = minute 251 (04:11)
  now = make_time(4, 11);
  m = prayer_check_current(&cfg, &now, &times);
  check_bool("reminder 15 type", m.type == PRAYER_FAJR);
  check_bool("reminder 15 offset", m.minutes_before == 15);

  // 5 min before fajr = minute 261 (04:21)
  now = make_time(4, 21);
  m = prayer_check_current(&cfg, &now, &times);
  check_bool("reminder 5 type", m.type == PRAYER_FAJR);
  check_bool("reminder 5 offset", m.minutes_before == 5);
}

static void test_no_match(void) {
  printf("  no match...\n");
  Config cfg = test_config();
  struct PrayerTimes times = jakarta_times();

  // 10:00 — not near any prayer or reminder
  struct tm now = make_time(10, 0);
  PrayerMatch m = prayer_check_current(&cfg, &now, &times);
  check_bool("no match type", m.type == PRAYER_NONE);
}

static void test_disabled_skipped(void) {
  printf("  disabled prayer skipped...\n");
  Config cfg = test_config();
  struct PrayerTimes times = jakarta_times();

  // Disable fajr, then check at fajr exact time
  cfg.fajr.enabled = false;
  struct tm now = make_time(4, 26);
  PrayerMatch m = prayer_check_current(&cfg, &now, &times);
  check_bool("disabled fajr skipped", m.type == PRAYER_NONE);

  // Sunrise is disabled by default
  now = make_time(5, 46);
  m = prayer_check_current(&cfg, &now, &times);
  check_bool("disabled sunrise skipped", m.type == PRAYER_NONE);
}

static void test_midnight_crossover(void) {
  printf("  midnight crossover...\n");

  // Construct a scenario where fajr is at 00:20 (0.333 hours)
  // with a 30-min reminder → reminder at 23:50 (minute 1430)
  Config cfg = test_config();
  struct PrayerTimes times = jakarta_times();
  times.fajr = 20.0 / 60.0; // 00:20
  cfg.fajr.reminders[0] = 30;
  cfg.fajr.reminder_count = 1;

  struct tm now = make_time(23, 50);
  PrayerMatch m = prayer_check_current(&cfg, &now, &times);
  check_bool("midnight cross type", m.type == PRAYER_FAJR);
  check_bool("midnight cross offset", m.minutes_before == 30);
}

static void test_all_disabled(void) {
  printf("  all disabled...\n");
  Config cfg = test_config();
  struct PrayerTimes times = jakarta_times();

  cfg.fajr.enabled = false;
  cfg.sunrise.enabled = false;
  cfg.dhuha.enabled = false;
  cfg.dhuhr.enabled = false;
  cfg.asr.enabled = false;
  cfg.maghrib.enabled = false;
  cfg.isha.enabled = false;

  // Check at dhuhr exact time — should still be NONE
  struct tm now = make_time(12, 4);
  PrayerMatch m = prayer_check_current(&cfg, &now, &times);
  check_bool("all disabled none", m.type == PRAYER_NONE);
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

  test_exact_match();
  test_reminder_match();
  test_no_match();
  test_disabled_skipped();
  test_midnight_crossover();
  test_all_disabled();
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
