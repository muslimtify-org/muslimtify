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

TriggerAction trigger_catchup_action(int trigger_minute, int minutes_before, int current_minute) {
  if (trigger_minute > current_minute)
    return TRIGGER_KEEP;
  if (current_minute - trigger_minute > CATCHUP_MAX_MIN)
    return TRIGGER_DROP;

  // A reminder (minutes_before > 0) is only worth firing late while its
  // prayer, at trigger_minute + minutes_before, is still ahead of now. Its
  // whole point is to arrive before the prayer, so once the prayer has
  // passed the reminder would land beside the adhan it was meant to
  // precede. This never loses a real catch-up: a reminder's lateness
  // exceeds its own prayer's by exactly minutes_before, so whenever a late
  // reminder is still inside the catch-up window, its prayer's trigger is
  // inside it too, by a wider margin, and fires on its own.
  if (minutes_before > 0 && trigger_minute + minutes_before <= current_minute)
    return TRIGGER_DROP;

  return TRIGGER_FIRE;
}

bool cache_is_valid_for_today(const char *cache_date, int trigger_count, const char *today) {
  (void)trigger_count;
  return strcmp(cache_date, today) == 0;
}

bool trigger_plays_adhan(int trigger_minute, int minutes_before, bool adhan_enabled,
                         int current_minute) {
  return minutes_before == 0 && adhan_enabled && current_minute <= trigger_minute;
}

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

  // Periodic refresh: once the saved auto-detected location is older than the
  // configured interval, re-fetch it. Non-fatal — location_refresh keeps the
  // last known good coordinates on failure. Shared by the Linux daemon loop and
  // the Windows scheduled task (both call run_check_cycle).
  if (location_is_stale(&cfg, (int64_t)time(NULL))) {
    if (location_refresh(&cfg) != 0)
      fprintf(stderr, "check: location refresh failed, using cached location\n");
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
      (cache_load(&cache) == 0 && cache_is_valid_for_today(cache.date, cache.trigger_count, today));

  if (!cache_valid) {
    struct PrayerTimes times =
        prayer_times_for_config(&cfg, tm_now->tm_year + 1900, tm_now->tm_mon + 1, tm_now->tm_mday);

    cache_build_triggers(&cache, &cfg, &times, current_min, today);
    cache_save(&cache);
  }

  bool notified = false;
  bool dirty = false;
  int i = 0;
  while (i < cache.trigger_count) {
    TriggerAction action = trigger_catchup_action(cache.triggers[i].minute,
                                                  cache.triggers[i].minutes_before, current_min);

    if (action == TRIGGER_KEEP) {
      i++;
      continue;
    }

    if (action == TRIGGER_FIRE) {
      if (!notified) {
        if (!notify_init_once("Muslimtify")) {
          fprintf(stderr, "Error: Failed to initialize notification system\n");
          return 1;
        }
        notified = true;
      }

      char time_str[16];
      format_time_hm(cache.triggers[i].prayer_time, time_str, sizeof(time_str));

      if (trigger_plays_adhan(cache.triggers[i].minute, cache.triggers[i].minutes_before,
                              cache.triggers[i].adhan_enabled, current_min)) {
        notify_adhan(cache.triggers[i].prayer, time_str, cache.triggers[i].adhan);
      } else {
        const char *sound_preset = NULL;
        if (strcmp(cfg.notification_sound, "off") != 0) {
          sound_preset = (cache.triggers[i].minutes_before == 0) ? cfg.notification_sound_alarm
                                                                 : cfg.notification_sound_reminder;
        }
        notify_prayer(cache.triggers[i].prayer, time_str, cache.triggers[i].minutes_before,
                      cfg.notification_urgency, sound_preset);
      }
    }

    // FIRE or DROP: consume the trigger so it can't linger to the next cycle.
    cache_remove_trigger(&cache, i);
    dirty = true;
  }

  if (notified)
    notify_cleanup();
  if (dirty)
    cache_save(&cache);

  return 0;
}
