#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#define PLATFORM_PATH_MAX 260
#define PLATFORM_PATH_SEP '\\'
#else
#include <limits.h>
#ifdef PATH_MAX
#define PLATFORM_PATH_MAX PATH_MAX
#else
#define PLATFORM_PATH_MAX 4096
#endif
#define PLATFORM_PATH_SEP '/'
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Returns the config directory path (e.g., ~/.config/muslimtify or %APPDATA%\muslimtify).
 *
 * Creates the directory if it doesn't exist. Returns a cached static buffer. No trailing
 * separator.
 */
const char *platform_config_dir(void);

/**
 * Returns the cache directory path (e.g., ~/.cache/muslimtify or %LOCALAPPDATA%\muslimtify).
 *
 * Creates the directory if it doesn't exist. Returns a cached static buffer. No trailing
 * separator.
 */
const char *platform_cache_dir(void);

/**
 * Returns the user's home directory. Returns a cached static buffer. No trailing separator.
 */
const char *platform_home_dir(void);

/**
 * Returns the full path to the running executable.
 */
const char *platform_exe_path(void);

/**
 * Returns the directory containing the running executable. No trailing separator.
 */
const char *platform_exe_dir(void);

/**
 * Reset cached platform paths so subsequent calls re-read environment-derived directories.
 */
void platform_reset_cached_paths(void);

/**
 * Recursively create directories (like mkdir -p). Returns 0 on success, -1 on failure.
 */
int platform_mkdir_p(const char *path);

/**
 * Check if a file exists. Returns 1 if exists, 0 otherwise.
 */
int platform_file_exists(const char *path);

/**
 * Open a file using a UTF-8 path on all platforms.
 */
FILE *platform_file_open(const char *path, const char *mode);

/**
 * Delete a file. Returns 0 on success, -1 on failure.
 */
int platform_file_delete(const char *path);

/**
 * Atomically rename a file (replaces destination if it exists).
 * Returns 0 on success, -1 on failure.
 */
int platform_atomic_rename(const char *src, const char *dst);

/**
 * Thread-safe localtime. Wraps localtime_r (POSIX) or localtime_s (MSVC).
 */
void platform_localtime(const time_t *t, struct tm *result);

/**
 * Check if a FILE stream is a terminal. Returns 1 if tty, 0 otherwise.
 */
int platform_isatty(FILE *stream);

/**
 * Result of platform_resolve_regular_file.
 */
typedef enum {
  PATH_FILE_OK = 0,
  PATH_FILE_NOT_FOUND,
  PATH_FILE_NOT_REGULAR,
  PATH_FILE_IS_SYMLINK,
  PATH_FILE_NOT_READABLE,
  PATH_FILE_RESOLVE_FAILED,
  PATH_FILE_TOO_LONG
} PathFileResult;

/**
 * Zero-trust path validation: verify `in` is an existing, readable, regular
 * file that is not a symlink (rejects symlinks / reparse points, directories,
 * and special files), and write its canonical absolute path into `out`.
 * Returns PATH_FILE_OK on success or a specific failure code.
 */
PathFileResult platform_resolve_regular_file(const char *in, char *out, size_t out_size);

/**
 * Restrict a freshly-created file to owner-only access. On POSIX this is
 * chmod 0600; on Windows it is a no-op (the per-user %APPDATA% /
 * %LOCALAPPDATA% roots are already user-scoped by their default ACL).
 */
void platform_restrict_to_owner(FILE *f);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_H */
