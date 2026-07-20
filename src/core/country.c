#include "country.h"
#include "core/country_internal.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

// ISO 3166-1 alpha-2 codes, sorted ascending by `code` for bsearch. `method`
// is the best-fit calculation method per country (CALC_MWL where none is
// dedicated). Non-static so the test-only country_internal.h can expose it.
const Country ISO_3166_1_ALPHA2[] = {
    {"AD", CALC_MWL},     {"AE", CALC_DUBAI},     {"AF", CALC_KARACHI}, {"AG", CALC_MWL},
    {"AI", CALC_MWL},     {"AL", CALC_MWL},       {"AM", CALC_MWL},     {"AO", CALC_MWL},
    {"AQ", CALC_MWL},     {"AR", CALC_MWL},       {"AS", CALC_MWL},     {"AT", CALC_MWL},
    {"AU", CALC_MWL},     {"AW", CALC_MWL},       {"AX", CALC_MWL},     {"AZ", CALC_MWL},
    {"BA", CALC_MWL},     {"BB", CALC_MWL},       {"BD", CALC_KARACHI}, {"BE", CALC_MWL},
    {"BF", CALC_MWL},     {"BG", CALC_MWL},       {"BH", CALC_GULF},    {"BI", CALC_MWL},
    {"BJ", CALC_MWL},     {"BL", CALC_MWL},       {"BM", CALC_MWL},     {"BN", CALC_JAKIM},
    {"BO", CALC_MWL},     {"BQ", CALC_MWL},       {"BR", CALC_MWL},     {"BS", CALC_MWL},
    {"BT", CALC_MWL},     {"BV", CALC_MWL},       {"BW", CALC_MWL},     {"BY", CALC_MWL},
    {"BZ", CALC_MWL},     {"CA", CALC_ISNA},      {"CC", CALC_MWL},     {"CD", CALC_MWL},
    {"CF", CALC_MWL},     {"CG", CALC_MWL},       {"CH", CALC_MWL},     {"CI", CALC_MWL},
    {"CK", CALC_MWL},     {"CL", CALC_MWL},       {"CM", CALC_MWL},     {"CN", CALC_MWL},
    {"CO", CALC_MWL},     {"CR", CALC_MWL},       {"CU", CALC_MWL},     {"CV", CALC_MWL},
    {"CW", CALC_MWL},     {"CX", CALC_MWL},       {"CY", CALC_MWL},     {"CZ", CALC_MWL},
    {"DE", CALC_MWL},     {"DJ", CALC_MWL},       {"DK", CALC_MWL},     {"DM", CALC_MWL},
    {"DO", CALC_MWL},     {"DZ", CALC_ALGERIA},   {"EC", CALC_MWL},     {"EE", CALC_MWL},
    {"EG", CALC_EGYPT},   {"EH", CALC_MWL},       {"ER", CALC_MWL},     {"ES", CALC_MWL},
    {"ET", CALC_MWL},     {"FI", CALC_MWL},       {"FJ", CALC_MWL},     {"FK", CALC_MWL},
    {"FM", CALC_MWL},     {"FO", CALC_MWL},       {"FR", CALC_FRANCE},  {"GA", CALC_MWL},
    {"GB", CALC_MWL},     {"GD", CALC_MWL},       {"GE", CALC_MWL},     {"GF", CALC_MWL},
    {"GG", CALC_MWL},     {"GH", CALC_MWL},       {"GI", CALC_MWL},     {"GL", CALC_MWL},
    {"GM", CALC_MWL},     {"GN", CALC_MWL},       {"GP", CALC_MWL},     {"GQ", CALC_MWL},
    {"GR", CALC_MWL},     {"GS", CALC_MWL},       {"GT", CALC_MWL},     {"GU", CALC_MWL},
    {"GW", CALC_MWL},     {"GY", CALC_MWL},       {"HK", CALC_MWL},     {"HM", CALC_MWL},
    {"HN", CALC_MWL},     {"HR", CALC_MWL},       {"HT", CALC_MWL},     {"HU", CALC_MWL},
    {"ID", CALC_KEMENAG}, {"IE", CALC_MWL},       {"IL", CALC_MWL},     {"IM", CALC_MWL},
    {"IN", CALC_KARACHI}, {"IO", CALC_MWL},       {"IQ", CALC_MWL},     {"IR", CALC_MWL},
    {"IS", CALC_MWL},     {"IT", CALC_MWL},       {"JE", CALC_MWL},     {"JM", CALC_MWL},
    {"JO", CALC_JORDAN},  {"JP", CALC_MWL},       {"KE", CALC_MWL},     {"KG", CALC_MWL},
    {"KH", CALC_MWL},     {"KI", CALC_MWL},       {"KM", CALC_MWL},     {"KN", CALC_MWL},
    {"KP", CALC_MWL},     {"KR", CALC_MWL},       {"KW", CALC_KUWAIT},  {"KY", CALC_MWL},
    {"KZ", CALC_MWL},     {"LA", CALC_MWL},       {"LB", CALC_MWL},     {"LC", CALC_MWL},
    {"LI", CALC_MWL},     {"LK", CALC_MWL},       {"LR", CALC_MWL},     {"LS", CALC_MWL},
    {"LT", CALC_MWL},     {"LU", CALC_MWL},       {"LV", CALC_MWL},     {"LY", CALC_EGYPT},
    {"MA", CALC_MOROCCO}, {"MC", CALC_MWL},       {"MD", CALC_MWL},     {"ME", CALC_MWL},
    {"MF", CALC_MWL},     {"MG", CALC_MWL},       {"MH", CALC_MWL},     {"MK", CALC_MWL},
    {"ML", CALC_MWL},     {"MM", CALC_MWL},       {"MN", CALC_MWL},     {"MO", CALC_MWL},
    {"MP", CALC_MWL},     {"MQ", CALC_MWL},       {"MR", CALC_MWL},     {"MS", CALC_MWL},
    {"MT", CALC_MWL},     {"MU", CALC_MWL},       {"MV", CALC_MWL},     {"MW", CALC_MWL},
    {"MX", CALC_MWL},     {"MY", CALC_JAKIM},     {"MZ", CALC_MWL},     {"NA", CALC_MWL},
    {"NC", CALC_MWL},     {"NE", CALC_MWL},       {"NF", CALC_MWL},     {"NG", CALC_MWL},
    {"NI", CALC_MWL},     {"NL", CALC_MWL},       {"NO", CALC_MWL},     {"NP", CALC_MWL},
    {"NR", CALC_MWL},     {"NU", CALC_MWL},       {"NZ", CALC_MWL},     {"OM", CALC_GULF},
    {"PA", CALC_MWL},     {"PE", CALC_MWL},       {"PF", CALC_MWL},     {"PG", CALC_MWL},
    {"PH", CALC_MWL},     {"PK", CALC_KARACHI},   {"PL", CALC_MWL},     {"PM", CALC_MWL},
    {"PN", CALC_MWL},     {"PR", CALC_MWL},       {"PS", CALC_JORDAN},  {"PT", CALC_PORTUGAL},
    {"PW", CALC_MWL},     {"PY", CALC_MWL},       {"QA", CALC_QATAR},   {"RE", CALC_MWL},
    {"RO", CALC_MWL},     {"RS", CALC_MWL},       {"RU", CALC_RUSSIA},  {"RW", CALC_MWL},
    {"SA", CALC_MAKKAH},  {"SB", CALC_MWL},       {"SC", CALC_MWL},     {"SD", CALC_EGYPT},
    {"SE", CALC_MWL},     {"SG", CALC_SINGAPORE}, {"SH", CALC_MWL},     {"SI", CALC_MWL},
    {"SJ", CALC_MWL},     {"SK", CALC_MWL},       {"SL", CALC_MWL},     {"SM", CALC_MWL},
    {"SN", CALC_MWL},     {"SO", CALC_MWL},       {"SR", CALC_MWL},     {"SS", CALC_MWL},
    {"ST", CALC_MWL},     {"SV", CALC_MWL},       {"SX", CALC_MWL},     {"SY", CALC_MWL},
    {"SZ", CALC_MWL},     {"TC", CALC_MWL},       {"TD", CALC_MWL},     {"TF", CALC_MWL},
    {"TG", CALC_MWL},     {"TH", CALC_MWL},       {"TJ", CALC_MWL},     {"TK", CALC_MWL},
    {"TL", CALC_MWL},     {"TM", CALC_MWL},       {"TN", CALC_TUNISIA}, {"TO", CALC_MWL},
    {"TR", CALC_TURKEY},  {"TT", CALC_MWL},       {"TV", CALC_MWL},     {"TW", CALC_MWL},
    {"TZ", CALC_MWL},     {"UA", CALC_MWL},       {"UG", CALC_MWL},     {"UM", CALC_MWL},
    {"US", CALC_ISNA},    {"UY", CALC_MWL},       {"UZ", CALC_MWL},     {"VA", CALC_MWL},
    {"VC", CALC_MWL},     {"VE", CALC_MWL},       {"VG", CALC_MWL},     {"VI", CALC_MWL},
    {"VN", CALC_MWL},     {"VU", CALC_MWL},       {"WF", CALC_MWL},     {"WS", CALC_MWL},
    {"YE", CALC_MAKKAH},  {"YT", CALC_MWL},       {"ZA", CALC_MWL},     {"ZM", CALC_MWL},
    {"ZW", CALC_MWL},
};

