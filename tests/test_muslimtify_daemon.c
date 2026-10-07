#define _GNU_SOURCE
#include "lib/muslimtify_internal.h"
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
static char daemon_bin[1024];
static char not_executable[1024];

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
        "*\" $key \"*)\n"
        "  case \"$2\" in\n"
        "  is-active) exit 3 ;;\n"
        "  is-enabled) exit 4 ;;\n"
        "  *) exit 1 ;;\n"
        "  esac\n"
        "  ;;\n"
        "esac\n"
        "exit 0\n",
        f);
  fclose(f);
  chmod(script, 0755);

  snprintf(daemon_bin, sizeof(daemon_bin), "%s/muslimtify", fake_dir);
  snprintf(not_executable, sizeof(not_executable), "%s/not-executable", fake_dir);
  const char *stand_in[2] = {daemon_bin, not_executable};
  const mode_t stand_in_mode[2] = {0755, 0644};
  for (int i = 0; i < 2; i++) {
    FILE *g = fopen(stand_in[i], "w");
    if (!g) {
      fprintf(stderr, "FATAL: cannot write a stand-in program\n");
      exit(2);
    }
    fputs("#!/bin/sh\nexit 0\n", g);
    fclose(g);
    chmod(stand_in[i], stand_in_mode[i]);
  }

  const char *old_path = getenv("PATH");
  char new_path[8192];
  snprintf(new_path, sizeof(new_path), "%s:%s", fake_dir, old_path ? old_path : "");
  setenv("PATH", new_path, 1);
  setenv("HOME", home_dir, 1);
  // The unit directory follows XDG_CONFIG_HOME when it is set, so the
  // developer's own value must not leak in here.
  unsetenv("XDG_CONFIG_HOME");
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
             muslimtify_daemon_install_binary(daemon_bin, &result) == MUSLIMTIFY_OK);
  check_bool("unit path reported", strcmp(result.unit_path, unit_file) == 0);
  check_bool("chosen binary reported", strcmp(result.binary_path, daemon_bin) == 0);
  check_bool("no legacy timer reported", !result.legacy_timer_disabled);
  check_bool("unit file written", file_exists(unit_file));

  char unit[4096];
  read_file(unit_file, unit, sizeof(unit));
  char expected_exec[2048];
  snprintf(expected_exec, sizeof(expected_exec), "ExecStart=\"%s\" daemon run\n", daemon_bin);
  check_bool("unit runs the given binary", strstr(unit, expected_exec) != NULL);
  check_bool("systemd reloaded", log_has("--user daemon-reload"));
  check_bool("service enabled and started", log_has("--user enable --now muslimtify.service"));
  check_bool("no timer disabled", !log_has("--user disable --now muslimtify.timer"));

  // An older install left the timer enabled.
  fake_fails("");
  clear_log();
  check_bool("install over a legacy timer",
             muslimtify_daemon_install_binary(daemon_bin, &result) == MUSLIMTIFY_OK);
  check_bool("legacy timer reported", result.legacy_timer_disabled);
  check_bool("legacy timer disabled", log_has("--user disable --now muslimtify.timer"));

  // The result is optional.
  fake_fails("is-enabled:muslimtify.timer");
  check_bool("NULL result is accepted",
             muslimtify_daemon_install_binary(daemon_bin, NULL) == MUSLIMTIFY_OK);

  // The public function chooses the program itself, from a fixed list of
  // places. What it finds depends on this machine, so both outcomes are
  // checked, and one thing holds either way: it never picks this test program.
  MuslimtifyError search_err = muslimtify_daemon_install(&result);
  if (search_err == MUSLIMTIFY_OK) {
    const char *slash = strrchr(result.binary_path, '/');
    check_bool("a program was chosen", result.binary_path[0] == '/');
    check_bool("it is named muslimtify", slash != NULL && strcmp(slash + 1, "muslimtify") == 0);
    check_bool("it is executable", access(result.binary_path, X_OK) == 0);
    char expected[4096];
    snprintf(expected, sizeof(expected), "ExecStart=\"%s\" daemon run\n", result.binary_path);
    read_file(unit_file, unit, sizeof(unit));
    check_bool("the unit runs the chosen program", strstr(unit, expected) != NULL);
  } else {
    check_bool("no program found is reported", search_err == MUSLIMTIFY_ERR_DAEMON_BINARY);
    check_bool("and none is named", result.binary_path[0] == '\0');
  }
  check_bool("it never chooses this test program",
             strcmp(result.binary_path, platform_exe_path()) != 0);
}

