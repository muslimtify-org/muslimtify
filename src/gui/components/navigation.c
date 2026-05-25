#include "components/navigation.h"

#include "app/assets.h"
#include "ccompose.h"
#include "themes/colors.h"
#include <raylib.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
  NAV_ICON_DASHBOARD,
  NAV_ICON_PRAYERS,
  NAV_ICON_LOCATION,
  NAV_ICON_NOTIFICATION,
  NAV_ICON_ABOUT,
} NavIcon;

typedef struct {
  char *chars;
  char *icon_id;
  NavIcon icon;
} Navigation;

static bool isExpanded = true;

static Navigation NAVIGATION_ITEMS[] = {
    {.chars = "Dashboard", .icon_id = "NavIconDashboard", .icon = NAV_ICON_DASHBOARD},
    {.chars = "Prayers", .icon_id = "NavIconPrayers", .icon = NAV_ICON_PRAYERS},
    {.chars = "Location", .icon_id = "NavIconLocation", .icon = NAV_ICON_LOCATION},
    {.chars = "Notification", .icon_id = "NavIconNotification", .icon = NAV_ICON_NOTIFICATION},
    {.chars = "About", .icon_id = "NavIconAbout", .icon = NAV_ICON_ABOUT},
};

#define NAVIGATION_ITEMS_COUNT (sizeof(NAVIGATION_ITEMS) / sizeof(NAVIGATION_ITEMS[0]))

typedef void (*OnNavigationSelectedItem)(const size_t index);

static size_t selectedItem = 0;

static Texture2D *NavIconTexture(NavIcon id, bool active) {
  Assets *a = App_Assets();
  switch (id) {
  case NAV_ICON_DASHBOARD:
    return active ? &a->dashboard : &a->dashboardInactive;
  case NAV_ICON_PRAYERS:
    return active ? &a->prayers : &a->prayersInactive;
  case NAV_ICON_LOCATION:
    return active ? &a->location : &a->locationInactive;
  case NAV_ICON_NOTIFICATION:
    return active ? &a->notification : &a->notificationInactive;
  case NAV_ICON_ABOUT:
    return active ? &a->about : &a->aboutInactive;
  }
  return &a->dashboard;
}

static CC_Color NavigationButtonColor(size_t currentIndex) {
  return currentIndex == selectedItem ? COLOR_PRIMARY : COLOR_SURFACE_VARIANT;
}

static CC_Color NavigationButtonTextColor(size_t currentIndex) {
  return currentIndex == selectedItem ? COLOR_ON_PRIMARY : COLOR_ON_SURFACE;
}

static void onNavigationItemSelected(const size_t index) {
  selectedItem = index;
}

static void NavigationItems(OnNavigationSelectedItem onSelected, bool isMinimized) {
  Assets *a = App_Assets();
  Column("NavigationItems", .layout = {
                                .sizing = {.width = Grow(), .height = Grow()},
                                .padding = PadSymmetric(0, 16),
                            }) {
    for (size_t i = 0; i < NAVIGATION_ITEMS_COUNT; i++) {
      if (CC_Clicked(NAVIGATION_ITEMS[i].chars)) {
        onSelected(i);
      }

      Button(NAVIGATION_ITEMS[i].chars,
             .layout = {.sizing = {.width = Grow()},
                        .padding = PadAll(16),
                        .childAlignment = {.x = AlignXStart(), .y = AlignYCenter()},
                        .childGap = 8},
             .backgroundColor = NavigationButtonColor(i), .cornerRadius = RadiusAll(8)) {

        bool selected = i == selectedItem;

        Texture2D *icon = NavIconTexture(NAVIGATION_ITEMS[i].icon, selected);

        Image(NAVIGATION_ITEMS[i].icon_id, ImgFit(icon),
              .layout = {.sizing = {.height = Fixed(20), .width = Fixed(20)}});

        if (!isMinimized)
          Text(NAVIGATION_ITEMS[i].chars, .textColor = NavigationButtonTextColor(i), .fontSize = 16,
               .fontId = selected ? a->fontBold : 0);
      }
    }
  }
}

static void SideNavigationCollapsed(void) {
  Assets *a = App_Assets();
  Column("Navigation",
         .layout = {.sizing = {.height = Grow(), .width = Fixed(80)}, .padding = PadAll(16)},
         .backgroundColor = COLOR_SURFACE_VARIANT) {

    VSpacer();
    if (CC_Clicked("Minimize")) {
      isExpanded = !isExpanded;
    }

    Button("Minimize", .layout = {.padding = PadAll(8)}, .cornerRadius = RadiusAll(999),
           .backgroundColor = CC_Hovered("Minimize") ? COLOR_ON_PRIMARY : COLOR_SURFACE_VARIANT) {
      Texture2D *icon = isExpanded ? &a->expand : &a->collapse;
      Image("ExpandedIcon", ImgFit(icon),
            .layout = {.sizing = {.height = Fixed(24), .width = Fixed(24)}});
    }
    NavigationItems(onNavigationItemSelected, true);
  }
}

static void SideNavigationExpanded(void) {
  Assets *a = App_Assets();
  Column("Navigation",
         .layout = {.sizing = {.height = Grow(), .width = Fixed(288)}, .padding = PadAll(16)},
         .backgroundColor = COLOR_SURFACE_VARIANT) {
    Row("LogoAndAction",
        .layout = {
            .sizing = {.width = Grow(), .height = Fit()},
            .childAlignment = {.x = AlignXStart(), .y = AlignYCenter()},
        }, ) {

      Text("Muslimtify", .textColor = COLOR_PRIMARY, .fontSize = 24, .fontId = a->fontManrope);
      HSpacer();
      if (CC_Clicked("Minimize")) {
        isExpanded = !isExpanded;
      }

      Button("Minimize", .layout = {.padding = PadAll(8)}, .cornerRadius = RadiusAll(999),
             .backgroundColor = CC_Hovered("Minimize") ? COLOR_ON_PRIMARY : COLOR_SURFACE_VARIANT) {
        Texture2D *icon = isExpanded ? &a->expand : &a->collapse;
        Image("ExpandedIcon", ImgFit(icon),
              .layout = {.sizing = {.height = Fixed(24), .width = Fixed(24)}});
      }
    }
    Text("Sacred Precision", .textColor = COLOR_ON_SURFACE);
    NavigationItems(onNavigationItemSelected, false);
  }
}

void SideNavigation(void) {
  if (isExpanded)
    SideNavigationExpanded();
  else
    SideNavigationCollapsed();
}
