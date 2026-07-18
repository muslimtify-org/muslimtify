#ifndef NATIVE_POSIX_H
#define NATIVE_POSIX_H

#include <stddef.h>
#include <sys/types.h> /* ssize_t */

// POSIX-only native primitives backing src/platform/posix/. Windows does not use
// this layer; it implements the equivalents directly in platform_win.c, so this
// header (and its POSIX-only ssize_t) is never compiled there.

// Write the running executable's absolute path (null-terminated) into buf.
// Returns bytes written (> 0), or -1 on failure (buf set to ""). Wraps
// readlink(), hence ssize_t.
ssize_t platform_native_exe_path(char *buf, size_t cap);

#endif /* NATIVE_POSIX_H */
