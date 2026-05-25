#include "components/daily_schedule.h"

#include "app/assets.h"
#include "ccompose.h"
#include "themes/colors.h"
#include "themes/fonts.h"
#include <raylib.h>
#include <stdbool.h>

typedef enum {
  PRAYER_ICON_FAJR,
  PRAYER_ICON_DHUHR,
  PRAYER_ICON_ASR,
  PRAYER_ICON_MAGHRIB,
  PRAYER_ICON_ISHA,
} PrayerIcon;

typedef struct {
  bool isPast;
  bool isCurrent;
  char *title;
  char *time;
  PrayerIcon icon;
} DailyScheduleItem;

static DailyScheduleItem dailyScheduleItems[] = {
    {.isPast = true,
     .isCurrent = false,
     .title = "Fajr",
     .time = "05:00 AM",
     .icon = PRAYER_ICON_FAJR},
    {.isPast = false,
     .isCurrent = true,
     .title = "Dhuhr",
     .time = "12:00 PM",
     .icon = PRAYER_ICON_DHUHR},
    {.isPast = false,
     .isCurrent = false,
     .title = "Asr",
     .time = "01:00 PM",
     .icon = PRAYER_ICON_ASR},
    {.isPast = false,
     .isCurrent = false,
     .title = "Maghrib",
     .time = "03:00 PM",
     .icon = PRAYER_ICON_MAGHRIB},
    {.isPast = false,
     .isCurrent = false,
     .title = "Isha",
     .time = "08:00 PM",
     .icon = PRAYER_ICON_ISHA},
};

static int dailyScheduleItemsCount = sizeof(dailyScheduleItems) / sizeof(DailyScheduleItem);

static CC_Scroll dailyScroll;

static Texture2D *PrayerIconTexture(PrayerIcon id, bool active) {
  Assets *a = App_Assets();
  switch (id) {
  case PRAYER_ICON_FAJR:
    return active ? &a->fajrActive : &a->fajr;
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

static void DailyScheduleCard(DailyScheduleItem item, int index) {
  Assets *a = App_Assets();
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
  Assets *a = App_Assets();
  CC_ScrollUpdate(&dailyScroll, "DailyScheduleScroll", .horizontal = true, .drag = true,
                  .wheel = true);

  Column("DailyScheduleSection",
         .layout = {.sizing = {.width = Grow(), .height = Fit()}, .childGap = 16}) {
    Column("DailyScheduleSectionHeader",
           .layout = {.sizing = {.width = Grow()}, .childGap = 8, .padding = PadSymmetric(48, 0)}) {
      Text("Daily Schedule", .fontId = a->fontManrope, .fontSize = FONT_SIZE_HEADLINE_MEDIUM);
      Text("Tuesday, 24 October 2026");
    }
    Row("DailyScheduleScroll", .clip = {.horizontal = true, .childOffset = dailyScroll.offset},
        .layout = {.sizing = {.width = Grow(), .height = Fit()}, .childGap = 16}) {
      Spacer(.width = Fixed(32));
      for (int i = 0; i < dailyScheduleItemsCount; i++) {
        DailyScheduleCard(dailyScheduleItems[i], i);
      }
      Spacer(.width = Fixed(32));
    }
  }
}
