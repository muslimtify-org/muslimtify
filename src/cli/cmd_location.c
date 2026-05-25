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

static int location_show_handler(int argc, char **argv) {
  (void)argc;
  (void)argv;

  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }
  display_location(&cfg);
  return 0;
}

// Parse `--city=<value>` or `--city <value>` out of argv. On success returns 0
// and writes the start of the value into *out_city. Returns 1 on a malformed
// flag (missing value), -1 if the flag is absent (out_city untouched).
static int parse_city_flag(int argc, char **argv, const char **out_city) {
  for (int i = 0; i < argc; ++i) {
    if (strncmp(argv[i], "--city=", 7) == 0) {
      const char *v = argv[i] + 7;
      if (*v == '\0') {
        fprintf(stderr, "Error: --city requires a value (e.g. --city=Jakarta)\n");
        return 1;
      }
      *out_city = v;
      return 0;
    } else if (strcmp(argv[i], "--city") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: --city requires a value (e.g. --city Jakarta)\n");
        return 1;
      }
      *out_city = argv[i + 1];
      return 0;
    }
  }
  return -1;
}

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

static int location_auto_handler(int argc, char **argv) {
  const char *override_city = NULL;
  int city_rc = parse_city_flag(argc, argv, &override_city);
  if (city_rc == 1)
    return 1;

  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }

  printf("Detecting location...\n");
  if (location_fetch(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to detect location\n");
    return 1;
  }

  // ipinfo no longer fills city — make it explicit opt-in. Clear whatever was
  // cached previously, then write the user override if one was provided.
  cfg.city[0] = '\0';
  if (override_city)
    set_city(&cfg, override_city);

  printf("✓ Location detected: ");
  if (cfg.city[0] != '\0') {
    printf("%s, %s\n", cfg.city, cfg.country);
  } else {
    printf("%.4f, %.4f\n", cfg.latitude, cfg.longitude);
  }
  printf("  Timezone: %s (UTC%+.1f)\n", cfg.timezone, cfg.timezone_offset);

  cfg.auto_detect = true;
  if (config_save(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to save config\n");
    return 1;
  }
  cache_invalidate();
  printf("✓ Saved to config\n");
  return 0;
}

static int location_refresh_handler(int argc, char **argv) {
  (void)argc;
  (void)argv;

  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }

  printf("Refreshing location...\n");
  if (location_fetch(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to fetch location\n");
    return 1;
  }
  printf("✓ Location detected: ");
  if (cfg.city[0] != '\0') {
    printf("%s, %s\n", cfg.city, cfg.country);
  } else {
    printf("%.4f, %.4f\n", cfg.latitude, cfg.longitude);
  }
  printf("  Timezone: %s (UTC%+.1f)\n", cfg.timezone, cfg.timezone_offset);

  if (config_save(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to save config\n");
    return 1;
  }
  cache_invalidate();
  printf("✓ Saved to config\n");
  return 0;
}

static const char *LOCATION_SET_USAGE =
    "Usage: muslimtify location set <latitude> <longitude> [--timezone=<iana>] "
    "[--city=<name>] [--country=<iso2>]\n";

// Returns true if `tz` is one of the canonical UTC aliases (so an offset of 0.0
// is expected, not a sign of an unrecognized zone).
static bool is_utc_zone(const char *tz) {
  return strcmp(tz, "UTC") == 0 || strcmp(tz, "Etc/UTC") == 0 || strcmp(tz, "Etc/GMT") == 0 ||
         strcmp(tz, "GMT") == 0;
}

static int location_set_handler(int argc, char **argv) {
  const char *override_tz = NULL;
  const char *override_city = NULL;
  const char *override_country = NULL;
  const char *positional[2] = {NULL, NULL};
  int pos_count = 0;

  for (int i = 0; i < argc; ++i) {
    if (strncmp(argv[i], "--timezone=", 11) == 0) {
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
    } else if (pos_count < 2) {
      positional[pos_count++] = argv[i];
    } else {
      fprintf(stderr, "Error: unexpected argument '%s'\n%s", argv[i], LOCATION_SET_USAGE);
      return 1;
    }
  }

  if (pos_count < 2) {
    fputs(LOCATION_SET_USAGE, stderr);
    return 1;
  }

  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }

  char *end_lat, *end_lon;
  errno = 0;
  cfg.latitude = strtod(positional[0], &end_lat);
  if (end_lat == positional[0] || *end_lat != '\0' || errno == ERANGE) {
    fprintf(stderr, "Error: Invalid latitude '%s'\n", positional[0]);
    return 1;
  }
  errno = 0;
  cfg.longitude = strtod(positional[1], &end_lon);
  if (end_lon == positional[1] || *end_lon != '\0' || errno == ERANGE) {
    fprintf(stderr, "Error: Invalid longitude '%s'\n", positional[1]);
    return 1;
  }
  cfg.auto_detect = false;

  // The user picked coordinates manually — the previously cached city/country
  // no longer apply. Clear them, then write the user's --city/--country
  // overrides if any.
  cfg.city[0] = '\0';
  cfg.country[0] = '\0';
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
  } else {
    // No override — re-derive from the host OS so the offset stays correct
    // even after manual coords (avoids inheriting a stale ipinfo-derived zone).
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
  printf("✓ Location set to: %.4f, %.4f\n", cfg.latitude, cfg.longitude);
  if (cfg.city[0] != '\0')
    printf("  City: %s\n", cfg.city);
  if (cfg.country[0] != '\0')
    printf("  Country: %s\n", cfg.country);
  if (override_tz) {
    printf("  Timezone: %s (UTC%+.1f) [override]\n", cfg.timezone, cfg.timezone_offset);
  } else {
    printf("  Timezone: %s (UTC%+.1f) [from system OS]\n", cfg.timezone, cfg.timezone_offset);
    printf("  Hint: pass --timezone=<iana> if the coordinates are in a different region\n");
    printf("        (e.g. --timezone=Asia/Jakarta)\n");
  }
  return 0;
}

static int location_clear_handler(int argc, char **argv) {
  (void)argc;
  (void)argv;

  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }

  cfg.latitude = 0.0;
  cfg.longitude = 0.0;
  cfg.auto_detect = true;
  cfg.city[0] = '\0';
  cfg.country[0] = '\0';

  if (config_save(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to save config\n");
    return 1;
  }

  cache_invalidate();
  printf("✓ Location cleared. Will auto-detect on next run.\n");
  return 0;
}

static const CommandEntry location_commands[] = {
    {"show", location_show_handler},       {"auto", location_auto_handler},
    {"set", location_set_handler},         {"clear", location_clear_handler},
    {"refresh", location_refresh_handler},
};

int handle_location(int argc, char **argv) {
  if (argc > 0) {
    const CommandEntry *sub =
        dispatch_lookup(location_commands, DISPATCH_N(location_commands), argv[0]);
    if (sub)
      return sub->handler(argc - 1, argv + 1);

    fprintf(stderr, "Error: Unknown location subcommand '%s'\n", argv[0]);
    return 1;
  }

  return location_show_handler(0, NULL);
}
