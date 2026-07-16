#ifndef PLATFORM_NATIVE_H
#define PLATFORM_NATIVE_H

#include <stddef.h>
#include <sys/types.h> /* ssize_t */

typedef struct {
  double lat;
  double lng;
} PlatformLatLng;

// Per-OS primitives backing the portable layer (src/platform/posix/). Each
// target supplies its own implementation; posix/ stays unchanged across them.

// Write the running executable's absolute path (null-terminated) into buf.
// Returns bytes written (> 0), or -1 on failure (buf set to "").
ssize_t platform_native_exe_path(char *buf, size_t cap);

// Get the current location (lat/lng) of the device.
// Returns 0 on success, or -1 on failure.
int platform_native_get_location(PlatformLatLng *latlong);

#endif /* PLATFORM_NATIVE_H */
