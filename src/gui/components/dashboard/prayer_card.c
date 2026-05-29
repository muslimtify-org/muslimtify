#include "prayer_card.h"

#include "app/assets.h"
#include "ccompose.h"
#include "platform.h"
#include "prayer_checker.h"
#include "prayertimes.h"
#include "themes/colors.h"
#include "themes/fonts.h"
#include "utils/gui_prayer.h"
#include <raylib.h>
#include <time.h>

static void paint_space(CC_BoundingBox bb, void *user) {
  (void)user;
  Assets *a = appAssets();
  if (!a->spaceShaderReady) {
    return; // ccompose already drew the rounded teal base as fallback
  }

  float t = (float)GetTime();
  float res[2] = {(float)GetScreenWidth(), (float)GetScreenHeight()};
  float rect[4] = {bb.x, bb.y, bb.width, bb.height};
  float radius = 16.0f;
  float colTop[3] = {0.0f, 0.31f, 0.27f};  // COLOR_PRIMARY (0x004F45)
  float colDeep[3] = {0.0f, 0.13f, 0.12f}; // darker teal

  SetShaderValue(a->spaceShader, a->spaceLocTime, &t, SHADER_UNIFORM_FLOAT);
  SetShaderValue(a->spaceShader, a->spaceLocResolution, res, SHADER_UNIFORM_VEC2);
  SetShaderValue(a->spaceShader, a->spaceLocCardRect, rect, SHADER_UNIFORM_VEC4);
  SetShaderValue(a->spaceShader, a->spaceLocRadius, &radius, SHADER_UNIFORM_FLOAT);
  SetShaderValue(a->spaceShader, a->spaceLocColorTop, colTop, SHADER_UNIFORM_VEC3);
  SetShaderValue(a->spaceShader, a->spaceLocColorDeep, colDeep, SHADER_UNIFORM_VEC3);

  BeginShaderMode(a->spaceShader);
  DrawRectangleRec((Rectangle){bb.x, bb.y, bb.width, bb.height}, WHITE);
  EndShaderMode();
}

void PrayerCard(void) {
  Assets *a = appAssets();
  const PrayerSnapshot *snap = guiGetPrayer();
  time_t now = time(NULL);
  struct tm tm_buf;
  platform_localtime(&now, &tm_buf);

  const char *clock = TextFormat("%02d:%02d:%02d", tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec);

  const char *nextName = (snap->next == PRAYER_NONE) ? "No prayer" : prayer_get_name(snap->next);

  int hours = snap->minutes_until / 60;
  int mins = snap->minutes_until % 60;
  const char *remaining =
      (hours > 0) ? TextFormat("%02d:%02d", hours, mins) : TextFormat("%02d minutes", mins);

  char startStr[16] = "--:--";
  if (snap->next != PRAYER_NONE) {
    format_time_hm(prayer_get_time(&snap->times, snap->next), startStr, sizeof(startStr));
  }

  DrawColumn("PrayerCard", paint_space, NULL,
             .layout = {.sizing = {.height = Grow(), .width = Grow()},
                        .padding = PadAll(48),
                        .childGap = 16},
             .cornerRadius = RadiusAll(16), .backgroundColor = COLOR_PRIMARY) {
    Text("UPCOMING PRAYER", .textColor = COLOR_ON_PRIMARY, .fontSize = FONT_SIZE_TITLE_MEDIUM);
    Text(nextName, .textColor = COLOR_ON_PRIMARY, .fontSize = FONT_SIZE_DISPLAY_LARGE,
         .fontId = a->fontManrope);
    VSpacer();
    Row("PrayerCardButtons", .layout = {.sizing = {.width = Grow()},
                                        .childGap = 16,
                                        .childAlignment = {.y = CC_ALIGN_Y_BOTTOM}}) {
      Column("startAt", .layout = {.sizing = {.height = Fit()}}) {
        Text("Starts at", .textColor = COLOR_ON_PRIMARY, .fontSize = FONT_SIZE_TITLE_MEDIUM);
        Text(startStr, .textColor = COLOR_ON_PRIMARY, .fontSize = FONT_SIZE_HEADLINE_LARGE,
             .fontId = a->fontBold);
      }
      VDivider(.thickness = 1, .color = COLOR_ON_PRIMARY);
      Column("TimeRemaining", .layout = {.sizing = {.height = Fit()}}) {
        Text("Time Remaining", .textColor = COLOR_ON_PRIMARY, .fontSize = FONT_SIZE_TITLE_MEDIUM);
        Text(remaining, .textColor = COLOR_ON_PRIMARY, .fontSize = FONT_SIZE_HEADLINE_LARGE,
             .fontId = a->fontBold);
      }
      HSpacer();
      Row("CurrentTime",
          .layout = {.sizing = {.width = Fixed(160), .height = Fit()},
                     .childGap = 8,
                     .padding = PadSymmetric(24, 16),
                     .childAlignment = {.y = AlignYCenter(), .x = AlignXCenter()}},
          .backgroundColor = COLOR_SURFACE_VARIANT,

          .cornerRadius = RadiusAll(12)) {
        Image("CurrentTimeIcon", ImgFit(&a->currentTime),
              .layout = {.sizing = {Fixed(24), Fixed(24)}});
        Text(clock, .textColor = COLOR_PRIMARY, .fontSize = FONT_SIZE_HEADLINE_SMALL,
             .fontId = a->fontBold);
      }
    }
  }
}
