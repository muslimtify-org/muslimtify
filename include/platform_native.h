#ifndef PLATFORM_NATIVE_H
#define PLATFORM_NATIVE_H

typedef struct {
  double lat;
  double lng;
} PlatformLatLng;

// Outcome of a GPS location read. Ordinary success is GPS_OK (0); the negative
// codes distinguish failure modes so callers can decide whether to warn,
// auto-disable, or silently retry.
typedef enum {
  GPS_OK = 0,           /* got a valid fix; *latlong written */
  GPS_UNAVAILABLE = -1, /* no GPS client on this platform (Windows stub) */
  GPS_NO_DAEMON = -2,   /* could not connect to gpsd (not installed / running) */
  GPS_NO_DEVICE = -3,   /* gpsd reachable but reports zero GPS devices */
  GPS_NO_FIX = -4,      /* device present, but no fix before the timeout */
} GpsStatus;

// Cross-platform device-location primitive. Each OS supplies its own
// implementation: Linux reads a running gpsd over a socket
// (src/platform/linux/gpsd_client.c); Windows returns GPS_UNAVAILABLE
// (src/platform/windows/platform_win.c). Kept free of POSIX-only types so it
// compiles in every build (it is included via location.h everywhere).
//
// Get the current location (lat/lng) of the device. See GpsStatus for outcomes.
GpsStatus platform_native_get_location(PlatformLatLng *latlong);

#endif /* PLATFORM_NATIVE_H */
