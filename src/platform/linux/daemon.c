#define _POSIX_C_SOURCE 200809L

#include "platform/linux/daemon_linux.h"

#include "log.h"
#include "platform.h"
#include "version.h"

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
  int wstatus = 0;
  pid_t waited;
  do {
    waited = waitpid(pid, &wstatus, 0);
  } while (waited < 0 && errno == EINTR);
  if (waited != pid)
    return 1;
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
  if (!home || home[0] == '\0') {
    struct passwd *pw = getpwuid(getuid());
    if (pw)
      home = pw->pw_dir;
  }
  return home;
}

// The directory systemd reads the user's own units from: $XDG_CONFIG_HOME when
// set, else ~/.config, each followed by systemd/user. False when there is no
// home or the path does not fit.
static bool unit_dir_path(char *out, size_t cap) {
  const char *xdg = getenv("XDG_CONFIG_HOME");
  int n;
  if (xdg && xdg[0] != '\0') {
    n = snprintf(out, cap, "%s/systemd/user", xdg);
  } else {
    const char *home = get_home();
    if (!home)
      return false;
    n = snprintf(out, cap, "%s/" UNIT_DIR, home);
  }
  return n > 0 && (size_t)n < cap;
}

// Writes binary_path into out as a systemd command-line argument: wrapped in
// double quotes, with backslash and quote escaped and '%' doubled so a path
// is never read as a specifier. False when it does not fit.
static bool quote_exec_path(const char *binary_path, char *out, size_t cap) {
  size_t pos = 0;
  if (cap < 3)
    return false;
  out[pos++] = '"';
  for (const char *p = binary_path; *p; p++) {
    const char *piece;
    if (*p == '\\')
      piece = "\\\\";
    else if (*p == '"')
      piece = "\\\"";
    else if (*p == '%')
      piece = "%%";
    else
      piece = NULL;
    size_t need = piece ? strlen(piece) : 1;
    if (pos + need + 2 > cap)
      return false;
    if (piece) {
      memcpy(out + pos, piece, need);
      pos += need;
    } else {
      out[pos++] = *p;
    }
  }
  out[pos++] = '"';
  out[pos] = '\0';
  return true;
}

int build_service_unit(const char *binary_path, char *buffer, size_t buffer_size) {
  if (!binary_path || !buffer || buffer_size == 0)
    return -1;

  char quoted[PLATFORM_DAEMON_PATH_MAX * 2];
  if (!quote_exec_path(binary_path, quoted, sizeof(quoted)))
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
                         quoted);

  if (written < 0 || (size_t)written >= buffer_size)
    return -1;
  return written;
}

// Writes "<unit_dir>/<name>" into out. False when it does not fit.
static bool unit_file_path(const char *unit_dir, const char *name, char *out, size_t cap) {
  int n = snprintf(out, cap, "%s/%s", unit_dir, name);
  return n > 0 && (size_t)n < cap;
}

