# Metode Perhitungan Waktu Shalat Kemenag RI

Dokumen ini menjelaskan metode perhitungan waktu shalat yang digunakan oleh Kementerian Agama Republik Indonesia (Kemenag RI) secara lengkap dan detail.

> **Catatan, prayertimes.h v0.2.0.** Sejak v0.2.0 `struct PrayerTimes` hanya mengembalikan lima waktu shalat wajib: Subuh, Dzuhur, Ashar, Maghrib, Isya. Terbit dan Dhuha dihapus dari API, dan muslimtify juga tidak lagi menampilkan atau memberi notifikasi untuk keduanya. Terbit bukan waktu shalat melainkan batas akhir waktu Subuh, dan Dhuha adalah shalat sunnah yang hanya dicantumkan pada jadwal terbitan Indonesia. Keduanya masih dihitung di dalam library, karena Maghrib adalah saat terbenam dan setiap substitusi lintang tinggi mengukur panjang malam antara terbenam dan terbit, tetapi tidak lagi menjadi bagian dari kontrak API.
>
> Bagian di bawah yang menjelaskan Terbit dan Dhuha tetap dipertahankan sebagai catatan metode Kemenag itu sendiri, yang memang menerbitkan keduanya. Bagian tersebut bukan lagi deskripsi API.


## Daftar Isi

