# Contributing to Muslimtify

Thanks for your interest in contributing! This guide will help you get started.

> **Using AI tools?** Read the [AI Usage Policy](AI_POLICY.md) first. Muslimtify
> calculates prayer times people rely on for worship, so AI-assisted
> contributions must be disclosed, fully understood, and reviewed by you.

## Development Setup

```bash
# Install dependencies (Debian/Ubuntu)
sudo apt install cmake pkg-config libnotify-dev libcurl4-openssl-dev

# Build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)

# Run tests
ctest --output-on-failure

# Run the binary
./bin/muslimtify
```

## Project Structure

```
src/
  muslimtify.c            # main() entry point
  json.h                  # JSON parser (header-only)
  string_util.h           # Small string helpers (header-only)
  version.h.in            # Version template (configured by CMake)
  cli/                    # CLI dispatcher + command handlers
    cli.c                 #   Top-level dispatch table
    cmd_show.c            #   show, check commands
    cmd_next.c            #   next command and sub-handlers
    cmd_config.c          #   config sub-commands
    cmd_location.c        #   location sub-commands
    cmd_prayer.c          #   enable, disable, list, reminder
    cmd_method.c          #   calculation method selection
    cmd_notification.c    #   notification settings
    cmd_sound.c           #   adhan sound settings
    cmd_daemon.c          #   systemd daemon management (Linux)
    cmd_daemon_win.c      #   scheduled-task daemon management (Windows)
  core/                   # Platform-agnostic logic
    config.c              #   JSON config load/save
    cache.c               #   Cached prayer-time storage
    location.c            #   IP geolocation
    country.c             #   Country/timezone lookup tables
    string_util.c         #   String helpers
    prayer_checker.c      #   Prayer time matching
    check_cycle.c         #   One-shot reminder check cycle
    daemon_loop.c         #   Long-running daemon scheduling loop (POSIX)
    audio.c               #   Adhan audio playback
    miniaudio.c           #   Vendored miniaudio backend
    display.c             #   Terminal output (tables, colors, JSON)
  platform/               # OS-specific implementations
    linux/                #   notification (libnotify), platform, timezone
    windows/              #   notification (WinRT), platform, timezone
include/                  # Public headers (prayertimes.h, config.h, etc.)
tests/                    # Test suites
docs/                     # Calculation method documentation
```

## Code Style

- C11 standard
- 2-space indentation
- Opening brace on same line
- Run `clang-format` before committing (config provided in `.clang-format`)

## Commit Messages

Follow this format:

```
type: short description

Optional longer explanation.
```

Types: `feat`, `fix`, `refactor`, `test`, `chore`, `docs`

Examples:
- `feat: add Muhammadiyah calculation method`
- `fix: handle midnight crossover in reminder times`
- `test: add display output tests`

## AI Usage

AI-assisted contributions are **discouraged but not forbidden**. Because
Muslimtify calculates prayer times people rely on for worship, reaching for an AI
tool means you take on *more* responsibility, not less. The full rules live in the
[AI Usage Policy](AI_POLICY.md) — read it before your first AI-assisted
contribution. In short:

- **Disclose it.** Name the tool (Claude, Copilot, Cursor, ChatGPT, …) and roughly
  how much of the work was AI-assisted, in your PR or issue description.
- **Understand every line.** If you can't explain what your change does and *why it
  is correct* — without the AI in front of you — don't submit it. This matters most
  for prayer-time calculation, timezones, method/madhab selection, and reminder
  scheduling.
- **Review and fix the output.** AI produces plausible-looking code that is often
  subtly wrong. Finding and fixing that is your job, not the maintainer's.
- **You own it.** "The AI wrote it" is never an excuse once you open the PR.
- **Never touch `prayertimes.h` or `docs/*METHOD*.md`** if you are not a maintainer
  (see [Prayer Calculation](#prayer-calculation)). Open an issue — not a PR — tagged
  `prayertimes.h`, with a human in the loop.
- **No AI-generated media** (images, audio, video, art). Text and code only, subject
  to the rules above. AI-assisted issues and discussions must be edited down by a
  human before submission.

## Pull Request Process

1. Fork the repo and create a branch from `main`
2. Make your changes
3. Ensure all tests pass: `ctest --output-on-failure`
4. Ensure no compiler warnings (the project uses `-Wall -Wextra -Wpedantic -Wshadow -Wformat=2`)
5. Run `clang-format -i` on changed files
6. Open a PR with a clear description of what and why
7. If you used AI tools, disclose it in the PR description (see [AI Usage](#ai-usage))

## Adding New Features

Source files are split across two OBJECT libraries in `CMakeLists.txt`:

- **`muslimtify_core`** — platform-agnostic logic (`src/core/`) plus the platform
  abstraction (`src/platform/{linux,windows}/`). Add core source files here.
- **`muslimtify_cli`** — the CLI dispatcher and command handlers (`src/cli/`).
  Add new command files here.

Guidelines:

- New source files go in `src/core/`, `src/cli/`, or `src/platform/<os>/`, with a
  matching public header in `include/`.
- Add the new `.c` file to the appropriate `add_library(... OBJECT ...)` block.
- For OS-specific code, follow the `$<IF:$<BOOL:${WIN32}>,...>` generator-expression
  pattern used for `notification`, `platform`, and `timezone`.
- New tests follow the existing pattern: `add_executable` + `muslimtify_set_target_defaults`
  + `add_test`, linking `muslimtify_core` (and `muslimtify_cli` for CLI tests), or
  compiling the unit's `.c` directly for small standalone tests (see `test_string_util`,
  `test_country`).
- Tests use simple pass/fail counters (no external framework).

## Platform Support

The project builds on both Linux and Windows. Linux uses libnotify and systemd;
Windows uses WinRT toast notifications and Scheduled Tasks. Keep platform-specific
code behind the `src/platform/` abstraction and `WIN32` CMake guards — shared logic
stays in `src/core/`.

## Prayer Calculation

> **Do not modify `prayertimes.h` if you are not a maintainer.** It holds the
> astronomical formulas that the rest of the project trusts, and a subtle mistake
> can make someone pray at the wrong time. If you find a bug or believe a
> calculation can be improved, **open an issue — do not send a pull request.** Tag
> it `prayertimes.h` so a maintainer picks it up with a human in the loop. The same
> rule applies to the method docs under `docs/*METHOD*.md`.

Calculation methods are documented under `docs/` (`KEMENAG_METHOD.md`,
`INTERNATIONAL_METHODS.md`, `METHOD_TOLERANCES.md`). If adding a new calculation
method, please include reference data for validation.

## Questions?

Open an issue if something is unclear.
