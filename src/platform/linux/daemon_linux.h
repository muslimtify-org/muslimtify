#ifndef DAEMON_LINUX_H
#define DAEMON_LINUX_H

#include <stdbool.h>
#include <stddef.h>

enum { DAEMON_UNIT_MAX = 1024 };

/* Renders the systemd user .service unit text into buffer. Returns the number
 * of bytes written (>0) or -1 on error/truncation. Pure: no I/O, no globals. */
int build_service_unit(const char *binary_path, char *buffer, size_t buffer_size);

/* True when a muslimtify.service file exists in any of the given directories.
 * Exposed so a test can point it at directories it controls. */
bool daemon_unit_in_dirs(const char *const *dirs, size_t count);

/* The first of the given paths that is an existing regular file the caller may
 * execute, or NULL when none is. Exposed so a test can give it paths it
 * controls. */
const char *daemon_find_binary(const char *const *candidates, size_t count);

#endif
