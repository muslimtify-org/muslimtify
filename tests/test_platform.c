#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "platform.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef _WIN32
#include <pwd.h>
#include <unistd.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static int total = 0;
static int failures = 0;

static void report_result(const char *label, bool pass) {
  total++;
  if (pass) {
    printf("  PASS: %s\n", label);
  } else {
    printf("  FAIL: %s\n", label);
    failures++;
  }
}

static void check_path(const char *label, const char *first, const char *second) {
  char non_empty_label[96];
  char stable_label[96];

  snprintf(non_empty_label, sizeof(non_empty_label), "%s is non-empty", label);
  snprintf(stable_label, sizeof(stable_label), "%s pointer is stable", label);

  report_result(non_empty_label,
                first != NULL && first[0] != '\0' && second != NULL && second[0] != '\0');
  report_result(stable_label, first == second);
}

static bool tm_fields_equal(const struct tm *a, const struct tm *b) {
  return a->tm_sec == b->tm_sec && a->tm_min == b->tm_min && a->tm_hour == b->tm_hour &&
         a->tm_mday == b->tm_mday && a->tm_mon == b->tm_mon && a->tm_year == b->tm_year &&
         a->tm_wday == b->tm_wday && a->tm_yday == b->tm_yday && a->tm_isdst == b->tm_isdst;
}

static void test_platform_time_helpers(void) {
  printf("test_platform_time_helpers\n");

  time_t sample = 0;
  struct tm first;
  struct tm second;

  platform_localtime(&sample, &first);
  platform_localtime(&sample, &second);

  report_result("platform_localtime() is stable", tm_fields_equal(&first, &second));

  int first_tty = platform_isatty(stdout);
  int second_tty = platform_isatty(stdout);
  report_result("platform_isatty(stdout) is stable",
                first_tty == second_tty && (first_tty == 0 || first_tty == 1));
}

static void test_platform_boundary(void) {
  printf("test_platform_boundary\n");

  const char *config_first = platform_config_dir();
  const char *config_second = platform_config_dir();
  check_path("platform_config_dir()", config_first, config_second);

  const char *cache_first = platform_cache_dir();
  const char *cache_second = platform_cache_dir();
  check_path("platform_cache_dir()", cache_first, cache_second);

  const char *home_first = platform_home_dir();
  const char *home_second = platform_home_dir();
  check_path("platform_home_dir()", home_first, home_second);

  const char *exe_path_first = platform_exe_path();
  const char *exe_path_second = platform_exe_path();
  check_path("platform_exe_path()", exe_path_first, exe_path_second);

  const char *exe_first = platform_exe_dir();
  const char *exe_second = platform_exe_dir();
  check_path("platform_exe_dir()", exe_first, exe_second);
}

#ifdef _WIN32
static bool wide_to_utf8(const wchar_t *wide, char *out, size_t out_size) {
  int len;

  if (!wide || !out || out_size == 0)
    return false;

  len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, out, (int)out_size, NULL, NULL);
  if (len <= 0) {
    out[0] = '\0';
    return false;
  }

  return true;
}

static void set_env_wide(const wchar_t *name, const wchar_t *value) {
  SetEnvironmentVariableW(name, value);
}

