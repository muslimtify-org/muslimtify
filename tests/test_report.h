#ifndef TEST_REPORT_H
#define TEST_REPORT_H

/* The verbose pass/fail reporter the tests share. A test file reads total and
   failures in main to print its summary and exit code. test_check.h holds the
   quiet one. */

#include <stdbool.h>
#include <stdio.h>

static int total = 0;
static int failures = 0;

/* Verbose style: print PASS or FAIL for every check, on stdout. */
static inline void report_result(const char *label, bool pass) {
  total++;
  if (pass) {
    printf("  PASS: %s\n", label);
  } else {
    printf("  FAIL: %s\n", label);
    failures++;
  }
}

#endif // TEST_REPORT_H
