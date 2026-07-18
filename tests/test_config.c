#define _GNU_SOURCE
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
  snprintf(tmpdir, sizeof(tmpdir), "/tmp/mt_cfgtest_XXXXXX");
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

// -- config_parse_reminders tests --------------------------------------------

static void test_parse_reminders(void) {
  printf("  parse_reminders...\n");
  int buf[MAX_REMINDERS];

  // Normal: "30,15,5"
  int n = config_parse_reminders("30,15,5", buf, MAX_REMINDERS);
  check_bool("parse 30,15,5 count", n == 3);
  check_bool("parse 30,15,5 [0]", buf[0] == 30);
  check_bool("parse 30,15,5 [1]", buf[1] == 15);
  check_bool("parse 30,15,5 [2]", buf[2] == 5);

  // Single value
  n = config_parse_reminders("10", buf, MAX_REMINDERS);
  check_bool("parse single count", n == 1);
  check_bool("parse single [0]", buf[0] == 10);

  // "clear" keyword
  n = config_parse_reminders("clear", buf, MAX_REMINDERS);
  check_bool("parse clear", n == 0);

  // "none" keyword
  n = config_parse_reminders("none", buf, MAX_REMINDERS);
  check_bool("parse none", n == 0);

  // NULL input
  n = config_parse_reminders(NULL, buf, MAX_REMINDERS);
  check_bool("parse NULL", n == -1);

  // NULL output buffer
  n = config_parse_reminders("30", NULL, MAX_REMINDERS);
  check_bool("parse NULL buf", n == -1);

  // Zero/negative values are skipped (atoi returns 0 for non-numeric,
  // and the parser only accepts value > 0)
  n = config_parse_reminders("0,abc,-5", buf, MAX_REMINDERS);
  check_bool("parse invalid vals", n == 0);

  // Max boundary: 1440 minutes (24 hours)
  n = config_parse_reminders("1440", buf, MAX_REMINDERS);
  check_bool("parse 1440 count", n == 1);
  check_bool("parse 1440 val", buf[0] == 1440);

  // Over max: 1441 is rejected
  n = config_parse_reminders("1441", buf, MAX_REMINDERS);
  check_bool("parse 1441 rejected", n == 0);
}

// -- config_validate tests ---------------------------------------------------

static void test_validate(void) {
  printf("  validate...\n");

  // Valid default config
  Config cfg = config_default();
  check_bool("validate default", config_validate(&cfg));

  // NULL
  check_bool("validate NULL", !config_validate(NULL));

  // Latitude boundaries
  cfg = config_default();
  cfg.latitude = 90.0;
  check_bool("validate lat=90", config_validate(&cfg));
  cfg.latitude = -90.0;
  check_bool("validate lat=-90", config_validate(&cfg));
  cfg.latitude = 90.1;
  check_bool("validate lat=90.1 invalid", !config_validate(&cfg));
  cfg.latitude = -90.1;
  check_bool("validate lat=-90.1 invalid", !config_validate(&cfg));

  // Longitude boundaries
  cfg = config_default();
  cfg.longitude = 180.0;
  check_bool("validate lon=180", config_validate(&cfg));
  cfg.longitude = -180.0;
  check_bool("validate lon=-180", config_validate(&cfg));
  cfg.longitude = 180.1;
  check_bool("validate lon=180.1 invalid", !config_validate(&cfg));
  cfg.longitude = -180.1;
  check_bool("validate lon=-180.1 invalid", !config_validate(&cfg));

  // Timezone offset boundaries
  cfg = config_default();
  cfg.timezone_offset = 14.0;
  check_bool("validate tz=14", config_validate(&cfg));
  cfg.timezone_offset = -12.0;
  check_bool("validate tz=-12", config_validate(&cfg));
  cfg.timezone_offset = 14.1;
  check_bool("validate tz=14.1 invalid", !config_validate(&cfg));
  cfg.timezone_offset = -12.1;
  check_bool("validate tz=-12.1 invalid", !config_validate(&cfg));

  // Bad reminder count
  cfg = config_default();
  cfg.fajr.reminder_count = MAX_REMINDERS + 1;
  check_bool("validate bad reminder count", !config_validate(&cfg));

  // Negative reminder count
  cfg = config_default();
  cfg.fajr.reminder_count = -1;
  check_bool("validate neg reminder count", !config_validate(&cfg));

  // Reminder value out of range
  cfg = config_default();
  cfg.fajr.reminders[0] = 1441;
  check_bool("validate reminder 1441", !config_validate(&cfg));
  cfg.fajr.reminders[0] = -1;
  check_bool("validate reminder -1", !config_validate(&cfg));

  // Offset boundaries
  cfg = config_default();
  cfg.fajr.offset = 60;
  check_bool("validate offset 60", config_validate(&cfg));
  cfg.fajr.offset = -60;
  check_bool("validate offset -60", config_validate(&cfg));
  cfg.fajr.offset = 61;
  check_bool("validate offset 61 invalid", !config_validate(&cfg));
  cfg.fajr.offset = -61;
  check_bool("validate offset -61 invalid", !config_validate(&cfg));
}

