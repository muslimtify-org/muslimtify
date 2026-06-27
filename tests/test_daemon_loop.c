#include "daemon_loop.h"

#include <stdbool.h>
#include <stdio.h>

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

static void test_seconds_until_next_minute(void) {
  report_result("minute boundary -> full minute", seconds_until_next_minute(0) == 60);
  report_result("one second before boundary -> 1", seconds_until_next_minute(59) == 1);
  report_result("mid-minute -> remaining", seconds_until_next_minute(30) == 30);
  report_result("next boundary -> full minute", seconds_until_next_minute(60) == 60);
  report_result("arbitrary epoch stays in [1,60]", seconds_until_next_minute(1719500000) >= 1 &&
                                                       seconds_until_next_minute(1719500000) <= 60);
}

int main(void) {
  printf("test_seconds_until_next_minute\n");
  test_seconds_until_next_minute();
  if (failures == 0) {
    printf("All %d tests passed.\n", total);
    return 0;
  }
  printf("%d/%d tests failed.\n", failures, total);
  return 1;
}
