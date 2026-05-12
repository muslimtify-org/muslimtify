#ifndef _WIN32
#define _GNU_SOURCE
#endif

#include "location.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int total = 0;
static int failures = 0;

// 2024-01-15 12:00:00 UTC (deep winter for N. hemisphere, peak DST for S.)
#define WINTER ((time_t)1705320000)
// 2024-07-15 12:00:00 UTC (peak DST for N. hemisphere, winter for S.)
#define SUMMER ((time_t)1721044800)

static void check(const char *zone, time_t when, const char *label, double expected) {
  total++;
  double got = parse_timezone_offset(zone, when);
  if (fabs(got - expected) < 1e-6) {
    printf("  PASS: %-22s @ %-22s -> %+.2f\n", zone, label, got);
  } else {
    printf("  FAIL: %-22s @ %-22s -> %+.2f  (expected %+.2f)\n", zone, label, got, expected);
    failures++;
  }
}

static void test_northern_dst(void) {
  printf("\n-- Northern hemisphere with DST --\n");
  check("Africa/Cairo", WINTER, "Jan (winter)", 2.0);
  check("Africa/Cairo", SUMMER, "Jul (DST)", 3.0);
  check("Europe/London", WINTER, "Jan (GMT)", 0.0);
  check("Europe/London", SUMMER, "Jul (BST)", 1.0);
  check("Europe/Paris", WINTER, "Jan (CET)", 1.0);
  check("Europe/Paris", SUMMER, "Jul (CEST)", 2.0);
  check("Europe/Berlin", WINTER, "Jan (CET)", 1.0);
  check("Europe/Berlin", SUMMER, "Jul (CEST)", 2.0);
  check("Europe/Istanbul", WINTER, "Jan (no DST)", 3.0);
  check("Europe/Istanbul", SUMMER, "Jul (no DST)", 3.0);
  check("America/New_York", WINTER, "Jan (EST)", -5.0);
  check("America/New_York", SUMMER, "Jul (EDT)", -4.0);
  check("America/Chicago", WINTER, "Jan (CST)", -6.0);
  check("America/Chicago", SUMMER, "Jul (CDT)", -5.0);
  check("America/Denver", WINTER, "Jan (MST)", -7.0);
  check("America/Denver", SUMMER, "Jul (MDT)", -6.0);
  check("America/Los_Angeles", WINTER, "Jan (PST)", -8.0);
  check("America/Los_Angeles", SUMMER, "Jul (PDT)", -7.0);
}

static void test_no_dst_fixed(void) {
  printf("\n-- Fixed-offset zones (no DST) --\n");
  check("Asia/Jakarta", WINTER, "Jan", 7.0);
  check("Asia/Jakarta", SUMMER, "Jul", 7.0);
  check("Asia/Makassar", WINTER, "Jan", 8.0);
  check("Asia/Makassar", SUMMER, "Jul", 8.0);
  check("Asia/Jayapura", WINTER, "Jan", 9.0);
  check("Asia/Jayapura", SUMMER, "Jul", 9.0);
  check("Asia/Singapore", WINTER, "Jan", 8.0);
  check("Asia/Singapore", SUMMER, "Jul", 8.0);
  check("Asia/Kuala_Lumpur", WINTER, "Jan", 8.0);
  check("Asia/Kuala_Lumpur", SUMMER, "Jul", 8.0);
  check("Asia/Bangkok", WINTER, "Jan", 7.0);
  check("Asia/Bangkok", SUMMER, "Jul", 7.0);
  check("Asia/Tokyo", WINTER, "Jan", 9.0);
  check("Asia/Tokyo", SUMMER, "Jul", 9.0);
  check("Asia/Dubai", WINTER, "Jan", 4.0);
  check("Asia/Dubai", SUMMER, "Jul", 4.0);
  check("Asia/Riyadh", WINTER, "Jan", 3.0);
  check("Asia/Riyadh", SUMMER, "Jul", 3.0);
  check("Asia/Karachi", WINTER, "Jan", 5.0);
  check("Asia/Karachi", SUMMER, "Jul", 5.0);
  check("Asia/Dhaka", WINTER, "Jan", 6.0);
  check("Asia/Dhaka", SUMMER, "Jul", 6.0);
  check("Africa/Nairobi", WINTER, "Jan", 3.0);
  check("Africa/Nairobi", SUMMER, "Jul", 3.0);
  check("Africa/Johannesburg", WINTER, "Jan", 2.0);
  check("Africa/Johannesburg", SUMMER, "Jul", 2.0);
  check("America/Sao_Paulo", WINTER, "Jan", -3.0);
  check("America/Sao_Paulo", SUMMER, "Jul", -3.0);
  check("America/Argentina/Buenos_Aires", WINTER, "Jan", -3.0);
  check("America/Argentina/Buenos_Aires", SUMMER, "Jul", -3.0);
  check("America/Bogota", WINTER, "Jan", -5.0);
  check("America/Bogota", SUMMER, "Jul", -5.0);
}