static void test_windows_file_operations(void) {
  printf("test_windows_file_operations\n");

  wchar_t temp_base[MAX_PATH];
  DWORD len = GetTempPathW(MAX_PATH, temp_base);
  if (len == 0 || len >= MAX_PATH) {
    report_result("GetTempPathW()", false);
    return;
  }

  wchar_t root_w[PLATFORM_PATH_MAX];
  if (swprintf(root_w, PLATFORM_PATH_MAX, L"%lsmuslimtify_\x00E9_platform_env", temp_base) < 0) {
    report_result("build UTF-8 root", false);
    return;
  }

  char root_utf8[PLATFORM_PATH_MAX];
  if (!wide_to_utf8(root_w, root_utf8, sizeof(root_utf8))) {
    report_result("root UTF-8 conversion", false);
    return;
  }

  report_result("platform_mkdir_p()", platform_mkdir_p(root_utf8) == 0);

  wchar_t appdata_w[PLATFORM_PATH_MAX];
  wchar_t local_w[PLATFORM_PATH_MAX];
  wchar_t home_w[PLATFORM_PATH_MAX];
  wchar_t expected_config_w[PLATFORM_PATH_MAX];
  wchar_t expected_cache_w[PLATFORM_PATH_MAX];

  swprintf(appdata_w, PLATFORM_PATH_MAX, L"%ls\\AppData\\Roaming", root_w);
  swprintf(local_w, PLATFORM_PATH_MAX, L"%ls\\AppData\\Local", root_w);
  swprintf(home_w, PLATFORM_PATH_MAX, L"%ls\\Users\\Profile", root_w);
  swprintf(expected_config_w, PLATFORM_PATH_MAX, L"%ls\\muslimtify", appdata_w);
  swprintf(expected_cache_w, PLATFORM_PATH_MAX, L"%ls\\muslimtify", local_w);

  set_env_wide(L"APPDATA", appdata_w);
  set_env_wide(L"LOCALAPPDATA", local_w);
  set_env_wide(L"USERPROFILE", home_w);

  char expected_config[PLATFORM_PATH_MAX];
  char expected_cache[PLATFORM_PATH_MAX];
  char expected_home[PLATFORM_PATH_MAX];
  report_result("config path UTF-8 conversion",
                wide_to_utf8(expected_config_w, expected_config, sizeof(expected_config)));
  report_result("cache path UTF-8 conversion",
                wide_to_utf8(expected_cache_w, expected_cache, sizeof(expected_cache)));
  report_result("home path UTF-8 conversion",
                wide_to_utf8(home_w, expected_home, sizeof(expected_home)));

  report_result("platform_home_dir() UTF-8 path", strcmp(platform_home_dir(), expected_home) == 0);
  report_result("platform_config_dir() UTF-8 path",
                strcmp(platform_config_dir(), expected_config) == 0);
  report_result("platform_cache_dir() UTF-8 path",
                strcmp(platform_cache_dir(), expected_cache) == 0);

  wchar_t exe_w[PLATFORM_PATH_MAX];
  char expected_exe[PLATFORM_PATH_MAX];
  DWORD exe_len = GetModuleFileNameW(NULL, exe_w, PLATFORM_PATH_MAX);
  if (exe_len > 0 && exe_len < PLATFORM_PATH_MAX) {
    report_result("platform_exe_path() UTF-8 path",
                  wide_to_utf8(exe_w, expected_exe, sizeof(expected_exe)) &&
                      strcmp(platform_exe_path(), expected_exe) == 0);

    wchar_t *last_sep = wcsrchr(exe_w, L'\\');
    if (!last_sep)
      last_sep = wcsrchr(exe_w, L'/');
    if (last_sep) {
      *last_sep = L'\0';
      report_result("platform_exe_dir() UTF-8 path",
                    wide_to_utf8(exe_w, expected_exe, sizeof(expected_exe)) &&
                        strcmp(platform_exe_dir(), expected_exe) == 0);
    } else {
      report_result("platform_exe_dir() UTF-8 path", false);
    }
  } else {
    report_result("platform_exe_path() UTF-8 path", false);
    report_result("platform_exe_dir() UTF-8 path", false);
  }

  char dir[PLATFORM_PATH_MAX + 32];
  snprintf(dir, sizeof(dir), "%s\\cache_\xC3\xA9", root_utf8);
  report_result("platform_mkdir_p() nested UTF-8", platform_mkdir_p(dir) == 0);

  wchar_t dir_w[PLATFORM_PATH_MAX];
  swprintf(dir_w, PLATFORM_PATH_MAX, L"%ls\\cache_\x00E9", root_w);

  char source[PLATFORM_PATH_MAX + 64];
  char renamed[PLATFORM_PATH_MAX + 64];
  snprintf(source, sizeof(source), "%s\\cache_\xC3\xA9.json", dir);
  snprintf(renamed, sizeof(renamed), "%s\\cache_renamed_\xC3\xA9.json", dir);

  FILE *f = platform_file_open(source, "w");
  report_result("platform_file_open()", f != NULL);
  if (f) {
    fputs("ok", f);
    fclose(f);
  }

  report_result("platform_file_exists()", platform_file_exists(source) == 1);
  report_result("platform_atomic_rename()", platform_atomic_rename(source, renamed) == 0);
  report_result("renamed file exists", platform_file_exists(renamed) == 1);
  report_result("platform_file_delete()", platform_file_delete(renamed) == 0);
  report_result("deleted file missing", platform_file_exists(renamed) == 0);

  RemoveDirectoryW(dir_w);
  RemoveDirectoryW(root_w);
}
#endif

#ifndef _WIN32
static void test_linux_home_fallback(void) {
  printf("test_linux_home_fallback\n");

  struct passwd *pw = getpwuid(getuid());
  if (!pw || !pw->pw_dir || pw->pw_dir[0] == '\0') {
    printf("  SKIP: linux passwd home fallback unavailable\n");
    return;
  }

  char old_home[PLATFORM_PATH_MAX];
  char old_xdg_config[PLATFORM_PATH_MAX];
  char old_xdg_cache[PLATFORM_PATH_MAX];
  const char *home_env = getenv("HOME");
  const char *xdg_config_env = getenv("XDG_CONFIG_HOME");
  const char *xdg_cache_env = getenv("XDG_CACHE_HOME");
  bool had_home = home_env != NULL;
  bool had_xdg_config = xdg_config_env != NULL;
  bool had_xdg_cache = xdg_cache_env != NULL;

  if (had_home) {
    snprintf(old_home, sizeof(old_home), "%s", home_env);
  }
  if (had_xdg_config) {
    snprintf(old_xdg_config, sizeof(old_xdg_config), "%s", xdg_config_env);
  }
  if (had_xdg_cache) {
    snprintf(old_xdg_cache, sizeof(old_xdg_cache), "%s", xdg_cache_env);
  }

  setenv("HOME", "", 1);
  unsetenv("XDG_CONFIG_HOME");
  unsetenv("XDG_CACHE_HOME");

  const char *resolved_home = platform_home_dir();
  report_result("platform_home_dir() falls back when HOME is empty",
                resolved_home != NULL && strcmp(resolved_home, pw->pw_dir) == 0);

  if (had_xdg_cache) {
    setenv("XDG_CACHE_HOME", old_xdg_cache, 1);
  } else {
    unsetenv("XDG_CACHE_HOME");
  }
  if (had_xdg_config) {
    setenv("XDG_CONFIG_HOME", old_xdg_config, 1);
  } else {
    unsetenv("XDG_CONFIG_HOME");
  }
  if (had_home) {
    setenv("HOME", old_home, 1);
  } else {
    unsetenv("HOME");
  }
}
#endif

int main(void) {
  printf("=== platform boundary tests ===\n\n");

#ifndef _WIN32
  test_linux_home_fallback();
#else
  test_windows_file_operations();
#endif
  test_platform_time_helpers();
  test_platform_boundary();

  printf("\n%d/%d tests passed\n", total - failures, total);
  return failures > 0 ? 1 : 0;
}
