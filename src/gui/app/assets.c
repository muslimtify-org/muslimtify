#include "app/assets.h"

#include "themes/asset_paths.h"
#include "themes/fonts.h"

static Assets g_assets;

Assets *App_Assets(void) {
  return &g_assets;
}

void Assets_Load(void) {
  g_assets.fontManrope = (int16_t)CC_LoadFont(FONT_MANROPE_BOLD, 48);
  g_assets.fontBold = (int16_t)CC_LoadFont(FONT_INTER_BOLD, 48);
  g_assets.fontNormal = (int16_t)CC_LoadFont(FONT_INTER_REGULAR, 48);

  g_assets.currentTime = CC_LoadImage(IC_PRAYERS_TIME);
  g_assets.currentLocation = CC_LoadImage(IC_CURRENT_LOCATION);
  g_assets.calculationProfile = CC_LoadImage(IC_CALCULATION_PROFILE);
  g_assets.modifySettings = CC_LoadImage(IC_NEXT);

  g_assets.fajr = CC_LoadImage(IC_FAJR);
  g_assets.fajrActive = CC_LoadImage(IC_FAJR_ACTIVE);
  g_assets.sunrise = CC_LoadImage(IC_SUNRISE);
  g_assets.sunriseActive = CC_LoadImage(IC_SUNRISE_ACTIVE);
  g_assets.dhuhr = CC_LoadImage(IC_DHUHR);
  g_assets.dhuhrActive = CC_LoadImage(IC_DHUHR_ACTIVE);
  g_assets.asr = CC_LoadImage(IC_ASR);
  g_assets.asrActive = CC_LoadImage(IC_ASR_ACTIVE);
  g_assets.maghrib = CC_LoadImage(IC_MAGHRIB);
  g_assets.maghribActive = CC_LoadImage(IC_MAGHRIB_ACTIVE);
  g_assets.isha = CC_LoadImage(IC_ISHA);
  g_assets.ishaActive = CC_LoadImage(IC_ISHA_ACTIVE);

  g_assets.dashboard = CC_LoadImage(IC_DASHBOARD);
  g_assets.dashboardInactive = CC_LoadImage(IC_DASHBOARD_INACTIVE);
  g_assets.prayers = CC_LoadImage(IC_PRAYERS);
  g_assets.prayersInactive = CC_LoadImage(IC_PRAYERS_INACTIVE);
  g_assets.location = CC_LoadImage(IC_LOCATION);
  g_assets.locationInactive = CC_LoadImage(IC_LOCATION_INACTIVE);
  g_assets.notification = CC_LoadImage(IC_NOTIFICATION);
  g_assets.notificationInactive = CC_LoadImage(IC_NOTIFICATION_INACTIVE);
  g_assets.about = CC_LoadImage(IC_ABOUT);
  g_assets.aboutInactive = CC_LoadImage(IC_ABOUT_INACTIVE);
  g_assets.expand = CC_LoadImage(IC_EXPAND);
  g_assets.collapse = CC_LoadImage(IC_COLLAPSE);

  g_assets.nextNotification = CC_LoadImage(IC_NEXT_NOTIFICATION);
}

void Assets_Unload(void) {
  CC_UnloadImage(g_assets.currentTime);
  CC_UnloadImage(g_assets.currentLocation);
  CC_UnloadImage(g_assets.calculationProfile);
  CC_UnloadImage(g_assets.modifySettings);

  CC_UnloadImage(g_assets.fajr);
  CC_UnloadImage(g_assets.fajrActive);
  CC_UnloadImage(g_assets.sunrise);
  CC_UnloadImage(g_assets.sunriseActive);
  CC_UnloadImage(g_assets.dhuhr);
  CC_UnloadImage(g_assets.dhuhrActive);
  CC_UnloadImage(g_assets.asr);
  CC_UnloadImage(g_assets.asrActive);
  CC_UnloadImage(g_assets.maghrib);
  CC_UnloadImage(g_assets.maghribActive);
  CC_UnloadImage(g_assets.isha);
  CC_UnloadImage(g_assets.ishaActive);

  CC_UnloadImage(g_assets.dashboard);
  CC_UnloadImage(g_assets.dashboardInactive);
  CC_UnloadImage(g_assets.prayers);
  CC_UnloadImage(g_assets.prayersInactive);
  CC_UnloadImage(g_assets.location);
  CC_UnloadImage(g_assets.locationInactive);
  CC_UnloadImage(g_assets.notification);
  CC_UnloadImage(g_assets.notificationInactive);
  CC_UnloadImage(g_assets.about);
  CC_UnloadImage(g_assets.aboutInactive);
  CC_UnloadImage(g_assets.expand);
  CC_UnloadImage(g_assets.collapse);
  CC_UnloadImage(g_assets.nextNotification);
}
