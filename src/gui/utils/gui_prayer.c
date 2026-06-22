
#include "utils/gui_prayer.h"
#include "app/assets.h"
#include "config.h"
#include "platform.h"

#include <raylib.h>
#include <stdio.h>
#include <time.h>

static PrayerSnapshot g_guiPrayer;

Texture2D *PrayerIconTexture(PrayerIcon id, bool active) {
  Assets *a = appAssets();
  switch (id) {
  case PRAYER_ICON_FAJR:
    return active ? &a->fajrActive : &a->fajr;
  case PRAYER_ICON_SUNRISE:
    return active ? &a->sunriseActive : &a->sunrise;
  case PRAYER_ICON_DHUHR:
    return active ? &a->dhuhrActive : &a->dhuhr;
  case PRAYER_ICON_ASR:
    return active ? &a->asrActive : &a->asr;
  case PRAYER_ICON_MAGHRIB:
    return active ? &a->maghribActive : &a->maghrib;
  case PRAYER_ICON_ISHA:
    return active ? &a->ishaActive : &a->isha;
  }
  return &a->fajr;
}

const PrayerSnapshot *guiGetPrayer(void) {
  prayer_helper_refresh_minute_until(&g_guiPrayer);
  return &g_guiPrayer;
}

void guiLoadPrayer(void) {
  if (prayer_helper_today(&g_guiPrayer) != PRAYER_HELPER_OK) {
    // Mirror gui_config's default-fallback so the UI always has data to show.
    Config def = config_default();
    time_t now = time(NULL);
    struct tm tm_buf;
    platform_localtime(&now, &tm_buf);
    prayer_helper_compute(&def, &tm_buf, &g_guiPrayer);
  }
}

// The daily schedule maps each iconed prayer to its index in the time-ordered
// TodayPrayer array.
static const struct {
  int index;
  PrayerIcon icon;
} dailyScheduleMap[] = {
    {0, PRAYER_ICON_FAJR},  {1, PRAYER_ICON_SUNRISE}, {2, PRAYER_ICON_SUNRISE},
    {3, PRAYER_ICON_DHUHR}, {4, PRAYER_ICON_ASR},     {5, PRAYER_ICON_MAGHRIB},
    {6, PRAYER_ICON_ISHA},
};

static const int dailyScheduleItemsCount = sizeof(dailyScheduleMap) / sizeof(dailyScheduleMap[0]);

int guiDailySchedule(TodayPrayer today[GUI_PRAYER_COUNT], DailyScheduleItem out[GUI_PRAYER_COUNT]) {
  // First upcoming prayer in the full array; everything before it is past.
  int nextIdx = -1;
  for (int i = 0; i < GUI_PRAYER_COUNT; i++) {
    if (today[i].next) {
      nextIdx = i;
      break;
    }
  }

  for (int i = 0; i < dailyScheduleItemsCount; i++) {
    int idx = dailyScheduleMap[i].index;
    out[i] = (DailyScheduleItem){
        .title = today[idx].name,
        .isEnabled = today[idx].enabled,
        .time = today[idx].time,
        .icon = dailyScheduleMap[i].icon,
        .isCurrent = today[idx].next,
        .isPast = (nextIdx == -1) || (idx < nextIdx),
    };
  }

  return dailyScheduleItemsCount;
}

void guiTodayPrayer(const PrayerSnapshot *snap, TodayPrayer out[GUI_PRAYER_COUNT]) {
  const struct PrayerTimes *times = &snap->times;
  const Config *cfg = &snap->config;

  const char *prayer_names[] = {"Fajr", "Sunrise", "Dhuha", "Dhuhr", "Asr", "Maghrib", "Isha"};
  PrayerType types[] = {PRAYER_FAJR, PRAYER_SUNRISE, PRAYER_DHUHA, PRAYER_DHUHR,
                        PRAYER_ASR,  PRAYER_MAGHRIB, PRAYER_ISHA};

  for (int i = 0; i < GUI_PRAYER_COUNT; i++) {
    const PrayerConfig *pcfg = prayer_get_config(cfg, types[i]);

    double prayer_time = prayer_get_time(times, types[i]);
    format_time_hm(prayer_time, out[i].time, sizeof(out[i].time));

    snprintf(out[i].name, sizeof(out[i].name), "%s", prayer_names[i]);
    out[i].next = (snap->next != PRAYER_NONE && types[i] == snap->next);
    out[i].enabled = pcfg->enabled;
  }
}
