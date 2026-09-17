#include "widgets.h"
#include "fonts.h"
#include "version.h"
#include <assert.h>
#include <ccompose.h>

void muslimtify_widget_toggle(MuslimtifyThemes *themes, bool *is_active) {
  assert(themes != NULL);

  if (CC_Clicked("muslimtify_widget_toggle_circle"))
    *is_active = !*is_active;

  Box("muslimtify_widget_toggle_box",
      .backgroundColor = *is_active ? themes->primary : themes->disabled,
      .cornerRadius = RadiusAll(25),
      .layout = {
          .padding = PadAll(8),
          .childAlignment = {.y = AlignYCenter(), .x = *is_active ? AlignXEnd() : AlignXStart()},
          .sizing = {
              .width = Fixed(70),
          },
      }) {
    Box("muslimtify_widget_toggle_circle",
        .backgroundColor = *is_active ? themes->on_primary : themes->on_disabled,
        .cornerRadius = RadiusAll(25),
        .layout = {
            .sizing = {.width = Fixed(18), .height = Fixed(18)},
        }) {}
  }
}

void muslimtify_widget_header(MuslimtifyThemes *themes) {
  assert(themes != NULL);

  static bool is_dark = false;

  Column("muslimtify_widget_header_parent", .layout = {.sizing = {.width = Grow()}}) {
    Row("muslimtify_widget_header",
        .backgroundColor = themes->surface,
        .layout = {
            .childAlignment = {.y = AlignYCenter()},
            .padding = PadSymmetric(16, 8),
            .sizing = {
                .height = Fixed(56),
                .width = Grow(),
            },
        }) {
      Text(
          "Muslimtify",
          .textColor = themes->on_surface,
          .fontId = FONT_MANROPE_BOLD,
          .fontSize = FONT_SIZE_TITLE_SMALL
      );
      Spacer(.width = Fixed(8));
      Text(MUSLIMTIFY_VERSION, .fontSize = FONT_SIZE_CAPTION_LARGE, .textColor = themes->border);
      CC_HSpacer();
      muslimtify_widget_toggle(themes, &is_dark);
    }
  }
  HDivider(.color = themes->outline);
}
