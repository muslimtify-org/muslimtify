#include "country.h"
#include "prayertimes.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static int total = 0;
static int failures = 0;

static void report_result(const char *label, bool pass) {
  total++;
  if (pass) {
    printf("  PASS: %s\n", label);
  } else {
    printf("  FAIL: %s\n", label);
    failures++;
  }
}

static void expect_valid(const char *code, const char *label) {
  report_result(label, country_is_valid_alpha2(code));
}

static void expect_invalid(const char *code, const char *label) {
  report_result(label, !country_is_valid_alpha2(code));
}

static void test_valid_codes(void) {
  printf("test_valid_codes\n");
  expect_valid("ID", "uppercase ID is valid");
  expect_valid("id", "lowercase id is valid");
  expect_valid("iD", "mixed-case iD is valid");
  expect_valid("US", "US is valid");
  expect_valid("ZW", "ZW is valid");
}

static void test_boundary_codes(void) {
  printf("test_boundary_codes\n");
  expect_valid("AD", "first table entry AD is valid");
  expect_valid("ZW", "last table entry ZW is valid");
}

static void test_invalid_codes(void) {
  printf("test_invalid_codes\n");
  expect_invalid("XX", "unknown XX is invalid");
  expect_invalid("I", "too-short I is invalid");
  expect_invalid("IDN", "too-long IDN is invalid");
  expect_invalid("1D", "non-letter 1D is invalid");
  expect_invalid("", "empty string is invalid");
  expect_invalid(NULL, "NULL is invalid");
}

// country_is_valid_alpha2 uses bsearch, which silently returns wrong answers if
// the table is not sorted ascending by code. Guard that invariant directly.
static void test_table_sorted(void) {
  printf("test_table_sorted\n");
  size_t count = 0;
  const Country *table = country_table(&count);
  report_result("table is non-empty", count > 0);

  bool sorted = true;
  for (size_t i = 1; i < count; ++i) {
    if (strcmp(table[i - 1].code, table[i].code) >= 0) {
      sorted = false;
      printf("  out-of-order at index %zu: '%s' >= '%s'\n", i, table[i - 1].code, table[i].code);
      break;
    }
  }
  report_result("table sorted strictly ascending by code", sorted);
}

static void expect_method(const char *code, CalcMethod expected, const char *label) {
  report_result(label, country_default_method(code) == expected);
}

static void test_country_default_method(void) {
  printf("test_country_default_method\n");
  // Dedicated-method countries.
  expect_method("ID", CALC_KEMENAG, "ID -> kemenag");
  expect_method("MY", CALC_JAKIM, "MY -> jakim");
  expect_method("BN", CALC_JAKIM, "BN -> jakim");
  expect_method("SG", CALC_SINGAPORE, "SG -> singapore");
  expect_method("SA", CALC_MAKKAH, "SA -> makkah");
  expect_method("US", CALC_ISNA, "US -> isna");
  expect_method("PK", CALC_KARACHI, "PK -> karachi");
  expect_method("TR", CALC_TURKEY, "TR -> turkey");
  expect_method("EG", CALC_EGYPT, "EG -> egypt");
  // Case-insensitive.
  expect_method("id", CALC_KEMENAG, "lowercase id -> kemenag");
  expect_method("Sa", CALC_MAKKAH, "mixed-case Sa -> makkah");
  // Fallback to MWL for valid-but-unmapped, unknown, and malformed input.
  expect_method("JP", CALC_MWL, "JP (unmapped) -> mwl");
  expect_method("DE", CALC_MWL, "DE (unmapped) -> mwl");
  expect_method("XX", CALC_MWL, "unknown XX -> mwl");
  expect_method("I", CALC_MWL, "too-short -> mwl");
  expect_method("", CALC_MWL, "empty -> mwl");
  expect_method(NULL, CALC_MWL, "NULL -> mwl");
}

// Exhaustive coverage of country_default_method over the whole table:
//   (1) every one of the 249 codes resolves to its table-declared method,
//   (2) the 29 dedicated (non-MWL) mappings hold by an independent expectation,
//   (3) exactly 29 entries are non-MWL (guards accidental add/remove).
static void test_country_default_method_all(void) {
  printf("test_country_default_method_all\n");

  size_t count = 0;
  const Country *table = country_table(&count);
  report_result("table has 249 entries", count == 249);

  // (1) Accessor agrees with the table for every code.
  bool all_match = true;
  for (size_t i = 0; i < count; ++i) {
    if (country_default_method(table[i].code) != table[i].method) {
      all_match = false;
      printf("  mismatch for '%s': accessor=%d table=%d\n", table[i].code,
             country_default_method(table[i].code), table[i].method);
      break;
    }
  }
  report_result("accessor matches table method for all 249 codes", all_match);

  // (2) Independent expectation for every dedicated (non-MWL) country.
  struct {
    const char *code;
    CalcMethod method;
  } expected[] = {
      {"ID", CALC_KEMENAG}, {"MY", CALC_JAKIM},   {"BN", CALC_JAKIM},   {"SG", CALC_SINGAPORE},
      {"SA", CALC_MAKKAH},  {"YE", CALC_MAKKAH},  {"AE", CALC_DUBAI},   {"QA", CALC_QATAR},
      {"KW", CALC_KUWAIT},  {"BH", CALC_GULF},    {"OM", CALC_GULF},    {"JO", CALC_JORDAN},
      {"PS", CALC_JORDAN},  {"EG", CALC_EGYPT},   {"LY", CALC_EGYPT},   {"SD", CALC_EGYPT},
      {"TN", CALC_TUNISIA}, {"DZ", CALC_ALGERIA}, {"MA", CALC_MOROCCO}, {"TR", CALC_TURKEY},
      {"US", CALC_ISNA},    {"CA", CALC_ISNA},    {"PK", CALC_KARACHI}, {"IN", CALC_KARACHI},
      {"BD", CALC_KARACHI}, {"AF", CALC_KARACHI}, {"FR", CALC_FRANCE},  {"PT", CALC_PORTUGAL},
      {"RU", CALC_RUSSIA},
  };
  size_t expected_count = sizeof(expected) / sizeof(expected[0]);
  report_result("29 dedicated mappings enumerated", expected_count == 29);

  bool dedicated_ok = true;
  for (size_t i = 0; i < expected_count; ++i) {
    if (country_default_method(expected[i].code) != expected[i].method) {
      dedicated_ok = false;
      printf("  wrong method for '%s': got %d, expected %d\n", expected[i].code,
             country_default_method(expected[i].code), expected[i].method);
      break;
    }
  }
  report_result("all 29 dedicated countries map to their method", dedicated_ok);

  // (3) Exactly 29 entries are non-MWL — no mapping silently added or removed.
  size_t non_mwl = 0;
  for (size_t i = 0; i < count; ++i) {
    if (table[i].method != CALC_MWL)
      non_mwl++;
  }
  report_result("exactly 29 non-MWL entries in the table", non_mwl == 29);
}

int main(void) {
  printf("=== country tests ===\n\n");

  test_valid_codes();
  test_boundary_codes();
  test_invalid_codes();
  test_table_sorted();
  test_country_default_method();
  test_country_default_method_all();

  printf("\n%d/%d tests passed\n", total - failures, total);
  return failures > 0 ? 1 : 0;
}
