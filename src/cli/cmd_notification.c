#include "cli_internal.h"
#include "config.h"
#include "display.h"
#include "location.h"
#include "notification.h"
#include "platform.h"
#include "prayer_checker.h"
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int notification_test(int argc, char **argv) {
  if (cli_wants_help(argc, argv)) {
    printf("Usage: muslimtify notification test [--adhan]\n");
    return 0;
  }
  // Checked before anything is loaded or sent.
  int consumed = (argc > 0 && strcmp(argv[0], "--adhan") == 0) ? 1 : 0;
  if (cli_reject_extra_args("notification test", argc - consumed, argv + consumed))
    return 1;

  Config cfg;
  if (config_load(&cfg) != 0) {
    fprintf(stderr, "Error: Failed to load config\n");
    return 1;
  }
  if (ensure_location(&cfg) != 0)
    return 1;

  time_t now = time(NULL);
  struct tm tm_buf;
  platform_localtime(&now, &tm_buf);
  struct tm *tm_now = &tm_buf;

  struct PrayerTimes times =
      prayer_times_for_config(&cfg, tm_now->tm_year + 1900, tm_now->tm_mon + 1, tm_now->tm_mday);

  NextPrayer next = prayer_get_next(&cfg, tm_now, &times);
  if (next.type == PRAYER_NONE) {
    fprintf(stderr, "No upcoming prayers enabled.\n");
    return 1;
  }

  if (!notify_init_once("Muslimtify")) {
    fprintf(stderr, "Error: Failed to initialize notification system\n");
    return 1;
  }

  char time_str[16];
  format_time_cfg(&cfg, next.time, time_str, sizeof(time_str));
  const char *sound_preset =
      strcmp(cfg.notification_sound, "off") != 0 ? cfg.notification_sound_alarm : NULL;
  if (argc > 0 && strcmp(argv[0], "--adhan") == 0) {
    // Use the next prayer's configured adhan; notify_adhan falls back to the
    // bundled adhan when the configured path is empty.
    const PrayerConfig *pcfg = prayer_get_config(&cfg, next.type);
    notify_adhan(prayer_get_name(next.type), time_str, pcfg ? pcfg->adhan : "");
  } else {
    notify_prayer(prayer_get_name(next.type), time_str, 0, cfg.notification_urgency, sound_preset);
  }

  notify_cleanup();
  printf("Sent test notification for %s at %s\n", prayer_get_name(next.type), time_str);
  return 0;
}

static void print_notification_help(void) {
  printf("\n");
  printf("Show or configure prayer notifications\n");
  printf("\n");
  printf("Usage: muslimtify notification [options]\n");
  printf("\n");
  printf("Commands:\n");
  printf("  %-25s %s\n", "enable|disable [prayer|all]", "Toggle prayer notifications");
  printf("  %-25s %s\n", "-h, --help", "Show this help");
  printf("\n");
  printf("Options:\n");
  printf("  %-25s %s\n", "--json", "Show settings as JSON");
  printf("  %-25s %s\n", "--headless", "Show settings as key=value");
  printf("  %-25s %s\n", "--urgency <level>", "Set urgency: normal|critical|low");
  printf("  %-25s %s\n", "--reminder <prayer|--all> <minutes...>", "Set pre-prayer reminders");
  printf("  %-25s %s\n", "--adhan <enable|disable> <prayer>", "Toggle per-prayer adhan");
  printf("  %-25s %s\n", "--adhan set <path>", "Set adhan audio file");
  printf("  %-25s %s\n", "--sound <adhan|default|off>", "Set notification sound mode");
  printf("\n");
  printf("Examples:\n");
  printf("  %-25s %s\n", "muslimtify notification", "# Show current settings");
  printf("  %-25s %s\n", "muslimtify notification enable fajr", "# Enable Fajr notification");
  printf("  %-25s %s\n", "muslimtify notification --reminder fajr 30 15 5", "# Set reminders");
  printf("  %-25s %s\n", "muslimtify notification --sound adhan", "# Play adhan on notify");
}

// Run `set` on one prayer, or on every prayer when `all`, stopping at the
// first failure.
typedef MuslimtifyError (*PrayerFlagSetter)(Muslimtify *, MuslimtifyPrayerType, bool);

