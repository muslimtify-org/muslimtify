#include "check_cycle.h"

#include <stdbool.h>
#include <string.h>

TriggerAction trigger_catchup_action(int trigger_minute, int minutes_before, int current_minute) {
  if (trigger_minute > current_minute)
    return TRIGGER_KEEP;
  if (current_minute - trigger_minute > CATCHUP_MAX_MIN)
    return TRIGGER_DROP;

  // A reminder (minutes_before > 0) is only worth firing late while its
  // prayer, at trigger_minute + minutes_before, is still ahead of now. Its
  // whole point is to arrive before the prayer, so once the prayer has
  // passed the reminder would land beside the adhan it was meant to
  // precede. This never loses a real catch-up: a reminder's lateness
  // exceeds its own prayer's by exactly minutes_before, so whenever a late
  // reminder is still inside the catch-up window, its prayer's trigger is
  // inside it too, by a wider margin, and fires on its own.
  if (minutes_before > 0 && trigger_minute + minutes_before <= current_minute)
    return TRIGGER_DROP;

  return TRIGGER_FIRE;
}

bool cache_is_valid_for_today(const char *cache_date, int trigger_count, const char *today) {
  (void)trigger_count;
  return strcmp(cache_date, today) == 0;
}

bool trigger_plays_adhan(int trigger_minute, int minutes_before, bool adhan_enabled,
                         int current_minute) {
  return minutes_before == 0 && adhan_enabled && current_minute <= trigger_minute;
}
