# Review — Unified `config auto`

**Date:** 2026-05-25
**Spec:** docs/specs/2026-05-25-unified-config-auto-design.md
**Plan:** docs/plans/2026-05-25-unified-config-auto.md
**Verify report:** docs/verifications/2026-05-25-unified-config-auto-verify.md (verdict: ready)
**Commits under review:** 9c6ce1e..5808eea on vibe/unified-config-auto

## Diff summary

- Files changed: 17 (code), +1 docs (verify report)
- Lines (code, excl. docs): 427 insertions, 576 deletions (net −149 — a simplification)
- Commits: 6 code/docs commits (b4c8cd2 → 5808eea)

## Findings

### Block
- None.

### Warn
- None.

### Nit
- ~~`include/country.h:45` stale comment naming `method_detect_by_country()`~~ — **RESOLVED** in `chore: drop stale comment and unused include`.
- ~~`src/cli/cmd_daemon.c:15` unused `#include "string_util.h"`~~ — **RESOLVED** in the same commit. Build + 10/10 tests + lint still clean after removal.
- Print blocks (`✓ Location detected: …` + `✓ Method auto-detected: …`) are duplicated across `cmd_config.c`, `cmd_daemon.c`, `cmd_daemon_win.c`. The spec scoped the shared helper to *detection* only (not display), and the daemon wording differs slightly (Windows has no `✓`), so this is left as-is — noted only for completeness.

## Self-critique (three risks)

1. **A regional method assignment is wrong for an untested country** (e.g. `BN`→JAKIM, `PS`→JORDAN). `test_country` checks 9 mapped countries + fallbacks, not all 29. Mitigation: the 29 assignments are the spec's §Approach list verbatim, and the table is verified sorted + 249 rows. Residual risk is a judgment call on a specific country's method, not a code defect; follow-up would be asserting all 29 mapped rows if desired.
2. **Empty/absent country from ipinfo silently sets method to `mwl`.** If ipinfo returns no `country`, `country_default_method("")` → `CALC_MWL` and `calculation_method` becomes `"mwl"`. Mitigation: this is the intended always-overwrite + MWL-fallback behavior (spec G5/G6); `expect_method("", CALC_MWL, …)` covers it.
3. **`daemon enable` auto-detect path is not exercised by an automated test** (it needs systemd/Windows). Mitigation: the daemon now calls the *same* `config_auto_detect` that `config auto` verified live end-to-end (Indonesia→kemenag); the daemon-specific surrounding save/print code is structurally unchanged from before this work. Pre-existing gap, not introduced here.

## Diff

Full diff (verbatim) via:
`git diff 9c6ce1e..5808eea`

Key code hunks reviewed:
- `include/country.h` — `#include "prayertimes.h"`, `CalcMethod method;` field, `country_default_method` decl.
- `src/core/country.c` — 249-row table gains the `method` column (29 non-MWL); `country_default_method` (bsearch, MWL fallback).
- `src/core/location.c` / `include/location.h` — `config_auto_detect` replaces the `location_auto_detect` alias.
- `src/cli/cmd_config.c` — `config_auto_handler` + `{"auto", …}` + usage string.
- `src/cli/cmd_location.c` / `src/cli/cmd_method.c` — `auto` handlers + dispatch entries removed; `parse_city_flag` removed; method/location includes trimmed.
- `src/cli/cmd_daemon.c` / `src/cli/cmd_daemon_win.c` — rewired to `config_auto_detect`; `method_detect.h`→`prayertimes.h`.
- `src/method_detect.h`, `tests/test_method_detect.c` — deleted.
- `src/cli/cli.c`, `README.md`, `AGENTS.md` — `config auto` replaces the old commands.
- `tests/test_country.c` — `test_country_default_method` (17 assertions); `tests/test_cli.c` — removed-subcommand assertions.

## Sign-off

- [ ] User reviewed findings.
- [ ] User reviewed diff.
- [ ] User approves proceeding to finish-branch.
