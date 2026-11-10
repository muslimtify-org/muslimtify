#include "cli.h"
#include "cli_internal.h"
#include "util.h"
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

int cli_unknown_prayer(const char *name) {
  fprintf(stderr, "Error: Unknown prayer '%s'\n", name);
  fprintf(stderr, "  Available: fajr, dhuhr, asr, maghrib, isha\n");
  return 1;
}

int cli_fail(MuslimtifyError err) {
  fprintf(stderr, "Error: %s\n", muslimtify_get_error(err));
  return 1;
}

int cli_fail_value(MuslimtifyError err, const char *value) {
  fprintf(stderr, "Error: %s '%s'\n", muslimtify_get_error(err), value);
  return 1;
}

int cli_save(Muslimtify *mt, MuslimtifyError err) {
  if (err == MUSLIMTIFY_OK)
    err = muslimtify_save(mt);
  return err == MUSLIMTIFY_OK ? 0 : cli_fail(err);
}

int cli_save_location(Muslimtify *mt, MuslimtifyError err, MuslimtifyLocation *loc) {
  if (err == MUSLIMTIFY_OK)
    err = muslimtify_save(mt);
  if (err == MUSLIMTIFY_OK)
    err = muslimtify_get_location(mt, loc);
  return err == MUSLIMTIFY_OK ? 0 : cli_fail(err);
}

int cli_open(Muslimtify **mt) {
  MuslimtifyError err = muslimtify_open(mt);
  return err == MUSLIMTIFY_OK ? 0 : cli_fail(err);
}

void cli_print_gps_warning(const MuslimtifyDetection *detection) {
  const char *warning = muslimtify_detection_warning(detection);
  if (warning)
    fprintf(stderr, "%s\n", warning);
}

int cli_ensure_location(Muslimtify *mt) {
  if (!muslimtify_location_needs_detect(mt))
    return 0;

  fprintf(stderr, "Detecting location...\n");
  MuslimtifyDetection detection;
  MuslimtifyError err = muslimtify_detect_location(mt, &detection);
  if (err != MUSLIMTIFY_OK)
    return cli_fail(err);
  cli_print_gps_warning(&detection);

  MuslimtifyLocation loc;
  if (cli_save_location(mt, MUSLIMTIFY_OK, &loc))
    return 1;

  if (loc.city[0] != '\0')
    fprintf(stderr, "✓ Location detected: %s, %s\n", loc.city, loc.country);
  else
    fprintf(stderr, "✓ Location detected: %.4f, %.4f\n", loc.latitude, loc.longitude);
  return 0;
}

int cli_daemon_status(int argc, char **argv) {
  (void)argc;
  (void)argv;

  MuslimtifyDaemonStatus st;
  MuslimtifyError err = muslimtify_daemon_status(&st);
  if (err != MUSLIMTIFY_OK)
    return cli_fail(err);

  printf("Installed: %s\n", st.installed ? "yes" : "no");
  printf("Enabled:   %s\n", st.enabled ? "yes" : "no");
  printf("Running:   %s\n", st.running ? "yes" : "no");
  // Non-zero when it is not running, so a script can test for a live daemon.
  return st.running ? 0 : 1;
}

int cli_daemon_dispatch(const CommandEntry *commands, int count, void (*print_help)(void), int argc,
                        char **argv) {
  if (cli_wants_help(argc, argv)) {
    print_help();
    return 0;
  }
  if (argc > 0) {
    const CommandEntry *sub = dispatch_lookup(commands, count, argv[0]);
    if (sub) {
      // No daemon subcommand takes arguments. Checked before any of them runs.
      char command[64];
      snprintf(command, sizeof(command), "daemon %s", argv[0]);
      if (cli_reject_extra_args(command, argc - 1, argv + 1))
        return 1;
      return sub->handler(argc - 1, argv + 1);
    }

    fprintf(stderr, "Error: Unknown daemon subcommand '%s'\n", argv[0]);
    print_help();
    return 1;
  }

  return cli_daemon_status(0, NULL);
}

