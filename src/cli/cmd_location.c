#include "cache.h"
#include "cli_internal.h"
#include "country.h"
#include "display.h"
#include "location.h"
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Copy `name` into cfg->city with NUL-termination, truncating on overflow.
static void set_city(Config *cfg, const char *name) {
  size_t cap = sizeof(cfg->city);
  size_t n = strlen(name);
  if (n >= cap)
    n = cap - 1;
  memcpy(cfg->city, name, n);
  cfg->city[n] = '\0';
}

// Copy the 2-letter `code` into cfg->country, uppercased and NUL-terminated.
// Caller must have validated `code` via country_is_valid_alpha2 first.
static void set_country(Config *cfg, const char *code) {
  cfg->country[0] = (char)toupper((unsigned char)code[0]);
  cfg->country[1] = (char)toupper((unsigned char)code[1]);
  cfg->country[2] = '\0';
}

static const char *LOCATION_SET_USAGE =
    "Usage: muslimtify location set [--lat=<latitude>] [--long=<longitude>] "
    "[--timezone=<iana>] [--city=<name>] [--country=<iso2>] "
    "[--refresh-interval=<seconds>]\n";

static void print_location_set_help(void) {
  printf("\n");
  printf("Update saved location fields\n");
  printf("\n");
  printf("Usage: muslimtify location set [options]\n");
  printf("\n");
  printf("Commands:\n");
  printf("  %-25s %s\n", "-h, --help", "Show this help");
  printf("\n");
  printf("Options:\n");
  printf("  %-25s %s\n", "--lat=<latitude>", "Set latitude");
  printf("  %-25s %s\n", "--long=<longitude>", "Set longitude");
  printf("  %-25s %s\n", "--timezone=<iana>", "Set IANA timezone");
  printf("  %-25s %s\n", "--city=<name>", "Set city label");
  printf("  %-25s %s\n", "--country=<iso2>", "Set ISO-2 country code");
  printf("  %-25s %s\n", "--auto", "Detect coordinates/timezone/country from IP");
  printf("  %-25s %s\n", "--refresh-interval=<s>",
         "Auto-refresh interval in seconds (0=off, min 3600)");
  printf("\n");
  printf("Note: --auto may be combined only with --city / --country.\n");
  printf("\n");
  printf("Examples:\n");
  printf("  %-25s %s\n", "muslimtify location set --lat=-6.21 --long=106.84", "# Set coordinates");
  printf("  %-25s %s\n", "muslimtify location set --timezone=Asia/Jakarta", "# Override timezone");
  printf("  %-25s %s\n", "muslimtify location set --auto", "# Detect from IP");
}

// What a manual `location set` asks for. A NULL string means the flag was not
// given. The numbers are valid only when their string is set.
typedef struct {
  const char *lat_str;
  const char *lon_str;
  const char *tz;
  const char *city;
  const char *country;
  bool has_refresh;
  double lat;
  double lon;
  long long refresh;
} LocationSetArgs;

