#include "components/topbar.h"

#include "app/assets.h"
#include "ccompose.h"
#include "helper/country_name.h"
#include "themes/colors.h"
#include "themes/fonts.h"
#include "utils/gui_config.h"
#include <raylib.h>

void TopBar(void) {
  Assets *a = appAssets();
  Config *cfg = guiGetConfig();

  Row("TopBar",
      .layout = {.sizing = {.height = Fixed(64), .width = Grow()},
                 .padding = PadSymmetric(42, 16),
                 .childGap = 8},
      .backgroundColor = COLOR_SURFACE_VARIANT) {
    Image("CurrentLocationLogo", ImgFit(&a->currentLocation),
          .layout = {.sizing = {.height = Fixed(18), .width = Fixed(18)}});
    Column("CurrentLocation", .layout = {.sizing = {.height = Grow(), .width = Grow()},
                                         .childAlignment = {.y = AlignYCenter()}}) {

      const char *location = NULL;
      if (cfg->city[0] != '\0' && cfg->country[0] != '\0') {
        location = TextFormat("%s, %s", cfg->city, getCountryName(cfg->country));
      } else if (cfg->city[0] != '\0') {
        location = cfg->city;
      } else if (cfg->country[0] != '\0') {
        location = getCountryName(cfg->country);
      }

      Text(location, .fontId = a->fontBold, .textColor = COLOR_PRIMARY,
           .fontSize = FONT_SIZE_TITLE_LARGE);
      Text(TextFormat("%.6f, %.6f", cfg->latitude, cfg->longitude),
           .textColor = COLOR_ON_BACKGROUND, .fontSize = FONT_SIZE_TITLE_MEDIUM,
           .wrapMode = CC_TEXT_WRAP_WORDS);
    }
  }
}
