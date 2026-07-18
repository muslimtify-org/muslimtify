#ifndef GPSD_CLIENT_H
#define GPSD_CLIENT_H

#include <stdbool.h>

// Accumulated state while reading gpsd's JSON stream.
typedef struct {
  bool saw_device; // a per-device report (TPV/SKY/DEVICE) was seen
  bool have_fix;   // a valid TPV fix was seen
  double lat;      // valid only when have_fix
  double lng;      // valid only when have_fix
} GpsdScan;

// Parse ONE gpsd JSON line (NUL-terminated) and fold it into *scan. Pure: no
// I/O, no globals. Ignores anything it does not understand. Sets have_fix and
// lat/lng only for a TPV with mode>=2 and in-range finite lat/lon. Sets
// saw_device for any TPV/SKY/DEVICE report. Safe on NULL, empty, over-long,
// and malformed input.
void gpsd_scan_line(const char *line, GpsdScan *scan);

#endif /* GPSD_CLIENT_H */
