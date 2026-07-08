#include "check_cycle.h"
#include "cli_internal.h"
#include "config.h"
#include "display.h"
#include "location.h"
#include "platform.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static void print_show_help(void) {
  printf("Usage: muslimtify show [--json | --headless] [--next]\n");
  printf("  (default)   Today's prayer times as a table\n");
  printf("  --json      Prayer times as JSON (prayers only)\n");
  printf("  --headless  Prayer times as key=value\n");
  printf("  --next      Next prayer (add --json or --headless to change format)\n");
  printf("  -h, --help  Show this help\n");
}

static void print_show_next_help(void) {
  printf("Usage: muslimtify show --next [--json | --headless]\n");
  printf("  (default)   Next prayer as a table\n");
  printf("  --json      Next prayer as JSON: {\"prayer\",\"time\",\"remaining\"}\n");
  printf("  --headless  Next prayer as key=value\n");
  printf("  -h, --help  Show this help\n");
}

int handle_show(int argc, char **argv) {
  bool want_next = false;
  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "--next") == 0)
      want_next = true;
  }

  if (cli_wants_help(argc, argv)) {
    if (want_next)
      print_show_next_help();
    else
      print_show_help();
    return 0;
  }

  // Reject removed / unknown flags (with a migration hint for the old spellings).
  for (int i = 0; i < argc; i++) {
    const char *a = argv[i];
    if (strcmp(a, "--next") == 0 || strcmp(a, "--json") == 0 || strcmp(a, "--headless") == 0)
      continue;
    if (strcmp(a, "--format") == 0) {
      fprintf(stderr, "Error: '--format json' was removed; use '--json'\n");
      return 1;
    }
    if (strcmp(a, "--no-header") == 0) {
      fprintf(stderr, "Error: '--no-header' was removed; use '--headless'\n");
      return 1;
    }
    fprintf(stderr, "Error: unknown option '%s'\n", a);
    print_show_help();
    return 1;
  }

  OutputMode mode = OUTPUT_TABLE;
  if (cli_parse_output_mode(argc, argv, &mode) != 0)
    return 1;

  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }
  if (ensure_location(&cfg) != 0)
    return 1;

  time_t now = time(NULL);
  struct tm tm_buf;
  platform_localtime(&now, &tm_buf);
  struct tm *tm_now = &tm_buf;

  struct PrayerTimes times =
      prayer_times_for_config(&cfg, tm_now->tm_year + 1900, tm_now->tm_mon + 1, tm_now->tm_mday);

  if (want_next) {
    switch (mode) {
    case OUTPUT_JSON:
      display_next_prayer_json(&times, &cfg, tm_now);
      break;
    case OUTPUT_HEADLESS:
      display_next_prayer_headless(&times, &cfg, tm_now);
      break;
    default:
      display_next_prayer(&times, &cfg, tm_now);
      break;
    }
  } else {
    switch (mode) {
    case OUTPUT_JSON:
      display_prayer_times_json(&times, &cfg, tm_now);
      break;
    case OUTPUT_HEADLESS:
      display_prayer_times_plain(&times, &cfg, tm_now);
      break;
    default:
      display_prayer_times_table(&times, &cfg, tm_now);
      break;
    }
  }

  return 0;
}

int handle_check(int argc, char **argv) {
  (void)argc;
  (void)argv;
  return run_check_cycle();
}
