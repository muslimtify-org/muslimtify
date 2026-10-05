#define _GNU_SOURCE
#include "log.h"
#include "muslimtify.h"
#include "platform.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int passed = 0;
static int failed = 0;

static char tmpdir[256];
static char home_dir[512];
static char fake_dir[512];
static char fake_log[512];
static char unit_file[1024];
static char timer_file[1024];

static void check_bool(const char *test, bool cond) {
  if (cond) {
    passed++;
  } else {
    failed++;
    fprintf(stderr, "FAIL [%s]\n", test);
  }
}

// Which fake systemctl calls answer with a failure, as space-separated keys. A
// key is the subcommand, or "<subcommand>:<unit>" for is-enabled and is-active.
static void fake_fails(const char *keys) {
  setenv("MT_FAKE_FAIL", keys, 1);
}

// The keys that make the fake report a clean machine: nothing enabled or active.
#define NOTHING_THERE \
  "is-enabled:muslimtify.timer is-enabled:muslimtify.service is-active:muslimtify.service"

static void clear_log(void) {
  unlink(fake_log);
}

static void read_file(const char *path, char *buffer, size_t cap) {
  buffer[0] = '\0';
  FILE *f = fopen(path, "r");
  if (!f)
    return;
  size_t n = fread(buffer, 1, cap - 1, f);
  buffer[n] = '\0';
  fclose(f);
}

static bool log_has(const char *line) {
  char log[8192];
  read_file(fake_log, log, sizeof(log));
  char needle[512];
  snprintf(needle, sizeof(needle), "%s\n", line);
  return strstr(log, needle) != NULL;
}

static bool file_exists(const char *path) {
  return access(path, F_OK) == 0;
}

// Point HOME into a temporary directory and put a fake systemctl first on PATH.
// The fake only logs its arguments and exits as MT_FAKE_FAIL tells it to. If
// systemctl does not then resolve to the fake, stop: nothing below may ever
// reach the real service manager.
static void setup(void) {
  snprintf(tmpdir, sizeof(tmpdir), "/tmp/mt_daemontest_XXXXXX");
  if (!mkdtemp(tmpdir)) {
    fprintf(stderr, "FATAL: mkdtemp failed\n");
    exit(2);
  }
  snprintf(home_dir, sizeof(home_dir), "%s/home", tmpdir);
  snprintf(fake_dir, sizeof(fake_dir), "%s/bin", tmpdir);
  snprintf(fake_log, sizeof(fake_log), "%s/systemctl.log", tmpdir);
  snprintf(unit_file, sizeof(unit_file), "%s/.config/systemd/user/muslimtify.service", home_dir);
  snprintf(timer_file, sizeof(timer_file), "%s/.config/systemd/user/muslimtify.timer", home_dir);
  mkdir(home_dir, 0755);
  mkdir(fake_dir, 0755);

  char script[1024];
  snprintf(script, sizeof(script), "%s/systemctl", fake_dir);
  FILE *f = fopen(script, "w");
  if (!f) {
    fprintf(stderr, "FATAL: cannot write the fake systemctl\n");
    exit(2);
  }
  fputs("#!/bin/sh\n"
        "echo \"$*\" >>\"$MT_FAKE_LOG\"\n"
        "key=\"$2\"\n"
        "case \"$2\" in\n"
        "is-enabled | is-active)\n"
        "  for last; do :; done\n"
        "  key=\"$2:$last\"\n"
        "  ;;\n"
        "esac\n"
        "case \" $MT_FAKE_FAIL \" in\n"
        "*\" $key \"*) exit 1 ;;\n"
        "esac\n"
        "exit 0\n",
        f);
  fclose(f);
  chmod(script, 0755);

  const char *old_path = getenv("PATH");
  char new_path[8192];
  snprintf(new_path, sizeof(new_path), "%s:%s", fake_dir, old_path ? old_path : "");
  setenv("PATH", new_path, 1);
  setenv("HOME", home_dir, 1);
  setenv("MT_FAKE_LOG", fake_log, 1);
  fake_fails("");

  char resolved[1024] = "";
  FILE *which = popen("command -v systemctl", "r");
  if (which) {
    if (fgets(resolved, sizeof(resolved), which))
      resolved[strcspn(resolved, "\n")] = '\0';
    pclose(which);
  }
  if (strcmp(resolved, script) != 0) {
    fprintf(stderr, "FATAL: systemctl resolves to '%s', not the fake. Refusing to run.\n",
            resolved);
    exit(2);
  }
}

static void teardown(void) {
  char cmd[1024];
  snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
  if (system(cmd) != 0) { /* best-effort cleanup */
  }
}

