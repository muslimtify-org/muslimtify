#include "daily_schedule.h"

#include "app/assets.h"
#include "ccompose.h"
#include "themes/colors.h"
#include "themes/fonts.h"
#include "utils/gui_config.h"
#include "utils/gui_prayer.h"
#include <raylib.h>
#include <stdbool.h>

static CC_Scroll dailyScroll;

static void DailyScheduleCard(DailyScheduleItem item, int index) {
  Assets *a = appAssets();
  const CC_Color headerColor = item.isCurrent ? COLOR_PRIMARY : COLOR_SURFACE;
  const CC_Color timeColor = item.isCurrent ? COLOR_PRIMARY : COLOR_ON_BACKGROUND;
  Texture2D *icon = PrayerIconTexture(item.icon, item.isCurrent);

  Column(
      TextFormat("DailyScheduleCard_%d", index),
      .layout = {.sizing = {.width = Fixed(180), .height = Fit()}, .padding = PadAll(16)},
      .backgroundColor = item.isCurrent ? COLOR_BACKGROUND : COLOR_SURFACE_ALT,
      .cornerRadius = RadiusAll(16),
      .border = {.width = item.isCurrent ? BorderAll(1) : BorderAll(0), .color = COLOR_PRIMARY}) {

    if (item.isCurrent)
      Box(TextFormat("shadow_%d", index),
          .floating = {.attachTo = CC_ATTACH_TO_PARENT, .offset = {6, 8}, .zIndex = -3},
          .layout = {.sizing = SizingAll(Grow())}, .backgroundColor = Color(0, 0, 0, 20),
          .cornerRadius = RadiusAll(16), ) {}

    Row(TextFormat("DailyScheduleCardStatus_%d", index),
        .layout = {.sizing = {.width = Grow(), .height = Fit()}}) {
      Image(TextFormat("DailyScheduleCardStatusIcon_%d", index), ImgFit(icon),
            .layout = {.sizing = {.height = Fixed(24), .width = Fixed(24)}});
      if (item.isCurrent) {
        HSpacer();
        Image(TextFormat("DailyScheduleCardStatusNotify_%d", index), ImgFit(&a->nextNotification),
              .layout = {.sizing = {.height = Fixed(18), .width = Fixed(18)}});
      }

      if (item.isPast) {
        HSpacer();
        Box(TextFormat("DailyScheduleCardStatusBox_%d", index),
            .layout = {.sizing = {.height = Fixed(24)},
                       .padding = PadSymmetric(8, 4),
                       .childAlignment = {.x = AlignXEnd()}},
            .backgroundColor = COLOR_DISABLED_ALPHA10, .cornerRadius = RadiusAll(8)) {
          Text("PAST", .textColor = COLOR_DISABLED);
        }
      }
    }
    Spacer(.height = Fixed(16));
    Text(item.title, .textColor = headerColor,
         .fontId = item.isCurrent ? a->fontBold : a->fontNormal, .fontSize = FONT_SIZE_TITLE_LARGE);
    Text(item.time, .textColor = timeColor, .fontId = item.isCurrent ? a->fontBold : a->fontNormal,
         .fontSize = FONT_SIZE_HEADLINE_LARGE);
  }
}

void DailySchedule(void) {
  Assets *a = appAssets();
  CurrentDate date = getCurrentDate();

  TodayPrayer today[GUI_PRAYER_COUNT];
  guiTodayPrayer(guiGetPrayer(), today);

  DailyScheduleItem dailyScheduleItems[GUI_PRAYER_COUNT];
  int dailyScheduleItemsCount = guiDailySchedule(today, dailyScheduleItems);

  CC_ScrollUpdate(&dailyScroll, "DailyScheduleScroll", .horizontal = true, .drag = true,
                  .wheel = true);

  Column("DailyScheduleSection",
         .layout = {.sizing = {.width = Grow(), .height = Fit()}, .childGap = 16}) {
    Column("DailyScheduleSectionHeader",
           .layout = {.sizing = {.width = Grow()}, .childGap = 8, .padding = PadSymmetric(48, 0)}) {
      Text("Daily Schedule", .fontId = a->fontManrope, .fontSize = FONT_SIZE_HEADLINE_MEDIUM);
      Text(TextFormat("%s", date.date));
    }
    Row("DailyScheduleScroll", .clip = {.horizontal = true, .childOffset = dailyScroll.offset},
        .layout = {.sizing = {.width = Grow(), .height = Fit()}, .childGap = 16}) {
      Spacer(.width = Fixed(32));
      for (int i = 0; i < dailyScheduleItemsCount; i++) {
        if (dailyScheduleItems[i].isEnabled)
          DailyScheduleCard(dailyScheduleItems[i], i);
      }
      Spacer(.width = Fixed(32));
    }
  }
}
