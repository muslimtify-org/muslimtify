#include "app/app.h"

#include "app/assets.h"
#include "ccompose.h"
#include "components/dashboard/dashboard_content.h"
#include "components/navigation.h"
#include "themes/colors.h"
#include "themes/fonts.h"
#include "utils/gui_config.h"
#include "utils/gui_prayer.h"

void Content(int index) {
  switch (index) {
  case 0:
    DashboardContent();
    break;
  default:
    break;
  }
}

static void AppFrame(void) {
  assetsLoad();
  guiLoadConfig();
  guiLoadPrayer();
  int nav_index = 0;

  while (CC_Running()) {
    CC_Begin();

    Row("Container", .layout = {.sizing = {.height = Grow(), .width = Grow()}}) {
      SideNavigation(&nav_index);
      Content(nav_index);
    }

    CC_End();
  }

  assetsUnload();
}

void AppRun(void) {
  CC_SetWindow(1200, 780, "Muslimtify");
  CC_SetBackground(COLOR_BACKGROUND);
  CC_Init();
  CC_LoadGlobalFont(FONT_INTER_REGULAR, 48);
  CC_SetGlobalFontColor(COLOR_ON_SURFACE);

  AppFrame();

  CC_Shutdown();
}
