#include "prayer_checker.h"
#include <math.h>
#include <stdio.h>

const char *prayer_get_name(PrayerType type) {
  switch (type) {
  case PRAYER_FAJR:
    return "Fajr";
  case PRAYER_SUNRISE:
    return "Sunrise";
  case PRAYER_DHUHA:
    return "Dhuha";
  case PRAYER_DHUHR:
    return "Dhuhr";
  case PRAYER_ASR:
    return "Asr";
  case PRAYER_MAGHRIB:
    return "Maghrib";
  case PRAYER_ISHA:
    return "Isha";
  default:
    return "Unknown";
  }
}

double prayer_get_time(const struct PrayerTimes *times, PrayerType type) {
  switch (type) {
  case PRAYER_FAJR:
    return times->fajr;
  case PRAYER_SUNRISE:
    return times->sunrise;
  case PRAYER_DHUHA:
    return times->dhuha;
  case PRAYER_DHUHR:
    return times->dhuhr;
  case PRAYER_ASR:
    return times->asr;
  case PRAYER_MAGHRIB:
    return times->maghrib;
  case PRAYER_ISHA:
    return times->isha;
  default:
    return 0.0;
  }
}

const PrayerConfig *prayer_get_config(const Config *cfg, PrayerType type) {
  switch (type) {
  case PRAYER_FAJR:
    return &cfg->fajr;
  case PRAYER_SUNRISE:
    return &cfg->sunrise;
  case PRAYER_DHUHA:
    return &cfg->dhuha;
  case PRAYER_DHUHR:
    return &cfg->dhuhr;
  case PRAYER_ASR:
    return &cfg->asr;
  case PRAYER_MAGHRIB:
    return &cfg->maghrib;
  case PRAYER_ISHA:
    return &cfg->isha;
  default:
    return NULL;
  }
}

bool prayer_is_enabled(const Config *cfg, PrayerType type) {
  const PrayerConfig *pcfg = prayer_get_config(cfg, type);
  return pcfg ? pcfg->enabled : false;
}

PrayerMatch prayer_check_current(const Config *cfg, struct tm *now, struct PrayerTimes *times) {
  PrayerMatch no_match = {.type = PRAYER_NONE, .minutes_before = -1, .prayer_time = 0.0};

  // Compare at integer-minute granularity to avoid double-firing
  // when a prayer time falls between two minutes (e.g., 19:24:30).
  int current_min = now->tm_hour * 60 + now->tm_min;

  // Array of all prayers to check
  PrayerType prayers[] = {PRAYER_FAJR, PRAYER_SUNRISE, PRAYER_DHUHA, PRAYER_DHUHR,
                          PRAYER_ASR,  PRAYER_MAGHRIB, PRAYER_ISHA};

  for (int i = 0; i < 7; i++) {
    PrayerType type = prayers[i];

    // Skip if disabled
    if (!prayer_is_enabled(cfg, type))
      continue;

    // Get prayer time and ceil to match display (Kemenag standard)
    double prayer_time = prayer_get_time(times, type);
    int prayer_min = (int)ceil(prayer_time * 60.0);
    const PrayerConfig *pcfg = prayer_get_config(cfg, type);

    // Check exact prayer time (same minute)
    if (prayer_min == current_min) {
      PrayerMatch match = {.type = type, .minutes_before = 0, .prayer_time = prayer_time};
      return match;
    }

    // Check reminders
    for (int j = 0; j < pcfg->reminder_count; j++) {
      int reminder_offset = pcfg->reminders[j];
      int reminder_min = prayer_min - reminder_offset;
      // Normalize for midnight crossover
      if (reminder_min < 0)
        reminder_min += 24 * 60;

      if (reminder_min == current_min) {
        PrayerMatch match = {
            .type = type, .minutes_before = reminder_offset, .prayer_time = prayer_time};
        return match;
      }
    }
  }

  return no_match;
}

PrayerType prayer_get_next(const Config *cfg, struct tm *now, struct PrayerTimes *times,
                           int *minutes_until) {
  double current_time = now->tm_hour + now->tm_min / 60.0;

  PrayerType prayers[] = {PRAYER_FAJR, PRAYER_SUNRISE, PRAYER_DHUHA, PRAYER_DHUHR,
                          PRAYER_ASR,  PRAYER_MAGHRIB, PRAYER_ISHA};

  PrayerType next_prayer = PRAYER_NONE;
  double min_diff = 24.0 * 60.0; // Max minutes in a day

  for (int i = 0; i < 7; i++) {
    PrayerType type = prayers[i];

    // Skip if disabled
    if (!prayer_is_enabled(cfg, type))
      continue;

    double prayer_time = prayer_get_time(times, type);
    double diff_hours = prayer_time - current_time;

    // Handle prayers that are tomorrow (negative diff means passed today)
    if (diff_hours < 0) {
      diff_hours += 24.0;
    }

    double diff_minutes = diff_hours * 60.0;

    if (diff_minutes < min_diff) {
      min_diff = diff_minutes;
      next_prayer = type;
    }
  }

  if (minutes_until) {
    *minutes_until = (int)min_diff;
  }

  return next_prayer;
}
