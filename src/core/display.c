#define PRAYERTIMES_IMPLEMENTATION
#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "display.h"
#include "platform.h"
#include "prayer_checker.h"
#include "util.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ANSI color codes
#define COL_RESET "\033[0m"
#define COL_BOLD "\033[1m"
#define COL_DIM "\033[2m"
#define COL_GREEN "\033[32m"
#define COL_YELLOW "\033[33m"
#define COL_CYAN "\033[36m"

static bool use_colors(void) {
  static int result = -1;
  if (result == -1) {
    const char *no_color = getenv("NO_COLOR");
    result = (platform_isatty(stdout) && (no_color == NULL || no_color[0] == '\0')) ? 1 : 0;
  }
  return result == 1;
}

#define C(code) (use_colors() ? (code) : "")

// Calendar serial <-> civil-date helpers (mt_days_from_civil / mt_civil_from_days)
// live in prayertimes.h so config.c can share them; used here to iterate an
// inclusive date range without touching struct tm / mktime (no DST hazards).

static void lower_copy(char *dst, size_t cap, const char *src) {
  size_t i = 0;
  for (; src[i] && i + 1 < cap; i++)
    dst[i] = (char)tolower((unsigned char)src[i]);
  dst[i] = '\0';
}

// ASCII box-drawing fallback (portable across all Windows code pages)
#define BOX_TL "+" // Top-left
#define BOX_TR "+" // Top-right
#define BOX_BL "+" // Bottom-left
#define BOX_BR "+" // Bottom-right
#define BOX_H "-"  // Horizontal
#define BOX_V "|"  // Vertical
#define BOX_VR "+" // Vertical-right
#define BOX_VL "+" // Vertical-left
#define BOX_VH "+" // Cross
#define BOX_HU "+" // Horizontal-up
#define BOX_HD "+" // Horizontal-down

static void print_horizontal_line(char pos) {
  const char *left, *mid, *right, *horiz;

  switch (pos) {
  case 't': // top
    left = BOX_TL;
    mid = BOX_HD;
    right = BOX_TR;
    horiz = BOX_H;
    break;
  case 'm': // middle
    left = BOX_VR;
    mid = BOX_VH;
    right = BOX_VL;
    horiz = BOX_H;
    break;
  case 'b': // bottom
    left = BOX_BL;
    mid = BOX_HU;
    right = BOX_BR;
    horiz = BOX_H;
    break;
  default:
    return;
  }

  printf("%s", left);
  for (int i = 0; i < 12; i++)
    printf("%s", horiz);
  printf("%s", mid);
  for (int i = 0; i < 10; i++)
    printf("%s", horiz);
  printf("%s", mid);
  for (int i = 0; i < 10; i++)
    printf("%s", horiz);
  printf("%s", mid);
  for (int i = 0; i < 23; i++)
    printf("%s", horiz);
  printf("%s\n", right);
}

