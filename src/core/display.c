#define PRAYERTIMES_IMPLEMENTATION
#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "display.h"
#include "platform.h"
#include "prayer_checker.h"
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

  const char *days[] = {"Sunday",   "Monday", "Tuesday", "Wednesday",
                        "Thursday", "Friday", "Saturday"};
  const char *months[] = {"January", "February", "March",     "April",   "May",      "June",
                          "July",    "August",   "September", "October", "November", "December"};

  int wday = (date_copy.tm_wday >= 0 && date_copy.tm_wday <= 6) ? date_copy.tm_wday : 0;
  int mon = (date_copy.tm_mon >= 0 && date_copy.tm_mon <= 11) ? date_copy.tm_mon : 0;
  printf("\n%sPrayer Times for %s, %s %d, %d%s\n", C(COL_BOLD), days[wday], months[mon],
         date_copy.tm_mday, date_copy.tm_year + 1900, C(COL_RESET));

  if (cfg->city[0] != '\0') {
    printf("Location: %s, %s (%.4f, %.4f)\n\n", cfg->city, cfg->country, cfg->latitude,
           cfg->longitude);
  } else {
    printf("Location: %.4f, %.4f\n\n", cfg->latitude, cfg->longitude);
  }

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

static void json_print_escaped(const char *s) {
  putchar('"');
  for (; *s; s++) {
    switch (*s) {
    case '"':
      fputs("\\\"", stdout);
      break;
    case '\\':
      fputs("\\\\", stdout);
      break;
    case '\b':
      fputs("\\b", stdout);
      break;
    case '\f':
      fputs("\\f", stdout);
      break;
    case '\n':
      fputs("\\n", stdout);
      break;
    case '\r':
      fputs("\\r", stdout);
      break;
    case '\t':
      fputs("\\t", stdout);
      break;
    default:
      if ((unsigned char)*s < 0x20) {
        printf("\\u%04x", (unsigned char)*s);
      } else {
        putchar(*s);
      }
      break;
    }
  }
  putchar('"');
}

void display_prayer_times_json(const struct PrayerTimes *times, const Config *cfg,
                               struct tm *date) {
  printf("{\n");
  printf("  \"date\": \"%04d-%02d-%02d\",\n", date->tm_year + 1900, date->tm_mon + 1,
         date->tm_mday);
  printf("  \"location\": {\n");
  printf("    \"latitude\": %.6f,\n", cfg->latitude);
  printf("    \"longitude\": %.6f,\n", cfg->longitude);
  printf("    \"city\": ");
  json_print_escaped(cfg->city);
  printf(",\n");
  printf("    \"country\": ");
  json_print_escaped(cfg->country);
  printf("\n");
  printf("  },\n");
  printf("  \"prayers\": {\n");

  const char *prayer_names[] = {"fajr", "sunrise", "dhuha", "dhuhr", "asr", "maghrib", "isha"};
  PrayerType types[] = {PRAYER_FAJR, PRAYER_SUNRISE, PRAYER_DHUHA, PRAYER_DHUHR,
                        PRAYER_ASR,  PRAYER_MAGHRIB, PRAYER_ISHA};

  for (int i = 0; i < 7; i++) {
    double prayer_time = prayer_get_time(times, types[i]);
    char time_str[16];
    format_time_hm(prayer_time, time_str, sizeof(time_str));

    const PrayerConfig *pcfg = prayer_get_config(cfg, types[i]);

    printf("    \"%s\": {\n", prayer_names[i]);
    printf("      \"time\": \"%s\",\n", time_str);
    printf("      \"enabled\": %s,\n", pcfg->enabled ? "true" : "false");
    printf("      \"reminders\": [");
    for (int j = 0; j < pcfg->reminder_count; j++) {
      printf("%d", pcfg->reminders[j]);
      if (j < pcfg->reminder_count - 1)
        printf(", ");
    }
    printf("]\n");
    printf("    }%s\n", i < 6 ? "," : "");
  }

  printf("  }\n");
  printf("}\n");
}

void display_next_prayer(const struct PrayerTimes *times, const Config *cfg,
                         struct tm *current_time) {
  int minutes_until = 0;
  PrayerType next = prayer_get_next(cfg, current_time, (struct PrayerTimes *)times, &minutes_until);

  if (next == PRAYER_NONE) {
    printf("No upcoming prayers enabled.\n");
    return;
  }

  double prayer_time = prayer_get_time(times, next);
  char time_str[16];
  format_time_hm(prayer_time, time_str, sizeof(time_str));

  printf("\nNext Prayer: %s\n", prayer_get_name(next));
  printf("Time: %s\n", time_str);

  int hours = minutes_until / 60;
  int mins = minutes_until % 60;

  if (hours > 0) {
    printf("Remaining: %d hour%s %d minute%s\n\n", hours, hours == 1 ? "" : "s", mins,
           mins == 1 ? "" : "s");
  } else {
    printf("Remaining: %d minute%s\n\n", mins, mins == 1 ? "" : "s");
  }
}

