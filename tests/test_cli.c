#define _GNU_SOURCE

#include "cli.h"
#include "cli_internal.h"
#include "config.h"
#include "country.h"
#include "display.h"
#include "prayertimes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// -- test infrastructure -----------------------------------------------------

static char tmpdir[256];
static char output_file[512];
static char captured[16384];
static int passed = 0;
static int failed = 0;
static int last_ret = -1;

static void setup(void) {
  snprintf(tmpdir, sizeof(tmpdir), "/tmp/muslimtify_test_XXXXXX");
  if (!mkdtemp(tmpdir)) {
    fprintf(stderr, "FATAL: mkdtemp failed\n");
    exit(1);
  }
  setenv("XDG_CONFIG_HOME", tmpdir, 1);
  snprintf(output_file, sizeof(output_file), "%s/_output.txt", tmpdir);

  // Force config_get_path() to pick up our XDG_CONFIG_HOME by creating
  // the config directory and saving a default config.
  char dir[512];
  snprintf(dir, sizeof(dir), "%s/muslimtify", tmpdir);
  mkdir(dir, 0755);
}

static void teardown(void) {
  char cmd[1024];
  snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
  if (system(cmd) != 0) { /* best-effort cleanup */
  }
}

static void reset_config(void) {
  Config cfg = config_default();
  // Set Jakarta location to avoid network calls
  cfg.latitude = -6.2088;
  cfg.longitude = 106.8456;
  strncpy(cfg.timezone, "Asia/Jakarta", sizeof(cfg.timezone) - 1);
  cfg.timezone_offset = 7.0;
  cfg.auto_detect = false;
  strncpy(cfg.city, "Jakarta", sizeof(cfg.city) - 1);
  strncpy(cfg.country, "Indonesia", sizeof(cfg.country) - 1);
  config_save(&cfg);
}

// Run cli_run() with given args, capturing stdout+stderr into `captured`.
// Returns cli_run() return value.
static int run(int argc, char **argv) {
  fflush(stdout);
  fflush(stderr);

  int saved_out = dup(STDOUT_FILENO);
  int saved_err = dup(STDERR_FILENO);

  FILE *f = fopen(output_file, "w");
  if (!f) {
    fprintf(stderr, "FATAL: cannot open output file\n");
    exit(1);
  }
  int fd = fileno(f);
  dup2(fd, STDOUT_FILENO);
  dup2(fd, STDERR_FILENO);

  int ret = cli_run(argc, argv);

  fflush(stdout);
  fflush(stderr);
  dup2(saved_out, STDOUT_FILENO);
  dup2(saved_err, STDERR_FILENO);
  close(saved_out);
  close(saved_err);
  fclose(f);

  // Read captured output
  FILE *r = fopen(output_file, "r");
  if (r) {
    size_t n = fread(captured, 1, sizeof(captured) - 1, r);
    captured[n] = '\0';
    fclose(r);
  } else {
    captured[0] = '\0';
  }

  last_ret = ret;
  return ret;
}

// -- assertion helpers --------------------------------------------------------

static void check_ret(const char *test, int expected) {
  if (last_ret == expected) {
    passed++;
  } else {
    failed++;
    fprintf(stderr, "FAIL [%s]: expected ret=%d, got %d\n", test, expected, last_ret);
  }
}

static void check_contains(const char *test, const char *needle) {
  if (strstr(captured, needle)) {
    passed++;
  } else {
    failed++;
    fprintf(stderr, "FAIL [%s]: output missing \"%s\"\n", test, needle);
    fprintf(stderr, "  got: %.200s%s\n", captured, strlen(captured) > 200 ? "..." : "");
  }
}

static void check_bool(const char *test, bool cond) {
  if (cond) {
    passed++;
  } else {
    failed++;
    fprintf(stderr, "FAIL [%s]\n", test);
  }
}

// -- test groups --------------------------------------------------------------

static void test_version_and_help(void) {
  printf("  version and help...\n");
  reset_config();

  // version
  run(2, (char *[]){"m", "version", NULL});
  check_ret("version ret", 0);
  check_contains("version out", "Muslimtify v");

  // --version
  run(2, (char *[]){"m", "--version", NULL});
  check_ret("--version ret", 0);
  check_contains("--version out", "Muslimtify v");

  // -v
  run(2, (char *[]){"m", "-v", NULL});
  check_ret("-v ret", 0);
  check_contains("-v out", "Muslimtify v");

  // help
  run(2, (char *[]){"m", "help", NULL});
  check_ret("help ret", 0);
  check_contains("help out", "Usage:");

  // --help
  run(2, (char *[]){"m", "--help", NULL});
  check_ret("--help ret", 0);
  check_contains("--help out", "Usage:");

  // -h
  run(2, (char *[]){"m", "-h", NULL});
  check_ret("-h ret", 0);
  check_contains("-h out", "Usage:");

  // unknown command
  run(2, (char *[]){"m", "xyzzy", NULL});
  check_ret("unknown ret", 1);
  check_contains("unknown out", "Unknown command");
}

