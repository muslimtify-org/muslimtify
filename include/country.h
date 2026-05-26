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
 * `name` is the English short name (e.g. "Indonesia"). `name` is provided for
 * a future name-lookup/display feature and is currently read by no caller.
 */
typedef struct {
  char code[3];      // ISO 3166-1 alpha-2, uppercase, NUL-terminated
  const char *name;  // English short name
  CalcMethod method; // best-fit calculation method; CALC_MWL when none dedicated
} Country;

/**
 * Returns true iff `code` is exactly two letters that, uppercased, match a
 * known ISO 3166-1 alpha-2 code. Does not mutate `code`. Returns false for
 * NULL, empty, wrong length, or non-letter input.
 */
bool country_is_valid_alpha2(const char *code);

/**
 * Return a pointer to the ISO 3166-1 alpha-2 table, sorted ascending by `code`,
 * and write the number of entries into `*count` (if non-NULL). The table is
 * statically allocated and lives for the program's lifetime; callers must not
 * modify it. Intended for the future name-lookup feature and for invariant
 * tests (the sorted order that `country_is_valid_alpha2`'s bsearch depends on).
 */
const Country *country_table(size_t *count);

/**
 * Best-fit calculation method for ISO 3166-1 alpha-2 `code` (case-insensitive).
 * Returns CALC_MWL for NULL, empty, malformed, unknown, or any country without
 * a dedicated method. Replaces the former method_detect_by_country().
 */
CalcMethod country_default_method(const char *code);

#ifdef __cplusplus
}
#endif

#endif // COUNTRY_H
