#define WIN32_LEAN_AND_MEAN

#include "muslimtify_cycle.h"

#include <windows.h>

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int show_cmd) {
  (void)instance;
  (void)prev_instance;
  (void)cmd_line;
  (void)show_cmd;

  return muslimtify_run_cycle(NULL) == MUSLIMTIFY_OK ? 0 : 1;
}