static void test_location(void) {
  printf("  location...\n");
  reset_config();

  // location (default: table)
  run(2, (char *[]){"m", "location", NULL});
  check_ret("location default ret", 0);
  check_contains("location default city", "Jakarta");
  check_contains("location default coords row", "coordinates");
  check_contains("location default tz row", "timezone");

  // location --json
  run(3, (char *[]){"m", "location", "--json", NULL});
  check_ret("location json ret", 0);
  check_contains("location json coords", "\"coordinates\"");
  check_contains("location json gmt", "\"gmt\"");

  // location --headless
  run(3, (char *[]){"m", "location", "--headless", NULL});
  check_ret("location headless ret", 0);
  check_contains("location headless coords", "coordinates=");
  check_contains("location headless gmt", "gmt=");

  // mutual exclusion
  run(4, (char *[]){"m", "location", "--json", "--headless", NULL});
  check_ret("location json+headless ret", 1);
  check_contains("location json+headless msg", "cannot be combined");

  // help
  run(3, (char *[]){"m", "location", "--help", NULL});
  check_ret("location help ret", 0);
  check_contains("location help usage", "muslimtify location");

  // removed subcommands -> migration hints
  run(3, (char *[]){"m", "location", "show", NULL});
  check_ret("location show removed ret", 1);
  check_contains("location show removed msg", "was removed");

  run(3, (char *[]){"m", "location", "refresh", NULL});
  check_ret("location refresh removed ret", 1);
  check_contains("location refresh removed msg", "set --auto");

  run(3, (char *[]){"m", "location", "clear", NULL});
  check_ret("location clear removed ret", 1);
  check_contains("location clear removed msg", "set --auto");

  // location set --lat=<lat> --long=<lon> (equals form)
  run(5, (char *[]){"m", "location", "set", "--lat=-7.25", "--long=112.75", NULL});
  check_ret("location set ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("location set lat", cfg.latitude > -7.26 && cfg.latitude < -7.24);
    check_bool("location set lon", cfg.longitude > 112.74 && cfg.longitude < 112.76);
  }
  // changing coordinates hints to verify the timezone
  check_contains("location set coords tz hint", "make sure the timezone is correct");

  // changing the timezone hints to verify the coordinates
  run(4, (char *[]){"m", "location", "set", "--timezone=Asia/Jakarta", NULL});
  check_ret("location set tz ret", 0);
  check_contains("location set tz coords hint", "make sure your coordinates are correct");

  // location set --auto rejects coordinate / timezone overrides
  run(5, (char *[]){"m", "location", "set", "--auto", "--lat=1.0", NULL});
  check_ret("location set auto+lat ret", 1);
  check_contains("location set auto+lat msg", "cannot be combined");

  run(5, (char *[]){"m", "location", "set", "--auto", "--timezone=Asia/Jakarta", NULL});
  check_ret("location set auto+tz ret", 1);
  check_contains("location set auto+tz msg", "cannot be combined");

  // location set --help
  run(4, (char *[]){"m", "location", "set", "--help", NULL});
  check_ret("location set help ret", 0);
  check_contains("location set help usage", "muslimtify location set");

  // location set --lat <lat> --long <lon> (space form)
  run(7, (char *[]){"m", "location", "set", "--lat", "-7.25", "--long", "112.75", NULL});
  check_ret("location set space ret", 0);

  // location set (no args) rejected
  run(3, (char *[]){"m", "location", "set", NULL});
  check_ret("location set noargs ret", 1);

  // location set (invalid lat)
  run(5, (char *[]){"m", "location", "set", "--lat=abc", "--long=112.75", NULL});
  check_ret("location set bad lat ret", 1);

  // location set (invalid lon)
  run(5, (char *[]){"m", "location", "set", "--lat=-7.25", "--long=xyz", NULL});
  check_ret("location set bad lon ret", 1);

  // location set out-of-range coordinates rejected
  run(4, (char *[]){"m", "location", "set", "--lat=91", NULL});
  check_ret("location set lat>90 ret", 1);
  run(4, (char *[]){"m", "location", "set", "--long=181", NULL});
  check_ret("location set lon>180 ret", 1);

  // location set at the equator/prime meridian (0.0 is a valid coordinate)
  run(5, (char *[]){"m", "location", "set", "--lat=0", "--long=0", NULL});
  check_ret("location set equator ret", 0);
  check_contains("location set equator coords shown", "Coordinates updated to 0.0000, 0.0000");

  // location set --timezone=<iana> (equals form)
  run(6, (char *[]){"m", "location", "set", "--lat=30.0", "--long=31.0", "--timezone=Africa/Cairo",
                    NULL});
  check_ret("location set --timezone= ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("location set --timezone= zone", strcmp(cfg.timezone, "Africa/Cairo") == 0);
    // Cairo is +2 (EET) or +3 (EEST); accept either.
    check_bool("location set --timezone= offset",
               cfg.timezone_offset >= 2.0 && cfg.timezone_offset <= 3.0);
  }

  // location set --timezone <iana> (space form)
  run(7, (char *[]){"m", "location", "set", "--lat=51.5", "--long=-0.1", "--timezone",
                    "Europe/London", NULL});
  check_ret("location set --timezone space ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("location set --timezone space zone", strcmp(cfg.timezone, "Europe/London") == 0);
  }

  // location set --timezone=<bogus> rejected
  run(6, (char *[]){"m", "location", "set", "--lat=30.0", "--long=31.0", "--timezone=Asia/Foobar",
                    NULL});
  check_ret("location set --timezone bogus ret", 1);

  // location set --timezone (missing value)
  run(6, (char *[]){"m", "location", "set", "--lat=30.0", "--long=31.0", "--timezone", NULL});
  check_ret("location set --timezone missing ret", 1);

  // location set with --timezone= preceding coordinates (flag-order flexibility)
  run(6, (char *[]){"m", "location", "set", "--timezone=Asia/Tokyo", "--lat=35.68", "--long=139.69",
                    NULL});
  check_ret("location set --timezone-first ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("location set --timezone-first zone", strcmp(cfg.timezone, "Asia/Tokyo") == 0);
    check_bool("location set --timezone-first offset",
               cfg.timezone_offset > 8.9 && cfg.timezone_offset < 9.1);
  }

  // location set --city=<name> writes the label
  run(6,
      (char *[]){"m", "location", "set", "--lat=-6.21", "--long=106.84", "--city=Jakarta", NULL});
  check_ret("location set --city= ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("location set --city= label", strcmp(cfg.city, "Jakarta") == 0);
  }

  // location set --city <name> (space form)
  run(7, (char *[]){"m", "location", "set", "--lat=-6.21", "--long=106.84", "--city", "Surabaya",
                    NULL});
  check_ret("location set --city space ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("location set --city space label", strcmp(cfg.city, "Surabaya") == 0);
  }

  // changing coordinates clears any prior city label
  run(5, (char *[]){"m", "location", "set", "--lat=-6.21", "--long=106.84", NULL});
  check_ret("location set no-city ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("location set no-city clears", cfg.city[0] == '\0');
  }

  // a timezone-only update leaves the existing city label intact
  run(6,
      (char *[]){"m", "location", "set", "--lat=-6.21", "--long=106.84", "--city=Bandung", NULL});
  check_ret("location set seed-city ret", 0);
  run(4, (char *[]){"m", "location", "set", "--timezone=Asia/Jakarta", NULL});
  check_ret("location set tz-only ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("location set tz-only keeps city", strcmp(cfg.city, "Bandung") == 0);
  }

  // location set --city (missing value)
  run(6, (char *[]){"m", "location", "set", "--lat=-6.21", "--long=106.84", "--city", NULL});
  check_ret("location set --city missing ret", 1);

  // location set combining --timezone and --city
  run(7, (char *[]){"m", "location", "set", "--lat=31.04", "--long=31.38",
                    "--timezone=Africa/Cairo", "--city=Mansoura", NULL});
  check_ret("location set --tz+--city ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("location set --tz+--city zone", strcmp(cfg.timezone, "Africa/Cairo") == 0);
    check_bool("location set --tz+--city label", strcmp(cfg.city, "Mansoura") == 0);
  }

  // location unknown
  run(3, (char *[]){"m", "location", "bogus", NULL});
  check_ret("location unknown ret", 1);

  // location auto removed -> unknown subcommand
  run(3, (char *[]){"m", "location", "auto", NULL});
  check_ret("location auto removed ret", 1);

  // JSON escapes a special character in a field value
  run(6, (char *[]){"m", "location", "set", "--lat=-6.21", "--long=106.84", "--city=a\"b", NULL});
  check_ret("location set escape-city ret", 0);
  run(3, (char *[]){"m", "location", "--json", NULL});
  check_contains("location json escapes quote", "a\\\"b");

  // city-only update: concise message, and the timezone is left untouched
  reset_config();
  run(4, (char *[]){"m", "location", "set", "--city=Medan", NULL});
  check_ret("location set city-only ret", 0);
  check_contains("location set city-only msg", "City updated to Medan");
  check_bool("location set city-only no coords line",
             strstr(captured, "Coordinates updated") == NULL);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("location set city-only keeps tz", strcmp(cfg.timezone, "Asia/Jakarta") == 0);
  }
}