// -- config_get_prayer tests -------------------------------------------------

static void test_get_prayer(void) {
  printf("  get_prayer...\n");
  Config cfg = config_default();

  // Lowercase
  check_bool("get fajr", config_get_prayer(&cfg, "fajr") == &cfg.fajr);
  check_bool("get isha", config_get_prayer(&cfg, "isha") == &cfg.isha);
  check_bool("get asr", config_get_prayer(&cfg, "asr") == &cfg.asr);
  check_bool("get maghrib", config_get_prayer(&cfg, "maghrib") == &cfg.maghrib);

  // Mixed case
  check_bool("get Fajr", config_get_prayer(&cfg, "Fajr") == &cfg.fajr);
  check_bool("get DHUHR", config_get_prayer(&cfg, "DHUHR") == &cfg.dhuhr);
  check_bool("get Maghrib", config_get_prayer(&cfg, "Maghrib") == &cfg.maghrib);

  // Alias: "dhur" → dhuhr
  check_bool("get dhur alias", config_get_prayer(&cfg, "dhur") == &cfg.dhuhr);

  // Unknown prayer
  check_bool("get unknown", config_get_prayer(&cfg, "badprayer") == NULL);

  // NULL inputs
  check_bool("get NULL cfg", config_get_prayer(NULL, "fajr") == NULL);
  check_bool("get NULL name", config_get_prayer(&cfg, NULL) == NULL);
}

// -- config_format_reminders tests -------------------------------------------

static void test_format_reminders(void) {
  printf("  format_reminders...\n");
  char buf[128];

  // Normal
  PrayerConfig pc = {.reminder_count = 3, .reminders = {30, 15, 5}};
  config_format_reminders(&pc, buf, sizeof(buf));
  check_bool("format 30,15,5", strcmp(buf, "30,15,5") == 0);

  // Single
  pc = (PrayerConfig){.reminder_count = 1, .reminders = {10}};
  config_format_reminders(&pc, buf, sizeof(buf));
  check_bool("format 10", strcmp(buf, "10") == 0);

  // Empty → "none"
  pc = (PrayerConfig){.reminder_count = 0};
  config_format_reminders(&pc, buf, sizeof(buf));
  check_bool("format none", strcmp(buf, "none") == 0);
}

// -- config_default tests ----------------------------------------------------

