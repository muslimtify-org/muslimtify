#include "cache.h"
#include "cli_internal.h"
#include "display.h"
#include "location.h"
#include "prayertimes.h"
#include "string_util.h"
#include <stdio.h>
#include <string.h>

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

static int config_auto_handler(int argc, char **argv) {
  const char *override_city = NULL;
  for (int i = 0; i < argc; ++i) {
    if (strncmp(argv[i], "--city=", 7) == 0) {
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
    } else {
      fprintf(stderr, "Error: unexpected argument '%s'\n", argv[i]);
      fprintf(stderr, "Usage: muslimtify config auto [--city=<name>]\n");
      return 1;
    }
  }

  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }

  printf("Detecting location...\n");
  if (config_auto_detect(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to detect location\n");
    return 1;
  }

  // ipinfo's city guess is not auto-filled; apply the optional user label.
  cfg.city[0] = '\0';
  if (override_city)
    copy_string(cfg.city, sizeof(cfg.city), override_city);

  cfg.auto_detect = true;
  if (config_save(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to save config\n");
    return 1;
  }
  cache_invalidate();

  printf("✓ Location detected: ");
  if (cfg.city[0] != '\0')
    printf("%s, %s\n", cfg.city, cfg.country);
  else
    printf("%.4f, %.4f\n", cfg.latitude, cfg.longitude);

  CalcMethod m = method_from_string(cfg.calculation_method);
  const MethodParams *p = method_params_get(m);
  printf("✓ Method auto-detected: %s", cfg.calculation_method);
  if (p)
    printf(" (%s)", p->name);
  printf("\n");
  return 0;
}

static const CommandEntry config_commands[] = {
    {"show", config_show_handler},
    {"reset", config_reset_handler},
    {"validate", config_validate_handler},
    {"auto", config_auto_handler},
};

int handle_config(int argc, char **argv) {
  if (argc > 0) {
    const CommandEntry *sub =
        dispatch_lookup(config_commands, DISPATCH_N(config_commands), argv[0]);
    if (sub)
      return sub->handler(argc - 1, argv + 1);

    fprintf(stderr, "Error: Unknown config subcommand '%s'\n", argv[0]);
    fprintf(stderr, "Usage: muslimtify config [show|reset|validate|auto]\n");
    return 1;
  }

  return config_show_handler(0, NULL);
}
