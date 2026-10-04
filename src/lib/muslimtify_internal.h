#ifndef MUSLIMTIFY_INTERNAL_H
#define MUSLIMTIFY_INTERNAL_H

#include "config.h"
#include "muslimtify.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Test seams. The public functions in muslimtify.h are wrappers over these,
   with the config read from disk and the time read from the system clock. */

/** muslimtify_open with the config supplied from memory. */
MuslimtifyError muslimtify_open_config(const Config *cfg, Muslimtify **out);

/** muslimtify_day with the current local time supplied. */
MuslimtifyError muslimtify_day_at(const Muslimtify *mt, const struct tm *now, int year, int month,
                                  int day, MuslimtifyDay *out);

/** muslimtify_next with the current local time supplied. */
MuslimtifyError muslimtify_next_at(const Muslimtify *mt, const struct tm *now, MuslimtifyNext *out);

#ifdef __cplusplus
}
#endif

#endif // MUSLIMTIFY_INTERNAL_H
