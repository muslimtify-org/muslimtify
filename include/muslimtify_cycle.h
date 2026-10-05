#ifndef MUSLIMTIFY_CYCLE_H
#define MUSLIMTIFY_CYCLE_H

#include "muslimtify.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The one call the background service makes. A frontend never needs it: the
   service installed through muslimtify.h does the notifying. */

/* What one muslimtify_run_cycle call did. */
typedef struct {
  bool detected;                 /* a first-run detection ran and was saved */
  bool refreshed;                /* a stale location was re-detected and saved */
  bool refresh_failed;           /* a stale refresh failed, the stored location was kept */
  MuslimtifyDetection detection; /* outcome of that detection or refresh, when one ran */
  int notifications;             /* notifications sent in this cycle */
} MuslimtifyCycle;

/**
 * Run one notification check for the current minute: load the config, detect
 * the location on a first run, refresh it when it is older than the refresh
 * interval, then send every notification that is due and play the adhan where
 * it is enabled. Call it once a minute. Takes no handle, because it reads the
 * config from disk each time, which is what makes saved settings take effect.
 * Blocks while an adhan plays, so call it off the UI thread.
 *
 * *out, when not NULL, is filled even on an error return. A failed stale
 * refresh is not an error: the cycle continues and sets out->refresh_failed.
 */
MuslimtifyError muslimtify_run_cycle(MuslimtifyCycle *out);

#ifdef __cplusplus
}
#endif

#endif /* MUSLIMTIFY_CYCLE_H */
