#ifndef MUSLIMTIFY_AUDIO_H
#define MUSLIMTIFY_AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Start playing an audio file asynchronously (non-blocking).
 * Decodes MP3/WAV/FLAC via miniaudio. Only one sound plays at a time:
 * if playback is already in progress it is stopped first.
 * Returns: 0 on success, -1 on failure (init/decode/start error).
 */
int audio_start(const char *path);

/**
 * Report whether audio started by audio_start() is still playing.
 * Returns: 1 while playing, 0 once it has finished, been stopped, or never started.
 */
int audio_is_playing(void);

/**
 * Stop playback and release the audio engine and decoder.
 * No-op if nothing is currently playing.
 */
void audio_stop(void);

/**
 * Play an audio file and block until it finishes on its own.
 * Convenience wrapper: audio_start() then poll audio_is_playing() until done.
 * Returns: 0 after the sound ends, -1 if playback could not be started.
 */
int audio_play_from_file(const char *path);

#ifdef __cplusplus
}
#endif

#endif // MUSLIMTIFY_AUDIO_H
