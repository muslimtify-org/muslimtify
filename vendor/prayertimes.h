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

/* prayertimes.h -- v0.1.0 -- single-header C/C++ prayer-time calculation library
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
 * That 6.5966 second figure is not a global accuracy claim. The oracle
 * covers |latitude| <= 60 only. Beyond that the two solvers diverge
 * sharply, because near the polar circle the Sun crosses the horizon at
 * grazing incidence and a small altitude difference becomes a large time
 * difference. Measured with the current code, maximum absolute
 * difference by latitude:
 *
 *   lat +60   2.39 s     lat -60    6.60 s
 *   lat +66   9.58 s     lat -66  123.65 s
 *   lat +68  29.55 s     lat -68  104.44 s
 *   lat +70  33.37 s     lat -70   77.99 s
 *
 * Sunrise is not asserted against an oracle, because hijri.h exposes no
 * sunrise finder. It is covered only indirectly, by sharing a code path
 * and a declination with sunset. Fajr and Isha are not asserted against
 * any oracle either, only by the published-table fixtures. Asr is also
 * covered only by the published-table fixtures, refining it was measured
 * during this work and made results worse, so it was deliberately left
 * alone.
 *
 * The published-table suite tests/test_prayertimes.c reports 903 checks
 * at a uniform tolerance of 2 minutes, with a residual distribution of
 * 442 checks at 0 minutes, 435 at 1 and 11 at 2.
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

#define REFRACTION_CORRECTION 0.833 // for dhuha/maghrib (deg)

// Dhuha prayer time: sun altitude of 4°30' above eastern horizon
// (irtifa' syams / setinggi tombak — standard in Indonesian falak)
#define DHUHA_ALTITUDE 4.3

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

typedef enum {
  HIGHLAT_NONE,
  HIGHLAT_MIDDLE_OF_NIGHT,
  HIGHLAT_ONE_SEVENTH,
  HIGHLAT_ANGLE_BASED,
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
} MethodParams;

struct PrayerTimes {
  double fajr;
  double sunrise;
  double dhuha; // Dhuha prayer time (Kemenag: ~28-30 min after sunrise)
  double dhuhr;
  double asr;
  double maghrib;
  double isha;
};

/**
 * Format a decimal-hours time (e.g. 5.5) into "HH:MM" in outBuffer.
 * Minutes are rounded up (Kemenag convention).
 */
PRAYERTIMESDEF void format_time_hm(double timeHours, char *outBuffer,
                                   size_t bufSize);

/**
 * Format a decimal-hours time (e.g. 5.5) into "HH:MM:SS" in outBuffer.
 * Seconds are rounded to nearest; use format_time_hm for the Kemenag
 * round-up-to-the-minute convention.
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
    [CALC_MWL] = {"Muslim World League", 18.0, 17.0, 0, 0, ASR_STANDARD,
                  MIDNIGHT_STANDARD, 0},
    [CALC_MAKKAH] = {"Umm al-Qura, Makkah", 18.5, 0, 90, 0, ASR_STANDARD,
                     MIDNIGHT_STANDARD, 0},
    [CALC_ISNA] = {"ISNA", 15.0, 15.0, 0, 0, ASR_STANDARD, MIDNIGHT_STANDARD,
                   0},
    [CALC_EGYPT] = {"Egyptian General Authority", 19.5, 17.5, 0, 0,
                    ASR_STANDARD, MIDNIGHT_STANDARD, 0},
    [CALC_KARACHI] = {"Univ. Islamic Sciences, Karachi", 18.0, 18.0, 0, 0,
                      ASR_STANDARD, MIDNIGHT_STANDARD, 0},
    [CALC_TURKEY] = {"Diyanet, Turkey", 18.0, 17.0, 0, 0, ASR_STANDARD,
                     MIDNIGHT_STANDARD, 0},
    [CALC_SINGAPORE] = {"MUIS, Singapore", 20.0, 18.0, 0, 0, ASR_STANDARD,
                        MIDNIGHT_STANDARD, 0},
    [CALC_JAKIM] = {"JAKIM, Malaysia", 20.0, 18.0, 0, 0, ASR_STANDARD,
                    MIDNIGHT_STANDARD, 0},
    [CALC_KEMENAG] = {"KEMENAG, Indonesia", 20.0, 18.0, 0, 0, ASR_STANDARD,
                      MIDNIGHT_STANDARD, 2},
    [CALC_FRANCE] = {"UOIF, France", 12.0, 12.0, 0, 0, ASR_STANDARD,
                     MIDNIGHT_STANDARD, 0},
    [CALC_RUSSIA] = {"Spiritual Admin., Russia", 16.0, 15.0, 0, 0, ASR_STANDARD,
                     MIDNIGHT_STANDARD, 0},
    [CALC_DUBAI] = {"GAIAE, Dubai", 18.2, 18.2, 0, 0, ASR_STANDARD,
                    MIDNIGHT_STANDARD, 0},
    [CALC_QATAR] = {"Min. of Awqaf, Qatar", 18.0, 0, 90, 0, ASR_STANDARD,
                    MIDNIGHT_STANDARD, 0},
    [CALC_KUWAIT] = {"Min. of Awqaf, Kuwait", 18.0, 17.5, 0, 0, ASR_STANDARD,
                     MIDNIGHT_STANDARD, 0},
    [CALC_JORDAN] = {"Min. of Awqaf, Jordan", 18.0, 18.0, 0, 0, ASR_STANDARD,
                     MIDNIGHT_STANDARD, 5},
    [CALC_GULF] = {"Gulf Region", 19.5, 0, 90, 0, ASR_STANDARD,
                   MIDNIGHT_STANDARD, 0},
    [CALC_TUNISIA] = {"Min. of Religious Affairs, Tunisia", 18.0, 18.0, 0, 0,
                      ASR_STANDARD, MIDNIGHT_STANDARD, 0},
    [CALC_ALGERIA] = {"Min. of Religious Affairs, Algeria", 18.0, 17.0, 0, 0,
                      ASR_STANDARD, MIDNIGHT_STANDARD, 0},
    [CALC_MOROCCO] = {"Min. of Habous, Morocco", 19.0, 17.0, 0, 0, ASR_STANDARD,
                      MIDNIGHT_STANDARD, 0},
    [CALC_PORTUGAL] = {"Comunidade Islamica de Lisboa", 18.0, 0, 77, 3,
                       ASR_STANDARD, MIDNIGHT_STANDARD, 0},
    [CALC_MOONSIGHTING] = {"Moonsighting Committee", 18.0, 18.0, 0, 3,
                           ASR_STANDARD, MIDNIGHT_STANDARD, 0},
    [CALC_CUSTOM] = {"Custom", 18.0, 17.0, 0, 0, ASR_STANDARD,
                     MIDNIGHT_STANDARD, 0},
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

/* Solve one hour-angle event with the Sun evaluated at the event's own
   instant, one iteration from an initial guess. sign is -1 before local
   noon and +1 after. Returns the refined local time in hours, or the guess
   unchanged when the event does not occur at the refined instant. */