static void test_removed_top_level(void) {
  printf("  removed top-level...\n");
  reset_config();
  run(3, (char *[]){"m", "enable", "fajr", NULL});
  check_ret("removed enable ret", 1);
  check_contains("removed enable msg", "notification enable");

  run(3, (char *[]){"m", "disable", "fajr", NULL});
  check_ret("removed disable ret", 1);

  run(2, (char *[]){"m", "list", NULL});
  check_ret("removed list ret", 1);
  check_contains("removed list msg", "notification");

  run(4, (char *[]){"m", "reminder", "fajr", "30", NULL});
  check_ret("removed reminder ret", 1);
  check_contains("removed reminder msg", "--reminder");
}

static void test_show(void) {
  printf("  show...\n");
  reset_config();

  run(1, (char *[]){"m", NULL});
  check_ret("default ret", 0);
  check_contains("default version", "Muslimtify v");
  check_contains("default help", "Usage:");

  run(2, (char *[]){"m", "show", NULL});
  check_ret("show explicit ret", 0);

  // --json: prayers-only, no location block
  run(3, (char *[]){"m", "show", "--json", NULL});
  check_ret("show json ret", 0);
  check_contains("show json prayers", "\"prayers\"");
  check_contains("show json fajr", "\"fajr\"");
  check_bool("show json no location", strstr(captured, "\"location\"") == NULL);

  // --headless: lowercase keys, disabled prayers omitted
  run(3, (char *[]){"m", "show", "--headless", NULL});
  check_ret("show headless ret", 0);
  check_contains("show headless fajr", "fajr=");
  check_contains("show headless isha", "isha=");
  check_bool("show headless no sunrise", strstr(captured, "sunrise=") == NULL);
  check_bool("show headless not capitalized", strstr(captured, "Fajr=") == NULL);

  // enable all -> disabled prayers now appear
  run(4, (char *[]){"m", "notification", "enable", "all", NULL});
  run(3, (char *[]){"m", "show", "--headless", NULL});
  check_contains("show headless sunrise", "sunrise=");

  // mutual exclusion
  run(4, (char *[]){"m", "show", "--json", "--headless", NULL});
  check_ret("show json+headless ret", 1);
  check_contains("show json+headless msg", "cannot be combined");

  // help
  run(3, (char *[]){"m", "show", "--help", NULL});
  check_ret("show help ret", 0);
  check_contains("show help usage", "muslimtify show");

  // removed flags -> migration hint
  run(4, (char *[]){"m", "show", "--format", "json", NULL});
  check_ret("show old format ret", 1);
  check_contains("show old format hint", "--json");
}

