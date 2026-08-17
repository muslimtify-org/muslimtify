#include "fonts.h"
#include "themes.h"
#include "widgets.h"
#include <ccompose.h>

int main(void) {
  MuslimtifyThemes themes = get_themes(false);

  CC_SetWindow(960, 640, "Hello");
  CC_SetBackground(themes.surface);
  CC_Init();

  muslimtify_fonts_load();

  CC_InitFont(FONT_MANROPE_REGULAR, 48);

  while (CC_Running()) {
    CC_Begin();
    Column(
        "Root",
        .layout = {
            .sizing = {Grow(), Grow()},
        },
    ) {
      muslimtify_widget_header(&themes);
      Text("Hello, world!", .textColor = Color(255, 255, 255, 255), .fontSize = 32);
    }
    CC_End();
  }
  CC_Shutdown();
  return 0;
}
