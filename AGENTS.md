# AGENTS.md
This playbook briefs autonomous agents working inside Muslimtify project.
Always read the entire file before acting; updates frequently accompany specs in docs/.

## Canonical References
- README.md gives feature overview and common CLI usage; mimic its command names.
- docs/*.md explains the astronomical method powering src/prayertimes.h.
- CONTRIBUTING.md + .clang-format codify style + workflow; keep them synced.

## Toolchain Snapshot
- Language: C11 (CMake enforces CMAKE_C_STANDARD 11; avoid GCC-only extensions).
- Compilers: GCC/Clang on Linux, MSVC on Windows (warnings at /W4).
- Build system: CMake 3.22+ generating in-tree build/; muslimtify_lib is an OBJECT lib.
- Notifications: libnotify on Linux, WinRT toast (ole32, runtimeobject) on Windows.
- HTTP/workloads: libcurl (system lib on Linux, vendored via FetchContent on Windows).
- System integration: systemd user service + timer for periodic muslimtify check.

## Build Commands (Cross-platform)
1. Configure Debug: cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
2. Configure Release: cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
3. Build default config: cmake --build build
4. Multi-config (MSVC) Release build: cmake --build build --config Release
5. Run binary locally: build/bin/Debug/muslimtify.exe or build/bin/muslimtify
6. Install artifacts (Linux dev only): sudo ./install.sh

## Platform Notes
- Linux requires pkg-config, libnotify, libcurl, systemd (user) for daemon tasks.
- Windows uses FetchContent to download curl-8.20.0; optional libs (zlib, nghttp2, etc.) can be disabled via -DCURL_ZLIB=OFF etc.
- Tests depending on libnotify/libcurl (cli, config, cache, prayer_checker) are Linux-only.
- Windows-only sources: src/cmd_daemon_win.c, src/notification_win.c, src/platform_win.c.
- Linux-only sources: src/cmd_daemon.c, src/notification.c, src/platform_linux.c.

## Fast Start Workflow
- git clone https://github.com/rizukirr/muslimtify.git
- cmake -S . -B build
- cmake --build build -j
- build/bin/muslimtify show
- muslimtify config auto

## Testing
- Enable tests via -DBUILD_TESTING=ON (default).
- Run full suite: cmake --build build && (cd build && ctest --output-on-failure)
- Single test example: ctest --test-dir build -R prayertimes --output-on-failure
- Verbose JSON test: ctest --test-dir build -R json --output-on-failure -V
- Linux-only tests: cli, prayer_checker, config, cache (guarded by if(NOT WIN32)).
- Cross-platform tests: prayertimes, json (link libm only on non-Windows).

## Lint, Format, Static Checks
- clang-format -i src/*.c src/*.h include/*.h (LLVM base, 2 spaces, attach braces).
- Sort includes case-sensitively; include blocks preserved (see .clang-format).
- Treat compiler warnings as errors during review; fix C4200/C4116/C4996 by adhering to safe functions when possible or adding _CRT_SECURE_NO_WARNINGS just for tests.
- Maintain AddressSanitizer/UBSan flags for Debug builds on GCC/Clang; do not disable without justification.
- Manual linting: prefer static analysis via clang --analyze or MSVC /analyze when diagnosing tricky bugs.

## File + Module Map
- src/cli.c: entry dispatch; keep CLI table updates localized.
- src/cmd_*.c: each command handler (show, next, config, location, prayer, daemon, reminder).
- src/config.c + include/muslimtify_config.h (if added) manage JSON config (~/.config/muslimtify/config.json).
- src/cache.c handles caching of location + prayer results.
- src/location.c performs libcurl requests to ipinfo.io; errors bubble via LOCATION_ERR codes.
- src/display.c renders tables/colors/JSON; keep Unicode constants centralized.
- src/prayertimes.h contains header-only astronomical formulas per docs/KEMENAG_METHOD.md.
- src/json.h is a bespoke parser referenced by tests/test_json.c and config logic.
- src/notification*.c wrap libnotify or WinRT toast; respect platform preprocessor switches.
- systemd/*.service|timer store user units; stage when packaging.

## Coding Standards
- C standard: stick to stdint.h, stdbool.h, math.h; avoid GNU extensions.
- Indent with 2 spaces, no tabs; 100-col limit including comments.
- Brace style: Attach (same line); use explicit braces even for single-line conditionals.
- Prefer static functions in translation units; expose prototypes via include/ only if needed cross-file.
- Avoid VLAs; use fixed-size arrays or heap allocations.
- Favor enum classes via enum { ... } namespacing; prefix enumerators with module tag (e.g., CONFIG_ERR).
- Name functions snake_case, modules prefix (config_load, prayer_checker_next).
- Global structs use typedef struct { ... } config_t; local structs can stay anonymous.
- Use const correctness for pointers you do not mutate.
- Use size_t for lengths/indices; double for astronomical floats per prayertimes.h.

## Include, Imports, Dependencies
- Standard library headers precede third-party, then project headers; separate groups with blank lines.
- On Windows, include <windows.h> cautiously; define WIN32_LEAN_AND_MEAN before inclusion.
- Include src/json.h and src/prayertimes.h only where needed; keep translation units lean.
- For libcurl, include <curl/curl.h> after project headers to honor MSVC warning pragmas.
- Guard platform-specific includes with #ifdef _WIN32 / #ifndef _WIN32.

## Error Handling
- Return explicit status codes (enum) rather than bool when multiple failure modes exist.
- Log via fprintf(stderr, ...) or display_error helper; do not printf from library-style code.
- Wrap libcurl errors using curl_easy_strerror and propagate through CLI for user feedback.
- Config parsing: validate JSON nodes, check buffer sizes, avoid unchecked strncpy; prefer memcpy with bounds or snprintf.
- CLI commands should exit non-zero on failure so systemd timers report errors.
- When adding Windows APIs, convert UTF-16/UTF-8 carefully; use helper functions in notification_win.c.

## Threading + Timing
- Service runs as oneshot/cron style; no persistent threads expected.
- Prayer calculations rely on timezone offsets; ensure tzdata lookups or config values stay consistent.
- Location caching should respect TTL; consult docs/superpowers/plans/2026-03-22-prayer-cache.md before changing durations.

## CLI Experience
- Keep output sync with README examples (Unicode tables, highlighting next prayer with ▶).
- For JSON outputs (--format json), ensure stable key ordering for tests.
- Add new CLI verbs under cli_command[] dispatch with help text + completion hints.
- Update docs/ and README if user-facing behavior changes.

## Tests Breakdown
- tests/test_prayertimes.c loads CSV fixtures; update tolerance carefully (1-3 minutes).
- tests/test_json.c stress tests parser; add regression cases for every bug fix.
- Linux-only tests mimic CLI behavior; when touching CLI code, run them inside Linux CI or WSL.
- Add new tests via add_executable + add_test in CMakeLists; link muslimtify_lib and platform libs.
- Use ctest -R name -VV when diagnosing flakiness; tests log to stdout/stderr only.

## Formatting & Docs
- Update CHANGELOG.md for user-visible changes.
- Keep docs/superpowers/specs updated when altering Windows notification behavior; follow format already present.
- Document new calculation methods in docs/KEMENAG_METHOD.md or sibling doc.
- Include diagrams/screens in docs/images/ if adding UX changes.

## Git + Commit Hygiene
- Create topic branches from main; do not commit directly to main.
- Commit message format: type: short description (types: feat, fix, refactor, test, chore, docs), never use Author or Co-Authored commits.
- Never mix formatting-only and logic changes in one commit.
- Run tests + clang-format before pushing; CI expects clean tree.
- Avoid committing secrets (config.json, API keys); path ~/.config/muslimtify is user-specific.

## Packaging & Install
- install.sh builds, installs binary, copies assets and systemd units; ensure sudo context.
- uninstall.sh supports --purge to delete ~/.config/muslimtify; mention in docs when relevant.
- Assets (icons) live under assets/; keep PNG path consistent with install rules in CMakeLists.
- Systemd timer triggers muslimtify check every minute (OnCalendar=*:*:00).

## Windows Guidance
- configure: cmake -S . -B build
- build: cmake --build build --config Release
- Optional: supply vcpkg toolchain for curl deps (zlib, nghttp2, libidn2, libpsl, libssh2).
- Executables land under build/bin/Release; run muslimtify.exe directly via cmd or PowerShell.
- Notification toasts rely on AppUserModelID; see docs/superpowers/specs/2026-03-22-windows-notification-design.md when editing.

## Linux Guidance
- configure: cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
- build: cmake --build build -j$(nproc)
- run tests: cd build && ctest --output-on-failure
- install timer: muslimtify daemon install (writes to ~/.config/systemd/user).
- Validate notifications via notify-send before debugging muslimtify.

## Support Channels
- File GitHub issues for bugs/feature requests; templates live under .github/ISSUE_TEMPLATE/.
- Use discussions/Issues for calculation clarifications; include reference city/timezone data.