static void test_show_range(void) {
  printf("  show --date range...\n");
  reset_config();

  // single date still works (regression)
  run(4, (char *[]){"m", "show", "--date", "2022-01-01", NULL});
  check_ret("range single ret", 0);

  // invalid month rejected
  run(4, (char *[]){"m", "show", "--date", "2022-13-01", NULL});
  check_ret("range bad month ret", 1);
  check_contains("range bad month msg", "Invalid date");

  // invalid day-of-month (Feb 30) rejected
  run(4, (char *[]){"m", "show", "--date", "2022-02-30", NULL});
  check_ret("range feb30 ret", 1);

  // trailing junk rejected
  run(4, (char *[]){"m", "show", "--date", "2022-01-01x", NULL});
  check_ret("range junk ret", 1);

  // reversed range rejected
  run(5, (char *[]){"m", "show", "--date", "2022-01-03", "2022-01-01", NULL});
  check_ret("range reversed ret", 1);
  check_contains("range reversed msg", "end date is before start date");

  // JSON range: array of day objects with dates
  run(6, (char *[]){"m", "show", "--date", "2022-01-01", "2022-01-03", "--json", NULL});
  check_ret("range json ret", 0);
  check_contains("range json has date key", "\"date\":");
  check_contains("range json d1", "\"2022-01-01\"");
  check_contains("range json d2", "\"2022-01-02\"");
  check_contains("range json d3", "\"2022-01-03\"");
  check_contains("range json prayers", "\"prayers\"");
  check_contains("range json fajr", "\"fajr\"");

  // flag BEFORE the dates works identically
  run(6, (char *[]){"m", "show", "--json", "--date", "2022-01-01", "2022-01-03", NULL});
  check_ret("range json flag-first ret", 0);
  check_contains("range json flag-first d2", "\"2022-01-02\"");

  // single-day JSON still prayers-only (regression)
  run(5, (char *[]){"m", "show", "--date", "2022-01-01", "--json", NULL});
  check_ret("range single json ret", 0);
  check_bool("range single json no date key", strstr(captured, "\"date\":") == NULL);
  check_contains("range single json prayers", "\"prayers\"");

  // headless range: date= blocks, enabled prayers only
  run(6, (char *[]){"m", "show", "--date", "2022-01-01", "2022-01-02", "--headless", NULL});
  check_ret("range headless ret", 0);
  check_contains("range headless date1", "date=2022-01-01");
  check_contains("range headless date2", "date=2022-01-02");
  check_contains("range headless fajr", "fajr=");
  check_bool("range headless no sunrise", strstr(captured, "sunrise=") == NULL);

  // table range (default): pivoted table, one row per day, columns = enabled prayers only
  run(5, (char *[]){"m", "show", "--date", "2022-01-01", "2022-01-02", NULL});
  check_ret("range table ret", 0);
  check_contains("range table header date", "Date");
  check_contains("range table col fajr", "Fajr");
  check_contains("range table col dhuhr", "Dhuhr");
  check_contains("range table col isha", "Isha");
  check_contains("range table d1", "2022-01-01");
  check_contains("range table d2", "2022-01-02");
  // disabled prayers (sunrise, dhuha by default) are hidden as columns
  check_bool("range table hides sunrise", strstr(captured, "Sunrise") == NULL);
  check_bool("range table hides dhuha", strstr(captured, "Dhuha") == NULL);
  // unbroken full-width border (no internal + column separators)
  check_bool("range table no cross border", strstr(captured, "-+-") == NULL);

  // leap boundary crossing includes Feb 29 in 2024
  run(6, (char *[]){"m", "show", "--date", "2024-02-27", "2024-03-01", "--headless", NULL});
  check_ret("range leap ret", 0);
  check_contains("range leap feb29", "date=2024-02-29");
  check_contains("range leap mar1", "date=2024-03-01");

  // non-leap 2023 has no Feb 29
  run(6, (char *[]){"m", "show", "--date", "2023-02-27", "2023-03-01", "--headless", NULL});
  check_ret("range nonleap ret", 0);
  check_bool("range nonleap no feb29", strstr(captured, "date=2023-02-29") == NULL);
  check_contains("range nonleap feb28", "date=2023-02-28");

  // mutual exclusion still enforced on a range
  run(7,
      (char *[]){"m", "show", "--date", "2022-01-01", "2022-01-03", "--json", "--headless", NULL});
  check_ret("range json+headless ret", 1);

  // per-command help documents the range form
  run(4, (char *[]){"m", "show", "--date", "--help", NULL});
  check_ret("range help ret", 0);
  check_contains("range help usage", "muslimtify show --date");
  check_contains("range help range", "<start> [end]");
}

