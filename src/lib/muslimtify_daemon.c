#include "muslimtify.h"

#include "platform.h"

#include <stdio.h>
#include <string.h>

_Static_assert(PLATFORM_DAEMON_PATH_MAX == MUSLIMTIFY_PATH_SIZE, "daemon path sizes must match");

static MuslimtifyError daemon_error(PlatformDaemonResult result) {
  switch (result) {
  case PLATFORM_DAEMON_OK:
    return MUSLIMTIFY_OK;
  case PLATFORM_DAEMON_UNSUPPORTED:
    return MUSLIMTIFY_ERR_UNSUPPORTED;
  case PLATFORM_DAEMON_NO_HOME:
    return MUSLIMTIFY_ERR_NO_HOME;
  case PLATFORM_DAEMON_UNIT_FAILED:
    return MUSLIMTIFY_ERR_DAEMON_UNIT;
  case PLATFORM_DAEMON_RELOAD_FAILED:
    return MUSLIMTIFY_ERR_DAEMON_RELOAD;
  case PLATFORM_DAEMON_ENABLE_FAILED:
    return MUSLIMTIFY_ERR_DAEMON_ENABLE;
  }
  // No default label above, so -Wswitch fails the build if a platform result
  // is added without deciding what it maps to.
  return MUSLIMTIFY_ERR_UNSUPPORTED;
}

MuslimtifyError muslimtify_daemon_install(const char *daemon_binary, MuslimtifyDaemonInstall *out) {
  if (out)
    memset(out, 0, sizeof(*out));

  const char *binary = daemon_binary ? daemon_binary : platform_exe_path();
  if (!binary || binary[0] == '\0')
    return MUSLIMTIFY_ERR_DAEMON_BINARY;

  PlatformDaemonInstall result;
  MuslimtifyError err = daemon_error(platform_daemon_install(binary, &result));
  if (out) {
    snprintf(out->unit_path, sizeof(out->unit_path), "%s", result.unit_path);
    out->legacy_timer_disabled = result.legacy_timer_disabled;
  }
  return err;
}

MuslimtifyError muslimtify_daemon_uninstall(MuslimtifyDaemonUninstall *out) {
  if (out)
    memset(out, 0, sizeof(*out));

  PlatformDaemonUninstall result;
  MuslimtifyError err = daemon_error(platform_daemon_uninstall(&result));
  if (out) {
    out->stopped = result.stopped;
    out->disabled = result.disabled;
    out->legacy_timer_disabled = result.legacy_timer_disabled;
    out->unit_removed = result.unit_removed;
    out->timer_removed = result.timer_removed;
    snprintf(out->unit_path, sizeof(out->unit_path), "%s", result.unit_path);
    snprintf(out->timer_path, sizeof(out->timer_path), "%s", result.timer_path);
  }
  return err;
}

MuslimtifyError muslimtify_daemon_status(MuslimtifyDaemonStatus *out) {
  if (!out)
    return MUSLIMTIFY_ERR_INVALID_ARG;
  memset(out, 0, sizeof(*out));

  PlatformDaemonStatus result;
  MuslimtifyError err = daemon_error(platform_daemon_status(&result));
  out->installed = result.installed;
  out->enabled = result.enabled;
  out->running = result.running;
  return err;
}
