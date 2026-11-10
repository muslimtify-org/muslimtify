#ifndef TEST_CHECK_H
#define TEST_CHECK_H

/* The quiet pass/fail reporter the tests share. A test file reads passed and
   failed in main to print its summary and exit code. test_report.h holds the
   verbose one. */

#include <stdbool.h>
#include <stdio.h>

static int passed = 0;
static int failed = 0;

/* Quiet style: count every check and name only a failure, on stderr. */
static inline void check_bool(const char *test, bool cond) {
  if (cond) {
    passed++;
  } else {
    failed++;
    fprintf(stderr, "FAIL [%s]\n", test);
  }
}

#endif // TEST_CHECK_H
