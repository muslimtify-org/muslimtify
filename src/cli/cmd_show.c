#include "check_cycle.h"
#include "config.h"
#include "display.h"
#include "location.h"
#include "platform.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

int handle_show(int argc, char **argv) {
  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }

  if (ensure_location(&cfg) != 0) {
    return 1;
  }

  time_t now = time(NULL);
  struct tm tm_buf;
  platform_localtime(&now, &tm_buf);
  struct tm *tm_now = &tm_buf;

  MethodParams params = method_params_from_config(&cfg);
  struct PrayerTimes times =
      calculate_prayer_times(tm_now->tm_year + 1900, tm_now->tm_mon + 1, tm_now->tm_mday,
                             cfg.latitude, cfg.longitude, cfg.timezone_offset, &params);

  bool json_format = false;
  bool no_header = false;

  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "--no-header") == 0) {
      no_header = true;
    } else if (i + 1 < argc && strcmp(argv[i], "--format") == 0 &&
               strcmp(argv[i + 1], "json") == 0) {
      json_format = true;
      i++;
    }
  }

  if (json_format) {
    display_prayer_times_json(&times, &cfg, tm_now);
  } else if (no_header) {
    display_prayer_times_plain(&times, &cfg, tm_now);
  } else {
    display_prayer_times_table(&times, &cfg, tm_now);
  }

  return 0;
}

int handle_check(int argc, char **argv) {
  (void)argc;
  (void)argv;
  return run_check_cycle();
}
