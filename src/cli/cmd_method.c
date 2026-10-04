#include "cache.h"
#include "cli_internal.h"
#include "country.h"
#include "location.h"
#include "prayertimes.h"
#include "string_util.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static void print_method_list(FILE *out, const Muslimtify *mt) {
  MuslimtifyMethodInfo current;
  const char *current_key = muslimtify_get_method(mt, &current) == MUSLIMTIFY_OK ? current.key : "";

  fprintf(out, "Available calculation methods:\n\n");
  size_t count = muslimtify_method_count();
  for (size_t i = 0; i < count; i++) {
    MuslimtifyMethodInfo info;
    if (muslimtify_method_at(i, &info) != MUSLIMTIFY_OK)
      continue;
    fprintf(out, "  %-14s %s %s\n", info.key, strcmp(current_key, info.key) == 0 ? "*" : " ",
            info.name);
  }
  fprintf(out, "\n* = current method\n");
}

static void print_method_help(void) {
  printf("\n");
  printf("Show or set the prayer time calculation method\n");
  printf("\n");
  printf("Usage: muslimtify method [<name>] [options]\n");
  printf("\n");
  printf("Commands:\n");
  printf("  %-25s %s\n", "<name>", "Set the calculation method");
  printf("  %-25s %s\n", "-h, --help", "Show this help");
  printf("\n");
  printf("Options:\n");
  printf("  %-25s %s\n", "--auto", "Auto-select the method from your country");
  printf("  %-25s %s\n", "--list", "List available methods");
  printf("\n");
  printf("Examples:\n");
  printf("  %-25s %s\n", "muslimtify method", "# Show the current method");
  printf("  %-25s %s\n", "muslimtify method mwl", "# Set calculation method");
  printf("  %-25s %s\n", "muslimtify method --auto", "# Auto-select from country");
  printf("  %-25s %s\n", "muslimtify method --list", "# List available methods");
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

static int method_show_current(Muslimtify *mt) {
  MuslimtifyMethodInfo info;
  MuslimtifyError err = muslimtify_get_method(mt, &info);
  if (err != MUSLIMTIFY_OK)
    return cli_fail(err);
  printf("Calculation Method: %s", info.key);
  if (info.name[0] != '\0')
    printf(" (%s)", info.name);
  printf("\n");
  return 0;
}

static int method_set(Muslimtify *mt, const char *name) {
  MuslimtifyError err = muslimtify_set_method(mt, name);
  if (err == MUSLIMTIFY_ERR_UNKNOWN_METHOD) {
    cli_fail_value(err, name);
    print_method_list(stderr, mt);
    return 1;
  }
  if (err == MUSLIMTIFY_OK)
    err = muslimtify_save(mt);
  if (err != MUSLIMTIFY_OK)
    return cli_fail(err);

  MuslimtifyMethodInfo info;
  printf("Method set to: %s", name);
  if (muslimtify_get_method(mt, &info) == MUSLIMTIFY_OK && info.name[0] != '\0')
    printf(" (%s)", info.name);
  printf("\n");
  return 0;
}

int handle_method(int argc, char **argv) {
  if (cli_wants_help(argc, argv)) {
    print_method_help();
    return 0;
  }

  // Removed subcommands -> migration hints.
  if (argc > 0 && strcmp(argv[0], "show") == 0) {
    fprintf(stderr, "Error: 'method show' was removed; use 'method' to show the current method\n");
    return 1;
  }
  if (argc > 0 && strcmp(argv[0], "set") == 0) {
    fprintf(stderr, "Error: 'method set' was removed; use 'method <name>' directly\n");
    return 1;
  }
  if (argc > 0 && strcmp(argv[0], "list") == 0) {
    fprintf(stderr, "Error: 'method list' was removed; use 'method --list'\n");
    return 1;
  }
  if (argc > 0 && strcmp(argv[0], "madhab") == 0) {
    fprintf(stderr, "Error: 'method madhab' was removed; use 'madzhab <name>'\n");
    return 1;
  }

  if (argc > 0 && cli_reject_extra_args("method", argc - 1, argv + 1))
    return 1;

  // Detects the location over the network when no country is known, so it
  // stays on the core config until that moves into the library.
  if (argc > 0 && strcmp(argv[0], "--auto") == 0)
    return method_auto();

  Muslimtify *mt = NULL;
  if (cli_open(&mt))
    return 1;
  int ret;
  if (argc == 0) {
    ret = method_show_current(mt);
  } else if (strcmp(argv[0], "--list") == 0) {
    print_method_list(stdout, mt);
    ret = 0;
  } else {
    ret = method_set(mt, argv[0]);
  }
  muslimtify_close(mt);
  return ret;
}

static void print_madzhab_help(void) {
  printf("\n");
  printf("Show or set the madzhab (affects Asr calculation)\n");
  printf("\n");
  printf("Usage: muslimtify madzhab [<shafi|hanafi>] [options]\n");
  printf("\n");
  printf("Commands:\n");
  printf("  %-25s %s\n", "<name>", "Set the madzhab (shafi or hanafi)");
  printf("  %-25s %s\n", "-h, --help", "Show this help");
  printf("\n");
  printf("Options:\n");
  printf("  %-25s %s\n", "--list", "List available madzhab options");
  printf("\n");
  printf("Examples:\n");
  printf("  %-25s %s\n", "muslimtify madzhab", "# Show the current madzhab");
  printf("  %-25s %s\n", "muslimtify madzhab hanafi", "# Set madzhab");
  printf("  %-25s %s\n", "muslimtify madzhab --list", "# List madzhab options");
}

static int madzhab_run(Muslimtify *mt, int argc, char **argv) {
  MuslimtifyMadhab current = muslimtify_get_madhab(mt);

  if (argc == 0) {
    printf("Madzhab: %s (%s)\n", muslimtify_madhab_key(current), muslimtify_madhab_name(current));
    return 0;
  }

  if (strcmp(argv[0], "--list") == 0) {
    printf("Available madzhab:\n\n");
    for (int m = MUSLIMTIFY_MADHAB_SHAFI; m <= MUSLIMTIFY_MADHAB_HANAFI; m++) {
      printf("  %-8s %s %s\n", muslimtify_madhab_key((MuslimtifyMadhab)m),
             (int)current == m ? "*" : " ", muslimtify_madhab_name((MuslimtifyMadhab)m));
    }
    printf("\n* = current madzhab\n");
    return 0;
  }

  MuslimtifyMadhab madhab;
  if (muslimtify_parse_madhab(argv[0], &madhab) != MUSLIMTIFY_OK) {
    fprintf(stderr, "Error: Unknown madzhab '%s'\n", argv[0]);
    fprintf(stderr, "  Available: shafi, hanafi\n");
    return 1;
  }

  MuslimtifyError err = muslimtify_set_madhab(mt, madhab);
  if (err == MUSLIMTIFY_OK)
    err = muslimtify_save(mt);
  if (err != MUSLIMTIFY_OK)
    return cli_fail(err);
  printf("Madzhab set to: %s (%s)\n", muslimtify_madhab_key(madhab),
         muslimtify_madhab_name(madhab));
  return 0;
}

int handle_madzhab(int argc, char **argv) {
  if (cli_wants_help(argc, argv)) {
    print_madzhab_help();
    return 0;
  }
  if (cli_reject_extra_args("madzhab", argc - 1, argv + 1))
    return 1;

  Muslimtify *mt = NULL;
  if (cli_open(&mt))
    return 1;
  int ret = madzhab_run(mt, argc, argv);
  muslimtify_close(mt);
  return ret;
}