static void test_next(void) {
  printf("  show --next...\n");
  reset_config();

  // default: bordered table
  run(3, (char *[]){"m", "show", "--next", NULL});
  check_ret("next table ret", 0);
  check_contains("next table border", "+------------+");
  check_contains("next table remaining", "Remaining");

  // headless
  run(4, (char *[]){"m", "show", "--next", "--headless", NULL});
  check_ret("next headless ret", 0);
  check_contains("next headless remaining", "remaining=");

  // json
  run(4, (char *[]){"m", "show", "--next", "--json", NULL});
  check_ret("next json ret", 0);
  check_contains("next json prayer", "\"prayer\"");
  check_contains("next json remaining", "\"remaining\"");

  // per-mode help
  run(4, (char *[]){"m", "show", "--next", "--help", NULL});
  check_ret("next help ret", 0);
  check_contains("next help usage", "--next");

  // bare `next` is removed -> unknown command
  run(2, (char *[]){"m", "next", NULL});
  check_ret("bare next removed ret", 1);
  check_contains("bare next removed msg", "Unknown command");
}

// When all of today's prayers have passed, `show --next` rolls over to tomorrow's
// Fajr. The displayed clock time must be TOMORROW's Fajr, not today's used as a
// proxy. Driven directly (not via cli_run) because the CLI path reads the wall
// clock; here we craft an "after isha" struct tm. Uses a high-latitude location
// on a spring date so the day-to-day Fajr shift exceeds a minute, making the
// today-vs-tomorrow distinction observable.
static void test_next_after_isha(void) {
  printf("  show --next after isha...\n");

  Config cfg = config_default();
  cfg.latitude = 51.5074;
  cfg.longitude = -0.1278;
  strncpy(cfg.timezone, "Europe/London", sizeof(cfg.timezone) - 1);
  cfg.timezone_offset = 0.0;
  cfg.auto_detect = false;

  const int Y = 2022, M = 3, D = 20; // near the spring equinox
  struct PrayerTimes today = prayer_times_for_config(&cfg, Y, M, D);
  struct PrayerTimes tomorrow = prayer_times_for_config(&cfg, Y, M, D + 1);
  char today_fajr[16], tomo_fajr[16];
  format_time_hm(today.fajr, today_fajr, sizeof(today_fajr));
  format_time_hm(tomorrow.fajr, tomo_fajr, sizeof(tomo_fajr));

  // 23:00 on date D: every prayer today has passed, so next is tomorrow's Fajr.
  struct tm now = {0};
  now.tm_year = Y - 1900;
  now.tm_mon = M - 1;
  now.tm_mday = D;
  now.tm_hour = 23;
  now.tm_min = 0;

  // Capture display_next_prayer_headless() called directly.
  fflush(stdout);
  int saved_out = dup(STDOUT_FILENO);
  FILE *f = fopen(output_file, "w");
  if (!f) {
    check_bool("next after isha capture open", false);
    return;
  }
  dup2(fileno(f), STDOUT_FILENO);
  display_next_prayer_headless(&today, &cfg, &now);
  fflush(stdout);
  dup2(saved_out, STDOUT_FILENO);
  close(saved_out);
  fclose(f);
  FILE *r = fopen(output_file, "r");
  if (r) {
    size_t n = fread(captured, 1, sizeof(captured) - 1, r);
    captured[n] = '\0';
    fclose(r);
  } else {
    captured[0] = '\0';
  }

  // Precondition: the chosen date/location actually shifts Fajr by >= 1 min, so
  // "shows tomorrow" is distinguishable from "shows today".
  check_bool("next after isha today!=tomorrow (precondition)", strcmp(today_fajr, tomo_fajr) != 0);

  char expect[32];
  snprintf(expect, sizeof(expect), "fajr=%s", tomo_fajr);
  check_contains("next after isha shows tomorrow fajr", expect);
}

