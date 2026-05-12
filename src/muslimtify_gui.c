#include <raylib.h>
#define MUSLIMTIFY_TOPBAR_IMPLEMENTATION
#define MUSLIMTIFY_NAVIGATION_IMPLEMENTATION
#define MUSLIMTIFY_DASHBOARD_CONTENT_IMPLEMENTATION
#include "ccompose.h"
#include "gui/components/dashboard_content.h"
#include "gui/components/navigation.h"
#include "gui/themes/colors.h"
#include "gui/themes/fonts.h"
#include <stdbool.h>

void App(void) {
  int LOGO_FONT = CC_LoadFont(FONT_MANROPE_BOLD, 48);
  int BOLD_FONT = CC_LoadFont(FONT_INTER_BOLD, 48);
  Texture2D currentTimeIcon = CC_LoadImage(IC_PRAYERS);
  Texture2D currentLocationIcon = CC_LoadImage(IC_CURRENT_LOCATION);
  Texture2D calculationProfileIcon = CC_LoadImage(IC_CALCULATION_PROFILE);

  NavigationLoadImage();

  while (CC_Running()) {
    CC_Begin();

    Row("Container", .layout = {.sizing = {.height = Grow(), .width = Grow()}}) {
      SideNavigation(LOGO_FONT, BOLD_FONT);
      DashboardContent(LOGO_FONT, BOLD_FONT, &currentTimeIcon, &currentLocationIcon,
                       &calculationProfileIcon);
    }

    CC_End();
  }

  NavigationUnloadImage();
}

int main(void) {
  CC_SetWindow(1200, 780, "Muslimtify");
  CC_SetBackground(COLOR_BACKGROUND);
  CC_Init();
  CC_LoadGlobalFont(FONT_INTER_REGULAR, 48);
  CC_LoadGlobalFontColor(COLOR_ON_SURFACE);

  App();

  CC_Shutdown();

  return 0;
}
