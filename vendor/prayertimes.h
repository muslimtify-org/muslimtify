/*
 * MIT License
 *
 * Copyright (c) 2025-2026 muslimtify-org
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* prayertimes.h -- v0.2.3 -- single-header C/C++ prayer-time calculation library
 *
 * The version above is this file's own. It is not the libmuslim release
 * tag, which is a calendar date such as 2026.08.18 and covers a snapshot
 * of several independently versioned headers. A difference between the
 * two is expected. Use this number for compatibility, the release tag to
 * pin a download.
 *
 * This library calculates daily prayer times for a date and location using a
 * selection of established calculation methods. It has no dependencies beyond
 * the C standard library and libm.
 *
 * In exactly one C or C++ source file, define PRAYERTIMES_IMPLEMENTATION before
 * including this header to create the implementation:
 *
 *     #define PRAYERTIMES_IMPLEMENTATION
 *     #include "prayertimes.h"
 *
 * In every other source file that uses the API, include the header normally:
 *
 *     #include "prayertimes.h"
 *
 * -----------------------------------------------------------------------
 * ACCURACY
 *
 * Maghrib is checked against hijri_find_sunset from hijri.h, which is
 * itself validated against JPL DE440, in tests/test_prayertimes_oracle.c.
 * That check covers a grid of latitudes -60 to +60 in steps of 10,
 * longitudes -120, 0 and +120, every day of 2025, 14235 points, all of
 * them usable, with the ihtiyat subtracted before comparing and the
 * refraction conventions paired as REFRACTION_CORRECTION 0.833 against
 * HIJRI_SUNSET_CONVENTION_ASTRONOMICAL, whose fields give
 * 0.5667 + 959.63/3600 = 0.83326. The measured maximum absolute
 * difference across that grid is 6.5966 seconds, at latitude -60,
 * longitude -120, on 2025-12-17, against a pinned bound of 15.0 seconds
 * at tests/test_prayertimes_oracle.c:195.
 *
 * That 6.5966 second figure is not a global accuracy claim. The seconds
 * grid covers |latitude| <= 60 only. Beyond that the two solvers diverge
 * sharply in the time domain, because near the polar circle the Sun
 * crosses the horizon at grazing incidence and a small altitude
 * difference becomes a large time difference. Measured with the current
 * code, maximum absolute difference by latitude:
 *
 *   lat +60   2.39 s     lat -60    6.60 s
 *   lat +66   9.58 s     lat -66  123.65 s
 *   lat +68  29.55 s     lat -68  104.44 s
 *   lat +70  33.37 s     lat -70   77.99 s
 *
 * The event solver bisects the true solar altitude between local noon and
 * solar midnight rather than iterating a closed form from a guess. That
 * tightened the twilight solved population from a mean of 0.0996 arcmin and
 * a max of 0.7095 to 0.0426 and 0.2312, and the polar check from 0.7511 to
 * 0.4680. No published-table comparison moved across any of this work, all
 * 702 lines byte-identical.
 *
 * Above 60 degrees the same oracle is applied in the angle domain
 * instead, which does not amplify at grazing incidence because nothing
 * is converted back into a time. At the instant this header reports for
 * maghrib, the DE440-validated solver is asked where the Sun is, and
 * that is compared against where the same solver puts the Sun at its own
 * sunset. Over |latitude| in {62, 66, 70, 75, 78}, longitudes -120, 0
 * and 120, every day of 2025 and both hemispheres, 7278 comparable
 * points agree to a mean of 0.1632 arcmin and a maximum of 0.4680
 * arcmin, against a pinned bound of 1.5 arcmin. A further 169 points
 * graze the sunset altitude and agree to 0.4556 arcmin, pinned at 1.5.
 * The 35 first and last days of the polar period are excluded, because
 * there the two solvers describe different events rather than
 * disagreeing about one.
 *
 * Fajr and Isha are asserted the same way, against the depression angle
 * each is defined by. hijri_sun_altitude returns a geometric altitude
 * with no refraction term, which is exactly how this header defines
 * them, so the two are directly comparable. Excluding days where the
 * fallback supplied the value, and polar days where the schedule is
 * solved at the reference latitude, the remainder splits by how far the
 * Sun passes the required depression. 17668 points that clear it by at
 * least half a degree agree to a mean of 0.0426 arcmin and a maximum of
 * 0.2312 arcmin, against a pinned bound of 1.0. The 160 points that only
 * graze it reach 0.1970 arcmin, pinned at 0.9.
 *
 * That grazing figure used to be 30.1964 arcmin, roughly 6 minutes, on
 * 186 points. It was never a precision figure. This header decided
 * whether an event exists from the declination at 0h UT and where it
 * falls from a refinement at the event, two questions answered from two
 * instants, and near the seasonal boundary they disagreed. 45 of those
 * 186 points were days the Sun never reaches the angle at all: at
 * latitude 70 on 2025-03-27 it reaches 16.9965 degrees against MWL's 17,
 * so the 30 arcmin was the distance to an event that was not there.
 * Those days now take the substitution. Issue #79.
 *
 * Existence is gated on both tests rather than on the solve alone,
 * because the published tables this header reproduces were computed with
 * the coarser one. At London on 2026-07-15 the Sun does reach 17 degrees,
 * at 00:49, and the published table gives the substitution at 23:25.
 * Whichever test declines wins. On grazing days that follows a published
 * convention rather than the criterion's own definition, which is the
 * right trade for this header and worth knowing about.
 *
 * Asr is covered only by the published-table fixtures. Refining it was
 * measured during this work and made results worse, so it was
 * deliberately left alone.
 *
 * The published-table suite tests/test_prayertimes.c reports 763 checks.
 * 702 of those compare a computed time against a published table at a
 * uniform tolerance of 2 minutes, with a residual distribution of 369
 * checks at 0 minutes, 326 at 1 and 7 at 2, which sums to the 702. That
 * distribution is unchanged by issue #79. The remaining 61 carry no
 * residual because they are not time comparisons:
 * 7 assert the test's own clock_diff_minutes helper, 8 assert the
 * civil-day converters, 15 assert the time formatters, 19 pin the
 * struct PrayerTimes field contract, including the per-method
 * high-latitude behaviour, the caller override for it and the asr
 * domain guard, 5 assert that the five prescribed times stay in order at
 * five locations across every method, and 7 pin the day where the Sun
 * never reaches the isha angle, so a time is reported as a substitution
 * rather than as a crossing.
 *
 * These counts fell from 940 and 895 when sunrise and dhuha were removed
 * from the struct in v0.2.0. The fixtures behind them were the sunrise
 * and dhuha columns of the same published tables, so the loss is of
 * coverage for two fields the library no longer returns, not of coverage
 * for the five it does.
 *
 * Computed times changed on 2026-08-17 by up to 2 minutes at high
 * latitudes. Before this change the Sun was evaluated once at 0h UT and
 * reused for events up to 20 hours later, producing a seasonal error that
 * was negative in spring and positive in autumn. Anyone comparing against
 * previously generated output should read this as a correction, not
 * drift. Equatorial results barely moved, measured at a mean absolute
 * difference of 0.616 before and 0.622 after over 357 Indonesian checks.
 *
 * prayertimes.h has no dependency on hijri.h. The coupling described
 * above exists only in tests/test_prayertimes_oracle.c, not in this
 * file, so it should not be read as the two headers being entangled.
 * -----------------------------------------------------------------------
 */

