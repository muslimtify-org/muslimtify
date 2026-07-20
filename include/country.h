#ifndef COUNTRY_H
#define COUNTRY_H

#include "prayertimes.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * One ISO 3166-1 alpha-2 country record.
 *
 * `code` is the uppercase two-letter code (NUL-terminated, e.g. "ID").
 */
typedef struct {
  char code[3];      // ISO 3166-1 alpha-2, uppercase, NUL-terminated
  CalcMethod method; // best-fit calculation method; CALC_MWL when none dedicated
} Country;

/**
 * Returns true iff `code` is exactly two letters that, uppercased, match a
 * known ISO 3166-1 alpha-2 code. Does not mutate `code`. Returns false for
 * NULL, empty, wrong length, or non-letter input.
 */
bool country_is_valid_alpha2(const char *code);

/**
 * Best-fit calculation method for ISO 3166-1 alpha-2 `code` (case-insensitive).
 * Returns CALC_MWL for NULL, empty, malformed, unknown, or any country without
 * a dedicated method.
 */
CalcMethod country_default_method(const char *code);

#ifdef __cplusplus
}
#endif

#endif // COUNTRY_H
