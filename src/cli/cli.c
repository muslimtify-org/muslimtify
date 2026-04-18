#include "cli.h"
#include "cli_internal.h"
#include "prayertimes.h"
#include "version.h"
#include <stdio.h>

// â”€â”€ top-level dispatch table
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

static const CommandEntry top_commands[] = {
    {"show", handle_show},         {"check", handle_check},
    {"next", handle_next},         {"config", handle_config},
    {"location", handle_location}, {"enable", handle_enable},
    {"disable", handle_disable},   {"list", handle_list},
    {"reminder", handle_reminder}, {"daemon", handle_daemon},
    {"method", handle_method},     {"notification", handle_notification},
    {"sound", handle_sound},       {"version", handle_version},
    {"--version", handle_version}, {"-v", handle_version},
    {"help", handle_help},         {"--help", handle_help},
    {"-h", handle_help},
};

// â”€â”€ version / help
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

int handle_version(int argc, char **argv) {
  (void)argc;
  (void)argv;

  printf("Muslimtify v%s\n", MUSLIMTIFY_VERSION);
  printf("Prayer Time Notification Daemon\n\n");
  Config cfg;
  if (config_load(&cfg) == 0) {
    CalcMethod m = method_from_string(cfg.calculation_method);
    const MethodParams *p = method_params_get(m);
    printf("Method: %s", cfg.calculation_method);
    if (p)
      printf(" (%s)", p->name);
    printf("\n");
  } else {
    printf("Method: kemenag (KEMENAG, Indonesia)\n");
  }

  return 0;
}

int handle_help(int argc, char **argv) {
  (void)argc;
  (void)argv;

  cli_print_help();
  return 0;
}

// â”€â”€ public API
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

void cli_print_help(void) {
  printf("Muslimtify - Prayer Time Notification Daemon\n\n");
  printf("Usage: muslimtify [COMMAND] [OPTIONS]\n\n");
  printf("Commands:\n");
  printf("  show              Display today's prayer times\n");
  printf("    --no-header       Plain key=value output (enabled prayers only)\n");
  printf("    --format json     JSON output\n");
  printf("  check             Check and send notification if prayer time\n");
  printf("  next              Show time until next prayer\n");
  printf("  next name         Print next prayer name only (e.g. Ashr)\n");
  printf("  next time         Print next prayer time only (e.g. 12:05)\n");
  printf("  next remaining    Print time remaining only (e.g. 1:23 or 23m)\n");
  printf("  config            Show configuration\n");
  printf("  location          Manage location [show|auto|set|clear|refresh]\n");
  printf("  method            Manage calculation method [show|set|list|madhab]\n");
  printf("  enable <prayer>   Enable prayer notification\n");
  printf("  disable <prayer>  Disable prayer notification\n");
  printf("  list              List prayer notification status\n");
  printf("  reminder          Manage prayer reminders\n");
#ifdef _WIN32
  printf("  daemon            Manage scheduled task "
         "[install|uninstall|status]\n");
#else
  printf("  daemon            Manage systemd daemon "
         "[install|uninstall|status]\n");
#endif
  printf("  notification test Send a test notification for the next prayer\n");
  printf("  sound             Manage notification sound "
         "[on|off|status|set|reminder-set]\n");
  printf("  version           Show version information\n");
  printf("  help              Show this help message\n\n");
  printf("Examples:\n");
  printf("  muslimtify                    # Show version and help\n");
  printf("  muslimtify next               # Show next prayer\n");
  printf("  muslimtify location auto      # Auto-detect location\n");
  printf("  muslimtify method list        # List available methods\n");
  printf("  muslimtify method set mwl     # Set calculation method\n");
  printf("  muslimtify method madhab hanafi  # Set madhab\n");
  printf("  muslimtify enable fajr        # Enable Fajr notifications\n");
  printf("  muslimtify reminder fajr 30,15,5  # Set Fajr reminders\n\n");
  printf("Config file: %s\n", config_get_path());
}

int cli_run(int argc, char **argv) {
  if (argc < 2) {
    handle_version(0, NULL);
    printf("\n");
    cli_print_help();
    return 0;
  }

  const char *cmd = argv[1];
  const CommandEntry *entry = dispatch_lookup(top_commands, DISPATCH_N(top_commands), cmd);
  if (entry)
    return entry->handler(argc - 2, argv + 2);

  fprintf(stderr, "Unknown command '%s'. Use 'muslimtify help' for usage.\n", cmd);
  return 1;
}
