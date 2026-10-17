#ifndef MUSLIMTIFY_STORAGE_H
#define MUSLIMTIFY_STORAGE_H

#include "muslimtify.h"

static char *prayer_names[] = {
    [MUSLIMTIFY_FAJR] = "Fajr",
    [MUSLIMTIFY_DHUHR] = "Dhuhr",
    [MUSLIMTIFY_ASR] = "Asr",
    [MUSLIMTIFY_MAGHRIB] = "Maghrib",
    [MUSLIMTIFY_ISHA] = "Isha",
};

typedef struct {
  MuslimtifyNext next;
} MuslimtifyInfo;

#endif // MUSLIMTIFY_STORAGE_H