static void test_default(void) {
  printf("  default...\n");
  Config cfg = config_default();

  check_bool("default auto_detect", cfg.auto_detect);
  check_bool("default lat=0", cfg.latitude == 0.0);
  check_bool("default lon=0", cfg.longitude == 0.0);
  check_bool("default use_gps off", cfg.use_gps == false);
  check_bool("default fajr enabled", cfg.fajr.enabled);
  check_bool("default sunrise disabled", !cfg.sunrise.enabled);
  check_bool("default dhuha disabled", !cfg.dhuha.enabled);
  check_bool("default dhuhr enabled", cfg.dhuhr.enabled);
  check_bool("default asr enabled", cfg.asr.enabled);
  check_bool("default maghrib enabled", cfg.maghrib.enabled);
  check_bool("default isha enabled", cfg.isha.enabled);
  check_bool("default fajr reminders",
             cfg.fajr.reminder_count == 3 && cfg.fajr.reminders[0] == 30 &&
                 cfg.fajr.reminders[1] == 15 && cfg.fajr.reminders[2] == 5);
  check_bool("default sunrise no reminders", cfg.sunrise.reminder_count == 0);
  check_bool("default method kemenag", strcmp(cfg.calculation_method, "kemenag") == 0);
  check_bool("default madhab shafi", strcmp(cfg.madhab, "shafi") == 0);
  check_bool("default sound adhan", strcmp(cfg.notification_sound, "adhan") == 0);
  check_bool("default sound_alarm", strcmp(cfg.notification_sound_alarm, "alarm") == 0);
  check_bool("default sound_reminder", strcmp(cfg.notification_sound_reminder, "reminder") == 0);
  check_bool("default adhan", strcmp(cfg.fajr.adhan, "") == 0);
  check_bool("default adhan", strcmp(cfg.sunrise.adhan, "") == 0);
  check_bool("default adhan", strcmp(cfg.dhuha.adhan, "") == 0);
  check_bool("default adhan", strcmp(cfg.dhuhr.adhan, "") == 0);
  check_bool("default adhan", strcmp(cfg.asr.adhan, "") == 0);
  check_bool("default adhan", strcmp(cfg.maghrib.adhan, "") == 0);
  check_bool("default adhan", strcmp(cfg.isha.adhan, "") == 0);

  check_bool("default adhan enabled", cfg.fajr.adhan_enabled == true);
  check_bool("default adhan enabled", cfg.sunrise.adhan_enabled == false);
  check_bool("default adhan enabled", cfg.dhuha.adhan_enabled == false);
  check_bool("default adhan enabled", cfg.dhuhr.adhan_enabled == true);
  check_bool("default adhan enabled", cfg.asr.adhan_enabled == true);
  check_bool("default adhan enabled", cfg.maghrib.adhan_enabled == true);
  check_bool("default adhan enabled", cfg.isha.adhan_enabled == true);
}

// -- round-trip save/load test -----------------------------------------------

static void test_path_resolution(void) {
  printf("  path resolution...\n");
  const char *path = config_get_path();

  check_bool("config path starts in tmpdir", strncmp(path, tmpdir, strlen(tmpdir)) == 0);
  check_bool("config path includes muslimtify dir",
             strstr(path, "/muslimtify/config.json") != NULL);
}

