#include "cache.h"
#include "cli_internal.h"
#include "display.h"
#include <stdio.h>

int handle_enable(int argc, char **argv) {
  if (argc < 1) {
    fprintf(stderr, "Usage: muslimtify enable <prayer>\n");
    fprintf(stderr, "       muslimtify enable all\n");
    return 1;
  }

  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }

  if (strcmp(argv[0], "all") == 0) {
    cfg.fajr.enabled = true;
    cfg.sunrise.enabled = true;
    cfg.dhuha.enabled = true;
    cfg.dhuhr.enabled = true;
    cfg.asr.enabled = true;
    cfg.maghrib.enabled = true;
    cfg.isha.enabled = true;

    if (config_save(&cfg) != 0) {
      fprintf(stderr, "Error: Failed to save config\n");
      return 1;
    }

    cache_invalidate();
    printf("✓ All prayers enabled\n");
    return 0;
  }

  PrayerConfig *prayer = config_get_prayer(&cfg, argv[0]);
  if (!prayer) {
    fprintf(stderr, "Error: Unknown prayer '%s'\n", argv[0]);
    return 1;
  }

  prayer->enabled = true;

  if (config_save(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to save config\n");
    return 1;
  }

  cache_invalidate();
  printf("✓ %s notifications enabled\n", argv[0]);
  return 0;
}

int handle_disable(int argc, char **argv) {
  if (argc < 1) {
    fprintf(stderr, "Usage: muslimtify disable <prayer>\n");
    fprintf(stderr, "       muslimtify disable all\n");
    return 1;
  }

  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }

  if (strcmp(argv[0], "all") == 0) {
    cfg.fajr.enabled = false;
    cfg.sunrise.enabled = false;
    cfg.dhuha.enabled = false;
    cfg.dhuhr.enabled = false;
    cfg.asr.enabled = false;
    cfg.maghrib.enabled = false;
    cfg.isha.enabled = false;

    if (config_save(&cfg) != 0) {
      fprintf(stderr, "Error: Failed to save config\n");
      return 1;
    }

    cache_invalidate();
    printf("✓ All prayers disabled\n");
    return 0;
  }

  PrayerConfig *prayer = config_get_prayer(&cfg, argv[0]);
  if (!prayer) {
    fprintf(stderr, "Error: Unknown prayer '%s'\n", argv[0]);
    return 1;
  }

  prayer->enabled = false;

  if (config_save(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to save config\n");
    return 1;
  }

  cache_invalidate();
  printf("✓ %s notifications disabled\n", argv[0]);
  return 0;
}

int handle_list(int argc, char **argv) {
  (void)argc;
  (void)argv;

  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }

  display_prayer_list(&cfg);
  return 0;
}

int handle_reminder(int argc, char **argv) {
  if (argc == 0 || strcmp(argv[0], "show") == 0) {
    Config cfg;
    if (config_load(&cfg) != 0) {
      fprintf(stderr, "Error: Failed to load config\n");
      return 1;
    }
    display_reminders(&cfg);
    return 0;
  }

  if (argc < 2) {
    fprintf(stderr, "Usage: muslimtify reminder <prayer> <time1,time2,...>\n");
    fprintf(stderr, "       muslimtify reminder <prayer> clear\n");
    fprintf(stderr, "       muslimtify reminder all <time1,time2,...>\n");
    fprintf(stderr, "       muslimtify reminder show\n");
    return 1;
  }

  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }

  const char *prayer_name = argv[0];
  const char *reminder_str = argv[1];

  if (strcmp(prayer_name, "all") == 0) {
    PrayerConfig *prayers[] = {&cfg.fajr, &cfg.sunrise, &cfg.dhuha, &cfg.dhuhr,
                               &cfg.asr,  &cfg.maghrib, &cfg.isha};

    int reminders[MAX_REMINDERS];
    int count = config_parse_reminders(reminder_str, reminders, MAX_REMINDERS);

    if (count < 0) {
      fprintf(stderr, "Error: Invalid reminder format\n");
      return 1;
    }

    for (int i = 0; i < 7; i++) {
      if (!prayers[i]->enabled)
        continue;

      prayers[i]->reminder_count = count;
      for (int j = 0; j < count; j++) {
        prayers[i]->reminders[j] = reminders[j];
      }
    }

    if (config_save(&cfg) != 0) {
      fprintf(stderr, "Error: Failed to save config\n");
      return 1;
    }

    cache_invalidate();
    if (count == 0) {
      printf("✓ Reminders cleared for all enabled prayers\n");
    } else {
      printf("✓ Reminders updated for all enabled prayers: %s\n", reminder_str);
    }
    return 0;
  }

  PrayerConfig *prayer = config_get_prayer(&cfg, prayer_name);
  if (!prayer) {
    fprintf(stderr, "Error: Unknown prayer '%s'\n", prayer_name);
    return 1;
  }

  int reminders[MAX_REMINDERS];
  int count = config_parse_reminders(reminder_str, reminders, MAX_REMINDERS);

  if (count < 0) {
    fprintf(stderr, "Error: Invalid reminder format\n");
    return 1;
  }

  prayer->reminder_count = count;
  for (int i = 0; i < count; i++) {
    prayer->reminders[i] = reminders[i];
  }

  if (config_save(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to save config\n");
    return 1;
  }

  cache_invalidate();
  if (count == 0) {
    printf("✓ Reminders cleared for %s\n", prayer_name);
  } else {
    printf("✓ Set %d reminder(s) for %s: %s\n", count, prayer_name, reminder_str);
  }

  return 0;
}
