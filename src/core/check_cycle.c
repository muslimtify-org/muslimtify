#include "check_cycle.h"

#include "cache.h"
#include "config.h"
#include "location.h"
#include "notification.h"
#include "platform.h"
#include "prayertimes.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

int run_check_cycle(void) {
  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }

  if (location_prepare(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to detect location\n");
    return 1;
  }

  time_t now = time(NULL);
  struct tm tm_buf;
  platform_localtime(&now, &tm_buf);
  struct tm *tm_now = &tm_buf;

  int current_min = tm_now->tm_hour * 60 + tm_now->tm_min;
  char today[32];
  snprintf(today, sizeof(today), "%04d-%02d-%02d", tm_now->tm_year + 1900, tm_now->tm_mon + 1,
           tm_now->tm_mday);

  PrayerCache cache;
  bool cache_valid =
      (cache_load(&cache) == 0 && strcmp(cache.date, today) == 0 && cache.trigger_count > 0);

  if (!cache_valid) {
    MethodParams params = method_params_from_config(&cfg);
    struct PrayerTimes times =
        calculate_prayer_times(tm_now->tm_year + 1900, tm_now->tm_mon + 1, tm_now->tm_mday,
                               cfg.latitude, cfg.longitude, cfg.timezone_offset, &params);

    cache_build_triggers(&cache, &cfg, &times, current_min, today);
    cache_save(&cache);
  }

  bool notified = false;
  int i = 0;
  while (i < cache.trigger_count) {
    if (cache.triggers[i].minute == current_min) {
      if (!notified) {
        if (!notify_init_once("Muslimtify")) {
          fprintf(stderr, "Error: Failed to initialize notification system\n");
          return 1;
        }
        notified = true;
      }

      char time_str[16];
      format_time_hm(cache.triggers[i].prayer_time, time_str, sizeof(time_str));

      const char *sound_preset = NULL;
      if (cfg.notification_sound) {
        sound_preset = (cache.triggers[i].minutes_before == 0) ? cfg.notification_sound_alarm
                                                               : cfg.notification_sound_reminder;
      }
      notify_prayer(cache.triggers[i].prayer, time_str, cache.triggers[i].minutes_before,
                    cfg.notification_urgency, sound_preset);

      cache_remove_trigger(&cache, i);
    } else {
      i++;
    }
  }

  if (notified) {
    notify_cleanup();
    cache_save(&cache);
  }

  return 0;
}
