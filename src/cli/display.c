#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "display.h"
#include "muslimtify.h"
#include "platform.h"
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

  // The three branches below are semantically distinct: top uses
  // BOX_TL/BOX_HD/BOX_TR, middle uses BOX_VR/BOX_VH/BOX_VL, bottom uses
  // BOX_BL/BOX_HU/BOX_BR.
  // They only look identical here because the ASCII fallback above maps
  // every BOX_* macro to "+" for Windows code page portability.
  // They diverge again the moment those macros carry real box-drawing
  // characters, so do not collapse this switch.
  // NOLINTBEGIN(bugprone-branch-clone)
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
  // NOLINTEND(bugprone-branch-clone)

  printf("%s", left);
  for (int i = 0; i < 12; i++)
    printf("%s", horiz);
  printf("%s", mid);
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

// Clock string for one cell. With `mark`, a time that falls on the next or the
// previous day gets a '+' or '-' appended.
static void format_cell(const MuslimtifyTime *t, int time_format, bool mark, char *out,
                        size_t cap) {
  char clock[MUSLIMTIFY_TIME_STR_SIZE];
  muslimtify_format_time(t, time_format, clock, sizeof(clock));
  int day = mark ? t->day_offset : 0;
  snprintf(out, cap, "%s%s", clock, day > 0 ? "+" : (day < 0 ? "-" : ""));
}

static void prayer_key(int index, char *out, size_t cap) {
  lower_copy(out, cap, muslimtify_prayer_name((MuslimtifyPrayerType)index));
}

/* Prints the legend for the day markers that were actually rendered, and
   nothing at all when none were. The marker is a single character and cannot
   carry what it means, so the table says it in words rather than relying on
   the reader knowing. */
static void print_day_marker_legend(bool any_next, bool any_prev) {
  if (any_next)
    printf("  + falls after midnight, on the next day\n");
  if (any_prev)
    printf("  - falls before midnight, on the previous day\n");
}

void display_day_table(const MuslimtifyDay *day, int time_format) {
  // Table header
  print_horizontal_line('t');
  printf("%s %s%-10s%s %s %s%-10s%s %s %s%-8s%s %s %s%-8s%s %s %s%-21s%s %s\n", BOX_V, C(COL_BOLD),
         "Date", C(COL_RESET), BOX_V, C(COL_BOLD), "Prayer", C(COL_RESET), BOX_V, C(COL_BOLD),
         "Time", C(COL_RESET), BOX_V, C(COL_BOLD), "Status", C(COL_RESET), BOX_V, C(COL_BOLD),
         "Reminders", C(COL_RESET), BOX_V);
  print_horizontal_line('m');

  for (int i = 0; i < MUSLIMTIFY_PRAYER_COUNT; i++) {
    const MuslimtifyPrayer *p = &day->prayers[i];
    const char *name = muslimtify_prayer_name((MuslimtifyPrayerType)i);
    char time_str[16];
    format_cell(&p->time, time_format, false, time_str, sizeof(time_str));

    // The Date column carries the day a prayer actually falls on, so the time
    // needs no marker. A time that does not exist has no day of its own.
    char date_str[40];
    if (p->time.valid)
      snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d", p->time.year, p->time.month,
               p->time.day);
    else
      snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d", day->year, day->month, day->day);

    // Buffer sized for: MUSLIMTIFY_MAX_REMINDERS * "1440, " + " min before" = 10*6+11 = 71
    char reminders[80] = "";
    if (p->enabled) {
      if (p->reminder_count == 0) {
        snprintf(reminders, sizeof(reminders), "At prayer time");
      } else {
        size_t pos = 0;
        for (int j = 0; j < p->reminder_count; j++) {
          int written = snprintf(reminders + pos, sizeof(reminders) - pos, "%s%d",
                                 j > 0 ? ", " : "", p->reminders[j]);
          if (written > 0 && (size_t)written < sizeof(reminders) - pos)
            pos += (size_t)written;
        }
        snprintf(reminders + pos, sizeof(reminders) - pos, " min before");
      }
    } else {
      snprintf(reminders, sizeof(reminders), "-");
    }

    bool is_next = (i == day->next);

    if (!p->enabled) {
      // Dim entire row; "Disabled" is exactly 8 chars
      printf("%s%s %-10s %s %-10s %s %-8s %s Disabled %s %-21s %s%s\n", C(COL_DIM), BOX_V, date_str,
             BOX_V, name, BOX_V, time_str, BOX_V, BOX_V, "-", BOX_V, C(COL_RESET));
    } else if (is_next) {
      // Next prayer: bold+yellow name, yellow time, > indicator
      printf("%s %-10s %s%s%s%-10s%s %s %s%-8s%s %s %sEnabled %s %s %-21s %s\n", BOX_V, date_str,
             BOX_V, C(COL_BOLD COL_YELLOW), use_colors() ? ">" : " ", name, C(COL_RESET), BOX_V,
             C(COL_YELLOW), time_str, C(COL_RESET), BOX_V, C(COL_GREEN), C(COL_RESET), BOX_V,
             reminders, BOX_V);
    } else {
      // Normal enabled row; "Enabled " (7+1 space) = 8 chars
      printf("%s %-10s %s %-10s %s %-8s %s %sEnabled %s %s %-21s %s\n", BOX_V, date_str, BOX_V,
             name, BOX_V, time_str, BOX_V, C(COL_GREEN), C(COL_RESET), BOX_V, reminders, BOX_V);
    }
  }

  print_horizontal_line('b');
  printf("\n");
}