static void test_round_trip(void) {
  printf("  round-trip save/load...\n");

  // Build a config with specific values
  Config out = config_default();
  out.latitude = -6.2088;
  out.longitude = 106.8456;
  strncpy(out.timezone, "Asia/Jakarta", sizeof(out.timezone) - 1);
  out.timezone_offset = 7.0;
  out.auto_detect = false;
  out.use_gps = true;
  strncpy(out.city, "Jakarta", sizeof(out.city) - 1);
  strncpy(out.country, "Indonesia", sizeof(out.country) - 1);
  out.fajr.enabled = true;
  out.fajr.reminder_count = 2;
  out.fajr.reminders[0] = 20;
  out.fajr.reminders[1] = 10;
  out.fajr.offset = -7;
  out.maghrib.offset = 5;
  strncpy(out.fajr.adhan, "/tmp/custom-fajr.mp3", sizeof(out.fajr.adhan) - 1);
  out.fajr.adhan_enabled = false; // default is true; flip it to prove it round-trips
  out.sunrise.enabled = true;
  out.notification_timeout = 8000;
  strncpy(out.notification_sound, "off", sizeof(out.notification_sound) - 1);
  strncpy(out.notification_sound_alarm, "default", sizeof(out.notification_sound_alarm) - 1);
  strncpy(out.notification_sound_reminder, "alarm", sizeof(out.notification_sound_reminder) - 1);
  strncpy(out.notification_urgency, "critical", sizeof(out.notification_urgency) - 1);

  check_bool("config path includes muslimtify dir",
             strstr(config_get_path(), "/muslimtify/config.json") != NULL);
  check_bool("save ok", config_save(&out) == 0);
  check_bool("config file exists", platform_file_exists(config_get_path()) == 1);

  Config in;
  check_bool("load ok", config_load(&in) == 0);

  // Verify all fields survived the round-trip
  check_bool("rt latitude", fabs(in.latitude - out.latitude) < 0.001);
  check_bool("rt longitude", fabs(in.longitude - out.longitude) < 0.001);
  check_bool("rt timezone", strcmp(in.timezone, out.timezone) == 0);
  check_bool("rt tz_offset", fabs(in.timezone_offset - out.timezone_offset) < 0.1);
  check_bool("rt auto_detect", in.auto_detect == out.auto_detect);
  check_bool("rt use_gps", in.use_gps == out.use_gps);
  check_bool("rt city", strcmp(in.city, out.city) == 0);
  check_bool("rt country", strcmp(in.country, out.country) == 0);
  check_bool("rt fajr enabled", in.fajr.enabled == out.fajr.enabled);
  check_bool("rt fajr reminders", in.fajr.reminder_count == 2 && in.fajr.reminders[0] == 20 &&
                                      in.fajr.reminders[1] == 10);
  check_bool("rt fajr offset", in.fajr.offset == -7);
  check_bool("rt maghrib offset", in.maghrib.offset == 5);
  check_bool("rt unset offset 0", in.dhuhr.offset == 0);
  check_bool("rt sunrise enabled", in.sunrise.enabled == true);
  check_bool("rt timeout", in.notification_timeout == 8000);
  check_bool("rt sound", strcmp(in.notification_sound, "off") == 0);
  check_bool("rt sound_alarm", strcmp(in.notification_sound_alarm, "default") == 0);
  check_bool("rt sound_reminder", strcmp(in.notification_sound_reminder, "alarm") == 0);
  check_bool("rt urgency", strcmp(in.notification_urgency, "critical") == 0);
  check_bool("rt method", strcmp(in.calculation_method, "kemenag") == 0);
  check_bool("rt madhab", strcmp(in.madhab, "shafi") == 0);
  check_bool("rt fajr adhan", strcmp(in.fajr.adhan, "/tmp/custom-fajr.mp3") == 0);
  check_bool("rt fajr adhan_enabled", in.fajr.adhan_enabled == false);
  check_bool("rt dhuhr adhan default empty", in.dhuhr.adhan[0] == '\0');
}

static void test_offset_apply(void) {
  printf("  offset apply...\n");

  // Jakarta, Kemenag (matches other engine tests in this repo)
  Config cfg = config_default();
  cfg.latitude = -6.2088;
  cfg.longitude = 106.8456;
  cfg.timezone_offset = 7.0;

  MethodParams params = method_params_from_config(&cfg);
  struct PrayerTimes raw = calculate_prayer_times(2026, 1, 15, cfg.latitude, cfg.longitude,
                                                  cfg.timezone_offset, &params);

  cfg.fajr.offset = 12; // +0.2 h
  cfg.asr.offset = -6;  // -0.1 h
  // dhuhr.offset stays 0

  struct PrayerTimes adj = prayer_times_for_config(&cfg, 2026, 1, 15);

  check_bool("offset fajr +12", fabs(adj.fajr - (raw.fajr + 12.0 / 60.0)) < 1e-9);
  check_bool("offset asr -6", fabs(adj.asr - (raw.asr - 6.0 / 60.0)) < 1e-9);
  check_bool("offset dhuhr 0 identity", fabs(adj.dhuhr - raw.dhuhr) < 1e-9);
}

