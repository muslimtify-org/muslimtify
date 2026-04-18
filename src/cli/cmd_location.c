#include "cache.h"
#include "cli_internal.h"
#include "display.h"
#include "location.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

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

static int location_auto_handler(int argc, char **argv) {
  (void)argc;
  (void)argv;

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

static int location_set_handler(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: muslimtify location set <latitude> <longitude>\n");
    return 1;
  }

  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }

  char *end_lat, *end_lon;
  errno = 0;
  cfg.latitude = strtod(argv[0], &end_lat);
  if (end_lat == argv[0] || *end_lat != '\0' || errno == ERANGE) {
    fprintf(stderr, "Error: Invalid latitude '%s'\n", argv[0]);
    return 1;
  }
  errno = 0;
  cfg.longitude = strtod(argv[1], &end_lon);
  if (end_lon == argv[1] || *end_lon != '\0' || errno == ERANGE) {
    fprintf(stderr, "Error: Invalid longitude '%s'\n", argv[1]);
    return 1;
  }
  cfg.auto_detect = false;

  if (config_save(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to save config\n");
    return 1;
  }

  cache_invalidate();
  printf("✓ Location set to: %.4f, %.4f\n", cfg.latitude, cfg.longitude);
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
