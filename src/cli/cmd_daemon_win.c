#include "cmd_daemon_win.h"
#include "cli_internal.h"
#include "platform.h"
#include "toast_activator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>

static wchar_t *utf8_to_wide(const char *text) {
  int len;
  wchar_t *wide;

  if (!text)
    return NULL;

  len = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
  if (len <= 0)
    return NULL;

  wide = (wchar_t *)malloc((size_t)len * sizeof(wchar_t));
  if (!wide)
    return NULL;

  if (MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, len) <= 0) {
    free(wide);
    return NULL;
  }

  return wide;
}

static int run_schtasks(const char *args) {
  wchar_t *wide_args;
  wchar_t *cmd;
  size_t cmd_len;

  STARTUPINFOW si;
  PROCESS_INFORMATION pi;
  DWORD err;
  int result = -1;

  wide_args = utf8_to_wide(args);
  if (!wide_args) {
    fprintf(stderr, "Error: Failed to prepare schtasks arguments\n");
    return -1;
  }

  cmd_len = wcslen(L"schtasks.exe ") + wcslen(wide_args) + 1;
  cmd = (wchar_t *)malloc(cmd_len * sizeof(wchar_t));
  if (!cmd) {
    free(wide_args);
    fprintf(stderr, "Error: Out of memory while preparing schtasks command\n");
    return -1;
  }

  swprintf(cmd, cmd_len, L"schtasks.exe %ls", wide_args);

  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  ZeroMemory(&pi, sizeof(pi));

  if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
    err = GetLastError();
    if (err == ERROR_ACCESS_DENIED) {
      fprintf(stderr, "Error: Access denied. Try running as administrator.\n");
    } else {
      fprintf(stderr, "Error: Failed to run schtasks (error %lu)\n", err);
    }
    goto cleanup;
  }

  /* Bounded wait: never block the installer indefinitely. schtasks should
     return in well under a second; if it stalls (e.g. waiting on input that
     can never arrive in an unattended context), terminate it rather than hang.
     A 60s ceiling is generous for a local schtasks call yet far below any
     installer/validation timeout. */
  DWORD wait_status = WaitForSingleObject(pi.hProcess, 60000);

  DWORD exit_code;
  if (wait_status == WAIT_TIMEOUT) {
    fprintf(stderr, "Error: schtasks did not complete within 60 seconds\n");
    TerminateProcess(pi.hProcess, 1);
    WaitForSingleObject(pi.hProcess, 5000);
    exit_code = 1;
  } else {
    GetExitCodeProcess(pi.hProcess, &exit_code);
  }
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  result = (int)exit_code;

cleanup:
  free(cmd);
  free(wide_args);
  return result;
}

static int build_windows_helper_path(const char *exe_dir, char *buffer, size_t buffer_size) {
  int written;

  if (!exe_dir || !buffer || buffer_size == 0 || exe_dir[0] == '\0') {
    return -1;
  }

  written =
      snprintf(buffer, buffer_size, "%s%c%s", exe_dir, PLATFORM_PATH_SEP, "muslimtify-service.exe");
  if (written < 0 || (size_t)written >= buffer_size) {
    return -1;
  }

  return written;
}

int build_windows_task_action(const char *exe_dir, char *buffer, size_t buffer_size) {
  int written;

  if (build_windows_helper_path(exe_dir, buffer, buffer_size) < 0) {
    return -1;
  }

  written = (int)strlen(buffer);
  if ((size_t)written + 3 > buffer_size) {
    return -1;
  }

  memmove(buffer + 1, buffer, (size_t)written + 1);
  buffer[0] = '"';
  buffer[written + 1] = '"';
  buffer[written + 2] = '\0';
  return written + 2;
}

static int daemon_install_handler(int argc, char **argv) {
  (void)argc;
  (void)argv;

  const char *exe_dir = platform_exe_dir();
  if (!exe_dir || exe_dir[0] == '\0') {
    fprintf(stderr, "Error: Cannot determine the executable directory\n");
    return 1;
  }

  char task_action[DAEMON_TASK_ACTION_MAX];
  char helper_path[DAEMON_TASK_ACTION_MAX];
  if (build_windows_task_action(exe_dir, task_action, sizeof(task_action)) < 0) {
    fprintf(stderr, "Error: Failed to build scheduled task action\n");
    return 1;
  }

  if (build_windows_helper_path(exe_dir, helper_path, sizeof(helper_path)) < 0) {
    fprintf(stderr, "Error: Failed to resolve helper executable path\n");
    return 1;
  }

  if (!platform_file_exists(helper_path)) {
    fprintf(stderr, "Error: Cannot find muslimtify-service.exe at '%s'\n", helper_path);
    return 1;
  }

  /* /it (interactive token) registers the task to run only while the user is
     logged on and, crucially, WITHOUT a stored password. Without it schtasks
     prompts "Enter the run as password for user ..." and, in a non-interactive
     context (e.g. winget's unattended-install validation VM), blocks on stdin
     forever -- hanging the installer until it times out. Interactive-only is
     also the correct mode for a notification daemon, which needs a logged-on
     session to show toasts. */
  char args[DAEMON_TASK_ACTION_MAX + 128];
  snprintf(args, sizeof(args), "/create /tn \"muslimtify\" /tr %s /sc minute /mo 1 /it /f",
           task_action);

  int result = run_schtasks(args);
  if (result == 0) {
    printf("Scheduled task 'muslimtify' created successfully.\n");
    /* Location and calculation method are auto-detected lazily on the first
       scheduled run (see location_prepare in check_cycle). Avoid any network
       I/O here so silent/unattended installs (e.g. winget) never block. */
    printf("Prayer times will be checked every minute.\n");
  }

  /* Register the toast activator (HKCU + Start-Menu shortcut) so the adhan
     notification's Stop button routes back to us. Independent of the scheduled
     task; a failure here does not fail the install. */
  if (register_toast_activator() == 0) {
    printf("Adhan notification Stop button registered.\n");
  } else {
    fprintf(stderr, "Warning: could not register the toast Stop button "
                    "(notifications still work; stop via 'muslimtify sound stop').\n");
  }
  return result;
}

static int daemon_uninstall_handler(int argc, char **argv) {
  (void)argc;
  (void)argv;

  int result = run_schtasks("/delete /tn \"muslimtify\" /f");
  if (result == 0) {
    printf("Scheduled task 'muslimtify' removed.\n");
  }
  unregister_toast_activator();
  return result;
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

static int daemon_status_handler(int argc, char **argv) {
  (void)argc;
  (void)argv;

  return run_schtasks("/query /tn \"muslimtify\"");
}

static const CommandEntry daemon_commands[] = {
    {"install", daemon_install_handler},       {"uninstall", daemon_uninstall_handler},
    {"status", daemon_status_handler},         {"register", daemon_register_handler},
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
  if (cli_wants_help(argc, argv)) {
    print_daemon_help();
    return 0;
  }
  if (argc > 0) {
    const CommandEntry *sub = dispatch_lookup(daemon_commands, ARRAY_LEN(daemon_commands), argv[0]);
    if (sub)
      return sub->handler(argc - 1, argv + 1);

    fprintf(stderr, "Error: Unknown daemon subcommand '%s'\n", argv[0]);
    print_daemon_help();
    return 1;
  }

  return daemon_status_handler(0, NULL);
}
