#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

// Number of elements in a fixed-size array. Evaluates to a size_t. Must be used
// on an actual array in scope, never a pointer (a decayed array parameter gives
// the pointer size, not the element count).
#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

#endif // UTIL_H