#ifndef PRAYERTIMES_H
#define PRAYERTIMES_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PRAYERTIMESDEF
#ifdef PRAYERTIMES_STATIC
#define PRAYERTIMESDEF static
#else
#define PRAYERTIMESDEF extern
#endif
#endif

#define _USE_MATH_DEFINES
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define DEG_TO_RAD (M_PI / 180.0)
#define RAD_TO_DEG (180.0 / M_PI)

#define JULIAN_EPOCH 2451545.0

#define SUN_MEAN_ANOMALY_OFFSET 357.529
#define SUN_MEAN_ANOMALY_RATE 0.98560028

#define SUN_MEAN_LONGITUDE_OFFSET 280.459
#define SUN_MEAN_LONGITUDE_RATE 0.98564736

#define SUN_ECCENTRICITY_AMPLITUDE1 1.915
#define SUN_ECCENTRICITY_AMPLITUDE2 0.020

#define OBLIQUITY_COEFF 23.439
#define OBLIQUITY_RATE 0.00000036

#define REFRACTION_CORRECTION 0.833 // for sunrise/sunset (deg)

/* ── Calculation-method catalogue ────────────────────────────────────── */

typedef enum {
  CALC_MWL,
  CALC_MAKKAH,
  CALC_ISNA,
  CALC_EGYPT,
  CALC_KARACHI,
  CALC_TURKEY,
  CALC_SINGAPORE,
  CALC_JAKIM,
  CALC_KEMENAG,
  CALC_FRANCE,
  CALC_RUSSIA,
  CALC_DUBAI,
  CALC_QATAR,
  CALC_KUWAIT,
  CALC_JORDAN,
  CALC_GULF,
  CALC_TUNISIA,
  CALC_ALGERIA,
  CALC_MOROCCO,
  CALC_PORTUGAL,
  CALC_MOONSIGHTING,
  CALC_CUSTOM,
  CALC_COUNT
} CalcMethod;

typedef enum {
  ASR_STANDARD = 1,
  ASR_HANAFI = 2,
} AsrSchool;

/**
 * What to do for fajr and isha when the Sun never reaches the required
 * depression angle, which happens every summer above roughly 48 degrees.
 *
 * This is a property of the calculation authority, not of the library. Most
 * authorities serve jurisdictions where the case never arises and publish no
 * rule at all, so most entries in the method table are HIGHLAT_ANGLE_BASED,
 * which is a computational convention rather than anyone's ruling. See
 * docs/research/2026-08-18-high-latitude-conventions.md.
 *
 * Every value except HIGHLAT_NEAREST_LATITUDE is defined in terms of the
 * interval between sunset and sunrise, so none of them can answer inside the
 * polar circle where that interval does not exist. MethodParams.high_lat_ref
 * covers that case separately.
 */
typedef enum {
  HIGHLAT_NONE,            /* no substitution, the time is NaN */
  HIGHLAT_MIDDLE_OF_NIGHT, /* half the night before sunrise, after sunset */
  HIGHLAT_ONE_SEVENTH,     /* one seventh of the night */
  HIGHLAT_ANGLE_BASED,     /* angle/60 of the night, praytimes.org */
  HIGHLAT_NEAREST_LATITUDE /* same fraction of the night as at high_lat_ref */
} HighLatMethod;

typedef enum {
  MIDNIGHT_STANDARD = 0,
} MidnightMode;

