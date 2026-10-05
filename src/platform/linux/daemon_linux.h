#ifndef DAEMON_LINUX_H
#define DAEMON_LINUX_H

#include <stddef.h>

enum { DAEMON_UNIT_MAX = 1024 };

/* Renders the systemd user .service unit text into buffer. Returns the number
 * of bytes written (>0) or -1 on error/truncation. Pure: no I/O, no globals. */
int build_service_unit(const char *binary_path, char *buffer, size_t buffer_size);

#endif
