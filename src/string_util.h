#ifndef STRING_UTIL_H
#define STRING_UTIL_H

#include <stdbool.h>
#include <stddef.h>

#ifdef _WIN32
#include <wchar.h>
#endif

// copy_string copies src into dst (including the null terminator) provided dst_size > 0.
// Returns true when the entire source fits; false on truncation or invalid args.
bool copy_string(char *dst, size_t dst_size, const char *src);

// append_string concatenates src to dst without exceeding dst_size.
// Returns true when everything fits; false if truncation occurred or arguments invalid.
bool append_string(char *dst, size_t dst_size, const char *src);

// bounded_strlen returns the length of s up to maxlen characters (stops at maxlen).
size_t bounded_strlen(const char *s, size_t maxlen);

// errno_string writes a human-readable message for err into buf.
// Returns true on success; false when err cannot be represented or inputs invalid.
bool errno_string(int err, char *buf, size_t buf_sz);

typedef bool (*token_cb)(const char *token, void *user);

// parse_tokens tokenizes input using the provided delimiters, invoking cb for each token.
// scratch must be a writable buffer used to copy input. Returns 0 on success,
// -1 on invalid arguments or copy failures, -2 when the callback stops iteration.
int parse_tokens(const char *input, char *scratch, size_t scratch_size, const char *delims,
                 token_cb cb, void *user);

#endif // STRING_UTIL_H
