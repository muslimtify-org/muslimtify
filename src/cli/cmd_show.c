#include "cli_internal.h"
#include "config.h"
#include "display.h"
#include "location.h"
#include "platform.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
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
  printf("Show prayer times for a single date or an inclusive date range\n");
  printf("\n");
  printf("Usage: muslimtify show --date <start> [end] [options]\n");
  printf("\n");
  printf("Commands:\n");
  printf("  %-25s %s\n", "-h, --help", "Show this help");
  printf("\n");
  printf("Options:\n");
  printf("  %-25s %s\n", "--json", "Prayer times as JSON (range: array of days)");
  printf("  %-25s %s\n", "--headless", "Prayer times as key=value (range: date= blocks)");
  printf("\n");
  printf("Notes:\n");
  printf("  %s\n", "Dates are yyyy-mm-dd. --json/--headless may appear before or after the dates.");
  printf("\n");
  printf("Examples:\n");
  printf("  %-40s %s\n", "muslimtify show --date 2022-01-01", "# One day");
  printf("  %-40s %s\n", "muslimtify show --date 2022-01-01 --json", "# One day as JSON");
  printf("  %-40s %s\n", "muslimtify show --date 2022-01-01 2022-01-07", "# Inclusive range");
  printf("  %-40s %s\n", "muslimtify show --date 2022-01-01 2022-01-07 --headless",
         "# Range as key=value");
}

static int mt_is_leap(int y) {
  return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

static int mt_days_in_month(int y, int m) {
  static const int dm[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (m < 1 || m > 12)
    return 0;
  if (m == 2 && mt_is_leap(y))
    return 29;
  return dm[m - 1];
}

// Parse one unsigned decimal field in [min,max], advancing *sp past its digits.
// Rejects an empty field, a leading sign or whitespace (strtol would accept all
// three and yield a number the user never typed), ERANGE overflow, and any
// value outside the bounds. Returns 0 on success, -1 otherwise.
static int parse_field(const char **sp, long min, long max, int *out) {
  const char *s = *sp;
  if (*s < '0' || *s > '9')
    return -1;
  errno = 0;
  char *end;
  long v = strtol(s, &end, 10);
  if (end == s || errno == ERANGE || v < min || v > max)
    return -1;
  *sp = end;
  *out = (int)v;
  return 0;
}

// Parse an ISO "YYYY-MM-DD" date into y/m/d with full range validation.
// Returns 0 on success, -1 on malformed input, trailing junk, a year outside
// 1..9999, or an out-of-range month/day (leap-aware). Components need not be
// zero-padded.
static int parse_date(const char *s, int *y, int *m, int *d) {
  if (s == NULL)
    return -1;
  int yy, mm, dd;
  if (parse_field(&s, 1, 9999, &yy) != 0 || *s != '-')
    return -1;
  s++;
  if (parse_field(&s, 1, 12, &mm) != 0 || *s != '-')
    return -1;
  s++;
  if (parse_field(&s, 1, mt_days_in_month(yy, mm), &dd) != 0)
    return -1;
  if (*s != '\0')
    return -1;
  *y = yy;
  *m = mm;
  *d = dd;
  return 0;
}

int handle_show(int argc, char **argv) {
  bool want_next = false;
  bool want_date = false;
  int date_idx = -1;
  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "--next") == 0)
      want_next = true;

    if (strcmp(argv[i], "--date") == 0) {
      want_date = true;
      date_idx = i;
    }
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
    char *start_arg = (date_idx + 1 < argc) ? argv[date_idx + 1] : NULL;
    char *end_arg = NULL;
    if (date_idx + 2 < argc && strncmp(argv[date_idx + 2], "--", 2) != 0)
      end_arg = argv[date_idx + 2];

    int sy, sm, sd;
    if (parse_date(start_arg, &sy, &sm, &sd) != 0) {
      fprintf(stderr, "Error: Invalid date %s\n", start_arg ? start_arg : "(missing)");
      print_show_date_help();
      return 1;
    }

    if (end_arg == NULL) {
      // Single day
      struct tm date_start = {0};
      date_start.tm_year = sy - 1900;
      date_start.tm_mon = sm - 1;
      date_start.tm_mday = sd;
      struct PrayerTimes start = prayer_times_for_config(&cfg, sy, sm, sd);
      switch (mode) {
      case OUTPUT_JSON:
        display_prayer_times_json(&start, &cfg, &date_start);
        break;
      case OUTPUT_HEADLESS:
        display_prayer_times_plain(&start, &cfg, &date_start);
        break;
      default:
        display_prayer_times_table(&start, &cfg, &date_start);
        break;
      }
    } else {
      // Date range
      int ey, em, ed;
      if (parse_date(end_arg, &ey, &em, &ed) != 0) {
        fprintf(stderr, "Error: Invalid date %s\n", end_arg);
        print_show_date_help();
        return 1;
      }
      if (sy > ey || (sy == ey && (sm > em || (sm == em && sd > ed)))) {
        fprintf(stderr, "Error: end date is before start date\n");
        return 1;
      }
      switch (mode) {
      case OUTPUT_JSON:
        display_prayer_times_range_json(&cfg, sy, sm, sd, ey, em, ed);
        break;
      case OUTPUT_HEADLESS:
        display_prayer_times_range_plain(&cfg, sy, sm, sd, ey, em, ed);
        break;
      default:
        display_prayer_times_range_table(&cfg, sy, sm, sd, ey, em, ed);
        break;
      }
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