typedef struct {
  const char *name;
  double fajr_angle;
  double isha_angle;    /* 0 when isha uses interval instead */
  int isha_interval;    /* minutes after maghrib (0 = use angle) */
  int maghrib_interval; /* minutes after sunset (0 = at sunset) */
  int asr_shadow;       /* shadow factor: 1 = standard, 2 = Hanafi */
  MidnightMode midnight_mode;
  int ihtiyat; /* precautionary minutes added to each time */

  /* Fajr and isha substitution when the depression angle is not reached but
     a real night still exists. */
  HighLatMethod high_lat_method;

  /* Reference latitude used when there is no sunset or sunrise at all, which
     is the case inside the polar circle. Every rule above is measured in
     units of the night, so without a reference there is nothing to measure
     and the affected times are NaN. Set to 0 when the authority publishes no
     rule for this case, which is most of them.

     A caller who needs a time anyway can copy the table entry and set this
     themselves. That is a deliberate escape hatch: the library will not put
     a ruling in an authority's mouth, but it will not stand between a user
     and a prayer time either. The choice is then the caller's, and it is
     recorded in their code rather than misattributed to the authority.

         MethodParams mine = *method_params_get(CALC_RUSSIA);
         mine.high_lat_method = HIGHLAT_ANGLE_BASED;
         mine.high_lat_ref = 45.0;

     At Murmansk, 68.97 N, that takes 2025 from 102 days with a non-finite
     prescribed time to none. tests/test_prayertimes.c pins both numbers. */
  double high_lat_ref;
} MethodParams;

/**
 * The five prescribed prayer times, each as decimal hours in local time,
 * so 17.75 means 17:45.
 *
 * Sunrise and dhuha were removed in v0.2.0. Sunrise is not a prayer, it
 * is the end of the fajr window, and dhuha is a voluntary prayer carried
 * only by Indonesian timetables. Both are still computed internally,
 * because maghrib is sunset and every high-latitude substitution measures
 * the night between sunset and sunrise, but neither is part of the
 * contract.
 *
 * A field is normally in [0, 24), but it is not guaranteed to be, and callers
 * that do anything other than print it must handle two cases.
 *
 * Non-finite. Above roughly 66 degrees the Sun can fail to reach the altitude
 * an event is defined by, and the field is then NaN. Test with isfinite()
 * before use. This depends on the method: those carrying a high_lat_ref,
 * currently MWL and Moonsighting, resolve every field at every latitude,
 * and the other 20 do not.
 *
 * Outside [0, 24). The high-latitude fallback for fajr and isha can return a
 * value below 0 or at or above 24, meaning the event falls on the previous or
 * the next calendar day. This happens on 107 days a year at Reykjavik and 23
 * at Anchorage under MWL, neither of which produces a NaN, so the two cases
 * are independent.
 *
 * The double is the only thing that carries the day offset. Reducing a field
 * into [0, 24) before building a date or a timestamp therefore moves the event
 * silently onto the wrong day. Keep the whole value. format_time_hm() and
 * format_time_hms() handle both cases, but a clock string cannot express a
 * date, so they do not preserve the offset either.
 *
 * Whether this struct should carry the offset explicitly is open, see issue
 * #56. Until it is settled the raw value is the contract, and
 * tests/test_prayertimes.c pins it.
 */
struct PrayerTimes {
  double fajr;
  double dhuhr;
  double asr;
  double maghrib;
  double isha;
};

/**
 * Format a decimal-hours time (e.g. 5.5) into "HH:MM" in outBuffer.
 * Minutes are rounded up (Kemenag convention).
 *
 * A value outside [0, 24) is reduced onto the clock face first, so 25.075
 * renders as "01:05" and -0.104 as "23:54". The result names an hour of the
 * day and cannot say which day, so read the raw double if you need that.
 *
 * A non-finite value renders as "--:--". Six bytes are enough for either.
 */
PRAYERTIMESDEF void format_time_hm(double timeHours, char *outBuffer,
                                   size_t bufSize);

/**
 * Format a decimal-hours time (e.g. 5.5) into "HH:MM:SS" in outBuffer.
 * Seconds are rounded to nearest; use format_time_hm for the Kemenag
 * round-up-to-the-minute convention.
 *
 * The same reduction applies, so -0.104 renders as "23:53:46". A non-finite
 * value renders as "--:--:--". Nine bytes are enough for either.
 */
PRAYERTIMESDEF void format_time_hms(double timeHours, char *outBuffer,
                                    size_t bufSize);

/**
 * Look up the parameter set for a calculation method.
 * Returns: pointer to a static MethodParams, or NULL if method is out of range.
 */
PRAYERTIMESDEF const MethodParams *method_params_get(CalcMethod method);

/**
 * Map a method key string (e.g. "kemenag") to its CalcMethod.
 * Returns: the matching method, or CALC_CUSTOM if name is NULL or unknown.
 */
PRAYERTIMESDEF CalcMethod method_from_string(const char *name);

/**
 * Map a CalcMethod back to its key string (e.g. "kemenag").
 * Returns: the key, or "custom" if the method has no key.
 */
PRAYERTIMESDEF const char *method_to_string(CalcMethod method);

/**
 * Compute all prayer times for a date and location using the given method.
 * Returns times as decimal hours in local time; high-latitude fallbacks apply.
 */
PRAYERTIMESDEF struct PrayerTimes
calculate_prayer_times(int year, int month, int day, double latitude,
                       double longitude, double timezone,
                       const MethodParams *params);

/**
 * Days since 1970-01-01 for a civil (proleptic Gregorian) date, and its
 * inverse. Howard Hinnant's public-domain algorithm. Lets callers iterate a
 * date range or build a UTC instant without touching struct tm / mktime (no DST
 * hazards).
 */
static inline long mt_days_from_civil(int y, int m, int d) {
  y -= m <= 2;
  long era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = (unsigned)(y - era * 400);
  unsigned doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097L + (long)doe - 719468;
}

static inline void mt_civil_from_days(long z, int *y, int *m, int *d) {
  z += 719468;
  long era = (z >= 0 ? z : z - 146096) / 146097;
  unsigned doe = (unsigned)(z - era * 146097);
  unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  long yy = (long)yoe + era * 400;
  unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  unsigned mp = (5 * doy + 2) / 153;
  unsigned dd = doy - (153 * mp + 2) / 5 + 1;
  unsigned mm = (mp < 10) ? (mp + 3) : (mp - 9);
  *y = (int)(yy + (mm <= 2));
  *m = (int)mm;
  *d = (int)dd;
}

