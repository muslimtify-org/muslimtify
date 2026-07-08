#define _GNU_SOURCE

#include "cli.h"
#include "config.h"
#include "cli_internal.h"
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

static void test_config(void) {
  printf("  config...\n");
  reset_config();

  // config (no subcommand) → defaults to show
  run(2, (char *[]){"m", "config", NULL});
  check_ret("config bare ret", 0);
  check_contains("config bare out", "Configuration:");

  // config show
  run(3, (char *[]){"m", "config", "show", NULL});
  check_ret("config show ret", 0);
  check_contains("config show out", "Configuration:");

  // config validate
  run(3, (char *[]){"m", "config", "validate", NULL});
  check_ret("config validate ret", 0);
  check_contains("config validate out", "valid");

  // config unknown
  run(3, (char *[]){"m", "config", "bogus", NULL});
  check_ret("config unknown ret", 1);

  // config reset
  run(3, (char *[]){"m", "config", "reset", NULL});
  check_ret("config reset ret", 0);
  // Verify config is back to default (auto_detect=true, lat=0)
  Config cfg;
  config_load(&cfg);
  check_bool("config reset auto_detect", cfg.auto_detect == true);
  check_bool("config reset lat", cfg.latitude == 0.0);
}

static void test_location(void) {
  printf("  location...\n");
  reset_config();

  // location (default = show)
  run(2, (char *[]){"m", "location", NULL});
  check_ret("location bare ret", 0);

  // location show
  run(3, (char *[]){"m", "location", "show", NULL});
  check_ret("location show ret", 0);
  check_contains("location show out", "Jakarta");

  // location set --lat=<lat> --long=<lon> (equals form)
  run(5, (char *[]){"m", "location", "set", "--lat=-7.25", "--long=112.75", NULL});
  check_ret("location set ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("location set lat", cfg.latitude > -7.26 && cfg.latitude < -7.24);
    check_bool("location set lon", cfg.longitude > 112.74 && cfg.longitude < 112.76);
  }

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
  check_contains("location set equator lat shown", "Latitude: 0.0000");
  check_contains("location set equator lon shown", "Longitude: 0.0000");

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

  // location clear
  reset_config();
  run(3, (char *[]){"m", "location", "clear", NULL});
  check_ret("location clear ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("location clear lat", cfg.latitude == 0.0);
    check_bool("location clear lon", cfg.longitude == 0.0);
    check_bool("location clear auto", cfg.auto_detect == true);
  }

  // location unknown
  run(3, (char *[]){"m", "location", "bogus", NULL});
  check_ret("location unknown ret", 1);

  // location auto removed -> unknown subcommand
  run(3, (char *[]){"m", "location", "auto", NULL});
  check_ret("location auto removed ret", 1);
}

static void test_enable_disable(void) {
  printf("  enable/disable...\n");
  reset_config();

  // enable a prayer
  run(3, (char *[]){"m", "enable", "sunrise", NULL});
  check_ret("enable sunrise ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("enable sunrise cfg", cfg.sunrise.enabled == true);
  }

  // disable a prayer
  run(3, (char *[]){"m", "disable", "fajr", NULL});
  check_ret("disable fajr ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("disable fajr cfg", cfg.fajr.enabled == false);
  }

  // enable all
  run(3, (char *[]){"m", "enable", "all", NULL});
  check_ret("enable all ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("enable all fajr", cfg.fajr.enabled);
    check_bool("enable all sunrise", cfg.sunrise.enabled);
    check_bool("enable all dhuha", cfg.dhuha.enabled);
    check_bool("enable all dhuhr", cfg.dhuhr.enabled);
    check_bool("enable all asr", cfg.asr.enabled);
    check_bool("enable all maghrib", cfg.maghrib.enabled);
    check_bool("enable all isha", cfg.isha.enabled);
  }

  // disable all
  run(3, (char *[]){"m", "disable", "all", NULL});
  check_ret("disable all ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("disable all fajr", !cfg.fajr.enabled);
    check_bool("disable all sunrise", !cfg.sunrise.enabled);
    check_bool("disable all dhuha", !cfg.dhuha.enabled);
    check_bool("disable all dhuhr", !cfg.dhuhr.enabled);
    check_bool("disable all asr", !cfg.asr.enabled);
    check_bool("disable all maghrib", !cfg.maghrib.enabled);
    check_bool("disable all isha", !cfg.isha.enabled);
  }

  // enable (no arg)
  run(2, (char *[]){"m", "enable", NULL});
  check_ret("enable noarg ret", 1);

  // disable (no arg)
  run(2, (char *[]){"m", "disable", NULL});
  check_ret("disable noarg ret", 1);

  // enable bad prayer
  run(3, (char *[]){"m", "enable", "badprayer", NULL});
  check_ret("enable bad ret", 1);

  // disable bad prayer
  run(3, (char *[]){"m", "disable", "badprayer", NULL});
  check_ret("disable bad ret", 1);
}

static void test_list(void) {
  printf("  list...\n");
  reset_config();

  run(2, (char *[]){"m", "list", NULL});
  check_ret("list ret", 0);
  check_contains("list out", "Prayer Notifications:");
}

