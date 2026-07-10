#include "cache.h"
#include "cli_internal.h"
#include "config.h"
#include "display.h"
#include "location.h"
#include "notification.h"
#include "platform.h"
#include "prayer_checker.h"
#include "string_util.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int notification_test(int argc, char **argv) {
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

  int minutes_until = 0;
  PrayerType next = prayer_get_next(&cfg, tm_now, &times, &minutes_until);
  if (next == PRAYER_NONE) {
    fprintf(stderr, "No upcoming prayers enabled.\n");
    return 1;
  }

  if (!notify_init_once("Muslimtify")) {
    fprintf(stderr, "Error: Failed to initialize notification system\n");
    return 1;
  }

  char time_str[16];
  format_time_hm(prayer_get_time(&times, next), time_str, sizeof(time_str));
  const char *sound_preset =
      strcmp(cfg.notification_sound, "off") != 0 ? cfg.notification_sound_alarm : NULL;
  if (argc > 0 && strcmp(argv[0], "--adhan") == 0) {
    // Use the next prayer's configured adhan; notify_adhan falls back to the
    // bundled adhan when the configured path is empty.
    const PrayerConfig *pcfg = prayer_get_config(&cfg, next);
    notify_adhan(prayer_get_name(next), time_str, pcfg ? pcfg->adhan : "");
  } else {
    notify_prayer(prayer_get_name(next), time_str, 0, cfg.notification_urgency, sound_preset);
  }

  notify_cleanup();
  printf("Sent test notification for %s at %s\n", prayer_get_name(next), time_str);
  return 0;
}

static void print_notification_help(void) {
  printf("\n");
  printf("Show or configure prayer notifications\n");
  printf("\n");
  printf("Usage: muslimtify notification [options]\n");
  printf("\n");
  printf("Commands:\n");
  printf("  %-25s %s\n", "enable|disable [prayer|all]", "Toggle prayer notifications");
  printf("  %-25s %s\n", "-h, --help", "Show this help");
  printf("\n");
  printf("Options:\n");
  printf("  %-25s %s\n", "--json", "Show settings as JSON");
  printf("  %-25s %s\n", "--headless", "Show settings as key=value");
  printf("  %-25s %s\n", "--urgency <level>", "Set urgency: normal|critical|low");
  printf("  %-25s %s\n", "--reminder [--all] <prayer> <minutes...>", "Set pre-prayer reminders");
  printf("  %-25s %s\n", "--adhan <enable|disable> <prayer>", "Toggle per-prayer adhan");
  printf("  %-25s %s\n", "--adhan set <path>", "Set adhan audio file");
  printf("  %-25s %s\n", "--sound <adhan|default|off>", "Set notification sound mode");
  printf("\n");
  printf("Examples:\n");
  printf("  %-25s %s\n", "muslimtify notification", "# Show current settings");
  printf("  %-25s %s\n", "muslimtify notification enable fajr", "# Enable Fajr notification");
  printf("  %-25s %s\n", "muslimtify notification --reminder fajr 30 15 5", "# Set reminders");
  printf("  %-25s %s\n", "muslimtify notification --sound adhan", "# Play adhan on notify");
}

// Set enabled on one prayer or all seven.
static int notif_enable(int argc, char **argv, bool enable) {
  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }

  if (argc == 0 || strcmp(argv[0], "all") == 0) {
    PrayerConfig *all[] = {&cfg.fajr, &cfg.sunrise, &cfg.dhuha, &cfg.dhuhr,
                           &cfg.asr,  &cfg.maghrib, &cfg.isha};
    for (int i = 0; i < 7; i++)
      all[i]->enabled = enable;
    if (config_save(&cfg) != 0) {
      fprintf(stderr, "Error: Failed to save config\n");
      return 1;
    }
    cache_invalidate();
    printf("All prayers %s\n", enable ? "enabled" : "disabled");
    return 0;
  }

  PrayerConfig *p = config_get_prayer(&cfg, argv[0]);
  if (!p) {
    return cli_unknown_prayer(argv[0]);
  }
  p->enabled = enable;
  if (config_save(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to save config\n");
    return 1;
  }
  cache_invalidate();
  printf("%s notifications %s\n", argv[0], enable ? "enabled" : "disabled");
  return 0;
}

static void print_reminder_help(void) {
  printf("Usage: muslimtify notification --reminder [--all] <prayer> <minutes...>\n");
  printf("       muslimtify notification --reminder <prayer> none   (clear)\n");
}

