#include "cli_internal.h"
#include "location.h"
#include "notification.h"
#include "platform.h"
#include "prayer_checker.h"
#include <stdio.h>
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
  const char *sound_preset = cfg.notification_sound ? cfg.notification_sound_alarm : NULL;
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

static const CommandEntry notification_commands[] = {
    {"test", notification_test},
};

int handle_notification(int argc, char **argv) {
  if (argc > 0) {
    const CommandEntry *sub =
        dispatch_lookup(notification_commands, DISPATCH_N(notification_commands), argv[0]);
    if (sub)
      return sub->handler(argc - 1, argv + 1);

    fprintf(stderr, "Error: Unknown notification subcommand '%s'\n", argv[0]);
  }

  fprintf(stderr, "Usage: muslimtify notification test\n");
  return 1;
}