static void test_reminder(void) {
  printf("  reminder...\n");
  reset_config();

  // reminder (default = show)
  run(2, (char *[]){"m", "reminder", NULL});
  check_ret("reminder bare ret", 0);
  check_contains("reminder bare out", "Prayer Reminders:");

  // reminder show
  run(3, (char *[]){"m", "reminder", "show", NULL});
  check_ret("reminder show ret", 0);

  // reminder fajr 30,15,5
  run(4, (char *[]){"m", "reminder", "fajr", "30,15,5", NULL});
  check_ret("reminder fajr set ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("reminder fajr count", cfg.fajr.reminder_count == 3);
    check_bool("reminder fajr[0]", cfg.fajr.reminders[0] == 30);
    check_bool("reminder fajr[1]", cfg.fajr.reminders[1] == 15);
    check_bool("reminder fajr[2]", cfg.fajr.reminders[2] == 5);
  }

  // reminder fajr clear
  run(4, (char *[]){"m", "reminder", "fajr", "clear", NULL});
  check_ret("reminder fajr clear ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("reminder fajr cleared", cfg.fajr.reminder_count == 0);
  }

  // reminder all 10,5
  reset_config();
  run(4, (char *[]){"m", "reminder", "all", "10,5", NULL});
  check_ret("reminder all ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    // fajr is enabled by default, should have reminders
    check_bool("reminder all fajr count", cfg.fajr.reminder_count == 2);
    check_bool("reminder all fajr[0]", cfg.fajr.reminders[0] == 10);
    check_bool("reminder all fajr[1]", cfg.fajr.reminders[1] == 5);
    // sunrise is disabled by default, should NOT get reminders
    check_bool("reminder all sunrise skip", cfg.sunrise.reminder_count == 0);
  }

  // reminder badprayer 30
  run(4, (char *[]){"m", "reminder", "badprayer", "30", NULL});
  check_ret("reminder bad prayer ret", 1);

  // reminder fajr (missing times)
  run(3, (char *[]){"m", "reminder", "fajr", NULL});
  check_ret("reminder missing times ret", 1);
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
  run(3, (char *[]){"m", "enable", "all", NULL});
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

static void test_check(void) {
  printf("  check...\n");
  reset_config();

  // Disable all prayers so check won't try to send a notification
  run(3, (char *[]){"m", "disable", "all", NULL});

  run(2, (char *[]){"m", "check", NULL});
  check_ret("check disabled ret", 0);
}

static void test_method(void) {
  printf("  method...\n");
  reset_config();

  // method (default = show)
  run(2, (char *[]){"m", "method", NULL});
  check_ret("method bare ret", 0);
  check_contains("method bare out", "Calculation Method:");

  // method show
  run(3, (char *[]){"m", "method", "show", NULL});
  check_ret("method show ret", 0);
  check_contains("method show method", "kemenag");
  check_contains("method show madhab", "Madhab:");

  // method list
  run(3, (char *[]){"m", "method", "list", NULL});
  check_ret("method list ret", 0);
  check_contains("method list header", "Available calculation methods:");
  check_contains("method list kemenag", "kemenag");
  check_contains("method list mwl", "mwl");
  check_contains("method list current", "*");

  // method set isna
  run(3, (char *[]){"m", "method", "isna", NULL});
  check_ret("method set isna ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("method set isna cfg", strcmp(cfg.calculation_method, "isna") == 0);
  }

  // method set (explicit)
  run(4, (char *[]){"m", "method", "set", "egypt", NULL});
  check_ret("method set egypt ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("method set egypt cfg", strcmp(cfg.calculation_method, "egypt") == 0);
  }

  // method set invalid
  run(4, (char *[]){"m", "method", "set", "invalid_method", NULL});
  check_ret("method set invalid ret", 1);

  // method madhab hanafi
  run(4, (char *[]){"m", "method", "madhab", "hanafi", NULL});
  check_ret("method madhab hanafi ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("method madhab hanafi cfg", strcmp(cfg.madhab, "hanafi") == 0);
  }

  // method madhab shafi
  run(4, (char *[]){"m", "method", "madhab", "shafi", NULL});
  check_ret("method madhab shafi ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("method madhab shafi cfg", strcmp(cfg.madhab, "shafi") == 0);
  }

  // method madhab invalid
  run(4, (char *[]){"m", "method", "madhab", "maliki", NULL});
  check_ret("method madhab invalid ret", 1);

  // method madhab (no arg)
  run(3, (char *[]){"m", "method", "madhab", NULL});
  check_ret("method madhab noarg ret", 1);

  // method auto removed -> falls through to "method set auto" which is invalid
  run(3, (char *[]){"m", "method", "auto", NULL});
  check_ret("method auto removed ret", 1);
}

static void test_sound(void) {
  printf("  sound...\n");
  reset_config();

  // sound (bare) → status output, ret 0
  run(2, (char *[]){"m", "sound", NULL});
  check_ret("sound bare ret", 0);
  check_contains("sound bare out", "Sound:");
  check_contains("sound bare alarm line", "Alarm preset:");
  check_contains("sound bare reminder line", "Reminder preset:");

  // sound status (explicit)
  run(3, (char *[]){"m", "sound", "status", NULL});
  check_ret("sound status ret", 0);
  check_contains("sound status out", "Sound:");

  // sound show (alias)
  run(3, (char *[]){"m", "sound", "show", NULL});
  check_ret("sound show ret", 0);
  check_contains("sound show out", "Sound:");

  // sound off
  run(3, (char *[]){"m", "sound", "off", NULL});
  check_ret("sound off ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("sound off cfg", cfg.notification_sound == false);
  }

  // sound on
  run(3, (char *[]){"m", "sound", "on", NULL});
  check_ret("sound on ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("sound on cfg", cfg.notification_sound == true);
  }

  // sound set alarm
  run(4, (char *[]){"m", "sound", "set", "alarm", NULL});
  check_ret("sound set alarm ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("sound set alarm cfg", strcmp(cfg.notification_sound_alarm, "alarm") == 0);
  }

  // sound set default
  run(4, (char *[]){"m", "sound", "set", "default", NULL});
  check_ret("sound set default ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("sound set default cfg", strcmp(cfg.notification_sound_alarm, "default") == 0);
  }

  // sound set reminder
  run(4, (char *[]){"m", "sound", "set", "reminder", NULL});
  check_ret("sound set reminder ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("sound set reminder cfg", strcmp(cfg.notification_sound_alarm, "reminder") == 0);
  }

  // sound reminder-set alarm
  run(4, (char *[]){"m", "sound", "reminder-set", "alarm", NULL});
  check_ret("sound reminder-set alarm ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("sound reminder-set alarm cfg",
               strcmp(cfg.notification_sound_reminder, "alarm") == 0);
  }

  // sound reminder-set default
  run(4, (char *[]){"m", "sound", "reminder-set", "default", NULL});
  check_ret("sound reminder-set default ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("sound reminder-set default cfg",
               strcmp(cfg.notification_sound_reminder, "default") == 0);
  }

  // sound reminder-set reminder (restore default)
  run(4, (char *[]){"m", "sound", "reminder-set", "reminder", NULL});
  check_ret("sound reminder-set reminder ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("sound reminder-set reminder cfg",
               strcmp(cfg.notification_sound_reminder, "reminder") == 0);
  }

  // sound set (no arg) → error
  run(3, (char *[]){"m", "sound", "set", NULL});
  check_ret("sound set noarg ret", 1);

  // sound reminder-set (no arg) → error
  run(3, (char *[]){"m", "sound", "reminder-set", NULL});
  check_ret("sound reminder-set noarg ret", 1);

  // sound set bogus → error, config unchanged
  reset_config();
  run(4, (char *[]){"m", "sound", "set", "bogus", NULL});
  check_ret("sound set bogus ret", 1);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("sound set bogus unchanged", strcmp(cfg.notification_sound_alarm, "alarm") == 0);
  }

  // sound reminder-set bogus → error
  run(4, (char *[]){"m", "sound", "reminder-set", "bogus", NULL});
  check_ret("sound reminder-set bogus ret", 1);

  // sound unknown subcommand → error
  run(3, (char *[]){"m", "sound", "bogus", NULL});
  check_ret("sound unknown ret", 1);
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

  // offset annotation appears in `reminder show`
  reset_config();
  run(4, (char *[]){"m", "offset", "maghrib", "+7", NULL});
  check_ret("offset maghrib for display ret", 0);
  run(3, (char *[]){"m", "reminder", "show", NULL});
  check_ret("reminder show ret", 0);
  check_contains("offset shown in reminders", "[+7 min]");
}

