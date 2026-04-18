#include "cache.h"
#include "cli_internal.h"
#include "location.h"
#include "method_detect.h"
#include "prayertimes.h"
#include "string_util.h"
#include <stdio.h>
#include <string.h>

static int method_show_handler(int argc, char **argv) {
  (void)argc;
  (void)argv;

  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }

  CalcMethod method = method_from_string(cfg.calculation_method);
  const MethodParams *p = method_params_get(method);

  printf("Calculation Method: %s", cfg.calculation_method);
  if (p)
    printf(" (%s)", p->name);
  printf("\n");

  printf("Madhab: %s", cfg.madhab);
  if (strcmp(cfg.madhab, "hanafi") == 0)
    printf(" (Hanafi)");
  else
    printf(" (Shafi'i)");
  printf("\n");

  return 0;
}

static int method_set_handler(int argc, char **argv) {
  if (argc < 1) {
    fprintf(stderr, "Usage: muslimtify method set <name>\n");
    fprintf(stderr, "Run 'muslimtify method list' to see available methods.\n");
    return 1;
  }

  CalcMethod method = method_from_string(argv[0]);
  const MethodParams *p = method_params_get(method);

  // Verify the name is valid (method_from_string falls back to kemenag)
  if (strcmp(argv[0], method_to_string(method)) != 0) {
    fprintf(stderr, "Error: Unknown method '%s'\n", argv[0]);
    fprintf(stderr, "Run 'muslimtify method list' to see available methods.\n");
    return 1;
  }

  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }

  copy_string(cfg.calculation_method, sizeof(cfg.calculation_method), argv[0]);

  if (config_save(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to save config\n");
    return 1;
  }

  cache_invalidate();
  printf("✓ Method set to: %s", argv[0]);
  if (p)
    printf(" (%s)", p->name);
  printf("\n");
  return 0;
}

static int method_madhab_handler(int argc, char **argv) {
  if (argc < 1) {
    fprintf(stderr, "Usage: muslimtify method madhab <shafi|hanafi>\n");
    return 1;
  }

  if (strcmp(argv[0], "shafi") != 0 && strcmp(argv[0], "hanafi") != 0) {
    fprintf(stderr, "Error: Unknown madhab '%s'. Use 'shafi' or 'hanafi'.\n", argv[0]);
    return 1;
  }

  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }

  copy_string(cfg.madhab, sizeof(cfg.madhab), argv[0]);

  if (config_save(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to save config\n");
    return 1;
  }

  cache_invalidate();
  printf("✓ Madhab set to: %s", argv[0]);
  if (strcmp(argv[0], "hanafi") == 0)
    printf(" (Hanafi)");
  else
    printf(" (Shafi'i)");
  printf("\n");
  return 0;
}

static int method_list_handler(int argc, char **argv) {
  (void)argc;
  (void)argv;

  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }

  printf("Available calculation methods:\n\n");

  for (int i = 0; i < CALC_CUSTOM; i++) {
    const MethodParams *p = method_params_get((CalcMethod)i);
    const char *key = method_to_string((CalcMethod)i);
    bool current = strcmp(cfg.calculation_method, key) == 0;

    printf("  %-14s %s %s\n", key, current ? "*" : " ", p ? p->name : "");
  }

  printf("\n* = current method\n");
  return 0;
}

static int method_auto_handler(int argc, char **argv) {
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

  if (cfg.city[0] != '\0') {
    printf("✓ Location detected: %s, %s\n", cfg.city, cfg.country);
  } else {
    printf("✓ Location detected: %.4f, %.4f\n", cfg.latitude, cfg.longitude);
  }

  CalcMethod detected = method_detect_by_country(cfg.country);
  const char *method_key = method_to_string(detected);
  const MethodParams *p = method_params_get(detected);

  copy_string(cfg.calculation_method, sizeof(cfg.calculation_method), method_key);

  if (config_save(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to save config\n");
    return 1;
  }

  cache_invalidate();
  printf("✓ Method set to: %s", method_key);
  if (p)
    printf(" (%s)", p->name);
  printf("\n");
  return 0;
}

static const CommandEntry method_commands[] = {
    {"show", method_show_handler}, {"set", method_set_handler},       {"auto", method_auto_handler},
    {"list", method_list_handler}, {"madhab", method_madhab_handler},
};

int handle_method(int argc, char **argv) {
  if (argc > 0) {
    const CommandEntry *sub =
        dispatch_lookup(method_commands, DISPATCH_N(method_commands), argv[0]);
    if (sub)
      return sub->handler(argc - 1, argv + 1);

    // If first arg is not a subcommand, treat it as "method set <name>"
    return method_set_handler(argc, argv);
  }

  return method_show_handler(0, NULL);
}
