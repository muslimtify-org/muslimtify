#include "components/topbar.h"

#include "app/assets.h"
#include "ccompose.h"
#include "themes/colors.h"
#include "themes/fonts.h"

void TopBar(const char *title) {
  Assets *a = appAssets();

  Row("TopBar",
      .layout = {.sizing = {.height = Fixed(64), .width = Grow()},
                 .padding = PadSymmetric(42, 16),
                 .childAlignment = {.y = AlignYCenter(), .x = AlignXStart()},
                 .childGap = 8},
      .backgroundColor = COLOR_SURFACE_VARIANT) {
    Text(title, .fontId = a->fontBold, .textColor = COLOR_PRIMARY,
         .fontSize = FONT_SIZE_TITLE_LARGE);
  }
}
