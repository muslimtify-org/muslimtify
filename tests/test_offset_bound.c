// Pins that a raw prayer time never drifts more than one day away from the
// requested date, which is spec goal 4 of the day-offset-visible plan.
//
// format_time_hm_day (src/core/display.c) expresses the day offset as a
// single character, '+' or '-'. That is only a complete encoding if the
// offset is always -1, 0 or +1. A raw field of struct PrayerTimes carries
// that offset as whole days folded into the decimal-hours value, so the
// complete set {-1, 0, +1} is exactly the open interval (-24, 48): below
// -24 the offset is two or more days early, at or above 48 it is two or
// more days late. This sweep asserts every finite field calculate_prayer_times
// produces stays inside that interval, across every calculation method, so a
// future method with a fajr angle deep enough to push a field outside it
// gets caught here instead of silently truncated by the marker character.
//
// The sweep is latitude -89 to 89 at 1 degree, every day of 2026, every
// method in CalcMethod. That is deliberately slow: measured at roughly
// 31.19 s built at -O1 (the CMakeLists.txt default, Release) and 33.08 s at
// -O0. Do not narrow it to make it fast, the coverage is the point. Only a
// summary is printed, not a line per iteration, or the run produces
// megabytes of output.

#include "prayertimes.h"

#include <math.h>
#include <stdio.h>

int main(void) {
  long violations = 0;
  long checked = 0;

  long first_day = mt_days_from_civil(2026, 1, 1);
  long last_day = mt_days_from_civil(2026, 12, 31);

  for (int lat_deg = -89; lat_deg <= 89; lat_deg++) {
    double latitude = (double)lat_deg;

    for (long day = first_day; day <= last_day; day++) {
      int y, m, d;
      mt_civil_from_days(day, &y, &m, &d);

      for (int method = 0; method < CALC_COUNT; method++) {
        const MethodParams *params = method_params_get((CalcMethod)method);

        struct PrayerTimes t = calculate_prayer_times(y, m, d, latitude, 0.0, 0.0, params);

        double fields[5] = {t.fajr, t.dhuhr, t.asr, t.maghrib, t.isha};

        for (int i = 0; i < 5; i++) {
          if (!isfinite(fields[i]))
            continue;

          checked++;
          if (!(fields[i] > -24.0 && fields[i] < 48.0))
            violations++;
        }
      }
    }
  }

  printf("Running offset bound sweep...\n");
  printf("Results: %ld field checked, %ld violation(s)\n", checked, violations);

  if (violations != 0) {
    printf("FAIL: %ld field(s) outside (-24, 48), which a single +/- marker cannot express\n",
           violations);
    return 1;
  }

  printf("PASS\n");
  return 0;
}
