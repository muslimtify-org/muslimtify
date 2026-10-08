#define _POSIX_C_SOURCE 200809L

#include "audio.h"
#include "notification.h"

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int failed = 0;

static void check_bool(const char *label, bool cond) {
  if (cond) {
    printf("  PASS: %s\n", label);
  } else {
    printf("  FAIL: %s\n", label);
    failed++;
  }
}

static void put_u32(FILE *f, uint32_t v) {
  unsigned char b[4] = {v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF, (v >> 24) & 0xFF};
  fwrite(b, 1, 4, f);
}

static void put_u16(FILE *f, uint16_t v) {
  unsigned char b[2] = {v & 0xFF, (v >> 8) & 0xFF};
  fwrite(b, 1, 2, f);
}

// A 60 second silent 16-bit mono 8 kHz WAV, long enough that only an
// interrupt can end it inside the test's time limit.
static bool write_silent_wav(const char *path) {
  const uint32_t rate = 8000;
  const uint32_t seconds = 60;
  const uint32_t data_bytes = rate * seconds * 2;
  FILE *f = fopen(path, "wb");
  if (!f)
    return false;
  fwrite("RIFF", 1, 4, f);
  put_u32(f, 36 + data_bytes);
  fwrite("WAVE", 1, 4, f);
  fwrite("fmt ", 1, 4, f);
  put_u32(f, 16);
  put_u16(f, 1);
  put_u16(f, 1);
  put_u32(f, rate);
  put_u32(f, rate * 2);
  put_u16(f, 2);
  put_u16(f, 16);
  fwrite("data", 1, 4, f);
  put_u32(f, data_bytes);
  unsigned char zeros[4096] = {0};
  uint32_t left = data_bytes;
  while (left > 0) {
    size_t n = left < sizeof(zeros) ? left : sizeof(zeros);
    if (fwrite(zeros, 1, n, f) != n) {
      fclose(f);
      return false;
    }
    left -= (uint32_t)n;
  }
  return fclose(f) == 0;
}

static void fire_interrupt(union sigval sv) {
  (void)sv;
  notify_adhan_interrupt();
}

static double elapsed_since(const struct timespec *start) {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (double)(now.tv_sec - start->tv_sec) + (double)(now.tv_nsec - start->tv_nsec) / 1e9;
}

int main(void) {
  printf("Running notification interrupt test...\n");

  char dir[] = "/tmp/mt_interrupt_XXXXXX";
  if (!mkdtemp(dir)) {
    fprintf(stderr, "FATAL: mkdtemp failed\n");
    return 1;
  }
  char wav[300];
  snprintf(wav, sizeof(wav), "%s/silence.wav", dir);
  if (!write_silent_wav(wav)) {
    fprintf(stderr, "FATAL: cannot write the WAV\n");
    return 1;
  }

  // Without an audio device there is nothing to interrupt, so the test cannot
  // prove anything here and says so instead of failing.
  if (audio_start(wav) != 0) {
    printf("  SKIP: no audio device, the interrupt path was not exercised\n");
    remove(wav);
    rmdir(dir);
    return 0;
  }
  audio_stop();

  check_bool("libnotify initialised", notify_init_once("muslimtify-test") != 0);

  timer_t timer;
  struct sigevent sev;
  memset(&sev, 0, sizeof(sev));
  sev.sigev_notify = SIGEV_THREAD;
  sev.sigev_notify_function = fire_interrupt;
  check_bool("timer created", timer_create(CLOCK_MONOTONIC, &sev, &timer) == 0);
  struct itimerspec when = {{0, 0}, {0, 300000000}};
  check_bool("timer armed", timer_settime(timer, 0, &when, NULL) == 0);

  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC, &start);
  notify_adhan("Fajr", "05:00", wav);
  double took = elapsed_since(&start);
  printf("  notify_adhan returned after %.2f s\n", took);

  check_bool("the adhan was cut off", took < 5.0);
  check_bool("audio is stopped", audio_is_playing() == 0);

  timer_delete(timer);
  notify_cleanup();
  remove(wav);
  rmdir(dir);

  printf("Results: %s\n", failed == 0 ? "all passed" : "FAILED");
  return failed == 0 ? 0 : 1;
}
