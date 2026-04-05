#define _GNU_SOURCE

#include "cli.h"
#include "config.h"
#include "platform.h"
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

// ── test infrastructure ─────────────────────────────────────────────────────

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

static int run_external_process(const char *path, char *const argv[]) {
  pid_t pid = fork();
  if (pid < 0) {
    captured[0] = '\0';
    last_ret = 1;
    return last_ret;
  }

  if (pid == 0) {
    FILE *f = fopen(output_file, "w");
    if (!f) {
      _exit(127);
    }

    int fd = fileno(f);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    fclose(f);

    execv(path, argv);
    perror(path);
    _exit(127);
  }

  int status = 0;
  if (waitpid(pid, &status, 0) < 0) {
    captured[0] = '\0';
    last_ret = 1;
    return last_ret;
  }

  FILE *r = fopen(output_file, "r");
  if (r) {
    size_t n = fread(captured, 1, sizeof(captured) - 1, r);
    captured[n] = '\0';
    fclose(r);
  } else {
    captured[0] = '\0';
  }

  if (WIFEXITED(status)) {
    last_ret = WEXITSTATUS(status);
  } else {
    last_ret = 1;
  }

  return last_ret;
}

// ── assertion helpers ────────────────────────────────────────────────────────

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

static void check_not_empty(const char *test) {
  if (captured[0] != '\0') {
    passed++;
  } else {
    failed++;
    fprintf(stderr, "FAIL [%s]: output is empty\n", test);
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

// ── test groups ──────────────────────────────────────────────────────────────

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

static void test_run_help_line(void) {
  printf("  run help line...\n");
  reset_config();

  run(2, (char *[]){"m", "help", NULL});
  check_ret("run help ret", 0);
  check_contains("run help out", "run               Launch the GUI");
}

static void test_run_command(void) {
  printf("  run command...\n");
  reset_config();

  char binary_path[PLATFORM_PATH_MAX];
  snprintf(binary_path, sizeof(binary_path), "%s/muslimtify", platform_exe_dir());
  char *argv[] = {binary_path, "run", NULL};
  run_external_process(binary_path, argv);
  check_ret("run command ret", 1);
  check_contains("run command out", "Failed to open display");
}

static void test_config(void) {
  printf("  config...\n");
  reset_config();

  // config (no subcommand) → error
  run(2, (char *[]){"m", "config", NULL});
  check_ret("config bare ret", 1);

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

  // location set <lat> <lon>
  run(5, (char *[]){"m", "location", "set", "-7.25", "112.75", NULL});
  check_ret("location set ret", 0);
  {
    Config cfg;
    config_load(&cfg);
    check_bool("location set lat", cfg.latitude > -7.26 && cfg.latitude < -7.24);
    check_bool("location set lon", cfg.longitude > 112.74 && cfg.longitude < 112.76);
  }

  // location set (no args)
  run(3, (char *[]){"m", "location", "set", NULL});
  check_ret("location set noargs ret", 1);

  // location set (invalid lat)
  run(5, (char *[]){"m", "location", "set", "abc", "112.75", NULL});
  check_ret("location set bad lat ret", 1);

  // location set (invalid lon)
  run(5, (char *[]){"m", "location", "set", "-7.25", "xyz", NULL});
  check_ret("location set bad lon ret", 1);

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

  // default (no args → version + help)
  run(1, (char *[]){"m", NULL});
  check_ret("default ret", 0);
  check_contains("default version", "Muslimtify v");
  check_contains("default help", "Usage:");

  // show (explicit)
  run(2, (char *[]){"m", "show", NULL});
  check_ret("show explicit ret", 0);

  // show --format json
  run(4, (char *[]){"m", "show", "--format", "json", NULL});
  check_ret("show json ret", 0);
  check_contains("show json prayers", "\"prayers\"");
  check_contains("show json fajr", "\"fajr\"");

  // show --no-header (basic output)
  run(3, (char *[]){"m", "show", "--no-header", NULL});
  check_ret("show no-header ret", 0);
  check_contains("show no-header fajr", "Fajr=");
  check_contains("show no-header dhuhr", "Dhuhr=");
  check_contains("show no-header asr", "Asr=");
  check_contains("show no-header maghrib", "Maghrib=");
  check_contains("show no-header isha", "Isha=");

  // show --no-header (disabled prayers omitted)
  check_bool("show no-header no sunrise", strstr(captured, "Sunrise=") == NULL);
  check_bool("show no-header no dhuha", strstr(captured, "Dhuha=") == NULL);

  // show --no-header (with all prayers enabled)
  run(3, (char *[]){"m", "enable", "all", NULL});
  run(3, (char *[]){"m", "show", "--no-header", NULL});
  check_ret("show no-header all ret", 0);
  check_contains("show no-header sunrise", "Sunrise=");
  check_contains("show no-header dhuha", "Dhuha=");
}

static void test_next(void) {
  printf("  next...\n");
  reset_config();

  // next (default display)
  run(2, (char *[]){"m", "next", NULL});
  check_ret("next ret", 0);
  check_contains("next out", "Next Prayer:");

  // next name
  run(3, (char *[]){"m", "next", "name", NULL});
  check_ret("next name ret", 0);
  check_not_empty("next name out");

  // next time
  run(3, (char *[]){"m", "next", "time", NULL});
  check_ret("next time ret", 0);
  check_not_empty("next time out");

  // next remaining
  run(3, (char *[]){"m", "next", "remaining", NULL});
  check_ret("next remaining ret", 0);
  check_not_empty("next remaining out");
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
}

static void test_daemon_errors(void) {
  printf("  daemon errors...\n");
  reset_config();

  // daemon unknown subcommand
  run(3, (char *[]){"m", "daemon", "blah", NULL});
  check_ret("daemon unknown ret", 1);
}

// ── main ─────────────────────────────────────────────────────────────────────

int main(void) {
  setup();

  printf("Running CLI tests...\n");
  test_version_and_help();
  test_run_help_line();
  test_run_command();
  test_config();
  test_location();
  test_enable_disable();
  test_list();
  test_reminder();
  test_show();
  test_next();
  test_check();
  test_method();
  test_daemon_errors();

  printf("\nResults: %d passed, %d failed\n", passed, failed);
  teardown();
  return failed > 0 ? 1 : 0;
}
