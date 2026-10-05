#define WIN32_LEAN_AND_MEAN

#include "muslimtify.h"

#include <curl/curl.h>
#include <windows.h>

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int show_cmd) {
  (void)instance;
  (void)prev_instance;
  (void)cmd_line;
  (void)show_cmd;

  if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
    return 1;
  }

  int result = muslimtify_run_cycle(NULL) == MUSLIMTIFY_OK ? 0 : 1;

  curl_global_cleanup();

  return result;
}