// Commands that no longer exist, each with where its job went.
static const struct {
  const char *name;
  const char *hint;
} removed_commands[] = {
    {"enable", "use 'notification enable <prayer|all>'"},
    {"disable", "use 'notification disable <prayer|all>'"},
    {"list", "use 'notification' to see settings"},
    {"reminder", "use 'notification --reminder'"},
    {"sound", "use 'notification --sound' or 'notification --adhan'"},
};

static const CommandEntry top_commands[] = {
    {"show", handle_show},
    {"location", handle_location},
    {"offset", handle_offset},
    {"daemon", handle_daemon},
    {"method", handle_method},
    {"madzhab", handle_madzhab},
    {"notification", handle_notification},
    {"timeformat", handle_timeformat},
    {"version", handle_version},
    {"--version", handle_version},
    {"-v", handle_version},
    {"help", handle_help},
    {"--help", handle_help},
    {"-h", handle_help},
};

int handle_version(int argc, char **argv) {
  if (cli_wants_help(argc, argv)) {
    printf("Usage: muslimtify version\n");
    return 0;
  }
  if (cli_reject_extra_args("version", argc, argv))
    return 1;

  printf("Muslimtify v%s\n", MUSLIMTIFY_VERSION);
  printf("Prayer Time Notification Daemon\n\n");
  Muslimtify *mt = NULL;
  MuslimtifyMethodInfo info;
  if (muslimtify_open(&mt) == MUSLIMTIFY_OK && muslimtify_get_method(mt, &info) == MUSLIMTIFY_OK) {
    printf("Method: %s", info.key);
    if (info.name[0] != '\0')
      printf(" (%s)", info.name);
    printf("\n");
  } else {
    printf("Method: unknown (the configuration could not be read)\n");
  }
  muslimtify_close(mt);

  return 0;
}

int handle_help(int argc, char **argv) {
  if (!cli_wants_help(argc, argv) && cli_reject_extra_args("help", argc, argv))
    return 1;

  cli_print_help();
  return 0;
}