static void test_install(void) {
  printf("  install...\n");

  // A clean machine: no legacy timer.
  fake_fails("is-enabled:muslimtify.timer");
  clear_log();
  MuslimtifyDaemonInstall result;
  memset(&result, 0x5a, sizeof(result));
  check_bool("install succeeds",
             muslimtify_daemon_install("/opt/mt/muslimtify", &result) == MUSLIMTIFY_OK);
  check_bool("unit path reported", strcmp(result.unit_path, unit_file) == 0);
  check_bool("no legacy timer reported", !result.legacy_timer_disabled);
  check_bool("unit file written", file_exists(unit_file));

  char unit[4096];
  read_file(unit_file, unit, sizeof(unit));
  check_bool("unit runs the given binary",
             strstr(unit, "ExecStart=/opt/mt/muslimtify daemon run\n") != NULL);
  check_bool("systemd reloaded", log_has("--user daemon-reload"));
  check_bool("service enabled and started", log_has("--user enable --now muslimtify.service"));
  check_bool("no timer disabled", !log_has("--user disable --now muslimtify.timer"));

  // An older install left the timer enabled.
  fake_fails("");
  clear_log();
  check_bool("install over a legacy timer",
             muslimtify_daemon_install("/opt/mt/muslimtify", &result) == MUSLIMTIFY_OK);
  check_bool("legacy timer reported", result.legacy_timer_disabled);
  check_bool("legacy timer disabled", log_has("--user disable --now muslimtify.timer"));

  // The result is optional.
  fake_fails("is-enabled:muslimtify.timer");
  check_bool("NULL result is accepted",
             muslimtify_daemon_install("/opt/mt/muslimtify", NULL) == MUSLIMTIFY_OK);

  // NULL means the running program.
  check_bool("install with the running binary",
             muslimtify_daemon_install(NULL, &result) == MUSLIMTIFY_OK);
  char expected[2048];
  snprintf(expected, sizeof(expected), "ExecStart=%s daemon run\n", platform_exe_path());
  read_file(unit_file, unit, sizeof(unit));
  check_bool("unit runs this program", strstr(unit, expected) != NULL);
}

static void test_install_failures(void) {
  printf("  install failures...\n");

  MuslimtifyDaemonInstall result;

  fake_fails("is-enabled:muslimtify.timer daemon-reload");
  clear_log();
  check_bool("reload failure is reported",
             muslimtify_daemon_install("/opt/mt/muslimtify", &result) ==
                 MUSLIMTIFY_ERR_DAEMON_RELOAD);
  check_bool("the unit was still written", strcmp(result.unit_path, unit_file) == 0);
  check_bool("enable was not attempted", !log_has("--user enable --now muslimtify.service"));

  fake_fails("is-enabled:muslimtify.timer enable");
  clear_log();
  check_bool("enable failure is reported",
             muslimtify_daemon_install("/opt/mt/muslimtify", &result) ==
                 MUSLIMTIFY_ERR_DAEMON_ENABLE);
  check_bool("reload ran before it", log_has("--user daemon-reload"));

  fake_fails("is-enabled:muslimtify.timer");
  unlink(unit_file);
  clear_log();
  memset(&result, 0x5a, sizeof(result));
  check_bool("an empty binary is refused",
             muslimtify_daemon_install("", &result) == MUSLIMTIFY_ERR_DAEMON_BINARY);
  check_bool("nothing was written", !file_exists(unit_file) && result.unit_path[0] == '\0');
  check_bool("systemctl was not called", !file_exists(fake_log));

  // A home that is a regular file: the unit directory cannot be created. The
  // reason is logged, which would clutter the test output, so discard it here.
  char not_a_dir[512];
  snprintf(not_a_dir, sizeof(not_a_dir), "%s/homefile", tmpdir);
  FILE *f = fopen(not_a_dir, "w");
  check_bool("stand-in home created", f != NULL);
  if (f)
    fclose(f);
  setenv("HOME", not_a_dir, 1);
  muslimtify_set_log_handler(NULL, NULL);
  check_bool("an unwritable home is reported",
             muslimtify_daemon_install("/opt/mt/muslimtify", &result) ==
                 MUSLIMTIFY_ERR_DAEMON_UNIT);
  check_bool("no unit path on that failure", result.unit_path[0] == '\0');
  mt_log_set_handler(mt_log_stderr, NULL);
  setenv("HOME", home_dir, 1);
}