static void test_method(void) {
  printf("  method...\n");
  reset_config();

  // method (no arg): show current
  run(2, (char *[]){"m", "method", NULL});
  check_ret("method show ret", 0);
  check_contains("method show key", "kemenag");

  // method <name>: set directly
  run(3, (char *[]){"m", "method", "isna", NULL});
  check_ret("method set isna ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("method set isna cfg", strcmp(cfg.calculation_method, "isna") == 0);
  }

  // method <bad>: unknown lists available methods
  run(3, (char *[]){"m", "method", "nope", NULL});
  check_ret("method bad ret", 1);
  check_contains("method bad lists", "Available");
  check_contains("method bad key", "kemenag");

  // method custom: not selectable via `method <name>`, treated as unknown
  run(3, (char *[]){"m", "method", "custom", NULL});
  check_ret("method custom ret", 1);
  check_contains("method custom unknown", "Unknown method");

  // method --list
  run(3, (char *[]){"m", "method", "--list", NULL});
  check_ret("method list ret", 0);
  check_contains("method list mwl", "mwl");
  check_contains("method list current", "*");

  // method --auto: derive from the current country (ID -> kemenag)
  run(3, (char *[]){"m", "method", "mwl", NULL});
  run(4, (char *[]){"m", "location", "set", "--country=ID", NULL});
  run(3, (char *[]){"m", "method", "--auto", NULL});
  check_ret("method auto ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("method auto cfg", strcmp(cfg.calculation_method, "kemenag") == 0);
  }

  // method --auto: a different saved country derives that country's default,
  // proving derivation is table-driven (country_default_method), not hardcoded.
  run(4, (char *[]){"m", "location", "set", "--country=US", NULL});
  run(3, (char *[]){"m", "method", "--auto", NULL});
  check_ret("method auto US ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    const char *want = method_to_string(country_default_method("US"));
    check_bool("method auto US cfg", strcmp(cfg.calculation_method, want) == 0);
  }

  // method --help
  run(3, (char *[]){"m", "method", "--help", NULL});
  check_ret("method help ret", 0);
  check_contains("method help usage", "muslimtify method");

  // removed subcommands -> migration hints
  run(3, (char *[]){"m", "method", "show", NULL});
  check_ret("method show removed ret", 1);
  check_contains("method show removed msg", "was removed");

  run(4, (char *[]){"m", "method", "set", "mwl", NULL});
  check_ret("method set removed ret", 1);
  check_contains("method set removed msg", "method <name>");

  run(3, (char *[]){"m", "method", "list", NULL});
  check_ret("method list removed ret", 1);
  check_contains("method list removed msg", "--list");

  run(4, (char *[]){"m", "method", "madhab", "hanafi", NULL});
  check_ret("method madhab removed ret", 1);
  check_contains("method madhab removed msg", "madzhab");
}

static void test_madzhab(void) {
  printf("  madzhab...\n");
  reset_config();

  // madzhab (no arg): show current
  run(2, (char *[]){"m", "madzhab", NULL});
  check_ret("madzhab show ret", 0);
  check_contains("madzhab show val", "shafi");

  // madzhab <name>: set
  run(3, (char *[]){"m", "madzhab", "hanafi", NULL});
  check_ret("madzhab set ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("madzhab set cfg", strcmp(cfg.madhab, "hanafi") == 0);
  }

  // madzhab <bad>: unknown lists options
  run(3, (char *[]){"m", "madzhab", "maliki", NULL});
  check_ret("madzhab bad ret", 1);
  check_contains("madzhab bad lists", "Available: shafi, hanafi");

  // madzhab --list
  run(3, (char *[]){"m", "madzhab", "--list", NULL});
  check_ret("madzhab list ret", 0);
  check_contains("madzhab list shafi", "shafi");
  check_contains("madzhab list hanafi", "hanafi");

  // madzhab --help
  run(3, (char *[]){"m", "madzhab", "--help", NULL});
  check_ret("madzhab help ret", 0);
  check_contains("madzhab help usage", "muslimtify madzhab");
}

static void test_daemon_errors(void) {
  printf("  daemon errors...\n");
  reset_config();

  // daemon unknown subcommand
  run(3, (char *[]){"m", "daemon", "blah", NULL});
  check_ret("daemon unknown ret", 1);
}

