#define _GNU_SOURCE
#include "cache.h"
#include "check_cycle.h"
#include "config.h"
#include "platform.h"
#include "prayer_checker.h"
#include <math.h>
#include <stdbool.h>
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
  // Use the stored fixed offset directly rather than a named zone lookup.
  // config_default leaves "UTC" here, which cache_build_triggers now resolves
  // for real when it fetches D-1/D+1, so an untouched name would silently
  // outrank timezone_offset and shift every neighbouring day by -7 hours.
  cfg.timezone[0] = '\0';
  return cfg;
}

// Reykjavik: high enough latitude that isha regularly crosses midnight (its
// decimal hour goes to or past 24.0), which is exactly the day-assembly
// behaviour these tests pin.
static Config reykjavik_config(void) {
  Config cfg = config_default();
  cfg.latitude = 64.1466;
  cfg.longitude = -21.9426;
  cfg.timezone_offset = 0.0;
  cfg.auto_detect = false;
  cfg.timezone[0] = '\0';
  strcpy(cfg.calculation_method, "mwl");
  return cfg;
}

// Scans 2026 for the first day whose own isha instant is at or past midnight,
// i.e. prayer_times_for_config reports isha >= 24.0 decimal hours. Returns
// true and fills *year/*month/*day on success. Scanning rather than pasting a
// date keeps the test tied to the actual astronomical condition instead of a
// value that could go stale if the calculation changes.
static bool find_isha_spill_day(const Config *cfg, int *year, int *month, int *day) {
  for (long d = mt_days_from_civil(2026, 1, 1); d <= mt_days_from_civil(2026, 12, 31); d++) {
    int y, m, dd;
    mt_civil_from_days(d, &y, &m, &dd);
    struct PrayerTimes times = prayer_times_for_config(cfg, y, m, dd);
    if (isfinite(times.isha) && times.isha >= 24.0) {
      *year = y;
      *month = m;
      *day = dd;
      return true;
    }
  }
  return false;
}

// Same rounding cache_build_triggers uses to turn a decimal-hour prayer time,
// shifted by the day offset it was sourced from, into an absolute minute of
// the day being built.
static int instant_minute(double prayer_time, int day_delta) {
  return (int)ceil((prayer_time + 24.0 * day_delta) * 60.0);
}

// Mutation record for the five tests below (Task 4 of the trigger-day-assembly
// plan). Each mutant was applied to src/core/cache.c, built, run against
// `ctest --test-dir build -R cache --output-on-failure`, and then reverted
// with `git checkout -- src/core/cache.c` before the next one, verified clean
// with `git status --porcelain`. cache.c itself carries none of these changes.
//
// Mutant one: in cache_build_triggers, start day_delta at 0 instead of -1, so
// nothing from D-1 spills forward into the day being built. This failed the
// suite, exit status 8. Verbatim output (121 per-day diagnostic lines from
// test_adhan_survives_capacity_all_year omitted here for length, each of the
// form "missing adhan for Isha at 2026-04-08 minute 6"):
//   cache: file too large (1048656 bytes), refusing to load
//   no matching ']'
//   FAIL [spilled isha adhan present on the next day at the rounded minute]
//   FAIL [every in-day prayer occurrence keeps its adhan across the year]
//   Results: 176 passed, 2 failed
// Caught by test_isha_adhan_fires_on_day_it_occurs and
// test_adhan_survives_capacity_all_year. The "cache: file too large" and "no
// matching ']'" lines are pre-existing stderr noise from a stale cache file
// on this machine, unrelated to the mutant.
//
// Mutant two: in the pass == 0 branch, drop the
// `instant_min >= 0 && instant_min < 1440` guard and instead wrap instant_min
// into [0, 1440) before using it, so a prayer whose instant falls outside the
// day is scheduled on the day anyway. This recreates the original 24-hour-early
// adhan defect. This failed the suite, exit status 8. Verbatim output:
//   cache: file too large (1048656 bytes), refusing to load
//   no matching ']'
//   FAIL [no isha adhan on the day whose own isha has not happened yet]
//   FAIL [no prayer instant scheduled twice across three consecutive days]
//   cache: capacity reached, dropped Isha reminder (50 min before)
//   Results: 226 passed, 2 failed
// Caught by test_isha_no_early_adhan_on_spill_day and
// test_no_double_scheduling_across_spill_days.
//
// Mutant three: restore the old reminder wrap, replacing
// `if (reminder_min < 0 || reminder_min >= 1440) continue;` with
// `if (reminder_min < 0) reminder_min += 24 * 60; if (reminder_min >= 1440)
// continue;`, the double-notification hazard. This failed the suite, exit
// status 8. Verbatim output:
//   FAIL [fajr has 3 reminders]
//   cache: file too large (1048656 bytes), refusing to load
//   no matching ']'
//   FAIL [no prayer instant scheduled twice across three consecutive days]
//   cache: capacity reached, dropped Fajr reminder (50 min before)
//   Results: 242 passed, 2 failed
// Caught by test_no_double_scheduling_across_spill_days and by the
// pre-existing test_build_triggers_includes_reminders case labelled
// "fajr has 3 reminders".
// Gap found: test_reminders_land_on_day_they_occur did not catch this mutant,
// even though its own comment names this exact defect. That test only
// exercises an own-day isha whose instant is at or past 1440, the forward
// spill case, so its reminder_min values only ever land in the reminder_min
// >= 1440 branch, which mutant three does not touch. It never drives
// reminder_min negative, so the reminder_min < 0 wrap this mutant restores is
// never exercised by that test. The suite as a whole still failed, so the
// regression would be caught, just not by the test written for it.
//
// After each restore, `git status --porcelain` was empty and
// `ctest --test-dir build -R cache --output-on-failure` passed again before
// the next mutant was applied.