#ifdef __cplusplus
}
#endif

#endif

#ifdef PRAYERTIMES_IMPLEMENTATION

#include <math.h>
#include <stdbool.h>
#include <stdio.h>

// Helper: normalize angle to [0,360)
static double normalize_deg(double angle) {
  double a = fmod(angle, 360.0);
  if (a < 0)
    a += 360.0;
  return a;
}

/* ── Method parameter table ─────────────────────────────────────────── */

static const MethodParams METHOD_TABLE[CALC_COUNT] = {
    /* Angle-based, not the Fiqh Council rule, and the tables decided this
       rather than the decree. Switching the night-exists case to the Council's
       proportional measurement moved London 2026-06-15 and 2026-07-15 by 3, 15
       and 17 minutes against the published MWL table, which the angle-based
       rule reproduces to within 1 minute. Whoever publishes those tables
       computes them the angle-based way, so that is what "the MWL method"
       means in practice for latitudes that have a night at all.

       The decree still answers the case the published tables do not cover.
       Inside the polar circle there is no night to take a fraction of, so
       high_lat_ref carries the 45 degrees the Council proposes and is used
       only there. */
    [CALC_MWL] = {"Muslim World League", 18.0, 17.0, 0, 0, ASR_STANDARD,
                  MIDNIGHT_STANDARD, 0, HIGHLAT_ANGLE_BASED, 45.0},
    [CALC_MAKKAH] = {"Umm al-Qura, Makkah", 18.5, 0, 90, 0, ASR_STANDARD,
                     MIDNIGHT_STANDARD, 0, HIGHLAT_ANGLE_BASED, 0.0},
    [CALC_ISNA] = {"ISNA", 15.0, 15.0, 0, 0, ASR_STANDARD, MIDNIGHT_STANDARD, 0,
                   HIGHLAT_ANGLE_BASED, 0.0},
    [CALC_EGYPT] = {"Egyptian General Authority", 19.5, 17.5, 0, 0,
                    ASR_STANDARD, MIDNIGHT_STANDARD, 0, HIGHLAT_ANGLE_BASED,
                    0.0},
    [CALC_KARACHI] = {"Univ. Islamic Sciences, Karachi", 18.0, 18.0, 0, 0,
                      ASR_STANDARD, MIDNIGHT_STANDARD, 0, HIGHLAT_ANGLE_BASED,
                      0.0},
    [CALC_TURKEY] = {"Diyanet, Turkey", 18.0, 17.0, 0, 0, ASR_STANDARD,
                     MIDNIGHT_STANDARD, 0, HIGHLAT_ANGLE_BASED, 0.0},
    [CALC_SINGAPORE] = {"MUIS, Singapore", 20.0, 18.0, 0, 0, ASR_STANDARD,
                        MIDNIGHT_STANDARD, 0, HIGHLAT_ANGLE_BASED, 0.0},
    [CALC_JAKIM] = {"JAKIM, Malaysia", 20.0, 18.0, 0, 0, ASR_STANDARD,
                    MIDNIGHT_STANDARD, 0, HIGHLAT_ANGLE_BASED, 0.0},
    [CALC_KEMENAG] = {"KEMENAG, Indonesia", 20.0, 18.0, 0, 0, ASR_STANDARD,
                      MIDNIGHT_STANDARD, 2, HIGHLAT_ANGLE_BASED, 0.0},
    [CALC_FRANCE] = {"UOIF, France", 12.0, 12.0, 0, 0, ASR_STANDARD,
                     MIDNIGHT_STANDARD, 0, HIGHLAT_ANGLE_BASED, 0.0},
    [CALC_RUSSIA] = {"Spiritual Admin., Russia", 16.0, 15.0, 0, 0, ASR_STANDARD,
                     MIDNIGHT_STANDARD, 0, HIGHLAT_ANGLE_BASED, 0.0},
    [CALC_DUBAI] = {"GAIAE, Dubai", 18.2, 18.2, 0, 0, ASR_STANDARD,
                    MIDNIGHT_STANDARD, 0, HIGHLAT_ANGLE_BASED, 0.0},
    [CALC_QATAR] = {"Min. of Awqaf, Qatar", 18.0, 0, 90, 0, ASR_STANDARD,
                    MIDNIGHT_STANDARD, 0, HIGHLAT_ANGLE_BASED, 0.0},
    [CALC_KUWAIT] = {"Min. of Awqaf, Kuwait", 18.0, 17.5, 0, 0, ASR_STANDARD,
                     MIDNIGHT_STANDARD, 0, HIGHLAT_ANGLE_BASED, 0.0},
    [CALC_JORDAN] = {"Min. of Awqaf, Jordan", 18.0, 18.0, 0, 0, ASR_STANDARD,
                     MIDNIGHT_STANDARD, 5, HIGHLAT_ANGLE_BASED, 0.0},
    [CALC_GULF] = {"Gulf Region", 19.5, 0, 90, 0, ASR_STANDARD,
                   MIDNIGHT_STANDARD, 0, HIGHLAT_ANGLE_BASED, 0.0},
    [CALC_TUNISIA] = {"Min. of Religious Affairs, Tunisia", 18.0, 18.0, 0, 0,
                      ASR_STANDARD, MIDNIGHT_STANDARD, 0, HIGHLAT_ANGLE_BASED,
                      0.0},
    [CALC_ALGERIA] = {"Min. of Religious Affairs, Algeria", 18.0, 17.0, 0, 0,
                      ASR_STANDARD, MIDNIGHT_STANDARD, 0, HIGHLAT_ANGLE_BASED,
                      0.0},
    [CALC_MOROCCO] = {"Min. of Habous, Morocco", 19.0, 17.0, 0, 0, ASR_STANDARD,
                      MIDNIGHT_STANDARD, 0, HIGHLAT_ANGLE_BASED, 0.0},
    [CALC_PORTUGAL] = {"Comunidade Islamica de Lisboa", 18.0, 0, 77, 3,
                       ASR_STANDARD, MIDNIGHT_STANDARD, 0, HIGHLAT_ANGLE_BASED,
                       0.0},
    /* moonsighting.com states its formulae are good to 55 degrees, applies the
       one-seventh rule between 55 and 60, and above 60 slides the calculation
       down to 60, which is the reference latitude here. */
    [CALC_MOONSIGHTING] = {"Moonsighting Committee", 18.0, 18.0, 0, 3,
                           ASR_STANDARD, MIDNIGHT_STANDARD, 0,
                           HIGHLAT_ONE_SEVENTH, 60.0},
    [CALC_CUSTOM] = {"Custom", 18.0, 17.0, 0, 0, ASR_STANDARD,
                     MIDNIGHT_STANDARD, 0, HIGHLAT_ANGLE_BASED, 0.0},
};

