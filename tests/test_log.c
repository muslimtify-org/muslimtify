#define _POSIX_C_SOURCE 200809L
#include "log.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int passed = 0;
static int failed = 0;

static void check_bool(const char *test, bool cond) {
  if (cond) {
    passed++;
  } else {
    failed++;
    printf("FAIL [%s]\n", test);
  }
}

static int got_calls;
static MtLogLevel got_level;
static char got_message[2 * MT_LOG_MESSAGE_MAX];
static void *got_user_data;

static void record(MtLogLevel level, const char *message, void *user_data) {
  got_calls++;
  got_level = level;
  snprintf(got_message, sizeof(got_message), "%s", message);
  got_user_data = user_data;
}

static void reset_record(void) {
  got_calls = 0;
  got_level = MT_LOG_WARNING;
  got_message[0] = '\0';
  got_user_data = NULL;
}

// Redirect file descriptor 2 into a temporary file, so a test can read back
// exactly what reached stderr.
static FILE *capture_file;
static int capture_saved_fd;

static bool capture_begin(void) {
  fflush(stderr);
  capture_file = tmpfile();
  if (!capture_file)
    return false;
  capture_saved_fd = dup(STDERR_FILENO);
  dup2(fileno(capture_file), STDERR_FILENO);
  return true;
}

static void capture_end(char *buffer, size_t cap) {
  fflush(stderr);
  dup2(capture_saved_fd, STDERR_FILENO);
  close(capture_saved_fd);
  rewind(capture_file);
  size_t n = fread(buffer, 1, cap - 1, capture_file);
  buffer[n] = '\0';
  fclose(capture_file);
}

static void test_handler_receives(void) {
  printf("  a handler receives the message...\n");

  int marker = 0;
  char captured[256];
  reset_record();
  mt_log_set_handler(record, &marker);
  if (!capture_begin()) {
    check_bool("stderr capture", false);
    return;
  }
  mt_log(MT_LOG_ERROR, "plain message");
  capture_end(captured, sizeof(captured));

  check_bool("called once", got_calls == 1);
  check_bool("level passed", got_level == MT_LOG_ERROR);
  check_bool("text passed", strcmp(got_message, "plain message") == 0);
  check_bool("user data passed", got_user_data == &marker);
  check_bool("nothing on stderr with a handler set", captured[0] == '\0');

  reset_record();
  MT_LOGF(MT_LOG_WARNING, "value %d of %s", 7, "nine");
  check_bool("formatted once", got_calls == 1);
  check_bool("formatted level", got_level == MT_LOG_WARNING);
  check_bool("formatted text", strcmp(got_message, "value 7 of nine") == 0);

  reset_record();
  mt_log(MT_LOG_ERROR, NULL);
  check_bool("a NULL message is ignored", got_calls == 0);
}

static void test_null_handler_discards(void) {
  printf("  a NULL handler discards...\n");

  char captured[256];
  reset_record();
  mt_log_set_handler(NULL, NULL);
  if (!capture_begin()) {
    check_bool("stderr capture", false);
    return;
  }
  mt_log(MT_LOG_ERROR, "should vanish");
  MT_LOGF(MT_LOG_WARNING, "so should %s", "this");
  capture_end(captured, sizeof(captured));

  check_bool("no handler was called", got_calls == 0);
  check_bool("nothing on stderr", captured[0] == '\0');
}

static void test_default_writes_stderr(void) {
  printf("  the default handler writes to stderr...\n");

  char captured[256];
  mt_log_set_handler(mt_log_stderr, NULL);
  if (!capture_begin()) {
    check_bool("stderr capture", false);
    return;
  }
  mt_log(MT_LOG_ERROR, "Error: something failed");
  capture_end(captured, sizeof(captured));

  check_bool("the text and one newline", strcmp(captured, "Error: something failed\n") == 0);
}

static void test_long_message_truncated(void) {
  printf("  an over-long message is cut...\n");

  static char big[3 * MT_LOG_MESSAGE_MAX];
  memset(big, 'a', sizeof(big) - 1);
  big[sizeof(big) - 1] = '\0';

  reset_record();
  mt_log_set_handler(record, NULL);
  MT_LOGF(MT_LOG_WARNING, "%s", big);
  check_bool("delivered once", got_calls == 1);
  check_bool("cut to the maximum", strlen(got_message) == (size_t)MT_LOG_MESSAGE_MAX - 1);
}

int main(void) {
  printf("Running log tests...\n");
  test_handler_receives();
  test_null_handler_discards();
  test_default_writes_stderr();
  test_long_message_truncated();
  mt_log_set_handler(mt_log_stderr, NULL);

  printf("\nResults: %d passed, %d failed\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