static void test_output_helpers(void) {
  printf("  output helpers...\n");
  OutputMode m = OUTPUT_TABLE;

  check_bool("mode default table", cli_parse_output_mode(0, (char *[]){NULL}, &m) == 0 && m == OUTPUT_TABLE);
  check_bool("mode json", cli_parse_output_mode(1, (char *[]){"--json"}, &m) == 0 && m == OUTPUT_JSON);
  check_bool("mode headless", cli_parse_output_mode(1, (char *[]){"--headless"}, &m) == 0 && m == OUTPUT_HEADLESS);
  check_bool("mode conflict", cli_parse_output_mode(2, (char *[]){"--json", "--headless"}, &m) != 0);

  check_bool("wants help -h", cli_wants_help(1, (char *[]){"-h"}));
  check_bool("wants help --help", cli_wants_help(1, (char *[]){"--help"}));
  check_bool("no help on --json", !cli_wants_help(1, (char *[]){"--json"}));
}

// -- main ---------------------------------------------------------------------

int main(void) {
  setup();

  printf("Running CLI tests...\n");
  test_version_and_help();
  test_output_helpers();
  test_config();
  test_location();
  test_enable_disable();
  test_list();
  test_reminder();
  test_show();
  test_next();
  test_check();
  test_method();
  test_sound();
  test_daemon_errors();
  test_offset();

  printf("\nResults: %d passed, %d failed\n", passed, failed);
  teardown();
  return failed > 0 ? 1 : 0;
}