void display_prayer_times_table(const struct PrayerTimes *times, const Config *cfg,
                                struct tm *date) {
  // Copy the caller's date to avoid clobbering it when platform_localtime() is called below
  struct tm date_copy = *date;

  const char *prayer_names[] = {"Fajr", "Sunrise", "Dhuha", "Dhuhr", "Asr", "Maghrib", "Isha"};
  PrayerType types[] = {PRAYER_FAJR, PRAYER_SUNRISE, PRAYER_DHUHA, PRAYER_DHUHR,
                        PRAYER_ASR,  PRAYER_MAGHRIB, PRAYER_ISHA};

  // Find the next upcoming prayer for today
  int next_idx = -1;
  {
    time_t now_t = time(NULL);
    struct tm now_buf;
    platform_localtime(&now_t, &now_buf);
    struct tm *now_tm = &now_buf;
    if (date_copy.tm_year == now_tm->tm_year && date_copy.tm_mon == now_tm->tm_mon &&
        date_copy.tm_mday == now_tm->tm_mday) {
      int dummy;
      PrayerType next = prayer_get_next(cfg, now_tm, (struct PrayerTimes *)times, &dummy);
      for (int i = 0; i < 7; i++) {
        if (types[i] == next) {
          next_idx = i;
          break;
        }
      }
    }
  }

  // Table header
  print_horizontal_line('t');
  printf("%s %s%-10s%s %s %s%-8s%s %s %s%-8s%s %s %s%-21s%s %s\n", BOX_V, C(COL_BOLD), "Prayer",
         C(COL_RESET), BOX_V, C(COL_BOLD), "Time", C(COL_RESET), BOX_V, C(COL_BOLD), "Status",
         C(COL_RESET), BOX_V, C(COL_BOLD), "Reminders", C(COL_RESET), BOX_V);
  print_horizontal_line('m');

  for (int i = 0; i < 7; i++) {
    double prayer_time = prayer_get_time(times, types[i]);
    char time_str[16];
    format_time_hm(prayer_time, time_str, sizeof(time_str));

    const PrayerConfig *pcfg = prayer_get_config(cfg, types[i]);
    bool enabled = pcfg->enabled;

    // Buffer sized for: MAX_REMINDERS * "1440, " + " min before" = 10*6+11 = 71
    char reminders[80] = "";
    if (enabled) {
      if (pcfg->reminder_count == 0) {
        snprintf(reminders, sizeof(reminders), "At prayer time");
      } else {
        size_t pos = 0;
        for (int j = 0; j < pcfg->reminder_count; j++) {
          int written = snprintf(reminders + pos, sizeof(reminders) - pos, "%s%d",
                                 j > 0 ? ", " : "", pcfg->reminders[j]);
          if (written > 0 && (size_t)written < sizeof(reminders) - pos)
            pos += (size_t)written;
        }
        snprintf(reminders + pos, sizeof(reminders) - pos, " min before");
      }
    } else {
      snprintf(reminders, sizeof(reminders), "-");
    }

    bool is_next = (i == next_idx);

    if (!enabled) {
      // Dim entire row; "Disabled" is exactly 8 chars
      printf("%s%s %-10s %s %-8s %s Disabled %s %-21s %s%s\n", C(COL_DIM), BOX_V, prayer_names[i],
             BOX_V, time_str, BOX_V, BOX_V, "-", BOX_V, C(COL_RESET));
    } else if (is_next) {
      // Next prayer: bold+yellow name, yellow time, > indicator
      printf("%s%s%s%-10s%s %s %s%-8s%s %s %sEnabled %s %s %-21s %s\n", BOX_V,
             C(COL_BOLD COL_YELLOW), use_colors() ? ">" : " ", prayer_names[i], C(COL_RESET), BOX_V,
             C(COL_YELLOW), time_str, C(COL_RESET), BOX_V, C(COL_GREEN), C(COL_RESET), BOX_V,
             reminders, BOX_V);
    } else {
      // Normal enabled row; "Enabled " (7+1 space) = 8 chars
      printf("%s %-10s %s %-8s %s %sEnabled %s %s %-21s %s\n", BOX_V, prayer_names[i], BOX_V,
             time_str, BOX_V, C(COL_GREEN), C(COL_RESET), BOX_V, reminders, BOX_V);
    }
  }

  print_horizontal_line('b');
  printf("\n");
}

void display_prayer_times_plain(const struct PrayerTimes *times, const Config *cfg,
                                struct tm *date) {
  struct tm date_copy = *date;

  const char *prayer_names[] = {"fajr", "sunrise", "dhuha", "dhuhr", "asr", "maghrib", "isha"};
  PrayerType types[] = {PRAYER_FAJR, PRAYER_SUNRISE, PRAYER_DHUHA, PRAYER_DHUHR,
                        PRAYER_ASR,  PRAYER_MAGHRIB, PRAYER_ISHA};

  // Find the next upcoming prayer for today
  int next_idx = -1;
  {
    time_t now_t = time(NULL);
    struct tm now_buf;
    platform_localtime(&now_t, &now_buf);
    struct tm *now_tm = &now_buf;
    if (date_copy.tm_year == now_tm->tm_year && date_copy.tm_mon == now_tm->tm_mon &&
        date_copy.tm_mday == now_tm->tm_mday) {
      int dummy;
      PrayerType next = prayer_get_next(cfg, now_tm, (struct PrayerTimes *)times, &dummy);
      for (int i = 0; i < 7; i++) {
        if (types[i] == next) {
          next_idx = i;
          break;
        }
      }
    }
  }

  for (int i = 0; i < 7; i++) {
    const PrayerConfig *pcfg = prayer_get_config(cfg, types[i]);
    if (!pcfg->enabled)
      continue;

    double prayer_time = prayer_get_time(times, types[i]);
    char time_str[16];
    format_time_hm(prayer_time, time_str, sizeof(time_str));

    if (i == next_idx) {
      printf("%s%s%s=%s%s\n", C(COL_BOLD COL_YELLOW), prayer_names[i],
             C(COL_RESET COL_BOLD COL_YELLOW), time_str, C(COL_RESET));
    } else {
      printf("%s=%s\n", prayer_names[i], time_str);
    }
  }
}

