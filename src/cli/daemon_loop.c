#define _POSIX_C_SOURCE 200809L

#include "daemon_loop.h"

#include <time.h>

int seconds_until_next_minute(time_t now) {
  return (int)(60 - now % 60);
}

#ifndef MUSLIMTIFY_DAEMON_LOOP_TEST

#include "muslimtify_cycle.h"

#include <signal.h>
#include <stdio.h>
#include <sys/select.h>

static volatile sig_atomic_t g_stop = 0;

static void handle_stop_signal(int signum) {
  (void)signum;
  g_stop = 1;
}

/* Sleep until the next wall-clock minute boundary (<=60s), returning early when
 * a stop signal arrives. The caller holds the stop signals blocked; pselect
 * unblocks them for exactly the duration of the sleep, so a signal that lands
 * between the g_stop check and the sleep is delivered as the sleep begins
 * instead of waiting out the whole minute. Bounds each nap so suspend/resume
 * or a clock jump cannot overshoot, and keeps fires aligned to :00. */
static void sleep_to_next_minute(const sigset_t *unblocked) {
  struct timespec req = {.tv_sec = seconds_until_next_minute(time(NULL)), .tv_nsec = 0};
  pselect(0, NULL, NULL, NULL, &req, unblocked);
}

int run_daemon_loop(void) {
  struct sigaction sa;
  sa.sa_handler = handle_stop_signal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);

  sigset_t stop_signals;
  sigemptyset(&stop_signals);
  sigaddset(&stop_signals, SIGTERM);
  sigaddset(&stop_signals, SIGINT);

  printf("muslimtify daemon: started (Ctrl+C or SIGTERM to stop)\n");
  fflush(stdout);

  while (!g_stop) {
    MuslimtifyCycle cycle;
    MuslimtifyError err = muslimtify_run_cycle(&cycle);
    const char *warning = muslimtify_detection_warning(&cycle.detection);
    if (warning)
      fprintf(stderr, "%s\n", warning);
    if (cycle.refresh_failed)
      fprintf(stderr, "check: location refresh failed, using cached location\n");
    if (err != MUSLIMTIFY_OK) {
      fprintf(stderr, "Error: %s\n", muslimtify_get_error(err));
      fprintf(stderr, "muslimtify daemon: check cycle reported an error, continuing\n");
    }

    sigset_t during_cycle;
    sigprocmask(SIG_BLOCK, &stop_signals, &during_cycle);
    if (!g_stop)
      sleep_to_next_minute(&during_cycle);
    sigprocmask(SIG_SETMASK, &during_cycle, NULL);
  }

  printf("muslimtify daemon: stopped\n");
  fflush(stdout);
  return 0;
}

#endif /* MUSLIMTIFY_DAEMON_LOOP_TEST */
