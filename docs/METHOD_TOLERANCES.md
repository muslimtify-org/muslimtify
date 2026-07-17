# Prayer Time Calculation: Accuracy & Tolerance Standards

Research into the official accuracy standards, ihtiyat (precautionary margins), and recommended
test tolerances for each calculation method. Sources include official publications in the local
languages of each authority.

---

## Key Finding

**No major calculation authority publishes a formal quantified tolerance like an ISO standard.**
What exists is a combination of: ihtiyat (precautionary buffer minutes), tamkin (optical/physical
correction), zone constraints, and scattered observational studies. The general scholarly consensus
from academic literature is that **+/- 5 minutes** from "true" prayer time is acceptable in
Islamic jurisprudence.

---

## Per-Method Analysis

### Kemenag (Indonesia): 2 min tolerance

| Parameter | Value |
|-----------|-------|
| Ihtiyat | +2 min all prayers, -2 min sunrise |
| Rounding | Ceiling (always rounds up) |
| Stated accuracy | +/- 1 min against official schedules |
| Transition | Moving to 16-second ihtiyat (VSOP/ELP/DE/INPOP ephemeris) |

Academic studies from UIN Malang and UIN Jakarta comparing hisab (calculation) vs rukyat
(observation) find 0-3 minute differences. The Ephemeris Hisab Rukyat method is described as
having "very high accuracy." A 1-2 minute difference is considered "still within acceptable
limits" (*masih dalam batas toleransi*) in the Indonesian hisab tradition.

**Recommended test tolerance: 2 minutes**