static MuslimtifyError set_prayer_flag(Muslimtify *mt, PrayerFlagSetter set, bool all,
                                       MuslimtifyPrayerType prayer, bool value) {
  if (!all)
    return set(mt, prayer, value);
  MuslimtifyError err = MUSLIMTIFY_OK;
  for (int i = 0; i < MUSLIMTIFY_PRAYER_COUNT && err == MUSLIMTIFY_OK; i++)
    err = set(mt, (MuslimtifyPrayerType)i, value);
  return err;
}

// Save what the setters changed. Prints the error and returns 1 on failure.
static int save_or_fail(Muslimtify *mt, MuslimtifyError err) {
  if (err == MUSLIMTIFY_OK)
    err = muslimtify_save(mt);
  return err == MUSLIMTIFY_OK ? 0 : cli_fail(err);
}

// Set enabled on one prayer or all of them.
static int notif_enable(int argc, char **argv, bool enable) {
  const char *command = enable ? "notification enable" : "notification disable";
  if (cli_wants_help(argc, argv)) {
    printf("Usage: muslimtify %s [prayer|all]\n", command);
    return 0;
  }
  if (cli_reject_extra_args(command, argc - 1, argv + 1))
    return 1;

  bool all = argc == 0 || strcmp(argv[0], "all") == 0;
  MuslimtifyPrayerType prayer = MUSLIMTIFY_FAJR;
  if (!all && muslimtify_parse_prayer(argv[0], &prayer) != MUSLIMTIFY_OK)
    return cli_unknown_prayer(argv[0]);

  Muslimtify *mt = NULL;
  if (cli_open(&mt))
    return 1;
  int ret =
      save_or_fail(mt, set_prayer_flag(mt, muslimtify_set_prayer_enabled, all, prayer, enable));
  muslimtify_close(mt);
  if (ret != 0)
    return ret;

  if (all)
    printf("All prayers %s\n", enable ? "enabled" : "disabled");
  else
    printf("%s notifications %s\n", argv[0], enable ? "enabled" : "disabled");
  return 0;
}

static void print_reminder_help(void) {
  printf("Usage: muslimtify notification --reminder <prayer|--all> <minutes...>\n");
  printf("       muslimtify notification --reminder <prayer> none   (clear)\n");
}

static int notif_urgency(int argc, char **argv) {
  if (cli_wants_help(argc, argv)) {
    printf("Usage: muslimtify notification --urgency <normal|critical|low>\n");
    return 0;
  }
  if (argc < 1) {
    fprintf(stderr, "Usage: muslimtify notification --urgency <normal|critical|low>\n");
    return 1;
  }
  MuslimtifyUrgency urgency;
  if (muslimtify_parse_urgency(argv[0], &urgency) != MUSLIMTIFY_OK) {
    fprintf(stderr, "Error: urgency must be normal, critical, or low\n");
    return 1;
  }
  if (cli_reject_extra_args("notification --urgency", argc - 1, argv + 1))
    return 1;

  Muslimtify *mt = NULL;
  if (cli_open(&mt))
    return 1;
  int ret = save_or_fail(mt, muslimtify_set_urgency(mt, urgency));
  muslimtify_close(mt);
  if (ret != 0)
    return ret;
  printf("Urgency set to %s\n", argv[0]);
  return 0;
}

// Parse trailing space-separated minutes into `out` as integers. A lone "none"
// or "clear" means no reminders and returns 0. Otherwise returns the count, -1
// when a value is not an integer, -2 when no value is given, or -3 when more
// than MUSLIMTIFY_MAX_REMINDERS values are given. Whether each integer is an
// acceptable reminder is the library's call.
static int parse_minute_args(int argc, char **argv, int *out) {
  if (argc == 1 && (strcmp(argv[0], "none") == 0 || strcmp(argv[0], "clear") == 0))
    return 0;
  if (argc == 0)
    return -2;
  if (argc > MUSLIMTIFY_MAX_REMINDERS)
    return -3;
  int count = 0;
  for (int i = 0; i < argc; i++) {
    char *end = NULL;
    long v = strtol(argv[i], &end, 10);
    if (end == argv[i] || *end != '\0' || v < INT_MIN || v > INT_MAX)
      return -1;
    out[count++] = (int)v;
  }
  return count;
}