static void test_offset(void) {
  printf("  offset...\n");
  reset_config();

  // offset fajr +4
  run(4, (char *[]){"m", "offset", "fajr", "+4", NULL});
  check_ret("offset fajr set ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("offset fajr value", cfg.fajr.offset == 4);
  }

  // offset asr -2 (negative)
  run(4, (char *[]){"m", "offset", "asr", "-2", NULL});
  check_ret("offset asr neg ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("offset asr value", cfg.asr.offset == -2);
  }

  // offset fajr 0 (reset)
  run(4, (char *[]){"m", "offset", "fajr", "0", NULL});
  check_ret("offset fajr reset ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("offset fajr reset", cfg.fajr.offset == 0);
  }

  // offset all 3 -> applies to all 7 including disabled sunrise
  reset_config();
  run(4, (char *[]){"m", "offset", "all", "3", NULL});
  check_ret("offset all ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("offset all fajr", cfg.fajr.offset == 3);
    check_bool("offset all sunrise", cfg.sunrise.offset == 3);
    check_bool("offset all isha", cfg.isha.offset == 3);
  }

  // out of range -> rejected, config unchanged
  reset_config();
  run(4, (char *[]){"m", "offset", "fajr", "61", NULL});
  check_ret("offset out of range ret", 1);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("offset unchanged after bad", cfg.fajr.offset == 0);
  }

  // non-numeric -> rejected
  run(4, (char *[]){"m", "offset", "fajr", "abc", NULL});
  check_ret("offset non-numeric ret", 1);

  // unknown prayer -> rejected
  run(4, (char *[]){"m", "offset", "badprayer", "4", NULL});
  check_ret("offset bad prayer ret", 1);

  // missing value -> usage error
  run(3, (char *[]){"m", "offset", "fajr", NULL});
  check_ret("offset missing value ret", 1);

  // offset value is persisted to config
  reset_config();
  run(4, (char *[]){"m", "offset", "maghrib", "+7", NULL});
  check_ret("offset maghrib for display ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("offset maghrib persisted", cfg.maghrib.offset == 7);
  }
}

static void test_output_helpers(void) {
  printf("  output helpers...\n");
  OutputMode m = OUTPUT_TABLE;

  check_bool("mode default table",
             cli_parse_output_mode(0, (char *[]){NULL}, &m) == 0 && m == OUTPUT_TABLE);
  check_bool("mode json",
             cli_parse_output_mode(1, (char *[]){"--json"}, &m) == 0 && m == OUTPUT_JSON);
  check_bool("mode headless",
             cli_parse_output_mode(1, (char *[]){"--headless"}, &m) == 0 && m == OUTPUT_HEADLESS);
  check_bool("mode conflict",
             cli_parse_output_mode(2, (char *[]){"--json", "--headless"}, &m) != 0);

  check_bool("wants help -h", cli_wants_help(1, (char *[]){"-h"}));
  check_bool("wants help --help", cli_wants_help(1, (char *[]){"--help"}));
  check_bool("no help on --json", !cli_wants_help(1, (char *[]){"--json"}));
}

