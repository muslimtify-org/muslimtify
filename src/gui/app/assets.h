#ifndef MUSLIMTIFY_APP_ASSETS_STRUCT_H
#define MUSLIMTIFY_APP_ASSETS_STRUCT_H

#include "ccompose.h"
#include <raylib.h>
#include <stdint.h>

typedef struct {
  int16_t fontManrope;
  int16_t fontBold;
  int16_t fontNormal;

  Texture2D currentTime;
  Texture2D currentLocation;
  Texture2D calculationProfile;
  Texture2D modifySettings;

  Texture2D fajr;
  Texture2D fajrActive;
  Texture2D sunrise;
  Texture2D sunriseActive;
  Texture2D dhuhr;
  Texture2D dhuhrActive;
  Texture2D asr;
  Texture2D asrActive;
  Texture2D maghrib;
  Texture2D maghribActive;
  Texture2D isha;
  Texture2D ishaActive;

  Texture2D dashboard;
  Texture2D dashboardInactive;
  Texture2D prayers;
  Texture2D prayersInactive;
  Texture2D location;
  Texture2D locationInactive;
  Texture2D notification;
  Texture2D notificationInactive;
  Texture2D about;
  Texture2D aboutInactive;
  Texture2D expand;
  Texture2D collapse;

  Texture2D nextNotification;
} Assets;

void Assets_Load(void);
void Assets_Unload(void);
Assets *App_Assets(void);

#endif // MUSLIMTIFY_APP_ASSETS_STRUCT_H
