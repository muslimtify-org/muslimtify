#include "widgets.h"
#include "fonts.h"
#include "version.h"
#include <assert.h>
#include <ccompose.h>

void muslimtify_widget_toggle(MuslimtifyThemes *themes, bool is_active) {}

void muslimtify_widget_header(MuslimtifyThemes *themes) {
  assert(themes != NULL);

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
    }
  }
  HDivider(.color = themes->outline);
}
