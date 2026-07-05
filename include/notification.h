#ifndef NOTIFICATION_H
#define NOTIFICATION_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize libnotify
 * Returns: 1 on success, 0 on failure
 */
int notify_init_once(const char *app_name);

/**
 * Send a generic notification
 */
void notify_send(const char *title, const char *message);

/**
 * Play an adhan sound notification
 */
void notify_adhan(const char *prayer_name, const char *time_str, const char *path);

/**
 * Send a prayer time notification with formatted time
 * minutes_before: 0 for exact time, >0 for reminder
 * sound_preset: "reminder", "alarm", "default", or NULL to silence.
 *               The backend maps the preset to a platform-specific sound.
 */
void notify_prayer(const char *prayer_name, const char *time_str, int minutes_before,
                   const char *urgency, const char *sound_preset);

/**
 * Cleanup libnotify resources
 */
void notify_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // NOTIFICATION_H
