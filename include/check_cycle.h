#ifndef CHECK_CYCLE_H
#define CHECK_CYCLE_H

#ifdef __cplusplus
extern "C" {
#endif

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
 * Pure function (no I/O) so it can be unit-tested. See TriggerAction.
 */
TriggerAction trigger_catchup_action(int trigger_minute, int current_minute);

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
