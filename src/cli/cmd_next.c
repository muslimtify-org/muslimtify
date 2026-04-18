#include "cli_internal.h"
#include "display.h"
#include "location.h"
#include "platform.h"
#include "prayer_checker.h"
#include <stdio.h>
#include <time.h>

static int next_name(int argc, char **argv) {
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
  printf("%s\n", prayer_get_name(next));
  return 0;
}

static int next_time(int argc, char **argv) {
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
  char time_str[16];
  format_time_hm(prayer_get_time(&times, next), time_str, sizeof(time_str));
  printf("%s\n", time_str);
  return 0;
}

static int next_remaining(int argc, char **argv) {
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
  int hours = minutes_until / 60;
  int mins = minutes_until % 60;
  if (hours > 0) {
    printf("%d:%02d\n", hours, mins);
  } else {
    printf("%dm\n", mins);
  }
  return 0;
}

static const CommandEntry next_commands[] = {
    {"name", next_name},
    {"time", next_time},
    {"remaining", next_remaining},
};

int handle_next(int argc, char **argv) {
  if (argc > 0) {
    const CommandEntry *sub = dispatch_lookup(next_commands, DISPATCH_N(next_commands), argv[0]);
    if (sub)
      return sub->handler(argc - 1, argv + 1);
  }

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

  display_next_prayer(&times, &cfg, tm_now);
  return 0;
}
