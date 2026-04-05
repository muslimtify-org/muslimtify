#include "cli_internal.h"
#include "platform.h"

#include <stdio.h>

#ifndef _WIN32
#include <unistd.h>
#endif

int cmd_run_resolve_gui_path_for_test(const char *exe_dir, char *buffer, size_t buffer_size) {
  if (!exe_dir || !buffer || buffer_size == 0 || exe_dir[0] == '\0')
    return 1;

#ifdef _WIN32
  int written = snprintf(buffer, buffer_size, "%s\\muslimtify-gui.exe", exe_dir);
#else
  int written = snprintf(buffer, buffer_size, "%s/muslimtify-gui", exe_dir);
#endif
  if (written < 0 || (size_t)written >= buffer_size) {
    if (buffer_size > 0)
      buffer[0] = '\0';
    return 1;
  }

  return 0;
}

int handle_run(int argc, char **argv) {
  (void)argc;
  (void)argv;

  const char *exe_dir = platform_exe_dir();
  if (!exe_dir || exe_dir[0] == '\0') {
    fprintf(stderr, "Error: could not determine executable directory\n");
    return 1;
  }

  char gui_path[PLATFORM_PATH_MAX];
  if (cmd_run_resolve_gui_path_for_test(exe_dir, gui_path, sizeof(gui_path)) != 0) {
    fprintf(stderr, "Error: could not resolve GUI executable path\n");
    return 1;
  }

#ifdef _WIN32
  fprintf(stderr, "Error: muslimtify run is not implemented yet on Windows\n");
  return 1;
#else
  execl(gui_path, gui_path, (char *)NULL);
  perror("muslimtify-gui");
  return 1;
#endif
}
