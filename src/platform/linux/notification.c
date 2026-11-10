#define _XOPEN_SOURCE 700

#include "notification.h"
#include "audio.h"
#include "log.h"
#include "platform.h"
#include <libnotify/notify.h>
#include <signal.h>
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

static int resolve_path(char *dst, size_t dst_size, const char *src) {
  if (src[0] == PLATFORM_PATH_SEP) {
    int n = snprintf(dst, dst_size, "%s", src);
    return (n < 0 || (size_t)n >= dst_size) ? -1 : 0;
  }

  if (realpath(src, dst) == NULL) {
    return -1;
  }

  return 0;
}

// Find an installed asset and write its resolved path into dst. Tried in
// order: /usr/local/share and /usr/share and $XDG_DATA_HOME, each followed by
// share_leaf, then the assets directory beside the binary, then assets/ and
// ../assets/ under the working directory, each followed by asset_name.
static bool find_asset(char *dst, size_t dst_size, const char *share_leaf, const char *asset_name) {
  char exe_assets[PLATFORM_PATH_MAX] = "";
  const char *exe = platform_exe_dir();
  if (exe[0] == '\0' || build_path(exe_assets, sizeof(exe_assets), exe, "../assets") != 0)
    exe_assets[0] = '\0';

  // An empty base is skipped: no XDG_DATA_HOME, or no usable binary directory.
  const struct {
    const char *base;
    const char *leaf;
  } candidates[] = {
      {"/usr/local/share", share_leaf},
      {"/usr/share", share_leaf},
      {getenv("XDG_DATA_HOME"), share_leaf},
      {exe_assets[0] ? exe_assets : NULL, asset_name},
      {"assets", asset_name},
      {"../assets", asset_name},
  };

  for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
    char path[PLATFORM_PATH_MAX];
    if (candidates[i].base == NULL ||
        build_path(path, sizeof(path), candidates[i].base, candidates[i].leaf) != 0) {
      continue;
    }
    if (platform_file_exists(path) && resolve_path(dst, dst_size, path) == 0)
      return true;
  }
  return false;
}

// Get default adhan path, cached once found.
static const char *get_adhan_path(void) {
  static char adhan_path[PLATFORM_PATH_MAX] = {0};
  if (adhan_path[0] != '\0')
    return adhan_path;
  return find_asset(adhan_path, sizeof(adhan_path), "muslimtify/adhan.mp3", "adhan.mp3")
             ? adhan_path
             : NULL;
}

// Get the icon path, cached once found. Falls back to the icon name so the
// system looks it up.
static const char *get_icon_path(void) {
  static char icon_path[PLATFORM_PATH_MAX] = {0};
  if (icon_path[0] != '\0')
    return icon_path;
  return find_asset(icon_path, sizeof(icon_path), "icons/hicolor/128x128/apps/muslimtify.png",
                    "muslimtify.png")
             ? icon_path
             : "muslimtify";
}

int notify_init_once(const char *app_name) {
  return notify_init(app_name);
}

// Show a notification. A failure, such as no notification daemon on the session
// bus, goes to the log handler, which writes to stderr by default, so it reaches the journal under
// the systemd user service.
static void show_notification(NotifyNotification *n) {
  GError *error = NULL;
  if (!notify_notification_show(n, &error)) {
    MT_LOGF(MT_LOG_ERROR, "muslimtify: could not show notification: %s",
            error ? error->message : "unknown error");
    g_clear_error(&error);
  }
}

void notify_send(const char *title, const char *message) {
  NotifyNotification *n = notify_notification_new(title, message, get_icon_path());
  notify_notification_set_timeout(n, 3000);
  show_notification(n);
  g_object_unref(G_OBJECT(n));
}

// Set from a signal handler, read by the poll below. Never cleared: the
// daemon loop exits after the cycle that saw it, and a command line process
// that plays a test adhan never sets it.
static volatile sig_atomic_t g_adhan_interrupt = 0;

void notify_adhan_interrupt(void) {
  g_adhan_interrupt = 1;
}

static gboolean adhan_poll_cb(gpointer user_data) {
  if (g_adhan_interrupt) {
    audio_stop();
    g_main_loop_quit((GMainLoop *)user_data);
  } else if (!audio_is_playing()) {
    g_main_loop_quit((GMainLoop *)user_data);
  }
  return G_SOURCE_CONTINUE;
}

static void adhan_closed_cb(NotifyNotification *n, gpointer user_data) {
  (void)n;
  audio_stop();
  g_main_loop_quit((GMainLoop *)user_data);
}

static void adhan_stop_cb(NotifyNotification *n, char *action, gpointer user_data) {
  (void)n;
  (void)action;
  audio_stop();
  g_main_loop_quit((GMainLoop *)user_data);
}

void notify_adhan(const char *prayer_name, const char *time_str, const char *path) {
  char title[128], message[256];
  snprintf(title, sizeof(title), "Prayer Time: %s", prayer_name);
  snprintf(message, sizeof(message), "It's time for %s prayer\nTime: %s", prayer_name, time_str);

  NotifyNotification *n = notify_notification_new(title, message, get_icon_path());
  notify_notification_set_urgency(n, NOTIFY_URGENCY_CRITICAL);
  notify_notification_set_timeout(n, NOTIFY_EXPIRES_NEVER);
  notify_notification_set_hint(n, "suppress-sound", g_variant_new_boolean(TRUE));

  GMainLoop *loop = g_main_loop_new(NULL, FALSE);
  notify_notification_add_action(n, "stop", "Stop", adhan_stop_cb, loop, NULL);
  g_signal_connect(n, "closed", G_CALLBACK(adhan_closed_cb), loop);

  const char *adhan_path = path[0] != '\0' ? path : get_adhan_path();

  // counldn't play -> show briefly, no loop
  if (audio_start(adhan_path) != 0) {
    show_notification(n);
    g_main_loop_unref(loop);
    g_object_unref(n);
    return;
  }

  show_notification(n);
  guint tick = g_timeout_add(200, adhan_poll_cb, loop);

  g_main_loop_run(loop);

  g_source_remove(tick);
  notify_notification_close(n, NULL);
  audio_stop(); // idempotent, no-op if a callback already stopped
  g_main_loop_unref(loop);
  g_object_unref(n);
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

  notify_prayer_text(prayer_name, time_str, minutes_before, title, sizeof(title), message,
                     sizeof(message));

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

  show_notification(n);
  g_object_unref(G_OBJECT(n));
}

void notify_cleanup(void) {
  notify_uninit();
}

int notify_adhan_stop(void) {
  // On Linux the adhan plays inside the daemon's own process (notify_adhan's
  // GMainLoop), so a separate CLI process cannot reach it; the notification's
  // Stop button handles that in-process. If audio happens to be playing in
  // this process, stop it; otherwise report there is nothing to stop.
  if (audio_is_playing()) {
    audio_stop();
    return 0;
  }
  return -1;
}