// Print the 7 prayer entries that form the CONTENTS of a "prayers": { ... }
// object, one per line, each prefixed by `pad` spaces of indentation. The
// caller prints the enclosing `"prayers": {` and `}`. Shared by single-day and
// range JSON so their per-prayer shape stays in sync.
static void print_prayer_entries(const struct PrayerTimes *times, const Config *cfg,
                                 const char *pad) {
  const char *prayer_names[] = {"fajr", "sunrise", "dhuha", "dhuhr", "asr", "maghrib", "isha"};
  PrayerType types[] = {PRAYER_FAJR, PRAYER_SUNRISE, PRAYER_DHUHA, PRAYER_DHUHR,
                        PRAYER_ASR,  PRAYER_MAGHRIB, PRAYER_ISHA};

  for (int i = 0; i < 7; i++) {
    double prayer_time = prayer_get_time(times, types[i]);
    char time_str[16];
    format_time_hm(prayer_time, time_str, sizeof(time_str));

    const PrayerConfig *pcfg = prayer_get_config(cfg, types[i]);

    printf("%s\"%s\": {\n", pad, prayer_names[i]);
    printf("%s  \"time\": \"%s\",\n", pad, time_str);
    printf("%s  \"enabled\": %s,\n", pad, pcfg->enabled ? "true" : "false");
    printf("%s  \"reminders\": [", pad);
    for (int j = 0; j < pcfg->reminder_count; j++) {
      printf("%d", pcfg->reminders[j]);
      if (j < pcfg->reminder_count - 1)
        printf(", ");
    }
    printf("]\n");
    printf("%s}%s\n", pad, i < 6 ? "," : "");
  }
}

void display_prayer_times_json(const struct PrayerTimes *times, const Config *cfg,
                               struct tm *date) {
  (void)date;
  printf("{\n");
  printf("  \"prayers\": {\n");
  print_prayer_entries(times, cfg, "    ");
  printf("  }\n");
  printf("}\n");
}

void display_prayer_times_range_json(const Config *cfg, int sy, int sm, int sd, int ey, int em,
                                     int ed) {
  long start = mt_days_from_civil(sy, sm, sd);
  long end = mt_days_from_civil(ey, em, ed);

  printf("[\n");
  for (long z = start; z <= end; z++) {
    int y, m, d;
    mt_civil_from_days(z, &y, &m, &d);
    struct PrayerTimes t = prayer_times_for_config(cfg, y, m, d);

    printf("  {\n");
    printf("    \"date\": \"%04d-%02d-%02d\",\n", y, m, d);
    printf("    \"prayers\": {\n");
    print_prayer_entries(&t, cfg, "      ");
    printf("    }\n");
    printf("  }%s\n", z < end ? "," : "");
  }
  printf("]\n");
}

void display_prayer_times_range_plain(const Config *cfg, int sy, int sm, int sd, int ey, int em,
                                      int ed) {
  const char *prayer_names[] = {"fajr", "sunrise", "dhuha", "dhuhr", "asr", "maghrib", "isha"};
  PrayerType types[] = {PRAYER_FAJR, PRAYER_SUNRISE, PRAYER_DHUHA, PRAYER_DHUHR,
                        PRAYER_ASR,  PRAYER_MAGHRIB, PRAYER_ISHA};

  long start = mt_days_from_civil(sy, sm, sd);
  long end = mt_days_from_civil(ey, em, ed);

  for (long z = start; z <= end; z++) {
    int y, m, d;
    mt_civil_from_days(z, &y, &m, &d);
    struct PrayerTimes t = prayer_times_for_config(cfg, y, m, d);

    printf("date=%04d-%02d-%02d\n", y, m, d);
    for (int i = 0; i < 7; i++) {
      const PrayerConfig *pcfg = prayer_get_config(cfg, types[i]);
      if (!pcfg->enabled)
        continue;
      char time_str[16];
      format_time_hm(prayer_get_time(&t, types[i]), time_str, sizeof(time_str));
      printf("%s=%s\n", prayer_names[i], time_str);
    }
    if (z < end)
      printf("\n");
  }
}

// Print a single unbroken horizontal rule: '+' then `inner` dashes then "+\n".
static void range_hrule(int inner) {
  putchar('+');
  for (int i = 0; i < inner; i++)
    putchar('-');
  printf("+\n");
}

