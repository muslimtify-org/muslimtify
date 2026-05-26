#include "cli_internal.h"
#include "config.h"
#include "string_util.h"
#include <stdio.h>
#include <string.h>

// Valid sound presets. Keep this list in sync with the mappings in
// notification.c (Linux) and notification_win.c (Windows).
static int is_valid_preset(const char *name) {
  if (!name)
    return 0;
  return strcmp(name, "reminder") == 0 || strcmp(name, "alarm") == 0 ||
         strcmp(name, "default") == 0;
}

static int save_cfg_or_fail(const Config *cfg) {
  if (config_save(cfg) != 0) {
    fprintf(stderr, "Error: Failed to save config\n");
    return 1;
  }
  return 0;
}

static void print_status(const Config *cfg) {
  printf("Sound:           %s\n", cfg->notification_sound ? "on" : "off");
  printf("Alarm preset:    %s  (it's-time notification)\n", cfg->notification_sound_alarm);
  printf("Reminder preset: %s  (pre-prayer reminder notifications)\n",
         cfg->notification_sound_reminder);
  printf("\nAvailable presets: reminder, alarm, default\n");
}

static int sound_on(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }
  cfg.notification_sound = true;
  if (save_cfg_or_fail(&cfg) != 0)
    return 1;
  printf("Notification sound enabled\n");
  return 0;
}

static int sound_off(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }
  cfg.notification_sound = false;
  if (save_cfg_or_fail(&cfg) != 0)
    return 1;
  printf("Notification sound disabled\n");
  return 0;
}

static int sound_status(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }
  print_status(&cfg);
  return 0;
}

// muslimtify sound set <preset>   → set alarm preset (the "it's time" sound)
static int sound_set(int argc, char **argv) {
  if (argc < 1) {
    fprintf(stderr, "Usage: muslimtify sound set <reminder|alarm|default>\n");
    return 1;
  }
  const char *preset = argv[0];
  if (!is_valid_preset(preset)) {
    fprintf(stderr, "Error: Invalid preset '%s'. Valid: reminder, alarm, default\n", preset);
    return 1;
  }
  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }
  if (!copy_string(cfg.notification_sound_alarm, sizeof(cfg.notification_sound_alarm), preset)) {
    fprintf(stderr, "Error: Preset name too long\n");
    return 1;
  }
  if (save_cfg_or_fail(&cfg) != 0)
    return 1;
  printf("Alarm sound preset set to '%s'\n", preset);
  return 0;
}

// muslimtify sound reminder-set <preset>  → set reminder preset (pre-prayer sound)
static int sound_reminder_set(int argc, char **argv) {
  if (argc < 1) {
    fprintf(stderr, "Usage: muslimtify sound reminder-set <reminder|alarm|default>\n");
    return 1;
  }
  const char *preset = argv[0];
  if (!is_valid_preset(preset)) {
    fprintf(stderr, "Error: Invalid preset '%s'. Valid: reminder, alarm, default\n", preset);
    return 1;
  }
  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }
  if (!copy_string(cfg.notification_sound_reminder, sizeof(cfg.notification_sound_reminder),
                   preset)) {
    fprintf(stderr, "Error: Preset name too long\n");
    return 1;
  }
  if (save_cfg_or_fail(&cfg) != 0)
    return 1;
  printf("Reminder sound preset set to '%s'\n", preset);
  return 0;
}

static const CommandEntry sound_commands[] = {
    {"on", sound_on},       {"off", sound_off}, {"status", sound_status},
    {"show", sound_status}, {"set", sound_set}, {"reminder-set", sound_reminder_set},
};

int handle_sound(int argc, char **argv) {
  if (argc == 0) {
    return sound_status(0, NULL);
  }

  const CommandEntry *sub = dispatch_lookup(sound_commands, DISPATCH_N(sound_commands), argv[0]);
  if (sub)
    return sub->handler(argc - 1, argv + 1);

  fprintf(stderr, "Error: Unknown sound subcommand '%s'\n", argv[0]);
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "  muslimtify sound [status]          Show current sound settings\n");
  fprintf(stderr, "  muslimtify sound on|off            Enable/disable notification sound\n");
  fprintf(stderr, "  muslimtify sound set <preset>      Set alarm sound preset\n");
  fprintf(stderr, "  muslimtify sound reminder-set <preset>\n");
  fprintf(stderr, "                                     Set reminder sound preset\n");
  fprintf(stderr, "\nPresets: reminder, alarm, default\n");
  return 1;
}
