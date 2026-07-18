#ifndef PLATFORM_NATIVE_H
#define PLATFORM_NATIVE_H

#include <stddef.h>
#include <sys/types.h> /* ssize_t */

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

// Per-OS primitives backing the portable layer (src/platform/posix/). Each
// target supplies its own implementation; posix/ stays unchanged across them.

// Write the running executable's absolute path (null-terminated) into buf.
// Returns bytes written (> 0), or -1 on failure (buf set to "").
// POSIX-only: uses ssize_t and is implemented by the posix/linux layer. Windows
// resolves the exe path in platform_win.c and never calls this, so the
// declaration is hidden there (MSVC has no ssize_t).
#ifndef _WIN32
ssize_t platform_native_exe_path(char *buf, size_t cap);
#endif

// Get the current location (lat/lng) of the device. See GpsStatus for outcomes.
GpsStatus platform_native_get_location(PlatformLatLng *latlong);

#endif /* PLATFORM_NATIVE_H */