// Goal 2: the 24-hour-early adhan is gone. On the day whose own isha spills
// past midnight, that day must not carry an isha adhan trigger, since the
// prayer has not actually happened yet on that day.
static void test_isha_no_early_adhan_on_spill_day(void) {
  printf("  isha spilling past midnight fires no early adhan...\n");
  Config cfg = reykjavik_config();
  int year, month, day;
  check_bool("found a day whose own isha spills", find_isha_spill_day(&cfg, &year, &month, &day));

  char date_str[16];
  snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d", year, month, day);
  struct PrayerTimes times = prayer_times_for_config(&cfg, year, month, day);
  PrayerCache cache = {0};
  cache_build_triggers(&cache, &cfg, &times, 0, date_str);

  bool found_isha_exact = false;
  for (int i = 0; i < cache.trigger_count; i++) {
    if (strcmp(cache.triggers[i].prayer, "Isha") == 0 && cache.triggers[i].minutes_before == 0) {
      found_isha_exact = true;
    }
  }
  check_bool("no isha adhan on the day whose own isha has not happened yet", !found_isha_exact);
}

// Goal 1: the adhan fires on the right day. The following day must carry the
// spilled isha's adhan, at the minute the spilled time rounds to.
static void test_isha_adhan_fires_on_day_it_occurs(void) {
  printf("  spilled isha adhan fires on the day it actually occurs...\n");
  Config cfg = reykjavik_config();
  int year, month, day;
  check_bool("found a day whose own isha spills", find_isha_spill_day(&cfg, &year, &month, &day));

  struct PrayerTimes times = prayer_times_for_config(&cfg, year, month, day);
  long day_num = mt_days_from_civil(year, month, day);
  int ny, nm, nd;
  mt_civil_from_days(day_num + 1, &ny, &nm, &nd);
  char next_date[32];
  snprintf(next_date, sizeof(next_date), "%04d-%02d-%02d", ny, nm, nd);
  struct PrayerTimes next_times = prayer_times_for_config(&cfg, ny, nm, nd);
  PrayerCache cache = {0};
  cache_build_triggers(&cache, &cfg, &next_times, 0, next_date);

  int expected_minute = instant_minute(times.isha, -1);
  bool found = false;
  for (int i = 0; i < cache.trigger_count; i++) {
    if (strcmp(cache.triggers[i].prayer, "Isha") == 0 && cache.triggers[i].minutes_before == 0 &&
        cache.triggers[i].minute == expected_minute) {
      found = true;
    }
  }
  check_bool("spilled isha adhan present on the next day at the rounded minute", found);
}

// Goal 4: no double scheduling. Assembling three consecutive days around a
// spill must never place the same prayer instant into two different days'
// trigger sets. Comparing on prayer name alone would fail on correct code,
// since the same name legitimately recurs across days, so the identity is
// the pair of name and absolute instant, reconstructed from each day's own
// epoch plus the trigger's minute.
static void test_no_double_scheduling_across_spill_days(void) {
  printf("  no prayer instant is scheduled on two different days...\n");
  Config cfg = reykjavik_config();
  int year, month, day;
  check_bool("found a day whose own isha spills", find_isha_spill_day(&cfg, &year, &month, &day));
  long day0 = mt_days_from_civil(year, month, day);

  typedef struct {
    char prayer[16];
    long instant;
  } SeenTrigger;
  SeenTrigger seen[3 * MAX_TRIGGERS];
  int seen_count = 0;
  bool dup_found = false;

  for (int offset = 0; offset < 3; offset++) {
    int cy, cm, cd;
    mt_civil_from_days(day0 + offset, &cy, &cm, &cd);
    char date_str[32];
    snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d", cy, cm, cd);
    struct PrayerTimes times = prayer_times_for_config(&cfg, cy, cm, cd);
    PrayerCache cache = {0};
    cache_build_triggers(&cache, &cfg, &times, 0, date_str);

    for (int i = 0; i < cache.trigger_count; i++) {
      long instant = (day0 + offset) * 1440L + cache.triggers[i].minute;
      for (int k = 0; k < seen_count; k++) {
        if (seen[k].instant == instant && strcmp(seen[k].prayer, cache.triggers[i].prayer) == 0) {
          dup_found = true;
        }
      }
      strcpy(seen[seen_count].prayer, cache.triggers[i].prayer);
      seen[seen_count].instant = instant;
      seen_count++;
    }
  }
  check_bool("no prayer instant scheduled twice across three consecutive days", !dup_found);
}