void display_prayer_times_range_table(const Config *cfg, int sy, int sm, int sd, int ey, int em,
                                      int ed) {
  const char *prayer_names[] = {"Fajr", "Sunrise", "Dhuha", "Dhuhr", "Asr", "Maghrib", "Isha"};
  PrayerType types[] = {PRAYER_FAJR, PRAYER_SUNRISE, PRAYER_DHUHA, PRAYER_DHUHR,
                        PRAYER_ASR,  PRAYER_MAGHRIB, PRAYER_ISHA};

  // Columns are the enabled prayers only; disabled ones are omitted entirely.
  int col[7];
  int ncol = 0;
  for (int i = 0; i < 7; i++) {
    if (prayer_get_config(cfg, types[i])->enabled)
      col[ncol++] = i;
  }

  // Column widths: Date is "YYYY-MM-DD" (10); each prayer is max(name, "HH:MM"=5).
  const int date_w = 10;
  int col_w[7];
  for (int c = 0; c < ncol; c++) {
    int len = (int)strlen(prayer_names[col[c]]);
    col_w[c] = len > 5 ? len : 5;
  }

  // Total printed row length, for an unbroken border spanning the whole table.
  // Each cell prints as "| %-*s " = width + 3 chars; a trailing "|" closes the row.
  int row_len = 1 + (date_w + 3);
  for (int c = 0; c < ncol; c++)
    row_len += col_w[c] + 3;

  range_hrule(row_len - 2);
  printf("| %-*s ", date_w, "Date");
  for (int c = 0; c < ncol; c++)
    printf("| %-*s ", col_w[c], prayer_names[col[c]]);
  printf("|\n");
  range_hrule(row_len - 2);

  long start = mt_days_from_civil(sy, sm, sd);
  long end = mt_days_from_civil(ey, em, ed);
  for (long z = start; z <= end; z++) {
    int y, m, d;
    mt_civil_from_days(z, &y, &m, &d);
    struct PrayerTimes t = prayer_times_for_config(cfg, y, m, d);
    printf("| %04d-%02d-%02d ", y, m, d);
    for (int c = 0; c < ncol; c++) {
      char time_str[16];
      format_time_hm(prayer_get_time(&t, types[col[c]]), time_str, sizeof(time_str));
      printf("| %-*s ", col_w[c], time_str);
    }
    printf("|\n");
  }
  range_hrule(row_len - 2);
}

// Resolve the next upcoming prayer and format its fields. Returns false (and
// writes nothing) when there is no upcoming prayer. `name` is the display name
// as-is (capitalized); callers that need a lowercase key run lower_copy on it.
static bool next_prayer_info(const struct PrayerTimes *times, const Config *cfg,
                             struct tm *current_time, const char **name, char *time_str,
                             size_t time_cap, char *remaining, size_t rem_cap) {
  int minutes_until = 0;
  PrayerType next = prayer_get_next(cfg, current_time, (struct PrayerTimes *)times, &minutes_until);
  if (next == PRAYER_NONE)
    return false;

  *name = prayer_get_name(next);

  double now_dec = current_time->tm_hour + current_time->tm_min / 60.0;
  double next_time = prayer_get_time(times, next);
  if (next_time < now_dec) {
    // The next occurrence is tomorrow (every prayer today has passed). Recompute
    // that prayer's time for the next day's date so the displayed clock time is
    // exact, and derive `remaining` from that same next-day time.
    long serial = mt_days_from_civil(current_time->tm_year + 1900, current_time->tm_mon + 1,
                                     current_time->tm_mday) +
                  1;
    int ny, nm, nd;
    mt_civil_from_days(serial, &ny, &nm, &nd);
    struct PrayerTimes tomorrow = prayer_times_for_config(cfg, ny, nm, nd);
    next_time = prayer_get_time(&tomorrow, next);
    minutes_until = (int)((next_time + 24.0 - now_dec) * 60.0);
  }

  format_time_hm(next_time, time_str, time_cap);
  snprintf(remaining, rem_cap, "%02d:%02d", minutes_until / 60, minutes_until % 60);
  return true;
}

void display_next_prayer(const struct PrayerTimes *times, const Config *cfg,
                         struct tm *current_time) {
  const char *name;
  char time_str[16], remaining[16];
  if (!next_prayer_info(times, cfg, current_time, &name, time_str, sizeof(time_str), remaining,
                        sizeof(remaining))) {
    printf("No upcoming prayers enabled.\n");
    return;
  }

  printf("+------------+----------+-----------+\n");
  printf("| %-10s | %-8s | %-9s |\n", "Prayer", "Time", "Remaining");
  printf("+------------+----------+-----------+\n");
  printf("| %-10s | %-8s | %-9s |\n", name, time_str, remaining);
  printf("+------------+----------+-----------+\n");
}