static void test_uninstall(void) {
  printf("  uninstall...\n");

  MuslimtifyDaemonUninstall result;

  // Installed, enabled and running, no legacy timer.
  fake_fails("is-enabled:muslimtify.timer");
  check_bool("install first",
             muslimtify_daemon_install("/opt/mt/muslimtify", NULL) == MUSLIMTIFY_OK);
  clear_log();
  memset(&result, 0x5a, sizeof(result));
  check_bool("uninstall succeeds", muslimtify_daemon_uninstall(&result) == MUSLIMTIFY_OK);
  check_bool("stopped", result.stopped && log_has("--user stop muslimtify.service"));
  check_bool("disabled", result.disabled && log_has("--user disable muslimtify.service"));
  check_bool("no legacy timer", !result.legacy_timer_disabled);
  check_bool("unit removed", result.unit_removed && !file_exists(unit_file));
  check_bool("no timer file", !result.timer_removed);
  check_bool("paths reported", strcmp(result.unit_path, unit_file) == 0 &&
                                   strcmp(result.timer_path, timer_file) == 0);
  check_bool("systemd reloaded", log_has("--user daemon-reload"));

  // Nothing installed.
  fake_fails(NOTHING_THERE);
  clear_log();
  check_bool("uninstall with nothing there", muslimtify_daemon_uninstall(&result) == MUSLIMTIFY_OK);
  check_bool("nothing was done", !result.stopped && !result.disabled &&
                                     !result.legacy_timer_disabled && !result.unit_removed &&
                                     !result.timer_removed);
  check_bool("no stop or disable call", !log_has("--user stop muslimtify.service") &&
                                            !log_has("--user disable muslimtify.service"));

  // A leftover timer file from an older version.
  mkdir(home_dir, 0755);
  FILE *f = fopen(timer_file, "w");
  check_bool("leftover timer created", f != NULL);
  if (f)
    fclose(f);
  check_bool("uninstall with a leftover timer",
             muslimtify_daemon_uninstall(&result) == MUSLIMTIFY_OK);
  check_bool("timer file removed", result.timer_removed && !file_exists(timer_file));

  check_bool("NULL result is accepted", muslimtify_daemon_uninstall(NULL) == MUSLIMTIFY_OK);
}

static void test_status(void) {
  printf("  status...\n");

  MuslimtifyDaemonStatus st;
  check_bool("NULL is refused", muslimtify_daemon_status(NULL) == MUSLIMTIFY_ERR_INVALID_ARG);

  // Nothing installed, nothing enabled, nothing running.
  unlink(unit_file);
  fake_fails(NOTHING_THERE);
  check_bool("status succeeds", muslimtify_daemon_status(&st) == MUSLIMTIFY_OK);
  check_bool("all false", !st.installed && !st.enabled && !st.running);

  // Each field follows its own source.
  fake_fails("is-enabled:muslimtify.timer is-active:muslimtify.service");
  check_bool("status again", muslimtify_daemon_status(&st) == MUSLIMTIFY_OK);
  check_bool("enabled alone", !st.installed && st.enabled && !st.running);

  fake_fails("is-enabled:muslimtify.timer is-enabled:muslimtify.service");
  check_bool("status again", muslimtify_daemon_status(&st) == MUSLIMTIFY_OK);
  check_bool("running alone", !st.installed && !st.enabled && st.running);

  fake_fails("is-enabled:muslimtify.timer");
  check_bool("install", muslimtify_daemon_install("/opt/mt/muslimtify", NULL) == MUSLIMTIFY_OK);
  fake_fails(NOTHING_THERE);
  check_bool("status again", muslimtify_daemon_status(&st) == MUSLIMTIFY_OK);
  check_bool("installed alone", st.installed && !st.enabled && !st.running);

  fake_fails("is-enabled:muslimtify.timer");
  check_bool("status again", muslimtify_daemon_status(&st) == MUSLIMTIFY_OK);
  check_bool("all true", st.installed && st.enabled && st.running);
}

static void test_messages(void) {
  printf("  error messages...\n");

  const MuslimtifyError codes[] = {
      MUSLIMTIFY_ERR_UNSUPPORTED, MUSLIMTIFY_ERR_DAEMON_BINARY, MUSLIMTIFY_ERR_NO_HOME,
      MUSLIMTIFY_ERR_DAEMON_UNIT, MUSLIMTIFY_ERR_DAEMON_RELOAD, MUSLIMTIFY_ERR_DAEMON_ENABLE,
  };
  for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); i++) {
    const char *message = muslimtify_get_error(codes[i]);
    check_bool("message exists", message != NULL && message[0] != '\0');
    check_bool("message is specific", strcmp(message, "Unknown error") != 0);
  }
}

int main(void) {
  printf("Running muslimtify daemon tests...\n");
  setup();
  test_install();
  test_install_failures();
  test_uninstall();
  test_status();
  test_messages();
  teardown();

  printf("\nResults: %d passed, %d failed\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
