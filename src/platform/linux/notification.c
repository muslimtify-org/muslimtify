#define _XOPEN_SOURCE 700

#include "notification.h"
#include "platform.h"
#include <libnotify/notify.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int build_path(char *dst, size_t dst_size, const char *base, const char *leaf) {
  int n = snprintf(dst, dst_size, "%s%c%s", base, PLATFORM_PATH_SEP, leaf);
  if (n < 0 || (size_t)n >= dst_size) {
    return -1;
  }
  return 0;
}

static int resolve_icon_path(char *dst, size_t dst_size, const char *src) {
  if (src[0] == PLATFORM_PATH_SEP) {
    int n = snprintf(dst, dst_size, "%s", src);
    return (n < 0 || (size_t)n >= dst_size) ? -1 : 0;
  }

  if (realpath(src, dst) == NULL) {
    return -1;
  }

  return 0;
}

// Get the icon path - tries multiple locations
static const char *get_icon_path(void) {
  static char icon_path[PLATFORM_PATH_MAX] = {0};

  // Return cached path if already found
  if (icon_path[0] != '\0') {
    return icon_path;
  }

  // Try different icon locations in order of preference
  const char *possible_paths[] = {"/usr/local/share/icons/hicolor/128x128/apps/muslimtify.png",
                                  "/usr/share/icons/hicolor/128x128/apps/muslimtify.png",
                                  NULL,
                                  NULL,
                                  "assets/muslimtify.png",
                                  "../assets/muslimtify.png",
                                  NULL};

  // Try XDG_DATA_HOME
  char xdg_path[PLATFORM_PATH_MAX];
  const char *xdg_data = getenv("XDG_DATA_HOME");
  if (xdg_data != NULL && build_path(xdg_path, sizeof(xdg_path), xdg_data,
                                     "icons/hicolor/128x128/apps/muslimtify.png") == 0) {
    possible_paths[2] = xdg_path;
  }

  // Try relative to binary location
  char assets_path[PLATFORM_PATH_MAX];
  const char *exe = platform_exe_dir();
  if (exe[0] != '\0' &&
      build_path(assets_path, sizeof(assets_path), exe, "../assets/muslimtify.png") == 0) {
    possible_paths[3] = assets_path;
  }

  // Check each path (NULL entries are skipped, not sentinels)
  size_t path_count = sizeof(possible_paths) / sizeof(possible_paths[0]);
  for (size_t i = 0; i < path_count; i++) {
    if (possible_paths[i] == NULL) {
      continue;
    }
    if (platform_file_exists(possible_paths[i])) {
      if (resolve_icon_path(icon_path, sizeof(icon_path), possible_paths[i]) == 0) {
        return icon_path;
      }
    }
  }

  // Fallback to icon name (let system find it)
  return "muslimtify";
}

int notify_init_once(const char *app_name) {
  return notify_init(app_name);
}

void notify_send(const char *title, const char *message) {
  NotifyNotification *n = notify_notification_new(title, message, get_icon_path());
  notify_notification_set_timeout(n, 3000);
  notify_notification_show(n, NULL);
  g_object_unref(G_OBJECT(n));
}

// Map a sound preset to a freedesktop XDG sound-name.
// Returns NULL for "default" (no hint — daemon picks its own) or unknown preset.
static const char *sound_preset_to_xdg_name(const char *preset) {
  if (!preset)
    return NULL;
  if (strcmp(preset, "reminder") == 0)
    return "message-new-instant";
  if (strcmp(preset, "alarm") == 0)
    return "alarm-clock-elapsed";
  return NULL; // "default" or unknown → let daemon decide
}

void notify_prayer(const char *prayer_name, const char *time_str, int minutes_before,
                   const char *urgency_str, const char *sound_preset) {
  char title[128];
  char message[256];

  if (minutes_before == 0) {
    // Exact prayer time notification
    snprintf(title, sizeof(title), "Prayer Time: %s", prayer_name);
    snprintf(message, sizeof(message), "It's time for %s prayer\nTime: %s", prayer_name, time_str);
  } else {
    // Reminder notification
    snprintf(title, sizeof(title), "Prayer Reminder: %s", prayer_name);
    snprintf(message, sizeof(message), "%s prayer in %d minutes\nTime: %s", prayer_name,
             minutes_before, time_str);
  }

  const char *icon = get_icon_path();
  NotifyNotification *n = notify_notification_new(title, message, icon);

  notify_notification_set_timeout(n, 5000);

  NotifyUrgency urgency = NOTIFY_URGENCY_CRITICAL;
  if (urgency_str && strcmp(urgency_str, "low") == 0) {
    urgency = NOTIFY_URGENCY_LOW;
  } else if (urgency_str && strcmp(urgency_str, "normal") == 0) {
    urgency = NOTIFY_URGENCY_NORMAL;
  }
  notify_notification_set_urgency(n, urgency);

  // Sound handling: NULL preset → explicitly suppress; otherwise set sound-name
  // hint if we have a mapping, else let the daemon pick its default.
  if (sound_preset == NULL) {
    notify_notification_set_hint(n, "suppress-sound", g_variant_new_boolean(TRUE));
  } else {
    const char *sound_name = sound_preset_to_xdg_name(sound_preset);
    if (sound_name) {
      notify_notification_set_hint(n, "sound-name", g_variant_new_string(sound_name));
    }
  }

  notify_notification_show(n, NULL);
  g_object_unref(G_OBJECT(n));
}

void notify_cleanup(void) {
  notify_uninit();
}
