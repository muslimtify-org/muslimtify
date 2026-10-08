#include "muslimtify_cycle.h"

#include "cache.h"
#include "check_cycle.h"
#include "config.h"
#include "lib/muslimtify_internal.h"
#include "location.h"
#include "notification.h"
#include "platform.h"
#include "prayer_checker.h"
#include "prayertimes.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

// The name shown as the sender of every notification.
#define APP_NAME "Muslimtify"

// How long a failed location refresh waits before the next attempt.
#define REFRESH_RETRY_SEC 600

// Epoch second before which a stale location is not refreshed again, set after
// a failed refresh. Process-wide on purpose: the cycle keeps no handle, and the
// daemon is one process.
static int64_t g_refresh_retry_at = 0;

static const MuslimtifyCycleHooks REAL_HOOKS = {
    location_detect, notify_init_once, notify_prayer, notify_adhan, notify_cleanup,
};

// Detect into *cfg through the library's own detection, which works on a copy
// and commits only on success, so a failed lookup leaves *cfg untouched.
static MuslimtifyError detect_into(Config *cfg, const MuslimtifyCycleHooks *hooks,
                                   MuslimtifyDetection *detection) {
  Muslimtify handle;
  handle.cfg = *cfg;
  MuslimtifyError err = muslimtify_detect_location_with(&handle, hooks->detect, detection);
  if (err == MUSLIMTIFY_OK)
    *cfg = handle.cfg;
  return err;
}

MuslimtifyError muslimtify_run_cycle_at(const MuslimtifyCycleHooks *hooks, const struct tm *now,
                                        time_t now_epoch, MuslimtifyCycle *out) {
  MuslimtifyCycle unused;
  MuslimtifyCycle *cycle = out ? out : &unused;
  memset(cycle, 0, sizeof(*cycle));
  if (!hooks || !now)
    return MUSLIMTIFY_ERR_INVALID_ARG;

  Config cfg;
  if (config_load(&cfg) != 0)
    return MUSLIMTIFY_ERR_CONFIG_LOAD;

  // The location is saved with config_save, never muslimtify_save. The latter
  // invalidates the trigger cache, and a rebuild keeps triggers up to
  // CATCHUP_MAX_MIN old, so a prayer announced a few minutes ago would be
  // announced again.
  if (config_location_needs_detect(&cfg)) {
    MuslimtifyError err = detect_into(&cfg, hooks, &cycle->detection);
    if (err != MUSLIMTIFY_OK)
      return err;
    if (config_save(&cfg) != 0)
      return MUSLIMTIFY_ERR_CONFIG_SAVE;
    cycle->detected = true;
  }

  // Periodic refresh: once the saved auto-detected location is older than the
  // configured interval, detect it again. Not fatal: a failed lookup keeps the
  // last known good location and the cycle carries on. A failure is not
  // retried every minute: the stored timestamp never advances, so without this
  // pause an offline machine would block on the lookup once a minute.
  if (location_is_stale(&cfg, (int64_t)now_epoch) && (int64_t)now_epoch >= g_refresh_retry_at) {
    if (detect_into(&cfg, hooks, &cycle->detection) != MUSLIMTIFY_OK) {
      cycle->refresh_failed = true;
      g_refresh_retry_at = (int64_t)now_epoch + REFRESH_RETRY_SEC;
    } else {
      cycle->refreshed = true;
      if (config_save(&cfg) != 0)
        cycle->refresh_failed = true;
    }
  }

  int current_min = now->tm_hour * 60 + now->tm_min;
  char today[32];
  snprintf(today, sizeof(today), "%04d-%02d-%02d", now->tm_year + 1900, now->tm_mon + 1,
           now->tm_mday);

  // Today's triggers were built for the location in force at the time. A
  // refreshed location means new times, so the day is rebuilt, minus whatever
  // already lies behind: the old location's version of those was handled by
  // earlier cycles, and announcing them again would be noise.
  PrayerCache cache = {0};
  bool cache_valid = !cycle->refreshed && cache_load(&cache) == 0 &&
                     cache_is_valid_for_today(cache.date, cache.trigger_count, today);

  bool cache_write_failed = false;
  if (!cache_valid) {
    struct PrayerTimes times =
        prayer_times_for_config(&cfg, now->tm_year + 1900, now->tm_mon + 1, now->tm_mday);

    cache_build_triggers(&cache, &cfg, &times, current_min, today);
    if (cycle->refreshed) {
      int j = 0;
      while (j < cache.trigger_count) {
        if (cache.triggers[j].minute < current_min)
          cache_remove_trigger(&cache, j);
        else
          j++;
      }
    }
    cache_write_failed = cache_save(&cache) != 0;
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
        // Returning here leaves the cache file as it was, so the trigger is
        // tried again on the next cycle.
        if (!hooks->notify_init(APP_NAME))
          return MUSLIMTIFY_ERR_NOTIFY_INIT;
        notified = true;
      }

      char time_str[16];
      format_time_cfg(&cfg, cache.triggers[i].prayer_time, time_str, sizeof(time_str));

      if (trigger_plays_adhan(cache.triggers[i].minute, cache.triggers[i].minutes_before,
                              cache.triggers[i].adhan_enabled, current_min)) {
        hooks->notify_adhan(cache.triggers[i].prayer, time_str, cache.triggers[i].adhan);
      } else {
        const char *sound_preset = NULL;
        if (strcmp(cfg.notification_sound, "off") != 0) {
          sound_preset = (cache.triggers[i].minutes_before == 0) ? cfg.notification_sound_alarm
                                                                 : cfg.notification_sound_reminder;
        }
        hooks->notify_prayer(cache.triggers[i].prayer, time_str, cache.triggers[i].minutes_before,
                             cfg.notification_urgency, sound_preset);
      }
      cycle->notifications++;
    }

    // FIRE or DROP: consume the trigger so it can't linger to the next cycle.
    cache_remove_trigger(&cache, i);
    dirty = true;
  }

  if (notified)
    hooks->notify_cleanup();

  // Another process may have changed the settings while this cycle ran, which
  // can take minutes when an adhan plays. It announces that by deleting the
  // cache file. Writing this cycle's copy back would resurrect the old
  // triggers, so a cache that is gone stays gone and the next cycle rebuilds.
  if (dirty && platform_file_exists(cache_get_path()))
    cache_write_failed = cache_save(&cache) != 0;

  // The notifications went out either way. Without a saved cache the same
  // triggers come back next minute, which the caller should hear about.
  return cache_write_failed ? MUSLIMTIFY_ERR_CACHE_SAVE : MUSLIMTIFY_OK;
}

