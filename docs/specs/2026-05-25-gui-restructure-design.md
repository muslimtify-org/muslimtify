---
title: GUI restructure — stb-style single-TU → scalable multi-TU layout
date: 2026-05-25
status: draft
---

# GUI restructure — Design

## Problem

The GUI under `src/gui/` is built stb-style: `src/muslimtify_gui.c` `#define`s four
`*_IMPLEMENTATION` macros and pulls every component's implementation into a single
translation unit. The stb single-header pattern exists to ship one drop-in `.h`; none of
that applies to application code that always builds in one place. The costs without the
benefits:

- **Fragile macro coordination.** `dashboard_content.h` internally `#define`s
  `MUSLIMTIFY_TOPBAR_IMPLEMENTATION` before including `topbar.h`, while `muslimtify_gui.c`
  owns the other three macros. Adding a component means reasoning about which TU defines
  whose implementation macro and in what order.
- **`static` data in headers.** `dailyScheduleItems` (`dashboard_content.h`) and
  `NAVIGATION_ITEMS` (`navigation.h`) are `static` in headers — correct only because exactly
  one TU includes them. A second includer silently gets its own copy.
- **No incremental compilation.** Any component edit recompiles the whole GUI.
- **Mutable globals defined in a header.** ~40 `extern Texture2D g_icon*` / `g_font*` live
  in `app_assets.h`, defined under a macro guard. Wrong home for mutable state.

## Goals

- Split the four implementation headers (`app_assets`, `topbar`, `navigation`,
  `dashboard_content`) into `.h` (declarations) + `.c` (implementation); remove every
  `*_IMPLEMENTATION` macro.
- Move `static` data tables out of headers into their `.c`.
- Replace the loose `g_icon*` / `g_font*` globals with a single `Assets` struct, owned by the
  app, loaded once, freed once, read through an accessor.
- Introduce a `gui` OBJECT library so each component is its own TU.
- Clean include paths: `src/gui` on the include path so includes read
  `components/topbar.h` / `themes/colors.h`, not `../themes/...`.
- Introduce an `app/` entry layer; reduce `muslimtify_gui.c` to `main()` only.
- Split the cards currently inside `dashboard_content` into their own component files.

## Non-goals

- No `screens/` layer or per-screen scaffolding (Option B). Add it later when a second screen
  (Prayers / Location / Notification / About) is actually built.
- No dependency injection / context-threaded components. ccompose components are immediate-mode
  global functions taking no context param; the `Assets` struct is reached via accessor, not
  passed down. Components remain non-isolated for unit testing — accepted.
- No behavior, layout, or visual change. Pure structural refactor.
- No changes to `colors.h` / `fonts.h` (pure macro headers, already correct).

## Constraints

- C99; `-Wall -Wextra -Wpedantic -Wshadow -Wformat=2` (GCC/Clang) / `/W4` (MSVC) must stay clean.
- ccompose's `-Wno-pedantic -Wno-missing-braces` and SYSTEM-include handling must carry over to
  the new object library.
- clang-format (LLVM-based, 2-space, 100-col) enforced by CI on all `src/gui/**` `.c`/`.h`.
- ccompose API: `int CC_LoadFont(path, base_size)` (font id), `Texture2D CC_LoadImage(path)`.
  Components are `void Name(void)` — no context parameter available.

## Approach

**Option A — flat split, struct + accessor.**

Target layout:

```
src/
  muslimtify_gui.c              # main() only — calls App_Run()
  gui/
    CMakeLists.txt              # adds muslimtify_gui_lib OBJECT library
    app/
      app.h / app.c             # App_Run(): window setup + frame loop + root Row("Container")
      assets.h / assets.c       # Assets struct, Assets_Load/Unload, App_Assets() accessor
    components/
      navigation.h / .c
      topbar.h / .c
      dashboard_content.h / .c  # composes the cards below
      prayer_card.h / .c
      calculation_profile_card.h / .c
      daily_schedule.h / .c
    themes/
      colors.h                  # unchanged
      fonts.h                   # unchanged
      asset_paths.h             # renamed from themes/assets.h (IC_* path macros)
    resources/                  # unchanged
```

Renames/moves: `themes/assets.h` → `themes/asset_paths.h` (it holds only `IC_*` path strings and
would otherwise clash with `app/assets.h`); the `App()` root-composition loop moves from
`muslimtify_gui.c` into `app/app.c`.

**Assets API:**

```c
// app/assets.h
typedef struct {
  int16_t fontManrope, fontBold, fontNormal;
  Texture2D currentTime, currentLocation, calculationProfile, modifySettings;
  Texture2D fajr, fajrActive, sunrise, sunriseActive, dhuhr, dhuhrActive,
            asr, asrActive, maghrib, maghribActive, isha, ishaActive;
  Texture2D dashboard, dashboardInactive, prayers, prayersInactive,
            location, locationInactive, notification, notificationInactive,
            about, aboutInactive;
  Texture2D expand, collapse, nextNotification;
} Assets;

void Assets_Load(void);          // fills the file-local instance (was AppAssetsLoad)
void Assets_Unload(void);
const Assets *App_Assets(void);  // accessor; components cache: const Assets *a = App_Assets();
```

The struct instance and accessor live in `app/assets.c` as one file-local static plus a getter.
Components reference `App_Assets()->fajr` instead of `g_iconFajr`.

**Data-table consequence:** `dailyScheduleItems[]` and `NAVIGATION_ITEMS[]` currently take
texture addresses (`.icon = &g_iconFajr`) in static initializers. Textures now live in a
runtime-filled struct, so static address-taking is impossible. The tables change to carry a small
**icon-id enum**; the component resolves the actual `Texture2D` from `App_Assets()` at render time
via a `switch`/index map. The tables become pure data with no pointers into mutable global state.

**Build / includes:** a new `muslimtify_gui_lib` OBJECT library compiles every `gui/**/*.c`; the
`muslimtify-gui` executable = `muslimtify_gui.c` + that object library. The existing
`-Wno-pedantic -Wno-missing-braces`, ccompose SYSTEM-include, and platform link logic move onto
the object library / executable as appropriate. `src/gui` is added to the include path.

## Alternatives considered

- **Option B — layered by feature/screen.** Add a `screens/dashboard/` layer anticipating the
  nav's other screens. Rejected as speculative: those screens don't exist yet (YAGNI). The
  `screens/` layer can be introduced cheaply when the second screen is built.
- **Option C — split files, keep globals.** Split `.h`/`.c` and drop the macros but keep asset
  globals (defined once in one `.c`). Rejected: under-delivers on the chosen Assets-struct goal.

## Testing

Structural refactor with no behavior change. Verification:

- `cmake --build build` is clean under `-Wall -Wextra -Wpedantic -Wshadow -Wformat=2`.
- `muslimtify-gui` launches and renders identically (manual smoke check).
- `clang-format --dry-run --Werror` passes on all new/moved `src/gui/**` `.c`/`.h`.
- Existing `ctest` suite still passes (unaffected — core library untouched).

No new unit tests: the GUI has none today and ccompose immediate-mode rendering is not unit-testable
in this setup.

## Open questions

None.
