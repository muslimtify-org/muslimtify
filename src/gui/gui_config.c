#define _POSIX_C_SOURCE 200809L
#include "gui_config.h"
#include "json.h"
#include "platform.h"
#include "string_util.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Refuse to load a file larger than this, gui.json holds a few flags.
#define MAX_GUI_CONFIG_FILE_BYTES (64L * 1024L)

GuiConfig gui_config_default(void) {
  GuiConfig cfg = {0};
  cfg.prefer_dark = false;
  return cfg;
}

const char *gui_config_get_path(void) {
  static char path[PLATFORM_PATH_MAX] = {0};
  if (path[0] != '\0')
    return path;

  const char *dir = platform_config_dir();
  if (dir[0] != '\0')
    snprintf(path, sizeof(path), "%s%cgui.json", dir, PLATFORM_PATH_SEP);

  return path;
}

static char *read_file(const char *path) {
  FILE *f = platform_file_open(path, "r");
  if (!f)
    return NULL;

  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  if (size < 0 || size > MAX_GUI_CONFIG_FILE_BYTES) {
    fclose(f);
    return NULL;
  }
  fseek(f, 0, SEEK_SET);

  char *content = malloc((size_t)size + 1);
  if (!content) {
    fclose(f);
    return NULL;
  }

  size_t n = fread(content, 1, (size_t)size, f);
  fclose(f);
  if (n > (size_t)size)
    n = (size_t)size;
  content[n] = '\0';
  return content;
}

// True when content, ignoring surrounding whitespace, starts with '{' and ends
// with '}'. Catches truncated and empty files so a broken gui.json is reported
// instead of loading as defaults.
static bool looks_like_object(const char *content) {
  while (*content && isspace((unsigned char)*content))
    content++;
  if (*content != '{')
    return false;

  const char *end = content + strlen(content);
  while (end > content && isspace((unsigned char)end[-1]))
    end--;
  return end - content >= 2 && end[-1] == '}';
}

int gui_config_load(GuiConfig *cfg) {
  *cfg = gui_config_default();

  const char *path = gui_config_get_path();
  if (!platform_file_exists(path))
    return 0;

  char *content = read_file(path);
  if (!content) {
    fprintf(stderr, "Error: Cannot read %s\n", path);
    return -1;
  }

  if (!looks_like_object(content)) {
    fprintf(stderr, "Error: %s is not valid JSON, fix or delete it\n", path);
    free(content);
    return -1;
  }

  JsonContext *ctx = json_begin();
  if (!ctx) {
    free(content);
    return -1;
  }

  char *prefer_dark_str = get_value(ctx, "prefer_dark", content);
  if (prefer_dark_str) {
    if (strcmp(prefer_dark_str, "true") == 0)
      cfg->prefer_dark = true;
    else if (strcmp(prefer_dark_str, "false") == 0)
      cfg->prefer_dark = false;
  }

  json_end(ctx);
  free(content);
  return 0;
}

static int write_gui_json(FILE *f, const GuiConfig *cfg) {
  fprintf(f, "{\n");
  fprintf(f, "  \"prefer_dark\": %s\n", cfg->prefer_dark ? "true" : "false");
  fprintf(f, "}\n");
  return ferror(f) ? -1 : 0;
}

int gui_config_save(const GuiConfig *cfg) {
  const char *dir = platform_config_dir();
  if (dir[0] == '\0' || platform_mkdir_p(dir) != 0) {
    fprintf(stderr, "Error: Cannot create config directory '%s'\n", dir);
    return -1;
  }

  const char *path = gui_config_get_path();
  char tmp_path[PLATFORM_PATH_MAX];
  int n = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
  if (n < 0 || (size_t)n >= sizeof(tmp_path)) {
    fprintf(stderr, "Error: GUI config path too long\n");
    return -1;
  }

  char errbuf[128];
  FILE *f = platform_file_create_private(tmp_path);
  if (!f) {
    errno_string(errno, errbuf, sizeof(errbuf));
    fprintf(stderr, "Error: Cannot write %s: %s\n", tmp_path, errbuf);
    return -1;
  }

  // Close on every path before deciding, Windows cannot delete an open file.
  int write_err = write_gui_json(f, cfg) != 0 || platform_file_sync(f) != 0;
  int err = errno;
  if (fclose(f) != 0 && !write_err) {
    write_err = 1;
    err = errno;
  }
  if (write_err) {
    errno_string(err, errbuf, sizeof(errbuf));
    fprintf(stderr, "Error: Failed to write %s: %s\n", tmp_path, errbuf);
    platform_file_delete(tmp_path);
    return -1;
  }

  if (platform_atomic_rename(tmp_path, path) != 0) {
    errno_string(errno, errbuf, sizeof(errbuf));
    fprintf(stderr, "Error: Failed to save %s: %s\n", path, errbuf);
    platform_file_delete(tmp_path);
    return -1;
  }

  return 0;
}
