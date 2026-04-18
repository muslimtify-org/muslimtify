#include "cache.h"
#include "cli_internal.h"
#include "display.h"
#include <stdio.h>

static int config_show_handler(int argc, char **argv) {
  (void)argc;
  (void)argv;

  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }
  display_config(&cfg);
  return 0;
}

static int config_reset_handler(int argc, char **argv) {
  (void)argc;
  (void)argv;

  Config cfg = config_default();
  if (config_save(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to save config\n");
    return 1;
  }
  cache_invalidate();
  printf("✓ Configuration reset to defaults\n");
  printf("  Config file: %s\n", config_get_path());
  return 0;
}

static int config_validate_handler(int argc, char **argv) {
  (void)argc;
  (void)argv;

  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }
  if (config_validate(&cfg)) {
    printf("✓ Configuration is valid\n");
    return 0;
  } else {
    fprintf(stderr, "✗ Configuration has errors\n");
    return 1;
  }
}

static const CommandEntry config_commands[] = {
    {"show", config_show_handler},
    {"reset", config_reset_handler},
    {"validate", config_validate_handler},
};

int handle_config(int argc, char **argv) {
  if (argc > 0) {
    const CommandEntry *sub =
        dispatch_lookup(config_commands, DISPATCH_N(config_commands), argv[0]);
    if (sub)
      return sub->handler(argc - 1, argv + 1);

    fprintf(stderr, "Error: Unknown config subcommand '%s'\n", argv[0]);
    return 1;
  }

  fprintf(stderr, "Usage: muslimtify config [show|reset|validate]\n");
  return 1;
}
