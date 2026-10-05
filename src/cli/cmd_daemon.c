#include "cli_internal.h"
#include "daemon_loop.h"
#include "util.h"
#include <stdio.h>

// Auto-detect location and calculation method only when the config still needs
// a location, the same condition the check cycle uses. Coordinates and a method
// the user set by hand are kept. Nothing here fails the install: a problem is a
// warning, and the service is still set up.
static void daemon_auto_setup(void) {
  Muslimtify *mt = NULL;
  if (muslimtify_open(&mt) != MUSLIMTIFY_OK) {
    fprintf(stderr, "Warning: Failed to load config, skipping auto-detect\n");
    return;
  }

  MuslimtifyLocation loc;
  MuslimtifyMethodInfo method;

  if (!muslimtify_location_needs_detect(mt)) {
    if (muslimtify_get_location(mt, &loc) == MUSLIMTIFY_OK &&
        muslimtify_get_method(mt, &method) == MUSLIMTIFY_OK)
      printf("✓ Using saved location %.4f, %.4f and method %s\n", loc.latitude, loc.longitude,
             method.key);
    muslimtify_close(mt);
    return;
  }

  printf("Detecting location...\n");
  MuslimtifyDetection detection;
  if (muslimtify_detect_location(mt, &detection) != MUSLIMTIFY_OK) {
    fprintf(stderr, "Warning: Failed to detect location, skipping auto-detect\n");
    muslimtify_close(mt);
    return;
  }
  cli_print_gps_warning(&detection);
  muslimtify_set_method_from_country(mt);

  if (muslimtify_get_location(mt, &loc) == MUSLIMTIFY_OK) {
    if (loc.city[0] != '\0')
      printf("✓ Location detected: %s, %s\n", loc.city, loc.country);
    else
      printf("✓ Location detected: %.4f, %.4f\n", loc.latitude, loc.longitude);
  }

  if (muslimtify_save(mt) != MUSLIMTIFY_OK) {
    fprintf(stderr, "Warning: Failed to save config\n");
  } else if (muslimtify_get_method(mt, &method) == MUSLIMTIFY_OK) {
    printf("✓ Method auto-detected: %s", method.key);
    if (method.name[0] != '\0')
      printf(" (%s)", method.name);
    printf("\n");
  }
  muslimtify_close(mt);
}

static int daemon_install_handler(int argc, char **argv) {
  (void)argc;
  (void)argv;

  // The location is settled before the service starts, so its first cycle has
  // something to work with.
  daemon_auto_setup();

  MuslimtifyDaemonInstall result;
  MuslimtifyError err = muslimtify_daemon_install(&result);

  // Report each step that happened, including on a failure part way through.
  if (result.unit_path[0] != '\0')
    printf("✓ Created %s\n", result.unit_path);
  if (result.legacy_timer_disabled)
    printf("✓ Disabled legacy muslimtify.timer\n");
  if (err == MUSLIMTIFY_OK || err == MUSLIMTIFY_ERR_DAEMON_ENABLE)
    printf("✓ Reloaded systemd\n");
  if (err != MUSLIMTIFY_OK)
    return cli_fail(err);
  printf("✓ Enabled and started muslimtify.service\n");

  printf("\nDaemon installed. Binary: %s\n", result.binary_path);
  printf("Run 'muslimtify daemon status' to verify.\n");
  return 0;
}

static int daemon_uninstall_handler(int argc, char **argv) {
  (void)argc;
  (void)argv;

  MuslimtifyDaemonUninstall result;
  MuslimtifyError err = muslimtify_daemon_uninstall(&result);

  if (result.stopped)
    printf("✓ Stopped muslimtify.service\n");
  if (result.disabled)
    printf("✓ Disabled muslimtify.service\n");
  if (result.legacy_timer_disabled)
    printf("✓ Disabled muslimtify.timer\n");
  if (err != MUSLIMTIFY_OK)
    return cli_fail(err);

  if (result.unit_removed)
    printf("✓ Removed %s\n", result.unit_path);
  if (result.timer_removed)
    printf("✓ Removed %s\n", result.timer_path);
  printf("✓ Reloaded systemd\n");

  if (!result.unit_removed && !result.timer_removed)
    printf("Note: No unit files were found (already uninstalled?)\n");

  printf("\nDaemon uninstalled. Binary and config files are untouched.\n");
  printf("To fully remove: sudo ./uninstall.sh\n");
  return 0;
}

static int daemon_status_handler(int argc, char **argv) {
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

static int daemon_run_handler(int argc, char **argv) {
  (void)argc;
  (void)argv;
  return run_daemon_loop();
}

static const CommandEntry daemon_commands[] = {
    {"install", daemon_install_handler},
    {"uninstall", daemon_uninstall_handler},
    {"status", daemon_status_handler},
    {"run", daemon_run_handler},
};

static void print_daemon_help(void) {
  printf("\n");
  printf("Manage the background prayer time daemon\n");
  printf("\n");
  printf("Usage: muslimtify daemon [command]\n");
  printf("\n");
  printf("Commands:\n");
  printf("  %-25s %s\n", "install", "Install and start the daemon");
  printf("  %-25s %s\n", "uninstall", "Stop and remove the daemon");
  printf("  %-25s %s\n", "status", "Show daemon status (also the default)");
  printf("  %-25s %s\n", "-h, --help", "Show this help");
  printf("\n");
  printf("Examples:\n");
  printf("  %-25s %s\n", "muslimtify daemon install", "# Install and start the daemon");
  printf("  %-25s %s\n", "muslimtify daemon status", "# Show daemon status");
  printf("  %-25s %s\n", "muslimtify daemon uninstall", "# Stop and remove the daemon");
}

int handle_daemon(int argc, char **argv) {
  if (cli_wants_help(argc, argv)) {
    print_daemon_help();
    return 0;
  }
  if (argc > 0) {
    const CommandEntry *sub = dispatch_lookup(daemon_commands, ARRAY_LEN(daemon_commands), argv[0]);
    if (sub) {
      // No daemon subcommand takes arguments. Checked before any of them runs.
      char command[64];
      snprintf(command, sizeof(command), "daemon %s", argv[0]);
      if (cli_reject_extra_args(command, argc - 1, argv + 1))
        return 1;
      return sub->handler(argc - 1, argv + 1);
    }

    fprintf(stderr, "Error: Unknown daemon subcommand '%s'\n", argv[0]);
    print_daemon_help();
    return 1;
  }

  return daemon_status_handler(0, NULL);
}
