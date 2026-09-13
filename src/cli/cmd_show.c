#include "cli_internal.h"
#include "config.h"
#include "display.h"
#include "location.h"
#include "platform.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Longest span accepted by `show --date <start> <end>`, inclusive. 366 covers a
// full leap year, the longest range with a real use case. Without this cap the
// range loop is bounded only by the year range and can run for weeks.
#define MAX_RANGE_DAYS 366

// Largest magnitude accepted by `show --day-offset`: more days than years 1-9999 span.
#define MAX_DAY_OFFSET (9999L * 366L)

static void print_show_help(void) {
  printf("\n");
  printf("Show today's prayer times as a table\n");
  printf("\n");
  printf("Usage: muslimtify show <Options> [Commands]\n");
  printf("\n");
  printf("Commands:\n");
  printf("  %-25s %s\n", "--next <Option>", "Show next prayer time");
  printf("  %-25s %s\n", "--day-offset <offset>", "Show prayer time (+/-)<offset> days from now");
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

static void print_show_day_offset_help(void) {
  printf("\n");
  printf("Show prayer times for a day offset from now\n");
  printf("\n");
  printf("Usage: muslimtify show --day-offset <offset> [options]\n");
  printf("\n");
  printf("Commands:\n");
  printf("  %-25s %s\n", "-h, --help", "Show this help");
  printf("\n");
  printf("Options:\n");
  printf("  %-25s %s\n", "--json", "Prayer times as JSON");
  printf("  %-25s %s\n", "--headless", "Prayer times as key=value");
  printf("\n");
  printf("Notes:\n");
  printf("  %s\n", "The offset is a whole number of days and may be negative.");
  printf("  %s\n", "The shifted date must fall in years 1-9999.");
  printf("\n");
  printf("Examples:\n");
  printf("  %-40s %s\n", "muslimtify show --day-offset 1", "# One day");
  printf("  %-40s %s\n", "muslimtify show --day-offset 1 --json", "# One day as JSON");
  printf("  %-40s %s\n", "muslimtify show --day-offset -1", "# One day ago");
  printf("  %-40s %s\n", "muslimtify show --day-offset -1 --json", "# One day ago as JSON");
  printf("  %-40s %s\n", "muslimtify show --day-offset -365", "# One year ago");
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
  printf("  %-25s %s\n", "--json",
         "Next prayer as JSON: {\"date\",\"prayer\",\"time\",\"remaining\"}");
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
  printf("  %s\n", "Years are 1-9999; a range may span at most 366 days.");
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

// Parse one unsigned decimal field of at most `maxdigits` digits into [min,max],
// advancing *sp past those digits. Rejects a leading sign or whitespace (strtol
// would accept both and yield a number the user never typed), an over-wide field
// such as "00002024", and any value outside the bounds. The width bound also caps
// the value, so strtol cannot overflow here and errno needs no inspection.
// Fewer digits than maxdigits are fine, so "2024-1-1" still parses.
// Returns 0 on success, -1 otherwise.
static int parse_field(const char **sp, int maxdigits, long min, long max, int *out) {
  const char *s = *sp;
  if (*s < '0' || *s > '9')
    return -1;
  char *end;
  long v = strtol(s, &end, 10);
  if (end - s > maxdigits || v < min || v > max)
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
  if (parse_field(&s, 4, 1, 9999, &yy) != 0 || *s != '-')
    return -1;
  s++;
  if (parse_field(&s, 2, 1, 12, &mm) != 0 || *s != '-')
    return -1;
  s++;
  if (parse_field(&s, 2, 1, mt_days_in_month(yy, mm), &dd) != 0)
    return -1;
  if (*s != '\0')
    return -1;
  *y = yy;
  *m = mm;
  *d = dd;
  return 0;
}

// Parse a whole-day offset such as "1", "+7" or "-365". Unlike atoi, a missing
// value, junk such as "abc" or "1x", and leading whitespace are errors instead of
// a silent 0. The bound is wider than the 1-9999 year span, so any value it cuts
// off would be out of range anyway, and it keeps the day serial far from
// overflowing the int year. Returns 0 on success, -1 otherwise.
static int parse_day_offset(const char *s, long *out) {
  if (s == NULL || (*s != '-' && *s != '+' && (*s < '0' || *s > '9')))
    return -1;
  char *end;
  long v = strtol(s, &end, 10);
  if (end == s || *end != '\0' || v < -MAX_DAY_OFFSET || v > MAX_DAY_OFFSET)
    return -1;
  *out = v;
  return 0;
}

int handle_show(int argc, char **argv) {
  bool want_next = false;
  bool want_date = false;
  bool want_day_offset = false;
  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "--day-offset") == 0)
      want_day_offset = true;
    else if (strcmp(argv[i], "--next") == 0)
      want_next = true;
    else if (strcmp(argv[i], "--date") == 0)
      want_date = true;
  }

  if (cli_wants_help(argc, argv)) {
    if (want_next)
      print_show_next_help();
    else if (want_date)
      print_show_date_help();
    else if (want_day_offset)
      print_show_day_offset_help();
    else
      print_show_help();
    return 0;
  }

  // Check every argument before the config is loaded. --date takes a start date
  // and an optional end date, --day-offset takes one value, and everything else
  // must be a known flag (with a migration hint for the removed spellings).
  const char *date_start = NULL;
  const char *date_end = NULL;
  const char *offset_arg = NULL;
  bool seen_date = false;
  bool seen_day_offset = false;
  for (int i = 0; i < argc; i++) {
    const char *a = argv[i];
    if (strcmp(a, "--date") == 0) {
      if (seen_date) {
        fprintf(stderr, "Error: --date given more than once\n");
        return 1;
      }
      seen_date = true;
      if (i + 1 < argc && strncmp(argv[i + 1], "--", 2) != 0)
        date_start = argv[++i];
      if (date_start && i + 1 < argc && strncmp(argv[i + 1], "--", 2) != 0)
        date_end = argv[++i];
      continue;
    }
    if (strcmp(a, "--day-offset") == 0) {
      if (seen_day_offset) {
        fprintf(stderr, "Error: --day-offset given more than once\n");
        return 1;
      }
      seen_day_offset = true;
      if (i + 1 < argc && strncmp(argv[i + 1], "--", 2) != 0)
        offset_arg = argv[++i];
      continue;
    }
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

  if (want_day_offset && (want_next || want_date)) {
    fprintf(stderr, "Error: --day-offset cannot be combined with --next or --date\n");
    return 1;
  }
  if (want_next && want_date) {
    fprintf(stderr, "Error: --next cannot be combined with --date\n");
    return 1;
  }

  OutputMode mode = OUTPUT_TABLE;
  if (cli_parse_output_mode(argc, argv, &mode) != 0)
    return 1;

  int sy = 0, sm = 0, sd = 0, ey = 0, em = 0, ed = 0;
  if (want_date) {
    if (parse_date(date_start, &sy, &sm, &sd) != 0) {
      fprintf(stderr, "Error: Invalid date %s\n", date_start ? date_start : "(missing)");
      print_show_date_help();
      return 1;
    }
    if (date_end) {
      if (parse_date(date_end, &ey, &em, &ed) != 0) {
        fprintf(stderr, "Error: Invalid date %s\n", date_end);
        print_show_date_help();
        return 1;
      }
      if (sy > ey || (sy == ey && (sm > em || (sm == em && sd > ed)))) {
        fprintf(stderr, "Error: end date is before start date\n");
        return 1;
      }
      long span = mt_days_from_civil(ey, em, ed) - mt_days_from_civil(sy, sm, sd) + 1;
      if (span > MAX_RANGE_DAYS) {
        fprintf(stderr, "Error: date range too long (%ld days, maximum %d)\n", span,
                MAX_RANGE_DAYS);
        return 1;
      }
    }
  }

  time_t now = time(NULL);
  struct tm tm_buf;
  platform_localtime(&now, &tm_buf);
  struct tm *tm_now = &tm_buf;

  struct tm date = *tm_now;
  if (want_day_offset) {
    long offset = 0;
    if (parse_day_offset(offset_arg, &offset) != 0) {
      fprintf(stderr, "Error: Invalid day offset %s\n", offset_arg ? offset_arg : "(missing)");
      print_show_day_offset_help();
      return 1;
    }
    int y, m, d;
    mt_civil_from_days(mt_days_from_civil(date.tm_year + 1900, date.tm_mon + 1, date.tm_mday) +
                           offset,
                       &y, &m, &d);
    if (y < 1 || y > 9999) {
      fprintf(stderr, "Error: day offset %ld falls outside years 1-9999\n", offset);
      return 1;
    }
    date.tm_year = y - 1900;
    date.tm_mon = m - 1;
    date.tm_mday = d;
  }

  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }
  if (ensure_location(&cfg) != 0)
    return 1;

  if (want_next) {
    struct PrayerTimes times =
        prayer_times_for_config(&cfg, tm_now->tm_year + 1900, tm_now->tm_mon + 1, tm_now->tm_mday);
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
  } else if (want_date && date_end == NULL) {
    struct tm start_tm = {0};
    start_tm.tm_year = sy - 1900;
    start_tm.tm_mon = sm - 1;
    start_tm.tm_mday = sd;
    struct PrayerTimes start = prayer_times_for_config(&cfg, sy, sm, sd);
    switch (mode) {
    case OUTPUT_JSON:
      display_prayer_times_json(&start, &cfg, &start_tm);
      break;
    case OUTPUT_HEADLESS:
      display_prayer_times_plain(&start, &cfg, &start_tm);
      break;
    default:
      display_prayer_times_table(&start, &cfg, &start_tm);
      break;
    }
  } else if (want_date) {
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
  } else {
    // Today, or today shifted by --day-offset.
    struct PrayerTimes times =
        prayer_times_for_config(&cfg, date.tm_year + 1900, date.tm_mon + 1, date.tm_mday);
    switch (mode) {
    case OUTPUT_JSON:
      display_prayer_times_json(&times, &cfg, &date);
      break;
    case OUTPUT_HEADLESS:
      display_prayer_times_plain(&times, &cfg, &date);
      break;
    default:
      display_prayer_times_table(&times, &cfg, &date);
      break;
    }
  }

  return 0;
}
