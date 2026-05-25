# Verification Report — --country flag for `location set`

**Date:** 2026-05-25
**Spec:** docs/specs/2026-05-25-location-country-flag-design.md
**Plan:** docs/plans/2026-05-25-location-country-flag.md
**Commit verified:** 0ddca9f (range 28f790f..0ddca9f)

> Note on method: the spec prescribes three independent subagent verdict passes
> per requirement. This feature's content (the embedded ISO country dataset)
> reproducibly trips the environment content filter on subagent I/O, so verdict
> passes and evidence collection were performed inline. The spec is small (2
> tasks, 4 goals); every verdict below is backed by verbatim command output.

## Repo-level checks

- Tests: **pass** — `ctest --test-dir build --output-on-failure` → exit 0
  ```
  10/11 Test #10: country ..........................   Passed    0.03 sec
        Start 11: method_detect
  11/11 Test #11: method_detect ....................   Passed    0.02 sec

  100% tests passed, 0 tests failed out of 11

  Total Test time (real) =   0.34 sec
  ```
- Build (clean rebuild, Debug, GCC): **pass** — `cmake --build build-verify --target muslimtify`
  ```
  NO warnings/errors for country.c or cmd_location.c
  ```
  (compiled under `-Wall -Wextra -Wpedantic -Wshadow -Wformat=2`, CMakeLists.txt:64)
- Linter (clang-format CI gate): **pass** — `clang-format --dry-run --Werror src/cli/*.c src/core/*.c include/*.h tests/*.c` → exit 0
  ```
  FORMAT CLEAN (exit 0)
  ```
- `git status --porcelain`:
  ```
  (empty — clean)
  ```
- `git log --oneline 28f790f..HEAD`:
  ```
  0ddca9f feat: add --country flag to location set
  073cfbe feat: add ISO 3166-1 alpha-2 country validation module
  ```
- Surgical-diff pass: **clean** — `git diff --name-only 28f790f..HEAD`
  ```
  CMakeLists.txt          → Task 1 (lib + test wiring)
  include/country.h       → Task 1
  src/core/country.c      → Task 1
  src/cli/cmd_location.c  → Task 2
  tests/test_country.c    → Task 1
  ```
  Zero orphans: every changed file is listed in a plan task's "Files" section.

## Requirements

### R1 (Goal). "Add `--country=<code>` / `--country <code>` to `muslimtify location set`."
- Verdict: **satisfied**
- Evidence:
  - Commit: `0ddca9f — feat: add --country flag to location set`
  - `--country=id` form, exit 0:
    ```
    ✓ Location set to: -6.2146, 106.8451
      Country: ID
    exit=0
    ```
  - `--country US` space form, exit 0, config shows `"country": "US"`.

### R2 (Goal). "Validate the value against the real ISO 3166-1 alpha-2 code list (~249 codes)."
- Verdict: **satisfied**
- Evidence:
  - `src/core/country.c` table row count = 249 (`grep -c '^    {"'`), sorted ascending (`sort -c` → SORTED OK).
  - Generated from the authoritative local `iso-codes` dataset (`/usr/share/iso-codes/json/iso_3166-1.json`).
  - `test_country` passes 13/13 including valid `ID/US/ZW` and boundary `AD`/`ZW`.

### R3 (Goal). "Accept input case-insensitively; store the normalized uppercase code in `cfg.country`."
- Verdict: **satisfied**
- Evidence:
  - Input `id` (lowercase) stored as uppercase:
    ```
      Country: ID
    "country": "ID"
    ```
  - `country_is_valid_alpha2` uppercases via `toupper` before `bsearch`; `set_country` uppercases on store (cmd_location.c:64).

### R4 (Goal). "Reject invalid/unknown codes with a clear error; nothing is saved on rejection."
- Verdict: **satisfied**
- Evidence:
  - Unknown `XX` rejected, exit 1, config unchanged (still `"US"` from prior run):
    ```
    Error: Invalid country code 'XX' (expected ISO 3166-1 alpha-2, e.g. ID)
    exit=1
    "country": "US"
    ```
  - Missing value rejected:
    ```
    Error: --country requires a value (e.g. --country=ID)
    exit=1
    ```

### R5 (Non-goal). "No `--country` on `location auto`."
- Verdict: **satisfied** (forbidden behavior absent)
- Evidence: `override_country` / `country_is_valid_alpha2` / `set_country` appear only within `location_set_handler` (cmd_location.c:155+). `location_auto_handler` references none of them.

### R6 (Non-goal). "No alpha-3 / numeric code support — alpha-2 only."
- Verdict: **satisfied** (forbidden behavior absent)
- Evidence: alpha-3 `IDN` rejected:
  ```
  Error: Invalid country code 'IDN' (expected ISO 3166-1 alpha-2, e.g. ID)
  exit=1
  ```
  (`country_is_valid_alpha2` requires `code[2] == '\0'`.)

### R7 (Non-goal). "No name-lookup accessor or name display yet — table stores name, no accessor added."
- Verdict: **satisfied** (forbidden behavior absent)
- Evidence: `grep -rn "country_name\|->name\|.name" src/core/country.c include/country.h` → "NO accessor / name field never read". The `Country.name` field is initialized but read by no code.

### R8 (Non-goal). "No change to how country is displayed or used downstream beyond echoing the code on `set`."
- Verdict: **satisfied** (forbidden behavior absent)
- Evidence: `git diff --name-only 28f790f..HEAD` does not include `src/core/display.c` (display.c NOT touched). The only output change is the `Country:` echo in `location set` (cmd_location.c), which the spec authorizes.

### C1 (Constraint). "C99; builds clean under `-Wall -Wextra -Wpedantic -Wshadow -Wformat=2` / `/W4`."
- Verdict: **satisfied**
- Evidence: clean rebuild produced no warnings/errors for country.c or cmd_location.c; flags present at CMakeLists.txt:64.

### C2 (Constraint). "Cross-platform (no platform-specific code in the new module)."
- Verdict: **satisfied**
- Evidence: `src/core/country.c` includes only `<ctype.h> <stdlib.h> <string.h>`; placed in the platform-agnostic `muslimtify_core` OBJECT library, not a platform-split source.

### C3 (Constraint). "`cfg.country` is `char[64]`; a 2-letter code plus NUL fits trivially."
- Verdict: **satisfied**
- Evidence: `config.h:27` `char country[64];`; `set_country` writes 2 chars + NUL.

### C4 (Constraint). "clang-format (LLVM, 2-space, 100-col) must pass on new/changed files."
- Verdict: **satisfied**
- Evidence: `clang-format --dry-run --Werror` → exit 0 (FORMAT CLEAN).

## Disagreements

None.

## Overall verdict

**ready** — all 8 requirements and 4 constraints satisfied, all repo-level checks pass (build clean, 11/11 tests, lint clean, working tree clean), no disagreements, surgical-diff pass returned `clean` (zero orphans).