PRAYERTIMESDEF const MethodParams *method_params_get(CalcMethod method) {
  if (method < 0 || method >= CALC_COUNT)
    return NULL;
  return &METHOD_TABLE[method];
}

/* String key ↔ enum mapping */

typedef struct {
  const char *key;
  CalcMethod method;
} MethodKeyEntry;

static const MethodKeyEntry METHOD_KEYS[] = {
    {"mwl", CALC_MWL},
    {"makkah", CALC_MAKKAH},
    {"isna", CALC_ISNA},
    {"egypt", CALC_EGYPT},
    {"karachi", CALC_KARACHI},
    {"turkey", CALC_TURKEY},
    {"singapore", CALC_SINGAPORE},
    {"jakim", CALC_JAKIM},
    {"kemenag", CALC_KEMENAG},
    {"france", CALC_FRANCE},
    {"russia", CALC_RUSSIA},
    {"dubai", CALC_DUBAI},
    {"qatar", CALC_QATAR},
    {"kuwait", CALC_KUWAIT},
    {"jordan", CALC_JORDAN},
    {"gulf", CALC_GULF},
    {"tunisia", CALC_TUNISIA},
    {"algeria", CALC_ALGERIA},
    {"morocco", CALC_MOROCCO},
    {"portugal", CALC_PORTUGAL},
    {"moonsighting", CALC_MOONSIGHTING},
    {"custom", CALC_CUSTOM},
};

/* ASCII-only, locale-independent case-insensitive compare.
 *
 * strcasecmp() is POSIX, not C, so it is absent under a strict -std=c11 build
 * and spelled _stricmp() on MSVC. tolower() from <ctype.h> is the usual
 * workaround but carries two traps: it is undefined for negative char values,
 * and it is locale-dependent -- in a Turkish locale 'I' does not fold to 'i',
 * so "ISNA" would stop matching "isna".
 *
 * METHOD_KEYS are fixed ASCII identifiers, so folding the ASCII range
 * explicitly is both correct and portable, and needs no extra header. */
static int pt__ascii_casecmp(const char *a, const char *b) {
  for (;; a++, b++) {
    unsigned char ca = (unsigned char)*a;
    unsigned char cb = (unsigned char)*b;
    if (ca >= 'A' && ca <= 'Z')
      ca = (unsigned char)(ca - 'A' + 'a');
    if (cb >= 'A' && cb <= 'Z')
      cb = (unsigned char)(cb - 'A' + 'a');
    if (ca != cb)
      return (int)ca - (int)cb;
    if (ca == '\0')
      return 0;
  }
}

PRAYERTIMESDEF CalcMethod method_from_string(const char *name) {
  if (!name)
    return CALC_CUSTOM;
  size_t count = sizeof(METHOD_KEYS) / sizeof(METHOD_KEYS[0]);
  for (size_t i = 0; i < count; i++) {
    if (pt__ascii_casecmp(name, METHOD_KEYS[i].key) == 0)
      return METHOD_KEYS[i].method;
  }
  return CALC_CUSTOM;
}

PRAYERTIMESDEF const char *method_to_string(CalcMethod method) {
  for (size_t i = 0; i < sizeof(METHOD_KEYS) / sizeof(METHOD_KEYS[0]); i++) {
    if (METHOD_KEYS[i].method == method)
      return METHOD_KEYS[i].key;
  }
  return "custom";
}

// Calculate Julian Day from a calendar date (simplified)
static double julian_day(int year, int month, int day) {
  if (month <= 2) {
    year -= 1;
    month += 12;
  }
  int A = year / 100;
  int B = 2 - A + (A / 4);
  double jd = floor(365.25 * (year + 4716)) + floor(30.6001 * (month + 1)) +
              day + B - 1524.5;
  return jd;
}

// Calculate solar declination and equation of time
static void sun_position(double jd, double *decl, double *eqt) {
  double D = jd - JULIAN_EPOCH;

  double g = normalize_deg(SUN_MEAN_ANOMALY_OFFSET + SUN_MEAN_ANOMALY_RATE * D);
  double q =
      normalize_deg(SUN_MEAN_LONGITUDE_OFFSET + SUN_MEAN_LONGITUDE_RATE * D);

  double L =
      normalize_deg(q + SUN_ECCENTRICITY_AMPLITUDE1 * sin(g * DEG_TO_RAD) +
                    SUN_ECCENTRICITY_AMPLITUDE2 * sin(2 * g * DEG_TO_RAD));

  double e = OBLIQUITY_COEFF - OBLIQUITY_RATE * D;

  double RA =
      atan2(cos(e * DEG_TO_RAD) * sin(L * DEG_TO_RAD), cos(L * DEG_TO_RAD)) *
      RAD_TO_DEG;
  RA = normalize_deg(RA);

  // Normalize difference to [-180, 180] to handle wrap-around near 0/360
  // boundary
  double diff = fmod(q - RA + 180.0, 360.0);
  if (diff < 0)
    diff += 360.0;
  diff -= 180.0;
  *eqt = diff / 15.0;
  *decl = asin(sin(e * DEG_TO_RAD) * sin(L * DEG_TO_RAD)) * RAD_TO_DEG;
}

