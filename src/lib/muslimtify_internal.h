#ifndef MUSLIMTIFY_INTERNAL_H
#define MUSLIMTIFY_INTERNAL_H

#include "config.h"
#include "muslimtify.h"
#include "platform.h"
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Not public. Shared between the library's source files, and the functions in
   muslimtify.h are wrappers over the ones below, with the config read from
   disk and the time read from the system clock. Tests use them to supply both,
   and cmd_show.c uses muslimtify_open_config until location detection moves
   into muslimtify_open. */

struct Muslimtify {
  Config cfg;
};

/** True when the handle's config has a location the queries can use. */
bool muslimtify_has_location(const Muslimtify *mt);

/** muslimtify_open with the config supplied from memory. */
MuslimtifyError muslimtify_open_config(const Config *cfg, Muslimtify **out);

/** muslimtify_day with the current local time supplied. */
MuslimtifyError muslimtify_day_at(const Muslimtify *mt, const struct tm *now, int year, int month,
                                  int day, MuslimtifyDay *out);

/** muslimtify_next with the current local time supplied. */
MuslimtifyError muslimtify_next_at(const Muslimtify *mt, const struct tm *now, MuslimtifyNext *out);

/** muslimtify_detect_location with the detection source supplied. */
MuslimtifyError muslimtify_detect_location_with(Muslimtify *mt,
                                                int (*detect)(Config *, GpsStatus *),
                                                MuslimtifyDetection *out);

/** muslimtify_set_gps with the receiver probe supplied. */
MuslimtifyError muslimtify_set_gps_with(Muslimtify *mt, bool enabled, GpsStatus (*probe)(Config *),
                                        bool *has_fix);

/* What a cycle calls to reach the outside world. Tests supply stand-ins that
   record the calls. notify_init returns non-zero on success. */
typedef struct {
  int (*detect)(Config *, GpsStatus *);
  int (*notify_init)(const char *app_name);
  void (*notify_prayer)(const char *prayer_name, const char *time_str, int minutes_before,
                        const char *urgency, const char *sound_preset);
  void (*notify_adhan)(const char *prayer_name, const char *time_str, const char *path);
  void (*notify_cleanup)(void);
} MuslimtifyCycleHooks;

/**
 * muslimtify_run_cycle with the outside world and the time supplied. `now` is
 * the local date and minute to work with, `now_epoch` the instant used to
 * decide whether the stored location is stale.
 */
MuslimtifyError muslimtify_run_cycle_at(const MuslimtifyCycleHooks *hooks, const struct tm *now,
                                        time_t now_epoch, MuslimtifyCycle *out);

/** muslimtify_notify_test with the outside world and the time supplied. */
MuslimtifyError muslimtify_notify_test_at(Muslimtify *mt, const MuslimtifyCycleHooks *hooks,
                                          const struct tm *now, bool adhan, MuslimtifyNext *sent);

/** muslimtify_daemon_install with the program to run supplied. */
MuslimtifyError muslimtify_daemon_install_binary(const char *daemon_binary,
                                                 MuslimtifyDaemonInstall *out);

#ifdef __cplusplus
}
#endif

#endif // MUSLIMTIFY_INTERNAL_H
