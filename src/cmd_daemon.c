#define _GNU_SOURCE
#include "cli_internal.h"
#include "platform.h"
#include <errno.h>
#include <linux/limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "location.h"
#include "method_detect.h"
#include "string_util.h"

// -- helpers -----------------------------------------------------------------

static int systemctl_user(const char *const *args) {
  int n = 0;
  while (args[n])
    n++;

  char **child_argv = (char **)malloc((size_t)(n + 3) * sizeof(char *));
  if (!child_argv)
    return 1;

  child_argv[0] = "systemctl";
  child_argv[1] = "--user";
  for (int i = 0; i < n; i++)
    child_argv[i + 2] = (char *)args[i];
  child_argv[n + 2] = NULL;

  pid_t pid = fork();
  if (pid < 0) {
    free((void *)child_argv);
    return 1;
  }
  if (pid == 0) {
    execvp("systemctl", child_argv);
    _exit(127);
  }
  free((void *)child_argv);
  int wstatus;
  waitpid(pid, &wstatus, 0);
  return WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : 1;
}

static void mkdir_p(const char *path) {
  char tmp[PATH_MAX];
  snprintf(tmp, sizeof(tmp), "%s", path);
  tmp[sizeof(tmp) - 1] = '\0';
  for (char *p = tmp + 1; *p; p++) {
    if (*p == '/') {
      *p = '\0';
      mkdir(tmp, 0755);
      *p = '/';
    }
  }
  mkdir(tmp, 0755);
}

static const char *get_home(void) {
  const char *home = getenv("HOME");
  if (!home) {
    struct passwd *pw = getpwuid(getuid());
    if (pw)
      home = pw->pw_dir;
  }
  return home;
}

// -- sub-handlers ------------------------------------------------------------

static int daemon_install_handler(int argc, char **argv) {
  (void)argc;
  (void)argv;

  const char *binary_path = platform_exe_path();
  if (!binary_path || binary_path[0] == '\0') {
    fprintf(stderr, "Error: Cannot determine binary path\n");
    return 1;
  }

  const char *home = get_home();
  if (!home) {
    fprintf(stderr, "Error: Cannot determine home directory\n");
    return 1;
  }

  char systemd_dir[PATH_MAX];
  snprintf(systemd_dir, sizeof(systemd_dir), "%s/.config/systemd/user", home);
  mkdir_p(systemd_dir);

  char service_path[PATH_MAX + 32];
  snprintf(service_path, sizeof(service_path), "%s/muslimtify.service", systemd_dir);
  FILE *f = fopen(service_path, "w");
  if (!f) {
    fprintf(stderr, "Error: Cannot write %s: %s\n", service_path, strerror(errno));
    return 1;
  }
  fprintf(f,
          "[Unit]\n"
          "Description=Prayer Time Notification Check\n"
          "After=network-online.target\n"
          "\n"
          "[Service]\n"
          "Type=oneshot\n"
          "ExecStart=%s check\n"
          "StandardOutput=journal\n"
          "StandardError=journal\n",
          binary_path);
  if (ferror(f) || fclose(f) != 0) {
    fprintf(stderr, "Error: Failed to write %s: %s\n", service_path, strerror(errno));
    return 1;
  }

  char timer_path[PATH_MAX + 32];
  snprintf(timer_path, sizeof(timer_path), "%s/muslimtify.timer", systemd_dir);
  f = fopen(timer_path, "w");
  if (!f) {
    fprintf(stderr, "Error: Cannot write %s: %s\n", timer_path, strerror(errno));
    return 1;
  }
  fprintf(f, "[Unit]\n"
             "Description=Check prayer times every minute\n"
             "After=network-online.target\n"
             "\n"
             "[Timer]\n"
             "OnCalendar=*:*:00\n"
             "Persistent=true\n"
             "AccuracySec=1s\n"
             "\n"
             "[Install]\n"
             "WantedBy=timers.target\n");
  if (ferror(f) || fclose(f) != 0) {
    fprintf(stderr, "Error: Failed to write %s: %s\n", timer_path, strerror(errno));
    return 1;
  }

  printf("✓ Created %s\n", service_path);
  printf("✓ Created %s\n", timer_path);

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
        printf("✓ Location detected: %s, %s\n", cfg.city, cfg.country);
      } else {
        printf("✓ Location detected: %.4f, %.4f\n", cfg.latitude, cfg.longitude);
      }

      CalcMethod detected = method_detect_by_country(cfg.country);
      const char *method_key = method_to_string(detected);
      const MethodParams *p = method_params_get(detected);

      copy_string(cfg.calculation_method, sizeof(cfg.calculation_method), method_key);

      if (config_save(&cfg) != 0) {
        fprintf(stderr, "Warning: Failed to save config\n");
      } else {
        printf("✓ Method auto-detected: %s", method_key);
        if (p)
          printf(" (%s)", p->name);
        printf("\n");
      }
    }
  }

  if (systemctl_user((const char *[]){"daemon-reload", NULL}) != 0) {
    fprintf(stderr, "Error: systemctl daemon-reload failed\n");
    return 1;
  }
  printf("✓ Reloaded systemd\n");

  if (systemctl_user((const char *[]){"enable", "muslimtify.timer", NULL}) != 0) {
    fprintf(stderr, "Error: Failed to enable muslimtify.timer\n");
    return 1;
  }
  printf("✓ Enabled muslimtify.timer\n");

  if (systemctl_user((const char *[]){"start", "muslimtify.timer", NULL}) != 0) {
    fprintf(stderr, "Error: Failed to start muslimtify.timer\n");
    return 1;
  }
  printf("✓ Started muslimtify.timer\n");

  printf("\nDaemon installed. Binary: %s\n", binary_path);
  printf("Run 'muslimtify daemon status' to verify.\n");
  return 0;
}

