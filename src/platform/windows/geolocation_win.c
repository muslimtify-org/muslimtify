/*
 * Windows implementation of platform_get_location(), using the WinRT
 * Windows.Devices.Geolocation.Geolocator.
 *
 * Deliberately calls AllowFallbackToConsentlessPositions() and never
 * RequestAccessAsync(): the latter is documented to require a UI thread and a
 * foreground window, and muslimtify is a CLI plus a headless daemon, so calling
 * it would throw. Consentless mode yields a precise fix when the user permits
 * app location and a >=4km obfuscated fix when they do not, with no prompt in
 * either case. Both are well inside what prayer-time calculation needs, and
 * both beat the city-level accuracy of the ipinfo fallback.
 *
 * The Linux counterpart is src/platform/linux/gpsd_client.c. CMakeLists.txt
 * compiles exactly one of the two, so the symbol has a single definition.
 */

#include "platform.h"

#include <stdbool.h>

#include <windows.h>

#include <roapi.h>
#include <winstring.h>

#include <windows.devices.geolocation.h>
#include <windows.foundation.h>

/* Accept a cached fix up to a minute old — ample for prayer times, and it lets
 * repeated calls avoid re-waking the location stack. Give a cold lookup 5s, to
 * match GPSD_READ_DEADLINE_MS in the Linux client. Measured: a warm/cold call
 * completes in about 2s, and a disabled location service fails in about 50ms. */
#define GEO_MAX_AGE_MS 60000
#define GEO_TIMEOUT_MS 5000
#define GEO_POLL_MS 50
/* Wall-clock backstop in case the async operation never leaves Started. */
#define GEO_POLL_DEADLINE_MS 15000

/* The SDK declares these IIDs as EXTERN_C const IID but ships no import library
 * defining them for C, so they are defined here from the MIDL_INTERFACE uuids
 * in windows.devices.geolocation.h. A wrong value fails at runtime with
 * E_NOINTERFACE, not at compile time. */
const IID IID___x_ABI_CWindows_CDevices_CGeolocation_CIGeolocator = {
    0xa9c3bf62, 0x4524, 0x4989, {0x8a, 0xa9, 0xde, 0x01, 0x9d, 0x2e, 0x55, 0x1f}};
const IID IID___x_ABI_CWindows_CDevices_CGeolocation_CIGeolocator2 = {
    0xd1b42e6d, 0x8891, 0x43b4, {0xad, 0x36, 0x27, 0xc6, 0xfe, 0x9a, 0x97, 0xb1}};
const IID IID___x_ABI_CWindows_CDevices_CGeolocation_CIGeocoordinateWithPoint = {
    0xfeea0525, 0xd22c, 0x4d46, {0xb5, 0x27, 0x0b, 0x96, 0x06, 0x6f, 0xc7, 0xdb}};

typedef __x_ABI_CWindows_CDevices_CGeolocation_CIGeolocator GeoLocator;
typedef __x_ABI_CWindows_CDevices_CGeolocation_CIGeolocator2 GeoLocator2;
typedef __x_ABI_CWindows_CDevices_CGeolocation_CIGeoposition GeoPosition;
typedef __x_ABI_CWindows_CDevices_CGeolocation_CIGeocoordinate GeoCoordinate;
typedef __x_ABI_CWindows_CDevices_CGeolocation_CIGeocoordinateWithPoint GeoCoordWithPoint;
typedef __x_ABI_CWindows_CDevices_CGeolocation_CIGeopoint GeoPoint;
typedef __FIAsyncOperation_1_Windows__CDevices__CGeolocation__CGeoposition GeoAsyncOp;

