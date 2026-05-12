// Windows implementation of parse_timezone_offset.
//
// Win32 timezone APIs use Windows zone names ("Egypt Standard Time"), not
// IANA names ("Africa/Cairo") that ipinfo.io returns. We translate via a
// CLDR-derived table covering the zones tests/test_location.c exercises.
// For zones outside this table the function returns 0.0; extend as needed.

#include "location.h"

#include <string.h>
#include <time.h>
#include <wchar.h>
#include <windows.h>

typedef struct {
  const char *iana;
  const wchar_t *win;
} IanaWinPair;

// Roughly alphabetical by IANA name. Where multiple IANA names share a
// Windows zone, the canonical one is listed FIRST so `windows_zone_to_iana`
// returns the expected name (e.g. "Asia/Tokyo" rather than "Asia/Jayapura"
// for "Tokyo Standard Time").
static const IanaWinPair IANA_TO_WIN[] = {
    {"Africa/Cairo", L"Egypt Standard Time"},
    {"Africa/Johannesburg", L"South Africa Standard Time"},
    {"Africa/Nairobi", L"E. Africa Standard Time"},
    {"America/Anchorage", L"Alaskan Standard Time"},
    {"America/Argentina/Buenos_Aires", L"Argentina Standard Time"},
    {"America/Bogota", L"SA Pacific Standard Time"},
    {"America/Chicago", L"Central Standard Time"},
    {"America/Denver", L"Mountain Standard Time"},
    {"America/Los_Angeles", L"Pacific Standard Time"},
    {"America/New_York", L"Eastern Standard Time"},
    {"America/Santiago", L"Pacific SA Standard Time"},
    {"America/Sao_Paulo", L"E. South America Standard Time"},
    {"America/St_Johns", L"Newfoundland Standard Time"},
    {"Asia/Jakarta", L"SE Asia Standard Time"},
    {"Asia/Bangkok", L"SE Asia Standard Time"},
    {"Asia/Colombo", L"Sri Lanka Standard Time"},
    {"Asia/Dhaka", L"Bangladesh Standard Time"},
    {"Asia/Dubai", L"Arabian Standard Time"},
    {"Asia/Tokyo", L"Tokyo Standard Time"},
    {"Asia/Jayapura", L"Tokyo Standard Time"},
    {"Asia/Kabul", L"Afghanistan Standard Time"},
    {"Asia/Karachi", L"Pakistan Standard Time"},
    {"Asia/Kathmandu", L"Nepal Standard Time"},
    {"Asia/Kolkata", L"India Standard Time"},
    {"Asia/Singapore", L"Singapore Standard Time"},
    {"Asia/Kuala_Lumpur", L"Singapore Standard Time"},
    {"Asia/Makassar", L"Singapore Standard Time"},
    {"Asia/Riyadh", L"Arab Standard Time"},
    {"Asia/Yangon", L"Myanmar Standard Time"},
    {"Australia/Adelaide", L"Cen. Australia Standard Time"},
    {"Australia/Sydney", L"AUS Eastern Standard Time"},
    {"Etc/UTC", L"UTC"},
    {"UTC", L"UTC"},
    {"Europe/Berlin", L"W. Europe Standard Time"},
    {"Europe/Istanbul", L"Turkey Standard Time"},
    {"Europe/London", L"GMT Standard Time"},
    {"Europe/Paris", L"Romance Standard Time"},
    {"Pacific/Apia", L"Samoa Standard Time"},
    {"Pacific/Auckland", L"New Zealand Standard Time"},
    {"Pacific/Honolulu", L"Hawaiian Standard Time"},
    {"Pacific/Kiritimati", L"Line Islands Standard Time"},
};

static const wchar_t *iana_to_windows_zone(const char *tz_name) {
  if (!tz_name)
    return NULL;
  for (size_t i = 0; i < sizeof(IANA_TO_WIN) / sizeof(IANA_TO_WIN[0]); ++i) {
    if (strcmp(IANA_TO_WIN[i].iana, tz_name) == 0)
      return IANA_TO_WIN[i].win;
  }
  return NULL;
}