// Goal 3: reminders land on the day they occur. On the day whose own isha
// spills, a reminder still in the evening (before midnight) belongs to that
// day's own set, while a reminder that itself falls past midnight does not,
// since it was moved to the following day instead.
static void test_reminders_land_on_day_they_occur(void) {
  printf("  reminders land on the day they actually occur...\n");
  Config cfg = reykjavik_config();
  int year, month, day;
  check_bool("found a day whose own isha spills", find_isha_spill_day(&cfg, &year, &month, &day));

  char date_str[16];
  snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d", year, month, day);
  struct PrayerTimes times = prayer_times_for_config(&cfg, year, month, day);
  PrayerCache cache = {0};
  cache_build_triggers(&cache, &cfg, &times, 0, date_str);

  long day_num = mt_days_from_civil(year, month, day);
  int ny, nm, nd;
  mt_civil_from_days(day_num + 1, &ny, &nm, &nd);
  char next_date[32];
  snprintf(next_date, sizeof(next_date), "%04d-%02d-%02d", ny, nm, nd);
  struct PrayerTimes next_times = prayer_times_for_config(&cfg, ny, nm, nd);
  PrayerCache next_cache = {0};
  cache_build_triggers(&next_cache, &cfg, &next_times, 0, next_date);

  int instant_min = instant_minute(times.isha, 0);
  int next_day_instant_min = instant_minute(times.isha, -1);
  const PrayerConfig *pcfg = prayer_get_config(&cfg, PRAYER_ISHA);
  bool checked_evening = false;
  bool checked_past_midnight = false;

  for (int j = 0; j < pcfg->reminder_count; j++) {
    int reminder_min = instant_min - pcfg->reminders[j];
    bool found = false;
    for (int i = 0; i < cache.trigger_count; i++) {
      if (strcmp(cache.triggers[i].prayer, "Isha") == 0 &&
          cache.triggers[i].minutes_before == pcfg->reminders[j] &&
          cache.triggers[i].minute == reminder_min) {
        found = true;
      }
    }
    if (reminder_min < 1440) {
      check_bool("evening isha reminder present on its own day", found);
      checked_evening = true;
    } else {
      check_bool("post-midnight isha reminder absent from the spilling day", !found);
      checked_past_midnight = true;

      // Half of goal 3 is checking the reminder is gone from the spilling
      // day, the other half is that it actually landed on the following
      // day, at its offset from that day's own midnight. 04-08 legitimately
      // carries isha entries for two different prayers, so match on the
      // pair of minutes_before and minute rather than on the prayer name
      // alone.
      int next_day_reminder_min = next_day_instant_min - pcfg->reminders[j];
      bool found_on_next_day = false;
      for (int i = 0; i < next_cache.trigger_count; i++) {
        if (strcmp(next_cache.triggers[i].prayer, "Isha") == 0 &&
            next_cache.triggers[i].minutes_before == pcfg->reminders[j] &&
            next_cache.triggers[i].minute == next_day_reminder_min) {
          found_on_next_day = true;
        }
      }
      check_bool("post-midnight isha reminder present on the following day at its own minute",
                 found_on_next_day);
    }
  }
  check_bool("the spill day has both an evening and a post-midnight isha reminder",
             checked_evening && checked_past_midnight);
}

