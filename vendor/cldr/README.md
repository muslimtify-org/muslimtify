# CLDR windowsZones data

`windowsZones.xml` is CLDR supplemental data mapping IANA timezone names to
Windows timezone key names. It is the source for
`src/platform/windows/windows_zones_generated.inc`, consumed on Windows by
`src/platform/windows/timezone.c`.

## Pinned version

- CLDR release: **release-46**
- Source: <https://raw.githubusercontent.com/unicode-org/cldr/release-46/common/supplemental/windowsZones.xml>

## Regenerating the table

1. Replace this file with a newer CLDR release:

   ```sh
   curl -fsSL -o vendor/cldr/windowsZones.xml \
     "https://raw.githubusercontent.com/unicode-org/cldr/<release>/common/supplemental/windowsZones.xml"
   ```

2. Run the generator (Windows PowerShell 5.1 or PowerShell 7):

   ```sh
   powershell -NoProfile -ExecutionPolicy Bypass -File scripts/gen-windows-zones.ps1
   ```

3. Reconcile `$overrides` in `scripts/gen-windows-zones.ps1` against the
   expectations pinned in `tests/test_location.c` (function
   `test_windows_zone_to_iana`). CLDR records some zones under legacy tz aliases
   (e.g. `Asia/Calcutta`, `Asia/Rangoon`) and picks `Asia/Bangkok` as the SE Asia
   canonical; the override map keeps the project's modern / Indonesia-first
   choices so existing tests stay green. If a newer CLDR changes a canonical the
   tests pin, add or adjust an override entry.

4. Build and run the `location` test on Windows; commit the regenerated `.inc`.

Regeneration is deterministic: rerunning the generator on the same XML produces a
byte-identical `.inc`.