void display_next_prayer_headless(const struct PrayerTimes *times, const Config *cfg,
                                  struct tm *current_time) {
  const char *name;
  char time_str[16], remaining[16];
  if (!next_prayer_info(times, cfg, current_time, &name, time_str, sizeof(time_str), remaining,
                        sizeof(remaining))) {
    printf("No upcoming prayers enabled.\n");
    return;
  }

  char lname[16];
  lower_copy(lname, sizeof(lname), name);
  printf("%s=%s\n", lname, time_str);
  printf("remaining=%s\n", remaining);
}

void display_next_prayer_json(const struct PrayerTimes *times, const Config *cfg,
                              struct tm *current_time) {
  const char *name;
  char time_str[16], remaining[16];
  if (!next_prayer_info(times, cfg, current_time, &name, time_str, sizeof(time_str), remaining,
                        sizeof(remaining))) {
    printf("{}\n");
    return;
  }

  char lname[16];
  lower_copy(lname, sizeof(lname), name);
  printf("{\n");
  printf("  \"prayer\": \"%s\",\n", lname);
  printf("  \"time\": \"%s\",\n", time_str);
  printf("  \"remaining\": \"%s\"\n", remaining);
  printf("}\n");
}

static void loc_border(int nw, int vw) {
  putchar('+');
  for (int i = 0; i < nw + 2; i++)
    putchar('-');
  putchar('+');
  for (int i = 0; i < vw + 2; i++)
    putchar('-');
  putchar('+');
  putchar('\n');
}

static void json_str(const char *s) {
  putchar('"');
  for (; *s; s++) {
    if (*s == '"' || *s == '\\') {
      putchar('\\');
      putchar(*s);
    } else if ((unsigned char)*s < 0x20) {
      printf("\\u%04x", (unsigned char)*s);
    } else {
      putchar(*s);
    }
  }
  putchar('"');
}