// The key=value lines for one day's enabled prayers. `highlight` is the index
// to color as the next prayer, or -1 for none.
static void print_plain_prayers(const MuslimtifyDay *day, int time_format, int highlight) {
  for (int i = 0; i < MUSLIMTIFY_PRAYER_COUNT; i++) {
    const MuslimtifyPrayer *p = &day->prayers[i];
    if (!p->enabled)
      continue;

    char key[16];
    prayer_key(i, key, sizeof(key));
    char time_str[16];
    format_cell(&p->time, time_format, false, time_str, sizeof(time_str));

    if (i == highlight) {
      printf("%s%s%s=%s%s\n", C(COL_BOLD COL_YELLOW), key, C(COL_RESET COL_BOLD COL_YELLOW),
             time_str, C(COL_RESET));
    } else {
      printf("%s=%s\n", key, time_str);
    }

    if (p->time.day_offset != 0)
      printf("%s_offset=%d\n", key, p->time.day_offset);
  }
}

void display_day_plain(const MuslimtifyDay *day, int time_format) {
  printf("date=%04d-%02d-%02d\n", day->year, day->month, day->day);
  print_plain_prayers(day, time_format, day->next);
}

// Print the prayer entries that form the CONTENTS of a "prayers": { ... }
// object, one per line, each prefixed by `pad` spaces of indentation. The
// caller prints the enclosing `"prayers": {` and `}`. Shared by single-day and
// range JSON so their per-prayer shape stays in sync.
static void print_prayer_entries(const MuslimtifyDay *day, int time_format, const char *pad) {
  for (int i = 0; i < MUSLIMTIFY_PRAYER_COUNT; i++) {
    const MuslimtifyPrayer *p = &day->prayers[i];
    char key[16];
    prayer_key(i, key, sizeof(key));
    char time_str[16];
    format_cell(&p->time, time_format, false, time_str, sizeof(time_str));

    printf("%s\"%s\": {\n", pad, key);
    printf("%s  \"time\": \"%s\",\n", pad, time_str);
    printf("%s  \"offset\": %d,\n", pad, p->time.day_offset);
    printf("%s  \"enabled\": %s,\n", pad, p->enabled ? "true" : "false");
    printf("%s  \"reminders\": [", pad);
    for (int j = 0; j < p->reminder_count; j++) {
      printf("%d", p->reminders[j]);
      if (j < p->reminder_count - 1)
        printf(", ");
    }
    printf("]\n");
    /* Separator derives from the loop bound, not a literal, because a literal is what broke
       here when the prayer count changed from seven to five. */
    printf("%s}%s\n", pad, i + 1 < MUSLIMTIFY_PRAYER_COUNT ? "," : "");
  }
}

