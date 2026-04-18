#define JSON_IMPLEMENTATION
#include "json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;
static int total = 0;

static void check_str(const char *result, const char *expected, const char *label) {
  total++;
  if (result == NULL && expected == NULL) {
    printf("  PASS: %s\n", label);
  } else if (result != NULL && expected != NULL && strcmp(result, expected) == 0) {
    printf("  PASS: %s\n", label);
  } else {
    printf("  FAIL: %s — got \"%s\", expected \"%s\"\n", label, result ? result : "(null)",
           expected ? expected : "(null)");
    failures++;
  }
}

static void check_null(const char *result, const char *label) {
  total++;
  if (result == NULL) {
    printf("  PASS: %s\n", label);
  } else {
    printf("  FAIL: %s — expected NULL, got \"%s\"\n", label, result);
    failures++;
  }
}

static void check_not_null(const void *result, const char *label) {
  total++;
  if (result != NULL) {
    printf("  PASS: %s\n", label);
  } else {
    printf("  FAIL: %s — expected non-NULL\n", label);
    failures++;
  }
}

/* -- Basic extraction ------------------------------------------------------- */

static void test_basic_string(void) {
  printf("test_basic_string\n");
  JsonContext *ctx = json_begin();
  char json[] = "{\"name\": \"muslimtify\"}";
  char *val = get_value(ctx, "name", json);
  check_str(val, "muslimtify", "string value");
  json_end(ctx);
}

static void test_basic_number(void) {
  printf("test_basic_number\n");
  JsonContext *ctx = json_begin();
  char json[] = "{\"count\": 42}";
  char *val = get_value(ctx, "count", json);
  check_str(val, "42", "number value");
  json_end(ctx);
}

static void test_basic_boolean(void) {
  printf("test_basic_boolean\n");
  JsonContext *ctx = json_begin();
  char json[] = "{\"enabled\": true}";
  char *val = get_value(ctx, "enabled", json);
  check_str(val, "true", "boolean value");
  json_end(ctx);
}

static void test_basic_null(void) {
  printf("test_basic_null\n");
  JsonContext *ctx = json_begin();
  char json[] = "{\"value\": null}";
  char *val = get_value(ctx, "value", json);
  check_str(val, "null", "null value");
  json_end(ctx);
}

static void test_empty_string_value(void) {
  printf("test_empty_string_value\n");
  JsonContext *ctx = json_begin();
  char json[] = "{\"key\": \"\"}";
  char *val = get_value(ctx, "key", json);
  check_not_null(val, "empty string is non-NULL");
  check_str(val, "", "empty string value");
  json_end(ctx);
}

/* -- Nested objects --------------------------------------------------------- */

static void test_nested_object(void) {
  printf("test_nested_object\n");
  JsonContext *ctx = json_begin();
  char json[] = "{\"location\": {\"lat\": 1.5, \"lon\": 2.0}}";
  char *loc = get_value(ctx, "location", json);
  check_not_null(loc, "nested object extracted");

  char *lat = get_value(ctx, "lat", loc);
  check_str(lat, "1.5", "nested lat value");

  char *lon = get_value(ctx, "lon", loc);
  check_str(lon, "2.0", "nested lon value");
  json_end(ctx);
}

/* -- Missing keys ----------------------------------------------------------- */

static void test_missing_key(void) {
  printf("test_missing_key\n");
  JsonContext *ctx = json_begin();
  char json[] = "{\"name\": \"test\"}";
  char *val = get_value(ctx, "nonexistent", json);
  check_null(val, "missing key returns NULL");
  json_end(ctx);
}

static void test_empty_object(void) {
  printf("test_empty_object\n");
  JsonContext *ctx = json_begin();
  char json[] = "{}";
  char *val = get_value(ctx, "key", json);
  check_null(val, "empty object returns NULL");
  json_end(ctx);
}

/* -- Escaped strings -------------------------------------------------------- */

static void test_escaped_quote(void) {
  printf("test_escaped_quote\n");
  JsonContext *ctx = json_begin();
  char json[] = "{\"msg\": \"hello\\\"world\"}";
  char *val = get_value(ctx, "msg", json);
  check_str(val, "hello\"world", "escaped quote");
  json_end(ctx);
}

static void test_escaped_backslash(void) {
  printf("test_escaped_backslash\n");
  JsonContext *ctx = json_begin();
  char json[] = "{\"path\": \"c:\\\\dir\"}";
  char *val = get_value(ctx, "path", json);
  check_str(val, "c:\\dir", "escaped backslash");
  json_end(ctx);
}