static void test_offset_wrap(void) {
  printf("  offset midnight wrap...\n");

  // London in June: high-latitude late Isha, near the midnight boundary.
  Config cfg = config_default();
  cfg.latitude = 51.5074;
  cfg.longitude = -0.1278;
  cfg.timezone_offset = 1.0;
  int y = 2026, m = 6, d = 15;

  MethodParams params = method_params_from_config(&cfg);
  struct PrayerTimes raw =
      calculate_prayer_times(y, m, d, cfg.latitude, cfg.longitude, cfg.timezone_offset, &params);

  cfg.isha.offset = 60; // push Isha up to an hour later
  struct PrayerTimes adj = prayer_times_for_config(&cfg, y, m, d);

  double expected = raw.isha + 1.0;
  if (expected >= 24.0)
    expected -= 24.0;
  else if (expected < 0.0)
    expected += 24.0;

  // Every field must stay a valid minute-of-day so the cache/checker/formatter agree.
  check_bool("wrap isha in [0,24)", adj.isha >= 0.0 && adj.isha < 24.0);
  check_bool("wrap isha value", fabs(adj.isha - expected) < 1e-9);
  check_bool("wrap fajr in [0,24)", adj.fajr >= 0.0 && adj.fajr < 24.0);
}

static void test_offset_clamp_on_load(void) {
  printf("  offset clamp on load...\n");

  Config out = config_default();
  out.latitude = -6.2088;
  out.longitude = 106.8456;
  out.timezone_offset = 7.0;
  out.auto_detect = false;
  out.fajr.offset = 9999; // out of range, written verbatim by config_save
  out.isha.offset = -9999;

  check_bool("clamp save ok", config_save(&out) == 0);

  Config in;
  check_bool("clamp load ok", config_load(&in) == 0);
  check_bool("clamp fajr to max", in.fajr.offset == PRAYER_OFFSET_MAX);
  check_bool("clamp isha to min", in.isha.offset == PRAYER_OFFSET_MIN);
}

static void test_config_size_cap(void) {
  printf("  config_size_cap...\n");

  // Seed a valid config so the dir + path exist.
  Config cfg = config_default();
  check_bool("cap: initial save", config_save(&cfg) == 0);

  // Overwrite it with a >1 MiB blob; config_load must refuse it.
  FILE *f = fopen(config_get_path(), "w");
  check_bool("cap: reopen for overwrite", f != NULL);
  if (f) {
    for (long i = 0; i < (1024L * 1024L) + 16L; i++)
      fputc(' ', f);
    fclose(f);
  }

  Config loaded;
  check_bool("cap: oversize config rejected", config_load(&loaded) == -1);
}

static void test_config_perms(void) {
#ifndef _WIN32
  printf("  config_perms...\n");
  Config cfg = config_default();
  check_bool("perms: save", config_save(&cfg) == 0);
  struct stat st;
  check_bool("perms: stat", stat(config_get_path(), &st) == 0);
  check_bool("perms: owner-only (0600)", (st.st_mode & 077) == 0);
#else
  (void)0;
#endif
}

static void test_sound_migration(void) {
  printf("  sound_migration...\n");
  // Write a config with the legacy boolean sound field, then load it.
  Config cfg = config_default();
  config_save(&cfg);
  FILE *f = fopen(config_get_path(), "w");
  if (f) {
    fputs("{\n  \"notification\": { \"sound\": true }\n}\n", f);
    fclose(f);
  }
  Config a;
  check_bool("migrate load true", config_load(&a) == 0);
  check_bool("migrate true->default", strcmp(a.notification_sound, "default") == 0);

  f = fopen(config_get_path(), "w");
  if (f) {
    fputs("{\n  \"notification\": { \"sound\": false }\n}\n", f);
    fclose(f);
  }
  Config b;
  check_bool("migrate load false", config_load(&b) == 0);
  check_bool("migrate false->off", strcmp(b.notification_sound, "off") == 0);
}

