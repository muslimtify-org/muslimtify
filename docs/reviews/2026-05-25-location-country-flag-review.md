# Review — --country flag for `location set`

**Date:** 2026-05-25
**Spec:** docs/specs/2026-05-25-location-country-flag-design.md
**Plan:** docs/plans/2026-05-25-location-country-flag.md
**Verify report:** docs/verifications/2026-05-25-location-country-flag-verify.md (verdict: ready)
**Commits under review:** 28f790f..0ddca9f on vibe/location-country-flag

## Diff summary

- Files changed: 5
- Lines added: 421, removed: 2 (249 of the additions are the pure ISO data table)
- Commits: 2 code commits (`073cfbe`, `0ddca9f`) + 1 docs commit (`e791dd8`, verify report)

## Findings

### Block
- None.

### Warn
- ~~**Test does not assert the table stays sorted.**~~ **RESOLVED in `c3a335d`.** Added `country_table()` accessor (also serves the future name-lookup feature) and `test_table_sorted`, which walks the table asserting `strcmp(prev,cur) < 0` for every adjacent pair. Test suite now 15/15.

### Nit
- **`--country` parsing duplicates the `--city` flag idiom** (equals-form + space-form branches) rather than factoring a shared flag parser. This intentionally matches the existing `--timezone`/`--city` style in the same function (cmd_location.c), so consistency wins over DRY here; noting only for completeness.
- Plan task checkboxes were not committed as `chore: complete Task N` markers because `docs/plans/` is gitignored in this repo. Bookkeeping only; no code impact.

## Self-critique (three risks)

1. **Out-of-order table → false negatives from bsearch.** Mitigation: `sort -c` verified ascending; head/mid/tail codes tested. Follow-up test described in Warn above.
2. **Locale-dependent `isalpha`/`toupper`.** In a non-C locale `isalpha` could accept accented bytes. Mitigation: the program never calls `setlocale`, so it runs in the default "C" locale (ASCII-only classification). Risk is latent, not active.
3. **Short-string reads in the validator.** For `"I"` / `""`, `country_is_valid_alpha2` reads `code[0]` then `code[1]`; short-circuit `&&` stops at the first non-alpha (`'\0'`), and `code[1]` of a length-1 NUL-terminated string is in-bounds. Mitigation: `""`, `"I"`, and `NULL` are all tested and return false (verify report R4/R6).

## Diff

`src/cli/cmd_location.c` (verbatim):

```diff
@@ includes
 #include "cache.h"
 #include "cli_internal.h"
+#include "country.h"
 #include "display.h"
 #include "location.h"
+#include <ctype.h>
 #include <errno.h>
@@ after set_city()
+// Copy the 2-letter `code` into cfg->country, uppercased and NUL-terminated.
+// Caller must have validated `code` via country_is_valid_alpha2 first.
+static void set_country(Config *cfg, const char *code) {
+  cfg->country[0] = (char)toupper((unsigned char)code[0]);
+  cfg->country[1] = (char)toupper((unsigned char)code[1]);
+  cfg->country[2] = '\0';
+}
@@ LOCATION_SET_USAGE
-    "[--city=<name>]\n";
+    "[--city=<name>] [--country=<iso2>]\n";
@@ location_set_handler declarations
+  const char *override_country = NULL;
@@ arg parse loop (after --city space form)
+    } else if (strncmp(argv[i], "--country=", 10) == 0) {
+      override_country = argv[i] + 10;
+      if (*override_country == '\0') {
+        fprintf(stderr, "Error: --country requires a value (e.g. --country=ID)\n");
+        return 1;
+      }
+    } else if (strcmp(argv[i], "--country") == 0) {
+      if (i + 1 >= argc) {
+        fprintf(stderr, "Error: --country requires a value (e.g. --country ID)\n");
+        return 1;
+      }
+      override_country = argv[++i];
@@ after clearing city/country + applying --city
+  if (override_country) {
+    if (!country_is_valid_alpha2(override_country)) {
+      fprintf(stderr, "Error: Invalid country code '%s' (expected ISO 3166-1 alpha-2, e.g. ID)\n",
+              override_country);
+      return 1;
+    }
+    set_country(&cfg, override_country);
+  }
@@ success output (after City line)
+  if (cfg.country[0] != '\0')
+    printf("  Country: %s\n", cfg.country);
```

`include/country.h`, `tests/test_country.c`, `CMakeLists.txt`, and the 249-row table in
`src/core/country.c` are reviewed via:
`git diff 28f790f..0ddca9f -- include/country.h tests/test_country.c CMakeLists.txt src/core/country.c`
(table omitted inline — it is verified pure data: 249 rows, sorted ascending, sourced from the
local `iso-codes` dataset).

## Sign-off

- [ ] User reviewed findings.
- [ ] User reviewed diff.
- [ ] User approves proceeding to finish-branch.
