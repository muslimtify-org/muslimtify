#include "core/gui_config.h"
#include "muslimtify.h"
#include "themes/fonts.h"
#include "themes/themes.h"
#include "widgets/widgets.h"
#include <ccompose.h>

int main(void) {
  GuiConfig cfg;
  gui_config_load(&cfg);
  MuslimtifyThemes themes = get_themes(cfg.prefer_dark);
  Muslimtify *mt = NULL;

  CC_SetWindow(960, 640, "Hello");
  CC_SetBackground(themes.surface);
  CC_Init();

  muslimtify_fonts_load();
  muslimtify_open(&mt);
  cfg.mt = mt;

  CC_InitFont(FONT_MANROPE_REGULAR, 48);

  while (CC_Running()) {
    CC_Begin();
    Column(
        "Root",
        .layout = {
            .sizing = {Grow(), Grow()},
        },
    ) {
      if (muslimtify_widget_header(&themes, &cfg)) {
        themes = get_themes(cfg.prefer_dark);
        CC_SetBackground(themes.surface);
      }
      muslimtify_widget_content(&themes, &cfg);
    }
    CC_End();
  }

  muslimtify_close(mt);
  CC_Shutdown();
  return 0;
}