// Print the error for a negative parse_minute_args result. Returns 1.
static int minute_args_error(int rc) {
  if (rc == -3)
    return cli_fail(MUSLIMTIFY_ERR_TOO_MANY_REMINDERS);
  if (rc == -2)
    fprintf(stderr, "Error: --reminder needs at least one minute value, or none to clear\n");
  else
    fprintf(stderr, "Error: reminder minutes must be integers 1..1440\n");
  return 1;
}

static int notif_reminder(int argc, char **argv) {
  if (cli_wants_help(argc, argv)) {
    print_reminder_help();
    return 0;
  }

  bool all = false;
  if (argc > 0 && strcmp(argv[0], "--all") == 0) {
    all = true;
    argc--;
    argv++;
  }

  const char *prayer_name = NULL;
  if (!all) {
    if (argc < 1) {
      print_reminder_help();
      return 1;
    }
    prayer_name = argv[0];
    argc--;
    argv++;
  }

  int mins[MUSLIMTIFY_MAX_REMINDERS];
  int count = parse_minute_args(argc, argv, mins);
  if (count < 0)
    return minute_args_error(count);

  Muslimtify *mt = NULL;
  if (cli_open(&mt))
    return 1;

  MuslimtifyPrayerType prayer = MUSLIMTIFY_FAJR;
  if (!all && muslimtify_parse_prayer(prayer_name, &prayer) != MUSLIMTIFY_OK) {
    muslimtify_close(mt);
    return cli_unknown_prayer(prayer_name);
  }

  MuslimtifyError err = MUSLIMTIFY_OK;
  if (all) {
    for (int i = 0; i < MUSLIMTIFY_PRAYER_COUNT && err == MUSLIMTIFY_OK; i++)
      err = muslimtify_set_prayer_reminders(mt, (MuslimtifyPrayerType)i, mins, (size_t)count);
  } else {
    err = muslimtify_set_prayer_reminders(mt, prayer, mins, (size_t)count);
  }
  int ret = save_or_fail(mt, err);
  muslimtify_close(mt);
  if (ret != 0)
    return ret;

  if (all)
    printf("Reminders updated for all prayers\n");
  else if (count == 0)
    printf("Reminders cleared for %s\n", prayer_name);
  else
    printf("Reminders updated for %s\n", prayer_name);
  return 0;
}

// Point every prayer at one adhan file. The library validates the path and
// stores its canonical form, which is what the confirmation prints.
static int notif_adhan_set(Muslimtify *mt, const char *path) {
  MuslimtifyError err = MUSLIMTIFY_OK;
  for (int i = 0; i < MUSLIMTIFY_PRAYER_COUNT && err == MUSLIMTIFY_OK; i++)
    err = muslimtify_set_prayer_adhan_file(mt, (MuslimtifyPrayerType)i, path);
  if (err != MUSLIMTIFY_OK)
    return cli_fail_value(err, path);
  if (save_or_fail(mt, MUSLIMTIFY_OK) != 0)
    return 1;

  MuslimtifyNotification settings;
  err = muslimtify_get_notification(mt, &settings);
  if (err != MUSLIMTIFY_OK)
    return cli_fail(err);
  printf("Adhan file set to %s\n", settings.prayers[MUSLIMTIFY_FAJR].adhan_file);
  return 0;
}

