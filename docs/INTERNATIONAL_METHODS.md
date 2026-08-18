# International Prayer Time Calculation Methods

Research notes for adding multi-method support to muslimtify.

---

## 1. Current State

> Updated 2026-08-18. This section described the state when these notes were written. Multi
> method support has since landed: 22 methods are implemented and `calculation.method` is
> resolved through `method_from_string()` at `src/core/config.c:711`. The rest of this
> document is the research that informed that work.

At the time of writing, Muslimtify supported **only the Kemenag method** with hardcoded
constants in `vendor/prayertimes.h`. The config fields `calculation.method` and
`calculation.madhab` were stored but **not used** in any calculation logic. The astronomical
algorithm is the USNO simplified model (~1 arcminute accuracy), which is sufficient for
prayer times.

---

## 2. Master Table of Calculation Methods

All angles are in degrees. Isha interval is in minutes after Maghrib when angle-based is not
used.

| #  | Method Key    | Organization                          | Fajr   | Isha Angle | Isha Interval | Maghrib Offset | Asr     |
|----|---------------|---------------------------------------|--------|------------|---------------|----------------|---------|
| 1  | `mwl`         | Muslim World League                   | 18.0   | 17.0       | -             | 0 min          | Shafi'i |
| 2  | `makkah`      | Umm al-Qura, Makkah                  | 18.5   | -          | 90 min\*      | 0 min          | Shafi'i |
| 3  | `isna`        | Islamic Society of North America      | 15.0   | 15.0       | -             | 0 min          | Shafi'i |
| 4  | `egypt`       | Egyptian General Authority of Survey  | 19.5   | 17.5       | -             | 0 min          | Shafi'i |
| 5  | `karachi`     | Univ. of Islamic Sciences, Karachi    | 18.0   | 18.0       | -             | 0 min          | Hanafi  |
| 6  | `turkey`      | Diyanet Isleri Baskanligi             | 18.0   | 17.0       | -             | 0 min          | Shafi'i |
| 7  | `singapore`   | MUIS, Singapore                       | 20.0   | 18.0       | -             | 0 min          | Shafi'i |
| 8  | `jakim`       | JAKIM, Malaysia                       | 20.0   | 18.0       | -             | 0 min          | Shafi'i |
| 9  | `kemenag`     | Kemenag, Indonesia                    | 20.0   | 18.0       | -             | 0 min          | Shafi'i |
| 10 | `france`      | UOIF, France                          | 12.0   | 12.0       | -             | 0 min          | Shafi'i |
| 11 | `russia`      | Spiritual Administration of Muslims   | 16.0   | 15.0       | -             | 0 min          | Shafi'i |
| 12 | `dubai`       | GAIAE, Dubai                          | 18.2   | 18.2       | -             | 0 min          | Shafi'i |
| 13 | `qatar`       | Ministry of Awqaf, Qatar              | 18.0   | -          | 90 min        | 0 min          | Shafi'i |
| 14 | `kuwait`      | Ministry of Awqaf, Kuwait             | 18.0   | 17.5       | -             | 0 min          | Shafi'i |
| 15 | `jordan`      | Ministry of Awqaf, Jordan             | 18.0   | 18.0       | -             | 5 min          | Shafi'i |
| 16 | `gulf`        | Gulf Region (general)                 | 19.5   | -          | 90 min        | 0 min          | Shafi'i |
| 17 | `tunisia`     | Ministry of Religious Affairs         | 18.0   | 18.0       | -             | 0 min          | Shafi'i |
| 18 | `algeria`     | Ministry of Religious Affairs         | 18.0   | 17.0       | -             | 0 min          | Shafi'i |
| 19 | `morocco`     | Ministry of Habous and Islamic Affairs| 19.0   | 17.0       | -             | 0 min          | Shafi'i |
| 20 | `portugal`    | Comunidade Islamica de Lisboa         | 18.0   | -          | 77 min        | 3 min          | Shafi'i |
| 21 | `moonsighting`| Moonsighting Committee Worldwide      | 18.0\*\*| 18.0\*\*  | -             | 3 min          | Shafi'i |

