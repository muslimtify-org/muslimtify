#ifndef TEST_SUPPORT_H
#define TEST_SUPPORT_H

/* What the tests need from the operating system, in one place, so a test file
   never branches on the platform itself. A file that includes this keeps its
   feature-test macro as its first line: the POSIX half needs mkdtemp and
   setenv declared. */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <process.h>
#define TEST_CONFIG_HOME_VAR "APPDATA"
#define TEST_CACHE_HOME_VAR "LOCALAPPDATA"
#define TEST_DUP _dup
#define TEST_DUP2 _dup2
#define TEST_CLOSE _close
#define TEST_FILENO _fileno
#else
#include <sys/stat.h>
#include <unistd.h>
#define TEST_CONFIG_HOME_VAR "XDG_CONFIG_HOME"
#define TEST_CACHE_HOME_VAR "XDG_CACHE_HOME"
#define TEST_DUP dup
#define TEST_DUP2 dup2
#define TEST_CLOSE close
#define TEST_FILENO fileno
#endif

/* Creates a new empty directory for a test and writes its path into buf.
   False when it could not be made. */
static inline bool test_tmpdir(char *buf, size_t cap, const char *tag) {
#ifdef _WIN32
  static int counter = 0;
  const char *base = getenv("TEMP");
  if (!base || base[0] == '\0')
    base = getenv("TMP");
  if (!base || base[0] == '\0')
    return false;
  int n = snprintf(buf, cap, "%s\\mt_%s_%d_%d", base, tag, _getpid(), counter++);
  return n > 0 && (size_t)n < cap && _mkdir(buf) == 0;
#else
  int n = snprintf(buf, cap, "/tmp/mt_%s_XXXXXX", tag);
  return n > 0 && (size_t)n < cap && mkdtemp(buf) != NULL;
#endif
}

static inline void test_set_env(const char *name, const char *value) {
#ifdef _WIN32
  _putenv_s(name, value);
#else
  setenv(name, value, 1);
#endif
}

/* The directory the product puts <dir>/muslimtify/config.json under. */
static inline void test_set_config_home(const char *dir) {
  test_set_env(TEST_CONFIG_HOME_VAR, dir);
}

/* The directory the product puts <dir>/muslimtify/ cache files under. */
static inline void test_set_cache_home(const char *dir) {
  test_set_env(TEST_CACHE_HOME_VAR, dir);
}

/* Undo test_set_config_home and test_set_cache_home. On Windows an unset
   variable would send the product to the user's real profile, so there the
   variable is pointed at a path nothing uses instead of being removed. */
static inline void test_clear_home(const char *name) {
#ifdef _WIN32
  char dead[512];
  const char *base = getenv("TEMP");
  snprintf(dead, sizeof(dead), "%s\\mt_cleared_home", base ? base : "C:\\mt_no_temp");
  _putenv_s(name, dead);
#else
  unsetenv(name);
#endif
}

static inline void test_clear_cache_home(void) {
  test_clear_home(TEST_CACHE_HOME_VAR);
}

static inline bool test_mkdir(const char *path) {
#ifdef _WIN32
  return _mkdir(path) == 0;
#else
  return mkdir(path, 0755) == 0;
#endif
}

static inline bool test_rmdir(const char *path) {
#ifdef _WIN32
  return _rmdir(path) == 0;
#else
  return rmdir(path) == 0;
#endif
}

/* Sets a POSIX file mode. Only call it when test_has_posix_modes is true. */
static inline bool test_chmod(const char *path, unsigned mode) {
#ifdef _WIN32
  (void)path;
  (void)mode;
  return false;
#else
  return chmod(path, (mode_t)mode) == 0;
#endif
}

/* Best-effort removal of a directory and everything in it. */
static inline void test_remove_tree(const char *dir) {
  char cmd[1024];
#ifdef _WIN32
  snprintf(cmd, sizeof(cmd), "rmdir /s /q \"%s\"", dir);
#else
  snprintf(cmd, sizeof(cmd), "rm -rf %s", dir);
#endif
  if (system(cmd) != 0) { /* best-effort cleanup */
  }
}

