#include "location.h"
#include "json.h"
#include "string_util.h"
#include <curl/curl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

// Parse timezone string like "Asia/Jakarta" to get UTC offset
static double parse_timezone_offset(const char *tz_name) {
  // Common timezone offsets
  struct {
    const char *name;
    double offset;
  } timezones[] = {
      {"Asia/Jakarta", 7.0},         {"Asia/Makassar", 8.0},
      {"Asia/Jayapura", 9.0},        {"Asia/Kuala_Lumpur", 8.0},
      {"Asia/Singapore", 8.0},       {"Asia/Bangkok", 7.0},
      {"Asia/Dubai", 4.0},           {"Asia/Riyadh", 3.0},
      {"Asia/Karachi", 5.0},         {"Asia/Kolkata", 5.5},
      {"Asia/Dhaka", 6.0},           {"Asia/Tokyo", 9.0},
      {"Europe/London", 0.0},        {"Europe/Paris", 1.0},
      {"Europe/Istanbul", 3.0},      {"America/New_York", -5.0},
      {"America/Chicago", -6.0},     {"America/Denver", -7.0},
      {"America/Los_Angeles", -8.0}, {"UTC", 0.0},
  };

  for (size_t i = 0; i < sizeof(timezones) / sizeof(timezones[0]); i++) {
    if (strcmp(tz_name, timezones[i].name) == 0) {
      return timezones[i].offset;
    }
  }

  // Default to UTC if unknown
  return 0.0;
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

  // Parse timezone
  char *tz_str = get_value(ctx, "timezone", response.data);
  if (tz_str) {
    if (!copy_string(cfg->timezone, sizeof(cfg->timezone), tz_str)) {
      location_log_trunc("timezone");
    }
    cfg->timezone_offset = parse_timezone_offset(tz_str);
  }

  // Parse city
  char *city_str = get_value(ctx, "city", response.data);
  if (city_str) {
    if (!copy_string(cfg->city, sizeof(cfg->city), city_str)) {
      location_log_trunc("city");
    }
  }

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

int location_auto_detect(Config *cfg) {
  return location_fetch(cfg);
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

void location_cleanup(void) {
  // No-op: main() owns the curl_global_init/cleanup lifecycle
}