// Compute the time difference from solar noon for a given sun altitude angle
// For angles below horizon (Fajr, Sunrise, Maghrib, Isha): pass positive angle
// For angles above horizon (Asr): pass negative angle to indicate positive
// altitude
static double hour_angle(double lat, double decl, double angle) {
  double lat_rad = lat * DEG_TO_RAD;
  double decl_rad = decl * DEG_TO_RAD;
  double angle_rad = angle * DEG_TO_RAD;

  // Formula: cos(H) = [sin(h) - sin(φ) × sin(δ)] / [cos(φ) × cos(δ)]
  // For below-horizon events: h is negative, so we use -sin(angle) where angle
  // is positive For above-horizon events: h is positive, so we use sin(angle)
  // directly
  double numerator = -sin(angle_rad) - sin(lat_rad) * sin(decl_rad);
  double denominator = cos(lat_rad) * cos(decl_rad);
  double ha = acos(numerator / denominator);
  return ha * RAD_TO_DEG / 15.0; // convert from degrees to hours
}

// Safe version that checks cos_ha bounds for high-latitude locations
static double hour_angle_safe(double lat, double decl, double angle,
                              bool *failed) {
  double lat_rad = lat * DEG_TO_RAD;
  double decl_rad = decl * DEG_TO_RAD;
  double angle_rad = angle * DEG_TO_RAD;

  double numerator = -sin(angle_rad) - sin(lat_rad) * sin(decl_rad);
  double denominator = cos(lat_rad) * cos(decl_rad);
  double cos_ha = numerator / denominator;

  if (cos_ha < -1.0 || cos_ha > 1.0) {
    *failed = true;
    return 0.0;
  }

  *failed = false;
  double ha = acos(cos_ha);
  return ha * RAD_TO_DEG / 15.0;
}

/* True altitude of the Sun at local time t, with the Sun's position taken at
   t rather than at 0h UT. This is the quantity every depression-angle event
   is actually defined by. */
static double solar_altitude(double jd, double lat, double lon, double tz,
                             double t) {
  double decl, eqt;
  sun_position(jd + (t - tz) / 24.0, &decl, &eqt);
  double noon = 12.0 + tz - (lon / 15.0) - eqt;
  double H = 15.0 * (t - noon) * DEG_TO_RAD;
  double phi = lat * DEG_TO_RAD;
  double dec = decl * DEG_TO_RAD;
  return asin(sin(phi) * sin(dec) + cos(phi) * cos(dec) * cos(H)) * RAD_TO_DEG;
}

/* Solve solar_altitude(t) = -angle on the morning branch (sign -1) or the
   evening branch (sign +1). Returns NAN when the Sun never reaches that
   depression, which is the honest answer and the signal the high-latitude
   fallback already reads.

   Altitude falls monotonically from its maximum at local noon to its minimum
   at solar midnight, so one sign change across the bracket means exactly one
   root and bisection cannot pick the wrong one.

   vibekit: solar midnight is taken as noon + 12 rather than solved. The
   equation of time drifts under a second across a day, so the true minimum
   sits within about half a second of that, and the bracket's far endpoint is
   wrong by the same amount. The only day it can change is one whose deepest
   depression falls within half a second of the threshold, where the verdict
   is a coin toss on any method. Upgrade path: solve for the instant where the
   hour angle reaches 180 degrees, which costs another bracketed search per
   event and buys nothing measurable at the tolerances above.

   This replaces an iteration of the closed-form hour angle from a guess.
   Near the seasonal boundary that formula stops having a root at the refined
   instant even when the event happens, so the iteration returned the last
   value that solved. See issue #79. */
static double solve_event(double jd, double lat, double lon, double tz,
                          double angle, double sign) {
  double decl, eqt;
  sun_position(jd, &decl, &eqt);
  double noon = 12.0 + tz - (lon / 15.0) - eqt;
  double midnight = noon + sign * 12.0;
  double target = -angle;

  if (solar_altitude(jd, lat, lon, tz, midnight) > target)
    return NAN;
  if (solar_altitude(jd, lat, lon, tz, noon) <= target)
    return NAN;

  double lo = noon, hi = midnight;
  for (int i = 0; i < 60; i++) {
    double mid = 0.5 * (lo + hi);
    if (fabs(hi - lo) < 1.0e-7) /* 0.36 ms, far inside any reported minute */
      break;
    if (solar_altitude(jd, lat, lon, tz, mid) > target)
      lo = mid;
    else
      hi = mid;
  }
  return 0.5 * (lo + hi);
}

/* Reduce an hour value onto the 24-hour clock face before it is decomposed
   into fields. A clock time is modular, so a value that has run past midnight
   or fallen before it still names an hour of some day, and rendering it must
   not produce a negative field. Casting to int truncates toward zero, so
   without this a value such as -0.104 decomposes to hours 0 and minutes -6
   and prints as "00:-6".

   This deliberately does not tell the caller that the day rolled over. The
   raw double in struct PrayerTimes still carries that, and a caller building
   a date-time must read it there. See issue #56. */
static double normalize_clock_hours(double t) {
  t = fmod(t, 24.0);
  if (t < 0.0)
    t += 24.0;
  return t;
}

/* Rendered in place of a time that does not exist, so that a non-finite input
   cannot reach the int casts below, where it would be undefined behaviour.
   Before this guard, format_time_hm(NAN) printed "-8:-2147483648". */
