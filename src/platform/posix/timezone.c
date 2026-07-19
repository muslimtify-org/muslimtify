// POSIX implementation of parse_timezone_offset.
// Uses the system tzdb (typically /usr/share/zoneinfo) via libc:
//   setenv(TZ) -> tzset() -> localtime_r() -> tm_gmtoff.
// DST and historical zone changes are honored automatically.

#define _GNU_SOURCE

#include "location.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

double parse_timezone_offset(const char *tz_name, time_t when) {
  // Defense-in-depth: never setenv/tzset an unvalidated string (a corrupted
  // config value on the load path bypasses location_fetch's check).
  if (!timezone_name_is_valid(tz_name))
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

// True if <dir>/<tz_name> is a regular file (a real tz database entry). stat
// follows symlinks; S_ISREG rejects namespace directories (e.g. "America").
static bool zone_file_in(const char *dir, const char *tz_name) {
  char path[512];
  int n = snprintf(path, sizeof(path), "%s/%s", dir, tz_name);
  if (n < 0 || (size_t)n >= sizeof(path))
    return false;
  struct stat st;
  return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

bool timezone_exists(const char *tz_name) {
  if (!timezone_name_is_valid(tz_name))
    return false;

  // timezone_name_is_valid forbids '.', so tz_name cannot traverse out of the
  // tz database directory. Honor TZDIR exclusively when set (matching libc);
  // otherwise probe the standard zoneinfo locations so a nonstandard layout
  // does not make every real zone read as missing.
  const char *tzdir = getenv("TZDIR");
  if (tzdir && tzdir[0] != '\0')
    return zone_file_in(tzdir, tz_name);

  static const char *const roots[] = {
      "/usr/share/zoneinfo",
      "/usr/lib/zoneinfo",
      "/etc/zoneinfo",
      "/usr/share/lib/zoneinfo",
  };
  for (size_t i = 0; i < ARRAY_LEN(roots); i++) {
    if (zone_file_in(roots[i], tz_name))
      return true;
  }
  return false;
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
