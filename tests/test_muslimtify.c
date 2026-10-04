#define _GNU_SOURCE
#include "config.h"
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

// -- muslimtify_open ----------------------------------------------------------

// Must run first: config_get_path resolves XDG_CONFIG_HOME once per process.
static void test_open_from_disk(void) {
  printf("  open from disk...\n");

  // A fresh config is auto_detect at 0,0, which has no usable location.
  Muslimtify *mt = (Muslimtify *)tmpdir;
  check_bool("fresh config has no location", muslimtify_open(&mt) == MUSLIMTIFY_ERR_NO_LOCATION);
  check_bool("handle is NULL on error", mt == NULL);

  Config cfg = jakarta_config();
  check_bool("save jakarta config", config_save(&cfg) == 0);
  check_bool("open succeeds with a location", muslimtify_open(&mt) == MUSLIMTIFY_OK);
  check_bool("handle is set", mt != NULL);
  muslimtify_close(mt);

  check_bool("open rejects NULL out", muslimtify_open(NULL) == MUSLIMTIFY_ERR_INVALID_ARG);
  muslimtify_close(NULL);
}

static void test_open_config(void) {
  printf("  open from memory...\n");

  Config cfg = jakarta_config();
  Muslimtify *mt = NULL;
  check_bool("NULL config", muslimtify_open_config(NULL, &mt) == MUSLIMTIFY_ERR_INVALID_ARG);
  check_bool("NULL out", muslimtify_open_config(&cfg, NULL) == MUSLIMTIFY_ERR_INVALID_ARG);

  Config bad = jakarta_config();
  bad.latitude = NAN;
  check_bool("NaN latitude has no location",
             muslimtify_open_config(&bad, &mt) == MUSLIMTIFY_ERR_NO_LOCATION);
  check_bool("handle NULL for NaN latitude", mt == NULL);

  check_bool("24 by default", muslimtify_open_config(&cfg, &mt) == MUSLIMTIFY_OK &&
                                  muslimtify_time_format(mt) == 24);
  muslimtify_close(mt);
  cfg.time_format = 12;
  check_bool("12 when configured", muslimtify_open_config(&cfg, &mt) == MUSLIMTIFY_OK &&
                                       muslimtify_time_format(mt) == 12);
  muslimtify_close(mt);
  check_bool("NULL handle is 24", muslimtify_time_format(NULL) == 24);
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

  for (int e = MUSLIMTIFY_OK; e <= MUSLIMTIFY_ERR_RANGE_TOO_LONG; e++) {
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
  test_open_config();
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