static int notif_urgency(int argc, char **argv) {
  if (cli_wants_help(argc, argv)) {
    printf("Usage: muslimtify notification --urgency <normal|critical|low>\n");
    return 0;
  }
  if (argc < 1) {
    fprintf(stderr, "Usage: muslimtify notification --urgency <normal|critical|low>\n");
    return 1;
  }
  if (strcmp(argv[0], "normal") != 0 && strcmp(argv[0], "critical") != 0 &&
      strcmp(argv[0], "low") != 0) {
    fprintf(stderr, "Error: urgency must be normal, critical, or low\n");
    return 1;
  }
  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }
  if (!copy_string(cfg.notification_urgency, sizeof(cfg.notification_urgency), argv[0])) {
    fprintf(stderr, "Error: urgency value too long\n");
    return 1;
  }
  if (config_save(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to save config\n");
    return 1;
  }
  printf("Urgency set to %s\n", argv[0]);
  return 0;
}

// Parse trailing space-separated minutes into `out` (0 < m <= 1440), up to
// MAX_REMINDERS. Returns the count, or -1 on a bad value. "none" -> 0.
static int parse_minute_args(int argc, char **argv, int *out) {
  if (argc == 1 && (strcmp(argv[0], "none") == 0 || strcmp(argv[0], "clear") == 0))
    return 0;
  int count = 0;
  for (int i = 0; i < argc && count < MAX_REMINDERS; i++) {
    char *end = NULL;
    long v = strtol(argv[i], &end, 10);
    if (end == argv[i] || *end != '\0' || v <= 0 || v > 1440)
      return -1;
    out[count++] = (int)v;
  }
  return count;
}

static int notif_reminder(int argc, char **argv) {
  if (cli_wants_help(argc, argv)) {
    print_reminder_help();
    return 0;
  }

  bool all = false;
  if (argc > 0 && strcmp(argv[0], "--all") == 0) {
    all = true;
    argc--;
    argv++;
  }

  if (all) {
    int mins[MAX_REMINDERS];
    int count = parse_minute_args(argc, argv, mins);
    if (count < 0) {
      fprintf(stderr, "Error: reminder minutes must be integers 1..1440\n");
      return 1;
    }
    Config cfg;
    if (config_load(&cfg) != 0) {
      fprintf(stderr, "Error: Failed to load config\n");
      return 1;
    }
    PrayerConfig *pc[] = {&cfg.fajr, &cfg.sunrise, &cfg.dhuha, &cfg.dhuhr,
                          &cfg.asr,  &cfg.maghrib, &cfg.isha};
    for (int i = 0; i < 7; i++) {
      pc[i]->reminder_count = count;
      for (int j = 0; j < count; j++)
        pc[i]->reminders[j] = mins[j];
    }
    if (config_save(&cfg) != 0) {
      fprintf(stderr, "Error: Failed to save config\n");
      return 1;
    }
    cache_invalidate();
    printf("Reminders updated for all prayers\n");
    return 0;
  }

  if (argc < 2) {
    print_reminder_help();
    return 1;
  }
  const char *prayer = argv[0];
  int mins[MAX_REMINDERS];
  int count = parse_minute_args(argc - 1, argv + 1, mins);
  if (count < 0) {
    fprintf(stderr, "Error: reminder minutes must be integers 1..1440\n");
    return 1;
  }
  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }
  PrayerConfig *p = config_get_prayer(&cfg, prayer);
  if (!p) {
    return cli_unknown_prayer(prayer);
  }
  p->reminder_count = count;
  for (int j = 0; j < count; j++)
    p->reminders[j] = mins[j];
  if (config_save(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to save config\n");
    return 1;
  }
  cache_invalidate();
  if (count == 0)
    printf("Reminders cleared for %s\n", prayer);
  else
    printf("Reminders updated for %s\n", prayer);
  return 0;
}

// Validate+canonicalize a user-supplied adhan path via the platform layer and
// map the result to a user-facing error. Returns 0 (and fills `out`) on
// success, 1 otherwise.
static int resolve_adhan_path(const char *in, char *out, size_t out_size) {
  switch (platform_resolve_regular_file(in, out, out_size)) {
  case PATH_FILE_OK:
    return 0;
  case PATH_FILE_NOT_FOUND:
    fprintf(stderr, "Error: adhan file not found: %s\n", in);
    return 1;
  case PATH_FILE_IS_SYMLINK:
    fprintf(stderr, "Error: adhan path must not be a symlink: %s\n", in);
    return 1;
  case PATH_FILE_NOT_REGULAR:
    fprintf(stderr, "Error: adhan path must be a regular file: %s\n", in);
    return 1;
  case PATH_FILE_NOT_READABLE:
    fprintf(stderr, "Error: adhan file is not readable: %s\n", in);
    return 1;
  case PATH_FILE_TOO_LONG:
    fprintf(stderr, "Error: adhan path is too long: %s\n", in);
    return 1;
  default:
    fprintf(stderr, "Error: cannot resolve adhan path: %s\n", in);
    return 1;
  }
}