// Goal 6, as amended: the capacity guard may drop a reminder, but it must
// never cost a prayer its adhan. With every prayer configured for
// MAX_REMINDERS reminders, walk a full Reykjavik year and, for every day,
// independently reconstruct which prayer occurrences actually fall inside
// it (from the same D-1/D/D+1 sources cache_build_triggers uses), then
// require each of those occurrences to have its minutes_before == 0 trigger
// present. The count itself is allowed to hit MAX_TRIGGERS, so it is
// deliberately not asserted here.
static void test_adhan_survives_capacity_all_year(void) {
  printf("  every adhan survives the capacity guard across a full year...\n");
  Config cfg = reykjavik_config();
  int reminders[MAX_REMINDERS];
  for (int i = 0; i < MAX_REMINDERS; i++) {
    reminders[i] = 5 + i * 5;
  }
  PrayerConfig *prayer_cfgs[] = {&cfg.fajr, &cfg.dhuhr, &cfg.asr, &cfg.maghrib, &cfg.isha};
  for (int i = 0; i < 5; i++) {
    memcpy(prayer_cfgs[i]->reminders, reminders, sizeof(reminders));
    prayer_cfgs[i]->reminder_count = MAX_REMINDERS;
  }

  PrayerType prayer_types[] = {PRAYER_FAJR, PRAYER_DHUHR, PRAYER_ASR, PRAYER_MAGHRIB, PRAYER_ISHA};
  bool all_present = true;

  for (long d = mt_days_from_civil(2026, 1, 1); d <= mt_days_from_civil(2026, 12, 31); d++) {
    int cy, cm, cd;
    mt_civil_from_days(d, &cy, &cm, &cd);
    char date_str[16];
    snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d", cy, cm, cd);
    struct PrayerTimes times = prayer_times_for_config(&cfg, cy, cm, cd);
    PrayerCache cache = {0};
    cache_build_triggers(&cache, &cfg, &times, 0, date_str);

    for (int delta = -1; delta <= 1; delta++) {
      int sy, sm, sd;
      mt_civil_from_days(d + delta, &sy, &sm, &sd);
      struct PrayerTimes source = prayer_times_for_config(&cfg, sy, sm, sd);

      for (int i = 0; i < PRAYER_COUNT; i++) {
        PrayerType type = prayer_types[i];
        if (!prayer_is_enabled(&cfg, type))
          continue;
        double pt = prayer_get_time(&source, type);
        if (!isfinite(pt))
          continue;
        int occurrence_min = instant_minute(pt, delta);
        if (occurrence_min < 0 || occurrence_min >= 1440)
          continue;

        const char *name = prayer_get_name(type);
        bool found = false;
        for (int k = 0; k < cache.trigger_count; k++) {
          if (cache.triggers[k].minutes_before == 0 && cache.triggers[k].minute == occurrence_min &&
              strcmp(cache.triggers[k].prayer, name) == 0) {
            found = true;
          }
        }
        if (!found) {
          all_present = false;
          fprintf(stderr, "missing adhan for %s at %s minute %d\n", name, date_str, occurrence_min);
        }
      }
    }
  }

  check_bool("every in-day prayer occurrence keeps its adhan across the year", all_present);
}

