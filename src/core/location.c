#include "location.h"
#include "country.h"
#include "json.h"
#include "log.h"
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
    MT_LOGF(MT_LOG_ERROR, "Error: Response chunk too large");
    return 0;
  }
  size_t realsize = size * nmemb;
  ResponseBuffer *buf = (ResponseBuffer *)userp;

  // Guard against overflow in buf->size + realsize + 1
  if (realsize > SIZE_MAX - buf->size - 1) {
    MT_LOGF(MT_LOG_ERROR, "Error: Response too large");
    return 0;
  }

  char *ptr = realloc(buf->data, buf->size + realsize + 1);
  if (!ptr) {
    MT_LOGF(MT_LOG_ERROR, "Error: Not enough memory for response");
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
    MT_LOGF(MT_LOG_WARNING, "location: truncated field %s", field ? field : "(unknown)");
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

int location_parse_ipinfo(Config *cfg, char *body);

static int location_fetch_ipinfo(Config *cfg) {
  if (!cfg)
    return -1;

  CURL *curl = curl_easy_init();
  if (!curl) {
    MT_LOGF(MT_LOG_ERROR, "Error: Failed to initialize libcurl");
    return -1;
  }

  ResponseBuffer response = {0};
  response.data = malloc(1);
  if (!response.data) {
    MT_LOGF(MT_LOG_ERROR, "Error: Not enough memory");
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
    MT_LOGF(MT_LOG_ERROR, "Error: Failed to apply TLS hardening to curl handle");
    curl_easy_cleanup(curl);
    free(response.data);
    return -1;
  }

  CURLcode res = curl_easy_perform(curl);

  if (res != CURLE_OK) {
    MT_LOGF(MT_LOG_ERROR, "Error: Failed to fetch location: %s", curl_easy_strerror(res));
    curl_easy_cleanup(curl);
    free(response.data);
    return -1;
  }

  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
  curl_easy_cleanup(curl);

  if (http_code != 200) {
    MT_LOGF(MT_LOG_ERROR, "Error: Location API returned HTTP %ld", http_code);
    free(response.data);
    return -1;
  }

  int rc = location_parse_ipinfo(cfg, response.data);
  free(response.data);
  return rc;
}

// Apply an ipinfo.io JSON reply to cfg. Returns 0 on success, -1 when the reply
// carries no usable coordinates, in which case cfg is left untouched. A 200
// reply can still lack them (the bogon reply for a private IP has no "loc"),
// and treating that as success would save 0,0 and refetch on every call.
// Non-static so tests can feed canned replies without a network round-trip.
int location_parse_ipinfo(Config *cfg, char *body) {
  if (!cfg || !body)
    return -1;

  JsonContext *ctx = json_begin();
  if (!ctx)
    return -1;

  // Parse "loc" field (format: "latitude,longitude"). Both numbers must be
  // fully consumed, finite and in range, or the whole reply is rejected.
  char *loc_str = get_value(ctx, "loc", body);
  char *comma = loc_str ? strchr(loc_str, ',') : NULL;
  if (!comma) {
    MT_LOGF(MT_LOG_ERROR, "Error: Location API returned no coordinates");
    json_end(ctx);
    return -1;
  }
  *comma = '\0';
  char *lat_end = NULL;
  char *lon_end = NULL;
  double lat = strtod(loc_str, &lat_end);
  double lon = strtod(comma + 1, &lon_end);
  if (lat_end == loc_str || *lat_end != '\0' || lon_end == comma + 1 || *lon_end != '\0' ||
      !isfinite(lat) || !isfinite(lon) || lat < -90.0 || lat > 90.0 || lon < -180.0 ||
      lon > 180.0) {
    MT_LOGF(MT_LOG_ERROR, "Error: Location API returned invalid coordinates");
    json_end(ctx);
    return -1;
  }
  cfg->latitude = lat;
  cfg->longitude = lon;

  // Parse timezone. Reject a hostile/garbage value from the network before it
  // reaches setenv("TZ")/tzset() or gets persisted to config.
  char *tz_str = get_value(ctx, "timezone", body);
  if (tz_str) {
    if (!timezone_exists(tz_str)) {
      MT_LOGF(MT_LOG_WARNING, "location: ignoring invalid/unknown timezone from API");
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
  char *country_str = get_value(ctx, "country", body);
  if (country_str) {
    if (!copy_string(cfg->country, sizeof(cfg->country), country_str)) {
      location_log_trunc("country");
    }
  }

  json_end(ctx);

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

int location_fetch_core(Config *cfg, GpsStatus (*gps)(Config *), int (*ipinfo)(Config *),
                        GpsStatus *gps_status) {
  if (!cfg)
    return -1;

  GpsStatus st = GPS_OK;
  if (cfg->use_gps) {
    st = gps(cfg);
    if (st == GPS_OK) {
      if (gps_status)
        *gps_status = st;
      return 0; // GPS fix wins
    }

    // Structural failure: the daemon or hardware is genuinely gone and will not
    // come back on its own. Auto-disable so we stop paying the probe cost every
    // cycle; whoever saves *cfg persists use_gps.
    if (st == GPS_NO_DAEMON || st == GPS_NO_DEVICE || st == GPS_UNAVAILABLE)
      cfg->use_gps = false;
    // GPS_NO_PERMISSION: fixable by the user in OS settings, so stay enabled:
    // the next fetch after they grant access succeeds with no further action.
    // GPS_NO_FIX: transient (e.g. indoors). Stay enabled, and fall through to
    // ipinfo for this cycle; GPS is retried on the next fetch.
  }

  if (gps_status)
    *gps_status = st;
  return ipinfo(cfg);
}

int location_detect(Config *cfg, GpsStatus *gps_status) {
  return location_fetch_core(cfg, location_fetch_gps, location_fetch_ipinfo, gps_status);
}

bool location_is_stale(const Config *cfg, int64_t now) {
  if (!cfg || !cfg->auto_detect)
    return false;
  if (cfg->refresh_interval <= 0) /* disabled */
    return false;
  return (now - cfg->updated_at) >= cfg->refresh_interval;
}
