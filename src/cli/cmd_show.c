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
  printf("\n");
  printf("Show today's prayer times as a table\n");
  printf("\n");
  printf("Usage: muslimtify show <Options> [Commands]\n");
  printf("\n");
  printf("Commands:\n");
  printf("  %-25s %s\n", "--next <Option>", "Show next prayer time");
  printf("  %-25s %s\n", "--date [date] <date> <options>",
         "Show prayer time at or until desire date (yyyy-mm-dd)");
  printf("  %-25s %s\n", "-h, --help", "Show this help");
  printf("\n");
  printf("Options:\n");
  printf("  %-25s %s\n", "--json", "Show as JSON");
  printf("  %-25s %s\n", "--headless", "Show as key=value");
  printf("\n");
  printf("Examples:\n");
  printf("  %-25s %s\n", "muslimtify show --headless", "# Show today's prayer as key=value");
  printf("  %-25s %s\n", "muslimtify show --next", "# Show next prayer");
  printf("  %-25s %s\n", "muslimtify show --next --json", "# Show next prayer as JSON");
  printf("  %-25s %s\n", "muslimtify show --date 2022-01-01", "# Show prayer times for 2022-01-01");
  printf("  %-25s %s\n", "muslimtify show --date 2022-01-01 --json",
         "# Show prayer times for 2022-01-01 as JSON");
  printf("  %-25s %s\n", "muslimtify show --date 2022-01-01 2023-01-01",
         "# Show prayer times from 2022-01-01 to 2023-01-01");
}

static void print_show_next_help(void) {
  printf("\n");
  printf("Show next prayer as a table\n");
  printf("\n");
  printf("Usage: muslimtify show --next [options]\n");
  printf("\n");
  printf("Commands:\n");
  printf("  %-25s %s\n", "-h, --help", "Show this help");
  printf("\n");
  printf("Options:\n");
  printf("  %-25s %s\n", "--json", "Next prayer as JSON: {\"prayer\",\"time\",\"remaining\"}");
  printf("  %-25s %s\n", "--headless", "Next prayer as key=value");
  printf("\n");
  printf("Examples:\n");
  printf("  %-25s %s\n", "muslimtify show --next --json", "# Show next prayer as JSON");
  printf("  %-25s %s\n", "muslimtify show --next --headless", "# Show next prayer as key=value");
}

static void print_show_date_help(void) {
  printf("\n");
  printf("Show prayer times at or until desire date as a table\n");
  printf("\n");
  printf("Usage: muslimtify show --date [date] <date> <options>\n");
  printf("\n");
  printf("Commands:\n");
  printf("  %-25s %s\n", "-h, --help", "Show this help");
  printf("\n");
  printf("Options:\n");
  printf("  %-25s %s\n", "--json", "Today's prayer times as JSON");
  printf("  %-25s %s\n", "--headless", "Today's prayer times as key=value");
  printf("\n");
  printf("Examples:\n");
  printf("  %-25s %s\n", "muslimtify show --date 2022-01-01", "# Show prayer times for 2022-01-01");
  printf("  %-25s %s\n", "muslimtify show --date 2022-01-01 --json",
         "# Show prayer times for 2022-01-01 as JSON");
  printf("  %-25s %s\n", "muslimtify show --date 2022-01-01 2023-01-01",
         "# Show prayer times from 2022-01-01 to 2023-01-01");
}

int handle_show(int argc, char **argv) {
  bool want_next = false;
  bool want_date = false;
  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "--next") == 0)
      want_next = true;

    if (strcmp(argv[i], "--date") == 0)
      want_date = true;
  }

  if (cli_wants_help(argc, argv)) {
    if (want_next)
      print_show_next_help();
    else if (want_date)
      print_show_date_help();
    else
      print_show_help();
    return 0;
  }

  // Reject removed / unknown flags (with a migration hint for the old spellings).
  for (int i = 0; i < argc; i++) {
    const char *a = argv[i];
    if (strcmp(a, "--date") == 0)
      break;
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
  } else if (want_date) {
    if (argc > 4) {
      fprintf(stderr, "Error: Too many arguments\n");
      print_show_date_help();
      return 1;
    }
    // TODO: switch(mode)
    // TODO: if (argv[1] != NULL && argv[2] != NULL) show_prayer_time_at_range(argv[1], argv[2]);
    // TODO: if (argv[1] != NULL && argv[2] != NULL) show_prayer_time_at_range(argv[1], argv[2]);
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
