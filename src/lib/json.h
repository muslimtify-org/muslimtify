/*
 * Copyright (c) 2025 Rizki Rakasiwi <rizkirr.xyz@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef JSON_H
#define JSON_H

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

// Define restrict keyword for portability
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
// C99 or later - restrict is available
#define JSON_RESTRICT restrict
#elif defined(__GNUC__) || defined(__clang__)
// GCC/Clang extension
#define JSON_RESTRICT __restrict__
#elif defined(_MSC_VER)
// MSVC extension
#define JSON_RESTRICT __restrict
#else
// No restrict support
#define JSON_RESTRICT
#endif

typedef struct JsonContext JsonContext;

/**
 * Initialize a new JSON parsing context.
 * Returns a new JsonContext or NULL on failure.
 */
JsonContext *json_begin(void);

/**
 * Free the JSON context and all associated memory.
 * @param ctx The context to free
 */
void json_end(JsonContext *ctx);

/**
 * Extract a value for the given key from a JSON object.
 * @param ctx The JSON context
 * @param key The key to search for
 * @param raw_json The raw JSON object string
 * @return Pointer to the extracted value string, or NULL if not found
 */
char *get_value(JsonContext *JSON_RESTRICT ctx, const char *JSON_RESTRICT key,
                char *JSON_RESTRICT raw_json);

#ifdef JSON_IMPLEMENTATION

#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ARENA

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#include <stdalign.h>
#define JSON_ALIGNOF(type) alignof(type)
#elif defined(__GNUC__) || defined(__clang__)
#define JSON_ALIGNOF(type) __alignof__(type)
#elif defined(_MSC_VER)
#define JSON_ALIGNOF(type) __alignof(type)
#else
#define JSON_ALIGNOF(type) \
  offsetof(                \
      struct {             \
        char c;            \
        type d;            \
      },                   \
      d)
#endif

#define JSON_OBJECT_OPEN '{'
#define JSON_OBJECT_CLOSE '}'
#define JSON_ARRAY_OPEN '['
#define JSON_ARRAY_CLOSE ']'
#define JSON_STRING_QUOTE '"'
#define JSON_ESCAPE_CHAR '\\'
#define JSON_KEY_VALUE_SEP ':'
#define JSON_VALUE_SEP ','

#ifndef ARENA_BLOCK_SIZE
#define ARENA_BLOCK_SIZE (1024UL * 4)
#endif

#define JSON_DEPTH_LIMIT 100

typedef struct Region Region;
struct Region {
  Region *next;
  size_t cap;
  size_t index;
  uint8_t *data;
};

typedef struct JsonArena {
  Region *head;
  Region *current;
  size_t block_size;
} JsonArena;

typedef struct AlignProbe {
  uintptr_t address;
  size_t alignment;
} AlignProbe;

typedef struct JsonSlice {
  const char *ptr;
  size_t length;
} JsonSlice;

typedef struct JsonUtf8 {
  unsigned char bytes[3];
  size_t length;
} JsonUtf8;

static Region *json_region_create(size_t block_size) {
  Region *region = (Region *)malloc(sizeof(Region) + block_size);
  if (!region) {
    fprintf(stderr, "Memory allocation fail, Buy more RAM LOL!\n");
    return NULL;
  }

  region->next = NULL;
  region->cap = block_size;
  region->index = 0;
  region->data = (uint8_t *)(region + 1);
  return region;
}

static JsonSlice json_slice_make(const char *ptr, size_t length) {
  JsonSlice slice;
  slice.ptr = ptr;
  slice.length = length;
  return slice;
}

static JsonSlice json_slice_from_range(const char *start, const char *end) {
  if (!start || !end || end < start) {
    return json_slice_make(NULL, 0);
  }
  return json_slice_make(start, (size_t)(end - start));
}

static JsonSlice json_slice_from_cstr(const char *str) {
  if (!str) {
    return json_slice_make(NULL, 0);
  }
  return json_slice_make(str, strlen(str));
}

static int json_hex_digit_value(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return 10 + (c - 'a');
  }
  if (c >= 'A' && c <= 'F') {
    return 10 + (c - 'A');
  }
  return -1;
}

