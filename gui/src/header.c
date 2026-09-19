#include "fonts.h"
#include "version.h"
#include "widgets.h"
#include <assert.h>
#include <ccompose.h>

bool muslimtify_widget_header(MuslimtifyThemes *themes, GuiConfig *cfg) {
  assert(themes != NULL);
  assert(cfg != NULL);

  bool changed = false;

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

      Row("muslimtify_widget_header_logo",
          .layout = {
              .childAlignment = {.y = AlignYEnd()},
          }) {

        Text(
            "Muslimtify",
            .textColor = themes->on_surface,
            .fontId = FONT_MANROPE_BOLD,
            .fontSize = FONT_SIZE_TITLE_SMALL
        );
        Spacer(.width = Fixed(8));
        Text(MUSLIMTIFY_VERSION, .fontSize = FONT_SIZE_CAPTION_LARGE, .textColor = themes->border);
      }
      CC_HSpacer();
      Text("dark theme", .textColor = themes->on_surface, .fontSize = FONT_SIZE_BODY_MEDIUM);
      Spacer(.width = 8);
      if (muslimtify_widget_toggle(themes, &cfg->prefer_dark)) {
        gui_config_save(cfg);
        changed = true;
      }
    }
  }
  HDivider(.color = themes->on_surface);

  return changed;
}
