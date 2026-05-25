#define MUSLIMTIFY_APP_ASSETS_IMPLEMENTATION
#define MUSLIMTIFY_TOPBAR_IMPLEMENTATION
#define MUSLIMTIFY_NAVIGATION_IMPLEMENTATION
#define MUSLIMTIFY_DASHBOARD_CONTENT_IMPLEMENTATION
#include "ccompose.h"
#include "gui/components/dashboard_content.h"
#include "gui/components/navigation.h"
#include "gui/themes/app_assets.h"
#include "gui/themes/colors.h"
#include "gui/themes/fonts.h"
#include "app/assets.h"
#include <raylib.h>
#include <stdbool.h>

void App(void) {
  AppAssetsLoad();
  Assets_Load();

  while (CC_Running()) {
    CC_Begin();

    Row("Container", .layout = {.sizing = {.height = Grow(), .width = Grow()}}) {
      SideNavigation();
      DashboardContent();
    }

    CC_End();
  }

  Assets_Unload();
  AppAssetsUnload();
}

int main(void) {
  CC_SetWindow(1200, 780, "Muslimtify");
  CC_SetBackground(COLOR_BACKGROUND);
  CC_Init();
  CC_LoadGlobalFont(FONT_INTER_REGULAR, 48);
  CC_SetGlobalFontColor(COLOR_ON_SURFACE);

  App();

  CC_Shutdown();

  return 0;
}
