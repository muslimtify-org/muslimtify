# AGENTS.md

Playbook for agents working in Muslimtify — a C11 CLI that calculates prayer times locally and delivers desktop notifications on Linux and Windows. Read `README.md` for features and CLI usage, and mirror its command names.

## Toolchain
- **C11**, no compiler extensions (`CMAKE_C_STANDARD 11`, `CMAKE_C_EXTENSIONS OFF`). GCC/Clang on Linux, MSVC (`/W4`) on Windows.
- **CMake 3.22+**. Two OBJECT libraries: `muslimtify_core` (`src/core/` + platform abstraction) and `muslimtify_cli` (`src/cli/`); the `muslimtify` binary is `src/muslimtify.c`.
- **Notifications:** libnotify (Linux), WinRT toast (Windows). **HTTP:** libcurl (system lib on Linux, FetchContent-vendored on Windows).
- **Daemon:** a long-running `Type=simple` systemd *user* service that runs `muslimtify daemon run` — a self-scheduling loop (`src/core/daemon_loop.c`). There is no timer. `muslimtify check` is the one-shot cycle (`src/core/check_cycle.c`).

## Build & test
```
cmake -S . -B build                                  # add -DCMAKE_BUILD_TYPE=Debug for ASan/UBSan
cmake --build build                                  # MSVC: append --config Release
ctest --test-dir build --output-on-failure
ctest --test-dir build -R <name> --output-on-failure # single test
```
Many tests are Linux-only (guarded by `if(NOT WIN32)`). Add new tests with `add_executable` + `add_test` in `CMakeLists.txt`.

## Layout
- `src/core/` — platform-agnostic logic: config, cache, location (ipinfo.io via libcurl), country, prayer_checker, check_cycle, daemon_loop, display.
- `src/cli/` — dispatch (`cli.c`) + one `cmd_*.c` per command. Daemon is `cmd_daemon.c` (Linux) / `cmd_daemon_win.c` (Windows).
- `src/platform/{linux,windows}/` — notification, platform paths, timezone.
- `include/` — public headers; header-only astronomical formulas in `prayertimes.h` (see `docs/KEMENAG_METHOD.md`). `src/json.h` is a bespoke JSON parser used by config + `tests/test_json.c`.
- `.packages/{aur,fedora,debian,winget}` — distro packaging. `systemd/muslimtify.service.in` is a `configure_file` template.

## Style
- clang-format: LLVM base, 2-space indent, 100 columns, attach braces, case-sensitive sorted includes (`.clang-format`). CI runs `clang-format --dry-run --Werror`.
- snake_case functions with a module prefix (`config_load`); explicit braces; `const`-correct pointers; `size_t` for sizes/indices; avoid GNU extensions and VLAs.
- Return enum status codes for multi-mode failures; log via `fprintf(stderr, …)` / the display helper, not `printf`. Commands exit non-zero on failure.
- Guard platform code with `#ifdef _WIN32` / `#ifndef _WIN32`; POSIX-only sources (e.g. `daemon_loop.c`) are excluded from the Windows build via CMake generator expressions.

## Git
- Branch from `main`; never commit directly to `main`. Open a PR; CI (gcc/clang/MSVC build + lint + packaging-version check) must be green.
- Conventional commit subjects: `feat`, `fix`, `refactor`, `test`, `chore`, `docs`. **Never** add `Author` or `Co-Authored-By` trailers.
- Keep formatting-only and logic changes in separate commits. Run tests + clang-format before pushing.

## Releasing
- `./bump-version.sh X.Y.Z` sets every packaging version offline; `./.packages/check-versions.sh` asserts they agree (also enforced in CI).
- The AUR source hash comes from the pushed tag — run `./.packages/refresh-aur-hash.sh` after `git push --tags` (or after creating the release), then commit the refreshed `PKGBUILD`/`.SRCINFO`.
