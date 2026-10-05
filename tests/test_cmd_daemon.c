#define _POSIX_C_SOURCE 200809L

#include "platform/linux/daemon_linux.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

static void test_unit_in_dirs(void) {
  char first[] = "/tmp/mt_unitdirs_XXXXXX";
  char second[] = "/tmp/mt_unitdirs_XXXXXX";
  if (!mkdtemp(first) || !mkdtemp(second)) {
    report_result("temporary directories created", false);
    return;
  }
  const char *dirs[] = {first, second};

  report_result("no unit in either directory", !daemon_unit_in_dirs(dirs, 2));

  char unit[600];
  snprintf(unit, sizeof(unit), "%s/muslimtify.service", second);
  FILE *f = fopen(unit, "w");
  if (f)
    fclose(f);
  report_result("unit in the second directory is found", f != NULL && daemon_unit_in_dirs(dirs, 2));
  report_result("a count of 0 looks nowhere", !daemon_unit_in_dirs(dirs, 0));

  unlink(unit);
  rmdir(first);
  rmdir(second);
}

static void test_find_binary(void) {
  char dir[] = "/tmp/mt_findbin_XXXXXX";
  if (!mkdtemp(dir)) {
    report_result("temporary directory created", false);
    return;
  }
  char missing[600], plain[600], exec[600];
  snprintf(missing, sizeof(missing), "%s/missing", dir);
  snprintf(plain, sizeof(plain), "%s/plain", dir);
  snprintf(exec, sizeof(exec), "%s/exec", dir);
  const char *files[2] = {plain, exec};
  const mode_t modes[2] = {0644, 0755};
  for (int i = 0; i < 2; i++) {
    FILE *f = fopen(files[i], "w");
    if (f)
      fclose(f);
    chmod(files[i], modes[i]);
  }

  const char *all[] = {missing, plain, exec};
  report_result("the executable file is returned", daemon_find_binary(all, 3) == exec);
  report_result("missing and non-executable give NULL", daemon_find_binary(all, 2) == NULL);
  const char *exec_first[] = {exec, plain};
  report_result("the first qualifying path wins", daemon_find_binary(exec_first, 2) == exec);
  const char *with_dir[] = {dir, exec};
  report_result("a directory is skipped", daemon_find_binary(with_dir, 2) == exec);
  const char *with_null[] = {NULL, exec};
  report_result("a NULL entry is skipped", daemon_find_binary(with_null, 2) == exec);
  report_result("a count of 0 gives NULL", daemon_find_binary(all, 0) == NULL);

  unlink(plain);
  unlink(exec);
  rmdir(dir);
}

int main(void) {
  printf("test_build_service_unit\n");
  test_build_service_unit();
  printf("test_unit_in_dirs\n");
  test_unit_in_dirs();
  printf("test_find_binary\n");
  test_find_binary();

  if (failures == 0) {
    printf("All %d tests passed.\n", total);
    return 0;
  }
  printf("%d/%d tests failed.\n", failures, total);
  return 1;
}
