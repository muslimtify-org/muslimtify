#define _POSIX_C_SOURCE 200809L
#include "audio.h"
#include "miniaudio.h"

static ma_engine g_engine;
static ma_sound g_sound;
static int g_is_playing = 0;

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