const size_t ISO_3166_1_ALPHA2_COUNT = sizeof(ISO_3166_1_ALPHA2) / sizeof(ISO_3166_1_ALPHA2[0]);

// bsearch comparator: `key` points to a 2-char uppercase NUL-terminated string,
// `elem` points to a Country. Compares against the entry's code.
static int country_code_cmp(const void *key, const void *elem) {
  const char *code = (const char *)key;
  const Country *entry = (const Country *)elem;
  return strcmp(code, entry->code);
}

bool country_is_valid_alpha2(const char *code) {
  if (!code)
    return false;

  // Must be exactly two ASCII letters.
  if (!isalpha((unsigned char)code[0]) || !isalpha((unsigned char)code[1]) || code[2] != '\0')
    return false;

  char key[3];
  key[0] = (char)toupper((unsigned char)code[0]);
  key[1] = (char)toupper((unsigned char)code[1]);
  key[2] = '\0';

  return bsearch(key, ISO_3166_1_ALPHA2, ISO_3166_1_ALPHA2_COUNT, sizeof(ISO_3166_1_ALPHA2[0]),
                 country_code_cmp) != NULL;
}

CalcMethod country_default_method(const char *code) {
  if (!code || !isalpha((unsigned char)code[0]) || !isalpha((unsigned char)code[1]) ||
      code[2] != '\0')
    return CALC_MWL;

  char key[3];
  key[0] = (char)toupper((unsigned char)code[0]);
  key[1] = (char)toupper((unsigned char)code[1]);
  key[2] = '\0';

  const Country *hit = bsearch(key, ISO_3166_1_ALPHA2, ISO_3166_1_ALPHA2_COUNT,
                               sizeof(ISO_3166_1_ALPHA2[0]), country_code_cmp);
  return hit ? hit->method : CALC_MWL;
}