#define TIME_UNAVAILABLE_HM "--:--"
#define TIME_UNAVAILABLE_HMS "--:--:--"

// Format time (double hours) into "HH:MM"
PRAYERTIMESDEF void format_time_hm(double timeHours, char *outBuffer,
                                   size_t bufSize) {
  if (!isfinite(timeHours)) {
    snprintf(outBuffer, bufSize, TIME_UNAVAILABLE_HM);
    return;
  }
  timeHours = normalize_clock_hours(timeHours);
  int hours = (int)timeHours;
  double fraction = timeHours - hours;
  int minutes = (int)ceil(fraction * 60.0); // Always round up (Kemenag method)

  if (minutes >= 60) {
    hours += 1;
    minutes -= 60;
  }

  hours %= 24;

  snprintf(outBuffer, bufSize, "%02d:%02d", hours, minutes);
}

// Format time into "HH:MM:SS"
PRAYERTIMESDEF void format_time_hms(double timeHours, char *outBuffer,
                                    size_t bufSize) {
  if (!isfinite(timeHours)) {
    snprintf(outBuffer, bufSize, TIME_UNAVAILABLE_HMS);
    return;
  }
  timeHours = normalize_clock_hours(timeHours);
  int hours = (int)timeHours;
  double fraction = timeHours - hours;
  int totalSeconds = (int)(fraction * 3600.0 + 0.5);

  int minutes = totalSeconds / 60;
  int seconds = totalSeconds % 60;

  if (minutes >= 60) {
    hours += minutes / 60;
    minutes %= 60;
  }

  hours %= 24;

  snprintf(outBuffer, bufSize, "%02d:%02d:%02d", hours, minutes, seconds);
}

/* Solve an event at a reference latitude on the same meridian and day.

   Declination and the equation of time do not depend on latitude, so the
   reference shares this location's solar noon exactly. That is what makes the
   substitution cheap and also what makes it faithful: transplanting the
   reference schedule about a shared noon is the decree's "divide the 24 hours
   the same way" without any further arithmetic.

   Returns NaN when even the reference cannot solve the event. */
static double reference_event(double ref_lat, double decl, double noon,
                              double angle, double sign) {
  return noon + sign * hour_angle(ref_lat, decl, angle);
}

/* Substitute a fajr or isha time that the depression angle could not reach.
   sign is -1 for fajr, which precedes sunrise, and +1 for isha, which follows
   sunset.

   Two cases, and they are not the same question. When a real night exists the
   method's own rule applies, measured in units of that night. When there is no
   sunset or sunrise at all, every such rule is undefined, because the unit it
   measures in does not exist, and only a reference latitude can answer. */
static double high_lat_substitute(const MethodParams *params, double decl,
                                  double noon, double sunrise, double evening,
                                  double night, double angle, double sign) {
  if (isnan(night)) {
    if (params->high_lat_ref <= 0.0)
      return NAN;
    return reference_event(params->high_lat_ref, decl, noon, angle, sign);
  }

  switch (params->high_lat_method) {
  case HIGHLAT_MIDDLE_OF_NIGHT:
    return sign < 0 ? sunrise - night / 2.0 : evening + night / 2.0;
  case HIGHLAT_ONE_SEVENTH:
    return sign < 0 ? sunrise - night / 7.0 : evening + night / 7.0;
  case HIGHLAT_ANGLE_BASED:
    return sign < 0 ? sunrise - (angle / 60.0) * night
                    : evening + (angle / 60.0) * night;
  case HIGHLAT_NEAREST_LATITUDE: {
    /* Proportional measurement: the event takes the same share of this night
       that it takes of the night at the reference latitude. */
    double ref = params->high_lat_ref;
    if (ref <= 0.0)
      return NAN;
    double r_rise =
        reference_event(ref, decl, noon, REFRACTION_CORRECTION, -1.0);
    double r_set = reference_event(ref, decl, noon, REFRACTION_CORRECTION, 1.0);
    double r_event = reference_event(ref, decl, noon, angle, sign);
    double r_night = (24.0 - r_set) + r_rise;
    if (isnan(r_night) || isnan(r_event) || r_night <= 0.0)
      return NAN;
    double frac = sign < 0 ? ((r_event + 24.0) - r_set) / r_night
                           : (r_event - r_set) / r_night;
    return fmod(evening + frac * night + 48.0, 24.0);
  }
  case HIGHLAT_NONE:
  default:
    return NAN;
  }
}

