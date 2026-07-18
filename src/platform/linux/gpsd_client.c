#define _POSIX_C_SOURCE 200809L

#include "gpsd_client.h"

#include "json.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

// Max bytes of a single gpsd JSON line the parser will consider. Longer lines
// are ignored (never grown or partially parsed).
#define GPSD_LINE_MAX 8192

void gpsd_scan_line(const char *line, GpsdScan *scan) {
  if (!line || !scan)
    return;

  // json get_value wants a mutable, NUL-terminated buffer; copy into a bounded
  // stack buffer and drop anything at or over the cap.
  char buf[GPSD_LINE_MAX];
  size_t n = strlen(line);
  if (n == 0 || n >= sizeof(buf))
    return;
  memcpy(buf, line, n + 1);

  JsonContext *ctx = json_begin();
  if (!ctx)
    return;

  char *cls = get_value(ctx, "class", buf);
  if (cls) {
    // Any per-device report means gpsd has a device, even without a fix.
    if (strcmp(cls, "TPV") == 0 || strcmp(cls, "SKY") == 0 || strcmp(cls, "DEVICE") == 0)
      scan->saw_device = true;

    if (strcmp(cls, "TPV") == 0) {
      char *mode = get_value(ctx, "mode", buf);
      char *lat = get_value(ctx, "lat", buf);
      char *lon = get_value(ctx, "lon", buf);
      if (mode && lat && lon && atoi(mode) >= 2) {
        char *elat = NULL;
        char *elon = NULL;
        double dlat = strtod(lat, &elat);
        double dlon = strtod(lon, &elon);
        // Require a full numeric parse and a valid geographic range.
        if (elat != lat && elon != lon && isfinite(dlat) && isfinite(dlon) &&
            dlat >= -90.0 && dlat <= 90.0 && dlon >= -180.0 && dlon <= 180.0) {
          scan->lat = dlat;
          scan->lng = dlon;
          scan->have_fix = true;
        }
      }
    }
  }

  json_end(ctx);
}