void display_location(const Config *cfg) {
  printf("\nLocation Information:\n");
  printf("  Coordinates: %.4f, %.4f\n", cfg->latitude, cfg->longitude);
  if (cfg->city[0] != '\0') {
    printf("  City: %s\n", cfg->city);
  }
  if (cfg->country[0] != '\0') {
    printf("  Country: %s\n", cfg->country);
  }
  printf("  Timezone: %s (UTC%+.1f)\n", cfg->timezone, cfg->timezone_offset);
  printf("  Auto-detect: %s\n\n", cfg->auto_detect ? "enabled" : "disabled");
}

void display_config(const Config *cfg) {
  printf("\nConfiguration:\n\n");
  display_location(cfg);

  printf("Notification Settings:\n");
  printf("  Timeout: %d ms\n", cfg->notification_timeout);
  printf("  Urgency: %s\n", cfg->notification_urgency);
  printf("  Sound: %s\n", cfg->notification_sound ? "enabled" : "disabled");
  printf("  Icon: %s\n\n", cfg->notification_icon);

  CalcMethod method = method_from_string(cfg->calculation_method);
  const MethodParams *mparams = method_params_get(method);
  printf("Calculation Method:\n");
  printf("  Method: %s (%s)\n", cfg->calculation_method, mparams ? mparams->name : "Unknown");
  printf("  Madhab: %s\n\n", cfg->madhab);

  display_reminders(cfg);
}

void display_prayer_list(const Config *cfg) {
  printf("\nPrayer Notifications:\n");

  const char *prayer_names[] = {"Fajr", "Sunrise", "Dhuha", "Dhuhr", "Asr", "Maghrib", "Isha"};
  PrayerType types[] = {PRAYER_FAJR, PRAYER_SUNRISE, PRAYER_DHUHA, PRAYER_DHUHR,
                        PRAYER_ASR,  PRAYER_MAGHRIB, PRAYER_ISHA};

  int enabled_count = 0;
  printf("  Enabled:  ");
  for (int i = 0; i < 7; i++) {
    const PrayerConfig *pcfg = prayer_get_config(cfg, types[i]);
    if (pcfg->enabled) {
      if (enabled_count > 0)
        printf(", ");
      printf("%s", prayer_names[i]);
      enabled_count++;
    }
  }
  if (enabled_count == 0)
    printf("None");
  printf("\n");

  int disabled_count = 0;
  printf("  Disabled: ");
  for (int i = 0; i < 7; i++) {
    const PrayerConfig *pcfg = prayer_get_config(cfg, types[i]);
    if (!pcfg->enabled) {
      if (disabled_count > 0)
        printf(", ");
      printf("%s", prayer_names[i]);
      disabled_count++;
    }
  }
  if (disabled_count == 0)
    printf("None");
  printf("\n\n");
}

void display_reminders(const Config *cfg) {
  printf("Prayer Reminders:\n");

  const char *prayer_names[] = {"Fajr", "Sunrise", "Dhuha", "Dhuhr", "Asr", "Maghrib", "Isha"};
  PrayerType types[] = {PRAYER_FAJR, PRAYER_SUNRISE, PRAYER_DHUHA, PRAYER_DHUHR,
                        PRAYER_ASR,  PRAYER_MAGHRIB, PRAYER_ISHA};

  for (int i = 0; i < 7; i++) {
    const PrayerConfig *pcfg = prayer_get_config(cfg, types[i]);
    printf("  %-8s: ", prayer_names[i]);

    if (!pcfg->enabled) {
      printf("(disabled)\n");
      continue;
    }

    if (pcfg->reminder_count == 0) {
      printf("At prayer time only\n");
      continue;
    }

    printf("%d reminder%s: ", pcfg->reminder_count, pcfg->reminder_count == 1 ? "" : "s");

    for (int j = 0; j < pcfg->reminder_count; j++) {
      printf("%d", pcfg->reminders[j]);
      if (j < pcfg->reminder_count - 1)
        printf(", ");
    }
    printf(" min before\n");
  }
  printf("\n");
}
