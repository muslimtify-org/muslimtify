#include "cli_internal.h"
#include "location.h"
#include "notification.h"
#include "platform.h"
#include "prayer_checker.h"
#include <stdio.h>
#include <time.h>

static int notification_test(int argc, char **argv) {
  (void)argc;
  (void)argv;

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

  MethodParams params = method_params_from_config(&cfg);
  struct PrayerTimes times =
      calculate_prayer_times(tm_now->tm_year + 1900, tm_now->tm_mon + 1, tm_now->tm_mday,
                             cfg.latitude, cfg.longitude, cfg.timezone_offset, &params);

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
  notify_prayer(prayer_get_name(next), time_str, 0, cfg.notification_urgency, sound_preset);
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
  }

  fprintf(stderr, "Usage: muslimtify notification test\n");
  return 1;
}
