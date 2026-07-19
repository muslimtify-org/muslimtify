#include "string_util.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <wchar.h>
#endif

static int total = 0;
static int failures = 0;

static void report_result(const char *label, bool pass) {
  total++;
  if (pass) {
    printf("  PASS: %s\n", label);
  } else {
    printf("  FAIL: %s\n", label);
    failures++;
  }
}

static void expect_true(bool value, const char *label) {
  report_result(label, value);
}

static void expect_false(bool value, const char *label) {
  report_result(label, !value);
}

static void expect_str(const char *buf, const char *expected, const char *label) {
  bool pass = (buf && expected && strcmp(buf, expected) == 0);
  report_result(label, pass);
}

static void test_copy_string_success(void) {
  printf("test_copy_string_success\n");
  char buffer[16];
  bool ok = copy_string(buffer, sizeof(buffer), "muslimtify");
  expect_true(ok, "copy reports success");
  expect_str(buffer, "muslimtify", "copy copies value");
}

static void test_copy_string_truncate(void) {
  printf("test_copy_string_truncate\n");
  char buffer[5];
  bool ok = copy_string(buffer, sizeof(buffer), "abcdef");
  expect_false(ok, "copy reports truncation");
  expect_str(buffer, "abcd", "copy truncates safely");
}

static void test_append_string_success(void) {
  printf("test_append_string_success\n");
  char buffer[16] = "foo";
  bool ok = append_string(buffer, sizeof(buffer), "bar");
  expect_true(ok, "append reports success");
  expect_str(buffer, "foobar", "append concatenates");
}

static void test_append_string_truncate(void) {
  printf("test_append_string_truncate\n");
  char buffer[6] = "foo";
  bool ok = append_string(buffer, sizeof(buffer), "barbaz");
  expect_false(ok, "append reports truncation");
  expect_str(buffer, "fooba", "append truncates safely");
}

static void test_bounded_strlen(void) {
  printf("test_bounded_strlen\n");
  size_t len = bounded_strlen("abcdef", 3);
  report_result("bounded strlen respects limit", len == 3);
  len = bounded_strlen("abc", 10);
  report_result("bounded strlen full length", len == 3);
}

static void test_errno_string_success(void) {
  printf("test_errno_string_success\n");
  char buffer[64];
  bool ok = errno_string(ENOENT, buffer, sizeof(buffer));
  expect_true(ok, "errno_string succeeds for ENOENT");
  report_result("errno_string writes string", buffer[0] != '\0');
}

static void test_errno_string_invalid_args(void) {
  printf("test_errno_string_invalid_args\n");
  bool ok = errno_string(ENOENT, NULL, 0);
  expect_false(ok, "errno_string rejects invalid buffer");
}

typedef struct TokenAccumulator {
  int count;
  char values[4][8];
} TokenAccumulator;

static bool collect_token(const char *token, void *user) {
  TokenAccumulator *acc = (TokenAccumulator *)user;
  if (acc->count >= 4) {
    return false;
  }
  copy_string(acc->values[acc->count], sizeof(acc->values[acc->count]), token);
  acc->count++;
  return true;
}

static bool stop_after_first(const char *token, void *user) {
  (void)token;
  TokenAccumulator *acc = (TokenAccumulator *)user;
  acc->count++;
  return false;
}

static void test_parse_tokens_success(void) {
  printf("test_parse_tokens_success\n");
  const char *input = "10,20,30";
  char scratch[32];
  TokenAccumulator acc = {0};
  int rc = parse_tokens(input, scratch, sizeof(scratch), ",", collect_token, &acc);
  report_result("parse_tokens success", rc == 0);
  report_result("parse_tokens count", acc.count == 3);
  expect_str(acc.values[0], "10", "token 1");
  expect_str(acc.values[1], "20", "token 2");
  expect_str(acc.values[2], "30", "token 3");
}

static void test_parse_tokens_truncation(void) {
  printf("test_parse_tokens_truncation\n");
  const char *input = "123456789";
  char scratch[5];
  TokenAccumulator acc = {0};
  int rc = parse_tokens(input, scratch, sizeof(scratch), ",", collect_token, &acc);
  report_result("parse_tokens truncation", rc == -1);
  report_result("parse_tokens truncation no tokens", acc.count == 0);
}

static void test_parse_tokens_callback_stop(void) {
  printf("test_parse_tokens_callback_stop\n");
  const char *input = "a,b,c";
  char scratch[16];
  TokenAccumulator acc = {0};
  int rc = parse_tokens(input, scratch, sizeof(scratch), ",", stop_after_first, &acc);
  report_result("parse_tokens callback stop", rc == -2);
  report_result("parse_tokens callback count", acc.count == 1);
}

int main(void) {
  printf("=== string_util tests ===\n\n");

  test_copy_string_success();
  test_copy_string_truncate();
  test_append_string_success();
  test_append_string_truncate();
  test_bounded_strlen();
  test_errno_string_success();
  test_errno_string_invalid_args();
  test_parse_tokens_success();
  test_parse_tokens_truncation();
  test_parse_tokens_callback_stop();

  printf("\n%d/%d tests passed\n", total - failures, total);
  return failures > 0 ? 1 : 0;
}
