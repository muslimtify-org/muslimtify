#ifndef MUSLIMTIFY_GUI_PRAYER_H
#define MUSLIMTIFY_GUI_PRAYER_H

#include "prayer_helper.h"
#include "raylib.h"

#define GUI_PRAYER_COUNT 7

typedef struct {
  char name[16];
  char time[16];
  bool next;
  bool enabled;
} TodayPrayer;

typedef enum {
  PRAYER_ICON_FAJR,
  PRAYER_ICON_SUNRISE,
  PRAYER_ICON_DHUHR,
  PRAYER_ICON_ASR,
  PRAYER_ICON_MAGHRIB,
  PRAYER_ICON_ISHA,
} PrayerIcon;

typedef struct {
  bool isPast;
  bool isCurrent;
  bool isEnabled;
  char *title;
  char *time;
  PrayerIcon icon;
} DailyScheduleItem;

Texture2D *PrayerIconTexture(PrayerIcon id, bool active);

// Compute today's prayer snapshot into the module-static cache. Quiet: falls
// back to a default-config snapshot if config/location are unavailable.
void guiLoadPrayer(void);

// Borrow the cached snapshot. Valid until the next guiLoadPrayer() call.
const PrayerSnapshot *guiGetPrayer(void);

// Fill `out` with GUI_PRAYER_COUNT prayers (time-ordered) from `snap`.
void guiTodayPrayer(const PrayerSnapshot *snap, TodayPrayer out[GUI_PRAYER_COUNT]);

// Build the daily-schedule view items from today's time-ordered prayers.
// Fills `out` (sized GUI_PRAYER_COUNT) and returns the number written.
int guiDailySchedule(TodayPrayer today[GUI_PRAYER_COUNT], DailyScheduleItem out[GUI_PRAYER_COUNT]);

#endif // MUSLIMTIFY_GUI_PRAYER_H