/* True when the path holds the fragment, with either separator counting as
   the other. Write the fragment with forward slashes. */
static inline bool test_path_contains(const char *path, const char *fragment) {
  char normal[1024];
  size_t n = 0;
  for (; path[n] != '\0' && n + 1 < sizeof(normal); n++)
    normal[n] = path[n] == '\\' ? '/' : path[n];
  normal[n] = '\0';
  return strstr(normal, fragment) != NULL;
}

static inline bool test_is_absolute_path(const char *path) {
#ifdef _WIN32
  return path[0] != '\0' && path[1] == ':' && (path[2] == '\\' || path[2] == '/');
#else
  return path[0] == '/';
#endif
}

/* Whether file modes such as 0600 and a read-only directory mean anything. */
static inline bool test_has_posix_modes(void) {
#ifdef _WIN32
  return false;
#else
  return true;
#endif
}

/* Whether a test can create a symbolic link without a special privilege. */
static inline bool test_can_symlink(void) {
#ifdef _WIN32
  return false;
#else
  return true;
#endif
}

/* Creates a symbolic link. Only call it when test_can_symlink is true. */
static inline bool test_symlink(const char *target, const char *link_path) {
#ifdef _WIN32
  (void)target;
  (void)link_path;
  return false;
#else
  return symlink(target, link_path) == 0;
#endif
}

/* True on Windows, after saying why, for a case the product gets wrong there.
   Wrap the case in `if (!test_known_windows_gap("reason")) { ... }`. */
static inline bool test_known_windows_gap(const char *reason) {
#ifdef _WIN32
  printf("  SKIP (windows gap): %s\n", reason);
  return true;
#else
  (void)reason;
  return false;
#endif
}

/* Sends one or two streams into a temporary file until test_capture_end. */
typedef struct {
  FILE *file;
  int fd[2];
  int saved[2];
  int count;
} TestCapture;

/* Also send another stream into a capture that test_capture_begin started. */
static inline bool test_capture_add(TestCapture *capture, FILE *stream) {
  if (!capture->file || capture->count >= 2)
    return false;
  fflush(stream);
  int fd = TEST_FILENO(stream);
  int saved = TEST_DUP(fd);
  if (saved < 0)
    return false;
  if (TEST_DUP2(TEST_FILENO(capture->file), fd) < 0) {
    TEST_CLOSE(saved);
    return false;
  }
  capture->fd[capture->count] = fd;
  capture->saved[capture->count] = saved;
  capture->count++;
  return true;
}

static inline bool test_capture_begin(TestCapture *capture, FILE *stream) {
  memset(capture, 0, sizeof(*capture));
  capture->file = tmpfile();
  if (!capture->file)
    return false;
  if (!test_capture_add(capture, stream)) {
    fclose(capture->file);
    capture->file = NULL;
    return false;
  }
  return true;
}

/* Puts the streams back and copies what they wrote into buffer, which may be
   NULL. Carriage returns are dropped, so a test compares the same text on
   every platform. */
static inline void test_capture_end(TestCapture *capture, char *buffer, size_t cap) {
  if (buffer && cap > 0)
    buffer[0] = '\0';
  if (!capture->file)
    return;
  fflush(stdout);
  fflush(stderr);
  for (int i = 0; i < capture->count; i++) {
    TEST_DUP2(capture->saved[i], capture->fd[i]);
    TEST_CLOSE(capture->saved[i]);
  }
  rewind(capture->file);
  if (buffer && cap > 0) {
    size_t n = 0;
    int ch;
    while (n + 1 < cap && (ch = fgetc(capture->file)) != EOF) {
      if (ch != '\r')
        buffer[n++] = (char)ch;
    }
    buffer[n] = '\0';
  }
  fclose(capture->file);
  capture->file = NULL;
  capture->count = 0;
}

#endif