static void test_install_failures(void) {
  printf("  install failures...\n");

  MuslimtifyDaemonInstall result;

  fake_fails("is-enabled:muslimtify.timer daemon-reload");
  clear_log();
  check_bool("reload failure is reported",
             muslimtify_daemon_install_binary(daemon_bin, &result) == MUSLIMTIFY_ERR_DAEMON_RELOAD);
  check_bool("the unit was still written", strcmp(result.unit_path, unit_file) == 0);
  check_bool("enable was not attempted", !log_has("--user enable --now muslimtify.service"));

  fake_fails("is-enabled:muslimtify.timer enable");
  clear_log();
  check_bool("enable failure is reported",
             muslimtify_daemon_install_binary(daemon_bin, &result) == MUSLIMTIFY_ERR_DAEMON_ENABLE);
  check_bool("reload ran before it", log_has("--user daemon-reload"));

  fake_fails("is-enabled:muslimtify.timer");
  muslimtify_set_log_handler(NULL, NULL);
  const char *bad_binaries[] = {"", "/no/such/dir/muslimtify", not_executable, fake_dir};
  const char *bad_labels[] = {"an empty binary", "a missing binary", "a non-executable file",
                              "a directory"};
  for (size_t i = 0; i < sizeof(bad_binaries) / sizeof(bad_binaries[0]); i++) {
    unlink(unit_file);
    clear_log();
    memset(&result, 0x5a, sizeof(result));
    char label[128];
    snprintf(label, sizeof(label), "%s is refused", bad_labels[i]);
    check_bool(label, muslimtify_daemon_install_binary(bad_binaries[i], &result) ==
                          MUSLIMTIFY_ERR_DAEMON_BINARY);
    check_bool("nothing was written", !file_exists(unit_file) && result.unit_path[0] == '\0');
    check_bool("no binary is named", result.binary_path[0] == '\0');
    check_bool("systemctl was not called", !file_exists(fake_log));
  }
  muslimtify_set_log_handler(muslimtify_log_stderr, NULL);

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
             muslimtify_daemon_install_binary(daemon_bin, &result) == MUSLIMTIFY_ERR_DAEMON_UNIT);
  check_bool("no unit path on that failure", result.unit_path[0] == '\0');
  muslimtify_set_log_handler(muslimtify_log_stderr, NULL);
  setenv("HOME", home_dir, 1);
}

// With XDG_CONFIG_HOME set, systemd reads user units from under it, so that
// is where the unit goes, and where status and uninstall look for it.
static void test_xdg_config_home(void) {
  printf("  XDG_CONFIG_HOME...\n");

  char xdg[512];
  char xdg_unit[1024];
  snprintf(xdg, sizeof(xdg), "%s/xdg", tmpdir);
  snprintf(xdg_unit, sizeof(xdg_unit), "%s/systemd/user/muslimtify.service", xdg);
  setenv("XDG_CONFIG_HOME", xdg, 1);
  fake_fails("is-enabled:muslimtify.timer");

  MuslimtifyDaemonInstall install;
  check_bool("install succeeds",
             muslimtify_daemon_install_binary(daemon_bin, &install) == MUSLIMTIFY_OK);
  check_bool("unit is under XDG_CONFIG_HOME",
             strcmp(install.unit_path, xdg_unit) == 0 && file_exists(xdg_unit));
  check_bool("nothing under ~/.config", !file_exists(unit_file));

  MuslimtifyDaemonStatus status;
  fake_fails(NOTHING_THERE);
  check_bool("status reads it",
             muslimtify_daemon_status(&status) == MUSLIMTIFY_OK && status.installed);

  MuslimtifyDaemonUninstall uninstall;
  check_bool("uninstall removes it", muslimtify_daemon_uninstall(&uninstall) == MUSLIMTIFY_OK &&
                                         uninstall.unit_removed && !file_exists(xdg_unit));

  // An empty value means unset, the same as for the config directory.
  setenv("XDG_CONFIG_HOME", "", 1);
  fake_fails("is-enabled:muslimtify.timer");
  check_bool("empty XDG_CONFIG_HOME falls back to home",
             muslimtify_daemon_install_binary(daemon_bin, &install) == MUSLIMTIFY_OK &&
                 strcmp(install.unit_path, unit_file) == 0);
  unsetenv("XDG_CONFIG_HOME");
}

