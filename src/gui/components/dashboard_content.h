#ifndef MUSLIMTIFY_DASHBOARD_CONTENT_H
#define MUSLIMTIFY_DASHBOARD_CONTENT_H

#include "ccompose.h"
#include <stdint.h>

void DashboardContent(int16_t manropeFontId, int16_t boldFontId, Texture2D *currentTimeIcon,
                      Texture2D *currentLocationIcon, Texture2D *calculationProfileIcon);

#ifdef MUSLIMTIFY_DASHBOARD_CONTENT_IMPLEMENTATION

#define MUSLIMTIFY_TOPBAR_IMPLEMENTATION
#include "../themes/colors.h"
#include "../themes/fonts.h"
#include "topbar.h"

static void CalculationMethodCard(int16_t boldFontId) {
  Column("CalculationMethodCard",
         .layout = {.sizing = {.width = Grow()}, .padding = Pad(16, 16, 32, 32), .childGap = 8},
         .backgroundColor = COLOR_SURFACE_ALT, .cornerRadius = RadiusAll(8)) {
    Text("CALCULATION METHOD", .fontSize = FONT_SIZE_TITLE_MEDIUM, .textColor = COLOR_ON_SURFACE);
    Text("Kemenag", .fontId = boldFontId, .fontSize = FONT_SIZE_HEADLINE_MEDIUM);
    Text("Ministry of Religious Affairs of the Republic of Indonesia");
  }
}

static void CalculationProfileCard(int16_t manropeFontId, int16_t boldFontId,
                                   Texture2D *calculationProfileIcon) {
  Column("CalculationProfileCard", .layout = {.padding = PadAll(16), .childGap = 16},
         .cornerRadius = RadiusAll(16), .backgroundColor = COLOR_ON_PRIMARY) {
    Row("CalculationProfileTitle", .layout = {.sizing = {.width = Grow()},
                                              .padding = PadAll(16),
                                              .childGap = 8,
                                              .childAlignment = {.y = AlignMiddle()}}) {
      Box("CalculationProfileIconBox",
          .layout =
              {
                  .padding = PadAll(8),
              },
          .backgroundColor = COLOR_SURFACE_VARIANT, .cornerRadius = RadiusAll(999)) {
        Image(
            "CalculationProfileIcon", ImgFit(calculationProfileIcon),
            .layout = {.sizing = {.height = Fixed(24), .width = Fixed(24)}, .padding = PadAll(8)});
      }
      Text("Calculation Profile", .fontId = manropeFontId, .fontSize = FONT_SIZE_TITLE_LARGE);
    }
    CalculationMethodCard(boldFontId);
  }
}

static void PrayerCard(int16_t manropeFontId, int16_t boldFontId, Texture2D *currentTimeIcon) {
  Column("PrayerCard",
         .layout = {.sizing = {.height = Fit(), .width = Fit()},
                    .padding = PadAll(48),
                    .childGap = 16},
         .cornerRadius = RadiusAll(16), .backgroundColor = COLOR_PRIMARY) {
    Text("UPCOMING PRAYER", .textColor = COLOR_ON_PRIMARY, .fontSize = FONT_SIZE_TITLE_MEDIUM);
    Text("Duhr", .textColor = COLOR_ON_PRIMARY, .fontSize = FONT_SIZE_DISPLAY_LARGE,
         .fontId = manropeFontId);
    Text("in 45 minutes", .textColor = COLOR_ON_PRIMARY, .fontSize = FONT_SIZE_TITLE_LARGE);
    VSpacer();
    Row("PrayerCardButtons", .layout = {.childGap = 16, .childAlignment = {.y = AlignMiddle()}}) {
      Column("startAt") {
        Text("Starts at", .textColor = COLOR_ON_PRIMARY, .fontSize = FONT_SIZE_TITLE_MEDIUM);
        Text("12:00 AM", .textColor = COLOR_ON_PRIMARY, .fontSize = FONT_SIZE_HEADLINE_LARGE,
             .fontId = boldFontId);
      }
      VDivider(.size = 48, .color = COLOR_ON_PRIMARY);
      Column("TimeRemaining") {
        Text("Time Remaining", .textColor = COLOR_ON_PRIMARY, .fontSize = FONT_SIZE_TITLE_MEDIUM);
        Text("00:43:00", .textColor = COLOR_ON_PRIMARY, .fontSize = FONT_SIZE_HEADLINE_SMALL,
             .fontId = boldFontId);
      }
      Row("CurrentTime",
          .layout = {.sizing = {.width = Fixed(150), .height = Fit()},
                     .childGap = 16,
                     .padding = Pad(24, 24, 16, 16),
                     .childAlignment = {.y = CC_ALIGN_Y_CENTER}},
          .backgroundColor = COLOR_SURFACE_VARIANT,

          .cornerRadius = RadiusAll(12)) {
        Image("CurrentTimeIcon", ImgFit(currentTimeIcon),
              .layout = {.sizing = {Fixed(30), Fixed(30)}});
        Text("11:49 AM", .textColor = COLOR_PRIMARY, .fontSize = FONT_SIZE_HEADLINE_LARGE,
             .fontId = boldFontId);
      }
    }
  }
}

void DashboardContent(int16_t manropeFontId, int16_t boldFontId, Texture2D *currentTimeIcon,
                      Texture2D *currentLocationIcon, Texture2D *calculationProfileIcon) {
  Column("Body", .layout = {.sizing = {.height = Grow(), .width = Grow()}}) {

    TopBar(boldFontId, currentLocationIcon);
    Column("ContentContainer",
           .layout = {.sizing = {.height = Grow(), .width = Grow()}, .padding = PadAll(48)}) {
      Row("Containter1", .layout = {.sizing = {.width = Grow()}, .childGap = 16}) {
        PrayerCard(manropeFontId, boldFontId, currentTimeIcon);
        CalculationProfileCard(manropeFontId, boldFontId, calculationProfileIcon);
      }
    }
  }
}
#endif

#endif
