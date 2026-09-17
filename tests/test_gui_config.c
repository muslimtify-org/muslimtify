#define _GNU_SOURCE
#include "gui_config.h"
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int passed = 0;
static int failed = 0;

static char tmpdir[256];

static void check_bool(const char *test, bool cond) {
  if (cond) {
    passed++;
  } else {
    failed++;
    fprintf(stderr, "FAIL [%s]\n", test);
  }
}

static void setup(void) {
  snprintf(tmpdir, sizeof(tmpdir), "/tmp/mt_guicfgtest_XXXXXX");
  if (!mkdtemp(tmpdir)) {
    fprintf(stderr, "FATAL: mkdtemp failed\n");
    exit(1);
  }
  setenv("XDG_CONFIG_HOME", tmpdir, 1);
}

static void teardown(void) {
  char cmd[1024];
  snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
  if (system(cmd) != 0) { /* best-effort cleanup */
  }
}

static void write_raw(const char *content) {
  const char *path = gui_config_get_path();
  unlink(path);
  if (content == NULL)
    return;

  const char *dir = platform_config_dir();
  platform_mkdir_p(dir);
  FILE *f = fopen(path, "w");
  if (!f) {
    fprintf(stderr, "FATAL: cannot write %s\n", path);
    exit(1);
  }
  fputs(content, f);
  fclose(f);
}

static char *slurp_compact(const char *path) {
  static char buf[256];
  FILE *f = fopen(path, "r");
  if (!f)
    return NULL;
  size_t n = 0;
  int c;
  while ((c = fgetc(f)) != EOF && n + 1 < sizeof(buf)) {
    if (c != ' ' && c != '\n' && c != '\r' && c != '\t')
      buf[n++] = (char)c;
  }
  buf[n] = '\0';
  fclose(f);
  return buf;
}

static void test_path(void) {
  printf("  path...\n");
  const char *path = gui_config_get_path();
  check_bool("path under tmpdir", strncmp(path, tmpdir, strlen(tmpdir)) == 0);
  size_t len = strlen(path);
  check_bool("path ends in gui.json", len >= 8 && strcmp(path + len - 8, "gui.json") == 0);
}

static void test_missing_file(void) {
  printf("  missing file...\n");
  write_raw(NULL);
  GuiConfig cfg = {.prefer_dark = true};
  check_bool("missing returns 0", gui_config_load(&cfg) == 0);
  check_bool("missing gives default", cfg.prefer_dark == false);
}

static void test_round_trip(void) {
  printf("  round trip...\n");
  GuiConfig cfg = gui_config_default();
  cfg.prefer_dark = true;
  check_bool("save true returns 0", gui_config_save(&cfg) == 0);
  char *compact = slurp_compact(gui_config_get_path());
  check_bool("file content true", compact && strcmp(compact, "{\"prefer_dark\":true}") == 0);

  GuiConfig loaded = gui_config_default();
  check_bool("load true returns 0", gui_config_load(&loaded) == 0);
  check_bool("load true value", loaded.prefer_dark == true);

  cfg.prefer_dark = false;
  check_bool("save false returns 0", gui_config_save(&cfg) == 0);
  compact = slurp_compact(gui_config_get_path());
  check_bool("file content false", compact && strcmp(compact, "{\"prefer_dark\":false}") == 0);
  loaded.prefer_dark = true;
  check_bool("load false returns 0", gui_config_load(&loaded) == 0);
  check_bool("load false value", loaded.prefer_dark == false);
}

static void test_missing_or_bad_key(void) {
  printf("  missing or bad key...\n");
  GuiConfig cfg;

  write_raw("{}\n");
  check_bool("empty object returns 0", gui_config_load(&cfg) == 0);
  check_bool("empty object default", cfg.prefer_dark == false);

  write_raw("{\"prefer_dark\": \"yes\"}\n");
  check_bool("string value returns 0", gui_config_load(&cfg) == 0);
  check_bool("string value default", cfg.prefer_dark == false);

  write_raw("{\"prefer_dark\": 1}\n");
  check_bool("number value returns 0", gui_config_load(&cfg) == 0);
  check_bool("number value default", cfg.prefer_dark == false);
}

static void test_malformed(void) {
  printf("  malformed...\n");
  GuiConfig cfg;

  write_raw("{\"prefer_dark\": true");
  cfg.prefer_dark = true;
  check_bool("truncated returns -1", gui_config_load(&cfg) == -1);
  check_bool("truncated default", cfg.prefer_dark == false);

  write_raw("");
  cfg.prefer_dark = true;
  check_bool("empty returns -1", gui_config_load(&cfg) == -1);
  check_bool("empty default", cfg.prefer_dark == false);

  write_raw("[true]");
  cfg.prefer_dark = true;
  check_bool("array returns -1", gui_config_load(&cfg) == -1);
  check_bool("array default", cfg.prefer_dark == false);
}

int main(void) {
  setup();

  printf("Running gui config tests...\n");
  test_path();
  test_missing_file();
  test_round_trip();
  test_missing_or_bad_key();
  test_malformed();

  teardown();
  printf("gui config: %d passed, %d failed\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
