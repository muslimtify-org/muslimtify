#include "prayer_checker.h"
#include "prayertimes.h"
#include <math.h>
#include <stdio.h>

const char *prayer_get_name(PrayerType type) {
  switch (type) {
  case PRAYER_FAJR:
    return "Fajr";
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

void clock_format(int hour, int minute, int time_format, char *out, size_t cap) {
  // The modulo is redundant on an in-range value: it is what lets GCC's range
  // analysis bound the field width and drop -Wformat-truncation.
  unsigned mm = (unsigned)minute % 100u;
  if (time_format != 12) {
    snprintf(out, cap, "%02u:%02u", (unsigned)hour % 100u, mm);
    return;
  }
  const char *meridiem = hour < 12 ? "AM" : "PM";
  int hour12 = hour % 12;
  if (hour12 == 0)
    hour12 = 12;
  snprintf(out, cap, "%02u:%02u %s", (unsigned)hour12 % 100u, mm, meridiem);
}

void format_time_cfg(const Config *cfg, double hours, char *out, size_t cap) {
  char hm[6];
  format_time_hm(hours, hm, sizeof(hm));

  // "--:--" has no hour to convert.
  if (hm[0] == '-') {
    snprintf(out, cap, "%s", hm);
    return;
  }

  int hour = (hm[0] - '0') * 10 + (hm[1] - '0');
  int minute = (hm[3] - '0') * 10 + (hm[4] - '0');
  clock_format(hour, minute, cfg ? cfg->time_format : 24, out, cap);
}

double prayer_get_time(const struct PrayerTimes *times, PrayerType type) {
  switch (type) {
  case PRAYER_FAJR:
    return times->fajr;
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

NextPrayer prayer_next_from_days(const Config *cfg, const struct tm *now,
                                 const struct PrayerTimes days[3], const double offsets[3]) {
  NextPrayer best = {PRAYER_NONE, 0, 0, 0.0};
  int now_min = now->tm_hour * 60 + now->tm_min;

  for (int delta = -1; delta <= 1; delta++) {
    for (int i = 0; i < PRAYER_COUNT; i++) {
      PrayerType type = (PrayerType)i;
      if (!prayer_is_enabled(cfg, type))
        continue;

      double pt = prayer_get_time(&days[delta + 1], type);
      // A prayer the Sun never reaches at this latitude has no time to count down to.
      if (!isfinite(pt))
        continue;

      // Minutes from the start of now's date to the prayer, rounded up like
      // format_time_hm so the countdown and the printed time agree. A source
      // day on a different UTC offset from now's date has its wall clock
      // shifted, so the offset difference is taken off to count real minutes.
      // vibekit: now is assumed to be on the noon offset of its own date, so a
      // DST switch inside now's own date (for example 00:30 on the transition
      // day, before the clocks change) still leaves the countdown an hour out.
      // The upgrade path is to pass now as a time_t and compare it with each
      // prayer as a UTC instant, using the offset in force at that moment.
      long shift = lround((offsets[delta + 1] - offsets[1]) * 60.0);
      long until = (long)ceil((pt + 24.0 * delta) * 60.0) - now_min - shift;
      if (until < 0 || (best.type != PRAYER_NONE && until >= best.minutes_until))
        continue;
      best = (NextPrayer){type, (int)until, delta, pt};
    }
  }

  return best;
}

NextPrayer prayer_get_next(const Config *cfg, const struct tm *now,
                           const struct PrayerTimes *today) {
  long serial = mt_days_from_civil(now->tm_year + 1900, now->tm_mon + 1, now->tm_mday);
  struct PrayerTimes days[3];
  double offsets[3];
  days[1] = *today;
  for (int delta = -1; delta <= 1; delta++) {
    int y, m, d;
    mt_civil_from_days(serial + delta, &y, &m, &d);
    // The offset prayer_times_for_config computes that day's times with.
    offsets[delta + 1] = effective_tz_offset(cfg, y, m, d);
    if (delta != 0)
      days[delta + 1] = prayer_times_for_config(cfg, y, m, d);
  }
  return prayer_next_from_days(cfg, now, days, offsets);
}
