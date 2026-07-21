#include "location.h"
#include "country.h"
#include "json.h"
#include "platform.h"
#include "string_util.h"
#include <curl/curl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

bool timezone_name_is_valid(const char *tz_name) {
  if (!tz_name || tz_name[0] == '\0' || tz_name[0] == ':')
    return false;

  size_t len = 0;
  for (const char *p = tz_name; *p; p++) {
    unsigned char c = (unsigned char)*p;
    bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
              c == '_' || c == '+' || c == '-' || c == '/';
    if (!ok)
      return false;
    if (++len > 64)
      return false;
  }
  return true;
}

typedef struct {
  char *data;
  size_t size;
} ResponseBuffer;

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
  // Guard against integer overflow in size * nmemb
  if (nmemb != 0 && size > SIZE_MAX / nmemb) {
    fprintf(stderr, "Error: Response chunk too large\n");
    return 0;
  }
  size_t realsize = size * nmemb;
  ResponseBuffer *buf = (ResponseBuffer *)userp;

  // Guard against overflow in buf->size + realsize + 1
  if (realsize > SIZE_MAX - buf->size - 1) {
    fprintf(stderr, "Error: Response too large\n");
    return 0;
  }

  char *ptr = realloc(buf->data, buf->size + realsize + 1);
  if (!ptr) {
    fprintf(stderr, "Error: Not enough memory for response\n");
    return 0;
  }

  buf->data = ptr;
  memcpy(&(buf->data[buf->size]), contents, realsize);
  buf->size += realsize;
  buf->data[buf->size] = '\0';

  return realsize;
}

static bool location_trunc_logged = false;

static void location_log_trunc(const char *field) {
  if (!location_trunc_logged) {
    fprintf(stderr, "location: truncated field %s\n", field ? field : "(unknown)");
    location_trunc_logged = true;
  }
}

// Apply TLS + protocol hardening to a curl handle. Returns the first failing
// CURLcode so callers can fail closed, or CURLE_OK when every option was
// accepted by the linked libcurl. Non-static so tests can exercise it without
// a network round-trip (a typo'd option enum or an "https" string the build
// does not recognize surfaces here as a non-OK code).
CURLcode location_harden_curl(CURL *curl) {
  CURLcode rc;
  // Explicit TLS verification (defense-in-depth over libcurl defaults) and
  // restrict transfer + redirects to https so a redirect cannot downgrade to
  // http/file/etc.
  rc = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  if (rc != CURLE_OK)
    return rc;
  rc = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
  if (rc != CURLE_OK)
    return rc;
#if LIBCURL_VERSION_NUM >= 0x075500 /* 7.85.0: string protocol API */
  rc = curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
  if (rc != CURLE_OK)
    return rc;
  rc = curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "https");
  if (rc != CURLE_OK)
    return rc;
#else
  rc = curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
  if (rc != CURLE_OK)
    return rc;
  rc = curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS);
  if (rc != CURLE_OK)
    return rc;
#endif
  return CURLE_OK;
}

