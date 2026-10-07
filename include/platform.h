#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>
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
 * On POSIX the last directory is created owner-only (0700) and missing parents
 * 0755. Directories that already exist keep their mode.
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
 * Flush a stream and make the OS write its data to disk (fsync on POSIX,
 * _commit on Windows), so a rename that follows cannot leave an empty file
 * after a power loss. Returns 0 on success, -1 on failure.
 */
int platform_file_sync(FILE *f);

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
 * Create or truncate a file for writing with owner-only access from the start.
 * On POSIX it is opened with mode 0600, so no other local user can open it
 * before its mode is narrowed, and a leftover regular file is set to 0600 too.
 * On Windows it is a plain open, since the per-user %APPDATA% and
 * %LOCALAPPDATA% roots are already user-scoped by their default ACL.
 * Returns NULL on failure.
 */
FILE *platform_file_create_private(const char *path);

/**
 * A geographic coordinate in decimal degrees.
 */
typedef struct {
  double lat;
  double lng;
} PlatformLatLng;

/**
 * Outcome of platform_get_location. GPS_OK (0) is success; the negative codes
 * distinguish failure modes so callers can warn, auto-disable, or silently retry.
 */
typedef enum {
  GPS_OK = 0,             /* got a valid fix; *latlong written */
  GPS_UNAVAILABLE = -1,   /* no GPS client on this platform (not yet implemented) */
  GPS_NO_DAEMON = -2,     /* could not connect to the GPS service (e.g. gpsd) */
  GPS_NO_DEVICE = -3,     /* GPS service reachable but reports no device */
  GPS_NO_FIX = -4,        /* device present, but no fix before the timeout */
  GPS_NO_PERMISSION = -5, /* service present but access is denied by the OS.
                           * Windows-only in practice: gpsd has no permission
                           * gate. Unlike the other failures this is fixable by
                           * the user, so callers warn without auto-disabling. */
} GpsStatus;

/**
 * Read the device's current location (lat/lng). Each platform supplies its own
 * source: Linux reads a running gpsd over a socket; Windows reads the WinRT
 * Geolocator. Returns GPS_OK with *latlong written, or a GpsStatus failure code.
 */
GpsStatus platform_get_location(PlatformLatLng *latlong);

/**
 * Outcome of a platform_daemon_* call.
 */
typedef enum {
  PLATFORM_DAEMON_OK = 0,
  PLATFORM_DAEMON_UNSUPPORTED,   /* this platform has no implementation yet */
  PLATFORM_DAEMON_NO_HOME,       /* the user's home directory could not be found */
  PLATFORM_DAEMON_UNIT_FAILED,   /* the service file could not be written */
  PLATFORM_DAEMON_RELOAD_FAILED, /* the service manager would not reload */
  PLATFORM_DAEMON_ENABLE_FAILED, /* the service could not be enabled and started */
  PLATFORM_DAEMON_BINARY_INVALID, /* the program to run is missing or not executable */
  PLATFORM_DAEMON_MANAGER_FAILED /* the service manager refused the request or could not be reached */
} PlatformDaemonResult;

enum { PLATFORM_DAEMON_PATH_MAX = 512 };

typedef struct {
  bool installed; /* a service file for it exists */
  bool enabled;   /* the service manager starts it at login */
  bool running;   /* it is active right now */
} PlatformDaemonStatus;

typedef struct {
  char binary_path[PLATFORM_DAEMON_PATH_MAX]; /* the program the service runs, "" if none was found
                                               */
  char unit_path[PLATFORM_DAEMON_PATH_MAX];   /* the service file written, "" if none */
  bool legacy_timer_disabled;                 /* an old timer unit was switched off */
} PlatformDaemonInstall;

typedef struct {
  bool stopped;
  bool disabled;
  bool legacy_timer_disabled;
  bool unit_removed;
  bool timer_removed;
  char unit_path[PLATFORM_DAEMON_PATH_MAX];
  char timer_path[PLATFORM_DAEMON_PATH_MAX];
} PlatformDaemonUninstall;

/**
 * Register the background service for the current user and start it. On Linux
 * the service runs `binary_path daemon run` under systemd. On Windows it is a
 * scheduled task that runs binary_path, the muslimtify-service.exe helper, once
 * a minute, and the toast activator is registered as well. A NULL binary_path,
 * which is what every caller but a test passes, means the program found in a
 * fixed list of known locations. Either way it must be an existing file, on
 * Linux one the caller may execute, or nothing is installed and
 * PLATFORM_DAEMON_BINARY_INVALID is returned. *out is zeroed first and then
 * filled as the call proceeds, so on a failure it shows how far it got. Prints
 * nothing: failure detail goes to the log handler.
 */
PlatformDaemonResult platform_daemon_install(const char *binary_path, PlatformDaemonInstall *out);

/**
 * Stop the service, switch it off and delete its files, or on Windows delete
 * the scheduled task and unregister the toast activator. Succeeds when nothing
 * was installed. *out says what was actually done.
 */
PlatformDaemonResult platform_daemon_uninstall(PlatformDaemonUninstall *out);

/**
 * Report the service's state. On Linux installed is true when a service file
 * exists in the user's own directory or in a system-wide one, and always when
 * the service is enabled or running. On Windows installed is true when the
 * scheduled task exists, and enabled and running are both true when the task is
 * not disabled.
 */
PlatformDaemonResult platform_daemon_status(PlatformDaemonStatus *out);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_H */
