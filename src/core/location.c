#include "location.h"
#include "country.h"
#include "json.h"
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

int location_fetch(Config *cfg) {
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
    if (!timezone_name_is_valid(tz_str)) {
      fprintf(stderr, "location: ignoring invalid timezone from API\n");
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

  return 0;
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