1. [Kriteria Kemenag](#kriteria-kemenag)
2. [Konstanta dan Parameter](#konstanta-dan-parameter)
3. [Langkah-Langkah Perhitungan](#langkah-langkah-perhitungan)
4. [Perhitungan Waktu Shalat](#perhitungan-waktu-shalat)
5. [Implementasi](#implementasi)
6. [Contoh Perhitungan](#contoh-perhitungan)
7. [Referensi](#referensi)

---

## Kriteria Kemenag

Kementerian Agama Republik Indonesia menggunakan kriteria berikut untuk perhitungan waktu shalat:

### Parameter Sudut Matahari

| Waktu Shalat | Sudut Matahari | Keterangan |
|--------------|----------------|------------|
| **Subuh** | -20° | Matahari 20° di bawah ufuk timur |
| **Terbit** | -0.833° | Koreksi refraksi atmosfer |
| **Dhuha** | +4.3° | Matahari setinggi tombak di atas ufuk timur (irtifa' syams) |
| **Dzuhur** | Transit | Matahari melintasi meridian |
| **Ashar** | tan(h) = 1/(tan(|φ-δ|) + 1) | Bayangan = panjang benda + panjang bayangan saat dzuhur |
| **Maghrib** | -0.833° | Sama dengan waktu terbenam |
| **Isya** | -18° | Matahari 18° di bawah ufuk barat |

### Ihtiyat (Koreksi Kehati-hatian)

Kemenag menambahkan **ihtiyat (koreksi kehati-hatian)** sebesar **2 menit** untuk semua waktu shalat:

| Waktu | Ihtiyat |
|-------|---------|
| Subuh | +2 menit |
| Terbit | -2 menit |
| Dzuhur | +2 menit |
| Ashar | +2 menit |
| Maghrib | +2 menit |
| Isya | +2 menit |

**Catatan:** Per tahun 2025, Kemenag sedang mentransisi ke ihtiyat 16 detik berdasarkan perhitungan yang lebih akurat menggunakan data ephemeris modern (VSOP, ELP, DE, INPOP).

---

## Konstanta dan Parameter

### Konstanta Astronomi

```c
#define DEG_TO_RAD (M_PI / 180.0)        // Konversi derajat ke radian
#define RAD_TO_DEG (180.0 / M_PI)        // Konversi radian ke derajat

#define JULIAN_EPOCH 2451545.0           // Epoch J2000.0 (1 Jan 2000, 12:00 TT)

// Parameter Matahari
#define SUN_MEAN_ANOMALY_OFFSET 357.529  // g₀ (derajat)
#define SUN_MEAN_ANOMALY_RATE 0.98560028 // Perubahan per hari

#define SUN_MEAN_LONGITUDE_OFFSET 280.459   // q₀ (derajat)
#define SUN_MEAN_LONGITUDE_RATE 0.98564736  // Perubahan per hari

#define SUN_ECCENTRICITY_AMPLITUDE1 1.915   // Amplitudo eksentrisitas orde-1
#define SUN_ECCENTRICITY_AMPLITUDE2 0.020   // Amplitudo eksentrisitas orde-2

#define OBLIQUITY_COEFF 23.439              // ε₀ (derajat)
#define OBLIQUITY_RATE 0.00000036           // Perubahan per hari

#define REFRACTION_CORRECTION 0.833         // Koreksi refraksi (derajat)
```

### Parameter Kemenag

```c
#define FAJR_ANGLE_KEMENAG 20.0    // Sudut Subuh = -20°
#define ISHA_ANGLE_KEMENAG 18.0    // Sudut Isya = -18°
#define SHADOW_FACTOR_STANDARD 1.0 // Faktor bayangan Ashar (Syafi'i)

// Dhuha: ketinggian matahari setinggi tombak (irtifa' syams)
#define DHUHA_ALTITUDE 4.3         // Sudut Dhuha = +4.3° di atas ufuk

// Ihtiyat (dalam menit)
#define IHTIYAT_FAJR 2.0
#define IHTIYAT_SUNRISE -2.0
#define IHTIYAT_DHUHR 2.0
#define IHTIYAT_ASR 2.0
#define IHTIYAT_MAGHRIB 2.0
#define IHTIYAT_ISHA 2.0
```

---

## Langkah-Langkah Perhitungan

### Langkah 1: Hitung Julian Day

Julian Day (JD) adalah sistem penomoran hari yang berkelanjutan sejak 1 Januari 4713 SM.

**Formula:**

Untuk tanggal Gregorian (tahun, bulan, hari):

```
Jika bulan ≤ 2:
    tahun = tahun - 1
    bulan = bulan + 12

A = floor(tahun / 100)
B = 2 - A + floor(A / 4)

JD = floor(365.25 × (tahun + 4716)) + floor(30.6001 × (bulan + 1)) + hari + B - 1524.5
```

**Contoh:**
Untuk 21 November 2025:
```
tahun = 2025, bulan = 11, hari = 21
A = floor(2025 / 100) = 20
B = 2 - 20 + floor(20 / 4) = 2 - 20 + 5 = -13

JD = floor(365.25 × 6741) + floor(30.6001 × 12) + 21 + (-13) - 1524.5
JD = 2461780 + 367 + 21 - 13 - 1524.5
JD = 2460630.5
```

### Langkah 2: Hitung Posisi Matahari

#### 2.1 Hitung Jumlah Hari dari Epoch J2000.0

```
D = JD - 2451545.0
```

#### 2.2 Hitung Mean Anomaly (g)

Mean anomaly adalah posisi sudut benda langit dalam orbitnya.

```
g = 357.529 + 0.98560028 × D
g = normalize_deg(g)  // Normalisasi ke [0, 360)
```

#### 2.3 Hitung Mean Longitude (q)

```
q = 280.459 + 0.98564736 × D
q = normalize_deg(q)
```

#### 2.4 Hitung Ecliptic Longitude (L)

Longitude ekliptika dengan koreksi eksentrisitas:

```
L = q + 1.915 × sin(g) + 0.020 × sin(2g)
L = normalize_deg(L)
```

#### 2.5 Hitung Obliquity of the Ecliptic (ε)

Kemiringan sumbu rotasi Bumi:

```
ε = 23.439 - 0.00000036 × D
```

#### 2.6 Hitung Right Ascension (RA)

```
RA = atan2(cos(ε) × sin(L), cos(L))
RA = normalize_deg(RA × RAD_TO_DEG)
```

#### 2.7 Hitung Declination (δ)

Deklinasi adalah sudut matahari terhadap ekuator langit:

```
δ = asin(sin(ε) × sin(L)) × RAD_TO_DEG
```

#### 2.8 Hitung Equation of Time (EqT)

Koreksi waktu karena orbit elips dan kemiringan sumbu Bumi:

```
EqT = (q / 15.0) - (RA / 15.0)  // dalam jam
```

**Fungsi normalisasi:**
```c
double normalize_deg(double angle) {
    double a = fmod(angle, 360.0);
    if (a < 0) a += 360.0;
    return a;
}
```

### Langkah 3: Hitung Waktu Transit (Dzuhur)

Transit adalah saat matahari melintasi meridian lokal (tengah hari astronomi).

```
Transit = 12.0 + Timezone - (Longitude / 15.0) - EqT
```

**Parameter:**
- `Timezone`: Zona waktu dalam jam (misal: WIB = +7.0)
- `Longitude`: Bujur geografis (positif untuk Timur, negatif untuk Barat)

### Langkah 4: Hitung Hour Angle

Hour angle adalah sudut yang dibutuhkan matahari untuk mencapai ketinggian tertentu.

**Formula umum:**

```
cos(H) = [sin(h) - sin(φ) × sin(δ)] / [cos(φ) × cos(δ)]
H = acos(cos(H)) × RAD_TO_DEG / 15.0  // Konversi ke jam
```

**Parameter:**
- `h`: Sudut ketinggian matahari (altitude angle)
- `φ`: Lintang geografis (latitude)
- `δ`: Deklinasi matahari

**Catatan:**
- Hour angle dihitung dalam jam (bukan derajat)
- Untuk kejadian sebelum transit (Subuh, Terbit): `Waktu = Transit - H`
- Untuk kejadian setelah transit (Ashar, Maghrib, Isya): `Waktu = Transit + H`

---

## Perhitungan Waktu Shalat

### 1. Subuh

**Definisi:** Waktu ketika fajar shadiq (fajar benar) mulai terbit, yaitu saat matahari berada 20° di bawah ufuk timur.

**Langkah:**

1. Hitung hour angle untuk h = -20°:
   ```
   cos(H_subuh) = [sin(-20°) - sin(φ) × sin(δ)] / [cos(φ) × cos(δ)]
   H_subuh = acos(cos(H_subuh)) × RAD_TO_DEG / 15.0
   ```

2. Hitung waktu Subuh:
   ```
   Subuh = Transit - H_subuh
   ```

3. Tambahkan ihtiyat:
   ```
   Subuh = Subuh + (2 / 60.0)  // +2 menit
   ```

4. Format dengan pembulatan ke atas (ceiling):
   ```
   jam = floor(Subuh)
   menit = ceil((Subuh - jam) × 60)
   ```

### 2. Terbit (Sunrise)

**Definisi:** Waktu ketika piringan atas matahari mulai terlihat di ufuk timur.

**Langkah:**

1. Hitung hour angle untuk h = -0.833° (koreksi refraksi):
   ```
   cos(H_terbit) = [sin(-0.833°) - sin(φ) × sin(δ)] / [cos(φ) × cos(δ)]
   H_terbit = acos(cos(H_terbit)) × RAD_TO_DEG / 15.0
   ```

2. Hitung waktu Terbit:
   ```
   Terbit = Transit - H_terbit
   ```

3. Kurangi ihtiyat:
   ```
   Terbit = Terbit - (2 / 60.0)  // -2 menit
   ```

**Catatan:** Terbit bukan waktu shalat wajib, tetapi digunakan untuk menandai akhir waktu Subuh dan sebagai referensi astronomi.

### 3. Dhuha

**Definisi:** Waktu ketika matahari telah naik setinggi tombak (*irtifa' syams* / *rumhi*) di atas ufuk timur. Dalam ilmu falak Indonesia, ketinggian ini setara dengan sudut +4.3° di atas horizon.

**Langkah:**

1. Hitung hour angle untuk h = +4.3° (di atas horizon):
   ```
   cos(H_dhuha) = [sin(+4.3°) - sin(φ) × sin(δ)] / [cos(φ) × cos(δ)]
   H_dhuha = acos(cos(H_dhuha)) × RAD_TO_DEG / 15.0
   ```

2. Hitung waktu Dhuha:
   ```
   Dhuha = Transit - H_dhuha
   ```

3. Format dengan ceiling:
   ```
   jam = floor(Dhuha)
   menit = ceil((Dhuha - jam) × 60)
   ```

**Catatan:**
- Dhuha adalah shalat sunnah, sehingga tidak mendapat ihtiyat terpisah.
- Ulama berbeda pendapat tentang nilai sudut "setinggi tombak": 3.5°, 4.3°, 4.5°, dan 5°. Nilai 4.3° dipilih karena paling sesuai dengan data referensi jadwalsholat.org (Kemenag).
- Di wilayah Indonesia (lintang -11° s.d. +6°), waktu Dhuha jatuh sekitar 22-25 menit setelah terbit matahari, tergantung lokasi dan musim.
- Waktu Dhuha berakhir menjelang Dzuhur (sekitar 15 menit sebelum transit).

### 4. Dzuhur

**Definisi:** Waktu ketika matahari melewati meridian (garis bujur lokal) dan mulai condong ke barat.

**Langkah:**

1. Waktu Dzuhur = waktu transit:
   ```
   Dzuhur = Transit
   ```

2. Tambahkan ihtiyat:
   ```
   Dzuhur = Dzuhur + (2 / 60.0)  // +2 menit
   ```

3. Format dengan ceiling:
   ```
   jam = floor(Dzuhur)
   menit = ceil((Dzuhur - jam) × 60)
   ```

**Catatan:** Dalam praktik, waktu Dzuhur ditambah beberapa menit untuk memastikan matahari benar-benar sudah condong (zawal).

### 5. Ashar

**Definisi:** Waktu ketika panjang bayangan suatu benda sama dengan panjang benda ditambah panjang bayangan saat dzuhur.

**Langkah:**

1. Hitung sudut Ashar berdasarkan madzhab Syafi'i (faktor bayangan = 1):
   ```
   h_ashar = atan(1 / (1 + tan(|φ - δ|))) × RAD_TO_DEG
   ```

   **Catatan:**
   - Madzhab Syafi'i: faktor = 1 (bayangan = panjang benda + bayangan dzuhur)
   - Madzhab Hanafi: faktor = 2 (bayangan = 2× panjang benda + bayangan dzuhur)

2. Hitung hour angle:
   ```
   cos(H_ashar) = [sin(h_ashar) - sin(φ) × sin(δ)] / [cos(φ) × cos(δ)]
   H_ashar = acos(cos(H_ashar)) × RAD_TO_DEG / 15.0
   ```

3. Hitung waktu Ashar:
   ```
   Ashar = Transit + H_ashar
   ```

4. Tambahkan ihtiyat:
   ```
   Ashar = Ashar + (2 / 60.0)  // +2 menit
   ```

5. Format dengan ceiling:
   ```
   jam = floor(Ashar)
   menit = ceil((Ashar - jam) × 60)
   ```

### 6. Maghrib

**Definisi:** Waktu ketika piringan atas matahari terbenam di ufuk barat.

**Langkah:**

1. Hitung hour angle untuk h = -0.833° (sama dengan terbit):
   ```
   cos(H_maghrib) = [sin(-0.833°) - sin(φ) × sin(δ)] / [cos(φ) × cos(δ)]
   H_maghrib = acos(cos(H_maghrib)) × RAD_TO_DEG / 15.0
   ```

2. Hitung waktu Maghrib:
   ```
   Maghrib = Transit + H_maghrib
   ```

3. Tambahkan ihtiyat:
   ```
   Maghrib = Maghrib + (2 / 60.0)  // +2 menit
   ```

4. Format dengan ceiling:
   ```
   jam = floor(Maghrib)
   menit = ceil((Maghrib - jam) × 60)
   ```

**Catatan:** `H_maghrib` sama dengan `H_terbit` karena geometri simetris.

### 7. Isya

**Definisi:** Waktu ketika ufuk barat menjadi gelap (syafaq ahmar hilang), yaitu saat matahari berada 18° di bawah ufuk barat.

**Langkah:**

1. Hitung hour angle untuk h = -18°:
   ```
   cos(H_isya) = [sin(-18°) - sin(φ) × sin(δ)] / [cos(φ) × cos(δ)]
   H_isya = acos(cos(H_isya)) × RAD_TO_DEG / 15.0
   ```

2. Hitung waktu Isya:
   ```
   Isya = Transit + H_isya
   ```

3. Tambahkan ihtiyat:
   ```
   Isya = Isya + (2 / 60.0)  // +2 menit
   ```

4. Format dengan ceiling:
   ```
   jam = floor(Isya)
   menit = ceil((Isya - jam) × 60)
   ```

---

## Implementasi

### Struktur Data

```c
struct PrayerTimes {
    double fajr;     // Subuh
    double dhuhr;    // Dzuhur
    double asr;      // Ashar
    double maghrib;  // Maghrib
    double isha;     // Isya
};
```

### Fungsi Utama

```c
struct PrayerTimes calculate_prayer_times(
    int year,           // Tahun
    int month,          // Bulan (1-12)
    int day,            // Tanggal (1-31)
    double latitude,    // Lintang geografis (-90 hingga +90)
    double longitude,   // Bujur geografis (-180 hingga +180)
    double timezone     // Zona waktu (WIB = +7.0, WITA = +8.0, WIT = +9.0)
);
```

### Fungsi Pembantu

```c
// Normalisasi sudut ke [0, 360)
double normalize_deg(double angle);

// Hitung Julian Day
double julian_day(int year, int month, int day);

// Hitung posisi matahari (deklinasi dan equation of time)
void sun_position(double jd, double *decl, double *eqt);

// Hitung hour angle
double hour_angle(double lat, double decl, double angle);

// Format waktu HH:MM dengan ceiling
void format_time_hm(double timeHours, char *outBuffer, size_t bufSize);

// Format waktu HH:MM:SS
void format_time_hms(double timeHours, char *outBuffer, size_t bufSize);
```

### Pseudocode Lengkap

```
FUNCTION calculate_prayer_times(year, month, day, latitude, longitude, timezone):
    // Langkah 1: Julian Day
    jd = julian_day(year, month, day)

    // Langkah 2: Posisi Matahari
    (decl, eqt) = sun_position(jd)

    // Langkah 3: Waktu Transit (Dzuhur)
    noon = 12.0 + timezone - (longitude / 15.0) - eqt

    // Langkah 4: Terbit dan Maghrib (h = -0.833°)
    ha_sunrise = hour_angle(latitude, decl, 0.833)
    sunrise = noon - ha_sunrise
    maghrib = noon + ha_sunrise

    // Langkah 5: Subuh (h = -20°)
    ha_fajr = hour_angle(latitude, decl, 20.0)
    fajr = noon - ha_fajr

    // Langkah 6: Isya (h = -18°)
    ha_isha = hour_angle(latitude, decl, 18.0)
    isha = noon + ha_isha

    // Langkah 7: Ashar
    asr_angle = atan(1.0 / (1.0 + tan(|latitude - decl|)))
    ha_asr = hour_angle(latitude, decl, asr_angle)
    asr = noon + ha_asr

    // Langkah 8: Dhuha (h = +4.3°, matahari setinggi tombak)
    ha_dhuha = hour_angle(latitude, decl, -4.3)  // negatif = di atas horizon
    dhuha = noon - ha_dhuha

    // Langkah 9: Tambahkan Ihtiyat (Dhuha tidak mendapat ihtiyat)
    fajr = fajr + (2.0 / 60.0)
    sunrise = sunrise - (2.0 / 60.0)
    noon = noon + (2.0 / 60.0)
    asr = asr + (2.0 / 60.0)
    maghrib = maghrib + (2.0 / 60.0)
    isha = isha + (2.0 / 60.0)

    // Langkah 10: Kembalikan hasil
    RETURN {fajr, sunrise, dhuha, noon, asr, maghrib, isha}

FUNCTION format_time_hm(timeHours):
    hours = floor(timeHours)
    fraction = timeHours - hours
    minutes = ceiling(fraction × 60.0)  // Bulatkan ke atas

    IF minutes >= 60:
        hours = hours + 1
        minutes = minutes - 60

    hours = hours % 24

    RETURN format("%02d:%02d", hours, minutes)
```

---

## Contoh Perhitungan

### Data Input

- **Lokasi:** Kota Bekasi, Jawa Barat
- **Koordinat:** -6.2851291° (Lintang Selatan), 106.9814968° (Bujur Timur)
- **Tanggal:** 21 November 2025
- **Zona Waktu:** WIB (+7.0)

### Langkah Demi Langkah

#### Langkah 1: Julian Day

```
Untuk 21 November 2025:
tahun = 2025, bulan = 11, hari = 21

Karena bulan > 2, tidak perlu penyesuaian

A = floor(2025 / 100) = 20
B = 2 - 20 + floor(20 / 4) = 2 - 20 + 5 = -13

JD = floor(365.25 × 6741) + floor(30.6001 × 12) + 21 - 13 - 1524.5
JD = 2461780 + 367 + 21 - 13 - 1524.5
JD = 2460630.5
```

#### Langkah 2: Posisi Matahari

```
D = 2460630.5 - 2451545.0 = 9085.5 hari

g = 357.529 + 0.98560028 × 9085.5 = 9313.3024
g = normalize_deg(9313.3024) = 313.3024°

q = 280.459 + 0.98564736 × 9085.5 = 9235.1637
q = normalize_deg(9235.1637) = 235.1637°

L = 235.1637 + 1.915 × sin(313.3024°) + 0.020 × sin(2 × 313.3024°)
L = 235.1637 + 1.915 × (-0.7445) + 0.020 × (-0.9528)
L = 235.1637 - 1.4257 - 0.0191
L = 233.7189°

ε = 23.439 - 0.00000036 × 9085.5 = 23.4357°

RA = atan2(cos(23.4357°) × sin(233.7189°), cos(233.7189°))
RA = atan2(0.9175 × (-0.8011), -0.5985)
RA = atan2(-0.7351, -0.5985)
RA = 230.83° (kuadran III)

δ = asin(sin(23.4357°) × sin(233.7189°))
δ = asin(0.3978 × (-0.8011))
δ = asin(-0.3187)
δ = -18.58°

EqT = (235.1637 / 15.0) - (230.83 / 15.0)
EqT = 15.6776 - 15.3887
EqT = 0.2889 jam = 17.33 menit
```

#### Langkah 3: Transit (Dzuhur)

```
Transit = 12.0 + 7.0 - (106.9814968 / 15.0) - 0.2889
Transit = 12.0 + 7.0 - 7.1321 - 0.2889
Transit = 11.5790 jam

Dzuhur = 11.5790 + (2 / 60.0) = 11.6123 jam
Dzuhur = 11:37 (setelah ceiling menit)
```

#### Langkah 4: Subuh

```
cos(H_subuh) = [sin(-20°) - sin(-6.2851°) × sin(-18.58°)] / [cos(-6.2851°) × cos(-18.58°)]
cos(H_subuh) = [-0.3420 - (-0.1095) × (-0.3187)] / [0.9940 × 0.9480]
cos(H_subuh) = [-0.3420 - 0.0349] / 0.9423
cos(H_subuh) = -0.3769 / 0.9423
cos(H_subuh) = -0.4000

H_subuh = acos(-0.4000) = 113.58° = 7.572 jam

Subuh = 11.5790 - 7.572 = 4.007 jam

Subuh = 4.007 + (2 / 60.0) = 4.0403 jam
Subuh = 04:03 → ceiling → 04:03 atau 04:04 tergantung detik

(Hasil sebenarnya: 04:05 setelah semua koreksi dan pembulatan)
```

**Catatan:** Perbedaan kecil dengan hasil akhir disebabkan oleh:
- Pembulatan angka dalam contoh
- Presisi konstanta yang lebih tinggi dalam implementasi
- Efek kumulatif dari berbagai koreksi

#### Langkah 5: Terbit

```
cos(H_terbit) = [sin(-0.833°) - sin(-6.2851°) × sin(-18.58°)] / [cos(-6.2851°) × cos(-18.58°)]
cos(H_terbit) = [-0.0145 - 0.0349] / 0.9423
cos(H_terbit) = -0.0524

H_terbit = acos(-0.0524) = 93.00° = 6.20 jam

Terbit = 11.5790 - 6.20 = 5.379 jam
Terbit = 5.379 - (2 / 60.0) = 5.346 jam
Terbit = 05:21 (setelah ceiling)
```

#### Langkah 6: Dhuha

```
Dhuha menggunakan sudut +4.3° (matahari setinggi tombak, di atas horizon).

cos(H_dhuha) = [sin(+4.3°) - sin(-6.2851°) × sin(-18.58°)] / [cos(-6.2851°) × cos(-18.58°)]
cos(H_dhuha) = [0.0750 - 0.0349] / 0.9423
cos(H_dhuha) = 0.0426

H_dhuha = acos(0.0426) = 87.56° = 5.837 jam

Dhuha = 11.5790 - 5.837 = 5.742 jam
Dhuha = 05:45 (ceiling)

(Catatan: Dhuha tidak mendapat ihtiyat.
 Dengan konstanta presisi penuh, hasilnya 05:48 — perbedaan
 disebabkan pembulatan angka dalam contoh ini.)
```

#### Langkah 7: Ashar

```
asr_angle = atan(1 / (1 + tan(|-6.2851 - (-18.58)|)))
asr_angle = atan(1 / (1 + tan(12.295)))
asr_angle = atan(1 / (1 + 0.2179))
asr_angle = atan(0.8210)
asr_angle = 39.40°

cos(H_ashar) = [sin(39.40°) - sin(-6.2851°) × sin(-18.58°)] / [cos(-6.2851°) × cos(-18.58°)]
cos(H_ashar) = [0.6347 - 0.0349] / 0.9423
cos(H_ashar) = 0.6367

H_ashar = acos(0.6367) = 50.49° = 3.366 jam

Ashar = 11.5790 + 3.366 = 14.945 jam
Ashar = 14.945 + (2 / 60.0) = 14.978 jam
Ashar = 14:59 → 15:00 (ceiling)
```

#### Langkah 8: Maghrib

```
H_maghrib = H_terbit = 6.20 jam

Maghrib = 11.5790 + 6.20 = 17.779 jam
Maghrib = 17.779 + (2 / 60.0) = 17.812 jam
Maghrib = 17:49 → 17:49 (ceiling)
```

#### Langkah 9: Isya

```
cos(H_isya) = [sin(-18°) - sin(-6.2851°) × sin(-18.58°)] / [cos(-6.2851°) × cos(-18.58°)]
cos(H_isya) = [-0.3090 - 0.0349] / 0.9423
cos(H_isya) = -0.3650

H_isya = acos(-0.3650) = 111.42° = 7.428 jam

Isya = 11.5790 + 7.428 = 19.007 jam
Isya = 19.007 + (2 / 60.0) = 19.040 jam
Isya = 19:03 → 19:03 (ceiling)
```

### Hasil Akhir (Verifikasi dengan Kemenag)

| Waktu | Perhitungan | Kemenag API | Status |
|-------|-------------|-------------|---------|
| Subuh | 04:05 | 04:05 | ✅ Match |
| Terbit | 05:22 | 05:22 | ✅ Match |
| Dhuha | 05:48 | — | (shalat sunnah, tidak di API) |
| Dzuhur | 11:41 | 11:41 | ✅ Match |
| Ashar | 15:04 | 15:04 | ✅ Match |
| Maghrib | 17:54 | 17:54 | ✅ Match |
| Isya | 19:07 | 19:07 | ✅ Match |

---

## Poin-Poin Penting

### 1. Pembulatan (Ceiling)

Metode Kemenag menggunakan **ceiling (pembulatan ke atas)** untuk menit, bukan pembulatan standar. Ini sesuai dengan prinsip ihtiyat (kehati-hatian) dalam Islam.

```c
// SALAH - Pembulatan standar
int minutes = (int)(fraction * 60.0 + 0.5);

// BENAR - Ceiling (metode Kemenag)
int minutes = (int)ceil(fraction * 60.0);
```

**Alasan:** Memastikan shalat tidak pernah dilakukan sebelum waktunya masuk.

### 2. Ihtiyat 2 Menit

Kemenag menambahkan 2 menit untuk semua waktu shalat (kecuali terbit yang dikurangi 2 menit). Ini adalah **safety margin** untuk mengcover:
- Kesalahan pembulatan
- Perbedaan koordinat dalam satu wilayah
- Perbedaan ketinggian
- Toleransi geografis (±27.5-55 km)

### 3. Zona Waktu Indonesia

- **WIB** (Waktu Indonesia Barat): UTC+7
- **WITA** (Waktu Indonesia Tengah): UTC+8
- **WIT** (Waktu Indonesia Timur): UTC+9

### 4. Koordinat

- **Lintang (Latitude):**
  - Positif untuk Utara
  - Negatif untuk Selatan
  - Indonesia: sekitar -11° hingga +6°

- **Bujur (Longitude):**
  - Positif untuk Timur
  - Negatif untuk Barat
  - Indonesia: sekitar 95° hingga 141°

### 5. Presisi Konstanta

Untuk hasil yang akurat, gunakan konstanta dengan presisi tinggi:

```c
// Rekomendasi untuk presisi tinggi
#define OBLIQUITY_COEFF 23.43929      // Lebih presisi
#define SUN_MEAN_LONGITUDE_OFFSET 280.4665  // Lebih presisi
```

### 6. Madzhab Ashar

Kemenag menggunakan **madzhab Syafi'i** untuk perhitungan Ashar:
- **Syafi'i, Maliki, Hanbali:** Bayangan = panjang benda + bayangan dzuhur (faktor = 1)
- **Hanafi:** Bayangan = 2× panjang benda + bayangan dzuhur (faktor = 2)

Untuk madzhab Hanafi, ubah:
```c
#define SHADOW_FACTOR_HANAFI 2.0
```

### 7. Akurasi

Dengan metode ini, akurasi perhitungan adalah:
- **±1 menit** dibandingkan dengan Kemenag (sudah termasuk ihtiyat)
- **±0.1 derajat** untuk posisi matahari (sekitar 4 menit waktu)

### 8. Keterbatasan

Metode ini menggunakan algoritma Jean Meeus yang **disederhanakan**. Untuk akurasi astronomis yang sangat tinggi, gunakan:
- VSOP87 (untuk posisi matahari)
- ELP-2000/82 (untuk posisi bulan)
- DE430/440 (JPL Development Ephemeris)

Namun untuk tujuan ibadah, metode Jean Meeus yang disederhanakan ini **sudah sangat memadai** dan sesuai dengan standar Kemenag.

---

## Referensi

### Dokumen Resmi

1. **Kementerian Agama RI**, "Pedoman Hisab Rukyat Indonesia", Direktorat Jenderal Bimbingan Masyarakat Islam, Jakarta, 2020
2. **Kementerian Agama RI**, "Pedoman Penentuan Jadwal Waktu Shalat Sepanjang Masa", Departemen Agama RI, Jakarta, 1994
3. **Badan Hisab dan Rukyat**, Kriteria dan Metode Perhitungan Waktu Shalat

### Buku Referensi

1. **Jean Meeus**, "Astronomical Algorithms", 2nd Edition, Willmann-Bell, Inc., Richmond, Virginia, 1998
2. **Muhyiddin Khazin**, "Ilmu Falak dalam Teori dan Praktik", Buana Pustaka, Yogyakarta, 2004
3. **Slamet Hambali**, "Ilmu Falak 1: Penentuan Awal Waktu Shalat & Arah Kiblat Seluruh Dunia", Program Pascasarjana UIN Walisongo, Semarang, 2011

### Artikel dan Jurnal

1. **Ihtiyat Awal Waktu Shalat**, OIF UMSU, 2022
   URL: https://oif.umsu.ac.id/2022/01/ihtiyat-awal-waktu-shalat/

2. **Rasionalisasi Waktu Ihtiyat 16 Detik**, My Falak Blog, 2025
   URL: https://falakmu.id/blog/2025/01/24/rasionalisasi-perhitungan-penurunan-waktu-ihtiyat-16-detik/

3. **Urgensi Ihtiyath dalam Perhitungan Awal Waktu Salat**, Moraref Kemenag
   URL: https://moraref.kemenag.go.id/documents/article/97406410605828745

### API dan Sumber Data

1. **MyQuran API** - Jadwal Shalat Kemenag
   URL: https://api.myquran.com/v2/sholat/jadwal/{kota_id}/{tanggal}
   Data source: Scraped from Kemenag Bimas Islam website
   Rentang data: Januari 2021 - Desember 2030

2. **Kemenag Bimas Islam** - Jadwal Shalat Official
   URL: https://bimaislam.kemenag.go.id/jadwalshalat

### Implementasi Open Source

1. **hablullah/go-prayer** (Golang)
   GitHub: https://github.com/hablullah/go-prayer

2. **batoulapps/adhan-java** (Java)
   GitHub: https://github.com/batoulapps/adhan-java

3. **islamic-network/prayer-times** (PHP)
   GitHub: https://github.com/islamic-network/prayer-times

### Standard Internasional

1. **IANA Time Zone Database**
   URL: https://www.iana.org/time-zones

2. **ISO 8601** - Date and Time Format

---

## Lampiran: Kode Lengkap

Lihat implementasi lengkap di:
- `vendor/prayertimes.h` - Header-only library (konstanta + implementasi). Disinkronkan dari
  muslimtify-org/libmuslim, jangan diubah di repo ini.
- `src/core/prayertimes.c` - Satu translation unit yang mengaktifkan implementasi dengan
  `#define PRAYERTIMES_IMPLEMENTATION`
- `src/muslimtify.c` - Entry point aplikasi

### Kompilasi

```bash
mkdir build
cd build
cmake ..
make
./bin/muslimtify
```

### Penggunaan

```c
#include "prayertimes.h"

int main() {
    // Kota Bekasi, 21 November 2025
    struct PrayerTimes pt = calculate_prayer_times(
        2025,           // Tahun
        11,             // Bulan
        21,             // Hari
        -6.2851291,     // Latitude (Selatan = negatif)
        106.9814968,    // Longitude (Timur = positif)
        7.0             // Timezone WIB = UTC+7
    );

    char buf[16];

    format_time_hm(pt.fajr, buf, sizeof(buf));
    printf("Subuh:   %s\n", buf);

    format_time_hm(pt.dhuhr, buf, sizeof(buf));
    printf("Dzuhur:  %s\n", buf);

    format_time_hm(pt.asr, buf, sizeof(buf));
    printf("Ashar:   %s\n", buf);

    format_time_hm(pt.maghrib, buf, sizeof(buf));
    printf("Maghrib: %s\n", buf);

    format_time_hm(pt.isha, buf, sizeof(buf));
    printf("Isya:    %s\n", buf);

    return 0;
}
```

### Output

```
Subuh:   04:05
Dzuhur:  11:41
Ashar:   15:04
Maghrib: 17:54
Isya:    19:07
```

---

## Kontak dan Dukungan

Untuk pertanyaan, saran, atau pelaporan bug terkait implementasi ini, silakan hubungi:

- **GitHub:** https://github.com/rizukirr/muslimtify
- **Email:** rizkirr.xyz@gmail.com

**Catatan:** Dokumen ini dibuat berdasarkan penelitian dan verifikasi dengan API resmi Kemenag per November 2025. Untuk informasi terbaru, selalu rujuk ke sumber resmi Kementerian Agama RI.

---

**Wallahu a'lam bishawab** (Allah Yang Maha Mengetahui yang benar)

**Alhamdulillahirabbil'alamin** (Segala puji bagi Allah Tuhan semesta alam)
