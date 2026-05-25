#include "app/app.h"

#include "app/assets.h"
#include "ccompose.h"
#include "components/dashboard_content.h"
#include "components/navigation.h"
#include "themes/colors.h"
#include "themes/fonts.h"
#include "utils/gui_config.h"

static void AppFrame(void) {
  assetsLoad();
  guiLoadConfig();

  while (CC_Running()) {
    CC_Begin();

    Row("Container", .layout = {.sizing = {.height = Grow(), .width = Grow()}}) {
      SideNavigation();
      DashboardContent();
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
