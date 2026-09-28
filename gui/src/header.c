#include "fonts.h"
#include "themes.h"
#include "version.h"
#include "widgets.h"
#include <assert.h>
#include <ccompose.h>

static void muslimtify_button_donation(MuslimtifyThemes *themes) {
  Button(
      "muslimtify_button_donation",
      .backgroundColor = themes->primary,
      .cornerRadius = RadiusAll(16),
      .layout = {
          .childAlignment = {.x = AlignXEnd()},
          .padding = PadSymmetric(16, 8),
      }
  ) {
    Text(
        "Donate",
        .textColor = themes->on_primary,
        .fontSize = FONT_SIZE_BODY_MEDIUM,
        .fontId = FONT_MANROPE_BOLD
    );
  }
}

bool muslimtify_widget_header(MuslimtifyThemes *themes, GuiConfig *cfg) {
  assert(themes != NULL);
  assert(cfg != NULL);

  bool changed = false;

  Column("muslimtify_widget_header_parent", .layout = {.sizing = {.width = Grow()}}) {
    Row("muslimtify_widget_header",
        .backgroundColor = themes->surface_alt,
        .layout = {
            .childAlignment = {.y = AlignYCenter()},
            .padding = PadSymmetric(16, 12),
            .sizing = {
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
        Text(
            MUSLIMTIFY_VERSION,
            .fontSize = FONT_SIZE_CAPTION_LARGE,
            .textColor = themes->on_surface_alt
        );
      }
      CC_HSpacer();
      Text("dark theme", .textColor = themes->on_surface, .fontSize = FONT_SIZE_BODY_MEDIUM);
      Spacer(.width = 8);
      if (muslimtify_widget_toggle(themes, &cfg->prefer_dark)) {
        gui_config_save(cfg);
        changed = true;
      }
      Spacer(.width = 8);
      muslimtify_button_donation(themes);
    }
  }

  return changed;
}
