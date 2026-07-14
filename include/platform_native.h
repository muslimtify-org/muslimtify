#ifndef PLATFORM_NATIVE_H
#define PLATFORM_NATIVE_H

#include <stddef.h>
#include <sys/types.h> /* ssize_t */

/*
 * Per-OS native primitives used by the portable POSIX platform layer
 * (src/platform/posix/platform_posix.c). Each supported OS supplies its own
 * implementation of these; the portable layer stays unchanged across targets.
 */

/*
 * Write the running executable's absolute path (null-terminated) into buf.
 * Returns the number of bytes written (> 0) on success, or -1 on failure
 * (buf is set to an empty string on failure).
 *   Linux: readlink("/proc/self/exe")  -- src/platform/linux/platform_linux.c
 */
ssize_t platform_native_exe_path(char *buf, size_t cap);

#endif /* PLATFORM_NATIVE_H */
