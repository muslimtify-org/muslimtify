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
#include <time.h>

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

// Returns true if `tz` is one of the canonical UTC aliases (so an offset of 0.0
// is expected, not a sign of an unrecognized zone).
static bool is_utc_zone(const char *tz) {
  return strcmp(tz, "UTC") == 0 || strcmp(tz, "Etc/UTC") == 0 || strcmp(tz, "Etc/GMT") == 0 ||
         strcmp(tz, "GMT") == 0;
}

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
  printf("  %-25s %s\n", "--refresh-interval=<s>", "Auto-refresh interval in seconds (0=off, min 3600)");
  printf("\n");
  printf("Note: --auto may be combined only with --city / --country.\n");
  printf("\n");
  printf("Examples:\n");
  printf("  %-25s %s\n", "muslimtify location set --lat=-6.21 --long=106.84", "# Set coordinates");
  printf("  %-25s %s\n", "muslimtify location set --timezone=Asia/Jakarta", "# Override timezone");
  printf("  %-25s %s\n", "muslimtify location set --auto", "# Detect from IP");
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
    if (override_lat || override_lon || override_tz) {
      fprintf(stderr, "Error: --auto detects coordinates and timezone from IP; "
                      "--lat / --long / --timezone cannot be combined with --auto\n");
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
    if (override_country) {
      if (!country_is_valid_alpha2(override_country)) {
        fprintf(stderr, "Error: Invalid country code '%s' (expected ISO 3166-1 alpha-2, e.g. ID)\n",
                override_country);
        return 1;
      }
      set_country(&cfg, override_country);
    }

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

  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }

  if (override_lat) {
    char *end_lat;
    errno = 0;
    double lat = strtod(override_lat, &end_lat);
    if (end_lat == override_lat || *end_lat != '\0' || errno == ERANGE || lat < -90.0 ||
        lat > 90.0) {
      fprintf(stderr, "Error: Invalid latitude '%s'\n", override_lat);
      return 1;
    }
    cfg.latitude = lat;
    cfg.auto_detect = false;
  }

  if (override_lon) {
    char *end_lon;
    errno = 0;
    double lon = strtod(override_lon, &end_lon);
    if (end_lon == override_lon || *end_lon != '\0' || errno == ERANGE || lon < -180.0 ||
        lon > 180.0) {
      fprintf(stderr, "Error: Invalid longitude '%s'\n", override_lon);
      return 1;
    }
    cfg.longitude = lon;
    cfg.auto_detect = false;
  }

  // If the user moved the coordinates, the previously cached city/country no
  // longer apply — clear them. (A timezone- or label-only update leaves the
  // existing labels intact.) Then write the user's --city/--country overrides.
  if (override_lat || override_lon) {
    cfg.city[0] = '\0';
    cfg.country[0] = '\0';
  }
  if (override_city)
    set_city(&cfg, override_city);
  if (override_country) {
    if (!country_is_valid_alpha2(override_country)) {
      fprintf(stderr, "Error: Invalid country code '%s' (expected ISO 3166-1 alpha-2, e.g. ID)\n",
              override_country);
      return 1;
    }
    set_country(&cfg, override_country);
  }

  if (override_refresh) {
    char *end_ri;
    errno = 0;
    long long ri = strtoll(override_refresh, &end_ri, 10);
    if (end_ri == override_refresh || *end_ri != '\0' || errno == ERANGE || ri < 0) {
      fprintf(stderr, "Error: Invalid --refresh-interval '%s' (expected a non-negative "
                      "integer number of seconds)\n",
              override_refresh);
      return 1;
    }
    if (ri > 0 && ri < LOCATION_MIN_REFRESH_SECONDS) {
      fprintf(stderr, "Error: --refresh-interval must be 0 (disabled) or at least %d seconds\n",
              LOCATION_MIN_REFRESH_SECONDS);
      return 1;
    }
    cfg.refresh_interval = (int64_t)ri;
  }

  if (override_tz) {
    // Explicit override — validate it resolves to something other than the
    // implicit UTC fallback. Useful when the host OS timezone differs from
    // the coordinates' real timezone (e.g. running from a different region).
    double off = parse_timezone_offset(override_tz, time(NULL));
    if (off == 0.0 && !is_utc_zone(override_tz)) {
      fprintf(stderr, "Error: Unknown timezone '%s' (no offset resolvable)\n", override_tz);
      return 1;
    }
    size_t tz_len = strlen(override_tz);
    if (tz_len + 1 > sizeof(cfg.timezone)) {
      fprintf(stderr, "Error: Timezone name too long\n");
      return 1;
    }
    memcpy(cfg.timezone, override_tz, tz_len + 1);
    cfg.timezone_offset = off;
  } else if (override_lat || override_lon) {
    // Coordinates changed without an explicit timezone: re-derive from the host
    // OS so the offset stays correct (avoids inheriting a stale ipinfo zone).
    // A label-only update (city/country) leaves the timezone untouched.
    if (get_system_timezone(cfg.timezone, sizeof(cfg.timezone)) != 0) {
      fprintf(stderr, "Warning: could not detect system timezone, defaulting to %s\n",
              cfg.timezone);
    }
    cfg.timezone_offset = parse_timezone_offset(cfg.timezone, time(NULL));
  }

  if (config_save(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to save config\n");
    return 1;
  }

  cache_invalidate();

  // Concise confirmation: report only the fields the user actually changed.
  bool coords_changed = override_lat || override_lon;
  if (coords_changed)
    printf("Coordinates updated to %.4f, %.4f\n", cfg.latitude, cfg.longitude);
  if (override_city)
    printf("City updated to %s\n", cfg.city);
  if (override_country)
    printf("Country updated to %s\n", cfg.country);
  if (override_tz)
    printf("Timezone updated to %s (UTC%+.1f)\n", cfg.timezone, cfg.timezone_offset);
  else if (coords_changed)
    printf("Timezone updated to %s (UTC%+.1f) from system timezone\n", cfg.timezone,
           cfg.timezone_offset);
  if (override_refresh) {
    if (cfg.refresh_interval == 0)
      printf("Auto-refresh disabled\n");
    else
      printf("Refresh interval updated to %llds\n", (long long)cfg.refresh_interval);
  }

  // Coordinates and timezone jointly determine the prayer times, so prompt the
  // user to sanity-check whichever of the pair they did not just set.
  if (coords_changed && !override_tz)
    printf("  Hint: make sure the timezone is correct (currently %s); it affects prayer time "
           "calculation.\n",
           cfg.timezone);
  else if (override_tz && !coords_changed)
    printf("  Hint: make sure your coordinates are correct (currently %.4f, %.4f); they affect "
           "prayer time calculation.\n",
           cfg.latitude, cfg.longitude);
  return 0;
}

static void print_location_help(void) {
  printf("\n");
  printf("Show or update your saved location\n");
  printf("\n");
  printf("Usage: muslimtify location [options]\n");
  printf("\n");
  printf("Commands:\n");
  printf("  %-25s %s\n", "set [options]", "Update saved location (see 'location set --help')");
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
}

int handle_location(int argc, char **argv) {
  if (argc > 0 && strcmp(argv[0], "set") == 0)
    return location_set_handler(argc - 1, argv + 1);

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

  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }

  switch (mode) {
  case OUTPUT_JSON:
    display_location_json(&cfg);
    break;
  case OUTPUT_HEADLESS:
    display_location_headless(&cfg);
    break;
  default:
    display_location(&cfg);
    break;
  }
  return 0;
}
