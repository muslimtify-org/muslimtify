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
  check("Asia/Tehran", WINTER, "Jan (+3:30)", 3.5);
  check("Asia/Tehran", SUMMER, "Jul (+3:30, no DST since 2022)", 3.5);
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

int main(void) {
  printf("=== parse_timezone_offset tests ===\n");

  test_northern_dst();
  test_no_dst_fixed();
  test_fractional_offsets();
  test_southern_dst();
  test_half_hour_dst();
  test_utc_and_negative_only();
  test_edge_cases();

  printf("\n%d/%d tests passed\n", total - failures, total);
  return failures > 0 ? 1 : 0;
}
