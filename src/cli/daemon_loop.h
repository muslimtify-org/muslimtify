#ifndef DAEMON_LOOP_H
#define DAEMON_LOOP_H

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Seconds to sleep until the next wall-clock minute boundary, in [1, 60].
 * Pure; exposed for unit testing. */
int seconds_until_next_minute(time_t now);

/* Runs the prayer-notification loop in the foreground until SIGTERM/SIGINT.
 * Calls muslimtify_run_cycle() once per wall-clock minute. Returns 0 on clean
 * shutdown. */
int run_daemon_loop(void);

#ifdef __cplusplus
}
#endif

#endif /* DAEMON_LOOP_H */
