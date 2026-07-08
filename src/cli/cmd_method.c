#include "cache.h"
#include "cli_internal.h"
#include "country.h"
#include "location.h"
#include "prayertimes.h"
#include "string_util.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static void print_method_list(FILE *out, const Config *cfg) {
  fprintf(out, "Available calculation methods:\n\n");
  for (int i = 0; i < CALC_CUSTOM; i++) {
    const MethodParams *p = method_params_get((CalcMethod)i);
    const char *key = method_to_string((CalcMethod)i);
    bool current = strcmp(cfg->calculation_method, key) == 0;
    fprintf(out, "  %-14s %s %s\n", key, current ? "*" : " ", p ? p->name : "");
  }
  fprintf(out, "\n* = current method\n");
}

static void print_method_help(void) {
  printf("Usage: muslimtify method [<name>]\n");
  printf("  %-12s %s\n", "(no arg)", "Show the current method");
  printf("  %-12s %s\n", "<name>", "Set the calculation method");
  printf("  %-12s %s\n", "--auto", "Auto-select the method from your country");
  printf("  %-12s %s\n", "--list", "List available methods");
  printf("  %-12s %s\n", "-h, --help", "Show this help");
}

// Auto-select the calculation method from the country in config. If no country
// is set yet, detect the location first (which populates the country), then
// derive the method.
static int method_auto(void) {
  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }
  if (cfg.country[0] == '\0') {
    printf("Detecting location...\n");
    if (location_fetch(&cfg) != 0) {
      fprintf(stderr, "Error: Failed to detect location\n");
      return 1;
    }
    cfg.auto_detect = true;
  }
  CalcMethod m = country_default_method(cfg.country);
  copy_string(cfg.calculation_method, sizeof(cfg.calculation_method), method_to_string(m));
  if (config_save(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to save config\n");
    return 1;
  }
  cache_invalidate();
  const MethodParams *p = method_params_get(m);
  printf("Method auto-detected: %s", cfg.calculation_method);
  if (p)
    printf(" (%s)", p->name);
  printf("\n");
  return 0;
}

static int method_show_current(void) {
  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }
  CalcMethod m = method_from_string(cfg.calculation_method);
  const MethodParams *p = method_params_get(m);
  printf("Calculation Method: %s", cfg.calculation_method);
  if (p)
    printf(" (%s)", p->name);
  printf("\n");
  return 0;
}

static int method_set(const char *name) {
  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }
  CalcMethod m = method_from_string(name);
  // method_from_string falls back to CALC_CUSTOM for unknown names; a name that
  // does not round-trip is not a real method key.
  if (strcmp(name, method_to_string(m)) != 0) {
    fprintf(stderr, "Error: Unknown method '%s'\n", name);
    print_method_list(stderr, &cfg);
    return 1;
  }
  const MethodParams *p = method_params_get(m);
  copy_string(cfg.calculation_method, sizeof(cfg.calculation_method), name);
  if (config_save(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to save config\n");
    return 1;
  }
  cache_invalidate();
  printf("Method set to: %s", name);
  if (p)
    printf(" (%s)", p->name);
  printf("\n");
  return 0;
}

int handle_method(int argc, char **argv) {
  if (cli_wants_help(argc, argv)) {
    print_method_help();
    return 0;
  }
  if (argc == 0)
    return method_show_current();

  if (strcmp(argv[0], "--auto") == 0)
    return method_auto();

  if (strcmp(argv[0], "--list") == 0) {
    Config cfg;
    if (config_load(&cfg) != 0) {
      fprintf(stderr, "Error: Failed to load config\n");
      return 1;
    }
    print_method_list(stdout, &cfg);
    return 0;
  }

  // Removed subcommands -> migration hints.
  if (strcmp(argv[0], "show") == 0) {
    fprintf(stderr, "Error: 'method show' was removed; use 'method' to show the current method\n");
    return 1;
  }
  if (strcmp(argv[0], "set") == 0) {
    fprintf(stderr, "Error: 'method set' was removed; use 'method <name>' directly\n");
    return 1;
  }
  if (strcmp(argv[0], "list") == 0) {
    fprintf(stderr, "Error: 'method list' was removed; use 'method --list'\n");
    return 1;
  }
  if (strcmp(argv[0], "madhab") == 0) {
    fprintf(stderr, "Error: 'method madhab' was removed; use 'madzhab <name>'\n");
    return 1;
  }

  return method_set(argv[0]);
}

static const char *madzhab_label(const char *v) {
  return strcmp(v, "hanafi") == 0 ? "Hanafi" : "Shafi'i";
}

static void print_madzhab_help(void) {
  printf("Usage: muslimtify madzhab [<shafi|hanafi>]\n");
  printf("  %-12s %s\n", "(no arg)", "Show the current madzhab");
  printf("  %-12s %s\n", "<name>", "Set the madzhab (shafi or hanafi)");
  printf("  %-12s %s\n", "--list", "List available madzhab options");
  printf("  %-12s %s\n", "-h, --help", "Show this help");
}

int handle_madzhab(int argc, char **argv) {
  if (cli_wants_help(argc, argv)) {
    print_madzhab_help();
    return 0;
  }
  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }

  if (argc == 0) {
    printf("Madzhab: %s (%s)\n", cfg.madhab, madzhab_label(cfg.madhab));
    return 0;
  }

  if (strcmp(argv[0], "--list") == 0) {
    printf("Available madzhab:\n\n");
    printf("  %-8s %s %s\n", "shafi", strcmp(cfg.madhab, "shafi") == 0 ? "*" : " ", "Shafi'i");
    printf("  %-8s %s %s\n", "hanafi", strcmp(cfg.madhab, "hanafi") == 0 ? "*" : " ", "Hanafi");
    printf("\n* = current madzhab\n");
    return 0;
  }

  if (strcmp(argv[0], "shafi") != 0 && strcmp(argv[0], "hanafi") != 0) {
    fprintf(stderr, "Error: Unknown madzhab '%s'\n", argv[0]);
    fprintf(stderr, "  Available: shafi, hanafi\n");
    return 1;
  }

  copy_string(cfg.madhab, sizeof(cfg.madhab), argv[0]);
  if (config_save(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to save config\n");
    return 1;
  }
  cache_invalidate();
  printf("Madzhab set to: %s (%s)\n", argv[0], madzhab_label(argv[0]));
  return 0;
}