static bool json_utf8_encode(uint32_t codepoint, JsonUtf8 *utf8) {
  if (!utf8)
    return false;

  if (codepoint <= 0x7F) {
    utf8->bytes[0] = (unsigned char)codepoint;
    utf8->length = 1;
    return true;
  }

  if (codepoint <= 0x7FF) {
    utf8->bytes[0] = (unsigned char)(0xC0 | (codepoint >> 6));
    utf8->bytes[1] = (unsigned char)(0x80 | (codepoint & 0x3F));
    utf8->length = 2;
    return true;
  }

  if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
    return false;
  }

  utf8->bytes[0] = (unsigned char)(0xE0 | (codepoint >> 12));
  utf8->bytes[1] = (unsigned char)(0x80 | ((codepoint >> 6) & 0x3F));
  utf8->bytes[2] = (unsigned char)(0x80 | (codepoint & 0x3F));
  utf8->length = 3;
  return true;
}

static bool json_slice_equals(JsonSlice lhs, JsonSlice rhs) {
  if (!lhs.ptr || !rhs.ptr) {
    return false;
  }

  const char *lhs_cur = lhs.ptr;
  const char *lhs_end = lhs.ptr + lhs.length;
  const unsigned char *rhs_cur = (const unsigned char *)rhs.ptr;
  const unsigned char *rhs_end = (const unsigned char *)(rhs.ptr + rhs.length);

  while (lhs_cur < lhs_end && rhs_cur < rhs_end) {
    JsonUtf8 to_compare;
    to_compare.length = 1;

    if (*lhs_cur == JSON_ESCAPE_CHAR) {
      lhs_cur++;
      if (lhs_cur >= lhs_end) {
        return false;
      }

      char escape = *lhs_cur++;
      switch (escape) {
      case '"':
        to_compare.bytes[0] = '"';
        break;
      case '\\':
        to_compare.bytes[0] = '\\';
        break;
      case '/':
        to_compare.bytes[0] = '/';
        break;
      case 'b':
        to_compare.bytes[0] = '\b';
        break;
      case 'f':
        to_compare.bytes[0] = '\f';
        break;
      case 'n':
        to_compare.bytes[0] = '\n';
        break;
      case 'r':
        to_compare.bytes[0] = '\r';
        break;
      case 't':
        to_compare.bytes[0] = '\t';
        break;
      case 'u': {
        if ((size_t)(lhs_end - lhs_cur) < 4) {
          return false;
        }

        uint32_t value = 0;
        for (int i = 0; i < 4; i++) {
          int digit = json_hex_digit_value(*lhs_cur++);
          if (digit < 0) {
            return false;
          }
          value = (value << 4) | (uint32_t)digit;
        }

        if (!json_utf8_encode(value, &to_compare)) {
          return false;
        }
        break;
      }
      default:
        return false;
      }
    } else {
      to_compare.bytes[0] = (unsigned char)*lhs_cur++;
    }

    for (size_t i = 0; i < to_compare.length; i++) {
      if (rhs_cur >= rhs_end || *rhs_cur != to_compare.bytes[i]) {
        return false;
      }
      rhs_cur++;
    }
  }

  return lhs_cur == lhs_end && rhs_cur == rhs_end;
}

static size_t json_alloc_align(AlignProbe probe) {
  return (probe.alignment - (probe.address % probe.alignment)) % probe.alignment;
}

static bool json_safe_add(size_t a, size_t b, size_t *result) {
  if (b > SIZE_MAX - a) {
    return false;
  }
  *result = a + b;
  return true;
}

static JsonArena *json_alloc_init(void) {
  JsonArena *arena = (JsonArena *)malloc(sizeof(JsonArena));
  if (arena == NULL) {
    fprintf(stderr, "Memory allocation fail, Buy more RAM LOL!\n");
    return NULL;
  }

  arena->block_size = ARENA_BLOCK_SIZE;
  arena->head = NULL;
  arena->current = NULL;
  return arena;
}

static void *json_alloc(JsonArena *arena, size_t size, size_t alignment) {
  if (!arena || size == 0 || alignment == 0) {
    fprintf(stderr, "Invalid arguments to json_alloc\n");
    return NULL;
  }

  // Check if alignment is a power of two (expected to be true)
#if defined(__GNUC__) || defined(__clang__)
  if (__builtin_expect((alignment & (alignment - 1)) != 0, 0))
#else
  if (alignment & (alignment - 1))
#endif
    return NULL;

  size_t slack = (alignment > 1) ? (alignment - 1) : 0;
  size_t size_with_slack = size;
  if (slack > 0 && !json_safe_add(size, slack, &size_with_slack)) {
    return NULL;
  }

  if (!arena->current) {
    size_t initial_cap =
        (size_with_slack > arena->block_size) ? size_with_slack : arena->block_size;
    Region *region = json_region_create(initial_cap);
    if (!region) {
      return NULL;
    }

    arena->head = arena->current = region;
  }

  uintptr_t current_ptr = (uintptr_t)(arena->current->data + arena->current->index);
  AlignProbe probe = {current_ptr, alignment};
  size_t padding = json_alloc_align(probe);

  if (arena->current->index + padding + size > arena->current->cap) {
    size_t next_cap = (size_with_slack > arena->block_size) ? size_with_slack : arena->block_size;
    Region *next = json_region_create(next_cap);
    if (!next) {
      return NULL;
    }

    arena->current->next = next;
    arena->current = next;

    current_ptr = (uintptr_t)next->data;
    probe.address = current_ptr;
    padding = json_alloc_align(probe);
  }

  arena->current->index += padding;
  void *ptr = arena->current->data + arena->current->index;
  arena->current->index += size;

  return ptr;
}

