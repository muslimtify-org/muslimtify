#include "platform.h"

#include <string.h>

/* Daemon management through the library is not implemented on Windows yet.
   `muslimtify daemon` keeps its own Task Scheduler code in cmd_daemon_win.c. */

PlatformDaemonResult platform_daemon_install(const char *binary_path, PlatformDaemonInstall *out) {
  (void)binary_path;
  memset(out, 0, sizeof(*out));
  return PLATFORM_DAEMON_UNSUPPORTED;
}

PlatformDaemonResult platform_daemon_uninstall(PlatformDaemonUninstall *out) {
  memset(out, 0, sizeof(*out));
  return PLATFORM_DAEMON_UNSUPPORTED;
}

PlatformDaemonResult platform_daemon_status(PlatformDaemonStatus *out) {
  memset(out, 0, sizeof(*out));
  return PLATFORM_DAEMON_UNSUPPORTED;
}