> \* Umm al-Qura uses **120 minutes** during Ramadan instead of 90.
>
> \*\* Moonsighting Committee uses 18.0 as a baseline but applies **seasonal latitude-dependent
> curve-fit functions**. It is the most complex method and cannot be reduced to a simple
> angle table.

### Midnight Modes

- **Standard:** Midpoint between Sunset and Sunrise.

---

## 3. Per-Method Special Notes

### Umm al-Qura (Makkah)

- Before December 2008, Fajr was 19.0. Changed to 18.5 after that date.
- Isha during Ramadan: **120 minutes** after Maghrib (vs. 90 in normal months).
- Requires Hijri calendar awareness or a Ramadan date lookup table.

### Diyanet (Turkey)

- Applies a "Tamkin" period (~10 minutes) accounting for solar semi-diameter, horizontal
  refraction, and parallax.
- During Ramadan, Imsak (fasting start) historically uses $-19.0^\circ$ while normal Fajr uses
  $-18.0^\circ$.
- Less accurate outside the geographical region of Turkey.

### Dubai (UAE)

- Uses 18.2 for both Fajr and Isha.
- Applies **+3 minute offsets** to Sunrise, Dhuhr, Asr, and Maghrib after calculation.

### Kemenag (Indonesia)

- Ihtiyat: +2 minutes on all prayer times (Sunrise gets -2 minutes).
- Ceiling rounding: always rounds up to the next minute.
- Includes Dhuha calculation at sun altitude $+4.3^\circ$.
- Transitioning to 16-second ihtiyat based on modern ephemeris data (VSOP, ELP, DE, INPOP).

### Morocco

- Official times are manually adjusted per city by the Ministry of Habous.
- Developers approximate with $19.0^\circ$ Fajr and $17.0^\circ$ Isha, plus Maghrib/Dhuhr
  offsets of up to 5-7 minutes.

### Jordan

- Unique in applying a 5-minute fixed offset after sunset for Maghrib.

### Portugal

- Uses a fixed 77-minute interval after Maghrib for Isha.
- 3-minute Maghrib offset after sunset.

### Moonsighting Committee

- Does **NOT** use simple fixed angles. Uses 18.0 as a baseline but applies latitude-and-season
  dependent curve-fit functions.
- Three *shafaq* (twilight) modes for Isha: `general` (default), `ahmer` (red twilight,
  preferred in summer), `abyad` (white twilight, preferred in winter).
- Latitude thresholds:
  - 0-55: seasonal functions
  - 55-60: applies Sab'u Lail (1/7th night rule)
  - 60+: slides calculations down to latitude 60
- Isha = **earlier** of: angle-based time or 1/7th of the night.
- Fajr = **later** of: angle-based time or 6/7ths of the night.
- Zuhr: adds 5 minutes past noon (1.5 min sun disk clearance + 1 min radius buffer + 2.5 min
  safety).

---

## 4. Asr Juristic Schools

