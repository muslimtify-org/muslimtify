#define _GNU_SOURCE

#include "cli.h"
#include "cli_internal.h"
#include "config.h"
#include "country.h"
#include "display.h"
#include "platform.h"
#include "prayertimes.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
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

/* Returns true when `s` contains a comma whose next non-whitespace character
   closes an object or an array, which is the trailing comma a strict JSON
   parser rejects. Commas inside string literals are skipped, so a city name
   containing one cannot raise a false alarm. */
static bool has_trailing_comma(const char *s) {
  bool in_string = false;
  for (const char *p = s; *p; p++) {
    if (in_string) {
      if (*p == '\\' && p[1])
        p++;
      else if (*p == '"')
        in_string = false;
      continue;
    }
    if (*p == '"') {
      in_string = true;
      continue;
    }
    if (*p != ',')
      continue;
    const char *q = p + 1;
    while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r')
      q++;
    if (*q == '}' || *q == ']')
      return true;
  }
  return false;
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

  // refresh_interval appears in headless + json output
  run(4, (char *[]){"m", "location", "set", "--refresh-interval=21600", NULL});
  run(3, (char *[]){"m", "location", "--headless", NULL});
  check_contains("location headless has refresh_interval", "refresh_interval=21600");
  run(3, (char *[]){"m", "location", "--json", NULL});
  check_contains("location json has refresh_interval", "\"refresh_interval\":");

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

  reset_config();

  // Valid: sets the interval (seconds stored as-is). The display of the stored
  // value is asserted in Task 5 (needs the display.c headless/json changes).
  run(4, (char *[]){"m", "location", "set", "--refresh-interval=21600", NULL});
  check_ret("refresh-interval set ret", 0);

  // Valid: exactly the floor.
  run(4, (char *[]){"m", "location", "set", "--refresh-interval=3600", NULL});
  check_ret("refresh-interval floor ret", 0);

  // Valid: 0 disables.
  run(4, (char *[]){"m", "location", "set", "--refresh-interval=0", NULL});
  check_ret("refresh-interval disable ret", 0);

  // Invalid: below the floor is rejected.
  run(4, (char *[]){"m", "location", "set", "--refresh-interval=1800", NULL});
  check_ret("refresh-interval below-floor rejected", 1);

  // Invalid: non-numeric is rejected.
  run(4, (char *[]){"m", "location", "set", "--refresh-interval=abc", NULL});
  check_ret("refresh-interval non-numeric rejected", 1);
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
  check_bool("show headless not capitalized", strstr(captured, "Fajr=") == NULL);

  // a disabled prayer drops out, and enable all brings it back
  run(4, (char *[]){"m", "notification", "disable", "asr", NULL});
  run(3, (char *[]){"m", "show", "--headless", NULL});
  check_bool("show headless no asr", strstr(captured, "asr=") == NULL);
  run(4, (char *[]){"m", "notification", "enable", "all", NULL});
  run(3, (char *[]){"m", "show", "--headless", NULL});
  check_contains("show headless asr", "asr=");

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

static void test_show_date_bounds(void) {
  printf("  show --date bounds...\n");
  reset_config();

  // year 0 is not a valid ISO year
  run(4, (char *[]){"m", "show", "--date", "0000-01-01", NULL});
  check_ret("bounds year 0 ret", 1);
  check_contains("bounds year 0 msg", "Invalid date");

  // negative year: previously accepted and printed as "-005-01-01"
  run(4, (char *[]){"m", "show", "--date", "-5-01-01", NULL});
  check_ret("bounds year negative ret", 1);

  // year that overflows int: previously undefined behavior via sscanf("%d")
  run(4, (char *[]){"m", "show", "--date", "99999999999999999999-01-01", NULL});
  check_ret("bounds year overflow ret", 1);

  // year above the 4-digit ISO range
  run(4, (char *[]){"m", "show", "--date", "10000-01-01", NULL});
  check_ret("bounds year 10000 ret", 1);

  // an explicit plus sign is not a date
  run(4, (char *[]){"m", "show", "--date", "+2024-01-01", NULL});
  check_ret("bounds year plus-sign ret", 1);

  // leading whitespace is not a date
  run(4, (char *[]){"m", "show", "--date", " 2024-01-01", NULL});
  check_ret("bounds year leading-space ret", 1);

  // lower and upper boundaries are accepted
  run(4, (char *[]){"m", "show", "--date", "0001-01-01", NULL});
  check_ret("bounds year min ret", 0);

  run(4, (char *[]){"m", "show", "--date", "9999-12-31", NULL});
  check_ret("bounds year max ret", 0);

  // exactly 366 days (a full leap year) is the inclusive cap and is accepted.
  // Only the exit code is asserted: a 366-day range overflows the 16384-byte
  // `captured` buffer, so output assertions would be checking truncated text.
  run(5, (char *[]){"m", "show", "--date", "2024-01-01", "2024-12-31", NULL});
  check_ret("bounds span 366 ret", 0);

  // a full non-leap year is 365 days and stays well inside the cap
  run(5, (char *[]){"m", "show", "--date", "2023-01-01", "2023-12-31", NULL});
  check_ret("bounds span 365 ret", 0);

  // 367 days is one past the cap and is rejected
  run(5, (char *[]){"m", "show", "--date", "2024-01-01", "2025-01-01", NULL});
  check_ret("bounds span 367 ret", 1);
  check_contains("bounds span 367 msg", "date range too long");

  // over-wide fields are rejected: "00002024" is not a 4-digit ISO year
  run(4, (char *[]){"m", "show", "--date", "00002024-01-01", NULL});
  check_ret("bounds year overpadded ret", 1);

  run(4, (char *[]){"m", "show", "--date", "2024-0001-01", NULL});
  check_ret("bounds month overpadded ret", 1);

  run(4, (char *[]){"m", "show", "--date", "2024-01-0001", NULL});
  check_ret("bounds day overpadded ret", 1);

  // ...but short (non-zero-padded) components remain valid, as before
  run(4, (char *[]){"m", "show", "--date", "2024-1-1", NULL});
  check_ret("bounds short form ret", 0);

  // the help text documents both limits
  run(4, (char *[]){"m", "show", "--date", "--help", NULL});
  check_ret("bounds help ret", 0);
  check_contains("bounds help span limit", "366 days");
  check_contains("bounds help year range", "1-9999");
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

  // single-day JSON has the same shape as one range entry
  run(5, (char *[]){"m", "show", "--date", "2022-01-01", "--json", NULL});
  check_ret("range single json ret", 0);
  check_contains("range single json date key", "\"date\": \"2022-01-01\"");
  check_contains("range single json prayers", "\"prayers\"");

  // headless range: date= blocks, enabled prayers only
  run(6, (char *[]){"m", "show", "--date", "2022-01-01", "2022-01-02", "--headless", NULL});
  check_ret("range headless ret", 0);
  check_contains("range headless date1", "date=2022-01-01");
  check_contains("range headless date2", "date=2022-01-02");
  check_contains("range headless fajr", "fajr=");

  // table range (default): pivoted table, one row per day, columns = enabled prayers only
  run(5, (char *[]){"m", "show", "--date", "2022-01-01", "2022-01-02", NULL});
  check_ret("range table ret", 0);
  check_contains("range table header date", "Date");
  check_contains("range table col fajr", "Fajr");
  check_contains("range table col dhuhr", "Dhuhr");
  check_contains("range table col isha", "Isha");
  check_contains("range table d1", "2022-01-01");
  check_contains("range table d2", "2022-01-02");
  // a disabled prayer is hidden as a column
  run(4, (char *[]){"m", "notification", "disable", "asr", NULL});
  run(5, (char *[]){"m", "show", "--date", "2022-01-01", "2022-01-02", NULL});
  check_bool("range table hides asr", strstr(captured, "Asr") == NULL);
  run(4, (char *[]){"m", "notification", "enable", "all", NULL});
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

// A "marked" time is HH:MM immediately followed by + or -, e.g. "00:06+".
// That suffix is the day marker: format_time_hm_day() appends it, and only
// the two show tables (single-day and range) call format_time_hm_day().
// Every other surface calls plain format_time_hm() and prints the bare
// HH:MM, so this scans for the marker shape rather than a fixed time.
static bool has_time_marker(const char *s) {
  for (const char *p = s; *p; p++) {
    if (isdigit((unsigned char)p[0]) && isdigit((unsigned char)p[1]) && p[2] == ':' &&
        isdigit((unsigned char)p[3]) && isdigit((unsigned char)p[4]) &&
        (p[5] == '+' || p[5] == '-')) {
      return true;
    }
  }
  return false;
}

// Goal 6 (only the two tables carry the marker, nothing else does) is
// checked by grep in Task 4 step 4, since a C test cannot count call sites.
// What this function adds is the observable half: the assertions below say
// that both tables actually mark and render the legend, and that every
// other surface (--next, its headless form, and notification) does not,
// which is what that call-site count is a proxy for.
//
// Legend text is matched as a whole line, since the brief gives the exact
// printed strings and a whole line is no less stable than a fragment of one.
// Markers are matched as a short "HH:MM+"/"HH:MM-" substring instead of a
// whole table row, since the row's column padding is incidental but the
// digits-colon-digits-marker shape is what the feature promises.
//
// Mutation record. Each mutant below was applied to src/core/display.c by
// hand, built, run against `ctest --test-dir build -R cli --output-on-failure`,
// then reverted with `git checkout -- src/core/display.c` before the next one.
// git status was confirmed empty after each revert. All three were caught.
//
// Mutant 1: print_day_marker_legend() made to print the next-day line
// unconditionally, dropping the `if (any_next)` guard. Caught by the Jakarta
// no-legend check. Output:
//   FAIL [markers jakarta no legend]
//
// Mutant 2: both `if (day != 0)` guards on the headless `_offset` key
// removed (single-day and range headless paths), so the key is always
// emitted. Caught by the Jakarta headless check. Output:
//   FAIL [markers jakarta headless no offset]
//
// Mutant 3: print_day_marker_legend() made to never print the previous-day
// line, dropping the `if (any_prev)` branch entirely. Caught by the Eureka
// legend check. Output:
//   FAIL [markers eureka legend]: output missing "  - falls before midnight, on the previous day"
//   got: +----------------------------------------------------------+
//   | Date       | Fajr   | Dhuhr  | Asr    | Maghrib | Isha   |
//   +----------------------------------------------------------+
//   | 2026-02-25 | 00...
static void test_day_markers(void) {
  printf("  day markers...\n");

  // Step 2: legend present. At Reykjavik, Isha on 2026-04-07 falls just
  // after midnight, so the range table marks it and prints the next-day
  // legend.
  run(7, (char *[]){"m", "location", "set", "--lat=64.1466", "--long=-21.9426",
                    "--timezone=Atlantic/Reykjavik", "--country=IS", NULL});
  run(3, (char *[]){"m", "method", "mwl", NULL});
  run(5, (char *[]){"m", "show", "--date", "2026-04-07", "2026-04-08", NULL});
  check_ret("markers reykjavik range ret", 0);
  check_contains("markers reykjavik has marker", "00:06+");
  check_contains("markers reykjavik legend", "  + falls after midnight, on the next day");

  // Step 3: legend absent. Jakarta has no midnight crossing on the same
  // dates, so the range table has neither a marker nor the legend.
  run(7, (char *[]){"m", "location", "set", "--lat=-6.2088", "--long=106.8456",
                    "--timezone=Asia/Jakarta", "--country=ID", NULL});
  run(3, (char *[]){"m", "method", "mwl", NULL});
  run(5, (char *[]){"m", "show", "--date", "2026-04-07", "2026-04-08", NULL});
  check_ret("markers jakarta range ret", 0);
  check_contains("markers jakarta has output", "2026-04-07");
  check_bool("markers jakarta no legend", strstr(captured, "falls after midnight") == NULL &&
                                              strstr(captured, "falls before midnight") == NULL);
  check_bool("markers jakarta no marker", !has_time_marker(captured));

  // Step 4: the previous-day legend. Eureka, Nunavut sits far enough east of
  // its America/Edmonton zone meridian that Fajr falls just before midnight
  // on the clock. Derived by scanning all of 2026 through the built binary
  // in headless mode rather than pasted: the previous-day legend fires
  // exactly twice in the year, both for Fajr, on 2026-02-27 (23:54) and
  // 2026-08-30 (23:55). A full year does not fit in the 16384-byte
  // `captured` buffer (see the 366-day note in test_show_date_bounds), so
  // this renders a narrow range around only the first of those two dates.
  run(7, (char *[]){"m", "location", "set", "--lat=79.9889", "--long=-85.9408",
                    "--timezone=America/Edmonton", "--country=CA", NULL});
  run(3, (char *[]){"m", "method", "mwl", NULL});
  run(5, (char *[]){"m", "show", "--date", "2026-02-25", "2026-03-01", NULL});
  check_ret("markers eureka range ret", 0);
  check_contains("markers eureka has marker", "23:54-");
  check_contains("markers eureka legend", "  - falls before midnight, on the previous day");

  // Step 5: headless keys. At Reykjavik, the headless isha key prints the
  // bare time (the marker character is a table-only device) plus a separate
  // _offset line.
  run(7, (char *[]){"m", "location", "set", "--lat=64.1466", "--long=-21.9426",
                    "--timezone=Atlantic/Reykjavik", "--country=IS", NULL});
  run(3, (char *[]){"m", "method", "mwl", NULL});
  run(5, (char *[]){"m", "show", "--date", "2026-04-07", "--headless", NULL});
  check_ret("markers reykjavik headless ret", 0);
  check_contains("markers reykjavik headless isha bare", "isha=00:06\n");
  check_contains("markers reykjavik headless offset", "isha_offset=1");

  // Jakarta never crosses midnight on this date, so no _offset key
  // appears at all.
  run(7, (char *[]){"m", "location", "set", "--lat=-6.2088", "--long=106.8456",
                    "--timezone=Asia/Jakarta", "--country=ID", NULL});
  run(3, (char *[]){"m", "method", "mwl", NULL});
  run(5, (char *[]){"m", "show", "--date", "2026-04-07", "--headless", NULL});
  check_ret("markers jakarta headless ret", 0);
  check_contains("markers jakarta headless has isha", "isha=19:01");
  check_bool("markers jakarta headless no offset", strstr(captured, "_offset") == NULL);

  // Step 6: no other surface carries the marker. Reuse Reykjavik, a
  // location the table above proves does mark, and check `show --next`,
  // its headless form, and the notification settings view already
  // exercised near test_notification.
  run(7, (char *[]){"m", "location", "set", "--lat=64.1466", "--long=-21.9426",
                    "--timezone=Atlantic/Reykjavik", "--country=IS", NULL});
  run(3, (char *[]){"m", "method", "mwl", NULL});

  run(3, (char *[]){"m", "show", "--next", NULL});
  check_ret("markers next ret", 0);
  check_contains("markers next has output", "Remaining");
  check_bool("markers next no marker", !has_time_marker(captured));

  run(4, (char *[]){"m", "show", "--next", "--headless", NULL});
  check_ret("markers next headless ret", 0);
  check_contains("markers next headless has output", "remaining=");
  check_bool("markers next headless no marker", !has_time_marker(captured));

  run(2, (char *[]){"m", "notification", NULL});
  check_ret("markers notification ret", 0);
  check_contains("markers notification has output", "fajr");
  check_bool("markers notification no marker", !has_time_marker(captured));
}

static void test_next(void) {
  printf("  show --next...\n");
  reset_config();

  // default: bordered table
  run(3, (char *[]){"m", "show", "--next", NULL});
  check_ret("next table ret", 0);
  check_contains("next table border", "+------------+");
  check_contains("next table remaining", "Remaining");
  check_contains("next table date", "| Date ");

  // headless
  run(4, (char *[]){"m", "show", "--next", "--headless", NULL});
  check_ret("next headless ret", 0);
  check_contains("next headless remaining", "remaining=");
  check_contains("next headless date", "date=");

  // json
  run(4, (char *[]){"m", "show", "--next", "--json", NULL});
  check_ret("next json ret", 0);
  check_contains("next json prayer", "\"prayer\"");
  check_contains("next json remaining", "\"remaining\"");
  check_contains("next json date", "\"date\"");

  // per-mode help
  run(4, (char *[]){"m", "show", "--next", "--help", NULL});
  check_ret("next help ret", 0);
  check_contains("next help usage", "--next");

  // bare `next` is removed -> unknown command
  run(2, (char *[]){"m", "next", NULL});
  check_ret("bare next removed ret", 1);
  check_contains("bare next removed msg", "Unknown command");
}

// Dates are derived from the clock the same way cmd_show.c derives them, so a
// run that straddles local midnight can see a one-day mismatch.
static void test_day_offset(void) {
  printf("  show --day-offset...\n");
  reset_config();

  time_t now = time(NULL);
  struct tm tm_buf;
  platform_localtime(&now, &tm_buf);
  long today = mt_days_from_civil(tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday);
  int y, m, d;
  char tomorrow[16], yesterday[16];
  mt_civil_from_days(today + 1, &y, &m, &d);
  snprintf(tomorrow, sizeof(tomorrow), "%04d-%02d-%02d", y, m, d);
  mt_civil_from_days(today - 1, &y, &m, &d);
  snprintf(yesterday, sizeof(yesterday), "%04d-%02d-%02d", y, m, d);

  // +1 renders exactly what --date renders for tomorrow: the date and the
  // prayer times computed for that date, not today's times under a new date.
  static char expected[sizeof(captured)];
  run(5, (char *[]){"m", "show", "--date", tomorrow, "--json", NULL});
  strcpy(expected, captured);
  run(5, (char *[]){"m", "show", "--day-offset", "1", "--json", NULL});
  check_ret("day offset +1 json ret", 0);
  check_bool("day offset +1 json matches --date", strcmp(captured, expected) == 0);

  char want[32];
  snprintf(want, sizeof(want), "date=%s\n", yesterday);
  run(5, (char *[]){"m", "show", "--day-offset", "-1", "--headless", NULL});
  check_ret("day offset -1 headless ret", 0);
  check_contains("day offset -1 headless date", want);

  run(4, (char *[]){"m", "show", "--day-offset", "+1", NULL});
  check_ret("day offset +1 table ret", 0);
  check_contains("day offset +1 table date", tomorrow);

  // atoi used to turn all of these into a silent 0
  const char *bad[] = {"abc", "1x", "", " 1", "99999999"};
  for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
    char name[64];
    snprintf(name, sizeof(name), "day offset bad [%s] ret", bad[i]);
    run(4, (char *[]){"m", "show", "--day-offset", (char *)bad[i], NULL});
    check_ret(name, 1);
    snprintf(name, sizeof(name), "day offset bad [%s] msg", bad[i]);
    check_contains(name, "Invalid day offset");
  }

  run(3, (char *[]){"m", "show", "--day-offset", NULL});
  check_ret("day offset missing ret", 1);
  check_contains("day offset missing msg", "(missing)");

  run(4, (char *[]){"m", "show", "--day-offset", "-999999", NULL});
  check_ret("day offset before year 1 ret", 1);
  check_contains("day offset before year 1 msg", "outside years 1-9999");

  run(5, (char *[]){"m", "show", "--day-offset", "1", "--next", NULL});
  check_ret("day offset with next ret", 1);
  check_contains("day offset with next msg", "cannot be combined");

  run(4, (char *[]){"m", "show", "--day-offset", "--help", NULL});
  check_ret("day offset help ret", 0);
  check_contains("day offset help usage", "--day-offset <offset>");
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

  // offset all 3 -> applies to all five
  reset_config();
  run(4, (char *[]){"m", "offset", "all", "3", NULL});
  check_ret("offset all ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("offset all fajr", cfg.fajr.offset == 3);
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

static void test_location_set_timezone_validation(void) {
  printf("test_location_set_timezone_validation\n");
  reset_config();

  // A real UTC+0 zone (GMT year-round, no DST) must be accepted. The old
  // heuristic (offset==0.0 && !is_utc_zone) wrongly rejected it. Issue #51.
  run(4, (char *[]){"m", "location", "set", "--timezone=Africa/Abidjan", NULL});
  check_ret("real UTC+0 zone accepted", 0);
  check_contains("real UTC+0 zone confirmed", "Africa/Abidjan");

  // A format-valid but nonexistent zone must be rejected.
  run(4, (char *[]){"m", "location", "set", "--timezone=Asia/Nowhere", NULL});
  check_bool("nonexistent zone rejected", last_ret != 0);
  check_contains("nonexistent zone error", "Unknown timezone");
}

// Regression coverage for the trailing comma the prayer count drop from
// seven to five left behind (fixed in commit 7a5b631). Every other JSON
// assertion in this file is a substring check_contains, and a trailing
// comma does not disturb a substring match, so none of them would have
// caught it. This test runs a real JSON syntax check instead.
//
// Mutation record. Each mutant below was applied by hand, built with
// `cmake --build build -j`, run against
// `ctest --test-dir build -R cli --output-on-failure`, then reverted with
// `git checkout -- <path>` before the next one. git status --porcelain was
// confirmed empty after each revert. All three were caught.
//
// Mutant 1: src/core/display.c:315, inside print_prayer_entries, changed
// `i + 1 < PRAYER_COUNT` back to `i < 6`. Caught by the show and show --date
// checks for both fixtures. Output:
//   FAIL [jakarta show json no trailing comma]
//   FAIL [jakarta show date json no trailing comma]
//   FAIL [reykjavik show json no trailing comma]
//   FAIL [reykjavik show date json no trailing comma]
//   Results: 357 passed, 4 failed
//
// Mutant 2: src/core/display.c:727, inside
// display_notification_settings_json, changed `i + 1 < PRAYER_COUNT` back to
// `i < 6`, leaving mutant 1's site fixed. A scan covering only the show
// commands would have passed this mutant. Caught by the notification check
// for both fixtures. Output:
//   FAIL [jakarta notification json no trailing comma]
//   FAIL [reykjavik notification json no trailing comma]
//   Results: 359 passed, 2 failed
//
// Mutant 3: tests/test_cli.c has_trailing_comma() made to return false
// unconditionally. This checks the scanner is load bearing rather than
// decorative. As expected, it did not fail any of the five command checks,
// since a scanner that always returns false trivially satisfies an
// assertion of absence. It failed the scanner's own self-checks instead.
// Output:
//   FAIL [scanner true before brace]
//   FAIL [scanner true before bracket]
//   Results: 359 passed, 2 failed
static void test_json_no_trailing_comma(void) {
  printf("  json no trailing comma...\n");

  // Step 2: prove the scanner itself fires and does not fire on the wrong
  // things, before trusting it to grade real CLI output.
  check_bool("scanner true before brace", has_trailing_comma("{\"a\": 1,}"));
  check_bool("scanner true before bracket", has_trailing_comma("[1, 2,]"));
  check_bool("scanner false well formed", !has_trailing_comma("{\"a\": 1}"));
  check_bool("scanner false comma in string", !has_trailing_comma("{\"a\": \"Jakarta, ID\"}"));

  // Step 3: Jakarta, over all five JSON-emitting commands.
  reset_config();
  run(3, (char *[]){"m", "method", "mwl", NULL});

  run(3, (char *[]){"m", "show", "--json", NULL});
  check_contains("jakarta show json has output", "{");
  check_bool("jakarta show json no trailing comma", !has_trailing_comma(captured));

  run(4, (char *[]){"m", "show", "--next", "--json", NULL});
  check_contains("jakarta show next json has output", "{");
  check_bool("jakarta show next json no trailing comma", !has_trailing_comma(captured));

  run(6, (char *[]){"m", "show", "--date", "2026-04-07", "2026-04-08", "--json", NULL});
  check_contains("jakarta show date json has output", "{");
  check_bool("jakarta show date json no trailing comma", !has_trailing_comma(captured));

  run(3, (char *[]){"m", "location", "--json", NULL});
  check_contains("jakarta location json has output", "{");
  check_bool("jakarta location json no trailing comma", !has_trailing_comma(captured));

  run(3, (char *[]){"m", "notification", "--json", NULL});
  check_contains("jakarta notification json has output", "{");
  check_bool("jakarta notification json no trailing comma", !has_trailing_comma(captured));

  // Step 4: Reykjavik, which puts an offset field into the show JSON.
  run(7, (char *[]){"m", "location", "set", "--lat=64.1466", "--long=-21.9426",
                    "--timezone=Atlantic/Reykjavik", "--country=IS", NULL});
  run(3, (char *[]){"m", "method", "mwl", NULL});

  run(3, (char *[]){"m", "show", "--json", NULL});
  check_contains("reykjavik show json has output", "{");
  check_bool("reykjavik show json no trailing comma", !has_trailing_comma(captured));

  run(4, (char *[]){"m", "show", "--next", "--json", NULL});
  check_contains("reykjavik show next json has output", "{");
  check_bool("reykjavik show next json no trailing comma", !has_trailing_comma(captured));

  run(6, (char *[]){"m", "show", "--date", "2026-04-07", "2026-04-08", "--json", NULL});
  check_contains("reykjavik show date json has output", "{");
  check_bool("reykjavik show date json no trailing comma", !has_trailing_comma(captured));

  run(3, (char *[]){"m", "location", "--json", NULL});
  check_contains("reykjavik location json has output", "{");
  check_bool("reykjavik location json no trailing comma", !has_trailing_comma(captured));

  run(3, (char *[]){"m", "notification", "--json", NULL});
  check_contains("reykjavik notification json has output", "{");
  check_bool("reykjavik notification json no trailing comma", !has_trailing_comma(captured));
}

// Write a config file that config_load refuses. A command that loads the config
// before checking its arguments then fails with "Failed to load config" instead
// of the argument error, and nothing can reach the network on the way.
static void corrupt_config(void) {
  FILE *f = fopen(config_get_path(), "w");
  if (f) {
    fputs("{not json", f);
    fclose(f);
  }
}

static void test_show_args(void) {
  printf("  show argument validation...\n");
  reset_config();

  // Arguments after --day-offset and its value are still checked.
  run(6, (char *[]){"m", "show", "--day-offset", "1", "junk", "--headless", NULL});
  check_ret("show args junk after day offset ret", 1);
  check_contains("show args junk after day offset msg", "unknown option 'junk'");

  // --date takes at most two dates.
  run(6, (char *[]){"m", "show", "--date", "2024-01-01", "2024-01-03", "2024-01-05", NULL});
  check_ret("show args third date ret", 1);
  check_contains("show args third date msg", "unknown option '2024-01-05'");

  // Removed-flag hints still fire after --date.
  run(5, (char *[]){"m", "show", "--date", "2024-01-01", "--no-header", NULL});
  check_ret("show args no-header after date ret", 1);
  check_contains("show args no-header after date msg", "use '--headless'");

  // --day-offset takes exactly one value.
  run(5, (char *[]){"m", "show", "--day-offset", "1", "2", NULL});
  check_ret("show args two offsets ret", 1);
  check_contains("show args two offsets msg", "unknown option '2'");

  // Valid forms still pass, with output flags before or after the values.
  run(6, (char *[]){"m", "show", "--headless", "--date", "2024-01-01", "2024-01-02", NULL});
  check_ret("show args flag before range ret", 0);
  check_contains("show args flag before range d2", "date=2024-01-02");
  run(5, (char *[]){"m", "show", "--day-offset", "-1", "--json", NULL});
  check_ret("show args negative offset json ret", 0);

  // --next cannot be combined with --date, in either order.
  run(5, (char *[]){"m", "show", "--date", "2024-01-01", "--next", NULL});
  check_ret("show args date with next ret", 1);
  check_contains("show args date with next msg", "--next cannot be combined with --date");
  run(5, (char *[]){"m", "show", "--next", "--date", "2024-01-01", NULL});
  check_ret("show args next with date ret", 1);

  // Bad values are reported before the config is loaded, so a fresh install
  // never detects its location just to print an argument error.
  corrupt_config();
  run(4, (char *[]){"m", "show", "--date", "bogus", NULL});
  check_ret("show args bad date before config ret", 1);
  check_contains("show args bad date before config msg", "Invalid date bogus");
  run(4, (char *[]){"m", "show", "--day-offset", "abc", NULL});
  check_ret("show args bad offset before config ret", 1);
  check_contains("show args bad offset before config msg", "Invalid day offset abc");
  run(5, (char *[]){"m", "show", "--date", "2024-01-02", "2024-01-01", NULL});
  check_ret("show args reversed range before config ret", 1);
  check_contains("show args reversed range before config msg", "end date is before start date");
  reset_config();
}

static int fajr_reminder_count(void) {
  Config cfg;
  if (config_load(&cfg) != 0)
    return -1;
  return cfg.fajr.reminder_count;
}

static void test_reminder_args(void) {
  printf("  notification --reminder validation...\n");
  reset_config();

  run(6, (char *[]){"m", "notification", "--reminder", "fajr", "30", "15", NULL});
  check_ret("reminder args seed ret", 0);

  // --all with no minutes used to clear every prayer's reminders.
  run(4, (char *[]){"m", "notification", "--reminder", "--all", NULL});
  check_ret("reminder args all no minutes ret", 1);
  check_contains("reminder args all no minutes msg", "at least one minute value");
  check_bool("reminder args all no minutes keeps fajr", fajr_reminder_count() == 2);

  // The per-prayer form with no minutes is rejected the same way.
  run(4, (char *[]){"m", "notification", "--reminder", "fajr", NULL});
  check_ret("reminder args prayer no minutes ret", 1);
  check_contains("reminder args prayer no minutes msg", "at least one minute value");
  check_bool("reminder args prayer no minutes keeps fajr", fajr_reminder_count() == 2);

  // Values past MAX_REMINDERS used to be dropped without being checked.
  run(16, (char *[]){"m", "notification", "--reminder", "fajr", "1", "2", "3", "4", "5", "6", "7",
                     "8", "9", "10", "11", "abc", NULL});
  check_ret("reminder args eleven plus junk ret", 1);
  check_contains("reminder args eleven plus junk msg", "at most 10 reminder values");
  check_bool("reminder args eleven plus junk keeps fajr", fajr_reminder_count() == 2);

  run(15, (char *[]){"m", "notification", "--reminder", "fajr", "1", "2", "3", "4", "5", "6", "7",
                     "8", "9", "10", "11", NULL});
  check_ret("reminder args eleven ret", 1);
  check_bool("reminder args eleven keeps fajr", fajr_reminder_count() == 2);

  run(15, (char *[]){"m", "notification", "--reminder", "--all", "1", "2", "3", "4", "5", "6", "7",
                     "8", "9", "10", "11", NULL});
  check_ret("reminder args all eleven ret", 1);
  check_bool("reminder args all eleven keeps fajr", fajr_reminder_count() == 2);

  // Exactly MAX_REMINDERS values are accepted.
  run(14, (char *[]){"m", "notification", "--reminder", "fajr", "1", "2", "3", "4", "5", "6", "7",
                     "8", "9", "10", NULL});
  check_ret("reminder args ten ret", 0);
  check_bool("reminder args ten stored", fajr_reminder_count() == 10);

  // none and clear still clear on purpose.
  run(5, (char *[]){"m", "notification", "--reminder", "--all", "clear", NULL});
  check_ret("reminder args all clear ret", 0);
  check_bool("reminder args all clear cfg", fajr_reminder_count() == 0);
}

// `location set --auto` fetches over the network, so the config is corrupted
// first: a rejection that came after config_load would fail on the config
// instead, and never reach the fetch.
static void test_location_auto_args(void) {
  printf("  location set --auto validation...\n");

  corrupt_config();
  run(5, (char *[]){"m", "location", "set", "--auto", "--refresh-interval=3600", NULL});
  check_ret("location auto refresh ret", 1);
  check_contains("location auto refresh msg", "cannot be combined");

  corrupt_config();
  run(5, (char *[]){"m", "location", "set", "--auto", "--country=ZZZ", NULL});
  check_ret("location auto bad country ret", 1);
  check_contains("location auto bad country msg", "Invalid country code 'ZZZ'");

  reset_config();
}

// Commands used to ignore arguments past the ones they read and exit 0. Each
// case checks the error and that the config was not changed. daemon and
// notification test are left out on purpose: if their check ever regressed,
// the test would reach systemctl, schtasks or a real notification.
static void test_extra_args(void) {
  printf("  extra arguments...\n");
  reset_config();
  Config before;
  config_load(&before);

  run(4, (char *[]){"m", "notification", "disable", "all", NULL});
  run(5, (char *[]){"m", "notification", "enable", "fajr", "isha", NULL});
  check_ret("extra notification enable ret", 1);
  check_contains("extra notification enable msg",
                 "unexpected argument 'isha' for 'notification enable'");
  {
    Config cfg;
    config_load(&cfg);
    check_bool("extra notification enable leaves fajr", !cfg.fajr.enabled);
    check_bool("extra notification enable leaves isha", !cfg.isha.enabled);
  }
  run(5, (char *[]){"m", "notification", "enable", "all", "x", NULL});
  check_ret("extra notification enable all ret", 1);
  run(4, (char *[]){"m", "notification", "enable", "all", NULL});
  check_ret("extra notification enable restore ret", 0);

  run(4, (char *[]){"m", "notification", "enable", "--help", NULL});
  check_ret("extra notification enable help ret", 0);
  check_contains("extra notification enable help msg", "Usage: muslimtify notification enable");

  run(5, (char *[]){"m", "offset", "fajr", "4", "extra", NULL});
  check_ret("extra offset ret", 1);
  check_contains("extra offset msg", "unexpected argument 'extra' for 'offset'");
  {
    Config cfg;
    config_load(&cfg);
    check_bool("extra offset unchanged", cfg.fajr.offset == before.fajr.offset);
  }

  run(4, (char *[]){"m", "method", "mwl", "extra", NULL});
  check_ret("extra method ret", 1);
  check_contains("extra method msg", "unexpected argument 'extra' for 'method'");
  {
    Config cfg;
    config_load(&cfg);
    check_bool("extra method unchanged",
               strcmp(cfg.calculation_method, before.calculation_method) == 0);
  }
  run(4, (char *[]){"m", "method", "--list", "extra", NULL});
  check_ret("extra method list ret", 1);

  run(4, (char *[]){"m", "madzhab", "hanafi", "extra", NULL});
  check_ret("extra madzhab ret", 1);
  check_contains("extra madzhab msg", "unexpected argument 'extra' for 'madzhab'");
  {
    Config cfg;
    config_load(&cfg);
    check_bool("extra madzhab unchanged", strcmp(cfg.madhab, before.madhab) == 0);
  }
  run(4, (char *[]){"m", "madzhab", "--list", "extra", NULL});
  check_ret("extra madzhab list ret", 1);

  run(5, (char *[]){"m", "location", "gps", "off", "extra", NULL});
  check_ret("extra location gps ret", 1);
  check_contains("extra location gps msg", "unexpected argument 'extra' for 'location gps'");

  run(5, (char *[]){"m", "notification", "--urgency", "low", "extra", NULL});
  check_ret("extra urgency ret", 1);
  check_contains("extra urgency msg", "unexpected argument 'extra' for 'notification --urgency'");
  {
    Config cfg;
    config_load(&cfg);
    check_bool("extra urgency unchanged",
               strcmp(cfg.notification_urgency, before.notification_urgency) == 0);
  }

  run(5, (char *[]){"m", "notification", "--sound", "off", "extra", NULL});
  check_ret("extra sound ret", 1);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("extra sound unchanged",
               strcmp(cfg.notification_sound, before.notification_sound) == 0);
  }

  run(6, (char *[]){"m", "notification", "--adhan", "enable", "fajr", "x", NULL});
  check_ret("extra adhan enable ret", 1);
  check_contains("extra adhan enable msg", "unexpected argument 'x' for 'notification --adhan'");
  {
    Config cfg;
    config_load(&cfg);
    check_bool("extra adhan enable unchanged", cfg.fajr.adhan_enabled == before.fajr.adhan_enabled);
  }

  // The config file is a readable regular file, so only the extra argument is wrong.
  char adhan_path[512];
  snprintf(adhan_path, sizeof(adhan_path), "%s", config_get_path());
  run(6, (char *[]){"m", "notification", "--adhan", "set", adhan_path, "x", NULL});
  check_ret("extra adhan set ret", 1);

  run(5, (char *[]){"m", "notification", "--adhan", "stop", "x", NULL});
  check_ret("extra adhan stop ret", 1);
  check_contains("extra adhan stop msg", "unexpected argument 'x' for 'notification --adhan stop'");

  run(3, (char *[]){"m", "version", "extra", NULL});
  check_ret("extra version ret", 1);
  check_contains("extra version msg", "unexpected argument 'extra' for 'version'");

  run(3, (char *[]){"m", "help", "extra", NULL});
  check_ret("extra help ret", 1);
  check_contains("extra help msg", "unexpected argument 'extra' for 'help'");

  reset_config();
}

static void test_help_text(void) {
  printf("  help text...\n");
  reset_config();

  run(2, (char *[]){"m", "help", NULL});
  check_ret("help text top ret", 0);
  check_contains("help text top day offset", "--day-offset <days>");
  check_contains("help text top reminder", "--reminder <prayer|--all>");

  run(3, (char *[]){"m", "show", "--help", NULL});
  check_ret("help text show ret", 0);
  check_contains("help text show date", "--date <start> [end]");

  run(4, (char *[]){"m", "notification", "--reminder", "--help", NULL});
  check_ret("help text reminder ret", 0);
  check_contains("help text reminder usage", "--reminder <prayer|--all> <minutes...>");
}

// `location set --lat=nan` used to exit 0 and save "latitude": nan, because
// the guard was `lat < -90.0 || lat > 90.0` and both comparisons are false for
// NaN. The saved NaN then reached calculate_prayer_times, which returned a
// believable schedule: fajr and isha printed at the same time as dhuhr.
static void test_location_set_rejects_nan(void) {
  printf("test_location_set_rejects_nan\n");
  reset_config();

  run(4, (char *[]){"m", "location", "set", "--lat=nan", NULL});
  check_bool("nan latitude rejected", last_ret != 0);
  check_contains("nan latitude error", "Invalid latitude");

  run(4, (char *[]){"m", "location", "set", "--long=nan", NULL});
  check_bool("nan longitude rejected", last_ret != 0);
  check_contains("nan longitude error", "Invalid longitude");

  // Positive control: the rejection must not be rejecting everything.
  run(4, (char *[]){"m", "location", "set", "--lat=45", NULL});
  check_ret("valid latitude accepted", 0);

  // The saved value survived both rejections and took the valid update.
  Config cfg;
  check_bool("config loads", config_load(&cfg) == 0);
  check_bool("latitude is the valid one", cfg.latitude == 45.0);
}

// -- main ---------------------------------------------------------------------

int main(void) {
  setup();

  printf("Running CLI tests...\n");
  test_version_and_help();
  test_output_helpers();
  test_location();
  test_location_auto_args();
  test_removed_top_level();
  test_show();
  test_show_date_bounds();
  test_show_range();
  test_day_markers();
  test_next();
  test_day_offset();
  test_show_args();
  test_help_text();
  test_next_after_isha();
  test_method();
  test_madzhab();
  test_notification();
  test_reminder_args();
  test_daemon_errors();
  test_offset();
  test_extra_args();
  test_location_set_timezone_validation();
  test_location_set_rejects_nan();
  test_json_no_trailing_comma();

  printf("\nResults: %d passed, %d failed\n", passed, failed);
  teardown();
  return failed > 0 ? 1 : 0;
}
