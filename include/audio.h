#ifndef MUSLIMTIFY_AUDIO_H
#define MUSLIMTIFY_AUDIO_H

int audio_start(const char *path);

int audio_is_playing(void);

void audio_stop(void);

int audio_play_from_file(const char *path);

#endif // MUSLIMTIFY_AUDIO_H
