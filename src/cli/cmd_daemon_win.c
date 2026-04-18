#include "cmd_daemon_win.h"
#include "cli_internal.h"
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>

#ifndef MUSLIMTIFY_CMD_DAEMON_WIN_TEST
#include "location.h"
#include "method_detect.h"
#include "string_util.h"
#endif

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

  WaitForSingleObject(pi.hProcess, INFINITE);

  DWORD exit_code;
  GetExitCodeProcess(pi.hProcess, &exit_code);
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

  char args[PLATFORM_PATH_MAX * 3];
  snprintf(args, sizeof(args), "/create /tn \"muslimtify\" /tr %s /sc minute /mo 1 /f",
           task_action);

  int result = run_schtasks(args);
  if (result == 0) {
    printf("Scheduled task 'muslimtify' created successfully.\n");

#ifndef MUSLIMTIFY_CMD_DAEMON_WIN_TEST
    /* Auto-detect location and calculation method */
    Config cfg;
    if (config_load(&cfg) != 0) {
      fprintf(stderr, "Warning: Failed to load config, skipping auto-detect\n");
    } else {
      printf("Detecting location...\n");
      if (location_fetch(&cfg) != 0) {
        fprintf(stderr, "Warning: Failed to detect location, skipping auto-detect\n");
      } else {
        if (cfg.city[0] != '\0') {
          printf("Location detected: %s, %s\n", cfg.city, cfg.country);
        } else {
          printf("Location detected: %.4f, %.4f\n", cfg.latitude, cfg.longitude);
        }

        CalcMethod detected = method_detect_by_country(cfg.country);
        const char *method_key = method_to_string(detected);
        const MethodParams *p = method_params_get(detected);

        copy_string(cfg.calculation_method, sizeof(cfg.calculation_method), method_key);

        if (config_save(&cfg) != 0) {
          fprintf(stderr, "Warning: Failed to save config\n");
        } else {
          printf("Method auto-detected: %s", method_key);
          if (p)
            printf(" (%s)", p->name);
          printf("\n");
        }
      }
    }
#endif

    printf("Prayer times will be checked every minute.\n");
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
  return result;
}

static int daemon_status_handler(int argc, char **argv) {
  (void)argc;
  (void)argv;

  return run_schtasks("/query /tn \"muslimtify\"");
}

static const CommandEntry daemon_commands[] = {
    {"install", daemon_install_handler},
    {"uninstall", daemon_uninstall_handler},
    {"status", daemon_status_handler},
};

int handle_daemon(int argc, char **argv) {
  if (argc > 0) {
    const CommandEntry *sub =
        dispatch_lookup(daemon_commands, DISPATCH_N(daemon_commands), argv[0]);
    if (sub)
      return sub->handler(argc - 1, argv + 1);

    fprintf(stderr, "Usage: muslimtify daemon [install|uninstall|status]\n");
    return 1;
  }

  return daemon_status_handler(0, NULL);
}
