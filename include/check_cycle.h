#ifndef CHECK_CYCLE_H
#define CHECK_CYCLE_H

#ifdef __cplusplus
extern "C" {
#endif

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
