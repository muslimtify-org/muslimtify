#include "muslimtify.h"
#include "themes/fonts.h"
#include "themes/themes.h"
#include "widgets/widgets.h"
#include <assert.h>
#include <ccompose.h>
#include <stdio.h>
#include <time.h>

// Filled once per wall-clock minute, the text lives until the frame is drawn.
typedef struct {
  time_t minute;
  bool ok;
  char title[32];
  char time[MUSLIMTIFY_TIME_STR_SIZE];
  char until_title[32];
  char until[32];
  MuslimtifyPrayerType prayer;
} NextPrayerText;

static const char *quote[] = {
    [MUSLIMTIFY_FAJR] = "A new dawn is near. Prepare your heart to begin the day with Allah.",

    [MUSLIMTIFY_DHUHR] = "Amid your daily activities, remember to make time for Allah.",

    [MUSLIMTIFY_ASR] = "As the afternoon approaches, prepare to renew your connection with Allah.",

    [MUSLIMTIFY_MAGHRIB] = "The sun will soon set. Prepare to end the day with gratitude.",

    [MUSLIMTIFY_ISHA] = "As night approaches, prepare your heart for a moment of peace and prayer.",
};

static const NextPrayerText *next_prayer_text(const Muslimtify *mt) {
  static NextPrayerText text = {.minute = -1};
  time_t now = time(NULL);
  time_t minute = now / 60;
  if (minute == text.minute) {
    return &text;
  }
  text.minute = minute;

  MuslimtifyNext next;
  text.ok = mt != NULL && muslimtify_next(mt, &next) == MUSLIMTIFY_OK &&
            next.prayer != MUSLIMTIFY_PRAYER_COUNT;
  if (text.ok) {
    text.prayer = next.prayer;
    snprintf(text.title, sizeof(text.title), "Next: %s", muslimtify_prayer_name(next.prayer));
    snprintf(text.time, sizeof(text.time), "%02d:%02d", next.time.hour, next.time.minute);
    // Rounded up, so the last minute before the prayer reads 00:01 rather than 00:00.
    long left = (long)((next.time.instant - now + 59) / 60);
    if (left < 0) {
      left = 0;
    }
    snprintf(
        text.until_title, sizeof(text.until_title), "%s in", muslimtify_prayer_name(next.prayer)
    );
    snprintf(text.until, sizeof(text.until), "%02ld:%02ld", left / 60, left % 60);
  } else {
    snprintf(text.title, sizeof(text.title), "Next: none");
    snprintf(text.time, sizeof(text.time), "--:--");
    snprintf(text.until_title, sizeof(text.until_title), "Next in");
    snprintf(text.until, sizeof(text.until), "--:--");
  }
  return &text;
}

static void muslimtify_prayer_time_info(MuslimtifyThemes *themes, const NextPrayerText *next) {
  Column(
      "muslimtify_widget_prayer_time_card",
      .layout = {
          .sizing = {.width = Grow(), .height = Grow()},
          .childGap = 8,
      },
  ) {
    Text(next->title, .fontSize = 24, .textColor = themes->on_surface);
    Text(next->time, .fontId = FONT_MANROPE_BOLD, .fontSize = 50, .textColor = themes->primary);
    Text(quote[next->prayer], .fontSize = 20, .textColor = themes->on_surface, );
  }
}

static void muslimtify_prayer_time_remaining(MuslimtifyThemes *themes, const NextPrayerText *next) {
  Box("muslimtify_widget_prayer_time_card_remaining_box",
      .layout =
          {
              .sizing = {.width = Fixed(170), .height = Fixed(100)},
              .padding = PadAll(8),
              .childAlignment = {.y = AlignYCenter(), .x = AlignXCenter()},
          },
      .cornerRadius = RadiusAll(12),
      .backgroundColor = themes->container_alt) {
    Text(next->until_title, .fontSize = 20, .textColor = themes->on_surface_alt);
    Text(next->until, .fontSize = 32, .fontId = FONT_MANROPE_BOLD, .textColor = themes->primary);
  }
}

bool muslimtify_widget_prayer_time_card(MuslimtifyThemes *themes, GuiConfig *cfg) {
  const NextPrayerText *next = next_prayer_text(cfg->mt);

  assert(themes != NULL);
  Column(
      "muslimtify_prayer_time_parent",
      .layout =
          {
              .sizing = {.width = Grow()},
              .padding = PadAll(24),
              .childGap = 16,

          },

      .backgroundColor = themes->container,
      .cornerRadius = RadiusAll(12),

  ) {
    Row("muslimtify_prayer_time_card_info",
        .layout = {
            .sizing = {.width = Grow(), .height = Grow()},
            .childGap = 16,
        }) {

      muslimtify_prayer_time_info(themes, next);
      muslimtify_prayer_time_remaining(themes, next);
    }
    Row("muslimtify_prayer_time_action_parent",
        .layout = {
            .sizing = {.width = Grow()},
            .childGap = 8,
        }) {
      Button(
          "muslimtify_prayer_time_adhan",
          .layout =
              {
                  .sizing = {.height = Fixed(34)},
                  .padding = PadSymmetric(20, 8),
              },
          .cornerRadius = RadiusAll(8),
          .backgroundColor = themes->primary,
      ) {
        Text(
            "Enable Adhan",
            .fontSize = 20,
            .fontId = FONT_MANROPE_BOLD,
            .textColor = themes->surface
        );
      }

      Button(
          "muslimtify_prayer_time_notification_settings",
          .layout =
              {
                  .sizing = {.height = Fixed(34)},
                  .padding = PadSymmetric(20, 8),
              },
          .cornerRadius = RadiusAll(8),
          .border = {.width = BorderAll(1), .color = themes->primary},
      ) {
        Text(
            "Notification Settings",
            .fontSize = 20,
            .fontId = FONT_MANROPE_BOLD,
            .textColor = themes->primary
        );
      }
    }
  }

  return true;
}
