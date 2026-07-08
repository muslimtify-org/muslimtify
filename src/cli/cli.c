#include "cli.h"
#include "cli_internal.h"
#include "config.h"
#include "prayertimes.h"
#include "version.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

int cli_parse_output_mode(int argc, char **argv, OutputMode *out) {
  bool json = false, headless = false;
  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "--json") == 0)
      json = true;
    else if (strcmp(argv[i], "--headless") == 0)
      headless = true;
  }
  if (json && headless) {
    fprintf(stderr, "Error: --json and --headless cannot be combined\n");
    return 1;
  }
  *out = json ? OUTPUT_JSON : (headless ? OUTPUT_HEADLESS : OUTPUT_TABLE);
  return 0;
}

bool cli_wants_help(int argc, char **argv) {
  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
      return true;
  }
  return false;
}

// --- top-level dispatch table -----------------------

static const CommandEntry top_commands[] = {
    {"show", handle_show},         {"check", handle_check},
    {"config", handle_config},     {"location", handle_location},
    {"enable", handle_enable},     {"disable", handle_disable},
    {"list", handle_list},         {"reminder", handle_reminder},
    {"offset", handle_offset},     {"daemon", handle_daemon},
    {"method", handle_method},     {"notification", handle_notification},
    {"sound", handle_sound},       {"version", handle_version},
    {"--version", handle_version}, {"-v", handle_version},
    {"help", handle_help},         {"--help", handle_help},
    {"-h", handle_help},
};

// --- version / help -----------------------

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

// --- public API -----------------------

void cli_print_help(void) {
  printf("Muslimtify - Cross-platform Prayer Time Notification Daemon\n\n");

  printf("Usage:\n");
  printf("  muslimtify <command> [options]\n\n");

  /*
   * PRAYER
   */
  printf("Prayer Commands:\n");

  printf("  show\tDisplay today's prayer times\n");

  printf("\t--headless\tPlain key=value output\n");

  printf("\t--json\tOutput prayer times as JSON\n");

  printf("\t--next\tShow next prayer\n");

  printf("  %-30s %s\n", "check", "Check and send notifications");

  printf("\n");

  /*
   * LOCATION
   */
  printf("Location Commands:\n");

  printf("  %-30s %s\n", "location show", "Show current location");

  printf("  %-30s %s\n", "location set", "Set coordinates manually");

  printf("  %-30s %s\n", "", "--lat=<latitude>");

  printf("  %-30s %s\n", "", "--long=<longitude>");

  printf("  %-30s %s\n", "", "--timezone=<iana>");

  printf("  %-30s %s\n", "", "--city=<name>");

  printf("  %-30s %s\n", "", "--country=<iso2>");

  printf("  %-30s %s\n", "location refresh", "Refresh auto-detected location");

  printf("  %-30s %s\n", "location clear", "Clear saved location");

  printf("\n");

  /*
   * CONFIG
   */
  printf("Configuration Commands:\n");

  printf("  %-30s %s\n", "config show", "Show current configuration");

  printf("  %-30s %s\n", "config validate", "Validate configuration");

  printf("  %-30s %s\n", "config reset", "Reset configuration");

  printf("  %-30s %s\n", "config auto", "Auto-detect location and method");

  printf("  %-30s %s\n", "", "Uses ipinfo.io for detection");

  printf("\n");

  /*
   * METHOD
   */
  printf("Calculation Method Commands:\n");

  printf("  %-30s %s\n", "method list", "List available methods");

  printf("  %-30s %s\n", "method show", "Show current method");

  printf("  %-30s %s\n", "method set <method>", "Set calculation method");

  printf("  %-30s %s\n", "method madhab <name>", "Set madhab");

  printf("\n");

  /*
   * NOTIFICATION
   */
  printf("Notification Commands:\n");

  printf("  %-30s %s\n", "enable <prayer>", "Enable prayer notification");

  printf("  %-30s %s\n", "disable <prayer>", "Disable prayer notification");

  printf("  %-30s %s\n", "list", "List notification status");

  printf("  %-30s %s\n", "notification test", "Send test notification");

  printf("\n");

  /*
   * REMINDER
   */
  printf("Reminder Commands:\n");

  printf("  %-30s %s\n", "reminder show", "Show configured reminders");

  printf("  %-30s %s\n", "reminder <prayer> <list>", "Set reminders (30,15,5)");

  printf("  %-30s %s\n", "reminder <prayer> clear", "Clear reminders");

  printf("  %-30s %s\n", "reminder all <list>", "Apply reminders to all prayers");

  printf("\n");

  /*
   * OFFSET
   */
  printf("Offset Commands:\n");

  printf("  %-30s %s\n", "offset <prayer> <±min>", "Adjust a prayer time (-60..60)");

  printf("  %-30s %s\n", "offset all <±min>", "Adjust all prayer times");

  printf("\n");

  /*
   * SOUND
   */
  printf("Sound Commands:\n");

  printf("  %-30s %s\n", "sound on", "Enable notification sound");

  printf("  %-30s %s\n", "sound off", "Disable notification sound");

  printf("  %-30s %s\n", "sound status", "Show sound status");

  printf("  %-30s %s\n", "sound set", "Set adhan sound");

  printf("  %-30s %s\n", "sound reminder-set", "Set reminder sound");

  printf("\n");

  /*
   * DAEMON
   */
#ifdef _WIN32
  printf("Scheduled Task Commands:\n");
#else
  printf("Daemon Commands:\n");
#endif

  printf("  %-30s %s\n", "daemon install", "Install daemon");

  printf("  %-30s %s\n", "daemon uninstall", "Remove daemon");

  printf("  %-30s %s\n", "daemon status", "Show daemon status");

  printf("\n");

  /*
   * GENERAL
   */
  printf("General Commands:\n");

  printf("  %-30s %s\n", "version", "Show version information");

  printf("  %-30s %s\n", "help", "Show help message");

  printf("\n");

  /*
   * EXAMPLES
   */
  printf("Examples:\n");

  printf("  %-55s %s\n", "muslimtify show --next", "# Show next prayer");

  printf("  %-55s %s\n", "muslimtify config auto", "# Auto detect configuration");

  printf("  %-55s %s\n", "muslimtify method set mwl", "# Set calculation method");

  printf("  %-55s %s\n", "muslimtify method madhab hanafi", "# Set madhab");

  printf("  %-55s %s\n", "muslimtify enable fajr", "# Enable Fajr notification");

  printf("  %-55s %s\n", "muslimtify reminder fajr 30,15,5", "# Set reminders");

  printf("  %-55s %s\n", "muslimtify offset fajr +4", "# Shift Fajr 4 min later");

  printf("  %-55s %s\n", "muslimtify location set --lat=-6.21 --long=106.84", "# Set coordinates");

  printf("  %-55s %s\n", "muslimtify location set --timezone=Asia/Jakarta", "# Override timezone");

  printf("\n");

  printf("Config File:\n");
  printf("  %s\n", config_get_path());
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
