# Windows handover

Written 2026-10-07, when `main` was at `9d2a00d` (PR #98 merged). This note is for picking the work up on a Windows machine.

## Where main stands

Eleven PRs landed after the last release, `v0.4.3`. The version in `CMakeLists.txt` is still `0.4.3`.

| PR | What it did |
|---|---|
| #87 to #92 | The library layer: `include/muslimtify.h` over `src/core`, with read queries, settings, location detection, the check cycle, the CLI moved off core headers, and a replaceable log handler |
| #93 | Daemon management through the library, Linux only. Windows returns `MUSLIMTIFY_ERR_UNSUPPORTED` |
| #94 | Review fixes: JSON parser over-read, cycle cache handling, unit file quoting, CLI messages |
| #95 | Library process state: `GTimeZone` instead of `TZ` mutation, libcurl initialised by the library, `muslimtify_log_stderr`, the cycle moved to `include/muslimtify_cycle.h` |
| #96 | A stop signal cuts a playing adhan on Linux |
| #97 | GPS on means automatic location, hand-set coordinates turn GPS off, six-decimal config numbers |
| #98 | The CLI is self-contained in `src/cli/` with its own `CMakeLists.txt`, and the `layers` test enforces its includes |

Two things to say in the next release notes: `daemon status` now prints three `Installed`, `Enabled`, `Running` lines and exits 0 only when running, which breaks scripts that parsed systemd's text, and config files are written with six decimals. Bump the minor version for the first.

Nothing from #94 to #98 has been executed on Windows. CI compiled all of it with MSVC on x64 and ARM64 and ran the two Windows tests. That is the whole of the Windows evidence.

## Building on Windows

MSVC is required. The build stops with an explanation under any other compiler. These are CI's exact steps, from a Developer Command Prompt for Visual Studio:

```
cmake -B build -A x64 -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --config Release
ctest --test-dir build --output-on-failure -C Release --no-tests=error
```

Use `-A ARM64` on an ARM machine. The first configure fetches libcurl through FetchContent, so it needs the network once. Config lives at `%APPDATA%\muslimtify\config.json`, the cache at `%LOCALAPPDATA%\muslimtify`. The installer source is `.packages/winget/muslimtify.iss`, and `.\uninstall.ps1` removes an install.

Only two tests run on Windows today: `cmd_daemon_win` and `notification_win`. Everything else sits inside `if(NOT WIN32)` in `CMakeLists.txt`. Most of those tests need nothing but a temp directory, so enabling them on Windows is mostly a CMake change, with `HOME` replaced by `APPDATA` in their fixtures. The `layers` test is a POSIX shell script and stays Linux-only unless rewritten in CMake.

## First things to run on Windows

These are the behaviours that changed since `v0.4.3` and have never run on Windows. Use a throwaway `APPDATA` so the real config is untouched.

- `muslimtify version`, `show`, `show --json`: the library path end to end.
- `location set --lat=27.7 --long=85.3 --timezone=Asia/Kathmandu`, then open `config.json`: `timezone_offset` should read `5.750000`.
- `location set --lat=10` with no stored location: must refuse and exit 1.
- `location gps on` with no receiver: must refuse with a clear message, and `location gps` must say disabled.
- Edit `config.json` to `auto_detect: false` and `use_gps: true`, then `location gps`: must say disabled.
- `location set --auto`: one real lookup, which proves the library initialises libcurl itself now that `main.c` no longer does.
- `daemon install`, `daemon status`, `daemon uninstall`: the Task Scheduler path in `src/cli/cmd_daemon_win.c`. Check `schtasks /Query /TN muslimtify` between the steps. The exit code fix from #94 is in this file.
- `muslimtify-service.exe`: run it by hand once with a prayer due, to see a toast. It calls `muslimtify_run_cycle` and no longer calls libcurl itself.
- `notification stop` while an adhan plays: Windows stops it across processes through a named event. Linux cannot, so this is the one place Windows does more.
- A location refresh failure, offline: the cycle should retry only every ten minutes.

## What Windows lacks

1. **Daemon management through the library.** `platform_daemon_install`, `platform_daemon_uninstall` and `platform_daemon_status` in `src/platform/windows/daemon_win.c` return unsupported. The CLI still carries its own `schtasks` code in `src/cli/cmd_daemon_win.c`. A GUI on Windows cannot install or query the task through `muslimtify.h`. The fix is to move the Task Scheduler logic behind those three functions, make `cmd_daemon_win.c` as thin as `cmd_daemon.c`, and give `muslimtify_daemon_install` the same fixed search for the `muslimtify` program. The Linux design is in `docs/specs/2026-10-05-muslimtify-daemon-management-design.md`, with a test that fakes the service manager through `PATH` and a temp home. The Windows equivalent needs a way to fake `schtasks.exe`.
2. **Tests.** See above.
3. **Geolocation timeout.** When the 15 second poll deadline in `src/platform/windows/geolocation_win.c` fires, the running WinRT operation is released without `IAsyncInfo::Cancel`. Low severity, found in the #94 review, not fixed because it could not be compiled on Linux.

By design, not gaps: `notify_adhan_interrupt` is a no-op on Windows because the scheduled task gets no SIGTERM, and the check cycle runs once per task instead of in a loop.

## Next pieces, in order

1. **GUI beside the CLI**, on the `muslimtify-gui` branch. Merge `main` into it, which was conflict-free before #98 and needs rechecking. Move `gui/` to `src/gui/` in the same shape as `src/cli/`: sources, headers and `main.c` together, its own `CMakeLists.txt` with the `BUILD_GUI` option, the ccompose fetch and the font install, fonts under `assets/fonts/`. Add `src/gui` to the `layers` test in the top-level `CMakeLists.txt`. `gui_config.c` reads its one boolean with the core JSON parser and `string_util.h`, which the rule forbids, so it parses that boolean itself. The widgets stay placeholders. Wiring real prayer data is the piece after.
2. **Windows daemon management through the library.** Needs a Windows machine, which is the reason for the switch.
3. **Distribution**, shelved. The GUI is in-tree and links `muslimtify_core` directly, so it needs no installed library. A third-party back end does: a static `libmuslimtify` with `muslimtify.h` and a pkg-config file, leaving `muslimtify_cycle.h` uninstalled. Separate CLI and GUI packages are possible, with the GUI package depending on the CLI package because the service runs `muslimtify daemon run`.

## Working rules that held

- Never commit to `main`. Branch, PR, CI green, then merge.
- No `Co-Authored-By` or `Author` trailers, no generated-by footer. Subagents add them anyway, so check every commit and amend.
- No section banner comments in `src/`, `include/` or `tests/`.
- Never touch `vendor/prayertimes.h` or `docs/*METHOD*.md`. The one exception was one line in `KEMENAG_METHOD.md` on explicit instruction.
- `docs/specs/` and `docs/plans/` are gitignored. They exist only on the Linux machine. Copy them before switching if you want them, or rely on the PR bodies, which summarise each one.
- Check `CMAKE_BUILD_TYPE` in `CMakeCache.txt` before calling a build Release. A reconfigured `build/` directory passed as Release once.
- Any concretely stateable misbehaviour is a finding with a severity, never a soft risk beside "nothing open".
- Tests never touch the real service manager: on Linux, a temp `HOME` and a fake `systemctl` first on `PATH`. On Windows the equivalent is a throwaway `APPDATA` and never running `schtasks` against the real task from a test.

## Housekeeping left on origin

Merged branches still exist on the remote and can be deleted: `feat/muslimtify-library`, `feat/muslimtify-settings`, `feat/muslimtify-location`, `feat/muslimtify-cycle`, `feat/muslimtify-cli-off-core`, `feat/muslimtify-log-handler`, `feat/muslimtify-daemon`, `fix/review-findings`, `feat/library-process-state`, `feat/adhan-stop-on-signal`, `fix/gps-auto-detect-and-precision`, `refactor/cli-self-contained`. `fix/ppa-signing-key` predates this work. `muslimtify-gui` is live.

One nit is on record from #97: the README says hand-set coordinates are replaced "at the next refresh" when GPS is turned on without a fix. That refresh is the daemon's next cycle, within a minute.