double parse_timezone_offset(const char *tz_name, time_t when) {
  const wchar_t *win_zone = iana_to_windows_zone(tz_name);
  if (!win_zone)
    return 0.0;

  // Find the DYNAMIC_TIME_ZONE_INFORMATION whose key matches.
  DYNAMIC_TIME_ZONE_INFORMATION dtzi;
  DWORD idx = 0;
  int found = 0;
  while (EnumDynamicTimeZoneInformation(idx++, &dtzi) == ERROR_SUCCESS) {
    if (wcscmp(dtzi.TimeZoneKeyName, win_zone) == 0) {
      found = 1;
      break;
    }
  }
  if (!found)
    return 0.0;

  // time_t (Unix epoch seconds, UTC) -> FILETIME (100ns ticks since 1601-01-01).
  // 11644473600 seconds separate 1601-01-01 from 1970-01-01.
  ULONGLONG ticks = ((ULONGLONG)when + 11644473600ULL) * 10000000ULL;
  FILETIME utc_ft;
  utc_ft.dwLowDateTime = (DWORD)(ticks & 0xFFFFFFFFULL);
  utc_ft.dwHighDateTime = (DWORD)(ticks >> 32);

  SYSTEMTIME utc_st;
  if (!FileTimeToSystemTime(&utc_ft, &utc_st))
    return 0.0;

  SYSTEMTIME local_st;
  if (!SystemTimeToTzSpecificLocalTimeEx(&dtzi, &utc_st, &local_st))
    return 0.0;

  // Treat local_st as if it were UTC to recover a tick count; the delta
  // against utc_ft is exactly the offset (DST already baked in by the API).
  FILETIME local_ft;
  if (!SystemTimeToFileTime(&local_st, &local_ft))
    return 0.0;

  ULONGLONG utc_ticks = ((ULONGLONG)utc_ft.dwHighDateTime << 32) | utc_ft.dwLowDateTime;
  ULONGLONG local_ticks = ((ULONGLONG)local_ft.dwHighDateTime << 32) | local_ft.dwLowDateTime;
  LONGLONG diff = (LONGLONG)local_ticks - (LONGLONG)utc_ticks;

  // 10^7 ticks/sec * 3600 sec/hr = 3.6 * 10^10 ticks/hr.
  return (double)diff / 36000000000.0;
}

// Exposed (non-static) so tests can pin specific Windows zone names and
// assert the table-backed reverse mapping. Treat as internal to this TU
// otherwise; callers should prefer `get_system_timezone`.
const char *windows_zone_to_iana(const wchar_t *win_zone) {
  if (!win_zone)
    return NULL;
  for (size_t i = 0; i < sizeof(IANA_TO_WIN) / sizeof(IANA_TO_WIN[0]); ++i) {
    if (wcscmp(IANA_TO_WIN[i].win, win_zone) == 0)
      return IANA_TO_WIN[i].iana;
  }
  return NULL;
}

int get_system_timezone(char *buf, size_t cap) {
  if (!buf || cap < 2)
    return -1;

  DYNAMIC_TIME_ZONE_INFORMATION dtzi;
  DWORD rc = GetDynamicTimeZoneInformation(&dtzi);
  if (rc == TIME_ZONE_ID_INVALID) {
    if (cap >= 4)
      memcpy(buf, "UTC", 4);
    else
      buf[0] = '\0';
    return -1;
  }

  const char *iana = windows_zone_to_iana(dtzi.TimeZoneKeyName);
  if (!iana) {
    if (cap >= 4)
      memcpy(buf, "UTC", 4);
    else
      buf[0] = '\0';
    return -1;
  }

  size_t n = strlen(iana);
  if (n + 1 > cap) {
    buf[0] = '\0';
    return -1;
  }
  memcpy(buf, iana, n + 1);
  return 0;
}
