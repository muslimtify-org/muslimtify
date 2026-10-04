#include "cli_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_timeformat_help(void) {
  printf("\n");
  printf("Show or set the clock format used by every printed time\n");
  printf("\n");
  printf("Usage: muslimtify timeformat [<12|24>] [options]\n");
  printf("\n");
  printf("Commands:\n");
  printf("  %-25s %s\n", "<value>", "Set the clock format (12 or 24)");
  printf("  %-25s %s\n", "-h, --help", "Show this help");
  printf("\n");
  printf("Options:\n");
  printf("  %-25s %s\n", "--list", "List available clock formats");
  printf("\n");
  printf("Examples:\n");
  printf("  %-25s %s\n", "muslimtify timeformat", "# Show the current clock format");
  printf("  %-25s %s\n", "muslimtify timeformat 12", "# Print times as 04:35 PM");
  printf("  %-25s %s\n", "muslimtify timeformat 24", "# Print times as 16:35");
  printf("  %-25s %s\n", "muslimtify timeformat --list", "# List clock formats");
}

static int timeformat_run(Muslimtify *mt, int argc, char **argv) {
  int current = muslimtify_time_format(mt);

  if (argc == 0) {
    printf("Time format: %d-hour\n", current);
    return 0;
  }

  if (strcmp(argv[0], "--list") == 0) {
    printf("Available time formats:\n\n");
    printf("  %-8s %s %s\n", "12", current == 12 ? "*" : " ", "04:35 PM");
    printf("  %-8s %s %s\n", "24", current == 24 ? "*" : " ", "16:35");
    printf("\n* = current time format\n");
    return 0;
  }

  if (strcmp(argv[0], "12") != 0 && strcmp(argv[0], "24") != 0) {
    fprintf(stderr, "Error: Unknown time format '%s'\n", argv[0]);
    fprintf(stderr, "  Available: 12, 24\n");
    return 1;
  }

  MuslimtifyError err = muslimtify_set_time_format(mt, atoi(argv[0]));
  if (err == MUSLIMTIFY_OK)
    err = muslimtify_save(mt);
  if (err != MUSLIMTIFY_OK)
    return cli_fail(err);
  printf("Time format set to: %d-hour\n", muslimtify_time_format(mt));
  return 0;
}

int handle_timeformat(int argc, char **argv) {
  if (cli_wants_help(argc, argv)) {
    print_timeformat_help();
    return 0;
  }
  if (cli_reject_extra_args("timeformat", argc - 1, argv + 1))
    return 1;

  Muslimtify *mt = NULL;
  if (cli_open(&mt))
    return 1;
  int ret = timeformat_run(mt, argc, argv);
  muslimtify_close(mt);
  return ret;
}