// The location display shows the offset in effect for TODAY, so it tracks DST
// rather than the value frozen in the config when the location was last set.
static double display_current_offset(const Config *cfg) {
  time_t now_t = time(NULL);
  struct tm lt;
  platform_localtime(&now_t, &lt);
  return effective_tz_offset(cfg, lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
}

void display_location(const Config *cfg) {
  char coords[32], gmt[16];
  snprintf(coords, sizeof(coords), "%.4f,%.4f", cfg->latitude, cfg->longitude);
  snprintf(gmt, sizeof(gmt), "UTC%+.1f", display_current_offset(cfg));

  // "disabled" when 0, else "<n>s".
  char refresh[24];
  if (cfg->refresh_interval <= 0)
    snprintf(refresh, sizeof(refresh), "disabled");
  else
    snprintf(refresh, sizeof(refresh), "%llds", (long long)cfg->refresh_interval);

  const char *rows[][2] = {
      {"coordinates", coords},
      {"city", cfg->city},
      {"country", cfg->country},
      {"timezone", cfg->timezone},
      {"gmt", gmt},
      {"refresh_interval", refresh},
      {"gps", cfg->use_gps ? "enabled" : "disabled"},
  };

  int nw = (int)strlen("Name"), vw = (int)strlen("Value");
  for (size_t i = 0; i < ARRAY_LEN(rows); i++) {
    int l = (int)strlen(rows[i][0]);
    if (l > nw)
      nw = l;
    l = (int)strlen(rows[i][1]);
    if (l > vw)
      vw = l;
  }

  loc_border(nw, vw);
  printf("| %-*s | %-*s |\n", nw, "Name", vw, "Value");
  loc_border(nw, vw);
  for (size_t i = 0; i < ARRAY_LEN(rows); i++)
    printf("| %-*s | %-*s |\n", nw, rows[i][0], vw, rows[i][1]);
  loc_border(nw, vw);
}

void display_location_headless(const Config *cfg) {
  printf("coordinates=%.4f,%.4f\n", cfg->latitude, cfg->longitude);
  printf("city=%s\n", cfg->city);
  printf("country=%s\n", cfg->country);
  printf("timezone=%s\n", cfg->timezone);
  printf("gmt=UTC%+.1f\n", display_current_offset(cfg));
  printf("gps=%s\n", cfg->use_gps ? "true" : "false");
  printf("refresh_interval=%lld\n", (long long)cfg->refresh_interval);
}

void display_location_json(const Config *cfg) {
  char coords[32], gmt[16];
  snprintf(coords, sizeof(coords), "%.4f,%.4f", cfg->latitude, cfg->longitude);
  snprintf(gmt, sizeof(gmt), "UTC%+.1f", display_current_offset(cfg));

  printf("{\n");
  printf("  \"coordinates\": ");
  json_str(coords);
  printf(",\n");
  printf("  \"city\": ");
  json_str(cfg->city);
  printf(",\n");
  printf("  \"country\": ");
  json_str(cfg->country);
  printf(",\n");
  printf("  \"timezone\": ");
  json_str(cfg->timezone);
  printf(",\n");
  printf("  \"gmt\": ");
  json_str(gmt);
  printf(",\n");
  printf("  \"gps\": %s,\n", cfg->use_gps ? "true" : "false");
  printf("  \"refresh_interval\": %lld\n", (long long)cfg->refresh_interval);
  printf("}\n");
}

void display_notification_settings(const Config *cfg) {
  const char *names[] = {"fajr", "sunrise", "dhuha", "dhuhr", "asr", "maghrib", "isha"};
  const PrayerConfig *pc[] = {&cfg->fajr, &cfg->sunrise, &cfg->dhuha, &cfg->dhuhr,
                              &cfg->asr,  &cfg->maghrib, &cfg->isha};

  printf("+---------+---------+---------------+-------+\n");
  printf("| %-7s | %-7s | %-13s | %-5s |\n", "Prayer", "Enabled", "Reminders", "Adhan");
  printf("+---------+---------+---------------+-------+\n");
  for (int i = 0; i < 7; i++) {
    char reminders[64];
    config_format_reminders(pc[i], reminders, sizeof(reminders));
    printf("| %-7s | %-7s | %-13s | %-5s |\n", names[i], pc[i]->enabled ? "yes" : "no", reminders,
           pc[i]->adhan_enabled ? "on" : "off");
  }
  printf("+---------+---------+---------------+-------+\n");
  printf("sound: %s\n", cfg->notification_sound);
  printf("urgency: %s\n", cfg->notification_urgency);
}

void display_notification_settings_headless(const Config *cfg) {
  const char *names[] = {"fajr", "sunrise", "dhuha", "dhuhr", "asr", "maghrib", "isha"};
  const PrayerConfig *pc[] = {&cfg->fajr, &cfg->sunrise, &cfg->dhuha, &cfg->dhuhr,
                              &cfg->asr,  &cfg->maghrib, &cfg->isha};

  printf("sound=%s\n", cfg->notification_sound);
  printf("urgency=%s\n", cfg->notification_urgency);
  for (int i = 0; i < 7; i++) {
    char reminders[64];
    config_format_reminders(pc[i], reminders, sizeof(reminders));
    printf("%s.enabled=%s\n", names[i], pc[i]->enabled ? "true" : "false");
    printf("%s.reminders=%s\n", names[i], reminders);
    printf("%s.adhan=%s\n", names[i], pc[i]->adhan_enabled ? "true" : "false");
  }
}

void display_notification_settings_json(const Config *cfg) {
  const char *names[] = {"fajr", "sunrise", "dhuha", "dhuhr", "asr", "maghrib", "isha"};
  const PrayerConfig *pc[] = {&cfg->fajr, &cfg->sunrise, &cfg->dhuha, &cfg->dhuhr,
                              &cfg->asr,  &cfg->maghrib, &cfg->isha};

  printf("{\n");
  printf("  \"sound\": ");
  json_str(cfg->notification_sound);
  printf(",\n");
  printf("  \"urgency\": ");
  json_str(cfg->notification_urgency);
  printf(",\n");
  printf("  \"prayers\": {\n");
  for (int i = 0; i < 7; i++) {
    printf("    \"%s\": { \"enabled\": %s, \"reminders\": [", names[i],
           pc[i]->enabled ? "true" : "false");
    for (int j = 0; j < pc[i]->reminder_count; j++) {
      printf("%d", pc[i]->reminders[j]);
      if (j < pc[i]->reminder_count - 1)
        printf(", ");
    }
    printf("], \"adhan\": %s }%s\n", pc[i]->adhan_enabled ? "true" : "false", i < 6 ? "," : "");
  }
  printf("  }\n");
  printf("}\n");
}
