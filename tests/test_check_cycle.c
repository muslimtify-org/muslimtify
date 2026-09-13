#include "check_cycle.h"

#include <stdio.h>

static int failures = 0;
static int total = 0;

static void check_action(TriggerAction got, TriggerAction want, const char *label) {
  total++;
  if (got == want) {
    printf("  PASS: %s\n", label);
  } else {
    printf("  FAIL: %s — got %d, want %d\n", label, (int)got, (int)want);
    failures++;
  }
}

static void check_bool(bool got, bool want, const char *label) {
  total++;
  if (got == want) {
    printf("  PASS: %s\n", label);
  } else {
    printf("  FAIL: %s — got %d, want %d\n", label, (int)got, (int)want);
    failures++;
  }
}

int main(void) {
  printf("=== check_cycle tests ===\n\n");

  // CATCHUP_MAX_MIN is 15 in check_cycle.h.
  check_action(trigger_catchup_action(600, 0, 600), TRIGGER_FIRE, "due exactly now fires");
  check_action(trigger_catchup_action(590, 0, 600), TRIGGER_FIRE,
               "missed 10 min ago fires (within window)");
  check_action(trigger_catchup_action(585, 0, 600), TRIGGER_FIRE,
               "missed exactly 15 min ago fires (window edge)");
  check_action(trigger_catchup_action(584, 0, 600), TRIGGER_DROP,
               "missed 16 min ago dropped (past window)");
  check_action(trigger_catchup_action(601, 0, 600), TRIGGER_KEEP, "future trigger kept");

  // A reminder's lateness always exceeds its prayer's by exactly
  // minutes_before, since it fires that many minutes before the prayer.
  //
  // clang-format off
  // Mutation record, task 5: deleting the
  // `minutes_before > 0 && trigger_minute + minutes_before <= current_minute`
  // clause from trigger_catchup_action, so a late reminder fires regardless
  // of whether its prayer has passed, was applied, rebuilt, and run under
  // ctest --test-dir build --output-on-failure. check_cycle failed with:
  //   FAIL: reminder 10 min late dropped once its prayer passed 5 min ago — got 1, want 2
  //   FAIL: reminder dropped exactly as its prayer arrives (nothing left to precede) — got 1, want 2
  // 10/12 tests passed. The two checks below are what caught it. Reverted
  // with git checkout, rebuilt, and ctest returned to 15/15 before the next
  // mutant was applied.
  // clang-format on
  check_action(trigger_catchup_action(590, 20, 600), TRIGGER_FIRE,
               "reminder 10 min late fires while its prayer is still 20 min ahead");
  check_action(trigger_catchup_action(590, 5, 600), TRIGGER_DROP,
               "reminder 10 min late dropped once its prayer passed 5 min ago");
  check_action(trigger_catchup_action(590, 10, 600), TRIGGER_DROP,
               "reminder dropped exactly as its prayer arrives (nothing left to precede)");

  // clang-format off
  // Mutation record, task 5: dropping the `current_minute <= trigger_minute`
  // clause from trigger_plays_adhan, so a late prayer recites again, was
  // applied, rebuilt, and run under ctest --test-dir build --output-on-failure.
  // check_cycle failed with:
  //   FAIL: the same trigger one minute late does not recite — got 1, want 0
  // 11/12 tests passed. The check right below is what caught it. Reverted
  // with git checkout, rebuilt, and ctest returned to 15/15 before the next
  // mutant was applied.
  // clang-format on
  check_bool(trigger_plays_adhan(600, 0, true, 600), true,
             "adhan trigger at its own minute recites");
  check_bool(trigger_plays_adhan(600, 0, true, 601), false,
             "the same trigger one minute late does not recite");
  check_bool(trigger_plays_adhan(600, 10, true, 605), false,
             "a reminder never recites regardless of lateness");
  check_bool(trigger_plays_adhan(600, 0, false, 600), false,
             "a trigger with adhan disabled never recites");

  printf("\n%d/%d tests passed\n", total - failures, total);
  return failures > 0 ? 1 : 0;
}