static void test_notification(void) {
  printf("  notification...\n");
  reset_config();

  // settings view (table)
  run(2, (char *[]){"m", "notification", NULL});
  check_ret("notification default ret", 0);
  check_contains("notification default fajr", "fajr");
  check_contains("notification default sound", "sound:");
  check_contains("notification default urgency", "urgency:");

  // --json
  run(3, (char *[]){"m", "notification", "--json", NULL});
  check_ret("notification json ret", 0);
  check_contains("notification json sound", "\"sound\"");
  check_contains("notification json prayers", "\"prayers\"");

  // --headless
  run(3, (char *[]){"m", "notification", "--headless", NULL});
  check_ret("notification headless ret", 0);
  check_contains("notification headless sound", "sound=");
  check_contains("notification headless fajr", "fajr.enabled=");

  // mutual exclusion
  run(4, (char *[]){"m", "notification", "--json", "--headless", NULL});
  check_ret("notification json+headless ret", 1);
  check_contains("notification json+headless msg", "cannot be combined");

  // enable / disable a single prayer
  run(4, (char *[]){"m", "notification", "disable", "fajr", NULL});
  check_ret("notification disable fajr ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("notification disable fajr cfg", cfg.fajr.enabled == false);
  }
  run(4, (char *[]){"m", "notification", "enable", "fajr", NULL});
  check_ret("notification enable fajr ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("notification enable fajr cfg", cfg.fajr.enabled == true);
  }

  // enable all / disable all
  run(4, (char *[]){"m", "notification", "disable", "all", NULL});
  check_ret("notification disable all ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("notification disable all cfg", cfg.isha.enabled == false);
  }
  run(3, (char *[]){"m", "notification", "enable", NULL});
  check_ret("notification enable noarg ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("notification enable noarg cfg", cfg.isha.enabled == true);
  }

  // unknown prayer rejected
  run(4, (char *[]){"m", "notification", "enable", "bogus", NULL});
  check_ret("notification enable bogus ret", 1);

  // help
  run(3, (char *[]){"m", "notification", "--help", NULL});
  check_ret("notification help ret", 0);
  check_contains("notification help usage", "muslimtify notification");

  // --urgency
  run(4, (char *[]){"m", "notification", "--urgency", "low", NULL});
  check_ret("notification urgency ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("notification urgency cfg", strcmp(cfg.notification_urgency, "low") == 0);
  }
  run(4, (char *[]){"m", "notification", "--urgency", "bogus", NULL});
  check_ret("notification urgency bad ret", 1);

  // --reminder set / clear / --all
  run(6, (char *[]){"m", "notification", "--reminder", "fajr", "30", "15", NULL});
  check_ret("notification reminder ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("notification reminder cfg", cfg.fajr.reminder_count == 2 &&
                                                cfg.fajr.reminders[0] == 30 &&
                                                cfg.fajr.reminders[1] == 15);
  }
  run(5, (char *[]){"m", "notification", "--reminder", "fajr", "none", NULL});
  check_ret("notification reminder clear ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("notification reminder clear cfg", cfg.fajr.reminder_count == 0);
  }
  run(5, (char *[]){"m", "notification", "--reminder", "--all", "10", NULL});
  check_ret("notification reminder all ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("notification reminder all cfg",
               cfg.dhuhr.reminder_count == 1 && cfg.dhuhr.reminders[0] == 10);
  }

  // --adhan enable/disable/set
  run(5, (char *[]){"m", "notification", "--adhan", "enable", "fajr", NULL});
  check_ret("notification adhan enable ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("notification adhan enable cfg", cfg.fajr.adhan_enabled == true);
  }
  run(5, (char *[]){"m", "notification", "--adhan", "disable", "fajr", NULL});
  check_ret("notification adhan disable ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("notification adhan disable cfg", cfg.fajr.adhan_enabled == false);
  }

  // unknown prayer lists the available names
  run(5, (char *[]){"m", "notification", "--adhan", "disable", "zuzu", NULL});
  check_ret("notification adhan unknown ret", 1);
  check_contains("notification adhan unknown lists", "Available:");
  check_contains("notification adhan unknown fajr", "fajr");
  // --adhan set is zero-trust: existing regular file only, canonicalized to an
  // absolute path; missing paths and symlinks are rejected.
  {
    char realf[512], linkf[512];
    snprintf(realf, sizeof(realf), "%s/adhan_real.mp3", tmpdir);
    snprintf(linkf, sizeof(linkf), "%s/adhan_link.mp3", tmpdir);
    FILE *af = fopen(realf, "w");
    if (af) {
      fputs("x", af);
      fclose(af);
    }

    run(5, (char *[]){"m", "notification", "--adhan", "set", realf, NULL});
    check_ret("notification adhan set ret", 0);
    {
      Config cfg;
      config_load(&cfg);
      check_bool("notification adhan set stored abs",
                 cfg.fajr.adhan[0] == '/' && strstr(cfg.fajr.adhan, "adhan_real.mp3") != NULL);
    }

    run(5, (char *[]){"m", "notification", "--adhan", "set", "/no/such/adhan.mp3", NULL});
    check_ret("notification adhan set missing ret", 1);

#ifndef _WIN32
    if (symlink(realf, linkf) != 0) {
      check_bool("notification adhan symlink setup", false);
    } else {
      run(5, (char *[]){"m", "notification", "--adhan", "set", linkf, NULL});
      check_ret("notification adhan set symlink ret", 1);
      check_contains("notification adhan set symlink msg", "symlink");
    }
#endif
  }

  // --adhan stop works on every platform (no-op when nothing is playing)
  run(4, (char *[]){"m", "notification", "--adhan", "stop", NULL});
  check_ret("notification adhan stop ret", 0);
  check_contains("notification adhan stop msg", "adhan");

  // --sound modes + invalid
  run(4, (char *[]){"m", "notification", "--sound", "off", NULL});
  check_ret("notification sound off ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("notification sound off cfg", strcmp(cfg.notification_sound, "off") == 0);
  }
  run(4, (char *[]){"m", "notification", "--sound", "adhan", NULL});
  check_ret("notification sound adhan ret", 0);
  run(4, (char *[]){"m", "notification", "--sound", "bogus", NULL});
  check_ret("notification sound bad ret", 1);

  // per-flag help
  run(4, (char *[]){"m", "notification", "--reminder", "--help", NULL});
  check_ret("notification reminder help ret", 0);
  check_contains("notification reminder help usage", "--reminder");
}

// -- main ---------------------------------------------------------------------

int main(void) {
  setup();

  printf("Running CLI tests...\n");
  test_version_and_help();
  test_output_helpers();
  test_location();
  test_removed_top_level();
  test_show();
  test_show_range();
  test_next();
  test_next_after_isha();
  test_method();
  test_madzhab();
  test_notification();
  test_daemon_errors();
  test_offset();

  printf("\nResults: %d passed, %d failed\n", passed, failed);
  teardown();
  return failed > 0 ? 1 : 0;
}