static void test_uninstall(void) {
  printf("  uninstall...\n");

  MuslimtifyDaemonUninstall result;

  // Installed, enabled and running, no legacy timer.
  fake_fails("is-enabled:muslimtify.timer");
  check_bool("install first", muslimtify_daemon_install_binary(daemon_bin, NULL) == MUSLIMTIFY_OK);
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

// True when this machine has a packaged unit in a system directory. The test
// cannot change that, so it has to expect it.
static bool host_has_system_unit(void) {
  return file_exists("/etc/systemd/user/muslimtify.service") ||
         file_exists("/usr/local/lib/systemd/user/muslimtify.service") ||
         file_exists("/usr/lib/systemd/user/muslimtify.service");
}

static bool expected_installed(bool enabled, bool running) {
  return file_exists(unit_file) || host_has_system_unit() || enabled || running;
}

static void test_status(void) {
  printf("  status...\n");

  MuslimtifyDaemonStatus st;
  check_bool("NULL is refused", muslimtify_daemon_status(NULL) == MUSLIMTIFY_ERR_INVALID_ARG);

  // Nothing there.
  unlink(unit_file);
  fake_fails(NOTHING_THERE);
  check_bool("status succeeds", muslimtify_daemon_status(&st) == MUSLIMTIFY_OK);
  check_bool("nothing there",
             st.installed == expected_installed(false, false) && !st.enabled && !st.running);

  // Each field follows its own source, and enabled or running implies installed.
  fake_fails("is-enabled:muslimtify.timer is-active:muslimtify.service");
  check_bool("status again", muslimtify_daemon_status(&st) == MUSLIMTIFY_OK);
  check_bool("enabled alone",
             st.installed == expected_installed(true, false) && st.enabled && !st.running);
  check_bool("enabled implies installed", st.installed);

  fake_fails("is-enabled:muslimtify.timer is-enabled:muslimtify.service");
  check_bool("status again", muslimtify_daemon_status(&st) == MUSLIMTIFY_OK);
  check_bool("running alone",
             st.installed == expected_installed(false, true) && !st.enabled && st.running);
  check_bool("running implies installed", st.installed);

  fake_fails("is-enabled:muslimtify.timer");
  check_bool("install", muslimtify_daemon_install_binary(daemon_bin, NULL) == MUSLIMTIFY_OK);
  fake_fails(NOTHING_THERE);
  check_bool("status again", muslimtify_daemon_status(&st) == MUSLIMTIFY_OK);
  check_bool("user file alone", st.installed == expected_installed(false, false) && st.installed &&
                                    !st.enabled && !st.running);

  fake_fails("is-enabled:muslimtify.timer");
  check_bool("status again", muslimtify_daemon_status(&st) == MUSLIMTIFY_OK);
  check_bool("all true", st.installed == expected_installed(true, true) && st.installed &&
                             st.enabled && st.running);
}

static void test_messages(void) {
  printf("  error messages...\n");

  const MuslimtifyError codes[] = {
      MUSLIMTIFY_ERR_UNSUPPORTED, MUSLIMTIFY_ERR_DAEMON_BINARY, MUSLIMTIFY_ERR_NO_HOME,
      MUSLIMTIFY_ERR_DAEMON_UNIT, MUSLIMTIFY_ERR_DAEMON_RELOAD, MUSLIMTIFY_ERR_DAEMON_ENABLE,
      MUSLIMTIFY_ERR_DAEMON_MANAGER,
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
  test_xdg_config_home();
  test_uninstall();
  test_status();
  test_messages();
  teardown();

  printf("\nResults: %d passed, %d failed\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
