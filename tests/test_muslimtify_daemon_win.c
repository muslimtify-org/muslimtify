#include "lib/muslimtify_internal.h"
#include "log.h"
#include "muslimtify.h"
#include "platform/windows/daemon_win.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static int passed = 0;
static int failed = 0;

static char fake_schtasks[MAX_PATH];
static char tmpdir[MAX_PATH];
static char fake_log[MAX_PATH];
static char service_bin[MAX_PATH];
static char missing_bin[MAX_PATH];

static int register_calls = 0;
static int unregister_calls = 0;
static bool register_fails = false;

// The real ones write to the registry. These only count, so no test touches it.
int register_toast_activator(void) {
  register_calls++;
  return register_fails ? -1 : 0;
}

int unregister_toast_activator(void) {
  unregister_calls++;
  return 0;
}

static void check_bool(const char *test, bool cond) {
  if (cond) {
    passed++;
  } else {
    failed++;
    fprintf(stderr, "FAIL [%s]\n", test);
  }
}

// Which fake schtasks subcommands answer with a failure, space-separated.
static void fake_fails(const char *keys) {
  _putenv_s("MT_FAKE_FAIL", keys);
}

static void fake_disabled(bool disabled) {
  _putenv_s("MT_FAKE_DISABLED", disabled ? "1" : "0");
}

static void reset(void) {
  DeleteFileA(fake_log);
  fake_fails("");
  fake_disabled(false);
  register_calls = 0;
  unregister_calls = 0;
  register_fails = false;
}

static void read_log(char *buffer, size_t cap) {
  buffer[0] = '\0';
  FILE *f = fopen(fake_log, "r");
  if (!f)
    return;
  size_t n = fread(buffer, 1, cap - 1, f);
  buffer[n] = '\0';
  fclose(f);
}

static bool log_has(const char *line) {
  char log[8192];
  read_log(log, sizeof(log));
  char needle[1024];
  snprintf(needle, sizeof(needle), "%s\n", line);
  return strstr(log, needle) != NULL;
}

static bool log_mentions(const char *text) {
  char log[8192];
  read_log(log, sizeof(log));
  return strstr(log, text) != NULL;
}

static bool log_is_empty(void) {
  char log[8192];
  read_log(log, sizeof(log));
  return log[0] == '\0';
}