static int location_fetch_ipinfo(Config *cfg) {
  if (!cfg)
    return -1;

  CURL *curl = curl_easy_init();
  if (!curl) {
    fprintf(stderr, "Error: Failed to initialize libcurl\n");
    return -1;
  }

  ResponseBuffer response = {0};
  response.data = malloc(1);
  if (!response.data) {
    fprintf(stderr, "Error: Not enough memory\n");
    curl_easy_cleanup(curl);
    return -1;
  }
  response.data[0] = '\0';
  response.size = 0;

  curl_easy_setopt(curl, CURLOPT_URL, "https://ipinfo.io/json");
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "muslimtify/1.0");
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_MAXFILESIZE, 65536L);
  // Fail closed: if the TLS/protocol hardening cannot be applied, do not fall
  // back to an unhardened transfer.
  if (location_harden_curl(curl) != CURLE_OK) {
    fprintf(stderr, "Error: Failed to apply TLS hardening to curl handle\n");
    curl_easy_cleanup(curl);
    free(response.data);
    return -1;
  }

  CURLcode res = curl_easy_perform(curl);

  if (res != CURLE_OK) {
    fprintf(stderr, "Error: Failed to fetch location: %s\n", curl_easy_strerror(res));
    curl_easy_cleanup(curl);
    free(response.data);
    return -1;
  }

  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
  curl_easy_cleanup(curl);

  if (http_code != 200) {
    fprintf(stderr, "Error: Location API returned HTTP %ld\n", http_code);
    free(response.data);
    return -1;
  }

  // Parse JSON response
  JsonContext *ctx = json_begin();
  if (!ctx) {
    free(response.data);
    return -1;
  }

  // Parse "loc" field (format: "latitude,longitude")
  char *loc_str = get_value(ctx, "loc", response.data);
  if (loc_str) {
    char *comma = strchr(loc_str, ',');
    if (comma) {
      *comma = '\0';
      double lat = strtod(loc_str, NULL);
      double lon = strtod(comma + 1, NULL);
      if (lat >= -90.0 && lat <= 90.0)
        cfg->latitude = lat;
      if (lon >= -180.0 && lon <= 180.0)
        cfg->longitude = lon;
    }
  }

  // Parse timezone. Reject a hostile/garbage value from the network before it
  // reaches setenv("TZ")/tzset() or gets persisted to config.
  char *tz_str = get_value(ctx, "timezone", response.data);
  if (tz_str) {
    if (!timezone_exists(tz_str)) {
      fprintf(stderr, "location: ignoring invalid/unknown timezone from API\n");
    } else {
      if (!copy_string(cfg->timezone, sizeof(cfg->timezone), tz_str)) {
        location_log_trunc("timezone");
      }
      cfg->timezone_offset = parse_timezone_offset(tz_str, time(NULL));
    }
  }

  // Note: ipinfo's "city" field is intentionally NOT read. The city label is
  // opt-in user metadata set via `location set/auto --city=<name>`. ipinfo's
  // guess is often wrong (e.g. picking the metro centroid over the user's
  // actual city) and feeds nothing functional in the calculation pipeline.

  // Parse country
  char *country_str = get_value(ctx, "country", response.data);
  if (country_str) {
    if (!copy_string(cfg->country, sizeof(cfg->country), country_str)) {
      location_log_trunc("country");
    }
  }

  json_end(ctx);
  free(response.data);

  cfg->updated_at = (int64_t)time(NULL);

  return 0;
}

const char *gps_status_message(GpsStatus st) {
  switch (st) {
  case GPS_NO_DAEMON:
    return "GPS: gpsd is no longer reachable; disabling GPS and using ipinfo. "
           "Re-enable with 'muslimtify location gps on'.";
  case GPS_NO_DEVICE:
    return "GPS: no GPS device detected; disabling GPS and using ipinfo. "
           "Re-enable with 'muslimtify location gps on'.";
  case GPS_UNAVAILABLE:
    return "GPS: this build has no GPS support; disabling GPS and using ipinfo.";
  case GPS_NO_PERMISSION:
    // Deliberately does not say "disabling": location_fetch_core keeps GPS on
    // for this status, because the user can grant access and have the next
    // fetch succeed with no further action.
    return "GPS: location access is turned off; using ipinfo for now. Turn on "
           "Settings > Privacy & security > Location, and GPS resumes "
           "automatically.";
  case GPS_OK:
  case GPS_NO_FIX:
    return NULL; // not failures the user needs told about
  }
  // No default: label above, so -Wswitch (via -Wall) fails the build if a new
  // GpsStatus variant is added without deciding what to say about it. This
  // return exists only to satisfy the compiler's flow analysis.
  return NULL;
}

GpsStatus location_fetch_gps(Config *cfg) {
  if (!cfg)
    return GPS_UNAVAILABLE;

  PlatformLatLng ll;
  GpsStatus st = platform_get_location(&ll);
  if (st != GPS_OK)
    return st;

  if (!(ll.lat >= -90.0 && ll.lat <= 90.0 && ll.lng >= -180.0 && ll.lng <= 180.0))
    return GPS_NO_FIX;

  cfg->latitude = ll.lat;
  cfg->longitude = ll.lng;
  // GPS carries no timezone: derive it from the host system. On failure
  // get_system_timezone sets "UTC"; continue with that.
  (void)get_system_timezone(cfg->timezone, sizeof(cfg->timezone));
  cfg->timezone_offset = parse_timezone_offset(cfg->timezone, time(NULL));
  cfg->updated_at = (int64_t)time(NULL);
  return GPS_OK; // country intentionally left unchanged
}