void display_day_json(const MuslimtifyDay *day, int time_format) {
  printf("{\n");
  printf("  \"date\": \"%04d-%02d-%02d\",\n", day->year, day->month, day->day);
  printf("  \"prayers\": {\n");
  print_prayer_entries(day, time_format, "    ");
  printf("  }\n");
  printf("}\n");
}

void display_range_json(const MuslimtifyDay *days, size_t count, int time_format) {
  printf("[\n");
  for (size_t i = 0; i < count; i++) {
    printf("  {\n");
    printf("    \"date\": \"%04d-%02d-%02d\",\n", days[i].year, days[i].month, days[i].day);
    printf("    \"prayers\": {\n");
    print_prayer_entries(&days[i], time_format, "      ");
    printf("    }\n");
    printf("  }%s\n", i + 1 < count ? "," : "");
  }
  printf("]\n");
}

void display_range_plain(const MuslimtifyDay *days, size_t count, int time_format) {
  for (size_t i = 0; i < count; i++) {
    printf("date=%04d-%02d-%02d\n", days[i].year, days[i].month, days[i].day);
    print_plain_prayers(&days[i], time_format, -1);
    if (i + 1 < count)
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

void display_range_table(const MuslimtifyDay *days, size_t count, int time_format) {
  if (count == 0)
    return;

  // Columns are the enabled prayers only; disabled ones are omitted entirely.
  // Whether a prayer is enabled is a setting, the same on every day.
  int col[MUSLIMTIFY_PRAYER_COUNT];
  int ncol = 0;
  for (int i = 0; i < MUSLIMTIFY_PRAYER_COUNT; i++) {
    if (days[0].prayers[i].enabled)
      col[ncol++] = i;
  }

  // See whether any cell will carry a day marker, so the minimum column width
  // matches what actually gets printed instead of always leaving room for a
  // marker that never appears.
  bool any_next = false;
  bool any_prev = false;
  for (size_t i = 0; i < count; i++) {
    for (int c = 0; c < ncol; c++) {
      int day = days[i].prayers[col[c]].time.day_offset;
      if (day > 0)
        any_next = true;
      else if (day < 0)
        any_prev = true;
    }
  }
  bool has_marker = any_next || any_prev;

  // Date is "YYYY-MM-DD".
  const int date_w = 10;
  // A clock cell is "HH:MM" (5) or "hh:MM AM" (8), plus one for a day marker.
  const int clock_w = (time_format == 12) ? 8 : 5;
  const int min_col_w = has_marker ? clock_w + 1 : clock_w;
  int col_w[MUSLIMTIFY_PRAYER_COUNT];
  for (int c = 0; c < ncol; c++) {
    int len = (int)strlen(muslimtify_prayer_name((MuslimtifyPrayerType)col[c]));
    col_w[c] = len > min_col_w ? len : min_col_w;
  }

  // Total printed row length, for an unbroken border spanning the whole table.
  // Each cell prints as "| %-*s " = width + 3 chars; a trailing "|" closes the row.
  int row_len = 1 + (date_w + 3);
  for (int c = 0; c < ncol; c++)
    row_len += col_w[c] + 3;

  range_hrule(row_len - 2);
  printf("| %-*s ", date_w, "Date");
  for (int c = 0; c < ncol; c++)
    printf("| %-*s ", col_w[c], muslimtify_prayer_name((MuslimtifyPrayerType)col[c]));
  printf("|\n");
  range_hrule(row_len - 2);

  for (size_t i = 0; i < count; i++) {
    printf("| %04d-%02d-%02d ", days[i].year, days[i].month, days[i].day);
    for (int c = 0; c < ncol; c++) {
      char time_str[16];
      format_cell(&days[i].prayers[col[c]].time, time_format, true, time_str, sizeof(time_str));
      printf("| %-*s ", col_w[c], time_str);
    }
    printf("|\n");
  }
  range_hrule(row_len - 2);
  print_day_marker_legend(any_next, any_prev);
}

// The three strings every next-prayer renderer prints. Returns false, writing
// nothing, when there is no upcoming prayer.
static bool next_prayer_strings(const MuslimtifyNext *next, int time_format, char *date_str,
                                size_t date_cap, char *time_str, size_t time_cap, char *remaining,
                                size_t rem_cap) {
  if (next->prayer == MUSLIMTIFY_PRAYER_COUNT)
    return false;
  snprintf(date_str, date_cap, "%04d-%02d-%02d", next->time.year, next->time.month, next->time.day);
  format_cell(&next->time, time_format, false, time_str, time_cap);
  snprintf(remaining, rem_cap, "%02d:%02d", next->minutes_until / 60, next->minutes_until % 60);
  return true;
}

void display_next_table(const MuslimtifyNext *next, int time_format) {
  char date_str[40], time_str[16], remaining[16];
  if (!next_prayer_strings(next, time_format, date_str, sizeof(date_str), time_str,
                           sizeof(time_str), remaining, sizeof(remaining))) {
    printf("No upcoming prayers enabled.\n");
    return;
  }

  printf("+------------+------------+----------+-----------+\n");
  printf("| %-10s | %-10s | %-8s | %-9s |\n", "Date", "Prayer", "Time", "Remaining");
  printf("+------------+------------+----------+-----------+\n");
  printf("| %-10s | %-10s | %-8s | %-9s |\n", date_str, muslimtify_prayer_name(next->prayer),
         time_str, remaining);
  printf("+------------+------------+----------+-----------+\n");
}

void display_next_plain(const MuslimtifyNext *next, int time_format) {
  char date_str[40], time_str[16], remaining[16];
  if (!next_prayer_strings(next, time_format, date_str, sizeof(date_str), time_str,
                           sizeof(time_str), remaining, sizeof(remaining))) {
    printf("No upcoming prayers enabled.\n");
    return;
  }

  char key[16];
  prayer_key((int)next->prayer, key, sizeof(key));
  printf("date=%s\n", date_str);
  printf("%s=%s\n", key, time_str);
  printf("remaining=%s\n", remaining);
}

void display_next_json(const MuslimtifyNext *next, int time_format) {
  char date_str[40], time_str[16], remaining[16];
  if (!next_prayer_strings(next, time_format, date_str, sizeof(date_str), time_str,
                           sizeof(time_str), remaining, sizeof(remaining))) {
    printf("{}\n");
    return;
  }

  char key[16];
  prayer_key((int)next->prayer, key, sizeof(key));
  printf("{\n");
  printf("  \"date\": \"%s\",\n", date_str);
  printf("  \"prayer\": \"%s\",\n", key);
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

void display_location(const MuslimtifyLocation *loc) {
  char coords[32], gmt[16];
  snprintf(coords, sizeof(coords), "%.4f,%.4f", loc->latitude, loc->longitude);
  snprintf(gmt, sizeof(gmt), "UTC%+.1f", loc->utc_offset);

  // "disabled" when 0, else "<n>s".
  char refresh[24];
  if (loc->refresh_interval <= 0)
    snprintf(refresh, sizeof(refresh), "disabled");
  else
    snprintf(refresh, sizeof(refresh), "%llds", loc->refresh_interval);

  const char *rows[][2] = {
      {"coordinates", coords},
      {"city", loc->city},
      {"country", loc->country},
      {"timezone", loc->timezone},
      {"gmt", gmt},
      {"refresh_interval", refresh},
      {"gps", loc->gps ? "enabled" : "disabled"},
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

void display_location_headless(const MuslimtifyLocation *loc) {
  printf("coordinates=%.4f,%.4f\n", loc->latitude, loc->longitude);
  printf("city=%s\n", loc->city);
  printf("country=%s\n", loc->country);
  printf("timezone=%s\n", loc->timezone);
  printf("gmt=UTC%+.1f\n", loc->utc_offset);
  printf("gps=%s\n", loc->gps ? "true" : "false");
  printf("refresh_interval=%lld\n", loc->refresh_interval);
}

void display_location_json(const MuslimtifyLocation *loc) {
  char coords[32], gmt[16];
  snprintf(coords, sizeof(coords), "%.4f,%.4f", loc->latitude, loc->longitude);
  snprintf(gmt, sizeof(gmt), "UTC%+.1f", loc->utc_offset);

  printf("{\n");
  printf("  \"coordinates\": ");
  json_str(coords);
  printf(",\n");
  printf("  \"city\": ");
  json_str(loc->city);
  printf(",\n");
  printf("  \"country\": ");
  json_str(loc->country);
  printf(",\n");
  printf("  \"timezone\": ");
  json_str(loc->timezone);
  printf(",\n");
  printf("  \"gmt\": ");
  json_str(gmt);
  printf(",\n");
  printf("  \"gps\": %s,\n", loc->gps ? "true" : "false");
  printf("  \"refresh_interval\": %lld\n", loc->refresh_interval);
  printf("}\n");
}

// "none" when there are no reminders, else the minutes joined by commas.
static void format_reminders(const MuslimtifyPrayerSettings *p, char *out, size_t cap) {
  if (p->reminder_count == 0) {
    snprintf(out, cap, "none");
    return;
  }
  size_t pos = 0;
  out[0] = '\0';
  for (int i = 0; i < p->reminder_count; i++) {
    int written = snprintf(out + pos, cap - pos, "%s%d", i > 0 ? "," : "", p->reminders[i]);
    if (written < 0 || (size_t)written >= cap - pos)
      break;
    pos += (size_t)written;
  }
}

void display_notification_settings(const MuslimtifyNotification *n) {
  printf("+---------+---------+---------------+-------+\n");
  printf("| %-7s | %-7s | %-13s | %-5s |\n", "Prayer", "Enabled", "Reminders", "Adhan");
  printf("+---------+---------+---------------+-------+\n");
  for (int i = 0; i < MUSLIMTIFY_PRAYER_COUNT; i++) {
    const MuslimtifyPrayerSettings *p = &n->prayers[i];
    char key[16];
    prayer_key(i, key, sizeof(key));
    char reminders[64];
    format_reminders(p, reminders, sizeof(reminders));
    printf("| %-7s | %-7s | %-13s | %-5s |\n", key, p->enabled ? "yes" : "no", reminders,
           p->adhan_enabled ? "on" : "off");
  }
  printf("+---------+---------+---------------+-------+\n");
  printf("sound: %s\n", muslimtify_sound_mode_key(n->sound_mode));
  printf("urgency: %s\n", muslimtify_urgency_key(n->urgency));
}

void display_notification_settings_headless(const MuslimtifyNotification *n) {
  printf("sound=%s\n", muslimtify_sound_mode_key(n->sound_mode));
  printf("urgency=%s\n", muslimtify_urgency_key(n->urgency));
  for (int i = 0; i < MUSLIMTIFY_PRAYER_COUNT; i++) {
    const MuslimtifyPrayerSettings *p = &n->prayers[i];
    char key[16];
    prayer_key(i, key, sizeof(key));
    char reminders[64];
    format_reminders(p, reminders, sizeof(reminders));
    printf("%s.enabled=%s\n", key, p->enabled ? "true" : "false");
    printf("%s.reminders=%s\n", key, reminders);
    printf("%s.adhan=%s\n", key, p->adhan_enabled ? "true" : "false");
  }
}

void display_notification_settings_json(const MuslimtifyNotification *n) {
  printf("{\n");
  printf("  \"sound\": ");
  json_str(muslimtify_sound_mode_key(n->sound_mode));
  printf(",\n");
  printf("  \"urgency\": ");
  json_str(muslimtify_urgency_key(n->urgency));
  printf(",\n");
  printf("  \"prayers\": {\n");
  for (int i = 0; i < MUSLIMTIFY_PRAYER_COUNT; i++) {
    const MuslimtifyPrayerSettings *p = &n->prayers[i];
    char key[16];
    prayer_key(i, key, sizeof(key));
    printf("    \"%s\": { \"enabled\": %s, \"reminders\": [", key, p->enabled ? "true" : "false");
    for (int j = 0; j < p->reminder_count; j++) {
      printf("%d", p->reminders[j]);
      if (j < p->reminder_count - 1)
        printf(", ");
    }
    /* Separator derives from the loop bound, not a literal, for the same reason as above. */
    printf("], \"adhan\": %s }%s\n", p->adhan_enabled ? "true" : "false",
           i + 1 < MUSLIMTIFY_PRAYER_COUNT ? "," : "");
  }
  printf("  }\n");
  printf("}\n");
}
