// POSIX implementation of parse_timezone_offset.
// Uses the system tzdb (typically /usr/share/zoneinfo) via libc:
//   setenv(TZ) -> tzset() -> localtime_r() -> tm_gmtoff.
// DST and historical zone changes are honored automatically.

#define _GNU_SOURCE

#include "location.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

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

static int copy_zone_tail(const char *path, char *buf, size_t cap) {
  // Find the substring "/zoneinfo/" and take everything after it.
  const char *needle = "/zoneinfo/";
  const char *p = strstr(path, needle);
  if (!p)
    return -1;
  p += strlen(needle);
  size_t n = strlen(p);
  if (n == 0 || n + 1 > cap)
    return -1;
  memcpy(buf, p, n + 1);
  return 0;
}

int get_system_timezone(char *buf, size_t cap) {
  if (!buf || cap < 2)
    return -1;

  // Primary: readlink("/etc/localtime") -> /usr/share/zoneinfo/<Area>/<Zone>.
  char link[512];
  ssize_t n = readlink("/etc/localtime", link, sizeof(link) - 1);
  if (n > 0) {
    link[n] = '\0';
    if (copy_zone_tail(link, buf, cap) == 0)
      return 0;
  }

  // Fallback: /etc/timezone (Debian/Ubuntu) contains "Area/Zone\n".
  FILE *f = fopen("/etc/timezone", "r");
  if (f) {
    char line[128];
    char *got = fgets(line, sizeof(line), f);
    fclose(f);
    if (got) {
      size_t len = strlen(line);
      while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
        line[--len] = '\0';
      if (len > 0 && len + 1 <= cap) {
        memcpy(buf, line, len + 1);
        return 0;
      }
    }
  }

  // Last resort: "UTC".
  if (cap >= 4) {
    memcpy(buf, "UTC", 4);
  } else {
    buf[0] = '\0';
  }
  return -1;
}
