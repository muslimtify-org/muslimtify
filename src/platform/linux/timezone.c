// POSIX implementation of parse_timezone_offset.
// Uses the system tzdb (typically /usr/share/zoneinfo) via libc:
//   setenv(TZ) -> tzset() -> localtime_r() -> tm_gmtoff.
// DST and historical zone changes are honored automatically.

#define _GNU_SOURCE

#include "location.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

double parse_timezone_offset(const char *tz_name, time_t when) {
  if (!tz_name)
    return 0.0;

  // Save the current TZ so we never leak our setenv to other callers.
  const char *old_tz = getenv("TZ");
  char *saved = old_tz ? strdup(old_tz) : NULL;

  setenv("TZ", tz_name, 1);
  tzset();

  struct tm lt;
  localtime_r(&when, &lt);
  double offset = (double)lt.tm_gmtoff / 3600.0;

  if (saved) {
    setenv("TZ", saved, 1);
    free(saved);
  } else {
    unsetenv("TZ");
  }
  tzset();

  return offset;
}
