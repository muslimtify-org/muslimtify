#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "platform_native.h"

#include <stdbool.h>
#include <unistd.h>

#ifdef HAVE_LIBGPS
#include <gps.h>
#include <math.h>
#endif

/* Linux-specific: resolve the running executable via /proc/self/exe. */
ssize_t platform_native_exe_path(char *buf, size_t cap) {
  if (!buf || cap == 0)
    return -1;

  ssize_t len = readlink("/proc/self/exe", buf, cap - 1);
  if (len > 0) {
    buf[len] = '\0';
    return len;
  }

  buf[0] = '\0';
  return -1;
}

#ifdef HAVE_LIBGPS

#define GPSD_HOST "localhost"
#define GPSD_PORT "2947"
#define GPSD_WAIT_TIMEOUT 5000000 /* 5s in microseconds */

/* Linux-specific: get the current location (lat/lng) of the device. */
GpsStatus platform_native_get_location(PlatformLatLng *latlong) {
  if (!latlong)
    return GPS_NO_FIX;

  struct gps_data_t gps_data;

  if (gps_open(GPSD_HOST, GPSD_PORT, &gps_data) != 0)
    return GPS_NO_DAEMON;

  (void)gps_stream(&gps_data, WATCH_ENABLE | WATCH_JSON, NULL);

  bool saw_device = false;
  GpsStatus result = GPS_NO_FIX;
  // exit if no data seen in 5s (5000000 micro seconds)
  while (gps_waiting(&gps_data, GPSD_WAIT_TIMEOUT)) {
    if (gps_read(&gps_data, NULL, 0) < 0)
      break;

    // Track whether gpsd knows about any GPS hardware, so a timeout without a
    // fix can be reported as "no device" vs merely "no signal yet".
    if ((gps_data.set & DEVICELIST_SET) && gps_data.devices.ndevices > 0)
      saw_device = true;
    if (gps_data.set & DEVICE_SET)
      saw_device = true;

    // Nothing to see here, continue
    if (MODE_SET != (MODE_SET & gps_data.set))
      continue;

    if (isfinite(gps_data.fix.latitude) && isfinite(gps_data.fix.longitude)) {
      *latlong = (PlatformLatLng){
          .lat = gps_data.fix.latitude,
          .lng = gps_data.fix.longitude,
      };
      result = GPS_OK;
      break;
    }
  }

  (void)gps_stream(&gps_data, WATCH_DISABLE, NULL);
  (void)gps_close(&gps_data);

  if (result != GPS_OK && !saw_device)
    result = GPS_NO_DEVICE;
  return result;
}

#else /* !HAVE_LIBGPS */

/* Built without gpsd/libgps: no native GPS source available. Callers fall
 * back to the ipinfo.io geolocation path in src/core/location.c. */
GpsStatus platform_native_get_location(PlatformLatLng *latlong) {
  (void)latlong;
  return GPS_UNAVAILABLE;
}

#endif /* HAVE_LIBGPS */