| School                              | Shadow Factor ($n$) | Formula                                     |
|-------------------------------------|---------------------|---------------------------------------------|
| Standard (Shafi'i, Maliki, Hanbali) | 1                   | Shadow = object height + noon shadow        |
| Hanafi                              | 2                   | Shadow = 2x object height + noon shadow     |

The Asr altitude angle:

```
asr_angle = atan(1.0 / (n + tan(abs(latitude - declination))))
```

> **Note:** Some Shia sources mention Asr at $n = 4/7$, but this is rarely implemented in modern
> calculators. Most Shia apps use Standard ($n = 1$) for Asr.

---

## 5. High-Latitude Adjustments

Standard formulas fail at high latitudes (generally above ~$48^\circ$-$50^\circ$ N/S) when the
Sun never reaches the required depression angle. The `acos()` argument falls outside $[-1, 1]$,
producing `NaN`.

### Detection

```c
double cos_ha = (sin(angle_rad) - sin(lat_rad) * sin(decl_rad)) /
                (cos(lat_rad) * cos(decl_rad));
if (cos_ha < -1.0 || cos_ha > 1.0) {
    // Abnormal period: apply high-latitude adjustment
}
```

### 5.1 Middle of the Night

Divides the night (Sunset to Sunrise) into two equal halves.

```c
double night = sunrise_next - sunset;
double midnight = sunset + night / 2.0;
// Isha: min(isha_calculated, midnight)
// Fajr: max(fajr_calculated, midnight)
```

### 5.2 One-Seventh of the Night (Sab'u Lail)

```c
double night = sunrise_next - sunset;
double seventh = night / 7.0;
isha = sunset + seventh;           // end of 1st seventh
fajr = sunrise_next - seventh;     // start of last seventh
```

Used by UK Islamic authorities and Moonsighting Committee above 55 latitude.

### 5.3 Angle-Based (Percentage Rule)

Treats the twilight angle as a percentage of a $60^\circ$ quadrant.

```c
double night = sunrise_next - sunset;
fajr = sunrise - (fajr_angle / 60.0) * night;
isha = sunset  + (isha_angle / 60.0) * night;
```

**Recommended default** for algorithmic implementations (used by PrayTimes.org).

### 5.4 Nearest Latitude (Aqrabul-Bilad)

For extreme latitudes (above ~$60^\circ$), re-run the calculation with `latitude = 45.0` (or
$48.0$) while keeping the actual longitude and timezone.

### Summary

| Method           | Mechanism               | Typical Usage                   |
|------------------|-------------------------|---------------------------------|
| Middle of Night  | Night / 2               | Areas with very long twilight   |
| One-Seventh      | Night / 7               | UK, Moonsighting Committee      |
| Angle-Based      | Angle / 60              | Modern algorithmic default      |
| Nearest Latitude | Use lower latitude times | Extreme North (Norway, Sweden)  |

---

## 6. Elevation Correction

Adjusts the sunrise/sunset depression angle for observer elevation:

$$
\alpha = -0.8333 - 0.0347 \times \sqrt{h}
$$

Where $h$ is meters above surrounding terrain.

- Affects **Sunrise** and **Maghrib** (and interval-based Isha in Umm al-Qura/Qatar/Gulf).
- Fajr and Isha angle-based calculations are generally **NOT** adjusted for elevation because
  twilight phenomena are atmospheric, not geometric.
- Example at 100m: $-0.8333 - 0.0347 \times 10.0 = -1.1803^\circ$

---

## 7. Astronomical Algorithm Accuracy

| Algorithm                        | Accuracy           | Complexity | Suitability              |
|----------------------------------|--------------------|------------|--------------------------|
| USNO Simplified (current)        | ~1 arcmin (~4 sec) | Low        | Prayer times, embedded   |
| Jean Meeus Ch. 25                | ~0.01 deg          | Medium     | General high accuracy    |
| VSOP87 (truncated, via Meeus)    | < 1 arcsecond      | Medium-High| Research-grade           |
| NREL SPA (Reda & Andreas 2004)   | +/- 0.0003 deg     | High       | Solar energy, high-prec  |

The current USNO simplified algorithm translates to ~4 seconds of time error. With a 2-minute
ihtiyat margin, **upgrading the astronomical algorithm is not a priority** for accuracy. The
method parameters (Fajr/Isha angles) have far greater impact on output times.

---

## 8. Proposed C Data Structures

### Enumerations

```c
typedef enum {
  CALC_MWL = 0,
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
  ASR_STANDARD = 1, // Shafi'i / Maliki / Hanbali
  ASR_HANAFI   = 2, // Hanafi
} AsrSchool;

typedef enum {
  HIGHLAT_NONE = 0,
  HIGHLAT_MIDDLE_OF_NIGHT,
  HIGHLAT_ONE_SEVENTH,
  HIGHLAT_ANGLE_BASED,
  HIGHLAT_NEAREST_LATITUDE,
} HighLatMethod;

typedef enum {
  MIDNIGHT_STANDARD = 0, // midpoint Sunset-Sunrise
} MidnightMode;
```

### Method Parameters Struct

```c
typedef struct {
  double       fajr_angle;
  double       isha_angle;          // 0 if using fixed interval
  double       isha_interval;       // minutes after Maghrib (0 if using angle)
  double       maghrib_interval;    // minutes after sunset (0 for immediate)
  AsrSchool    asr_school;
  MidnightMode midnight_mode;
  double       ihtiyat;             // precautionary margin in minutes
  double       ramadan_isha_interval; // 0 or 120 (Umm al-Qura)
} MethodParams;
```

### Method Table (Static Initialization)

```c
//                              fajr   isha_a isha_i mag_i asr       midnight       iht  ram
static const MethodParams METHOD_TABLE[CALC_COUNT] = {
  [CALC_MWL]         = { 18.0,  17.0,   0,   0,   ASR_STANDARD, MIDNIGHT_STANDARD, 0,   0   },
  [CALC_MAKKAH]      = { 18.5,   0,    90,   0,   ASR_STANDARD, MIDNIGHT_STANDARD, 0, 120   },
  [CALC_ISNA]        = { 15.0,  15.0,   0,   0,   ASR_STANDARD, MIDNIGHT_STANDARD, 0,   0   },
  [CALC_EGYPT]       = { 19.5,  17.5,   0,   0,   ASR_STANDARD, MIDNIGHT_STANDARD, 0,   0   },
  [CALC_KARACHI]     = { 18.0,  18.0,   0,   0,   ASR_HANAFI,   MIDNIGHT_STANDARD, 0,   0   },
  [CALC_TURKEY]      = { 18.0,  17.0,   0,   0,   ASR_STANDARD, MIDNIGHT_STANDARD, 0,   0   },
  [CALC_SINGAPORE]   = { 20.0,  18.0,   0,   0,   ASR_STANDARD, MIDNIGHT_STANDARD, 0,   0   },
  [CALC_JAKIM]       = { 20.0,  18.0,   0,   0,   ASR_STANDARD, MIDNIGHT_STANDARD, 0,   0   },
  [CALC_KEMENAG]     = { 20.0,  18.0,   0,   0,   ASR_STANDARD, MIDNIGHT_STANDARD, 2,   0   },
  [CALC_FRANCE]      = { 12.0,  12.0,   0,   0,   ASR_STANDARD, MIDNIGHT_STANDARD, 0,   0   },
  [CALC_RUSSIA]      = { 16.0,  15.0,   0,   0,   ASR_STANDARD, MIDNIGHT_STANDARD, 0,   0   },
  [CALC_DUBAI]       = { 18.2,  18.2,   0,   0,   ASR_STANDARD, MIDNIGHT_STANDARD, 0,   0   },
  [CALC_QATAR]       = { 18.0,   0,    90,   0,   ASR_STANDARD, MIDNIGHT_STANDARD, 0,   0   },
  [CALC_KUWAIT]      = { 18.0,  17.5,   0,   0,   ASR_STANDARD, MIDNIGHT_STANDARD, 0,   0   },
  [CALC_JORDAN]      = { 18.0,  18.0,   0,   5,   ASR_STANDARD, MIDNIGHT_STANDARD, 0,   0   },
  [CALC_GULF]        = { 19.5,   0,    90,   0,   ASR_STANDARD, MIDNIGHT_STANDARD, 0,   0   },
  [CALC_TUNISIA]     = { 18.0,  18.0,   0,   0,   ASR_STANDARD, MIDNIGHT_STANDARD, 0,   0   },
  [CALC_ALGERIA]     = { 18.0,  17.0,   0,   0,   ASR_STANDARD, MIDNIGHT_STANDARD, 0,   0   },
  [CALC_MOROCCO]     = { 19.0,  17.0,   0,   0,   ASR_STANDARD, MIDNIGHT_STANDARD, 0,   0   },
  [CALC_PORTUGAL]    = { 18.0,   0,    77,   3,   ASR_STANDARD, MIDNIGHT_STANDARD, 0,   0   },
  [CALC_MOONSIGHTING]= { 18.0,  18.0,   0,   3,   ASR_STANDARD, MIDNIGHT_STANDARD, 0,   0   },
};
```

---

## 9. Implementation Considerations

### Calculation Branching Logic

The current `calculate_prayer_times()` must be extended to handle three branching paths:

1. **Isha:** If `isha_interval > 0`, compute as `maghrib + interval / 60.0` instead of using
   the hour angle formula with `isha_angle`.

2. **Maghrib:** If `maghrib_interval > 0` (Jordan/Portugal), add interval to sunset. Otherwise
   Maghrib = sunset.

3. **Asr:** Use `asr_school` as the shadow factor $n$ in the formula.

### Ihtiyat Handling

Currently hardcoded to 2 minutes for all times. Must become per-method:
- Kemenag: +2 min all times, -2 min sunrise
- Dubai: +3 min on sunrise/dhuhr/asr/maghrib
- Moonsighting: +5 min on dhuhr
- Most others: 0 min (no ihtiyat)

### High-Latitude Detection

Wrap the `hour_angle()` function to detect `NaN` results:

```c
static double hour_angle_safe(double lat, double decl, double angle, bool *failed) {
    double cos_ha = (-sin(angle * DEG_TO_RAD) - sin(lat * DEG_TO_RAD) * sin(decl * DEG_TO_RAD))
                  / (cos(lat * DEG_TO_RAD) * cos(decl * DEG_TO_RAD));
    if (cos_ha < -1.0 || cos_ha > 1.0) {
        *failed = true;
        return 0.0;
    }
    *failed = false;
    return acos(cos_ha) * RAD_TO_DEG / 15.0;
}
```

### Ramadan Detection for Umm al-Qura

The simplest approach is a lookup table of Ramadan start/end Gregorian dates for the next
~20 years, avoiding a full Hijri calendar implementation.

### Backward Compatibility

The current `calculate_prayer_times()` signature takes no method parameter. To maintain backward
compatibility:

```c
// New function with method parameter
struct PrayerTimes calculate_prayer_times_ex(
    int year, int month, int day,
    double latitude, double longitude, double timezone,
    CalcMethod method, AsrSchool asr, HighLatMethod highlat);

// Old function remains as a Kemenag shortcut
struct PrayerTimes calculate_prayer_times(
    int year, int month, int day,
    double latitude, double longitude, double timezone) {
    return calculate_prayer_times_ex(year, month, day, latitude, longitude, timezone,
                                     CALC_KEMENAG, ASR_STANDARD, HIGHLAT_NONE);
}
```

### Config Integration

The existing config fields need to be wired to the calculator:

```json
{
  "calculation": {
    "method": "mwl",
    "madhab": "hanafi",
    "high_latitude": "angle_based"
  }
}
```

---

## 10. Validation Strategy

### Reference Sources for Testing

| Method     | Reference Source                                      |
|------------|-------------------------------------------------------|
| Kemenag    | jadwalsholat.org (already validated, 51 test cases)   |
| MWL        | praytimes.org calculator                              |
| ISNA       | praytimes.org calculator, IslamicFinder.org            |
| Egypt      | praytimes.org calculator                              |
| Makkah     | ummulqura.org.sa                                      |
| Turkey     | namazvakti.diyanet.gov.tr                             |
| Singapore  | www.muis.gov.sg                                       |
| Malaysia   | www.e-solat.gov.my (JAKIM)                            |
| Aladhan    | api.aladhan.com (supports all methods via API)        |

### Testing Approach

1. For each new method, pick 3-5 geographically representative cities.
2. Compare against the official source or Aladhan API for 4 dates across seasons (equinox +
   solstice).
3. Accept tolerance of **1-2 minutes** (matching Kemenag test standard).
4. Add high-latitude test cases (e.g., London 51.5N, Oslo 59.9N, Tromso 69.6N) to verify
   adjustment methods.

---

## 11. Sources

- [Aladhan API - Calculation Methods](https://aladhan.com/calculation-methods)
- [PrayTimes.org - Calculation](https://praytimes.org/docs/calculation)
- [PrayTimes.org - Methods](https://praytimes.org/docs/methods)
- [Adhan-JS METHODS.md (Batoul Apps)](https://github.com/batoulapps/adhan-js/blob/master/METHODS.md)
- [Moonsighting Committee](https://www.moonsighting.com/how-we.html)
- [NREL Solar Position Algorithm](https://midcdmz.nrel.gov/spa/)
- [FIQH Council of North America](https://fiqhcouncil.org/the-suggested-calculation-method-for-fajr-and-isha/)
- [Radhi Fadlillah - Calculating Prayer Times](https://radhifadlillah.com/blog/2020-09-06-calculating-prayer-times/)
- Jean Meeus, *Astronomical Algorithms*, 2nd Ed., 1998 (Chapters 22, 25, 12, 15)
