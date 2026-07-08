#include "cache.h"
#include "cli_internal.h"
#include "config.h"
#include "display.h"
#include "location.h"
#include "notification.h"
#include "platform.h"
#include "prayer_checker.h"
#include <stdbool.h>
#include <stdio.h>
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

static const char *const PRAYER_NAMES[] = {"fajr",    "sunrise", "dhuha", "dhuhr",
                                           "asr",     "maghrib", "isha"};

static void print_notification_help(void) {
  printf("Usage: muslimtify notification [--json | --headless]\n");
  printf("       muslimtify notification <enable|disable> [prayer|all]\n");
  printf("       muslimtify notification --urgency <normal|critical|low>\n");
  printf("       muslimtify notification --reminder [--all] <prayer> <minutes...>\n");
  printf("       muslimtify notification --adhan <enable|disable> <prayer> | set <path>\n");
  printf("       muslimtify notification --sound <adhan|default|off>\n");
  printf("  %-22s %s\n", "(default)", "Show current settings as a table");
  printf("  %-22s %s\n", "--json / --headless", "Machine-readable settings");
  printf("  %-22s %s\n", "-h, --help", "Show this help");
}

// Set enabled on one prayer or all seven.
static int notif_enable(int argc, char **argv, bool enable) {
  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }

  if (argc == 0 || strcmp(argv[0], "all") == 0) {
    PrayerConfig *all[] = {&cfg.fajr,    &cfg.sunrise, &cfg.dhuha, &cfg.dhuhr,
                           &cfg.asr,     &cfg.maghrib, &cfg.isha};
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
    fprintf(stderr, "Error: Unknown prayer '%s'\n", argv[0]);
    return 1;
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

int handle_notification(int argc, char **argv) {
  if (argc > 0) {
    if (strcmp(argv[0], "enable") == 0)
      return notif_enable(argc - 1, argv + 1, true);
    if (strcmp(argv[0], "disable") == 0)
      return notif_enable(argc - 1, argv + 1, false);
    if (strcmp(argv[0], "test") == 0)
      return notification_test(argc - 1, argv + 1);
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
