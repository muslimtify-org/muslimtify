#ifndef PLATFORM_WIN_H
#define PLATFORM_WIN_H

#include <wchar.h>

/* Converts UTF-8 text to a newly allocated wide string the caller frees.
 * Returns NULL for NULL text, on a conversion failure or when out of memory. */
wchar_t *platform_win_utf8_to_wide(const char *text);

#endif
