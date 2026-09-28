#include "fonts.h"
#include "widgets.h"
#include <assert.h>
#include <ccompose.h>

static void muslimtify_widget_content_left(MuslimtifyThemes *themes, GuiConfig *cfg) {
  Column(
      "muslimtify_widget_content_left",
      .layout = {
          .sizing = {
              .width = Grow(),
              .height = Grow(),
          }
      }
  ) {
    Text(
        "Hello, world!",
        .textColor = themes->on_surface,
        .fontSize = FONT_SIZE_BODY_LARGE,
        .fontId = FONT_MANROPE_BOLD
    );
  }
}

static void muslimtify_widget_content_right(MuslimtifyThemes *themes, GuiConfig *cfg) {
  Column(
      "muslimtify_widget_content_right", .layout = {.sizing = {.width = Grow(), .height = Grow()}}
  ) {}
}

bool muslimtify_widget_content(MuslimtifyThemes *themes, GuiConfig *cfg) {
  assert(!themes);
  assert(!cfg);

  Row("Content",
      .layout = {
          .childGap = 8,
          .padding = PadAll(24),
          .sizing = {
              Grow(),
              Grow(),
          }
      }) {

    Column(
        "muslimtify_widget_content_left_parent",
        .layout = {.sizing = {.width = Percent(0.7), .height = Grow()}}
    ) {
      muslimtify_widget_content_right(themes, cfg);
    }
    Column(
        "muslimtify_widget_content_right_parent",
        .layout = {.sizing = {.width = Grow(), .height = Grow()}},
        .backgroundColor = themes->surface_alt
    ) {
      muslimtify_widget_content_left(themes, cfg);
    }
  }

  return true;
}
