#ifndef MUSLIMTIFY_NAVIGATION_H
#define MUSLIMTIFY_NAVIGATION_H

#include <raylib.h>
#include <stdint.h>

void SideNavigation(uint16_t logoFontId, uint16_t boldFontId);

void NavigationLoadImage(void);

void NavigationUnloadImage(void);

#ifdef MUSLIMTIFY_NAVIGATION_IMPLEMENTATION

#include "../themes/assets.h"
#include "../themes/colors.h"
#include "ccompose.h"

typedef struct {
  char *chars;
  char *icon_path;
  char *icon_inactive_path;
  Texture2D icon;
  Texture2D icon_inactive;
} Navigation;

static bool isExpanded = true;
static Texture2D iconExpand;
static Texture2D iconCollapse;

static Navigation NAVIGATION_ITEMS[] = {
    {
        .chars = "Dashboard",
        .icon_path = IC_DASHBOARD,
        .icon_inactive_path = IC_DASHBOARD_INACTIVE,
    },
    {
        .chars = "Prayers",
        .icon_path = IC_PRAYERS,
        .icon_inactive_path = IC_PRAYERS_INACTIVE,
    },
    {
        .chars = "Location",
        .icon_path = IC_LOCATION,
        .icon_inactive_path = IC_LOCATION_INACTIVE,
    },
    {
        .chars = "Notification",
        .icon_path = IC_NOTIFICATION,
        .icon_inactive_path = IC_NOTIFICATION_INACTIVE,
    },
    {
        .chars = "About",
        .icon_path = IC_ABOUT,
        .icon_inactive_path = IC_ABOUT_INACTIVE,
    },
};

#define NAVIGATION_ITEMS_COUNT (sizeof(NAVIGATION_ITEMS) / sizeof(NAVIGATION_ITEMS[0]))

typedef void (*OnNavigationSelectedItem)(const size_t index);

static size_t selectedItem = 0;

static CC_Color NavigationButtonColor(size_t currentIndex) {
  return currentIndex == selectedItem ? COLOR_ON_PRIMARY : COLOR_SURFACE_VARIANT;
}

static void onNavigationItemSelected(const size_t index) {
  selectedItem = index;
}

static void NavigationItems(OnNavigationSelectedItem onSelected, int boldFontId, bool isMinimized) {
  Column("NavigationItems", .layout = {
                                .sizing = {.width = Grow(), .height = Grow()},
                                .padding = Pad(0, 0, 16, 16),
                            }) {
    for (size_t i = 0; i < NAVIGATION_ITEMS_COUNT; i++) {
      if (CC_Clicked(NAVIGATION_ITEMS[i].chars)) {
        onSelected(i);
      }

      Button(NAVIGATION_ITEMS[i].chars,
             .layout = {.sizing = {.width = Grow()},
                        .padding = PadAll(16),
                        .childAlignment = {.x = AlignStart(), .y = AlignMiddle()},
                        .childGap = 8},
             .backgroundColor = NavigationButtonColor(i), .cornerRadius = RadiusAll(8)) {

        bool selected = i == selectedItem;

        Texture2D *icon;
        if (selected)
          icon = &NAVIGATION_ITEMS[i].icon;
        else
          icon = &NAVIGATION_ITEMS[i].icon_inactive;

        Image(NAVIGATION_ITEMS[i].icon_path, ImgFit(icon),
              .layout = {.sizing = {.height = Fixed(20), .width = Fixed(20)}});

        if (!isMinimized)
          Text(NAVIGATION_ITEMS[i].chars, .textColor = COLOR_ON_SURFACE, .fontSize = 16,
               .fontId = selected ? boldFontId : 0);
      }
    }
  }
}

static void SideNavigationCollapsed(uint16_t boldFontId) {
  Column("Navigation",
         .layout = {.sizing = {.height = Grow(), .width = Fixed(80)}, .padding = PadAll(16)},
         .backgroundColor = COLOR_SURFACE_VARIANT) {

    VSpacer();
    if (CC_Clicked("Minimize")) {
      isExpanded = !isExpanded;
    }

    Button("Minimize", .layout = {.padding = PadAll(8)}, .cornerRadius = RadiusAll(999),
           .backgroundColor = CC_Hovered("Minimize") ? COLOR_ON_PRIMARY : COLOR_SURFACE_VARIANT) {
      Texture2D *icon;
      if (isExpanded)
        icon = &iconExpand;
      else
        icon = &iconCollapse;
      Image("ExpandedIcon", ImgFit(icon),
            .layout = {.sizing = {.height = Fixed(24), .width = Fixed(24)}});
    }
    NavigationItems(onNavigationItemSelected, boldFontId, true);
  }
}

static void SideNavigationExpanded(uint16_t logoFontId, uint16_t boldFontId) {
  Column("Navigation",
         .layout = {.sizing = {.height = Grow(), .width = Fixed(288)}, .padding = PadAll(16)},
         .backgroundColor = COLOR_SURFACE_VARIANT) {
    Row("LogoAndAction", .layout = {.sizing = {.width = Grow(), .height = Fit()}}) {

      Text("Muslimtify", .textColor = COLOR_PRIMARY, .fontSize = 24, .fontId = logoFontId);
      HSpacer();

      if (CC_Clicked("Minimize")) {
        isExpanded = !isExpanded;
      }

      Button("Minimize", .layout = {.padding = PadAll(8)}, .cornerRadius = RadiusAll(999),
             .backgroundColor = CC_Hovered("Minimize") ? COLOR_ON_PRIMARY : COLOR_SURFACE_VARIANT) {
        Texture2D *icon;
        if (isExpanded)
          icon = &iconExpand;
        else
          icon = &iconCollapse;
        Image("ExpandedIcon", ImgFit(icon),
              .layout = {.sizing = {.height = Fixed(24), .width = Fixed(24)}});
      }
    }
    Text("Sacred Precision", .textColor = COLOR_ON_SURFACE);
    NavigationItems(onNavigationItemSelected, boldFontId, false);
  }
}

void SideNavigation(uint16_t logoFontId, uint16_t boldFontId) {
  if (isExpanded)
    SideNavigationExpanded(logoFontId, boldFontId);
  else
    SideNavigationCollapsed(boldFontId);
}

void NavigationLoadImage(void) {
  iconExpand = CC_LoadImage(IC_EXPAND);
  iconCollapse = CC_LoadImage(IC_COLLAPSE);

  for (size_t i = 0; i < NAVIGATION_ITEMS_COUNT; i++) {
    NAVIGATION_ITEMS[i].icon = CC_LoadImage(NAVIGATION_ITEMS[i].icon_path);
    NAVIGATION_ITEMS[i].icon_inactive = CC_LoadImage(NAVIGATION_ITEMS[i].icon_inactive_path);
  }
}

void NavigationUnloadImage(void) {
  for (size_t i = 0; i < NAVIGATION_ITEMS_COUNT; i++) {
    CC_UnloadImage(NAVIGATION_ITEMS[i].icon);
    CC_UnloadImage(NAVIGATION_ITEMS[i].icon_inactive);
  }

  CC_UnloadImage(iconExpand);
  CC_UnloadImage(iconCollapse);
}

#endif

#endif