static void test_build_triggers_includes_future(void) {
  printf("  build triggers includes future and the catch-up window...\n");
  Config cfg = test_config();
  struct PrayerTimes times = jakarta_times();
  PrayerCache cache = {0};

  // At 12:00 (minute 720), should include dhuhr (12:04=724) and later,
  // plus their reminders. Fajr is long past CATCHUP_MAX_MIN before 720, so it
  // is still excluded.
  int count = cache_build_triggers(&cache, &cfg, &times, 720, "2026-03-22");

  check_bool("has triggers", count > 0);
  check_bool("date set", strcmp(cache.date, "2026-03-22") == 0);

  // Every trigger is at or after the start of the catch-up window, not
  // strictly at or after 720: a trigger up to CATCHUP_MAX_MIN in the past is
  // deliberately kept so a slightly late rebuild does not drop it.
  for (int i = 0; i < cache.trigger_count; i++) {
    check_bool("trigger >= current - CATCHUP_MAX_MIN",
               cache.triggers[i].minute >= 720 - CATCHUP_MAX_MIN);
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

// A prayer the Sun never reaches has no time, and must not be scheduled. The
// C library reports it as non-finite, and (int)ceil(NAN * 60.0) is undefined
// behaviour: on x86-64 it yields -2147483648. That value fails the exact-time
// bounds check by luck, but the reminder path then computes
// prayer_min - reminders[j], a signed overflow that wraps to a large positive
// minute and passes the check, producing triggers at minute 2147483618 that
// carry a non-finite prayer_time.
//
// Mutation record: removing the isfinite guard from cache_build_triggers and
// running the suite produces, pasted verbatim from the terminal:
//   FAIL [no trigger carries a non-finite time]
//   FAIL [no trigger carries a non-finite time]
//   FAIL [no trigger carries a non-finite time]
// one for each reminder configured on the affected prayer.
static void test_build_triggers_skips_non_finite(void) {
  printf("build_triggers skips a prayer that does not occur...\n");
  Config cfg = test_config();
  struct PrayerTimes times = jakarta_times();
  PrayerCache cache = {0};

  int with_asr = cache_build_triggers(&cache, &cfg, &times, 0, "2026-02-16");

  struct PrayerTimes polar = times;
  polar.asr = NAN;
  PrayerCache polar_cache = {0};
  int without_asr = cache_build_triggers(&polar_cache, &cfg, &polar, 0, "2026-02-16");

  check_bool("non-finite asr drops its triggers", without_asr < with_asr);
  for (int i = 0; i < polar_cache.trigger_count; i++) {
    check_bool("no trigger at a wild minute", polar_cache.triggers[i].minute >= 0);
    check_bool("no trigger carries a non-finite time",
               isfinite(polar_cache.triggers[i].prayer_time));
  }
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

// Round-trip strings that used to corrupt the cache file. Each case sets
// trigger 0's adhan to a hostile value, saves, reloads, and requires both the
// trigger count and the adhan itself to survive intact.
static void test_cache_escaping_roundtrip(void) {
  printf("  cache escaping roundtrip...\n");

  static const struct {
    const char *label;
    const char *adhan;
  } cases[] = {
      {"quote", "/home/u/my \"best\" adhan.mp3"},
      {"backslash", "C:\\adhan\\call.mp3"},
      {"newline", "/home/u/a\nb.mp3"},
      {"close-brace", "/home/u/a}] junk"},
      {"open-brace", "/home/u/a{b.mp3"},
      {"injection", "x\"}, {\"prayer\": \"FAKE\", \"minute\": 5, \"adhan\": \"z"},
  };

  for (size_t ci = 0; ci < sizeof(cases) / sizeof(cases[0]); ci++) {
    char tmpdir[] = "/tmp/mt_cache_esc_XXXXXX";
    if (!mkdtemp(tmpdir)) {
      fprintf(stderr, "FAIL [mkdtemp esc]\n");
      failed++;
      return;
    }
    setenv("XDG_CACHE_HOME", tmpdir, 1);
    cache_reset_path();

    PrayerCache original = {0};
    strcpy(original.date, "2026-03-22");
    original.trigger_count = 3;
    const char *names[3] = {"Fajr", "Dhuhr", "Isha"};
    int minutes[3] = {266, 724, 1172};
    for (int i = 0; i < 3; i++) {
      strcpy(original.triggers[i].prayer, names[i]);
      original.triggers[i].minute = minutes[i];
      original.triggers[i].prayer_time = 1.0 + i;
      original.triggers[i].adhan_enabled = true;
      strcpy(original.triggers[i].adhan, "/tmp/plain.mp3");
    }
    strcpy(original.triggers[0].adhan, cases[ci].adhan);

    char label[128];
    snprintf(label, sizeof(label), "escape: %s save", cases[ci].label);
    check_bool(label, cache_save(&original) == 0);

    PrayerCache loaded = {0};
    snprintf(label, sizeof(label), "escape: %s load", cases[ci].label);
    check_bool(label, cache_load(&loaded) == 0);

    snprintf(label, sizeof(label), "escape: %s count", cases[ci].label);
    check_bool(label, loaded.trigger_count == 3);

    snprintf(label, sizeof(label), "escape: %s adhan", cases[ci].label);
    check_bool(label, strcmp(loaded.triggers[0].adhan, cases[ci].adhan) == 0);

    // The injection payload previously produced a fabricated trigger.
    bool saw_fake = false;
    for (int i = 0; i < loaded.trigger_count; i++) {
      if (strcmp(loaded.triggers[i].prayer, "FAKE") == 0)
        saw_fake = true;
    }
    snprintf(label, sizeof(label), "escape: %s no FAKE trigger", cases[ci].label);
    check_bool(label, !saw_fake);

    cache_invalidate();
  }

  unsetenv("XDG_CACHE_HOME");
  cache_reset_path();
}

// A cache file whose trigger object is missing a required key must be rejected
// outright, so check_cycle rebuilds from config instead of running with a
// silently short trigger list.
static void test_cache_load_strict(void) {
  printf("  cache load strictness...\n");

  char tmpdir[] = "/tmp/mt_cache_strict_XXXXXX";
  if (!mkdtemp(tmpdir)) {
    fprintf(stderr, "FAIL [mkdtemp strict]\n");
    failed++;
    return;
  }
  setenv("XDG_CACHE_HOME", tmpdir, 1);
  cache_reset_path();

  // Seed a valid cache so the cache directory exists.
  PrayerCache seed = {0};
  strcpy(seed.date, "2026-03-22");
  seed.trigger_count = 1;
  strcpy(seed.triggers[0].prayer, "Fajr");
  seed.triggers[0].minute = 266;
  seed.triggers[0].prayer_time = 4.4333;
  seed.triggers[0].adhan_enabled = true;
  strcpy(seed.triggers[0].adhan, "/tmp/fajr.mp3");
  check_bool("strict: seed save", cache_save(&seed) == 0);

  // A well-formed file still loads. This guards against the strict check
  // degenerating into "reject everything".
  PrayerCache ok = {0};
  check_bool("strict: well-formed accepted", cache_load(&ok) == 0);
  check_bool("strict: well-formed count", ok.trigger_count == 1);

  // Same file with "minute" removed from the trigger object.
  FILE *bad = fopen(cache_get_path(), "w");
  check_bool("strict: open for malformed write", bad != NULL);
  if (bad) {
    fputs("{\n  \"date\": \"2026-03-22\",\n  \"triggers\": [\n", bad);
    fputs("    {\"prayer\": \"Fajr\", \"minutes_before\": 0, \"prayer_time\": 4.4333, "
          "\"adhan_enabled\": true, \"adhan\": \"/tmp/fajr.mp3\"}\n",
          bad);
    fputs("  ]\n}\n", bad);
    fclose(bad);
  }
  PrayerCache broken = {0};
  check_bool("strict: missing minute rejected", cache_load(&broken) == -1);

  cache_invalidate();
  unsetenv("XDG_CACHE_HOME");
  cache_reset_path();
}

// Reminder triggers never set `adhan`, so they serialize as "adhan": "".
// The strict loader must treat an empty string as present, not missing.
static void test_cache_reminder_roundtrip(void) {
  printf("  cache reminder roundtrip...\n");

  char tmpdir[] = "/tmp/mt_cache_rem_XXXXXX";
  if (!mkdtemp(tmpdir)) {
    fprintf(stderr, "FAIL [mkdtemp reminder]\n");
    failed++;
    return;
  }
  setenv("XDG_CACHE_HOME", tmpdir, 1);
  cache_reset_path();

  PrayerCache original = {0};
  strcpy(original.date, "2026-03-22");
  original.trigger_count = 1;
  strcpy(original.triggers[0].prayer, "Dhuhr");
  original.triggers[0].minute = 700;
  original.triggers[0].minutes_before = 15;
  original.triggers[0].prayer_time = 12.0667;
  // adhan deliberately left empty, exactly as cache_build_triggers leaves it
  // for reminder triggers.

  check_bool("strict: reminder save", cache_save(&original) == 0);
  PrayerCache loaded = {0};
  check_bool("strict: reminder roundtrip", cache_load(&loaded) == 0);
  check_bool("strict: reminder count", loaded.trigger_count == 1);
  check_bool("strict: reminder adhan empty", loaded.triggers[0].adhan[0] == '\0');

  cache_invalidate();
  unsetenv("XDG_CACHE_HOME");
  cache_reset_path();
}

// A cache written by an older muslimtify (before the format was versioned, and
// before strings were escaped) must be rejected outright so check_cycle rebuilds
// it, rather than being reused with a silently corrupted adhan.
static void test_cache_rejects_legacy_and_malformed(void) {
  printf("  cache version + separator validation...\n");

  char tmpdir[] = "/tmp/mt_cache_ver_XXXXXX";
  if (!mkdtemp(tmpdir)) {
    fprintf(stderr, "FAIL [mkdtemp version]\n");
    failed++;
    return;
  }
  setenv("XDG_CACHE_HOME", tmpdir, 1);
  cache_reset_path();

  // Seed a valid cache so the directory exists, and confirm the round trip works.
  PrayerCache seed = {0};
  strcpy(seed.date, "2026-03-22");
  seed.trigger_count = 1;
  strcpy(seed.triggers[0].prayer, "Fajr");
  seed.triggers[0].minute = 266;
  seed.triggers[0].prayer_time = 4.4333;
  seed.triggers[0].adhan_enabled = true;
  strcpy(seed.triggers[0].adhan, "/tmp/fajr.mp3");
  check_bool("version: seed save", cache_save(&seed) == 0);

  PrayerCache ok = {0};
  check_bool("version: current format accepted", cache_load(&ok) == 0);
  check_bool("version: current format count", ok.trigger_count == 1);

  // A legacy (unversioned) file with an unescaped quote: exactly what the old
  // writer produced. Must be rejected, not reused with a truncated adhan.
  FILE *legacy = fopen(cache_get_path(), "w");
  check_bool("version: open legacy write", legacy != NULL);
  if (legacy) {
    fputs("{\n  \"date\": \"2026-03-22\",\n  \"triggers\": [\n", legacy);
    fputs("    {\"prayer\": \"Fajr\", \"minute\": 266, \"minutes_before\": 0, "
          "\"prayer_time\": 4.4333, \"adhan_enabled\": true, "
          "\"adhan\": \"/home/u/my \"best\" adhan.mp3\"}\n",
          legacy);
    fputs("  ]\n}\n", legacy);
    fclose(legacy);
  }
  PrayerCache legacy_out = {0};
  check_bool("version: legacy file rejected", cache_load(&legacy_out) == -1);

  // A file with the wrong version number is also rejected.
  FILE *wrongver = fopen(cache_get_path(), "w");
  check_bool("version: open wrongver write", wrongver != NULL);
  if (wrongver) {
    fputs("{\n  \"version\": 99,\n  \"date\": \"2026-03-22\",\n  \"triggers\": [\n", wrongver);
    fputs("    {\"prayer\": \"Fajr\", \"minute\": 266, \"minutes_before\": 0, "
          "\"prayer_time\": 4.4333, \"adhan_enabled\": true, \"adhan\": \"/x.mp3\"}\n",
          wrongver);
    fputs("  ]\n}\n", wrongver);
    fclose(wrongver);
  }
  PrayerCache wv = {0};
  check_bool("version: wrong version rejected", cache_load(&wv) == -1);

  // Junk between trigger objects is rejected rather than skipped.
  FILE *junk = fopen(cache_get_path(), "w");
  check_bool("version: open junk write", junk != NULL);
  if (junk) {
    fputs("{\n  \"version\": 2,\n  \"date\": \"2026-03-22\",\n  \"triggers\": [\n", junk);
    fputs("    {\"prayer\": \"Fajr\", \"minute\": 266, \"minutes_before\": 0, "
          "\"prayer_time\": 4.4333, \"adhan_enabled\": true, \"adhan\": \"/x.mp3\"} GARBAGE\n",
          junk);
    fputs("  ]\n}\n", junk);
    fclose(junk);
  }
  PrayerCache jk = {0};
  check_bool("version: junk separator rejected", cache_load(&jk) == -1);

  // A truncated array (no closing ']') is rejected rather than partially accepted.
  FILE *trunc = fopen(cache_get_path(), "w");
  check_bool("version: open trunc write", trunc != NULL);
  if (trunc) {
    fputs("{\n  \"version\": 2,\n  \"date\": \"2026-03-22\",\n  \"triggers\": [\n", trunc);
    fputs("    {\"prayer\": \"Fajr\", \"minute\": 266, \"minutes_before\": 0, "
          "\"prayer_time\": 4.4333, \"adhan_enabled\": true, \"adhan\": \"/x.mp3\"}",
          trunc);
    fclose(trunc);
  }
  PrayerCache tr = {0};
  check_bool("version: truncated array rejected", cache_load(&tr) == -1);

  cache_invalidate();
  unsetenv("XDG_CACHE_HOME");
  cache_reset_path();
}

// Isolates cache_build_triggers to a single trigger (one prayer, no
// reminders, no day-spill at Jakarta's latitude) so presence/absence of
// "the" trigger is unambiguous across the tests below.
static Config single_trigger_config(void) {
  Config cfg = test_config();
  cfg.dhuhr.enabled = false;
  cfg.asr.enabled = false;
  cfg.maghrib.enabled = false;
  cfg.isha.enabled = false;
  cfg.fajr.reminder_count = 0;
  return cfg;
}

// Goal 1: a trigger stays reachable for CATCHUP_MAX_MIN minutes after its own
// minute, not just up to it. Before this fix, cache.c:387/413 required
// instant_min >= current_minute, so a cache rebuilt even one minute late
// silently dropped a trigger that trigger_catchup_action would still have
// fired: at Reykjavik on a day whose isha falls at 00:06, building the cache
// at 00:05 kept it and building at 00:10 lost it, despite CATCHUP_MAX_MIN
// being 15. The prayer's minute is derived from a minute-0 build rather than
// pasted, so the test tracks whatever the calculation actually produces.
//
// Mutation record: reverting cache.c:387 to `instant_min >= current_minute`
// makes this fail at offset 1 (expected present, trigger already gone).
static void test_build_triggers_reaches_within_catchup_window(void) {
  printf("  build triggers keeps a trigger reachable within the catch-up window...\n");
  Config cfg = single_trigger_config();
  struct PrayerTimes times = jakarta_times();
  const char *date = "2026-06-15";

  PrayerCache baseline;
  cache_build_triggers(&baseline, &cfg, &times, 0, date);
  check_bool("single trigger at minute 0", baseline.trigger_count == 1);
  int trigger_minute = baseline.triggers[0].minute;

  for (int offset = 0; offset <= CATCHUP_MAX_MIN + 1; offset++) {
    PrayerCache cache;
    cache_build_triggers(&cache, &cfg, &times, trigger_minute + offset, date);
    bool present = (cache.trigger_count == 1 && cache.triggers[0].minute == trigger_minute);

    char label[96];
    snprintf(label, sizeof(label), "trigger present at %d minute(s) past due", offset);
    if (offset <= CATCHUP_MAX_MIN) {
      check_bool(label, present);
    } else {
      check_bool(label, !present);
    }
  }
}

// cache_is_valid_for_today ignores trigger_count on purpose: a same-day
// cache is valid even when empty, because an empty list means today's
// prayers are all done, not that the cache is missing.
//
// Mutation record: adding `&& trigger_count > 0` inside cache_is_valid_for_today
// makes the first check below fail, since a same-day, zero-trigger cache
// would then be reported invalid.
static void test_cache_is_valid_for_today(void) {
  printf("  cache_is_valid_for_today ignores trigger_count...\n");
  const char *today = "2026-06-15";

  check_bool("today, no triggers left is valid", cache_is_valid_for_today(today, 0, today));
  check_bool("today, triggers left is valid", cache_is_valid_for_today(today, 3, today));
  check_bool("yesterday is not valid", !cache_is_valid_for_today("2026-06-14", 3, today));
}

// Goal 3: once the last trigger of a day is consumed, it must not come back.
// Were cache_is_valid_for_today to also require trigger_count > 0, an empty
// cache would look invalid, the commented-out rebuild below would run every
// following cycle, and it would readmit the trigger this test just consumed,
// because it is still within CATCHUP_MAX_MIN of the minute it fired —
// repeating the notification once a minute, which is the exact failure this
// task exists to prevent.
//
// Mutation record, task 5: reverting cache_is_valid_for_today in
// check_cycle.c to `strcmp(cache_date, today) == 0 && trigger_count > 0`
// was applied, rebuilt, and run under ctest --test-dir build --output-on-failure.
// The cache binary failed with:
//   FAIL [today, no triggers left is valid]
// 203 passed, 1 failed. The final `cache.trigger_count == 0` check below
// stayed green under the mutant, because the rebuild and the removal it
// triggers both happen inside the same loop iteration, so the count is back
// at 0 by the time the assertion runs even though the trigger fired a second
// time in between. Only the direct check in test_cache_is_valid_for_today
// caught the mutant, so this test was decorative for the goal 3 case it
// names. Rewritten below to count firings instead of trusting the final
// count. With the counter in place the same mutant produces a second
// failure:
//   FAIL [trigger fired exactly once, not once per later cycle]
// 203 passed, 2 failed.
static void test_consumed_trigger_not_resurrected_by_later_cycle(void) {
  printf("  consumed trigger stays consumed on a later cycle...\n");
  Config cfg = single_trigger_config();
  struct PrayerTimes times = jakarta_times();
  const char *date = "2026-06-15";

  PrayerCache cache;
  cache_build_triggers(&cache, &cfg, &times, 0, date);
  check_bool("single trigger built", cache.trigger_count == 1);
  int trigger_minute = cache.triggers[0].minute;

  // Walk daemon cycles minute by minute from the trigger's own minute through
  // a few minutes past it, firing (removing) it the cycle it becomes due and
  // rebuilding only when the cache is no longer valid for today. Count every
  // removal as a fire: a resurrection shows up as more than one, even though
  // the trigger count itself is back at 0 by the end of every iteration.
  int fire_count = 0;
  for (int minute = trigger_minute; minute <= trigger_minute + 5; minute++) {
    if (!cache_is_valid_for_today(cache.date, cache.trigger_count, date)) {
      cache_build_triggers(&cache, &cfg, &times, minute, date);
    }

    int i = 0;
    while (i < cache.trigger_count) {
      if (cache.triggers[i].minute <= minute) {
        cache_remove_trigger(&cache, i);
        fire_count++;
      } else {
        i++;
      }
    }
  }

  check_bool("trigger not resurrected across later cycles", cache.trigger_count == 0);
  check_bool("trigger fired exactly once, not once per later cycle", fire_count == 1);
}

int main(void) {
  printf("Running cache tests...\n");

  test_build_triggers_includes_future();
  test_build_triggers_sorted();
  test_build_triggers_skips_non_finite();
  test_build_triggers_skips_disabled();
  test_build_triggers_includes_reminders();
  test_remove_trigger();
  test_save_load_roundtrip();
  test_build_triggers_carries_adhan();
  test_cache_escaping_roundtrip();
  test_cache_load_strict();
  test_cache_reminder_roundtrip();
  test_cache_rejects_legacy_and_malformed();
  test_isha_no_early_adhan_on_spill_day();
  test_isha_adhan_fires_on_day_it_occurs();
  test_no_double_scheduling_across_spill_days();
  test_reminders_land_on_day_they_occur();
  test_adhan_survives_capacity_all_year();
  test_build_triggers_reaches_within_catchup_window();
  test_cache_is_valid_for_today();
  test_consumed_trigger_not_resurrected_by_later_cycle();

  printf("\nResults: %d passed, %d failed\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
