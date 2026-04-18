#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "string_util.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <wchar.h>
#endif

static bool log_invalid_once(const char *name, bool *logged_flag, const char *detail);

size_t bounded_strlen(const char *s, size_t maxlen) {
  size_t len = 0;
  if (!s) {
    return 0;
  }
  while (len < maxlen && s[len] != '\0') {
    len++;
  }
  return len;
}

bool copy_string(char *dst, size_t dst_size, const char *src) {
  static bool invalid_logged = false;
  if (!dst || !src || dst_size == 0) {
    return log_invalid_once("copy_string", &invalid_logged, "invalid arguments");
  }

  size_t limit = dst_size - 1;
  size_t src_len = strlen(src);
  size_t copy_len = src_len < limit ? src_len : limit;
  bool fits = src_len <= limit;
  if (copy_len > 0) {
    memcpy(dst, src, copy_len);
  }
  dst[copy_len] = '\0';
  return fits;
}

bool append_string(char *dst, size_t dst_size, const char *src) {
  static bool invalid_logged = false;
  static bool unterminated_logged = false;
  if (!dst || !src || dst_size == 0) {
    return log_invalid_once("append_string", &invalid_logged, "invalid arguments");
  }

  size_t dst_len = bounded_strlen(dst, dst_size);
  if (dst_len == dst_size) {
    dst[dst_size - 1] = '\0';
    return log_invalid_once("append_string", &unterminated_logged,
                            "destination missing terminator");
  }

  size_t remaining = dst_size - dst_len - 1;
  size_t copy_len = bounded_strlen(src, remaining);
  bool fits = src[copy_len] == '\0';
  if (copy_len > 0) {
    memcpy(dst + dst_len, src, copy_len);
  }
  dst[dst_len + copy_len] = '\0';
  return fits;
}

bool errno_string(int err, char *buf, size_t buf_sz) {
  static bool invalid_logged = false;
  if (!buf || buf_sz == 0) {
    return log_invalid_once("errno_string", &invalid_logged, "invalid buffer");
  }

#ifdef _WIN32
  if (strerror_s(buf, buf_sz, err) == 0) {
    return true;
  }
#else
#if defined(__GLIBC__) && defined(_GNU_SOURCE)
  char *result = strerror_r(err, buf, buf_sz);
  if (result) {
    if (result != buf) {
      return copy_string(buf, buf_sz, result);
    }
    return true;
  }
#else
  if (strerror_r(err, buf, buf_sz) == 0) {
    return true;
  }
#endif
#endif

  snprintf(buf, buf_sz, "unknown error %d", err);
  return false;
}

#ifdef _WIN32
static size_t bounded_wcslen(const wchar_t *s, size_t maxlen) {
  size_t len = 0;
  if (!s) {
    return 0;
  }
  while (len < maxlen && s[len] != L'\0') {
    len++;
  }
  return len;
}

bool copy_wstring(wchar_t *dst, size_t dst_len, const wchar_t *src) {
  static bool invalid_logged = false;
  if (!dst || !src || dst_len == 0) {
    return log_invalid_once("copy_wstring", &invalid_logged, "invalid arguments");
  }

  size_t limit = dst_len - 1;
  size_t copy_len = bounded_wcslen(src, limit);
  bool fits = src[copy_len] == L'\0';
  if (copy_len > 0) {
    wmemcpy(dst, src, copy_len);
  }
  dst[copy_len] = L'\0';
  return fits;
}
#endif

int parse_tokens(const char *input, char *scratch, size_t scratch_size, const char *delims,
                 token_cb cb, void *user) {
  static bool invalid_logged = false;
  if (!input || !scratch || scratch_size == 0 || !delims || !cb) {
    log_invalid_once("parse_tokens", &invalid_logged, "invalid arguments");
    return -1;
  }

  if (!copy_string(scratch, scratch_size, input)) {
    return -1;
  }

#ifdef _WIN32
  char *context = NULL;
  char *token = strtok_s(scratch, delims, &context);
#else
  char *save = NULL;
  char *token = strtok_r(scratch, delims, &save);
#endif

  while (token) {
    if (!cb(token, user)) {
      return -2;
    }
#ifdef _WIN32
    token = strtok_s(NULL, delims, &context);
#else
    token = strtok_r(NULL, delims, &save);
#endif
  }

  return 0;
}

static bool log_invalid_once(const char *name, bool *logged_flag, const char *detail) {
  if (!*logged_flag) {
    fprintf(stderr, "string_util: %s %s\n", name, detail);
    *logged_flag = true;
  }
  return false;
}
