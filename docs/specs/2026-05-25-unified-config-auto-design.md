---
title: Unified `config auto` (consolidate location/method auto-detect)
date: 2026-05-25
status: approved
---

# Unified `config auto` — Design

## Problem
Auto-detection is fragmented across three code paths that each fetch location
via ipinfo and then map country → calculation method:
- `muslimtify location auto` (sets coords/timezone/country, but **not** method),
- `muslimtify method auto` (sets method, re-fetches location),
- `daemon enable` on Linux (`cmd_daemon.c`) and Windows (`cmd_daemon_win.c`),
  which inline the same fetch-then-detect block.

Having multiple `auto` commands is confusing and error-prone (e.g. `location
auto` silently leaves the method stale). The country→method mapping
(`COUNTRY_METHOD_MAP` in `src/method_detect.h`, ~28 entries) is also a separate
table from the new 249-entry `country.c` table (code+name), so country data is
duplicated across two structures.

## Goals
- Add one unified command: `muslimtify config auto [--city=<name>]` that detects
  location **and** sets the calculation method in a single step.
- Remove `location auto` and `method auto` subcommands.
- Extract one shared detection helper used by `config auto` and by `daemon
  enable` on both platforms — no duplicated detection blocks.
- Merge the country→method mapping into the single `country.c` master table
  (add a method field to `Country`; remove `COUNTRY_METHOD_MAP`).
