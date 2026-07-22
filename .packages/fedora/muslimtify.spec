Name:           muslimtify
Version:        0.4.0
Release:        1%{?dist}
Summary:        An Islamic prayer time notification daemon for Linux
License:        MIT
URL:            https://github.com/rizukirr/muslimtify
Source0:        %{url}/archive/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.22
BuildRequires:  gcc
BuildRequires:  pkgconfig(libnotify)
BuildRequires:  pkgconfig(libcurl)

Requires:       libnotify
Requires:       libcurl

%description
A daily prayer notification daemon for Muslims on Windows and Linux, supporting 21 global standard calculation methods, all madzhab, all country.

%prep
%autosetup -n %{name}-%{version}

%build
%cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
%cmake_build

%install
%cmake_install

%post
if [ -n "$SUDO_USER" ] && [ "$SUDO_USER" != "root" ]; then
    uid=$(id -u "$SUDO_USER")
    echo "==> Setting up muslimtify daemon for $SUDO_USER..."
    XDG_RUNTIME_DIR="/run/user/$uid" \
        runuser -u "$SUDO_USER" -- muslimtify daemon install || true
else
    echo ""
    echo "  Run 'muslimtify daemon install' to enable the prayer time daemon."
    echo ""
fi

%files
%license LICENSE
%{_bindir}/muslimtify
%{_datadir}/muslimtify/adhan.mp3
%{_datadir}/icons/hicolor/128x128/apps/muslimtify.png
%{_datadir}/pixmaps/muslimtify.png
%{_prefix}/lib/systemd/user/muslimtify.service

%changelog
* Wed Jul 22 2026 Rizki Rakasiwi <rizkirr.xyz@gmail.com> - 0.4.0-1
- Add a runtime GPS location toggle, 'location gps on|off', with an automatic ipinfo fallback when no fix is available
- Read GPS on Linux from gpsd over a bounded loopback socket, dropping the libgps build dependency so one binary uses GPS whenever gpsd runs
- Read GPS on Windows via the WinRT Geolocator
- Report OS-denied location access as a warning instead of silently auto-disabling GPS
- Show the GPS source state in the location views
- Back the Windows timezone table with the full CLDR release-46 windowsZones mapping (449 entries)
- Fix prayer times being computed and displayed with a non-DST offset; the effective offset is now resolved for the target date
- Reject nonexistent IANA timezones at set-time and from ipinfo, falling back to the stored offset instead of a wrong one
- Fix prayer and reminder triggers being lost when a daemon cycle overruns; missed triggers now fire on catch-up
- Fix an out-of-bounds read in the JSON parser on a value ending in a trailing backslash
- Escape user-controlled strings written to the prayer cache, version the cache, and reject a malformed trigger object
- Bound 'show --date' to years 1..9999 and cap a range at 366 days, removing sscanf overflow undefined behaviour
- Validate Windows paths with wide APIs so validation matches the UTF-8 open path, and reject reparse points
- install.sh now builds as the invoking user instead of root and refuses a world-writable source tree
- Remove production-dead code (prayer_check_current, copy_wstring, country_table)
- Build and test Linux aarch64 and Windows ARM64 on every push, not only at release

* Fri Jul 17 2026 Rizki Rakasiwi <rizkirr.xyz@gmail.com> - 0.3.2-1
- add ARRAY_LEN helper and replace harcoded array lengths
- add cross-platform location auto-refresh with configurable interval

* Mon Jul 13 2026 Rizki Rakasiwi <rizkirr.xyz@gmail.com> - 0.3.1-1
- Rework 'show --date' to accept a single date or an inclusive range, with full support in the table, --json, and --headless output modes
- Validate input dates up front; reject malformed or out-of-order ranges with a clear error
- Fix 'show --next' rollover after the last prayer of the day; the wrapped prayer's time is recomputed for the next day rather than reusing today's clock time
- Rewrite the 'show --date' help text to cover single-date and range behaviour
- Ship the bundled adhan.mp3 inside the package