static int daemon_uninstall_handler(int argc, char **argv) {
  (void)argc;
  (void)argv;

  if (systemctl_user((const char *[]){"is-active", "--quiet", "muslimtify.timer", NULL}) == 0) {
    systemctl_user((const char *[]){"stop", "muslimtify.timer", NULL});
    printf("✓ Stopped muslimtify.timer\n");
  }

  if (systemctl_user((const char *[]){"is-enabled", "--quiet", "muslimtify.timer", NULL}) == 0) {
    systemctl_user((const char *[]){"disable", "muslimtify.timer", NULL});
    printf("✓ Disabled muslimtify.timer\n");
  }

  const char *home = get_home();
  if (!home) {
    fprintf(stderr, "Error: Cannot determine home directory\n");
    return 1;
  }

  char path[PATH_MAX];
  int removed = 0;
  for (int i = 0; i < 2; i++) {
    snprintf(path, sizeof(path), "%s/.config/systemd/user/%s", home,
             i == 0 ? "muslimtify.service" : "muslimtify.timer");
    if (remove(path) == 0) {
      printf("✓ Removed %s\n", path);
      removed++;
    }
  }

  systemctl_user((const char *[]){"daemon-reload", NULL});
  printf("✓ Reloaded systemd\n");

  if (removed == 0)
    printf("Note: No unit files were found (already uninstalled?)\n");

  printf("\nDaemon uninstalled. Binary and config files are untouched.\n");
  printf("To fully remove: sudo ./uninstall.sh\n");
  return 0;
}

static int daemon_status_handler(int argc, char **argv) {
  (void)argc;
  (void)argv;

  printf("=== Timer ===\n");
  systemctl_user((const char *[]){"status", "muslimtify.timer", "--no-pager", NULL});

  printf("\n=== Next trigger ===\n");
  systemctl_user((const char *[]){"list-timers", "muslimtify", "--no-pager", NULL});
  return 0;
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
