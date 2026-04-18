#define WIN32_LEAN_AND_MEAN

#include "check_cycle.h"

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

  int result = run_check_cycle();

  curl_global_cleanup();

  return result;
}
