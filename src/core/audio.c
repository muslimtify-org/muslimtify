#define _POSIX_C_SOURCE 200809L
#include "audio.h"
#include "miniaudio.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#include <unistd.h>
#endif

static ma_engine g_engine;
static ma_sound g_sound;
static int g_is_playing = 0;

static void sleep_ms(int ms) {
#ifdef _WIN32
  Sleep((DWORD)ms);
#else
  struct timespec req = {.tv_sec = ms / 1000, .tv_nsec = (long)((ms % 1000) * 1000000)};
  nanosleep(&req, NULL);
#endif
}

int audio_start(const char *path) {
  if (g_is_playing)
    audio_stop();

  if (ma_engine_init(NULL, &g_engine) != MA_SUCCESS)
    return -1;

  if (ma_sound_init_from_file(&g_engine, path, 0, NULL, NULL, &g_sound) != MA_SUCCESS) {
    ma_engine_uninit(&g_engine);
    return -1;
  }

  if (ma_sound_start(&g_sound) != MA_SUCCESS) {
    ma_sound_uninit(&g_sound);
    ma_engine_uninit(&g_engine);
    return -1;
  }

  g_is_playing = 1;
  return 0;
}

int audio_is_playing(void) {
  if (!g_is_playing)
    return 0;
  return ma_sound_is_playing(&g_sound);
}

void audio_stop(void) {
  if (!g_is_playing)
    return;

  ma_sound_uninit(&g_sound);
  ma_engine_uninit(&g_engine);
  g_is_playing = 0;
}

int audio_play_from_file(const char *path) {
  if (audio_start(path) != 0)
    return -1;

  while (audio_is_playing()) {
    sleep_ms(100);
  }

  audio_stop();
  return 0;
}