static double refine_event(double jd, double latitude, double longitude,
                           double timezone, double altitude, double sign,
                           double guess) {
  double d2, e2, n2, ha;
  sun_position(jd + (guess - timezone) / 24.0, &d2, &e2);
  n2 = 12.0 + timezone - (longitude / 15.0) - e2;
  ha = hour_angle(latitude, d2, altitude);
  if (isnan(ha)) return guess;
  return n2 + sign * ha;
}

// Format time (double hours) into "HH:MM"
PRAYERTIMESDEF void format_time_hm(double timeHours, char *outBuffer,
                                   size_t bufSize) {
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

PRAYERTIMESDEF struct PrayerTimes
calculate_prayer_times(int year, int month, int day, double latitude,
                       double longitude, double timezone,
                       const MethodParams *params) {
  double jd = julian_day(year, month, day);
  double decl, eqt;
  sun_position(jd, &decl, &eqt);

  double noon = 12.0 + timezone - (longitude / 15.0) - eqt;

  /* Sunrise & sunset (always use refraction correction) */
  double ha_sunrise = hour_angle(latitude, decl, REFRACTION_CORRECTION);
  double sunrise = noon - ha_sunrise;
  double sunset = noon + ha_sunrise;

  sunrise = refine_event(jd, latitude, longitude, timezone,
                         REFRACTION_CORRECTION, -1.0, sunrise);
  sunset = refine_event(jd, latitude, longitude, timezone,
                        REFRACTION_CORRECTION, +1.0, sunset);

  /* Night duration for high-latitude fallback */
  double night = (24.0 - sunset) + sunrise;

  /* Fajr */
  bool fajr_failed = false;
  double ha_fajr =
      hour_angle_safe(latitude, decl, params->fajr_angle, &fajr_failed);
  double fajr = noon - ha_fajr;
  if (fajr_failed) {
    /* Angle-based high-latitude fallback */
    fajr = sunrise - (params->fajr_angle / 60.0) * night;
  }
  if (!fajr_failed)
    fajr = refine_event(jd, latitude, longitude, timezone, params->fajr_angle,
                        -1.0, fajr);

  /* Maghrib */
  double maghrib = sunset;
  if (params->maghrib_interval > 0) {
    maghrib = sunset + (double)params->maghrib_interval / 60.0;
  }

  /* Isha */
  double isha;
  if (params->isha_angle > 0.0) {
    bool isha_failed = false;
    double ha_isha =
        hour_angle_safe(latitude, decl, params->isha_angle, &isha_failed);
    isha = noon + ha_isha;
    if (isha_failed) {
      isha = sunset + (params->isha_angle / 60.0) * night;
    }
    if (!isha_failed)
      isha = refine_event(jd, latitude, longitude, timezone,
                          params->isha_angle, +1.0, isha);
  } else {
    /* Interval-based (e.g. Makkah 90 min after maghrib) */
    isha = maghrib + (double)params->isha_interval / 60.0;
  }

  /* Asr */
  double asr_angle = atan(1.0 / ((double)params->asr_shadow +
                                 tan(fabs(latitude - decl) * DEG_TO_RAD))) *
                     RAD_TO_DEG;
  double ha_asr = hour_angle(latitude, decl, -asr_angle);
  double asr = noon + ha_asr;

  /* Dhuha */
  double ha_dhuha = hour_angle(latitude, decl, -DHUHA_ALTITUDE);
  double dhuha = noon - ha_dhuha;

  /* Apply ihtiyat (precautionary) adjustments */
  double iht = (double)params->ihtiyat / 60.0;
  fajr += iht;
  sunrise -= iht; /* sunrise ihtiyat is inverted */
  noon += iht;
  asr += iht;
  maghrib += iht;
  isha += iht;
  /* Dhuha does not get ihtiyat */

  struct PrayerTimes times = {
      .fajr = fajr,
      .sunrise = sunrise,
      .dhuha = dhuha,
      .dhuhr = noon,
      .asr = asr,
      .maghrib = maghrib,
      .isha = isha,
  };

  return times;
}
#endif // PRAYERTIMES_IMPLEMENTATION
