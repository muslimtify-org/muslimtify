#include "cli.h"
#include <curl/curl.h>

#ifdef _WIN32
#include "toast_activator.h"
#include <string.h>
#endif

int main(int argc, char **argv) {
#ifdef _WIN32
  /* When the user clicks a toast action, Windows COM-activates our
     ToastActivatorCLSID by relaunching this exe with "-Embedding". In that mode
     we only serve the activation callback (which stops the adhan) and exit --
     we never enter the normal CLI. */
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-Embedding") == 0 || strcmp(argv[i], "/Embedding") == 0) {
      return run_toast_activator_server();
    }
  }
#endif

  curl_global_init(CURL_GLOBAL_DEFAULT);

  int result = cli_run(argc, argv);

  curl_global_cleanup();

  return result;
}
