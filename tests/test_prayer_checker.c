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
// Values in decimal hours: fajr=04:26,
// dhuhr=12:04, asr=15:29, maghrib=18:17, isha=19:32
static struct PrayerTimes jakarta_times(void) {
  return (struct PrayerTimes){
      .fajr = 4.0 + 26.0 / 60.0,     // 4.4333
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

// -- next prayer tests -------------------------------------------------------

// Jakarta's times for the previous, same and next day. Real neighbours differ
// by a minute or so, which none of these cases depend on.
static void same_days(struct PrayerTimes days[3]) {
  for (int i = 0; i < 3; i++)
    days[i] = jakarta_times();
}

static NextPrayer next_at(const Config *cfg, const struct PrayerTimes days[3], int hour, int min) {
  struct tm now = make_time(hour, min);
  static const double no_shift[3] = {0.0, 0.0, 0.0};
  return prayer_next_from_days(cfg, &now, days, no_shift);
}

static void test_next_upcoming(void) {
  printf("  next upcoming...\n");
  Config cfg = test_config();
  struct PrayerTimes days[3];
  same_days(days);

  // At 03:00, next enabled prayer is fajr (04:26)
  NextPrayer next = next_at(&cfg, days, 3, 0);
  check_bool("next@03:00 is fajr", next.type == PRAYER_FAJR && next.day_delta == 0);
  check_bool("next@03:00 ~86min", next.minutes_until > 80 && next.minutes_until < 92);

  // At 13:00, next enabled is asr (15:29)
  next = next_at(&cfg, days, 13, 0);
  check_bool("next@13:00 is asr", next.type == PRAYER_ASR && next.day_delta == 0);
  check_bool("next@13:00 ~149min", next.minutes_until > 140 && next.minutes_until < 155);
}

static void test_next_wraps_to_tomorrow(void) {
  printf("  next wraps to tomorrow...\n");
  Config cfg = test_config();
  struct PrayerTimes days[3];
  same_days(days);

  // At 20:00, all today's prayers have passed.
  // Next should be fajr (04:26 tomorrow) ≈ 506 minutes away.
  NextPrayer next = next_at(&cfg, days, 20, 0);
  check_bool("next@20:00 is fajr", next.type == PRAYER_FAJR);
  check_bool("next@20:00 from tomorrow", next.day_delta == 1);
  check_bool("next@20:00 ~506min", next.minutes_until > 500 && next.minutes_until < 515);
}

static void test_next_all_disabled(void) {
  printf("  next all disabled...\n");
  Config cfg = test_config();
  struct PrayerTimes days[3];
  same_days(days);

  cfg.fajr.enabled = false;
  cfg.dhuhr.enabled = false;
  cfg.asr.enabled = false;
  cfg.maghrib.enabled = false;
  cfg.isha.enabled = false;

  NextPrayer next = next_at(&cfg, days, 10, 0);
  check_bool("next all disabled", next.type == PRAYER_NONE);
}

static void test_next_skips_disabled(void) {
  printf("  next skips disabled...\n");
  Config cfg = test_config();
  struct PrayerTimes days[3];
  same_days(days);

  // Disable dhuhr, at 11:00 next should be asr (not dhuhr)
  cfg.dhuhr.enabled = false;
  NextPrayer next = next_at(&cfg, days, 11, 0);
  check_bool("next skips disabled dhuhr", next.type == PRAYER_ASR);
}

// Reykjavik in April: isha falls just after midnight. At 00:05 the previous
// day's isha, at 00:06, is still ahead, and it only exists in the previous
// day's times. A search over the same day's times alone reported fajr here,
// and with isha as the only enabled prayer it reported nothing at all.
static void test_next_spill_from_yesterday(void) {
  printf("  next spill from yesterday...\n");
  Config cfg = test_config();
  struct PrayerTimes days[3];
  same_days(days);
  days[0].isha = 24.0 + 6.0 / 60.0;  // previous day's isha, at 00:06 today
  days[1].isha = 24.0 + 16.0 / 60.0; // same day's isha, at 00:16 tomorrow
  days[2].isha = 24.0 + 26.0 / 60.0;

  NextPrayer next = next_at(&cfg, days, 0, 5);
  check_bool("spill@00:05 is isha", next.type == PRAYER_ISHA);
  check_bool("spill@00:05 from previous day", next.day_delta == -1);
  check_bool("spill@00:05 time is previous day's", next.time == days[0].isha);
  check_bool("spill@00:05 ~1min", next.minutes_until >= 1 && next.minutes_until <= 2);

  Config only_isha = cfg;
  only_isha.fajr.enabled = false;
  only_isha.dhuhr.enabled = false;
  only_isha.asr.enabled = false;
  only_isha.maghrib.enabled = false;
  next = next_at(&only_isha, days, 0, 5);
  check_bool("spill only isha found", next.type == PRAYER_ISHA && next.day_delta == -1);

  // Once the previous day's isha has passed, the same day's isha is next, a day away.
  next = next_at(&only_isha, days, 0, 10);
  check_bool("spill@00:10 is same day's isha", next.type == PRAYER_ISHA && next.day_delta == 0);
  check_bool("spill@00:10 ~1446min", next.minutes_until >= 1445 && next.minutes_until <= 1447);
}

// A negative offset can move fajr before midnight. A prayer that has already
// passed must never be picked with a negative countdown.
static void test_next_before_midnight(void) {
  printf("  next before midnight...\n");
  Config cfg = test_config();
  struct PrayerTimes days[3];
  same_days(days);
  days[1].fajr = -0.2; // same day's fajr, at 23:48 on the previous day
  days[2].fajr = -0.2; // next day's fajr, at 23:48 today

  // 23:30: every prayer of the day has passed, and the next day's fajr is 18 minutes away.
  NextPrayer next = next_at(&cfg, days, 23, 30);
  check_bool("before midnight@23:30 is fajr", next.type == PRAYER_FAJR && next.day_delta == 1);
  check_bool("before midnight@23:30 ~18min", next.minutes_until >= 17 && next.minutes_until <= 19);

  // 23:59: that fajr has passed too, so the next is the next day's dhuhr.
  next = next_at(&cfg, days, 23, 59);
  check_bool("before midnight@23:59 is next day's dhuhr",
             next.type == PRAYER_DHUHR && next.day_delta == 1);
  check_bool("before midnight@23:59 countdown not negative", next.minutes_until >= 0);
}

// A DST change between now's date and the next day. With offsets {0, 0, 1}
// the next day runs an hour ahead, so a prayer at 04:26 on its wall clock is
// an hour sooner than the wall-clock difference says. {1, 1, 0} is the mirror.
static void test_next_across_offset_change(void) {
  printf("  next across offset change...\n");
  Config cfg = test_config();
  struct PrayerTimes days[3];
  same_days(days);
  struct tm now = make_time(20, 0);

  // 20:00 to 04:26 is 506 wall-clock minutes.
  const double spring[3] = {0.0, 0.0, 1.0};
  NextPrayer next = prayer_next_from_days(&cfg, &now, days, spring);
  check_bool("spring fajr from next day", next.type == PRAYER_FAJR && next.day_delta == 1);
  check_bool("spring countdown 446min", next.minutes_until == 446);
  check_bool("spring time stays wall clock", next.time == days[2].fajr);

  const double autumn[3] = {1.0, 1.0, 0.0};
  next = prayer_next_from_days(&cfg, &now, days, autumn);
  check_bool("autumn fajr from next day", next.type == PRAYER_FAJR && next.day_delta == 1);
  check_bool("autumn countdown 566min", next.minutes_until == 566);
  check_bool("autumn time stays wall clock", next.time == days[2].fajr);

  // The choice must use the corrected countdown. Now's date has isha at
  // 23:40, 220 minutes away. The next day's fajr is at 01:00 on its wall
  // clock, 300 wall-clock minutes away but 180 real minutes with a two hour
  // offset jump, so it is the nearer one. Without the jump isha wins.
  days[1].isha = 23.0 + 40.0 / 60.0;
  days[2].fajr = 1.0;
  const double jump[3] = {0.0, 0.0, 2.0};
  next = prayer_next_from_days(&cfg, &now, days, jump);
  check_bool("jump picks next day's fajr", next.type == PRAYER_FAJR && next.day_delta == 1);
  check_bool("jump countdown 180min", next.minutes_until == 180);
  const double flat[3] = {0.0, 0.0, 0.0};
  next = prayer_next_from_days(&cfg, &now, days, flat);
  check_bool("no jump picks isha", next.type == PRAYER_ISHA && next.day_delta == 0);
}

// Europe/London at 22:00, after isha, the evening before a clock change. The
// next prayer is the following day's fajr. The expected countdown is taken
// from UTC instants with the zone's known offsets, GMT +0 and BST +1, not
// from effective_tz_offset.
static void check_london_after_isha(const char *label, int y, int m, int d, double now_off,
                                    double next_off) {
  Config cfg = config_default();
  cfg.latitude = 51.5074;
  cfg.longitude = -0.1278;
  strncpy(cfg.timezone, "Europe/London", sizeof(cfg.timezone) - 1);
  cfg.timezone_offset = 0.0;
  cfg.auto_detect = false;

  long serial = mt_days_from_civil(y, m, d);
  int ny, nm, nd;
  mt_civil_from_days(serial + 1, &ny, &nm, &nd);
  struct PrayerTimes today = prayer_times_for_config(&cfg, y, m, d);
  struct PrayerTimes tomorrow = prayer_times_for_config(&cfg, ny, nm, nd);

  struct tm now = {0};
  now.tm_year = y - 1900;
  now.tm_mon = m - 1;
  now.tm_mday = d;
  now.tm_hour = 22;

  // Minutes since the epoch, UTC. The prayer is rounded up like the countdown.
  long now_utc = serial * 1440L + 22 * 60 - lround(now_off * 60.0);
  long fajr_utc = (serial + 1) * 1440L + (long)ceil(tomorrow.fajr * 60.0) - lround(next_off * 60.0);
  long expected = fajr_utc - now_utc;

  NextPrayer next = prayer_get_next(&cfg, &now, &today);
  char name[96];
  snprintf(name, sizeof(name), "%s isha before 22:00", label);
  check_bool(name, today.isha < 22.0);
  snprintf(name, sizeof(name), "%s next is tomorrow's fajr", label);
  check_bool(name, next.type == PRAYER_FAJR && next.day_delta == 1);
  snprintf(name, sizeof(name), "%s countdown %ld min (got %d)", label, expected,
           next.minutes_until);
  check_bool(name, next.minutes_until == expected);
  snprintf(name, sizeof(name), "%s time stays wall clock", label);
  check_bool(name, next.time == tomorrow.fajr);
}

static void test_next_london_dst(void) {
  printf("  next across London DST change...\n");
  // BST starts at 01:00 UTC on 2026-03-29 and ends at 01:00 UTC on 2026-10-25.
  check_london_after_isha("london spring", 2026, 3, 28, 0.0, 1.0);
  check_london_after_isha("london autumn", 2026, 10, 24, 1.0, 0.0);
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
  test_next_spill_from_yesterday();
  test_next_before_midnight();
  test_next_across_offset_change();
  test_next_london_dst();
  test_prayer_get_name();
  test_prayer_is_enabled();
  test_prayer_get_time();

  printf("\nResults: %d passed, %d failed\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
