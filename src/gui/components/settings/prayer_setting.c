#include "prayer_setting.h"
#include "ccompose.h"
#include "components/topbar.h"
#include "themes/colors.h"
#include "utils/gui_prayer.h"
#include <raylib.h>

void PrayerSettingItem(int index, const DailyScheduleItem item) {
  Column(TextFormat("PrayerSettingItem%d", index),
         .layout = {.sizing = {.width = Grow()}, .padding = PadAll(16)}) {
    Row(TextFormat("PrayerSettingItemHeader%d", index),
        .layout = {.sizing = {.width = Grow()}, .padding = PadAll(16)}) {
      Box(TextFormat("PrayerSettingItemHeaderIconBox%d", index), .layout = {.padding = PadAll(8)},
          .cornerRadius = RadiusAll(8), .backgroundColor = COLOR_SURFACE_VARIANT) {
        Image(TextFormat("PrayerSettingItemHeaderIcon%d", index),
              ImgFit(PrayerIconTexture(item.icon, true)),
              .layout = {.sizing = {.width = Fixed(18), .height = Fixed(18)}});
      }
    }
  }
}

void PrayerSetting(void) {
  TodayPrayer today[GUI_PRAYER_COUNT];
  guiTodayPrayer(guiGetPrayer(), today);

  DailyScheduleItem dailyScheduleItems[GUI_PRAYER_COUNT];
  int dailyScheduleItemsCount = guiDailySchedule(today, dailyScheduleItems);

  Column("PrayerSettings", .layout = {.sizing = {.width = Grow(), .height = Grow()}}) {
    TopBar("Prayer Settings");
    for (int i = 0; i < dailyScheduleItemsCount; i++) {
      PrayerSettingItem(i, (const DailyScheduleItem)dailyScheduleItems[i]);
    }
  }
}
