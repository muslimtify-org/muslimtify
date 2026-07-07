#ifdef _WIN32

#include "cmd_daemon_win.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* Link-only stubs. cmd_daemon_win.c references the toast-activator functions
   from its install/register handlers, but these tests only exercise
   build_windows_task_action. The real implementations live in
   toast_activator_win.c, intentionally not linked into this unit test. */
int register_toast_activator(void) {
  return 0;
}
int unregister_toast_activator(void) {
  return 0;
}

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

static void test_service_task_action_builder(void) {
  char task_action[DAEMON_TASK_ACTION_MAX];
  int written =
      build_windows_task_action("C:\\Program Files\\Muslimtify", task_action, sizeof(task_action));

  report_result("builder returns success", written > 0);
  report_result("task action schedules helper directly",
                strcmp(task_action, "\"C:\\Program Files\\Muslimtify\\muslimtify-service.exe\"") ==
                    0);
  report_result("task action does not use powershell",
                strstr(task_action, "powershell.exe") == NULL);
  report_result("task action does not append check", strstr(task_action, " check") == NULL);
  report_result("task action preserves quoted executable path",
                task_action[0] == '"' && strstr(task_action, "\\muslimtify-service.exe\"") != NULL);
}

int main(void) {
  printf("test_service_task_action_builder\n");
  test_service_task_action_builder();

  if (failures == 0) {
    printf("All %d tests passed.\n", total);
    return 0;
  }

  printf("%d/%d tests failed.\n", failures, total);
  return 1;
}

#endif
