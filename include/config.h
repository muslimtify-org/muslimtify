#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_REMINDERS 10 // Maximum reminders per prayer

typedef struct {
  bool enabled;
  int reminders[MAX_REMINDERS]; // Minutes before prayer
  int reminder_count;           // Number of reminders
  int offset;                   // Signed minutes added to the calculated time (-60..60)
} PrayerConfig;

typedef struct {
  // Location
  double latitude;
  double longitude;
  char timezone[64];
  double timezone_offset;
  bool auto_detect;
  char city[128];
  char country[64];

  // Prayers
  PrayerConfig fajr;
  PrayerConfig sunrise;
  PrayerConfig dhuha;
  PrayerConfig dhuhr;
  PrayerConfig asr;
  PrayerConfig maghrib;
  PrayerConfig isha;

  // Notification
  int notification_timeout;
  char notification_urgency[16];
  bool notification_sound;
  char notification_sound_alarm[16];    // preset name for "it's time" notification
  char notification_sound_reminder[16]; // preset name for pre-prayer reminder notifications
  char notification_icon[64];

  // Calculation
  char calculation_method[32];
  char madhab[16];
  double fajr_angle; // custom method only (0 = use method default)
  double isha_angle; // custom method only (0 = use method default)
} Config;

/**
 * Load config from ~/.config/muslimtify/config.json
 * Creates default config if file doesn't exist
 * Returns: 0 on success, -1 on error
 */
int config_load(Config *cfg);

/**
 * Save config to file
 * Returns: 0 on success, -1 on error
 */
int config_save(const Config *cfg);

/**
 * Get default config with sunrise/dhuha disabled
 */
Config config_default(void);

/**
 * Get config file path
 */
const char *config_get_path(void);

/**
 * Validate config structure
 */
bool config_validate(const Config *cfg);

/**
 * Get prayer config by name (case-insensitive)
 * Returns: pointer to PrayerConfig or NULL if not found
 */
PrayerConfig *config_get_prayer(Config *cfg, const char *prayer_name);

/**
 * Parse reminder string (e.g., "30,15,5") into array
 * Returns: number of reminders parsed, -1 on error
 */
int config_parse_reminders(const char *reminder_str, int *reminders, int max_reminders);

/**
 * Format reminders array to string (e.g., "30,15,5")
 */
void config_format_reminders(const PrayerConfig *prayer, char *buffer, size_t bufsize);

#include "prayertimes.h"
MethodParams method_params_from_config(const Config *cfg);

struct PrayerTimes prayer_times_for_config(const Config *cfg, int year, int month, int day);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_H