static void test_fractional_offsets(void) {
  printf("\n-- Fractional-hour offsets --\n");
  check("Asia/Kolkata", WINTER, "Jan (+5:30)", 5.5);
  check("Asia/Kolkata", SUMMER, "Jul (+5:30)", 5.5);
  check("Asia/Kabul", WINTER, "Jan (+4:30)", 4.5);
  check("Asia/Kabul", SUMMER, "Jul (+4:30)", 4.5);
  check("Asia/Yangon", WINTER, "Jan (+6:30)", 6.5);
  check("Asia/Yangon", SUMMER, "Jul (+6:30)", 6.5);
  check("Asia/Kathmandu", WINTER, "Jan (+5:45)", 5.75);
  check("Asia/Kathmandu", SUMMER, "Jul (+5:45)", 5.75);
  check("Asia/Colombo", WINTER, "Jan (+5:30)", 5.5);
  check("Australia/Adelaide", WINTER, "Jan (+10:30 ACDT)", 10.5);
  check("Australia/Adelaide", SUMMER, "Jul (+9:30 ACST)", 9.5);
}

static void test_southern_dst(void) {
  printf("\n-- Southern hemisphere DST (inverted seasons) --\n");
  check("Australia/Sydney", WINTER, "Jan (AEDT/DST)", 11.0);
  check("Australia/Sydney", SUMMER, "Jul (AEST)", 10.0);
  check("Pacific/Auckland", WINTER, "Jan (NZDT)", 13.0);
  check("Pacific/Auckland", SUMMER, "Jul (NZST)", 12.0);
  check("America/Santiago", WINTER, "Jan (CLST)", -3.0);
  check("America/Santiago", SUMMER, "Jul (CLT)", -4.0);
}

static void test_half_hour_dst(void) {
  printf("\n-- Half-hour offset with DST (Newfoundland) --\n");
  check("America/St_Johns", WINTER, "Jan (NST)", -3.5);
  check("America/St_Johns", SUMMER, "Jul (NDT)", -2.5);
}

static void test_utc_and_negative_only(void) {
  printf("\n-- UTC, Pacific negatives, and far east --\n");
  check("UTC", WINTER, "Jan", 0.0);
  check("UTC", SUMMER, "Jul", 0.0);
  check("Etc/UTC", WINTER, "Jan", 0.0);
  check("Pacific/Honolulu", WINTER, "Jan", -10.0);
  check("Pacific/Honolulu", SUMMER, "Jul (no DST)", -10.0);
  check("America/Anchorage", WINTER, "Jan (AKST)", -9.0);
  check("America/Anchorage", SUMMER, "Jul (AKDT)", -8.0);
  check("Pacific/Kiritimati", WINTER, "Jan", 14.0);
  check("Pacific/Apia", SUMMER, "Jul (after 2011 dateline)", 13.0);
}

static void test_edge_cases(void) {
  printf("\n-- Edge cases --\n");

  total++;
  if (fabs(parse_timezone_offset(NULL, WINTER)) < 1e-6) {
    printf("  PASS: NULL tz_name returns 0.0\n");
  } else {
    printf("  FAIL: NULL tz_name did not return 0.0\n");
    failures++;
  }

  // Unknown zone: libc silently treats as UTC. We accept that contract here.
  total++;
  double got = parse_timezone_offset("Not/A_Real_Zone", WINTER);
  if (fabs(got - 0.0) < 1e-6) {
    printf("  PASS: unknown zone falls back to 0.0 (libc default)\n");
  } else {
    printf("  FAIL: unknown zone returned %+.2f (expected 0.0)\n", got);
    failures++;
  }

#ifndef _WIN32
  // TZ leak check (POSIX-specific): a call must restore the previous TZ env.
  // The Windows backend does not touch TZ — skip there.
  total++;
  setenv("TZ", "Asia/Tokyo", 1);
  tzset();
  (void)parse_timezone_offset("America/New_York", WINTER);
  const char *after = getenv("TZ");
  if (after && strcmp(after, "Asia/Tokyo") == 0) {
    printf("  PASS: process-wide TZ restored after call\n");
  } else {
    printf("  FAIL: TZ not restored (now %s)\n", after ? after : "(unset)");
    failures++;
  }
  unsetenv("TZ");
  tzset();
#endif
}

#ifdef _WIN32
#include <wchar.h>
extern const char *windows_zone_to_iana(const wchar_t *win_zone);

static void check_win2iana(const wchar_t *win, const char *expected) {
  total++;
  const char *got = windows_zone_to_iana(win);
  if (expected == NULL) {
    if (got == NULL) {
      wprintf(L"  PASS: %-32ls -> (NULL)\n", win ? win : L"(NULL)");
    } else {
      wprintf(L"  FAIL: %-32ls -> %hs (expected NULL)\n", win ? win : L"(NULL)", got);
      failures++;
    }
    return;
  }
  if (got && strcmp(got, expected) == 0) {
    wprintf(L"  PASS: %-32ls -> %hs\n", win, got);
  } else {
    wprintf(L"  FAIL: %-32ls -> %hs (expected %hs)\n", win, got ? got : "(NULL)", expected);
    failures++;
  }
}

