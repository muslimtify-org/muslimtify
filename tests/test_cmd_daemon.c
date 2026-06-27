#include "cmd_daemon.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

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

static void test_build_service_unit(void) {
  char unit[DAEMON_UNIT_MAX];
  int written = build_service_unit("/usr/local/bin/muslimtify", unit, sizeof(unit));

  report_result("builder returns success", written > 0);
  report_result("unit is a simple (long-running) service", strstr(unit, "Type=simple") != NULL);
  report_result("unit runs the loop",
                strstr(unit, "ExecStart=/usr/local/bin/muslimtify daemon run") != NULL);
  report_result("unit installs into the user target",
                strstr(unit, "WantedBy=default.target") != NULL);
  report_result("unit is not a timer", strstr(unit, "OnCalendar") == NULL);
  report_result("unit is not oneshot", strstr(unit, "Type=oneshot") == NULL);
}

int main(void) {
  printf("test_build_service_unit\n");
  test_build_service_unit();

  if (failures == 0) {
    printf("All %d tests passed.\n", total);
    return 0;
  }
  printf("%d/%d tests failed.\n", failures, total);
  return 1;
}
