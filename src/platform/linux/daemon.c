#define _POSIX_C_SOURCE 200809L

#include "platform/linux/daemon_linux.h"

#include "log.h"
#include "platform.h"

#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define UNIT_DIR ".config/systemd/user"
#define SERVICE_UNIT "muslimtify.service"
#define TIMER_UNIT "muslimtify.timer"

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

// Every directory is created 0755 and the home directory is read on each call.
// platform_mkdir_p and platform_home_dir are not used here on purpose: the
// first makes its last directory owner-only and the second caches its answer.
static void mkdir_p(const char *path) {
  char tmp[PLATFORM_DAEMON_PATH_MAX];
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

int build_service_unit(const char *binary_path, char *buffer, size_t buffer_size) {
  if (!binary_path || !buffer || buffer_size == 0)
    return -1;

  int written = snprintf(buffer, buffer_size,
                         "[Unit]\n"
                         "Description=Muslimtify prayer notification daemon\n"
                         "After=network-online.target\n"
                         "\n"
                         "[Service]\n"
                         "Type=simple\n"
                         "ExecStart=%s daemon run\n"
                         "Restart=on-failure\n"
                         "RestartSec=5\n"
                         "\n"
                         "[Install]\n"
                         "WantedBy=default.target\n",
                         binary_path);

  if (written < 0 || (size_t)written >= buffer_size)
    return -1;
  return written;
}

// Writes "<home>/.config/systemd/user/<name>" into out. False when it does not fit.
static bool unit_file_path(const char *home, const char *name, char *out, size_t cap) {
  int n = snprintf(out, cap, "%s/" UNIT_DIR "/%s", home, name);
  return n > 0 && (size_t)n < cap;
}

PlatformDaemonResult platform_daemon_install(const char *binary_path, PlatformDaemonInstall *out) {
  memset(out, 0, sizeof(*out));

  const char *home = get_home();
  if (!home)
    return PLATFORM_DAEMON_NO_HOME;

  char unit_dir[PLATFORM_DAEMON_PATH_MAX];
  char unit_path[PLATFORM_DAEMON_PATH_MAX];
  char timer_path[PLATFORM_DAEMON_PATH_MAX];
  int n = snprintf(unit_dir, sizeof(unit_dir), "%s/" UNIT_DIR, home);
  if (n <= 0 || (size_t)n >= sizeof(unit_dir) ||
      !unit_file_path(home, SERVICE_UNIT, unit_path, sizeof(unit_path)) ||
      !unit_file_path(home, TIMER_UNIT, timer_path, sizeof(timer_path))) {
    MT_LOGF(MT_LOG_ERROR, "Error: Home directory path is too long for the service file");
    return PLATFORM_DAEMON_UNIT_FAILED;
  }
  mkdir_p(unit_dir);

  char unit[DAEMON_UNIT_MAX];
  if (build_service_unit(binary_path, unit, sizeof(unit)) < 0) {
    MT_LOGF(MT_LOG_ERROR, "Error: Failed to render service unit");
    return PLATFORM_DAEMON_UNIT_FAILED;
  }

  FILE *f = fopen(unit_path, "w");
  if (!f) {
    MT_LOGF(MT_LOG_ERROR, "Error: Cannot write %s: %s", unit_path, strerror(errno));
    return PLATFORM_DAEMON_UNIT_FAILED;
  }
  fputs(unit, f);
  bool write_failed = ferror(f) != 0;
  if (fclose(f) != 0)
    write_failed = true;
  if (write_failed) {
    MT_LOGF(MT_LOG_ERROR, "Error: Failed to write %s: %s", unit_path, strerror(errno));
    return PLATFORM_DAEMON_UNIT_FAILED;
  }
  snprintf(out->unit_path, sizeof(out->unit_path), "%s", unit_path);

  // Heal upgrades from the timer era: an older version may have left
  // muslimtify.timer enabled. Best effort, and silent on a clean install.
  if (systemctl_user((const char *[]){"is-enabled", "--quiet", TIMER_UNIT, NULL}) == 0) {
    systemctl_user((const char *[]){"disable", "--now", TIMER_UNIT, NULL});
    out->legacy_timer_disabled = true;
  }
  remove(timer_path);

  if (systemctl_user((const char *[]){"daemon-reload", NULL}) != 0)
    return PLATFORM_DAEMON_RELOAD_FAILED;

  if (systemctl_user((const char *[]){"enable", "--now", SERVICE_UNIT, NULL}) != 0)
    return PLATFORM_DAEMON_ENABLE_FAILED;

  return PLATFORM_DAEMON_OK;
}

PlatformDaemonResult platform_daemon_uninstall(PlatformDaemonUninstall *out) {
  memset(out, 0, sizeof(*out));

  if (systemctl_user((const char *[]){"is-active", "--quiet", SERVICE_UNIT, NULL}) == 0) {
    systemctl_user((const char *[]){"stop", SERVICE_UNIT, NULL});
    out->stopped = true;
  }

  if (systemctl_user((const char *[]){"is-enabled", "--quiet", SERVICE_UNIT, NULL}) == 0) {
    systemctl_user((const char *[]){"disable", SERVICE_UNIT, NULL});
    out->disabled = true;
  }

  if (systemctl_user((const char *[]){"is-enabled", "--quiet", TIMER_UNIT, NULL}) == 0) {
    systemctl_user((const char *[]){"disable", "--now", TIMER_UNIT, NULL});
    out->legacy_timer_disabled = true;
  }

  const char *home = get_home();
  if (!home)
    return PLATFORM_DAEMON_NO_HOME;

  if (unit_file_path(home, SERVICE_UNIT, out->unit_path, sizeof(out->unit_path)))
    out->unit_removed = remove(out->unit_path) == 0;
  if (unit_file_path(home, TIMER_UNIT, out->timer_path, sizeof(out->timer_path)))
    out->timer_removed = remove(out->timer_path) == 0;

  systemctl_user((const char *[]){"daemon-reload", NULL});
  return PLATFORM_DAEMON_OK;
}

PlatformDaemonResult platform_daemon_status(PlatformDaemonStatus *out) {
  memset(out, 0, sizeof(*out));

  char unit_path[PLATFORM_DAEMON_PATH_MAX];
  const char *home = get_home();
  if (home && unit_file_path(home, SERVICE_UNIT, unit_path, sizeof(unit_path)))
    out->installed = access(unit_path, F_OK) == 0;

  out->enabled = systemctl_user((const char *[]){"is-enabled", "--quiet", SERVICE_UNIT, NULL}) == 0;
  out->running = systemctl_user((const char *[]){"is-active", "--quiet", SERVICE_UNIT, NULL}) == 0;
  return PLATFORM_DAEMON_OK;
}