static void test_windows_zone_to_iana(void) {
  printf("\n-- windows_zone_to_iana (every Windows zone in the table) --\n");

  // Africa
  check_win2iana(L"Egypt Standard Time", "Africa/Cairo");
  check_win2iana(L"South Africa Standard Time", "Africa/Johannesburg");
  check_win2iana(L"E. Africa Standard Time", "Africa/Nairobi");

  // Americas
  check_win2iana(L"Alaskan Standard Time", "America/Anchorage");
  check_win2iana(L"Argentina Standard Time", "America/Argentina/Buenos_Aires");
  check_win2iana(L"SA Pacific Standard Time", "America/Bogota");
  check_win2iana(L"Central Standard Time", "America/Chicago");
  check_win2iana(L"Mountain Standard Time", "America/Denver");
  check_win2iana(L"Pacific Standard Time", "America/Los_Angeles");
  check_win2iana(L"Eastern Standard Time", "America/New_York");
  check_win2iana(L"Pacific SA Standard Time", "America/Santiago");
  check_win2iana(L"E. South America Standard Time", "America/Sao_Paulo");
  check_win2iana(L"Newfoundland Standard Time", "America/St_Johns");

  // Asia (duplicates resolve to canonical IANA)
  check_win2iana(L"SE Asia Standard Time", "Asia/Jakarta");
  check_win2iana(L"Sri Lanka Standard Time", "Asia/Colombo");
  check_win2iana(L"Bangladesh Standard Time", "Asia/Dhaka");
  check_win2iana(L"Arabian Standard Time", "Asia/Dubai");
  check_win2iana(L"Tokyo Standard Time", "Asia/Tokyo");
  check_win2iana(L"Afghanistan Standard Time", "Asia/Kabul");
  check_win2iana(L"Pakistan Standard Time", "Asia/Karachi");
  check_win2iana(L"Nepal Standard Time", "Asia/Kathmandu");
  check_win2iana(L"India Standard Time", "Asia/Kolkata");
  check_win2iana(L"Singapore Standard Time", "Asia/Singapore");
  check_win2iana(L"Arab Standard Time", "Asia/Riyadh");
  check_win2iana(L"Myanmar Standard Time", "Asia/Yangon");

  // Australia
  check_win2iana(L"Cen. Australia Standard Time", "Australia/Adelaide");
  check_win2iana(L"AUS Eastern Standard Time", "Australia/Sydney");

  // UTC + Europe
  check_win2iana(L"UTC", "Etc/UTC");
  check_win2iana(L"W. Europe Standard Time", "Europe/Berlin");
  check_win2iana(L"Turkey Standard Time", "Europe/Istanbul");
  check_win2iana(L"GMT Standard Time", "Europe/London");
  check_win2iana(L"Romance Standard Time", "Europe/Paris");

  // Pacific
  check_win2iana(L"Samoa Standard Time", "Pacific/Apia");
  check_win2iana(L"New Zealand Standard Time", "Pacific/Auckland");
  check_win2iana(L"Hawaiian Standard Time", "Pacific/Honolulu");
  check_win2iana(L"Line Islands Standard Time", "Pacific/Kiritimati");

  // Negative cases
  check_win2iana(L"Bogus Standard Time", NULL);
  check_win2iana(NULL, NULL);
}
#endif

static void test_get_system_timezone(void) {
  printf("\n-- get_system_timezone --\n");

  char buf[64] = {0};
  int rc = get_system_timezone(buf, sizeof(buf));

  total++;
  if (rc == 0 && buf[0] != '\0') {
    printf("  PASS: get_system_timezone returned %s\n", buf);
  } else {
    printf("  FAIL: get_system_timezone returned rc=%d buf=\"%s\"\n", rc, buf);
    failures++;
  }

  // Offset round-trip: whatever zone the helper returned must round-trip
  // through parse_timezone_offset to a finite offset within [-12, +14].
  total++;
  double off = parse_timezone_offset(buf, time(NULL));
  if (off >= -12.0 && off <= 14.0) {
    printf("  PASS: %s round-trips to UTC%+.2f\n", buf, off);
  } else {
    printf("  FAIL: %s round-trips to out-of-range UTC%+.2f\n", buf, off);
    failures++;
  }

  // Capacity guard: cap=1 must not write past the buffer.
  total++;
  char tiny[2] = {'X', 'X'};
  int rc2 = get_system_timezone(tiny, 1);
  if (rc2 == -1 && tiny[1] == 'X') {
    printf("  PASS: cap<2 rejected without write\n");
  } else {
    printf("  FAIL: cap<2 not rejected (rc=%d, tiny[1]=%c)\n", rc2, tiny[1]);
    failures++;
  }
}

int main(void) {
  printf("=== parse_timezone_offset tests ===\n");

  test_northern_dst();
  test_no_dst_fixed();
  test_fractional_offsets();
  test_southern_dst();
  test_half_hour_dst();
  test_utc_and_negative_only();
  test_edge_cases();
#ifdef _WIN32
  test_windows_zone_to_iana();
#endif
  test_get_system_timezone();

  printf("\n%d/%d tests passed\n", total - failures, total);
  return failures > 0 ? 1 : 0;
}