GpsStatus platform_get_location(PlatformLatLng *latlong) {
  if (!latlong)
    return GPS_NO_FIX;

  GpsStatus result = GPS_UNAVAILABLE;
  bool owns_ro_init = false;

  GeoLocator *geo = NULL;
  GeoLocator2 *geo2 = NULL;
  IInspectable *inst = NULL;
  GeoAsyncOp *op = NULL;
  IAsyncInfo *info = NULL;
  GeoPosition *pos = NULL;
  GeoCoordinate *coord = NULL;
  GeoCoordWithPoint *coordpt = NULL;
  GeoPoint *point = NULL;
  HSTRING cls = NULL;
  HSTRING_HEADER cls_hdr;

  /* The toast path (notification_win.c) may already have initialized this
   * thread. S_FALSE means "already initialized, same mode" and is success;
   * RPC_E_CHANGED_MODE means another apartment type owns the thread, which is
   * still usable here. In both cases we must not call RoUninitialize. */
  HRESULT hr = RoInitialize(RO_INIT_MULTITHREADED);
  if (hr == S_OK)
    owns_ro_init = true;
  else if (hr != S_FALSE && hr != RPC_E_CHANGED_MODE)
    return GPS_UNAVAILABLE;

  if (FAILED(WindowsCreateStringReference(
          RuntimeClass_Windows_Devices_Geolocation_Geolocator,
          (UINT32)wcslen(RuntimeClass_Windows_Devices_Geolocation_Geolocator), &cls_hdr,
          &cls)))
    goto done;

  if (FAILED(RoActivateInstance(cls, &inst)))
    goto done;

  if (FAILED(inst->lpVtbl->QueryInterface(
          inst, &IID___x_ABI_CWindows_CDevices_CGeolocation_CIGeolocator, (void **)&geo)))
    goto done;
  if (FAILED(inst->lpVtbl->QueryInterface(
          inst, &IID___x_ABI_CWindows_CDevices_CGeolocation_CIGeolocator2, (void **)&geo2)))
    goto done;

  /* Past this point the location stack is reachable, so a failure is about
   * access or signal rather than the platform lacking support. */
  result = GPS_NO_FIX;

  if (FAILED(geo2->lpVtbl->AllowFallbackToConsentlessPositions(geo2)))
    goto done;
  if (FAILED(geo->lpVtbl->put_DesiredAccuracy(geo, PositionAccuracy_Default)))
    goto done;

  {
    __x_ABI_CWindows_CFoundation_CTimeSpan max_age, timeout;
    max_age.Duration = (INT64)GEO_MAX_AGE_MS * 10000; /* TimeSpan is 100ns ticks */
    timeout.Duration = (INT64)GEO_TIMEOUT_MS * 10000;
    if (FAILED(geo->lpVtbl->GetGeopositionAsyncWithAgeAndTimeout(geo, max_age, timeout, &op)))
      goto done;
  }

  if (FAILED(op->lpVtbl->QueryInterface(op, &IID_IAsyncInfo, (void **)&info)))
    goto done;

  /* Poll rather than implement an IAsyncOperationCompletedHandler vtbl in C.
   * Mirrors the bounded-deadline read loop in gpsd_client.c. */
  {
    DWORD start = GetTickCount();
    AsyncStatus st = Started;
    for (;;) {
      if (FAILED(info->lpVtbl->get_Status(info, &st)))
        goto done;
      if (st != Started)
        break;
      if (GetTickCount() - start > GEO_POLL_DEADLINE_MS)
        goto done; /* stays GPS_NO_FIX: timeout */
      Sleep(GEO_POLL_MS);
    }

    if (st != Completed) {
      HRESULT err = S_OK;
      if (SUCCEEDED(info->lpVtbl->get_ErrorCode(info, &err)) && err == E_ACCESSDENIED)
        result = GPS_NO_PERMISSION; /* system-wide location services are off */
      goto done;
    }
  }

  if (FAILED(op->lpVtbl->GetResults(op, &pos)))
    goto done;
  if (FAILED(pos->lpVtbl->get_Coordinate(pos, &coord)))
    goto done;
  if (FAILED(coord->lpVtbl->QueryInterface(
          coord, &IID___x_ABI_CWindows_CDevices_CGeolocation_CIGeocoordinateWithPoint,
          (void **)&coordpt)))
    goto done;
  if (FAILED(coordpt->lpVtbl->get_Point(coordpt, &point)))
    goto done;

  {
    __x_ABI_CWindows_CDevices_CGeolocation_CBasicGeoposition bg;
    if (FAILED(point->lpVtbl->get_Position(point, &bg)))
      goto done;
    /* Same range guard the core applies; a nonsense fix is treated as no fix. */
    if (bg.Latitude >= -90.0 && bg.Latitude <= 90.0 && bg.Longitude >= -180.0 &&
        bg.Longitude <= 180.0) {
      latlong->lat = bg.Latitude;
      latlong->lng = bg.Longitude;
      result = GPS_OK;
    }
  }

done:
  if (point)
    point->lpVtbl->Release(point);
  if (coordpt)
    coordpt->lpVtbl->Release(coordpt);
  if (coord)
    coord->lpVtbl->Release(coord);
  if (pos)
    pos->lpVtbl->Release(pos);
  if (info)
    info->lpVtbl->Release(info);
  if (op)
    op->lpVtbl->Release(op);
  if (geo2)
    geo2->lpVtbl->Release(geo2);
  if (geo)
    geo->lpVtbl->Release(geo);
  if (inst)
    inst->lpVtbl->Release(inst);
  if (owns_ro_init)
    RoUninitialize();
  return result;
}
