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

int main(void) {
  printf("=== check_cycle tests ===\n\n");

  // CATCHUP_MAX_MIN is 15 in check_cycle.c.
  check_action(trigger_catchup_action(600, 600), TRIGGER_FIRE, "due exactly now fires");
  check_action(trigger_catchup_action(590, 600), TRIGGER_FIRE,
               "missed 10 min ago fires (within window)");
  check_action(trigger_catchup_action(585, 600), TRIGGER_FIRE,
               "missed exactly 15 min ago fires (window edge)");
  check_action(trigger_catchup_action(584, 600), TRIGGER_DROP,
               "missed 16 min ago dropped (past window)");
  check_action(trigger_catchup_action(601, 600), TRIGGER_KEEP, "future trigger kept");

  printf("\n%d/%d tests passed\n", total - failures, total);
  return failures > 0 ? 1 : 0;
}
