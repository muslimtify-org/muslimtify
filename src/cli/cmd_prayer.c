#include "cache.h"
#include "cli_internal.h"
#include "display.h"
#include <stdio.h>
#include <stdlib.h>

int handle_offset(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: muslimtify offset <prayer> <minutes>\n");
    fprintf(stderr, "       muslimtify offset all <minutes>\n");
    fprintf(stderr, "       minutes is a signed integer from -60 to 60 (e.g. +4, -2, 0)\n");
    return 1;
  }

  const char *prayer_name = argv[0];
  const char *value_str = argv[1];

  char *end = NULL;
  long value = strtol(value_str, &end, 10);
  if (end == value_str || *end != '\0' || value < PRAYER_OFFSET_MIN || value > PRAYER_OFFSET_MAX) {
    fprintf(stderr, "Error: Offset must be an integer from %d to %d\n", PRAYER_OFFSET_MIN,
            PRAYER_OFFSET_MAX);
    return 1;
  }

  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }

  bool is_all = strcmp(prayer_name, "all") == 0;
  if (is_all) {
    PrayerConfig *prayers[] = {&cfg.fajr, &cfg.sunrise, &cfg.dhuha, &cfg.dhuhr,
                               &cfg.asr,  &cfg.maghrib, &cfg.isha};
    for (int i = 0; i < 7; i++) {
      prayers[i]->offset = (int)value;
    }
  } else {
    PrayerConfig *prayer = config_get_prayer(&cfg, prayer_name);
    if (!prayer) {
      fprintf(stderr, "Error: Unknown prayer '%s'\n", prayer_name);
      return 1;
    }
    prayer->offset = (int)value;
  }

  if (config_save(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to save config\n");
    return 1;
  }

  cache_invalidate();
  printf("✓ Offset set to %+d min for %s\n", (int)value, is_all ? "all prayers" : prayer_name);
  return 0;
}
