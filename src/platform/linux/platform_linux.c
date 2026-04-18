#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "platform.h"
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char config_dir_buf[PLATFORM_PATH_MAX] = {0};
static char cache_dir_buf[PLATFORM_PATH_MAX] = {0};
static char home_dir_buf[PLATFORM_PATH_MAX] = {0};
static char exe_path_buf[PLATFORM_PATH_MAX] = {0};
static char exe_dir_buf[PLATFORM_PATH_MAX] = {0};

const char *platform_home_dir(void) {
  if (home_dir_buf[0] != '\0')
    return home_dir_buf;

  const char *home = getenv("HOME");
  if (home && home[0] != '\0') {
    snprintf(home_dir_buf, sizeof(home_dir_buf), "%s", home);
  } else {
    const char *fallback_home = NULL;
    struct passwd *pw = getpwuid(getuid());
    if (pw && pw->pw_dir && pw->pw_dir[0] != '\0')
      fallback_home = pw->pw_dir;
    if (fallback_home)
      snprintf(home_dir_buf, sizeof(home_dir_buf), "%s", fallback_home);
  }

  return home_dir_buf;
}

const char *platform_config_dir(void) {
  if (config_dir_buf[0] != '\0')
    return config_dir_buf;

  const char *xdg = getenv("XDG_CONFIG_HOME");
  if (xdg && xdg[0] != '\0') {
    snprintf(config_dir_buf, sizeof(config_dir_buf), "%s/muslimtify", xdg);
  } else {
    const char *home = platform_home_dir();
    if (home[0] != '\0' && strlen(home) < sizeof(config_dir_buf) - 20)
      snprintf(config_dir_buf, sizeof(config_dir_buf), "%s/.config/muslimtify", home);
  }

  return config_dir_buf;
}

const char *platform_cache_dir(void) {
  if (cache_dir_buf[0] != '\0')
    return cache_dir_buf;

  const char *xdg = getenv("XDG_CACHE_HOME");
  if (xdg && xdg[0] != '\0') {
    snprintf(cache_dir_buf, sizeof(cache_dir_buf), "%s/muslimtify", xdg);
  } else {
    const char *home = platform_home_dir();
    if (home[0] != '\0' && strlen(home) < sizeof(cache_dir_buf) - 19)
      snprintf(cache_dir_buf, sizeof(cache_dir_buf), "%s/.cache/muslimtify", home);
  }

  return cache_dir_buf;
}

const char *platform_exe_path(void) {
  if (exe_path_buf[0] != '\0')
    return exe_path_buf;

  ssize_t len = readlink("/proc/self/exe", exe_path_buf, sizeof(exe_path_buf) - 1);
  if (len > 0) {
    exe_path_buf[len] = '\0';
  } else {
    exe_path_buf[0] = '\0';
  }

  return exe_path_buf;
}

const char *platform_exe_dir(void) {
  if (exe_dir_buf[0] != '\0')
    return exe_dir_buf;

  const char *exe_path = platform_exe_path();
  if (exe_path[0] != '\0') {
    char tmp[PLATFORM_PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s", exe_path);
    tmp[sizeof(tmp) - 1] = '\0';

    char *last_slash = strrchr(tmp, '/');
    if (last_slash) {
      *last_slash = '\0';
      snprintf(exe_dir_buf, sizeof(exe_dir_buf), "%s", tmp);
    }
  }

  return exe_dir_buf;
}

void platform_reset_cached_paths(void) {
  config_dir_buf[0] = '\0';
  cache_dir_buf[0] = '\0';
  home_dir_buf[0] = '\0';
  exe_path_buf[0] = '\0';
  exe_dir_buf[0] = '\0';
}

int platform_mkdir_p(const char *path) {
  char tmp[PLATFORM_PATH_MAX];
  snprintf(tmp, sizeof(tmp), "%s", path);

  for (char *p = tmp + 1; *p; p++) {
    if (*p == '/') {
      *p = '\0';
      if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
        return -1;
      *p = '/';
    }
  }
  if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
    return -1;

  return 0;
}

int platform_file_exists(const char *path) {
  return access(path, F_OK) == 0 ? 1 : 0;
}

FILE *platform_file_open(const char *path, const char *mode) {
  return fopen(path, mode);
}

int platform_file_delete(const char *path) {
  return unlink(path) == 0 ? 0 : -1;
}

int platform_atomic_rename(const char *src, const char *dst) {
  return rename(src, dst) == 0 ? 0 : -1;
}

void platform_localtime(const time_t *t, struct tm *result) {
  localtime_r(t, result);
}

int platform_isatty(FILE *stream) {
  return isatty(fileno(stream));
}