MuslimtifyError muslimtify_run_cycle(MuslimtifyCycle *out) {
  time_t epoch = time(NULL);
  struct tm now;
  platform_localtime(&epoch, &now);
  return muslimtify_run_cycle_at(&REAL_HOOKS, &now, epoch, out);
}

MuslimtifyError muslimtify_notify_test_at(Muslimtify *mt, const MuslimtifyCycleHooks *hooks,
                                          const struct tm *now, bool adhan, MuslimtifyNext *sent) {
  if (!mt || !hooks || !now)
    return MUSLIMTIFY_ERR_INVALID_ARG;

  MuslimtifyNext next;
  MuslimtifyError err = muslimtify_next_at(mt, now, &next);
  if (err != MUSLIMTIFY_OK)
    return err;
  if (next.prayer == MUSLIMTIFY_PRAYER_COUNT)
    return MUSLIMTIFY_ERR_NO_UPCOMING_PRAYER;

  if (!hooks->notify_init(APP_NAME))
    return MUSLIMTIFY_ERR_NOTIFY_INIT;

  const Config *cfg = &mt->cfg;
  const char *name = muslimtify_prayer_name(next.prayer);
  char time_str[MUSLIMTIFY_TIME_STR_SIZE];
  muslimtify_format_time(&next.time, cfg->time_format, time_str, sizeof(time_str));

  if (adhan) {
    // The next prayer's configured adhan. The notifier falls back to the
    // bundled adhan when the configured path is empty.
    const PrayerConfig *pcfg = prayer_get_config(cfg, (PrayerType)next.prayer);
    hooks->notify_adhan(name, time_str, pcfg ? pcfg->adhan : "");
  } else {
    const char *sound_preset =
        strcmp(cfg->notification_sound, "off") != 0 ? cfg->notification_sound_alarm : NULL;
    hooks->notify_prayer(name, time_str, 0, cfg->notification_urgency, sound_preset);
  }

  hooks->notify_cleanup();
  if (sent)
    *sent = next;
  return MUSLIMTIFY_OK;
}

MuslimtifyError muslimtify_notify_test(Muslimtify *mt, bool adhan, MuslimtifyNext *sent) {
  time_t epoch = time(NULL);
  struct tm now;
  platform_localtime(&epoch, &now);
  return muslimtify_notify_test_at(mt, &REAL_HOOKS, &now, adhan, sent);
}

MuslimtifyError muslimtify_adhan_stop(void) {
  return notify_adhan_stop() == 0 ? MUSLIMTIFY_OK : MUSLIMTIFY_ERR_ADHAN_NOT_PLAYING;
}
