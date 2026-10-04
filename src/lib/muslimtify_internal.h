#ifndef MUSLIMTIFY_INTERNAL_H
#define MUSLIMTIFY_INTERNAL_H

#include "config.h"
#include "muslimtify.h"
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Not public. Shared between the library's source files, and the functions in
   muslimtify.h are wrappers over the ones below, with the config read from
   disk and the time read from the system clock. Tests use them to supply both,
   and cmd_show.c uses muslimtify_open_config until location detection moves
   into muslimtify_open. */

struct Muslimtify {
  Config cfg;
};

/** True when the handle's config has a location the queries can use. */
bool muslimtify_has_location(const Muslimtify *mt);

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