static void json_alloc_free(JsonArena *arena) {
  if (!arena)
    return;
  Region *region = arena->head;
  while (region) {
    Region *next = region->next;
    free(region);
    region = next;
  }
  free(arena);
}

// Json Parser

struct JsonContext {
  JsonArena *arena;
};

static inline const char *skip_whitespace(const char *cursor) {
  while (*cursor && isspace(*cursor))
    cursor++;
  return cursor;
}

static const char *find_matching_bracket(const char *start, char open_bracket) {
  if (*start != open_bracket) {
    fprintf(stderr, "expected '%c'\n", open_bracket);
    return NULL;
  }

  char close_bracket = (open_bracket == JSON_OBJECT_OPEN) ? JSON_OBJECT_CLOSE : JSON_ARRAY_CLOSE;
  const char *cursor = start + 1;
  int depth = 1;
  bool in_string = false;
  bool escaped = false;

  while (*cursor && depth > 0) {
    if (in_string) {
      if (escaped) {
        escaped = false;
      } else if (*cursor == JSON_ESCAPE_CHAR) {
        escaped = true;
      } else if (*cursor == JSON_STRING_QUOTE) {
        in_string = false;
      }
    } else {
      if (*cursor == JSON_STRING_QUOTE) {
        in_string = true;
      } else if (*cursor == open_bracket) {
        depth++;
        // Enforce depth limit
        if (depth > JSON_DEPTH_LIMIT) {
          fprintf(stderr, "depth exceeds limit %d\n", JSON_DEPTH_LIMIT);
          return NULL;
        }
      } else if (*cursor == close_bracket) {
        depth--;
        if (depth == 0) {
          return cursor;
        }
      }
    }
    cursor++;
  }

  fprintf(stderr, "no matching '%c'\n", close_bracket);
  return NULL;
}

static char *json_find_key(JsonSlice key, char *JSON_RESTRICT json) {
  if (!key.ptr || !json) {
    fprintf(stderr, "Invalid arguments, expected key and raw json\n");
    return NULL;
  }

  const char *cursor = json;
  int depth = 0;
  bool in_string = false;
  bool escaped = false;

  while (*cursor) {
    char current_char = *cursor;

    if (in_string) {
      if (escaped) {
        escaped = false;
      } else if (current_char == JSON_ESCAPE_CHAR) {
        escaped = true;
      } else if (current_char == JSON_STRING_QUOTE) {
        in_string = false;
      }

      cursor++;
      continue;
    }

    switch (current_char) {
    case JSON_STRING_QUOTE:
      if (depth == 1) {
        const char *key_start = cursor + 1;
        const char *key_end = key_start;

        while (*key_end) {
          if (*key_end == JSON_ESCAPE_CHAR) {
            key_end++;
            if (!*key_end) {
              break;
            }
          } else if (*key_end == JSON_STRING_QUOTE) {
            break;
          }
          key_end++;
        }

        if (*key_end != JSON_STRING_QUOTE) {
          return NULL;
        }

        JsonSlice candidate = json_slice_from_range(key_start, key_end);
        if (json_slice_equals(candidate, key)) {
          cursor = skip_whitespace(key_end + 1);

          if (*cursor == JSON_KEY_VALUE_SEP) {
            cursor = skip_whitespace(cursor + 1);
            return (char *)cursor;
          }
        }
      }
      in_string = true;
      break;
    case JSON_ARRAY_OPEN:
    case JSON_OBJECT_OPEN:
      depth++;
      if (depth > JSON_DEPTH_LIMIT) {
        fprintf(stderr, "depth exceeds limit %d\n", JSON_DEPTH_LIMIT);
        return NULL;
      }
      break;
    case JSON_ARRAY_CLOSE:
    case JSON_OBJECT_CLOSE:
      depth--;
      break;
    default:
      break;
    }

    cursor++;
  }

  return NULL;
}

