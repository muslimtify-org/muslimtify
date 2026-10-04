#include "cli_internal.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static void print_offset_help(void) {
  printf("\n");
  printf("Adjust prayer times by a fixed number of minutes\n");
  printf("\n");
  printf("Usage: muslimtify offset <prayer> <minutes>\n");
  printf("\n");
  printf("Commands:\n");
  printf("  %-25s %s\n", "<prayer> <minutes>", "Adjust one prayer time");
  printf("  %-25s %s\n", "all <minutes>", "Adjust every prayer time");
  printf("  %-25s %s\n", "-h, --help", "Show this help");
  printf("\n");
  printf("Note: minutes is a signed integer from -60 to 60 (e.g. +4, -2, 0).\n");
  printf("\n");
  printf("Examples:\n");
  printf("  %-25s %s\n", "muslimtify offset fajr +4", "# Shift Fajr 4 min later");
  printf("  %-25s %s\n", "muslimtify offset asr -2", "# Shift Asr 2 min earlier");
  printf("  %-25s %s\n", "muslimtify offset all 0", "# Reset every prayer offset");
}

static int offset_run(Muslimtify *mt, const char *prayer_name, int minutes) {
  bool is_all = strcmp(prayer_name, "all") == 0;
  MuslimtifyError err = MUSLIMTIFY_OK;

  if (is_all) {
    for (int i = 0; i < MUSLIMTIFY_PRAYER_COUNT && err == MUSLIMTIFY_OK; i++)
      err = muslimtify_set_prayer_offset(mt, (MuslimtifyPrayerType)i, minutes);
  } else {
    MuslimtifyPrayerType prayer;
    if (muslimtify_parse_prayer(prayer_name, &prayer) != MUSLIMTIFY_OK)
      return cli_unknown_prayer(prayer_name);
    err = muslimtify_set_prayer_offset(mt, prayer, minutes);
  }

  if (err == MUSLIMTIFY_OK)
    err = muslimtify_save(mt);
  if (err != MUSLIMTIFY_OK)
    return cli_fail(err);

  printf("✓ Offset set to %+d min for %s\n", minutes, is_all ? "all prayers" : prayer_name);
  return 0;
}

int handle_offset(int argc, char **argv) {
  if (cli_wants_help(argc, argv)) {
    print_offset_help();
    return 0;
  }
  if (argc < 2) {
    fprintf(stderr, "Error: offset requires a prayer and minutes\n");
    print_offset_help();
    return 1;
  }
  if (cli_reject_extra_args("offset", argc - 2, argv + 2))
    return 1;

  // Text that is not an integer is the CLI's error. Whether the integer is an
  // acceptable offset is the library's.
  const char *value_str = argv[1];
  char *end = NULL;
  long value = strtol(value_str, &end, 10);
  if (end == value_str || *end != '\0') {
    fprintf(stderr, "Error: Offset must be an integer from -60 to 60\n");
    return 1;
  }
  if (value < INT_MIN || value > INT_MAX)
    return cli_fail(MUSLIMTIFY_ERR_INVALID_OFFSET);

  Muslimtify *mt = NULL;
  if (cli_open(&mt))
    return 1;
  int ret = offset_run(mt, argv[0], (int)value);
  muslimtify_close(mt);
  return ret;
}