int location_fetch_core(Config *cfg, GpsStatus (*gps)(Config *), int (*ipinfo)(Config *)) {
  if (!cfg)
    return -1;

  if (cfg->use_gps) {
    GpsStatus st = gps(cfg);
    if (st == GPS_OK)
      return 0; // GPS fix wins
    // Structural failure: hardware/daemon is genuinely gone. Warn once and
    // auto-disable so we stop trying; whoever saves *cfg persists use_gps.
    if (st == GPS_NO_DAEMON || st == GPS_NO_DEVICE || st == GPS_UNAVAILABLE) {
      fprintf(stderr, "%s\n", gps_status_message(st));
      cfg->use_gps = false;
    }
    // GPS_NO_FIX: transient (e.g. indoors). Stay enabled and fall through to
    // ipinfo for this cycle; GPS is retried on the next fetch.
  }

  return ipinfo(cfg);
}

int location_fetch(Config *cfg) {
  return location_fetch_core(cfg, location_fetch_gps, location_fetch_ipinfo);
}

int config_auto_detect(Config *cfg) {
  if (!cfg)
    return -1;
  if (location_fetch(cfg) != 0)
    return -1;
  copy_string(cfg->calculation_method, sizeof(cfg->calculation_method),
              method_to_string(country_default_method(cfg->country)));
  return 0;
}

int location_prepare(Config *cfg) {
  if (!cfg)
    return -1;

  if (cfg->auto_detect && (fabs(cfg->latitude) < 1e-6 && fabs(cfg->longitude) < 1e-6)) {
    if (location_fetch(cfg) != 0) {
      return -1;
    }

    if (config_save(cfg) != 0) {
      fprintf(stderr, "Error: Failed to save config\n");
      return -1;
    }
  }

  return 0;
}

int ensure_location(Config *cfg) {
  if (!cfg)
    return -1;

  if (cfg->auto_detect && (fabs(cfg->latitude) < 1e-6 && fabs(cfg->longitude) < 1e-6)) {
    printf("Detecting location...\n");
    if (location_prepare(cfg) != 0) {
      fprintf(stderr, "Error: Failed to detect location\n");
      return -1;
    }

    printf("✓ Location detected: ");
    if (cfg->city[0] != '\0') {
      printf("%s, %s\n", cfg->city, cfg->country);
    } else {
      printf("%.4f, %.4f\n", cfg->latitude, cfg->longitude);
    }

    return 0;
  }

  int prepare_result = location_prepare(cfg);
  if (prepare_result < 0) {
    fprintf(stderr, "Error: Failed to detect location\n");
    return -1;
  }

  if (prepare_result == 1) {
    printf("Detecting location...\n");
    printf("✓ Location detected: ");
    if (cfg->city[0] != '\0') {
      printf("%s, %s\n", cfg->city, cfg->country);
    } else {
      printf("%.4f, %.4f\n", cfg->latitude, cfg->longitude);
    }
  }

  return 0;
}

/* Core of location_refresh with the fetch step injected, so tests can drive the
 * copy-then-commit fail-safe deterministically without a network round-trip.
 * Non-static (declared test-only) to keep the seam out of the public header. */
int location_refresh_with(Config *cfg, int (*fetch)(Config *)) {
  if (!cfg)
    return -1;

  /* Respect a manually-configured location: only auto-detected setups get
   * refreshed. */
  if (!cfg->auto_detect)
    return 0;

  /* Fetch into a copy so a failed/partial lookup cannot clobber the cached
   * coordinates. The struct holds only fixed-size fields, so a shallow copy
   * is a complete snapshot. */
  Config candidate = *cfg;
  if (fetch(&candidate) != 0)
    return -1; /* keep *cfg (last known good) untouched */

  *cfg = candidate;
  if (config_save(cfg) != 0) {
    fprintf(stderr, "Error: Failed to save refreshed location\n");
    return -1;
  }

  return 0;
}

int location_refresh(Config *cfg) {
  return location_refresh_with(cfg, location_fetch);
}

bool location_is_stale(const Config *cfg, int64_t now) {
  if (!cfg || !cfg->auto_detect)
    return false;
  if (cfg->refresh_interval <= 0) /* disabled */
    return false;
  return (now - cfg->updated_at) >= cfg->refresh_interval;
}
