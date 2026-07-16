#define _POSIX_C_SOURCE 200809L

#include "daemon_loop.h"

#include <time.h>

int seconds_until_next_minute(time_t now) {
  return (int)(60 - now % 60);
}

#ifndef MUSLIMTIFY_DAEMON_LOOP_TEST

#include "check_cycle.h"
#include "config.h"
#include "location.h"

#include <signal.h>
#include <stdio.h>

static volatile sig_atomic_t g_stop = 0;

static void handle_stop_signal(int signum) {
  (void)signum;
  g_stop = 1;
}

/* Sleep until the next wall-clock minute boundary (<=60s), returning early when
 * a signal interrupts the sleep. Bounds each nap so suspend/resume or a clock
 * jump cannot overshoot, and keeps fires aligned to :00 like the old timer. */
static void sleep_to_next_minute(void) {
  struct timespec req = {.tv_sec = seconds_until_next_minute(time(NULL)), .tv_nsec = 0};
  nanosleep(&req, NULL); /* EINTR on signal: return early; loop re-checks g_stop */
}

int run_daemon_loop(void) {
  struct sigaction sa;
  sa.sa_handler = handle_stop_signal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0; /* no SA_RESTART: let nanosleep return on signal */
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);

  printf("muslimtify daemon: started (Ctrl+C or SIGTERM to stop)\n");
  fflush(stdout);

  bool refreshed = false;
  while (!g_stop) {
    if (run_check_cycle() != 0) {
      fprintf(stderr, "muslimtify daemon: check cycle reported an error, continuing\n");
    }
    if (g_stop)
      break;

    /* Refresh location once per daemon start (i.e. once per boot, since systemd
     * launches this at login). Done AFTER the first check so a slow or offline
     * network never delays the first notification; the fresh coordinates take
     * effect from the next cycle onward. Non-fatal: on failure we keep the
     * cached location and carry on. */
    if (!refreshed) {
      refreshed = true;
      Config cfg;
      if (config_load(&cfg) == 0 && location_refresh(&cfg) != 0) {
        fprintf(stderr, "muslimtify daemon: location refresh failed, using cached location\n");
      }
    }

    sleep_to_next_minute();
  }

  printf("muslimtify daemon: stopped\n");
  fflush(stdout);
  return 0;
}

#endif /* MUSLIMTIFY_DAEMON_LOOP_TEST */
