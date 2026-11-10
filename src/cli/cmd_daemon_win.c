#include "cli_internal.h"
#include "toast_activator.h"
#include "util.h"
#include <stdio.h>

static int daemon_install_handler(int argc, char **argv) {
  (void)argc;
  (void)argv;

  /* Location and calculation method are auto-detected lazily on the first
     scheduled run (see muslimtify_run_cycle in src/lib/muslimtify_cycle.c). Avoid any network
     I/O here so silent/unattended installs (e.g. winget) never block. */
  MuslimtifyError err = muslimtify_daemon_install(NULL);
  if (err != MUSLIMTIFY_OK)
    return cli_fail(err);

  printf("Scheduled task 'muslimtify' created successfully.\n");
  printf("Prayer times will be checked every minute.\n");
  return 0;
}

static int daemon_uninstall_handler(int argc, char **argv) {
  (void)argc;
  (void)argv;

  MuslimtifyDaemonUninstall result;
  MuslimtifyError err = muslimtify_daemon_uninstall(&result);
  if (err != MUSLIMTIFY_OK)
    return cli_fail(err);

  if (result.unit_removed)
    printf("Scheduled task 'muslimtify' removed.\n");
  else
    printf("Note: No scheduled task was found (already uninstalled?)\n");
  return 0;
}

static int daemon_register_handler(int argc, char **argv) {
  (void)argc;
  (void)argv;
  if (register_toast_activator() != 0) {
    fprintf(stderr, "Error: failed to register the toast activator\n");
    return 1;
  }
  printf("Adhan notification Stop button registered.\n");
  return 0;
}

static int daemon_unregister_handler(int argc, char **argv) {
  (void)argc;
  (void)argv;
  unregister_toast_activator();
  printf("Adhan notification Stop button unregistered.\n");
  return 0;
}

static const CommandEntry daemon_commands[] = {
    {"install", daemon_install_handler},
    {"uninstall", daemon_uninstall_handler},
    {"status", cli_daemon_status},
    {"register", daemon_register_handler},
    {"unregister", daemon_unregister_handler},
};

static void print_daemon_help(void) {
  printf("\n");
  printf("Manage the prayer time scheduled task\n");
  printf("\n");
  printf("Usage: muslimtify daemon [command]\n");
  printf("\n");
  printf("Commands:\n");
  printf("  %-25s %s\n", "install", "Create and start the scheduled task");
  printf("  %-25s %s\n", "uninstall", "Stop and remove the scheduled task");
  printf("  %-25s %s\n", "status", "Show task status (also the default)");
  printf("  %-25s %s\n", "register", "Register the adhan notification Stop button");
  printf("  %-25s %s\n", "unregister", "Unregister the adhan notification Stop button");
  printf("  %-25s %s\n", "-h, --help", "Show this help");
  printf("\n");
  printf("Examples:\n");
  printf("  %-25s %s\n", "muslimtify daemon install", "# Create and start the task");
  printf("  %-25s %s\n", "muslimtify daemon status", "# Show task status");
  printf("  %-25s %s\n", "muslimtify daemon uninstall", "# Stop and remove the task");
}

int handle_daemon(int argc, char **argv) {
  return cli_daemon_dispatch(daemon_commands, ARRAY_LEN(daemon_commands), print_daemon_help, argc,
                             argv);
}