static int notif_adhan(int argc, char **argv) {
  if (cli_wants_help(argc, argv)) {
    printf("Usage: muslimtify notification --adhan <enable|disable> <prayer> | set <path>\n");
    return 0;
  }
  if (argc == 1 && strcmp(argv[0], "stop") == 0) {
    if (notify_adhan_stop() == 0)
      printf("Adhan playback stopped\n");
    else
      printf("No adhan is currently playing\n");
    return 0;
  }
  if (argc < 2) {
    fprintf(stderr,
            "Usage: muslimtify notification --adhan <enable|disable> <prayer> | set <path>\n");
    return 1;
  }

  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }

  if (strcmp(argv[0], "set") == 0) {
    char adhan[MAX_ADHAN_PATH];
    if (resolve_adhan_path(argv[1], adhan, sizeof(adhan)) != 0)
      return 1;
    PrayerConfig *pc[] = {&cfg.fajr, &cfg.sunrise, &cfg.dhuha, &cfg.dhuhr,
                          &cfg.asr,  &cfg.maghrib, &cfg.isha};
    for (int i = 0; i < 7; i++)
      copy_string(pc[i]->adhan, sizeof(pc[i]->adhan), adhan);
    if (config_save(&cfg) != 0) {
      fprintf(stderr, "Error: Failed to save config\n");
      return 1;
    }
    cache_invalidate();
    printf("Adhan file set to %s\n", adhan);
    return 0;
  }

  bool enable;
  if (strcmp(argv[0], "enable") == 0)
    enable = true;
  else if (strcmp(argv[0], "disable") == 0)
    enable = false;
  else {
    fprintf(stderr, "Error: --adhan expects enable, disable, or set\n");
    return 1;
  }
  PrayerConfig *p = config_get_prayer(&cfg, argv[1]);
  if (!p) {
    return cli_unknown_prayer(argv[1]);
  }
  p->adhan_enabled = enable;
  if (config_save(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to save config\n");
    return 1;
  }
  cache_invalidate();
  printf("Adhan %s for %s\n", enable ? "enabled" : "disabled", argv[1]);
  return 0;
}

static int notif_sound(int argc, char **argv) {
  if (cli_wants_help(argc, argv)) {
    printf("Usage: muslimtify notification --sound <adhan|default|off>\n");
    return 0;
  }
  if (argc < 1 || (strcmp(argv[0], "adhan") != 0 && strcmp(argv[0], "default") != 0 &&
                   strcmp(argv[0], "off") != 0)) {
    fprintf(stderr, "Error: --sound expects adhan, default, or off\n");
    return 1;
  }
  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }
  if (!copy_string(cfg.notification_sound, sizeof(cfg.notification_sound), argv[0])) {
    fprintf(stderr, "Error: sound value too long\n");
    return 1;
  }
  if (config_save(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to save config\n");
    return 1;
  }
  printf("Sound mode set to %s\n", argv[0]);
  return 0;
}

int handle_notification(int argc, char **argv) {
  if (argc > 0) {
    if (strcmp(argv[0], "enable") == 0)
      return notif_enable(argc - 1, argv + 1, true);
    if (strcmp(argv[0], "disable") == 0)
      return notif_enable(argc - 1, argv + 1, false);
    if (strcmp(argv[0], "test") == 0)
      return notification_test(argc - 1, argv + 1);
    if (strcmp(argv[0], "--urgency") == 0)
      return notif_urgency(argc - 1, argv + 1);
    if (strcmp(argv[0], "--reminder") == 0)
      return notif_reminder(argc - 1, argv + 1);
    if (strcmp(argv[0], "--adhan") == 0)
      return notif_adhan(argc - 1, argv + 1);
    if (strcmp(argv[0], "--sound") == 0)
      return notif_sound(argc - 1, argv + 1);
  }

  if (cli_wants_help(argc, argv)) {
    print_notification_help();
    return 0;
  }

  // Default settings view: only --json / --headless are accepted here.
  for (int i = 0; i < argc; i++) {
    const char *a = argv[i];
    if (strcmp(a, "--json") == 0 || strcmp(a, "--headless") == 0)
      continue;
    fprintf(stderr, "Error: unknown notification argument '%s'\n", a);
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
  switch (mode) {
  case OUTPUT_JSON:
    display_notification_settings_json(&cfg);
    break;
  case OUTPUT_HEADLESS:
    display_notification_settings_headless(&cfg);
    break;
  default:
    display_notification_settings(&cfg);
    break;
  }
  return 0;
}