static bool is_executable_file(const char *path) {
  struct stat st;
  return path[0] != '\0' && access(path, X_OK) == 0 && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

const char *daemon_find_binary(const char *const *candidates, size_t count) {
  for (size_t i = 0; i < count; i++) {
    if (candidates[i] && is_executable_file(candidates[i]))
      return candidates[i];
  }
  return NULL;
}

// The program the service will run. The caller does not choose it: it is the
// muslimtify program found in a fixed list of places, so a frontend can never
// install itself, or anything else, as the service. A test passes a path of its
// own. Returns NULL, after logging why, when there is no usable program.
static const char *resolve_daemon_binary(const char *binary_path, char *sibling, size_t cap) {
  if (binary_path) {
    if (is_executable_file(binary_path))
      return binary_path;
    MT_LOGF(MT_LOG_ERROR, "Error: %s is not an executable program", binary_path);
    return NULL;
  }

  // Beside the running program first: that is the command line tool itself, a
  // frontend installed next to it, or a build run from its build directory.
  // Then where this build installs to, then the usual system locations.
  const char *candidates[4];
  size_t count = 0;
  const char *dir = platform_exe_dir();
  if (dir && dir[0] != '\0') {
    int n = snprintf(sibling, cap, "%s/muslimtify", dir);
    if (n > 0 && (size_t)n < cap)
      candidates[count++] = sibling;
  }
  candidates[count++] = MUSLIMTIFY_INSTALL_BINDIR "/muslimtify";
  candidates[count++] = "/usr/local/bin/muslimtify";
  candidates[count++] = "/usr/bin/muslimtify";

  const char *found = daemon_find_binary(candidates, count);
  if (!found)
    MT_LOGF(MT_LOG_ERROR, "Error: Cannot find the muslimtify program in any known location");
  return found;
}

PlatformDaemonResult platform_daemon_install(const char *binary_path, PlatformDaemonInstall *out) {
  memset(out, 0, sizeof(*out));

  char sibling[PLATFORM_DAEMON_PATH_MAX];
  binary_path = resolve_daemon_binary(binary_path, sibling, sizeof(sibling));
  if (!binary_path)
    return PLATFORM_DAEMON_BINARY_INVALID;
  snprintf(out->binary_path, sizeof(out->binary_path), "%s", binary_path);

  char unit_dir[PLATFORM_DAEMON_PATH_MAX];
  char unit_path[PLATFORM_DAEMON_PATH_MAX];
  char timer_path[PLATFORM_DAEMON_PATH_MAX];
  if (!get_home())
    return PLATFORM_DAEMON_NO_HOME;
  if (!unit_dir_path(unit_dir, sizeof(unit_dir)) ||
      !unit_file_path(unit_dir, SERVICE_UNIT, unit_path, sizeof(unit_path)) ||
      !unit_file_path(unit_dir, TIMER_UNIT, timer_path, sizeof(timer_path))) {
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

  char unit_dir[PLATFORM_DAEMON_PATH_MAX];
  if (!get_home())
    return PLATFORM_DAEMON_NO_HOME;
  if (!unit_dir_path(unit_dir, sizeof(unit_dir)))
    return PLATFORM_DAEMON_UNIT_FAILED;

  if (unit_file_path(unit_dir, SERVICE_UNIT, out->unit_path, sizeof(out->unit_path)))
    out->unit_removed = remove(out->unit_path) == 0;
  if (unit_file_path(unit_dir, TIMER_UNIT, out->timer_path, sizeof(out->timer_path)))
    out->timer_removed = remove(out->timer_path) == 0;

  systemctl_user((const char *[]){"daemon-reload", NULL});
  return PLATFORM_DAEMON_OK;
}

bool daemon_unit_in_dirs(const char *const *dirs, size_t count) {
  for (size_t i = 0; i < count; i++) {
    char path[PLATFORM_DAEMON_PATH_MAX];
    int n = snprintf(path, sizeof(path), "%s/" SERVICE_UNIT, dirs[i]);
    if (n > 0 && (size_t)n < sizeof(path) && access(path, F_OK) == 0)
      return true;
  }
  return false;
}

PlatformDaemonResult platform_daemon_status(PlatformDaemonStatus *out) {
  memset(out, 0, sizeof(*out));

  // Where systemd looks for user units that are not in the user's own home: a
  // distro package installs the unit into one of these.
  static const char *const system_dirs[] = {
      "/etc/systemd/user",
      "/usr/local/lib/systemd/user",
      "/usr/lib/systemd/user",
  };

  char unit_dir[PLATFORM_DAEMON_PATH_MAX];
  char unit_path[PLATFORM_DAEMON_PATH_MAX];
  if (unit_dir_path(unit_dir, sizeof(unit_dir)) &&
      unit_file_path(unit_dir, SERVICE_UNIT, unit_path, sizeof(unit_path)))
    out->installed = access(unit_path, F_OK) == 0;
  if (!out->installed)
    out->installed = daemon_unit_in_dirs(system_dirs, sizeof(system_dirs) / sizeof(system_dirs[0]));

  out->enabled = systemctl_user((const char *[]){"is-enabled", "--quiet", SERVICE_UNIT, NULL}) == 0;
  out->running = systemctl_user((const char *[]){"is-active", "--quiet", SERVICE_UNIT, NULL}) == 0;

  // A service the manager reports as enabled or active exists, wherever its
  // unit file lives, so the three answers can never contradict each other.
  if (out->enabled || out->running)
    out->installed = true;
  return PLATFORM_DAEMON_OK;
}
