#include "cli_internal.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

static int passed = 0;
static int failed = 0;

static void check(int cond, const char *message) {
  if (cond) {
    passed++;
  } else {
    failed++;
    fprintf(stderr, "FAIL: %s\n", message);
  }
}

int main(void) {
  char path[256];
  int ret = cmd_run_resolve_gui_path_for_test("/usr/local/bin", path, sizeof(path));
  check(ret == 0, "expected gui path resolution to succeed");
  check(strcmp(path, "/usr/local/bin/muslimtify-gui") == 0,
        "expected exact gui path /usr/local/bin/muslimtify-gui");

  char small[8];
  ret = cmd_run_resolve_gui_path_for_test("/usr/local/bin", small, sizeof(small));
  check(ret != 0, "expected small buffer resolution to fail");

  printf("Results: %d passed, %d failed\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