* Thu Jul 09 2026 Rizki Rakasiwi <rizkirr.xyz@gmail.com> - 0.3.0-1
- Redesign the command-line into show/location/notification/method/madzhab verbs with --json and --headless output (breaking change)
- Add per-prayer adhan audio with a bundled adhan and a cancellable notification
- Add 'offset <prayer|all> <minutes>' to shift individual prayer times
- Harden security: TLS-only location fetch, timezone validation before setenv, 0600 config and cache files, adhan path checks
- Remove the config and check commands; reset by deleting config.json

* Sun Jun 28 2026 Rizki Rakasiwi <rizkirr.xyz@gmail.com> - 0.2.5-1
- Fix the Windows silent install hanging on a network call, which failed winget validation on the 0.2.4 manifest
- 'daemon install' now only registers the scheduled task and returns immediately; location and method are auto-detected lazily on the task's first run
- Normalize mixed-slash paths during Windows install-time runtime-dependency resolution (CMake policy CMP0207)
- Remove the unused MUSLIMTIFY_CMD_DAEMON_WIN_TEST guard and dead includes from the Windows daemon command

* Sat Jun 27 2026 Rizki Rakasiwi <rizkirr.xyz@gmail.com> - 0.2.4-1
- Refactor the timer-driven daemon into a long-running systemd user service (Type=simple); drop the .timer
- Auto-remove the legacy systemd timer on install and upgrade
- Render the systemd unit from a single configure-time template
- Add unit tests for the service-unit builder and the loop sleep interval

* Thu Jun 18 2026 Rizki Rakasiwi <rizkirr.xyz@gmail.com> - 0.2.3-3
- Add Fedora 44 to the COPR build targets

* Wed May 27 2026 Rizki Rakasiwi <rizkirr.xyz@gmail.com> - 0.2.3-2
- fix config auto detection
* Tue May 26 2026 Rizki Rakasiwi <rizkirr.xyz@gmail.com> - 0.2.3-1
- Add unified 'config auto' command that auto-detects both location and calculation method
- Merge calculation method into the country master table; share config_auto_detect helper with daemon install
- Remove the separate 'location auto' and 'method auto' subcommands (replaced by 'config auto')
- Add --country=<code> flag to 'location set' with ISO 3166-1 alpha-2 validation
- Fix Windows CI by disabling optional curl dependencies
- Bump bundled curl to 8.20.0
- Silence Windows build warnings
- Align CLI help text with actual subcommand behavior

* Tue May 12 2026 Rizki Rakasiwi <rizkirr.xyz@gmail.com> - 0.2.2-1
- Fix timezone offsets by using system tzdb instead of hardcoded table; honors DST (closes #11)
- Add Win32 timezone implementation with CLDR-derived IANA-to-Windows mapping
- Add --timezone=<iana> override flag to 'location set'
- Add get_system_timezone helper for Linux and Windows
- Clear stale city/country and refresh timezone on 'location set'
- Stop auto-filling city from ipinfo; add --city=<name> flag
- Prefer Asia/Jakarta over Asia/Bangkok for SE Asia Standard Time
- Add 'sound' command with reminder/alarm/default presets
- Refactor source tree into src/{core,cli,platform}/ subdirectories
- CI: enforce non-empty test suites on Windows

* Fri Apr 03 2026 Rizki Rakasiwi <rizkirr.xyz@gmail.com> - 0.2.1-1
- Auto-detect location and method on daemon install
- Bug fixes and improvements

* Fri Mar 27 2026 Rizki Rakasiwi <rizkirr.xyz@gmail.com> - 0.2.0-1
- Add full Windows support for Muslimtify, including toast notifications with icon, Task Scheduler daemon, service helper, install/uninstall scripts, and Inno Setup installer for winget distribution
- Add 23 international prayer time calculation methods (MWL, Makkah, ISNA, Egypt, and more)
- Add muslimtify method command for calculation method management
- Add fajr_angle/isha_angle config fields for custom method parameters
- Display full method name in config output
- Add platform abstraction layer (Linux/Windows)
- Extract shared check cycle, decouple from CLI
- Route display, persistence, and system calls through platform layer
- Downgrade from C23 to C99 for MSVC compatibility
- Add comprehensive multi-method validation (~108 data points)
- Add unit tests for json.h, platform boundary, and Windows components

* Mon Mar 02 2026 Rizki Rakasiwi <rizkirr.xyz@gmail.com> - 0.1.4-1
- Release v0.1.4
