#ifndef MUSLIMTIFY_COUNTRY_NAME_H
#define MUSLIMTIFY_COUNTRY_NAME_H

#include <string.h>

typedef struct {
  const char *code; /* ISO 3166-1 alpha-2 */
  const char *name; /* English short name */
} CountryName;

static const CountryName COUNTRY_NAMES[] = {
    {"AE", "United Arab Emirates"},
    {"AF", "Afghanistan"},
    {"AU", "Australia"},
    {"BD", "Bangladesh"},
    {"BH", "Bahrain"},
    {"BN", "Brunei"},
    {"BR", "Brazil"},
    {"CA", "Canada"},
    {"CH", "Switzerland"},
    {"CN", "China"},
    {"DE", "Germany"},
    {"DZ", "Algeria"},
    {"EG", "Egypt"},
    {"ES", "Spain"},
    {"FR", "France"},
    {"GB", "United Kingdom"},
    {"ID", "Indonesia"},
    {"IN", "India"},
    {"IQ", "Iraq"},
    {"IR", "Iran"},
    {"IT", "Italy"},
    {"JO", "Jordan"},
    {"JP", "Japan"},
    {"KR", "South Korea"},
    {"KW", "Kuwait"},
    {"LB", "Lebanon"},
    {"LY", "Libya"},
    {"MA", "Morocco"},
    {"MY", "Malaysia"},
    {"NG", "Nigeria"},
    {"NL", "Netherlands"},
    {"OM", "Oman"},
    {"PH", "Philippines"},
    {"PK", "Pakistan"},
    {"PS", "Palestine"},
    {"PT", "Portugal"},
    {"QA", "Qatar"},
    {"RU", "Russia"},
    {"SA", "Saudi Arabia"},
    {"SD", "Sudan"},
    {"SG", "Singapore"},
    {"TH", "Thailand"},
    {"TN", "Tunisia"},
    {"TR", "Turkey"},
    {"US", "United States"},
    {"YE", "Yemen"},
    {"ZA", "South Africa"},
};

/* Case-insensitive 2-letter code compare (ipinfo returns uppercase, but a
 * hand-edited config might not). */
static inline int countryCodeEquals(const char *a, const char *b) {
  while (*a && *b) {
    char ca = *a >= 'a' && *a <= 'z' ? (char)(*a - 32) : *a;
    char cb = *b >= 'a' && *b <= 'z' ? (char)(*b - 32) : *b;
    if (ca != cb)
      return 0;
    a++;
    b++;
  }
  return *a == '\0' && *b == '\0';
}

/* Returns the full English country name for an ISO 3166-1 alpha-2 code, or the
 * code itself when empty or unrecognized, so callers can use it directly. */
static inline const char *getCountryName(const char *code) {
  if (!code || code[0] == '\0')
    return code;

  size_t size = sizeof(COUNTRY_NAMES) / sizeof(CountryName);
  for (size_t i = 0; i < size; i++) {
    if (countryCodeEquals(code, COUNTRY_NAMES[i].code))
      return COUNTRY_NAMES[i].name;
  }

  return code;
}

#endif // MUSLIMTIFY_COUNTRY_NAME_H