// Apply a manual `location set` to the handle, save, and print what changed.
static int location_set_run(Muslimtify *mt, const LocationSetArgs *a) {
  MuslimtifyLocation loc;
  MuslimtifyError err = muslimtify_get_location(mt, &loc);
  if (err != MUSLIMTIFY_OK)
    return cli_fail(err);

  // Coordinates go first: moving them resets the city, the country and the
  // timezone, which the setters below then override.
  bool coords_changed = a->lat_str || a->lon_str;
  if (coords_changed) {
    err = muslimtify_set_coordinates(mt, a->lat_str ? a->lat : loc.latitude,
                                     a->lon_str ? a->lon : loc.longitude);
    if (err == MUSLIMTIFY_ERR_INVALID_LATITUDE && a->lat_str)
      return cli_fail_value(err, a->lat_str);
    if (err == MUSLIMTIFY_ERR_INVALID_LONGITUDE && a->lon_str)
      return cli_fail_value(err, a->lon_str);
    if (err != MUSLIMTIFY_OK)
      return cli_fail(err);
  }
  if (a->city) {
    err = muslimtify_set_city(mt, a->city);
    if (err != MUSLIMTIFY_OK)
      return cli_fail(err);
  }
  if (a->country) {
    err = muslimtify_set_country(mt, a->country);
    if (err != MUSLIMTIFY_OK)
      return cli_fail_value(err, a->country);
  }
  if (a->has_refresh) {
    err = muslimtify_set_refresh_interval(mt, a->refresh);
    if (err != MUSLIMTIFY_OK)
      return cli_fail(err);
  }
  if (a->tz) {
    err = muslimtify_set_timezone(mt, a->tz);
    if (err != MUSLIMTIFY_OK)
      return cli_fail_value(err, a->tz);
  }

  err = muslimtify_save(mt);
  if (err == MUSLIMTIFY_OK)
    err = muslimtify_get_location(mt, &loc);
  if (err != MUSLIMTIFY_OK)
    return cli_fail(err);

  // Concise confirmation: report only the fields the user actually changed.
  if (coords_changed)
    printf("Coordinates updated to %.4f, %.4f\n", loc.latitude, loc.longitude);
  if (a->city)
    printf("City updated to %s\n", loc.city);
  if (a->country)
    printf("Country updated to %s\n", loc.country);
  if (a->tz)
    printf("Timezone updated to %s (UTC%+.1f)\n", loc.timezone, loc.utc_offset);
  else if (coords_changed)
    printf("Timezone updated to %s (UTC%+.1f) from system timezone\n", loc.timezone,
           loc.utc_offset);
  if (a->has_refresh) {
    if (loc.refresh_interval == 0)
      printf("Auto-refresh disabled\n");
    else
      printf("Refresh interval updated to %llds\n", loc.refresh_interval);
  }

  // Coordinates and timezone jointly determine the prayer times, so prompt the
  // user to sanity-check whichever of the pair they did not just set.
  if (coords_changed && !a->tz)
    printf("  Hint: make sure the timezone is correct (currently %s); it affects prayer time "
           "calculation.\n",
           loc.timezone);
  else if (a->tz && !coords_changed)
    printf("  Hint: make sure your coordinates are correct (currently %.4f, %.4f); they affect "
           "prayer time calculation.\n",
           loc.latitude, loc.longitude);
  return 0;
}