static bool file_exists(const char *path) {
  DWORD attrs = GetFileAttributesA(path);
  return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

// Point the library at the fake schtasks before anything else. If the fake is
// not there, stop: nothing below may ever reach the real Task Scheduler.
static void setup(const char *fake_path) {
  snprintf(fake_schtasks, sizeof(fake_schtasks), "%s", fake_path);
  for (char *p = fake_schtasks; *p; p++) {
    if (*p == '/')
      *p = '\\';
  }
  if (!file_exists(fake_schtasks)) {
    fprintf(stderr, "FATAL: the fake schtasks '%s' does not exist. Refusing to run.\n",
            fake_schtasks);
    exit(2);
  }
  daemon_win_set_schtasks_path(fake_schtasks);

  char temp_base[MAX_PATH];
  DWORD len = GetTempPathA(MAX_PATH, temp_base);
  if (len == 0 || len >= MAX_PATH) {
    fprintf(stderr, "FATAL: GetTempPathA failed\n");
    exit(2);
  }
  snprintf(tmpdir, sizeof(tmpdir), "%smt_daemontest_%lu", temp_base, GetCurrentProcessId());
  if (!CreateDirectoryA(tmpdir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
    fprintf(stderr, "FATAL: cannot create %s\n", tmpdir);
    exit(2);
  }
  snprintf(fake_log, sizeof(fake_log), "%s\\schtasks.log", tmpdir);
  snprintf(service_bin, sizeof(service_bin), "%s\\muslimtify-service.exe", tmpdir);
  snprintf(missing_bin, sizeof(missing_bin), "%s\\not-there.exe", tmpdir);

  FILE *f = fopen(service_bin, "w");
  if (!f) {
    fprintf(stderr, "FATAL: cannot write the stand-in service program\n");
    exit(2);
  }
  fputs("stand-in\n", f);
  fclose(f);

  _putenv_s("MT_FAKE_LOG", fake_log);
  mt_log_set_handler(NULL, NULL);
  reset();
}

static void teardown(void) {
  DeleteFileA(fake_log);
  DeleteFileA(service_bin);
  RemoveDirectoryA(tmpdir);
}

static void test_task_action(void) {
  printf("  task action...\n");

  char action[DAEMON_TASK_ACTION_MAX];
  int written = daemon_win_task_action("C:\\Program Files\\Muslimtify\\muslimtify-service.exe",
                                       action, sizeof(action));
  check_bool("builder returns success", written > 0);
  check_bool("task action is the quoted program",
             strcmp(action, "\"C:\\Program Files\\Muslimtify\\muslimtify-service.exe\"") == 0);
  check_bool("task action does not use powershell", strstr(action, "powershell.exe") == NULL);
  check_bool("task action does not append check", strstr(action, " check") == NULL);

  char tiny_buf[8];
  check_bool("too small a buffer is refused",
             daemon_win_task_action("C:\\muslimtify-service.exe", tiny_buf, sizeof(tiny_buf)) < 0);
  check_bool("NULL path is refused", daemon_win_task_action(NULL, action, sizeof(action)) < 0);
  check_bool("empty path is refused", daemon_win_task_action("", action, sizeof(action)) < 0);
}

static void test_xml_enabled(void) {
  printf("  task xml...\n");

  const char *enabled = "<Task><Settings><MultipleInstancesPolicy>IgnoreNew"
                        "</MultipleInstancesPolicy></Settings><Triggers></Triggers></Task>";
  const char *disabled = "<Task><Settings><Enabled>false</Enabled></Settings>"
                         "<Triggers></Triggers></Task>";
  const char *trigger_off = "<Task><Settings><Enabled>true</Enabled></Settings><Triggers>"
                            "<TimeTrigger><Enabled>false</Enabled></TimeTrigger></Triggers></Task>";
  const char *no_settings = "<Task><Triggers></Triggers></Task>";

  check_bool("settings without the element are enabled",
             daemon_win_task_xml_enabled(enabled, strlen(enabled)));
  check_bool("settings switched off are disabled",
             !daemon_win_task_xml_enabled(disabled, strlen(disabled)));
  check_bool("a disabled trigger does not disable the task",
             daemon_win_task_xml_enabled(trigger_off, strlen(trigger_off)));
  check_bool("no settings block is enabled",
             daemon_win_task_xml_enabled(no_settings, strlen(no_settings)));
  check_bool("NULL is enabled", daemon_win_task_xml_enabled(NULL, 0));

  char wide[512];
  size_t n = strlen(disabled);
  for (size_t i = 0; i < n; i++) {
    wide[2 * i] = disabled[i];
    wide[2 * i + 1] = '\0';
  }
  check_bool("UTF-16 input is read", !daemon_win_task_xml_enabled(wide, 2 * n));
}

static void test_install(void) {
  printf("  install...\n");

  reset();
  MuslimtifyDaemonInstall result;
  check_bool("install succeeds",
             muslimtify_daemon_install_binary(service_bin, &result) == MUSLIMTIFY_OK);
  char expected[1024];
  snprintf(expected, sizeof(expected), "/create /tn muslimtify /tr %s /sc minute /mo 1 /it /f",
           service_bin);
  check_bool("the task is created with the expected arguments", log_has(expected));
  check_bool("the chosen program is reported", strcmp(result.binary_path, service_bin) == 0);
  check_bool("no service file is reported", result.unit_path[0] == '\0');
  check_bool("no legacy timer is reported", !result.legacy_timer_disabled);
  check_bool("the activator is registered once", register_calls == 1);

  reset();
  check_bool("NULL result is accepted",
             muslimtify_daemon_install_binary(service_bin, NULL) == MUSLIMTIFY_OK);

  reset();
  check_bool("a missing program is refused",
             muslimtify_daemon_install_binary(missing_bin, &result) ==
                 MUSLIMTIFY_ERR_DAEMON_BINARY);
  check_bool("nothing was run for a missing program", log_is_empty());
  check_bool("no activator for a missing program", register_calls == 0);

  reset();
  fake_fails("/create");
  check_bool("a failing create is reported",
             muslimtify_daemon_install_binary(service_bin, &result) ==
                 MUSLIMTIFY_ERR_DAEMON_MANAGER);
  check_bool("the program is still reported", strcmp(result.binary_path, service_bin) == 0);
  check_bool("no activator after a failing create", register_calls == 0);

  reset();
  register_fails = true;
  check_bool("a failing activator does not fail the install",
             muslimtify_daemon_install_binary(service_bin, &result) == MUSLIMTIFY_OK);
}

static void test_uninstall(void) {
  printf("  uninstall...\n");

  reset();
  fake_fails("/query");
  MuslimtifyDaemonUninstall result;
  check_bool("uninstall with nothing there", muslimtify_daemon_uninstall(&result) == MUSLIMTIFY_OK);
  check_bool("nothing was removed", !result.unit_removed);
  check_bool("delete was not called", !log_mentions("/delete"));
  check_bool("the activator is unregistered anyway", unregister_calls == 1);

  reset();
  check_bool("uninstall succeeds", muslimtify_daemon_uninstall(&result) == MUSLIMTIFY_OK);
  check_bool("the task is deleted", log_has("/delete /tn muslimtify /f"));
  check_bool("the removal is reported", result.unit_removed);
  check_bool("the activator is unregistered", unregister_calls == 1);
  check_bool("the Linux-only fields stay clear",
             !result.stopped && !result.disabled && !result.legacy_timer_disabled &&
                 !result.timer_removed && result.unit_path[0] == '\0' &&
                 result.timer_path[0] == '\0');

  reset();
  check_bool("NULL result is accepted", muslimtify_daemon_uninstall(NULL) == MUSLIMTIFY_OK);

  reset();
  fake_fails("/delete");
  check_bool("a failing delete is reported",
             muslimtify_daemon_uninstall(&result) == MUSLIMTIFY_ERR_DAEMON_MANAGER);
  check_bool("no removal is reported", !result.unit_removed);
}

static void test_status(void) {
  printf("  status...\n");

  reset();
  MuslimtifyDaemonStatus st;
  check_bool("NULL is refused", muslimtify_daemon_status(NULL) == MUSLIMTIFY_ERR_INVALID_ARG);

  fake_fails("/query");
  check_bool("status succeeds", muslimtify_daemon_status(&st) == MUSLIMTIFY_OK);
  check_bool("nothing there", !st.installed && !st.enabled && !st.running);

  reset();
  check_bool("status again", muslimtify_daemon_status(&st) == MUSLIMTIFY_OK);
  check_bool("an enabled task", st.installed && st.enabled && st.running);
  check_bool("the task is asked for as xml", log_has("/query /tn muslimtify /xml"));

  reset();
  fake_disabled(true);
  check_bool("status again", muslimtify_daemon_status(&st) == MUSLIMTIFY_OK);
  check_bool("a disabled task", st.installed && !st.enabled && !st.running);
}

// Last, because it points the library away from the fake. The path it uses does
// not exist, so nothing can be run through it.
static void test_unreachable(void) {
  printf("  unreachable service manager...\n");

  reset();
  daemon_win_set_schtasks_path(missing_bin);

  MuslimtifyDaemonStatus st;
  check_bool("status reports it", muslimtify_daemon_status(&st) == MUSLIMTIFY_ERR_DAEMON_MANAGER);
  check_bool("status claims nothing", !st.installed && !st.enabled && !st.running);

  MuslimtifyDaemonUninstall uninstall;
  check_bool("uninstall reports it",
             muslimtify_daemon_uninstall(&uninstall) == MUSLIMTIFY_ERR_DAEMON_MANAGER);

  MuslimtifyDaemonInstall install;
  check_bool("install reports it", muslimtify_daemon_install_binary(service_bin, &install) ==
                                       MUSLIMTIFY_ERR_DAEMON_MANAGER);

  daemon_win_set_schtasks_path(fake_schtasks);
}

int main(int argc, char **argv) {
  printf("Running muslimtify daemon tests (Windows)...\n");
  if (argc < 2) {
    fprintf(stderr, "FATAL: pass the path of the fake schtasks program\n");
    return 2;
  }
  setup(argv[1]);
  test_task_action();
  test_xml_enabled();
  test_install();
  test_uninstall();
  test_status();
  test_unreachable();
  teardown();

  printf("\nResults: %d passed, %d failed\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