- Auto-detect always overwrites `calculation_method` from the detected country
  (always-overwrite policy, consistent with today's `method auto`).
- Map every country to its best-fit *existing* regional method; all countries
  without a dedicated national method use `CALC_MWL` (the existing fallback).

## Non-goals
- No madhab auto-detection (explicitly dropped — madhab stays a manual `method
  madhab` choice).
- No new calculation methods. We only assign from the 21 methods that already
  exist (`CALC_MWL`..`CALC_MOONSIGHTING`); no inventing per-country methods.
- `location refresh` is kept as-is (location-only re-fetch); not folded in.
- `location set`, `location show`, `location clear`, and all `method`
  subcommands except `auto` are unchanged.
- No change to the `auto_detect` config flag semantics.

## Constraints
- C99; clean under `-Wall -Wextra -Wpedantic -Wshadow -Wformat=2` / `/W4`.
- Cross-platform: the master table and helper must not use platform-specific
  code. `country.c` stays in the platform-agnostic `muslimtify_core` library.
- clang-format (LLVM, 2-space, 100-col) must pass on all changed files.
- `country.h` gains a dependency on `prayertimes.h` (for `CalcMethod`); this is
  acceptable (`prayertimes.h` is header-only and already widely included).

## Approach

### 1. Master country table (`country.c` / `country.h`)
Extend the record:
```c
typedef struct {
  char code[3];      // ISO 3166-1 alpha-2, uppercase, NUL-terminated
  const char *name;  // English short name
  CalcMethod method; // best-fit calculation method; CALC_MWL when none dedicated
} Country;
```
`country.h` includes `prayertimes.h` for `CalcMethod`. Every one of the 249
rows gets a `method`. Regional assignments (best-fit existing methods):

- `ID`→KEMENAG; `MY`,`BN`→JAKIM; `SG`→SINGAPORE.
- `SA`,`YE`→MAKKAH; `AE`→DUBAI; `QA`→QATAR; `KW`→KUWAIT; `BH`,`OM`→GULF.
- `JO`,`PS`→JORDAN.
- `EG`,`LY`,`SD`→EGYPT.
- `TN`→TUNISIA; `DZ`→ALGERIA; `MA`→MOROCCO.
- `TR`→TURKEY.
- `US`,`CA`→ISNA.
- `PK`,`IN`,`BD`,`AF`→KARACHI.
- `FR`→FRANCE; `PT`→PORTUGAL; `RU`→RUSSIA.
- All other 220-odd countries → `CALC_MWL`.

New accessor (replaces `method_detect_by_country`):
```c
// Best-fit calculation method for ISO alpha-2 `code` (case-insensitive).
// Returns CALC_MWL for NULL, empty, unknown, or any country without a
// dedicated method.
CalcMethod country_default_method(const char *code);
```
Implemented with the same case-insensitive 2-letter lookup used by
`country_is_valid_alpha2` (reuse the bsearch over the sorted table; on hit
return the entry's `method`, else `CALC_MWL`).

Delete `src/method_detect.h` and its `COUNTRY_METHOD_MAP`.

### 2. Shared detection helper (`location.c` / `location.h`)
```c
// Fetch location via ipinfo and set calculation_method from the detected
// country. Mutates *cfg only; does NOT save config or print. Returns 0 on
// success, -1 on fetch failure.
int config_auto_detect(Config *cfg);
```
Body: `if (location_fetch(cfg) != 0) return -1;` then
`copy_string(cfg->calculation_method, sizeof(cfg->calculation_method),
method_to_string(country_default_method(cfg->country)));` `return 0;`.

The pre-existing unused-ish alias `location_auto_detect` (which just wrapped
`location_fetch`) is removed; `config_auto_detect` is its principled successor.

### 3. New command `config auto`
Add to `cmd_config.c`'s dispatch table: `{"auto", config_auto_handler}`.
Handler (mirrors the combined old `location auto` + `method auto` output):
1. Parse `--city=<name>` / `--city <name>` (reuse the same idiom as
   `cmd_location.c`).
2. `config_load`.
3. `printf("Detecting location...\n");` then `config_auto_detect(&cfg)`; on
   failure print error + return 1.
4. `cfg.auto_detect = true;` set `--city` label if provided (else leave as
   fetched — note ipinfo city is not auto-filled, matching current behavior).
5. `config_save`; on failure error + return 1.
6. `cache_invalidate()`.
7. Print: `✓ Location detected: …` (city,country or lat,lon) and `✓ Method
   auto-detected: <key> (<name>)`.
Usage string updated to `config [show|reset|validate|auto]`.

### 4. Removals and call-site updates
- `cmd_location.c`: remove `location_auto_handler` and its `{"auto", …}` entry.
  `parse_city_flag` / `set_city`: if `set_city` is still used by `set`, keep it;
  if `parse_city_flag` becomes unused after removing auto, remove it too (verify
  during implementation; no dead code left behind).
- `cmd_method.c`: remove `method_auto_handler` and its `{"auto", …}` entry; drop
  now-unused includes (`location.h`, `method_detect.h`, `cache.h` if unused).
- `cmd_daemon.c` and `cmd_daemon_win.c`: replace the inlined
  `location_fetch`+`method_detect_by_country`+save block with a call to
  `config_auto_detect(&cfg)` followed by their existing save/print/warning
  logic. Include `location.h` instead of `method_detect.h`.

### 5. Docs / help
- `cli.c`: update `location` help line (drop `auto`), add `config auto` to the
  `config` description and the Examples block; replace the `location auto`
  example with `config auto`.
- `README.md`: replace `location auto` usages with `config auto`.
- `AGENTS.md`: replace `muslimtify location auto` with `muslimtify config auto`.

### 6. Build
- `country.c` already in `muslimtify_core`; no new source files.
- Remove the `test_method_detect` target if its source is deleted, or repoint it
  (see Testing). `method_detect.h` deletion: ensure no remaining `#include
  "method_detect.h"` anywhere.

## Alternatives considered
- **Keep two tables, just expand `COUNTRY_METHOD_MAP`** (no merge): less churn
  but leaves country data split across two structures and the duplication the
  user wants gone. Rejected per the "single source of truth" decision.
- **Helper also saves + invalidates + prints**: fewer call-site lines, but bakes
  in I/O/printing that differs between the interactive command (status text) and
  the daemon (warnings), and is harder to unit-test. Rejected; the helper
  mutates `cfg` only.
- **Keep `method auto`/`location auto` as thin aliases of `config auto`**:
  avoids breaking muscle memory but re-introduces the "multiple auto commands"
  confusion the user explicitly wants removed. Rejected.

## Testing
- `tests/test_country.c`: add `test_country_default_method` — `ID`→`CALC_KEMENAG`,
  `SA`→`CALC_MAKKAH`, `US`→`CALC_ISNA`, `id` (lowercase)→`CALC_KEMENAG`,
  `"ZZ"`/unknown→`CALC_MWL`, `NULL`→`CALC_MWL`, and a spot-check that an
  unmapped-but-valid country (e.g. `JP`) →`CALC_MWL`.
- `test_method_detect`: since `method_detect_by_country` is removed, either
  delete `tests/test_method_detect.c` and its CMake target, or rename its
  assertions to call `country_default_method`. Decision: fold its country cases
  into `test_country` and remove `test_method_detect` (one country test module).
- `tests/test_cli.c` (Linux): assert `config auto` dispatches (no "Unknown config
  subcommand"), and that `location auto` / `method auto` now error as unknown
  subcommands.
- Manual: `config auto` against a temp `XDG_CONFIG_HOME` sets both location and
  method and prints both lines; `daemon enable` still auto-detects via the shared
  helper.

## Open questions
None.