static int location_set_handler(int argc, char **argv) {
  if (cli_wants_help(argc, argv)) {
    print_location_set_help();
    return 0;
  }
  bool auto_detect = false;
  const char *override_lat = NULL;
  const char *override_lon = NULL;
  const char *override_tz = NULL;
  const char *override_city = NULL;
  const char *override_country = NULL;
  const char *override_refresh = NULL;

  for (int i = 0; i < argc; ++i) {
    if (strncmp(argv[i], "--lat=", 6) == 0) {
      override_lat = argv[i] + 6;
      if (*override_lat == '\0') {
        fprintf(stderr, "Error: --lat requires a value (e.g. --lat=1.2345)\n");
        return 1;
      }
    } else if (strcmp(argv[i], "--lat") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: --lat requires a value (e.g. --lat 1.2345)\n");
        return 1;
      }
      override_lat = argv[++i];
    } else if (strncmp(argv[i], "--long=", 7) == 0) {
      override_lon = argv[i] + 7;
      if (*override_lon == '\0') {
        fprintf(stderr, "Error: --long requires a value (e.g. --long=1.2345)\n");
        return 1;
      }
    } else if (strcmp(argv[i], "--long") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: --long requires a value (e.g. --long 1.2345)\n");
        return 1;
      }
      override_lon = argv[++i];
    } else if (strncmp(argv[i], "--timezone=", 11) == 0) {
      override_tz = argv[i] + 11;
      if (*override_tz == '\0') {
        fprintf(stderr, "Error: --timezone requires a value (e.g. --timezone=Asia/Jakarta)\n");
        return 1;
      }
    } else if (strcmp(argv[i], "--timezone") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: --timezone requires a value (e.g. --timezone Asia/Jakarta)\n");
        return 1;
      }
      override_tz = argv[++i];
    } else if (strncmp(argv[i], "--city=", 7) == 0) {
      override_city = argv[i] + 7;
      if (*override_city == '\0') {
        fprintf(stderr, "Error: --city requires a value (e.g. --city=Jakarta)\n");
        return 1;
      }
    } else if (strcmp(argv[i], "--city") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: --city requires a value (e.g. --city Jakarta)\n");
        return 1;
      }
      override_city = argv[++i];
    } else if (strncmp(argv[i], "--country=", 10) == 0) {
      override_country = argv[i] + 10;
      if (*override_country == '\0') {
        fprintf(stderr, "Error: --country requires a value (e.g. --country=ID)\n");
        return 1;
      }
    } else if (strcmp(argv[i], "--country") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: --country requires a value (e.g. --country ID)\n");
        return 1;
      }
      override_country = argv[++i];
    } else if (strncmp(argv[i], "--refresh-interval=", 19) == 0) {
      override_refresh = argv[i] + 19;
      if (*override_refresh == '\0') {
        fprintf(stderr, "Error: --refresh-interval requires a value in seconds "
                        "(e.g. --refresh-interval=43200, or 0 to disable)\n");
        return 1;
      }
    } else if (strcmp(argv[i], "--refresh-interval") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: --refresh-interval requires a value in seconds "
                        "(e.g. --refresh-interval 43200, or 0 to disable)\n");
        return 1;
      }
      override_refresh = argv[++i];
    } else if (strcmp(argv[i], "--auto") == 0) {
      auto_detect = true;
    } else {
      fprintf(stderr, "Error: unexpected argument '%s'\n%s", argv[i], LOCATION_SET_USAGE);
      return 1;
    }
  }

  if (auto_detect) {
    if (override_lat || override_lon || override_tz || override_refresh) {
      fprintf(stderr, "Error: --auto may be combined only with --city / --country; "
                      "--lat / --long / --timezone / --refresh-interval cannot be combined "
                      "with --auto\n");
      return 1;
    }
    // Check the country code before the network fetch, not after it.
    if (override_country && !country_is_valid_alpha2(override_country)) {
      fprintf(stderr, "Error: Invalid country code '%s' (expected ISO 3166-1 alpha-2, e.g. ID)\n",
              override_country);
      return 1;
    }

    Config cfg;
    if (config_load(&cfg) != 0) {
      fprintf(stderr, "Error: Failed to load config\n");
      return 1;
    }

    printf("Detecting location...\n");
    if (location_fetch(&cfg) != 0) {
      fprintf(stderr, "Error: Failed to fetch location\n");
      return 1;
    }
    cfg.auto_detect = true;

    if (override_city)
      set_city(&cfg, override_city);
    if (override_country)
      set_country(&cfg, override_country);

    if (config_save(&cfg) != 0) {
      fprintf(stderr, "Error: Failed to save config\n");
      return 1;
    }
    cache_invalidate();

    printf("✓ Location detected: %.4f, %.4f\n", cfg.latitude, cfg.longitude);
    printf("  Timezone: %s (UTC%+.1f)\n", cfg.timezone, cfg.timezone_offset);
    if (cfg.city[0] != '\0')
      printf("  City: %s\n", cfg.city);
    if (cfg.country[0] != '\0')
      printf("  Country: %s\n", cfg.country);
    return 0;
  }

  if (!override_lat && !override_lon && !override_tz && !override_city && !override_country &&
      !override_refresh) {
    fputs(LOCATION_SET_USAGE, stderr);
    return 1;
  }

  // Text that is not a number is the CLI's error. Whether the number is an
  // acceptable value is the library's.
  LocationSetArgs args = {0};
  args.lat_str = override_lat;
  args.lon_str = override_lon;
  args.tz = override_tz;
  args.city = override_city;
  args.country = override_country;
  args.has_refresh = override_refresh != NULL;

  if (override_lat) {
    char *end_lat;
    errno = 0;
    args.lat = strtod(override_lat, &end_lat);
    if (end_lat == override_lat || *end_lat != '\0' || errno == ERANGE)
      return cli_fail_value(MUSLIMTIFY_ERR_INVALID_LATITUDE, override_lat);
  }
  if (override_lon) {
    char *end_lon;
    errno = 0;
    args.lon = strtod(override_lon, &end_lon);
    if (end_lon == override_lon || *end_lon != '\0' || errno == ERANGE)
      return cli_fail_value(MUSLIMTIFY_ERR_INVALID_LONGITUDE, override_lon);
  }
  if (override_refresh) {
    char *end_ri;
    errno = 0;
    args.refresh = strtoll(override_refresh, &end_ri, 10);
    if (end_ri == override_refresh || *end_ri != '\0' || errno == ERANGE || args.refresh < 0) {
      fprintf(stderr,
              "Error: Invalid --refresh-interval '%s' (expected a non-negative "
              "integer number of seconds)\n",
              override_refresh);
      return 1;
    }
  }

  Muslimtify *mt = NULL;
  if (cli_open(&mt))
    return 1;
  int ret = location_set_run(mt, &args);
  muslimtify_close(mt);
  return ret;
}

