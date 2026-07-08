#define PRAYERTIMES_IMPLEMENTATION
#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "display.h"
#include "platform.h"
#include "prayer_checker.h"
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

void display_prayer_times_json(const struct PrayerTimes *times, const Config *cfg,
                               struct tm *date) {
  (void)date;
  printf("{\n");
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
  format_time_hm(prayer_get_time(times, next), time_str, time_cap);
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

void display_location(const Config *cfg) {
  char coords[32], gmt[16];
  snprintf(coords, sizeof(coords), "%.4f,%.4f", cfg->latitude, cfg->longitude);
  snprintf(gmt, sizeof(gmt), "UTC%+.1f", cfg->timezone_offset);

  const char *rows[][2] = {
      {"coordinates", coords},     {"city", cfg->city}, {"country", cfg->country},
      {"timezone", cfg->timezone}, {"gmt", gmt},
  };

  int nw = (int)strlen("Name"), vw = (int)strlen("Value");
  for (int i = 0; i < 5; i++) {
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
  for (int i = 0; i < 5; i++)
    printf("| %-*s | %-*s |\n", nw, rows[i][0], vw, rows[i][1]);
  loc_border(nw, vw);
}

void display_location_headless(const Config *cfg) {
  printf("coordinates=%.4f,%.4f\n", cfg->latitude, cfg->longitude);
  printf("city=%s\n", cfg->city);
  printf("country=%s\n", cfg->country);
  printf("timezone=%s\n", cfg->timezone);
  printf("gmt=UTC%+.1f\n", cfg->timezone_offset);
}

void display_location_json(const Config *cfg) {
  char coords[32], gmt[16];
  snprintf(coords, sizeof(coords), "%.4f,%.4f", cfg->latitude, cfg->longitude);
  snprintf(gmt, sizeof(gmt), "UTC%+.1f", cfg->timezone_offset);

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
  printf("\n}\n");
}

void display_config(const Config *cfg) {
  printf("\nConfiguration:\n\n");
  display_location(cfg);

  printf("Notification Settings:\n");
  printf("  Timeout: %d ms\n", cfg->notification_timeout);
  printf("  Urgency: %s\n", cfg->notification_urgency);
  printf("  Sound: %s\n", cfg->notification_sound);
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
    char off[24] = "";
    if (pcfg->offset != 0) {
      snprintf(off, sizeof(off), " [%+d min]", pcfg->offset);
    }
    char snd[MAX_ADHAN_PATH + 16] = "";
    if (pcfg->adhan[0] != '\0') {
      snprintf(snd, sizeof(snd), " (sound: %s)", pcfg->adhan_enabled ? pcfg->adhan : "disabled");
    }
    printf("  %-8s: ", prayer_names[i]);

    if (!pcfg->enabled) {
      printf("(disabled)%s\n", off);
      continue;
    }

    if (pcfg->reminder_count == 0) {
      printf("At prayer time only%s\n", off);
      continue;
    }

    printf("%d reminder%s: ", pcfg->reminder_count, pcfg->reminder_count == 1 ? "" : "s");

    for (int j = 0; j < pcfg->reminder_count; j++) {
      printf("%d", pcfg->reminders[j]);
      if (j < pcfg->reminder_count - 1)
        printf(", ");
    }
    printf(" min before%s%s\n", off, snd);
  }
  printf("\n");
}

void display_notification_settings(const Config *cfg) {
  const char *names[] = {"fajr", "sunrise", "dhuha", "dhuhr", "asr", "maghrib", "isha"};
  const PrayerConfig *pc[] = {&cfg->fajr,    &cfg->sunrise, &cfg->dhuha, &cfg->dhuhr,
                              &cfg->asr,      &cfg->maghrib, &cfg->isha};

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
  const PrayerConfig *pc[] = {&cfg->fajr,    &cfg->sunrise, &cfg->dhuha, &cfg->dhuhr,
                              &cfg->asr,      &cfg->maghrib, &cfg->isha};

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
  const PrayerConfig *pc[] = {&cfg->fajr,    &cfg->sunrise, &cfg->dhuha, &cfg->dhuhr,
                              &cfg->asr,      &cfg->maghrib, &cfg->isha};

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