static char *json_extract_value(JsonArena *JSON_RESTRICT arena,
                                const char *JSON_RESTRICT value_start) {

  if (!value_start || !arena) {
    fprintf(stderr, "Expected JsonArena and value_start");
    return NULL;
  }

  size_t value_len;
  const char *cursor = skip_whitespace(value_start);
  const char *value_end;

  if (*cursor == '"') {
    // String value — find end first to determine max length
    cursor++;
    const char *scan = cursor;
    while (*scan && *scan != '"') {
      if (*scan == '\\')
        scan++; // Skip escaped char
      scan++;
    }
    size_t raw_len = scan - cursor;

    // Allocate raw_len + 1 (unescaped is always <= raw length)
    char *result = json_alloc(arena, raw_len + 1, JSON_ALIGNOF(char));
    if (!result)
      return NULL;

    // Copy with unescape
    char *dst = result;
    const char *src = cursor;
    while (src < scan) {
      if (*src == '\\' && src + 1 < scan) {
        src++;
        switch (*src) {
        case '"':
          *dst++ = '"';
          break;
        case '\\':
          *dst++ = '\\';
          break;
        case '/':
          *dst++ = '/';
          break;
        case 'n':
          *dst++ = '\n';
          break;
        case 'r':
          *dst++ = '\r';
          break;
        case 't':
          *dst++ = '\t';
          break;
        case 'b':
          *dst++ = '\b';
          break;
        case 'f':
          *dst++ = '\f';
          break;
        case 'u':
          // \uXXXX — pass through as-is (6 chars)
          *dst++ = '\\';
          *dst++ = 'u';
          for (int i = 0; i < 4 && src + 1 < scan; i++) {
            src++;
            *dst++ = *src;
          }
          break;
        default:
          // Unknown escape — keep as-is
          *dst++ = '\\';
          *dst++ = *src;
          break;
        }
      } else {
        *dst++ = *src;
      }
      src++;
    }
    *dst = '\0';
    return result;

  } else if (*cursor == '{' || *cursor == '[') {
    // Object or array - use helper to find matching bracket
    const char *start = cursor;
    char open = *cursor;
    value_end = find_matching_bracket(cursor, open);

    if (!value_end)
      return NULL;

    value_len = value_end - start + 1; // Include closing bracket
    char *result = json_alloc(arena, value_len + 1, JSON_ALIGNOF(char));
    if (!result)
      return NULL;
    memcpy(result, start, value_len);
    result[value_len] = '\0';
    return result;

  } else {
    // Number, boolean, or null - find end
    value_end = cursor;
    while (*value_end && !isspace(*value_end) && *value_end != ',' && *value_end != '}' &&
           *value_end != ']') {
      value_end++;
    }

    value_len = value_end - cursor;
    char *result = json_alloc(arena, value_len + 1, JSON_ALIGNOF(char));
    if (!result)
      return NULL;
    memcpy(result, cursor, value_len);
    result[value_len] = '\0';
    return result;
  }
}

JsonContext *json_begin(void) {
  JsonArena *arena = json_alloc_init();
  if (!arena) {
    fprintf(stderr, "Cannot allocate memory arena for json_begin\n");
    return NULL;
  }

  JsonContext *ctx = json_alloc(arena, sizeof(JsonContext), JSON_ALIGNOF(JsonContext));
  if (!ctx) {
    fprintf(stderr, "Cannot create JsonContext, please check your RAM usage\n");
    json_alloc_free(arena);
    return NULL;
  }

  ctx->arena = arena;
  return ctx;
}

void json_end(JsonContext *ctx) {
  if (!ctx) {
    return;
  }

  json_alloc_free(ctx->arena);
}

static inline char *get_obj(JsonArena *JSON_RESTRICT arena, char *JSON_RESTRICT json,
                            const char *JSON_RESTRICT key) {
  if (!json || !arena || !key) {
    fprintf(stderr, "Invalid arguments to get_obj\n");
    return NULL;
  }

  JsonSlice key_slice = json_slice_from_cstr(key);
  char *value = json_find_key(key_slice, json);
  if (!value) {
    return NULL;
  }

  char *result = json_extract_value(arena, value);
  return result;
}

char *get_value(JsonContext *JSON_RESTRICT ctx, const char *JSON_RESTRICT key,
                char *JSON_RESTRICT raw_json) {
  if (!ctx || !ctx->arena)
    return NULL;

  return get_obj(ctx->arena, raw_json, key);
}

#endif /* JSON_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif

#endif /* JSON_H */