static void print_location_gps_help(void) {
  printf("\n");
  printf("Enable or disable reading location from a local gpsd receiver\n");
  printf("\n");
  printf("Usage: muslimtify location gps [on|off]\n");
  printf("\n");
  printf("Commands:\n");
  printf("  %-25s %s\n", "on", "Probe GPS and enable it if a receiver is available");
  printf("  %-25s %s\n", "off", "Disable GPS (use ipinfo network geolocation)");
  printf("  %-25s %s\n", "-h, --help", "Show this help");
  printf("\n");
  printf("With no argument, shows whether GPS is currently enabled.\n");
}

static int location_gps_handler(int argc, char **argv) {
  if (cli_wants_help(argc, argv)) {
    print_location_gps_help();
    return 0;
  }
  if (cli_reject_extra_args("location gps", argc - 1, argv + 1))
    return 1;

  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }

  // No argument: report current state.
  if (argc == 0) {
    printf("GPS is %s\n", cfg.use_gps ? "enabled" : "disabled");
    return 0;
  }

  if (strcmp(argv[0], "off") == 0) {
    cfg.use_gps = false;
    if (config_save(&cfg) != 0) {
      fprintf(stderr, "Error: Failed to save config\n");
      return 1;
    }
    cache_invalidate();
    printf("GPS disabled; using ipinfo network geolocation.\n");
    return 0;
  }

  if (strcmp(argv[0], "on") == 0) {
    // Validating enable: probe GPS now and only persist use_gps=true if a
    // receiver is present. A transient no-fix (device present, no signal yet)
    // still enables, since GPS engages once a fix is available.
    GpsStatus st = location_fetch_gps(&cfg);
    switch (st) {
    case GPS_OK:
    case GPS_NO_FIX:
      // Both enable: a fix is ready now, or a device is present and GPS will
      // engage once it gets one. Only the confirmation message differs.
      cfg.use_gps = true;
      if (config_save(&cfg) != 0) {
        fprintf(stderr, "Error: Failed to save config\n");
        return 1;
      }
      cache_invalidate();
      if (st == GPS_OK)
        printf("✓ GPS ready — enabled. Location: %.4f, %.4f\n", cfg.latitude, cfg.longitude);
      else
        printf("✓ GPS enabled. No fix yet; using ipinfo until a fix is available.\n");
      return 0;
    case GPS_NO_DAEMON:
      fprintf(stderr, "GPS: cannot reach gpsd. Install and start it, then try again.\n");
      return 1;
    case GPS_NO_DEVICE:
      fprintf(stderr, "GPS: no GPS device detected. Connect one, then try again.\n");
      return 1;
    case GPS_NO_PERMISSION:
      fprintf(stderr, "GPS: location access is turned off. Turn on Settings > "
                      "Privacy & security > Location, then try again.\n");
      return 1;
    case GPS_UNAVAILABLE:
      fprintf(stderr, "GPS not available in this build.\n");
      return 1;
    }
    // No default: label above, so -Wswitch (via -Wall) fails the build if a new
    // GpsStatus variant is added without deciding whether it should enable GPS.
    // This return exists only to satisfy the compiler's flow analysis.
    return 1;
  }

  fprintf(stderr, "Error: unknown 'location gps' argument '%s' (expected on|off)\n", argv[0]);
  return 1;
}

