#include "fonts.h"
#include "platform.h"
#include <ccompose.h>
#include <stdio.h>
#include <stdlib.h>

#define FONT_MANROPE_BOLD_SIZE 48
#define FONT_MANROPE_REGULAR_SIZE 48

int FONT_MANROPE_BOLD = 0;
int FONT_MANROPE_REGULAR = 0;

static int resolve_font(char *dst, size_t dst_size, const char *leaf) {
  const char *prefixes[] = {
      "/usr/local/share/muslimtify/fonts",
      "/usr/share/muslimtify/fonts",
      NULL,
      NULL,
      "gui/fonts",
      "../gui/fonts",
      "../../gui/fonts",
  };

  char xdg_path[PLATFORM_PATH_MAX];
  const char *xdg_data = getenv("XDG_DATA_HOME");
  if (xdg_data != NULL && snprintf(
                              xdg_path,
                              sizeof(xdg_path),
                              "%s%cmuslimtify%cfonts",
                              xdg_data,
                              PLATFORM_PATH_SEP,
                              PLATFORM_PATH_SEP
                          ) < (int)sizeof(xdg_path)) {
    prefixes[2] = xdg_path;
  }

  char exe_path[PLATFORM_PATH_MAX];
  const char *exe = platform_exe_dir();
  if (exe[0] != '\0' && snprintf(
                            exe_path,
                            sizeof(exe_path),
                            "%s%c..%cshare%cmuslimtify%cfonts",
                            exe,
                            PLATFORM_PATH_SEP,
                            PLATFORM_PATH_SEP,
                            PLATFORM_PATH_SEP,
                            PLATFORM_PATH_SEP
                        ) < (int)sizeof(exe_path)) {
    prefixes[3] = exe_path;
  }

  for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i++) {
    if (prefixes[i] == NULL) {
      continue;
    }
    int n = snprintf(dst, dst_size, "%s%c%s", prefixes[i], PLATFORM_PATH_SEP, leaf);
    if (n < 0 || (size_t)n >= dst_size) {
      continue;
    }
    if (platform_file_exists(dst)) {
      return 0;
    }
  }
  return -1;
}

void muslimtify_fonts_load(void) {
  char path[PLATFORM_PATH_MAX];

  if (resolve_font(path, sizeof(path), "manrope/manrope-bold.ttf") != 0) {
    fprintf(stderr, "muslimtify-gui: manrope-bold.ttf not found, using default font\n");
    return;
  }

  int id = CC_LoadFont(path, FONT_MANROPE_BOLD_SIZE);
  if (id < 0) {
    fprintf(stderr, "muslimtify-gui: failed to load %s, using default font\n", path);
    return;
  }
  FONT_MANROPE_BOLD = id;

  if (resolve_font(path, sizeof(path), "manrope/manrope-regular.ttf") != 0) {
    fprintf(stderr, "muslimtify-gui: manrope-regular.ttf not found, using default font\n");
    return;
  }

  id = CC_LoadFont(path, FONT_MANROPE_REGULAR_SIZE);
  if (id < 0) {
    fprintf(stderr, "muslimtify-gui: failed to load %s, using default font\n", path);
    return;
  }

  FONT_MANROPE_REGULAR = id;
}