static int notif_adhan(int argc, char **argv) {
  if (cli_wants_help(argc, argv)) {
    printf("Usage: muslimtify notification --adhan <enable|disable> <prayer> | set <path>\n");
    return 0;
  }
  if (argc > 0 && strcmp(argv[0], "stop") == 0) {
    if (cli_reject_extra_args("notification --adhan stop", argc - 1, argv + 1))
      return 1;
    if (notify_adhan_stop() == 0)
      printf("Adhan playback stopped\n");
    else
      printf("No adhan is currently playing\n");
    return 0;
  }
  if (argc < 2) {
    fprintf(stderr,
            "Usage: muslimtify notification --adhan <enable|disable> <prayer> | set <path>\n");
    return 1;
  }
  if (cli_reject_extra_args("notification --adhan", argc - 2, argv + 2))
    return 1;

  Muslimtify *mt = NULL;
  if (cli_open(&mt))
    return 1;

  if (strcmp(argv[0], "set") == 0) {
    int ret = notif_adhan_set(mt, argv[1]);
    muslimtify_close(mt);
    return ret;
  }

  bool enable;
  if (strcmp(argv[0], "enable") == 0)
    enable = true;
  else if (strcmp(argv[0], "disable") == 0)
    enable = false;
  else {
    muslimtify_close(mt);
    fprintf(stderr, "Error: --adhan expects enable, disable, or set\n");
    return 1;
  }

  MuslimtifyPrayerType prayer;
  if (muslimtify_parse_prayer(argv[1], &prayer) != MUSLIMTIFY_OK) {
    muslimtify_close(mt);
    return cli_unknown_prayer(argv[1]);
  }
  int ret = save_or_fail(mt, muslimtify_set_prayer_adhan(mt, prayer, enable));
  muslimtify_close(mt);
  if (ret != 0)
    return ret;
  printf("Adhan %s for %s\n", enable ? "enabled" : "disabled", argv[1]);
  return 0;
}

static int notif_sound(int argc, char **argv) {
  if (cli_wants_help(argc, argv)) {
    printf("Usage: muslimtify notification --sound <adhan|default|off>\n");
    return 0;
  }
  MuslimtifySoundMode mode;
  if (argc < 1 || muslimtify_parse_sound_mode(argv[0], &mode) != MUSLIMTIFY_OK) {
    fprintf(stderr, "Error: --sound expects adhan, default, or off\n");
    return 1;
  }
  if (cli_reject_extra_args("notification --sound", argc - 1, argv + 1))
    return 1;

  Muslimtify *mt = NULL;
  if (cli_open(&mt))
    return 1;
  int ret = save_or_fail(mt, muslimtify_set_sound_mode(mt, mode));
  muslimtify_close(mt);
  if (ret != 0)
    return ret;
  printf("Sound mode set to %s\n", argv[0]);
  return 0;
}

int handle_notification(int argc, char **argv) {
  if (argc > 0) {
    if (strcmp(argv[0], "enable") == 0)
      return notif_enable(argc - 1, argv + 1, true);
    if (strcmp(argv[0], "disable") == 0)
      return notif_enable(argc - 1, argv + 1, false);
    if (strcmp(argv[0], "test") == 0)
      return notification_test(argc - 1, argv + 1);
    if (strcmp(argv[0], "--urgency") == 0)
      return notif_urgency(argc - 1, argv + 1);
    if (strcmp(argv[0], "--reminder") == 0)
      return notif_reminder(argc - 1, argv + 1);
    if (strcmp(argv[0], "--adhan") == 0)
      return notif_adhan(argc - 1, argv + 1);
    if (strcmp(argv[0], "--sound") == 0)
      return notif_sound(argc - 1, argv + 1);
  }

  if (cli_wants_help(argc, argv)) {
    print_notification_help();
    return 0;
  }

  // Default settings view: only --json / --headless are accepted here.
  for (int i = 0; i < argc; i++) {
    const char *a = argv[i];
    if (strcmp(a, "--json") == 0 || strcmp(a, "--headless") == 0)
      continue;
    fprintf(stderr, "Error: unknown notification argument '%s'\n", a);
    return 1;
  }

  OutputMode mode = OUTPUT_TABLE;
  if (cli_parse_output_mode(argc, argv, &mode) != 0)
    return 1;

  Muslimtify *mt = NULL;
  if (cli_open(&mt))
    return 1;
  MuslimtifyNotification settings;
  MuslimtifyError err = muslimtify_get_notification(mt, &settings);
  muslimtify_close(mt);
  if (err != MUSLIMTIFY_OK)
    return cli_fail(err);

  switch (mode) {
  case OUTPUT_JSON:
    display_notification_settings_json(&settings);
    break;
  case OUTPUT_HEADLESS:
    display_notification_settings_headless(&settings);
    break;
  default:
    display_notification_settings(&settings);
    break;
  }
  return 0;
}