PRAYERTIMESDEF struct PrayerTimes
calculate_prayer_times(int year, int month, int day, double latitude,
                       double longitude, double timezone,
                       const MethodParams *params) {
  double jd = julian_day(year, month, day);
  double decl, eqt;
  sun_position(jd, &decl, &eqt);

  double noon = 12.0 + timezone - (longitude / 15.0) - eqt;

  /* Sunrise & sunset (always use refraction correction).

     Two tests again, as for fajr and isha below. The 0h UT hour angle is the
     coarse one, and solve_event asks at the event's own instant. At
     Longyearbyen on 2025-04-18 the first says the Sun sets and the second
     says its lowest point of the day is -0.6082 degrees, above the -0.833
     that defines sunset, so it does not. That is the first day of the
     midnight sun and the library used to report a maghrib for it. */
  double ha_sunrise = hour_angle(latitude, decl, REFRACTION_CORRECTION);
  double sunrise = NAN;
  double sunset = NAN;
  if (!isnan(ha_sunrise)) {
    sunrise = solve_event(jd, latitude, longitude, timezone,
                          REFRACTION_CORRECTION, -1.0);
    sunset = solve_event(jd, latitude, longitude, timezone,
                         REFRACTION_CORRECTION, +1.0);
  }

  /* Inside the polar circle the Sun does not cross the horizon, so there is no
     sunrise or sunset to refine and no night to measure a substitution
     against. A method whose authority names a reference latitude borrows that
     latitude's day. One that does not leaves these NaN, which is the honest
     answer when nothing has been published for the case.

     When the reference latitude is used it has to solve the whole day, not
     just sunrise and sunset. Borrowing those two while leaving fajr, isha and
     asr at the true latitude puts two different places in one schedule, and
     the result is not ordered: at 78.22 N the Sun can fail to reach -0.833
     while still crossing -17, so maghrib comes from 45 N and isha from 78 N
     and isha lands first. solve_lat carries that choice to every hour angle
     below. */
  int polar = 0;
  double solve_lat = latitude;
  if (isnan(sunrise) || isnan(sunset)) {
    if (params->high_lat_ref > 0.0) {
      polar = 1;
      solve_lat = params->high_lat_ref;
      sunrise = reference_event(solve_lat, decl, noon,
                                REFRACTION_CORRECTION, -1.0);
      sunset = reference_event(solve_lat, decl, noon,
                               REFRACTION_CORRECTION, 1.0);
    }
  }

  /* Night duration for high-latitude fallback */
  double night = (24.0 - sunset) + sunrise;

  /* Fajr. Two tests must agree that the event happens before a time is
     reported for it.

     hour_angle_safe asks at 0h UT, which is the test the published sources
     this library reproduces also use. solve_event asks at the event's own
     instant. Near the seasonal boundary they disagree, and either saying no
     sends the day to the substitution.

     Trusting only the 0h UT test reported an isha that does not occur: at
     latitude 70 on 2025-03-27 the Sun reaches 16.9965 degrees of depression
     against MWL's 17. Trusting only the instant test broke published-table
     agreement the other way: at London on 2026-07-15 the Sun does reach 17
     degrees, at 00:49, while the published table gives the angle-based
     substitution at 23:25. Deferring to whichever test declines is what keeps
     both right. See issue #79. */
  double fajr;
  if (polar) {
    /* A borrowed day is solved at the reference latitude and left alone,
       because re-solving at the true location would undo the transplant. */
    bool fajr_failed = false;
    double ha_fajr =
        hour_angle_safe(solve_lat, decl, params->fajr_angle, &fajr_failed);
    fajr = fajr_failed ? NAN : noon - ha_fajr;
  } else {
    bool fajr_failed = false;
    /* Called for the flag, not the angle: solve_event supplies the
       instant. */
    (void)hour_angle_safe(latitude, decl, params->fajr_angle, &fajr_failed);
    fajr = fajr_failed ? NAN
                       : solve_event(jd, latitude, longitude, timezone,
                                     params->fajr_angle, -1.0);
  }
  if (isnan(fajr)) {
    fajr = high_lat_substitute(params, decl, noon, sunrise, sunset, night,
                               params->fajr_angle, -1.0);
  }

  /* Maghrib */
  double maghrib = sunset;
  if (params->maghrib_interval > 0) {
    maghrib = sunset + (double)params->maghrib_interval / 60.0;
  }

  /* Isha */
  double isha;
  if (params->isha_angle > 0.0) {
    if (polar) {
      bool isha_failed = false;
      double ha_isha =
          hour_angle_safe(solve_lat, decl, params->isha_angle, &isha_failed);
      isha = isha_failed ? NAN : noon + ha_isha;
    } else {
      bool isha_failed = false;
      /* Called for the flag, not the angle: solve_event supplies the
       instant. */
    (void)hour_angle_safe(latitude, decl, params->isha_angle, &isha_failed);
      isha = isha_failed ? NAN
                         : solve_event(jd, latitude, longitude, timezone,
                                       params->isha_angle, +1.0);
    }
    if (isnan(isha)) {
      isha = high_lat_substitute(params, decl, noon, sunrise, maghrib, night,
                                 params->isha_angle, 1.0);
    }
  } else {
    /* Interval-based (e.g. Makkah 90 min after maghrib) */
    isha = maghrib + (double)params->isha_interval / 60.0;
  }

  /* Asr. Solved at solve_lat, and not only for consistency: asr is defined by
     the length of a shadow, so it needs the Sun above the horizon.

     The Sun's greatest altitude on a day is 90 - fabs(lat - decl), so a
     separation of 90 degrees or more means it never rises and no shadow of
     any ratio is cast. The formula does not say so. Its tangent turns
     negative past 90 and it returns a negative shadow altitude, which
     hour_angle then solves into a plausible looking time. At 78.22 N on
     2026-01-01 that altitude is -13.922 degrees against a peak Sun altitude
     of -11.235, and the result was reported as 14:47.

     That is the one field that used to survive an isfinite() check on a day
     where fajr, maghrib and isha were all correctly unavailable, so a caller
     filtering per field would show it. A wrong time is worse than no time.
     Where a reference latitude is in use solve_lat is that latitude, its
     separation is always under 90, and a real shadow exists. */
  double asr;
  if (fabs(solve_lat - decl) >= 90.0) {
    asr = NAN;
  } else {
    double asr_angle = atan(1.0 / ((double)params->asr_shadow +
                                   tan(fabs(solve_lat - decl) * DEG_TO_RAD))) *
                       RAD_TO_DEG;
    asr = noon + hour_angle(solve_lat, decl, -asr_angle);
  }

  /* Apply ihtiyat (precautionary) adjustments */
  double iht = (double)params->ihtiyat / 60.0;
  fajr += iht;
  noon += iht;
  asr += iht;
  maghrib += iht;
  isha += iht;

  struct PrayerTimes times = {
      .fajr = fajr,
      .dhuhr = noon,
      .asr = asr,
      .maghrib = maghrib,
      .isha = isha,
  };

  return times;
}
#endif // PRAYERTIMES_IMPLEMENTATION
