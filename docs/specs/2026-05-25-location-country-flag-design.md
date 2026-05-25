---
title: --country flag for `location set`
date: 2026-05-25
status: draft
---

# --country flag for `location set` — Design

## Problem
`muslimtify location set <lat> <lon>` lets users set coordinates manually and
optionally override `--timezone` and `--city`, but it **clears** the `country`
field (because the previously cached ipinfo country no longer applies to the new
coordinates). There is no way to set the country manually. We want a `--country`
flag, accepting only valid ISO 3166-1 alpha-2 codes.

## Goals
- Add `--country=<code>` / `--country <code>` to `muslimtify location set`.
- Validate the value against the real ISO 3166-1 alpha-2 code list (~249 codes).
- Accept input case-insensitively; store the normalized uppercase code in
  `cfg.country`.
- Reject invalid/unknown codes with a clear error; nothing is saved on rejection.

## Non-goals
- No `--country` on `location auto` (auto keeps deriving country from ipinfo).
- No alpha-3 / numeric code support — alpha-2 only.
- No name-lookup accessor or name display **yet** — the table stores the English
  short name per code so a future feature can expose it, but no accessor is
  added in this change.
- No change to how country is displayed or used downstream beyond echoing the
  code on `set`.

## Constraints
- C99; builds clean under `-Wall -Wextra -Wpedantic -Wshadow -Wformat=2` / `/W4`.
- Cross-platform (no platform-specific code in the new module).
- `cfg.country` is `char[64]`; a 2-letter code plus NUL fits trivially.
- clang-format (LLVM, 2-space, 100-col) must pass on new/changed files.

## Approach
**New module** `src/core/country.c` + `include/country.h`.

Public header exposes a record type plus the validation function:

```c
typedef struct {
  char code[3];      // ISO 3166-1 alpha-2, uppercase, NUL-terminated (e.g. "ID")
  const char *name;  // English short name (e.g. "Indonesia") — for future use
} Country;

// Returns true iff `code` is exactly two letters that, uppercased, match a
// known ISO 3166-1 alpha-2 code. Does not mutate `code`. Returns false for
// NULL, empty, wrong length, or non-letter input.
bool country_is_valid_alpha2(const char *code);
```

Implementation: a `static const Country ISO_3166_1_ALPHA2[]` table sorted
ascending by `code`, queried with `bsearch`. `country_is_valid_alpha2` first
checks `code` is exactly 2 ASCII letters, builds a 2-char uppercased key, then
binary-searches the table comparing the key against each entry's `.code`. The
`.name` field is populated now but read by no caller yet (reserved for a future
name-lookup/display feature) — storing it costs nothing and avoids a later data
migration.

**Wiring in `src/cli/cmd_location.c`:**
- Extend `location_set_handler`'s argument loop with `--country=` / `--country`
  cases, mirroring the existing `--city` handling, capturing `override_country`.
- A small static helper `set_country(Config *cfg, const char *code)` uppercases
  the 2 chars into `cfg.country` (NUL-terminated). Validation happens before the
  config is saved.
- Validation point: after parsing positionals/coords, if `override_country` is
  set, call `country_is_valid_alpha2`; on failure print error + `return 1`
  before any `config_save`. On success, `set_country` writes the normalized code
  into `cfg.country` (which `set` otherwise leaves cleared).
- Update `LOCATION_SET_USAGE` to include `[--country=<iso2>]`.
- Echo the country in the success output when set (e.g. `  Country: ID`).

**Data flow:**
`location set ... --country=id`
→ parse loop captures `override_country = "id"`
→ `country_is_valid_alpha2("id")` → true
→ `set_country` writes `"ID"` into `cfg.country`
→ `config_save` → `✓ Location set ...` with `  Country: ID`.

**Error handling:**
- Missing value: `Error: --country requires a value (e.g. --country=ID)` → exit 1.
- Invalid/unknown: `Error: Invalid country code '<v>' (expected ISO 3166-1 alpha-2, e.g. ID)` → exit 1.
- Both paths return before `config_save`, leaving config untouched.

**CMake/CI:**
- Add `src/core/country.c` to the `muslimtify_lib` OBJECT library.
- Register `test_country` in the test section (cross-platform).

## Alternatives considered
- **Header-only table in `include/country.h`** (matching `prayertimes.h`/`json.h`):
  rejected — bakes a ~249-entry data array into every translation unit that
  includes it; the project reserves header-only for logic, not bulk data.
- **Format-only check (two `isalpha` chars), no table:** rejected — does not
  satisfy the requirement to validate against actual ISO 3166-1 alpha-2 codes.

## Testing
New cross-platform `tests/test_country.c` using the existing pass/fail counter
framework:
- Valid: `"ID"`, `"id"`, `"iD"`, `"US"`, `"ZW"` → true.
- Invalid: `"XX"` (unknown), `"I"` (too short), `"IDN"` (too long), `"1D"`
  (non-letter), `""` (empty), `NULL` → false.
- Boundary codes: first and last entries of the sorted table → true.

Manual/CLI check: `location set -6.21 106.84 --country=id` round-trips `"ID"`
into config and prints `  Country: ID`; an invalid code exits non-zero and does
not modify config.

## Open questions
None.
