#include "components/calculation_profile_card.h"

#include "app/assets.h"
#include "ccompose.h"
#include "helper/method_description.h"
#include "themes/colors.h"
#include "themes/fonts.h"
#include "utils/gui_config.h"
#include "utils/strfmt.h"
#include <raylib.h>

static void CalculationMethodCard(void) {
  Assets *a = appAssets();
  Config *cfg = guiGetConfig();
  MethodDescription method_desc = getMethodDescription(cfg->calculation_method);

  char *calculation_method = cfg->calculation_method;
  toTitleCase(calculation_method);

  char *madhab = cfg->madhab;
  toTitleCase(madhab);

  Column("CalculationMethodCard",
         .layout = {.sizing = {.width = Grow()}, .padding = PadSymmetric(16, 32), .childGap = 8},
         .backgroundColor = COLOR_SURFACE_ALT, .cornerRadius = RadiusAll(8)) {
    Text("CALCULATION METHOD", .fontSize = FONT_SIZE_TITLE_MEDIUM, .textColor = COLOR_ON_SURFACE);
    Text(TextFormat("%s - %s", calculation_method, madhab), .fontId = a->fontBold,
         .fontSize = FONT_SIZE_HEADLINE_MEDIUM);
    Text(method_desc.description);
  }
}

static void ModifySettingsCard(void) {
  Assets *a = appAssets();
  Row("ModifySettingsCard", .layout = {.sizing = {.width = Grow()}, .padding = PadAll(16)},
      .backgroundColor = COLOR_SURFACE_ALT, .cornerRadius = RadiusAll(8)) {
    Text("Modify Settings", .fontSize = FONT_SIZE_TITLE_MEDIUM, .fontId = a->fontBold,
         .textColor = COLOR_ON_SURFACE);
    HSpacer();
    Image("ModifySettingsIcons", ImgFit(&a->modifySettings),
          .layout = {.sizing = {.height = Fixed(18), .width = Fixed(18)}});
  }
}

void CalculationProfileCard(void) {
  Assets *a = appAssets();
  Column("CalculationProfileCard", .layout = {.padding = PadAll(16), .childGap = 16},
         .cornerRadius = RadiusAll(16), .backgroundColor = COLOR_ON_PRIMARY) {
    Row("CalculationProfileTitle", .layout = {.sizing = {.width = Grow()},
                                              .padding = PadAll(16),
                                              .childGap = 8,
                                              .childAlignment = {.y = AlignYCenter()}}) {
      Box("CalculationProfileIconBox",
          .layout =
              {
                  .padding = PadAll(8),
              },
          .backgroundColor = COLOR_SURFACE_VARIANT, .cornerRadius = RadiusAll(999)) {
        Image(
            "CalculationProfileIcon", ImgFit(&a->calculationProfile),
            .layout = {.sizing = {.height = Fixed(24), .width = Fixed(24)}, .padding = PadAll(8)});
      }
      Text("Calculation Profile", .fontId = a->fontManrope, .fontSize = FONT_SIZE_TITLE_LARGE);
    }
    CalculationMethodCard();
    ModifySettingsCard();
  }
}
