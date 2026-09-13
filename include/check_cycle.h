#ifndef CHECK_CYCLE_H
#define CHECK_CYCLE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Fire a missed trigger only if it came due within this many minutes of now.
// Covers any realistic adhan length and short cycle overruns, while dropping
// prayers missed by a long suspend/resume rather than replaying them.
#define CATCHUP_MAX_MIN 15

/**
 * Decision for a single cache trigger given the current minute-of-day.
 * KEEP: trigger is still in the future — leave it in the cache.
 * FIRE: trigger is due now or was missed within the catch-up window — fire it.
 * DROP: trigger was missed longer ago than the catch-up window — remove it
 *       without firing (avoids replaying stale adhans after suspend/resume).
 */
typedef enum {
  TRIGGER_KEEP = 0,
  TRIGGER_FIRE,
  TRIGGER_DROP,
} TriggerAction;

/**
 * Classify a trigger by its scheduled minute-of-day vs. the current minute.
 * minutes_before is 0 for an adhan and positive for a reminder scheduled
 * that many minutes ahead of its prayer. A reminder's whole value is
 * arriving before its prayer, so once minutes_before > 0 it only fires
 * while trigger_minute + minutes_before, the prayer's own minute, is still
 * ahead of current_minute. Past that point it would arrive beside the adhan
 * it was meant to precede, so it is dropped instead of fired late. Nothing
 * is lost by dropping it: a reminder's lateness exceeds its prayer's by
 * exactly minutes_before, so a rescuable late reminder always coexists with
 * a rescuable late adhan, which fires on its own.
 * Pure function (no I/O) so it can be unit-tested. See TriggerAction.
 */
TriggerAction trigger_catchup_action(int trigger_minute, int minutes_before, int current_minute);

/**
 * True when a cache dated cache_date is valid as-is for today. Deliberately
 * ignores trigger_count: a cache carrying today's date is valid even with no
 * triggers left, because an empty list means today's prayers are all done,
 * not that the cache is missing. Requiring a nonzero trigger count here would
 * force a rebuild every cycle after the last trigger fires, and with the
 * widened catch-up gate in cache_build_triggers that rebuild would readmit
 * the trigger that just fired and fire it again, once a minute, forever.
 * trigger_count is taken as a parameter, rather than dropped, so a test can
 * prove it has no effect on the result.
 * Pure function (no I/O) so it can be unit-tested.
 */
bool cache_is_valid_for_today(const char *cache_date, int trigger_count, const char *today);

/**
 * True when this trigger should play the full adhan recitation rather than a
 * plain notification. A prayer announced at its own minute is recited. One
 * announced late is stated, because a recitation beginning after the prayer
 * time has passed describes a moment that is already gone.
 * Pure function (no I/O) so it can be unit-tested.
 */
bool trigger_plays_adhan(int trigger_minute, int minutes_before, bool adhan_enabled,
                         int current_minute);

/**
 * Run one prayer-notification check for the current minute.
 * Loads (or rebuilds) the prayer cache, fires any due adhan/reminder
 * notifications, and prunes elapsed triggers.
 * Returns: 0 on success, non-zero on error.
 */
int run_check_cycle(void);

#ifdef __cplusplus
}
#endif

#endif // CHECK_CYCLE_H