static void print_location_help(void) {
  printf("\n");
  printf("Show or update your saved location\n");
  printf("\n");
  printf("Usage: muslimtify location [options]\n");
  printf("\n");
  printf("Commands:\n");
  printf("  %-25s %s\n", "set [options]", "Update saved location fields");
  printf("  %-25s %s\n", "      --lat=<latitude>", "Set latitude");
  printf("  %-25s %s\n", "      --long=<longitude>", "Set longitude");
  printf("  %-25s %s\n", "      --timezone=<iana>", "Set IANA timezone");
  printf("  %-25s %s\n", "      --city=<name>", "Set city label");
  printf("  %-25s %s\n", "      --country=<iso2>", "Set ISO-2 country code");
  printf("  %-25s %s\n", "      --auto", "Detect coordinates/timezone/country from IP");
  printf("  %-25s %s\n", "      --refresh-interval=<s>",
         "Auto-refresh interval in seconds (0=off, min 3600)");
  printf("  %-25s %s\n", "gps [on|off]", "Enable/disable GPS (gpsd) location source");
  printf("  %-25s %s\n", "-h, --help", "Show this help");
  printf("\n");
  printf("Options:\n");
  printf("  %-25s %s\n", "--json", "Show location as JSON");
  printf("  %-25s %s\n", "--headless", "Show location as key=value");
  printf("\n");
  printf("Examples:\n");
  printf("  %-25s %s\n", "muslimtify location", "# Show current location");
  printf("  %-25s %s\n", "muslimtify location --json", "# Show location as JSON");
  printf("  %-25s %s\n", "muslimtify location set --auto", "# Detect location from IP");
  printf("  %-25s %s\n", "muslimtify location gps on", "# Use local gpsd for location");
  printf("  %-25s %s\n", "muslimtify location set --refresh-interval=21600",
         "# Auto-refresh every 6h (0=off)");
}

int handle_location(int argc, char **argv) {
  if (argc > 0 && strcmp(argv[0], "set") == 0)
    return location_set_handler(argc - 1, argv + 1);

  if (argc > 0 && strcmp(argv[0], "gps") == 0)
    return location_gps_handler(argc - 1, argv + 1);

  if (argc > 0 && strcmp(argv[0], "show") == 0) {
    fprintf(
        stderr,
        "Error: 'location show' was removed; use 'location' (optionally --json / --headless)\n");
    return 1;
  }
  if (argc > 0 && strcmp(argv[0], "refresh") == 0) {
    fprintf(stderr, "Error: 'location refresh' was removed; use 'location set --auto'\n");
    return 1;
  }
  if (argc > 0 && strcmp(argv[0], "clear") == 0) {
    fprintf(stderr, "Error: 'location clear' was removed; use 'location set --auto'\n");
    return 1;
  }

  if (cli_wants_help(argc, argv)) {
    print_location_help();
    return 0;
  }

  for (int i = 0; i < argc; i++) {
    const char *a = argv[i];
    if (strcmp(a, "--json") == 0 || strcmp(a, "--headless") == 0)
      continue;
    fprintf(stderr, "Error: unknown location argument '%s'\n", a);
    return 1;
  }

  OutputMode mode = OUTPUT_TABLE;
  if (cli_parse_output_mode(argc, argv, &mode) != 0)
    return 1;

  Muslimtify *mt = NULL;
  if (cli_open(&mt))
    return 1;
  MuslimtifyLocation loc;
  MuslimtifyError err = muslimtify_get_location(mt, &loc);
  muslimtify_close(mt);
  if (err != MUSLIMTIFY_OK)
    return cli_fail(err);

  switch (mode) {
  case OUTPUT_JSON:
    display_location_json(&loc);
    break;
  case OUTPUT_HEADLESS:
    display_location_headless(&loc);
    break;
  default:
    display_location(&loc);
    break;
  }
  return 0;
}
