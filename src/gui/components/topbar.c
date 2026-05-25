#include "components/topbar.h"

#include "app/assets.h"
#include "ccompose.h"
#include "themes/colors.h"
#include "themes/fonts.h"
#include <raylib.h>

void TopBar(void) {
  Assets *a = App_Assets();
  Row("TopBar",
      .layout = {.sizing = {.height = Fixed(64), .width = Grow()},
                 .padding = PadSymmetric(42, 16),
                 .childGap = 8},
      .backgroundColor = COLOR_SURFACE_VARIANT) {
    Image("CurrentLocationLogo", ImgFit(&a->currentLocation),
          .layout = {.sizing = {.height = Fixed(18), .width = Fixed(18)}});
    Column("CurrentLocation", .layout = {.sizing = {.height = Grow(), .width = Grow()},
                                         .childAlignment = {.y = AlignYCenter()}}) {
      Text("Jakarta, Indonesia", .fontId = a->fontBold, .textColor = COLOR_PRIMARY,
           .fontSize = FONT_SIZE_TITLE_LARGE);
      Text("6.2088 S, 106.8456 E", .textColor = COLOR_ON_BACKGROUND,
           .fontSize = FONT_SIZE_TITLE_MEDIUM, .wrapMode = CC_TEXT_WRAP_WORDS);
    }
  }
}