void cli_print_help(void) {
  printf("Muslimtify - Cross-platform Prayer Time Notification Daemon\n\n");

  printf("Usage: muslimtify <command> [options]\n\n");

  /*
   * PRAYER
   */
  printf("Prayer Commands:\n");

  printf("  %-25s %s\n", "show [options]", "Display today's prayer times");

  printf("  %-25s %s\n", "    --headless", "Plain key=value output");

  printf("  %-25s %s\n", "    --json", "Output prayer times as JSON");

  printf("  %-25s %s\n", "    --next [options]", "Show next prayer");
  printf("  %-25s %s\n", "      --json", "Show next prayer as JSON");
  printf("  %-25s %s\n", "      --headless", "Show next prayer as key=value");
  printf("  %-25s %s\n", "    --date <start> [end]",
         "Show prayer times for a date or inclusive range (yyyy-mm-dd)");
  printf("  %-25s %s\n", "    --day-offset <days>", "Show prayer times <days> from today (+/-)");

  printf("\n");

  /*
   * LOCATION
   */
  printf("Location Commands:\n");

  printf("  %-25s %s\n", "location [options]", "Show current location");

  printf("  %-25s %s\n", "    --json", "Output as JSON");

  printf("  %-25s %s\n", "    --headless", "Output as key=value");

  printf("  %-25s %s\n", "    set [options]", "Update saved location fields");

  printf("  %-25s %s\n", "      --lat=<latitude>", "Set latitude");

  printf("  %-25s %s\n", "      --long=<longitude>", "Set longitude");

  printf("  %-25s %s\n", "      --timezone=<iana>", "Set timezone (IANA)");

  printf("  %-25s %s\n", "      --city=<name>", "Set city name");

  printf("  %-25s %s\n", "      --country=<iso2>", "Set country code");

  printf("  %-25s %s\n", "      --auto", "Detect from IP address");

  printf("  %-25s %s\n", "      --refresh-interval=<s>",
         "Auto-refresh interval in seconds (0=off, min 3600)");

  printf("  %-25s %s\n", "    gps <on|off>", "Enable/disable GPS (gpsd) location source");

  printf("\n");

  /*
   * METHOD
   */
  printf("Calculation Method Commands:\n");

  printf("  %-25s %s\n", "method", "Show current method");

  printf("  %-25s %s\n", "    <name>", "Set calculation method");

  printf("  %-25s %s\n", "    --auto", "Auto-select method from country");

  printf("  %-25s %s\n", "    --list", "List available methods");

  printf("  %-25s %s\n", "madzhab <shafi|hanafi>", "Set madzhab");

  printf("  %-25s %s\n", "    --list", "List madzhab options");
  printf("  %-25s %s\n", "timeformat <12|24>", "Set clock format (12-hour or 24-hour)");
  printf("  %-25s %s\n", "    --list", "List clock formats");

  printf("\n");

  /*
   * NOTIFICATION
   */
  printf("Notification Commands:\n");
  printf("  %-25s %s\n", "notification", "Show notification settings");
  printf("  %-25s %s\n", "    enable|disable [prayer]", "Toggle prayer notifications");
  printf("  %-25s %s\n", "    --urgency <level>", "normal|critical|low");
  printf("  %-25s %s\n", "    --reminder <prayer|--all> <mins...>", "Pre-prayer reminders");
  printf("  %-25s %s\n", "    --after <prayer|--all> <mins...>", "Post-adhan reminders");
  printf("  %-25s %s\n", "    --adhan <enable|disable> <prayer>", "Per-prayer adhan");
  printf("  %-25s %s\n", "    --sound <adhan|default|off>", "Notification sound mode");
  printf("\n");

  /*
   * OFFSET
   */
  printf("Offset Commands:\n");

  printf("  %-25s %s\n", "offset <prayer> <min>", "Adjust a prayer time (+/-60)");

  printf("  %-25s %s\n", "    all <min>", "Adjust every prayer time");

  printf("\n");

  /*
   * DAEMON
   */
  printf("Daemon Commands:\n");

  printf("  %-25s %s\n", "daemon install", "Install and start the daemon");

  printf("  %-25s %s\n", "    uninstall", "Stop and remove the daemon");

  printf("  %-25s %s\n", "    status", "Show status (also the default)");

  printf("\n");

  /*
   * GENERAL
   */
  printf("General Commands:\n");

  printf("  %-25s %s\n", "version", "Show version information");

  printf("  %-25s %s\n", "help", "Show help message");

  printf("\n");

  /*
   * EXAMPLES
   */
  printf("Examples:\n");

  printf("  %-25s %s\n", "muslimtify show --next", "# Show next prayer");

  printf("  %-25s %s\n", "muslimtify method mwl", "# Set calculation method");

  printf("  %-25s %s\n", "muslimtify madzhab hanafi", "# Set madzhab");

  printf("  %-25s %s\n", "muslimtify notification enable fajr", "# Enable Fajr notification");

  printf("  %-25s %s\n", "muslimtify notification --reminder fajr 30 15 5", "# Set reminders");

  printf("  %-25s %s\n", "muslimtify offset fajr +4", "# Shift Fajr 4 min later");

  printf("  %-25s %s\n", "muslimtify location set --lat=-6.21 --long=106.84", "# Set coordinates");

  printf("  %-25s %s\n", "muslimtify location set --timezone=Asia/Jakarta", "# Override timezone");

  printf("  %-25s %s\n", "muslimtify location gps on", "# Use local gpsd for location");

  printf("\n");

  printf("Config File:\n");
  printf("  %s\n", muslimtify_config_path());
}

int cli_run(int argc, char **argv) {
  if (argc < 2) {
    handle_version(0, NULL);
    printf("\n");
    cli_print_help();
    return 0;
  }

  const char *cmd = argv[1];
  const CommandEntry *entry = dispatch_lookup(top_commands, ARRAY_LEN(top_commands), cmd);
  if (entry)
    return entry->handler(argc - 2, argv + 2);

  for (size_t i = 0; i < ARRAY_LEN(removed_commands); i++) {
    if (strcmp(removed_commands[i].name, cmd) == 0) {
      fprintf(stderr, "Error: '%s' was removed; %s\n", cmd, removed_commands[i].hint);
      return 1;
    }
  }

  fprintf(stderr, "Unknown command '%s'. Use 'muslimtify help' for usage.\n", cmd);
  return 1;
}