Sources:
- [Kriteria Jadwal Shalat Kemenag](https://mimikamuslim.com/artikel/detail/210)
- [Rasionalisasi Waktu Ihtiyat 16 Detik](https://falakmu.id/blog/2025/01/24/rasionalisasi-perhitungan-penurunan-waktu-ihtiyat-16-detik/)
- [UIN Malang - Penentuan Akurasi Waktu Shalat](https://urj.uin-malang.ac.id/index.php/jfs/article/download/297/221/)

---

### MWL (Muslim World League): 2 min tolerance

| Parameter | Value |
|-----------|-------|
| Ihtiyat | +2 min recommended |
| Rounding | Not specified |
| Stated accuracy | None published |

The MWL does not publish formal accuracy standards. The 2009 Brussels Conference on high-latitude
prayer times defined a "disturbance threshold" as a >10 minute jump between consecutive days, and
a 5-minute adjustment buffer at transition points. The general recommendation from MWL-affiliated
guidance is +2 minutes precaution.

**Recommended test tolerance: 2 minutes**

---

### ISNA / FCNA (North America): 2 min tolerance

| Parameter | Value |
|-----------|-------|
| Ihtiyat | "A few minutes" recommended, not required |
| Rounding | Not specified |
| Stated accuracy | "Not always exact" (official FCNA statement) |

The FCNA explicitly states: "these timings are not always exact" and "none of the calculation
methods are entirely accurate for all locations across North America." The 15-degree angle is
described as "a good approximation." Multiple twilight observation campaigns (UK 1983, Chicago
1985, Riyadh 2004) found true Fajr/Isha varies between 12-18 degrees depending on season,
latitude, and atmospheric conditions.

The FCNA identifies an inherent contradiction in applying fixed ihtiyat: advancing Fajr for
prayer safety makes the fasting start time less safe, and vice versa.

**Recommended test tolerance: 2 minutes**

Sources:
- [FCNA - Fifteen or Eighteen Degrees](https://fiqhcouncil.org/fifteen-or-eighteen-degrees-calculating-prayer-fasting-times-in-islam/)
- [FCNA - Suggested Calculation Method](https://fiqhcouncil.org/the-suggested-calculation-method-for-fajr-and-isha/)

---

### Egypt (EGAS): 2 min tolerance

| Parameter | Value |
|-----------|-------|
| Ihtiyat | None published |
| Rounding | Not specified |
| Stated accuracy | None published |

The Egyptian General Authority of Survey publishes no specific precision standards, tolerance
levels, or ihtiyat values. The deep 19.5-degree Fajr angle (the most conservative of all major
methods) may itself function as a built-in safety margin since observation studies find true
Fajr closer to 15-18 degrees.

**Recommended test tolerance: 2 minutes**

---

### Umm al-Qura (Makkah): 3 min tolerance

| Parameter | Value |
|-----------|-------|
| Ihtiyat | ~3 min (intentional, duration of adhan) |
| Rounding | Not specified |
| Stated accuracy | Grand Mufti: "accurate and tested" |

The Umm al-Qura calendar is **intentionally advanced by approximately 3 minutes** as ihtiyat,
corresponding roughly to the duration of the adhan call. Astronomer Abdullah al-Khudayri
confirmed the calendar is "three minutes ahead of the actual Fajr time as well as three minutes
ahead on Dhuhr." This is a deliberate religious precaution, not a calculation error.

The fixed 90-minute (120 during Ramadan) Isha interval avoids angle-based computation entirely,
making it simpler but less astronomically precise for locations far from Mecca.

**Recommended test tolerance: 3 minutes** (due to the built-in 3-min adhan ihtiyat)

Sources:
- [Arab News - Umm Al-Qura Timings Accurate](https://www.arabnews.com/saudi-arabia/news/773436)
- [IslamWeb - Umm al-Qura](https://www.islamweb.net/ar/fatwa/138714/)

---

### Karachi: 2 min tolerance

| Parameter | Value |
|-----------|-------|
| Ihtiyat | Not published (regional practice: ~2 min) |
| Rounding | Not specified |
| Stated accuracy | None published |

No formally published accuracy tolerance from the University of Islamic Sciences, Karachi. The
symmetric 18/18-degree angles align with classical Islamic astronomical texts. The general South
Asian practice follows standard 2-minute precautionary addition.

**Recommended test tolerance: 2 minutes**

---

### JAKIM (Malaysia): 2 min tolerance

| Parameter | Value |
|-----------|-------|
| Ihtiyat | Not explicitly published (zone constraint: ≤2 min) |
| Rounding | Not specified |
| Stated accuracy | Zone-based (max 2-min deviation within zone) |

Malaysia is divided into prayer time zones where the time difference between easternmost and
westernmost reference points must not exceed 2 minutes. This zone constraint effectively
functions as the tolerance. JAKIM changed Fajr from 18 to 20 degrees based on year-long research,
resulting in Fajr being ~8 minutes earlier. The Mufti Office recommends delaying congregational
Subuh by 8-10 minutes after adhan.

Third-party implementations report 1-3 minute discrepancies against official JAKIM times,
suggesting undocumented adjustment factors may exist.

**Recommended test tolerance: 2 minutes** (matching zone constraint)

Sources:
- [Mufti WP - Ketepatan Waktu Solat](https://www.muftiwp.gov.my/ms/artikel/bayan-al-falak/5088-bayan-al-falak-siri-ke-5-isu-ketepatan-waktu-solat-dalam-takwim-waktu-solat-pejabat-mufti)
- [Mufti WP - Isu Tambahan 8 Minit Subuh](https://www.muftiwp.gov.my/ms/artikel/bayan-linnas/3858-bayan-linnas-khas-isu-tambahan-waktu-8-minit-bagi-permulaan-azan-subuh-dan-hukum-solat-subuh-sebelum-masuk-waktu)

---

### MUIS (Singapore): 2 min tolerance

| Parameter | Value |
|-----------|-------|
| Ihtiyat | Not published |
| Rounding | Not specified |
| Stated accuracy | Not published |

MUIS does not publicly document methodology or accuracy standards. Third-party implementations
report 1-3 minute discrepancies. Since Singapore is geographically small (~50km), zone-based
corrections are not needed. MUIS, JAKIM, and Kemenag share identical angles (20/18), they are
algorithmically equivalent except for ihtiyat and rounding.

**Recommended test tolerance: 2 minutes**

---

### Diyanet (Turkey): 2 min tolerance

| Parameter | Value |
|-----------|-------|
| Ihtiyat (Temkin) | Per-prayer: Sunrise/Maghrib 7 min, Dhuhr 5 min, Asr 4 min, Fajr/Isha 0 min |
| Rounding | Not specified |
| Stated accuracy | "Reasonableness standard" |

Turkey's Diyanet applies **temkin** (precautionary time) that accounts for sun's apparent radius
(15'45"), atmospheric refraction (44'30"), and horizontal parallax (8.8"). For Istanbul, total
temkin is standardized at 10 minutes. The 1982 Supreme Religious Council reform moderated
previously excessive temkin values. Turkish sources warn that omitting temkin creates 15-20 minute
errors in imsak times at latitudes 36-42 degrees.

**Recommended test tolerance: 2 minutes** (for our implementation which does not apply temkin,
comparisons against official Diyanet times will show larger differences due to temkin)

Sources:
- [Diyanet - Temkin Hesaplama](https://vakithesaplama.diyanet.gov.tr/temkin.php)
- [Sorularla Islamiyet - Temkin Vakti](https://sorularlaislamiyet.com/takvimlerde-namaz-vakitleri-oruc-icin-uygulanan-temkin-vakti-hakkinda-bilgi-verir-misiniz-diyanetin)

---

## Summary: Recommended Test Tolerances

| Method | Tolerance | Rationale |
|--------|-----------|-----------|
| **Kemenag** | 2 min | Official ihtiyat standard, well-validated |
| **MWL** | 2 min | Standard ihtiyat practice |
| **ISNA** | 2 min | Standard ihtiyat practice |
| **Egypt** | 2 min | No published ihtiyat, deep Fajr angle is conservative |
| **Makkah** | 3 min | Built-in 3-min adhan ihtiyat in official calendar |
| **Karachi** | 2 min | Standard ihtiyat practice |
| **JAKIM** | 2 min | Zone constraint ≤2 min |
| **MUIS** | 2 min | Matches JAKIM/Kemenag parameters |
| **Diyanet** | 2 min | Without temkin, official times differ by temkin amount |
| **All others** | 2 min | Default conservative standard |

**Note:** These tolerances are for validating our calculator against reference sources (Aladhan
API, official timetables). The tolerances reflect both computational precision and the inherent
uncertainty acknowledged by each authority. A 2-minute tolerance means our calculated time should
be within +/- 2 minutes of the reference source.
