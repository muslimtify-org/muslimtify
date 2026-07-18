#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "platform_native.h"

#include <unistd.h>

/* Linux-specific: resolve the running executable via /proc/self/exe. */
ssize_t platform_native_exe_path(char *buf, size_t cap) {
  if (!buf || cap == 0)
    return -1;

  ssize_t len = readlink("/proc/self/exe", buf, cap - 1);
  if (len > 0) {
    buf[len] = '\0';
    return len;
  }

  buf[0] = '\0';
  return -1;
}
