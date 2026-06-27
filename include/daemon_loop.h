#ifndef DAEMON_LOOP_H
#define DAEMON_LOOP_H

#ifdef __cplusplus
extern "C" {
#endif

/* Runs the prayer-notification loop in the foreground until SIGTERM/SIGINT.
 * Calls run_check_cycle() once per wall-clock minute. Returns 0 on clean
 * shutdown. */
int run_daemon_loop(void);

#ifdef __cplusplus
}
#endif

#endif /* DAEMON_LOOP_H */