static void test_refresh_interval(void) {
  printf("test_refresh_interval\n");

  // Round-trip: a set interval + timestamp survive save/load.
  Config out = config_default();
  out.refresh_interval = 21600; // 6h
  out.updated_at = 1752624000;
  config_save(&out);
  Config in;
  check_bool("load ok", config_load(&in) == 0);
  check_bool("refresh_interval round-trips", in.refresh_interval == 21600);
  check_bool("updated_at round-trips", in.updated_at == 1752624000);

  // Missing keys → 12h default (NOT 0/disabled) and updated_at 0.
  FILE *f = fopen(config_get_path(), "w");
  fputs("{\n  \"location\": {\n    \"latitude\": 1.0,\n    \"longitude\": 2.0\n  }\n}\n", f);
  fclose(f);
  Config miss;
  check_bool("missing load ok", config_load(&miss) == 0);
  check_bool("missing refresh_interval defaults to 12h",
             miss.refresh_interval == LOCATION_DEFAULT_REFRESH_SECONDS);
  check_bool("missing updated_at defaults to 0", miss.updated_at == 0);

  // Below-floor positive value is clamped up to the 1h minimum on load.
  f = fopen(config_get_path(), "w");
  fputs("{\n  \"location\": {\n    \"refresh_interval\": 600\n  }\n}\n", f);
  fclose(f);
  Config clamp;
  check_bool("clamp load ok", config_load(&clamp) == 0);
  check_bool("sub-minimum interval clamped to 3600",
             clamp.refresh_interval == LOCATION_MIN_REFRESH_SECONDS);

  // 0 (disabled) is preserved, not clamped.
  f = fopen(config_get_path(), "w");
  fputs("{\n  \"location\": {\n    \"refresh_interval\": 0\n  }\n}\n", f);
  fclose(f);
  Config off;
  check_bool("disabled load ok", config_load(&off) == 0);
  check_bool("interval 0 preserved (disabled)", off.refresh_interval == 0);
}

static void test_effective_tz_offset(void) {
  printf("  effective_tz_offset (DST-aware)...\n");

  Config cfg = config_default();
  cfg.timezone_offset = 99.0; // sentinel: must be ignored when tz name is valid

  // America/New_York: EST (UTC-5) in January, EDT (UTC-4) in July 2023.
  strncpy(cfg.timezone, "America/New_York", sizeof(cfg.timezone) - 1);
  cfg.timezone[sizeof(cfg.timezone) - 1] = '\0';
  check_bool("NY winter EST -5.0", fabs(effective_tz_offset(&cfg, 2023, 1, 15) - (-5.0)) < 1e-6);
  check_bool("NY summer EDT -4.0", fabs(effective_tz_offset(&cfg, 2023, 7, 15) - (-4.0)) < 1e-6);

  // Invalid/empty timezone name -> fall back to the stored scalar.
  cfg.timezone[0] = '\0';
  cfg.timezone_offset = 3.5;
  check_bool("invalid tz falls back to stored offset",
             fabs(effective_tz_offset(&cfg, 2023, 7, 15) - 3.5) < 1e-6);
}

// -- main ---------------------------------------------------------------------

int main(void) {
  setup();

  printf("Running config tests...\n");
  test_parse_reminders();
  test_validate();
  test_get_prayer();
  test_format_reminders();
  test_default();
  test_path_resolution();
  test_round_trip();
  test_sound_migration();
  test_offset_apply();
  test_offset_wrap();
  test_offset_clamp_on_load();
  test_config_size_cap();
  test_config_perms();
  test_refresh_interval();
  test_effective_tz_offset();

  printf("\nResults: %d passed, %d failed\n", passed, failed);
  teardown();
  return failed > 0 ? 1 : 0;
}