static void test_escaped_newline(void) {
  printf("test_escaped_newline\n");
  JsonContext *ctx = json_begin();
  char json[] = "{\"text\": \"line1\\nline2\"}";
  char *val = get_value(ctx, "text", json);
  check_str(val, "line1\nline2", "escaped newline");
  json_end(ctx);
}

static void test_escaped_tab(void) {
  printf("test_escaped_tab\n");
  JsonContext *ctx = json_begin();
  char json[] = "{\"text\": \"col1\\tcol2\"}";
  char *val = get_value(ctx, "text", json);
  check_str(val, "col1\tcol2", "escaped tab");
  json_end(ctx);
}

static void test_escaped_keys(void) {
  printf("test_escaped_keys\n");
  JsonContext *ctx = json_begin();
  char json[] = "{\"line\\nname\": \"line value\", \"quote\\\"key\": \"quote value\", "
                "\"unicod\\u00E9\": \"unicode value\"}";

  char *line = get_value(ctx, "line\nname", json);
  check_str(line, "line value", "newline key");

  char *quote = get_value(ctx, "quote\"key", json);
  check_str(quote, "quote value", "quote key");

  const char unicode_key[] = "unicodé";
  char *unicode_val = get_value(ctx, unicode_key, json);
  check_str(unicode_val, "unicode value", "unicode key");

  json_end(ctx);
}

static void test_unicode_passthrough(void) {
  printf("test_unicode_passthrough\n");
  JsonContext *ctx = json_begin();
  char json[] = "{\"u\": \"\\u0041\"}";
  char *val = get_value(ctx, "u", json);
  check_str(val, "\\u0041", "unicode passthrough");
  json_end(ctx);
}

/* -- Edge cases ------------------------------------------------------------- */

static void test_key_inside_value(void) {
  printf("test_key_inside_value\n");
  JsonContext *ctx = json_begin();
  char json[] = "{\"city\": \"lat=1.0\", \"lat\": \"2.0\"}";
  char *val = get_value(ctx, "lat", json);
  check_str(val, "2.0", "key inside value not confused");
  json_end(ctx);
}

static void test_multiple_get_value(void) {
  printf("test_multiple_get_value\n");
  JsonContext *ctx = json_begin();
  char json[] = "{\"a\": \"first\", \"b\": \"second\", \"c\": \"third\"}";

  char *a = get_value(ctx, "a", json);
  char *b = get_value(ctx, "b", json);
  char *c = get_value(ctx, "c", json);

  // Earlier results must remain valid (arena doesn't overwrite)
  check_str(a, "first", "first value still valid");
  check_str(b, "second", "second value still valid");
  check_str(c, "third", "third value still valid");
  json_end(ctx);
}

/* -- Lifecycle -------------------------------------------------------------- */

static void test_json_begin(void) {
  printf("test_json_begin\n");
  JsonContext *ctx = json_begin();
  check_not_null(ctx, "json_begin returns non-NULL");
  json_end(ctx);
}

static void test_json_end_null(void) {
  printf("test_json_end_null\n");
  // Should not crash or print to stderr
  json_end(NULL);
  total++;
  printf("  PASS: json_end(NULL) does not crash\n");
}

static void test_arena_large_alignment(void) {
  printf("test_arena_large_alignment\n");
  JsonArena *arena = json_alloc_init();
  size_t size = ARENA_BLOCK_SIZE + 256;
  size_t alignment = JSON_ALIGNOF(double) * 2;

  void *ptr = json_alloc(arena, size, alignment);
  check_not_null(ptr, "large aligned allocation");

  total++;
  size_t required_cap = size + alignment - 1;
  if (arena && arena->current && arena->current->cap >= required_cap) {
    printf("  PASS: arena capacity respected for large allocation\n");
  } else {
    printf("  FAIL: arena capacity overflowed for large allocation\n");
    failures++;
  }

  json_alloc_free(arena);
}

/* -- Main ------------------------------------------------------------------- */

int main(void) {
  printf("=== json.h tests ===\n\n");

  test_json_begin();
  test_json_end_null();

  test_basic_string();
  test_basic_number();
  test_basic_boolean();
  test_basic_null();
  test_empty_string_value();

  test_nested_object();

  test_missing_key();
  test_empty_object();

  test_escaped_quote();
  test_escaped_backslash();
  test_escaped_newline();
  test_escaped_tab();
  test_escaped_keys();
  test_unicode_passthrough();

  test_key_inside_value();
  test_multiple_get_value();

  test_arena_large_alignment();

  printf("\n%d/%d tests passed\n", total - failures, total);
  return failures > 0 ? 1 : 0;
}
