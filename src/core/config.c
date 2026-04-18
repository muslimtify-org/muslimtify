#define JSON_IMPLEMENTATION
#include "config.h"
#include "json.h"
#include "platform.h"
#include "string_util.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool config_trunc_logged = false;

static void log_truncation(const char *key) {
  if (!config_trunc_logged) {
    fprintf(stderr, "config: value '%s' truncated\n", key);
    config_trunc_logged = true;
  }
}

const char *config_get_path(void) {
  static char config_path[PLATFORM_PATH_MAX] = {0};
  if (config_path[0] != '\0')
    return config_path;

  const char *dir = platform_config_dir();
  if (dir[0] != '\0')
    snprintf(config_path, sizeof(config_path), "%s%cconfig.json", dir, PLATFORM_PATH_SEP);

  return config_path;
}

static int ensure_config_dir(void) {
  const char *dir = platform_config_dir();
  if (dir[0] == '\0')
    return -1;
  if (platform_mkdir_p(dir) != 0) {
    fprintf(stderr, "Error: Cannot create config directory '%s'\n", dir);
    return -1;
  }
  return 0;
}

Config config_default(void) {
  Config cfg = {0};

  // Location defaults
  cfg.auto_detect = true;
  if (!copy_string(cfg.timezone, sizeof(cfg.timezone), "UTC")) {
    log_truncation("timezone");
  }
  cfg.timezone_offset = 0.0;

  // Prayer defaults with reminders [30, 15, 5]
  int default_reminders[] = {30, 15, 5};

  cfg.fajr.enabled = true;
  memcpy(cfg.fajr.reminders, default_reminders, sizeof(default_reminders));
  cfg.fajr.reminder_count = 3;

  cfg.sunrise.enabled = false; // DISABLED BY DEFAULT
  cfg.sunrise.reminder_count = 0;

  cfg.dhuha.enabled = false; // DISABLED BY DEFAULT
  cfg.dhuha.reminder_count = 0;

  cfg.dhuhr.enabled = true;
  memcpy(cfg.dhuhr.reminders, default_reminders, sizeof(default_reminders));
  cfg.dhuhr.reminder_count = 3;

  cfg.asr.enabled = true;
  memcpy(cfg.asr.reminders, default_reminders, sizeof(default_reminders));
  cfg.asr.reminder_count = 3;

  cfg.maghrib.enabled = true;
  memcpy(cfg.maghrib.reminders, default_reminders, sizeof(default_reminders));
  cfg.maghrib.reminder_count = 3;

  cfg.isha.enabled = true;
  memcpy(cfg.isha.reminders, default_reminders, sizeof(default_reminders));
  cfg.isha.reminder_count = 3;

  // Notification defaults
  cfg.notification_timeout = 5000;
  if (!copy_string(cfg.notification_urgency, sizeof(cfg.notification_urgency), "critical")) {
    log_truncation("notification_urgency");
  }
  cfg.notification_sound = true;
  if (!copy_string(cfg.notification_sound_alarm, sizeof(cfg.notification_sound_alarm), "alarm")) {
    log_truncation("notification_sound_alarm");
  }
  if (!copy_string(cfg.notification_sound_reminder, sizeof(cfg.notification_sound_reminder),
                   "reminder")) {
    log_truncation("notification_sound_reminder");
  }
  if (!copy_string(cfg.notification_icon, sizeof(cfg.notification_icon), "muslimtify")) {
    log_truncation("notification_icon");
  }

  // Calculation defaults
  if (!copy_string(cfg.calculation_method, sizeof(cfg.calculation_method), "kemenag")) {
    log_truncation("calculation_method");
  }
  if (!copy_string(cfg.madhab, sizeof(cfg.madhab), "shafi")) {
    log_truncation("madhab");
  }
  cfg.fajr_angle = 0;
  cfg.isha_angle = 0;

  return cfg;
}

static void json_escape_string(FILE *f, const char *s) {
  fputc('"', f);
  for (; *s; s++) {
    switch (*s) {
    case '"':
      fputs("\\\"", f);
      break;
    case '\\':
      fputs("\\\\", f);
      break;
    case '\b':
      fputs("\\b", f);
      break;
    case '\f':
      fputs("\\f", f);
      break;
    case '\n':
      fputs("\\n", f);
      break;
    case '\r':
      fputs("\\r", f);
      break;
    case '\t':
      fputs("\\t", f);
      break;
    default:
      if ((unsigned char)*s < 0x20) {
        fprintf(f, "\\u%04x", (unsigned char)*s);
      } else {
        fputc(*s, f);
      }
      break;
    }
  }
  fputc('"', f);
}

static int write_json_file(FILE *f, const Config *cfg) {
  fprintf(f, "{\n");
  fprintf(f, "  \"location\": {\n");
  fprintf(f, "    \"latitude\": %.6f,\n", cfg->latitude);
  fprintf(f, "    \"longitude\": %.6f,\n", cfg->longitude);
  fprintf(f, "    \"timezone\": ");
  json_escape_string(f, cfg->timezone);
  fprintf(f, ",\n");
  fprintf(f, "    \"timezone_offset\": %.1f,\n", cfg->timezone_offset);
  fprintf(f, "    \"auto_detect\": %s,\n", cfg->auto_detect ? "true" : "false");
  fprintf(f, "    \"city\": ");
  json_escape_string(f, cfg->city);
  fprintf(f, ",\n");
  fprintf(f, "    \"country\": ");
  json_escape_string(f, cfg->country);
  fprintf(f, "\n");
  fprintf(f, "  },\n");

  fprintf(f, "  \"prayers\": {\n");

  const char *prayer_names[] = {"fajr", "sunrise", "dhuha", "dhuhr", "asr", "maghrib", "isha"};
  const PrayerConfig *prayers[] = {&cfg->fajr, &cfg->sunrise, &cfg->dhuha, &cfg->dhuhr,
                                   &cfg->asr,  &cfg->maghrib, &cfg->isha};

  for (int i = 0; i < 7; i++) {
    fprintf(f, "    \"%s\": {\n", prayer_names[i]);
    fprintf(f, "      \"enabled\": %s,\n", prayers[i]->enabled ? "true" : "false");
    fprintf(f, "      \"reminders\": [");
    for (int j = 0; j < prayers[i]->reminder_count; j++) {
      fprintf(f, "%d", prayers[i]->reminders[j]);
      if (j < prayers[i]->reminder_count - 1)
        fprintf(f, ", ");
    }
    fprintf(f, "]\n");
    fprintf(f, "    }%s\n", i < 6 ? "," : "");
  }

  fprintf(f, "  },\n");

  fprintf(f, "  \"notification\": {\n");
  fprintf(f, "    \"timeout\": %d,\n", cfg->notification_timeout);
  fprintf(f, "    \"urgency\": ");
  json_escape_string(f, cfg->notification_urgency);
  fprintf(f, ",\n");
  fprintf(f, "    \"sound\": %s,\n", cfg->notification_sound ? "true" : "false");
  fprintf(f, "    \"sound_alarm\": ");
  json_escape_string(f, cfg->notification_sound_alarm);
  fprintf(f, ",\n");
  fprintf(f, "    \"sound_reminder\": ");
  json_escape_string(f, cfg->notification_sound_reminder);
  fprintf(f, ",\n");
  fprintf(f, "    \"icon\": ");
  json_escape_string(f, cfg->notification_icon);
  fprintf(f, "\n");
  fprintf(f, "  },\n");

  fprintf(f, "  \"calculation\": {\n");
  fprintf(f, "    \"method\": ");
  json_escape_string(f, cfg->calculation_method);
  fprintf(f, ",\n");
  fprintf(f, "    \"madhab\": ");
  json_escape_string(f, cfg->madhab);
  if (strcmp(cfg->calculation_method, "custom") == 0) {
    fprintf(f, ",\n");
    fprintf(f, "    \"fajr_angle\": %.1f,\n", cfg->fajr_angle);
    fprintf(f, "    \"isha_angle\": %.1f\n", cfg->isha_angle);
  } else {
    fprintf(f, "\n");
  }
  fprintf(f, "  }\n");
  fprintf(f, "}\n");

  return ferror(f) ? -1 : 0;
}

int config_save(const Config *cfg) {
  if (ensure_config_dir() != 0) {
    return -1;
  }

  const char *path = config_get_path();
  char tmp_path[PLATFORM_PATH_MAX];
  int n = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
  if (n < 0 || (size_t)n >= sizeof(tmp_path)) {
    fprintf(stderr, "Error: Config path too long\n");
    return -1;
  }

  FILE *f = platform_file_open(tmp_path, "w");
  if (!f) {
    int err = errno;
    char errbuf[128];
    errno_string(err, errbuf, sizeof(errbuf));
    fprintf(stderr, "Error: Cannot write config file: %s\n", errbuf);
    return -1;
  }

  if (write_json_file(f, cfg) != 0 || fflush(f) != 0 || fclose(f) != 0) {
    int err = errno;
    char errbuf[128];
    errno_string(err, errbuf, sizeof(errbuf));
    fprintf(stderr, "Error: Failed to write config file: %s\n", errbuf);
    platform_file_delete(tmp_path);
    return -1;
  }

  if (platform_atomic_rename(tmp_path, path) != 0) {
    int err = errno;
    char errbuf[128];
    errno_string(err, errbuf, sizeof(errbuf));
    fprintf(stderr, "Error: Failed to save config file: %s\n", errbuf);
    platform_file_delete(tmp_path);
    return -1;
  }

  return 0;
}

static char *read_file(const char *path) {
  FILE *f = platform_file_open(path, "r");
  if (!f) {
    return NULL;
  }

  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  if (size < 0) {
    fclose(f);
    return NULL;
  }
  fseek(f, 0, SEEK_SET);

  char *content = malloc((size_t)size + 1);
  if (!content) {
    fclose(f);
    return NULL;
  }

  size_t n = fread(content, 1, (size_t)size, f);
  fclose(f);
  if (n > (size_t)size)
    n = (size_t)size;
  // NOLINTNEXTLINE(clang-analyzer-security.ArrayBound) -- n <= size by fread contract
  content[n] = '\0';

  return content;
}

static void parse_prayer_config(JsonContext *ctx, char *prayer_obj, PrayerConfig *pcfg) {
  char *enabled_str = get_value(ctx, "enabled", prayer_obj);
  if (enabled_str) {
    pcfg->enabled = strcmp(enabled_str, "true") == 0;
  }

  char *reminders_str = get_value(ctx, "reminders", prayer_obj);
  if (reminders_str && reminders_str[0] == '[') {
    // Simple manual parsing of array [30, 15, 5]
    char *p = reminders_str + 1; // Skip '['
    pcfg->reminder_count = 0;

    while (*p && *p != ']' && pcfg->reminder_count < MAX_REMINDERS) {
      // Skip whitespace and commas
      while (*p && (*p == ' ' || *p == ','))
        p++;

      if (*p >= '0' && *p <= '9') {
        int value = (int)strtol(p, NULL, 10);
        if (value > 0) {
          pcfg->reminders[pcfg->reminder_count++] = value;
        }
        // Skip to next number
        while (*p && *p >= '0' && *p <= '9')
          p++;
      } else {
        break;
      }
    }
  }
}

int config_load(Config *cfg) {
  const char *path = config_get_path();

  // Check if file exists
  if (!platform_file_exists(path)) {
    // Config doesn't exist, return default
    *cfg = config_default();
    return 0;
  }

  // Initialize with defaults so partial JSON still has sane values
  *cfg = config_default();

  char *content = read_file(path);
  if (!content) {
    fprintf(stderr, "Error: Cannot read config file\n");
    return -1;
  }

  JsonContext *ctx = json_begin();
  if (!ctx) {
    free(content);
    return -1;
  }

  // Parse location
  char *location = get_value(ctx, "location", content);
  if (location) {
    char *lat_str = get_value(ctx, "latitude", location);
    char *lon_str = get_value(ctx, "longitude", location);
    char *tz_str = get_value(ctx, "timezone", location);
    char *tz_offset_str = get_value(ctx, "timezone_offset", location);
    char *auto_detect_str = get_value(ctx, "auto_detect", location);
    char *city_str = get_value(ctx, "city", location);
    char *country_str = get_value(ctx, "country", location);

    if (lat_str)
      cfg->latitude = strtod(lat_str, NULL);
    if (lon_str)
      cfg->longitude = strtod(lon_str, NULL);
    if (tz_str) {
      if (!copy_string(cfg->timezone, sizeof(cfg->timezone), tz_str)) {
        log_truncation("timezone");
      }
    }
    if (tz_offset_str)
      cfg->timezone_offset = strtod(tz_offset_str, NULL);
    if (auto_detect_str)
      cfg->auto_detect = strcmp(auto_detect_str, "true") == 0;
    if (city_str) {
      if (!copy_string(cfg->city, sizeof(cfg->city), city_str)) {
        log_truncation("city");
      }
    }
    if (country_str) {
      if (!copy_string(cfg->country, sizeof(cfg->country), country_str)) {
        log_truncation("country");
      }
    }
  }

  // Parse prayers
  char *prayers = get_value(ctx, "prayers", content);
  if (prayers) {
    char *fajr = get_value(ctx, "fajr", prayers);
    if (fajr)
      parse_prayer_config(ctx, fajr, &cfg->fajr);

    char *sunrise = get_value(ctx, "sunrise", prayers);
    if (sunrise)
      parse_prayer_config(ctx, sunrise, &cfg->sunrise);

    char *dhuha = get_value(ctx, "dhuha", prayers);
    if (dhuha)
      parse_prayer_config(ctx, dhuha, &cfg->dhuha);

    char *dhuhr = get_value(ctx, "dhuhr", prayers);
    if (dhuhr)
      parse_prayer_config(ctx, dhuhr, &cfg->dhuhr);

    char *asr = get_value(ctx, "asr", prayers);
    if (asr)
      parse_prayer_config(ctx, asr, &cfg->asr);

    char *maghrib = get_value(ctx, "maghrib", prayers);
    if (maghrib)
      parse_prayer_config(ctx, maghrib, &cfg->maghrib);

    char *isha = get_value(ctx, "isha", prayers);
    if (isha)
      parse_prayer_config(ctx, isha, &cfg->isha);
  }

  // Parse notification
  char *notification = get_value(ctx, "notification", content);
  if (notification) {
    char *timeout_str = get_value(ctx, "timeout", notification);
    char *urgency_str = get_value(ctx, "urgency", notification);
    char *sound_str = get_value(ctx, "sound", notification);
    char *sound_alarm_str = get_value(ctx, "sound_alarm", notification);
    char *sound_reminder_str = get_value(ctx, "sound_reminder", notification);
    char *icon_str = get_value(ctx, "icon", notification);

    if (timeout_str)
      cfg->notification_timeout = (int)strtol(timeout_str, NULL, 10);
    if (urgency_str) {
      if (!copy_string(cfg->notification_urgency, sizeof(cfg->notification_urgency), urgency_str)) {
        log_truncation("notification_urgency");
      }
    }
    if (sound_str)
      cfg->notification_sound = strcmp(sound_str, "true") == 0;
    if (sound_alarm_str) {
      if (!copy_string(cfg->notification_sound_alarm, sizeof(cfg->notification_sound_alarm),
                       sound_alarm_str)) {
        log_truncation("notification_sound_alarm");
      }
    }
    if (sound_reminder_str) {
      if (!copy_string(cfg->notification_sound_reminder, sizeof(cfg->notification_sound_reminder),
                       sound_reminder_str)) {
        log_truncation("notification_sound_reminder");
      }
    }
    if (icon_str) {
      if (!copy_string(cfg->notification_icon, sizeof(cfg->notification_icon), icon_str)) {
        log_truncation("notification_icon");
      }
    }
  }

  // Parse calculation
  char *calculation = get_value(ctx, "calculation", content);
  if (calculation) {
    char *method_str = get_value(ctx, "method", calculation);
    char *madhab_str = get_value(ctx, "madhab", calculation);

    if (method_str) {
      if (!copy_string(cfg->calculation_method, sizeof(cfg->calculation_method), method_str)) {
        log_truncation("calculation_method");
      }
    }
    if (madhab_str) {
      if (!copy_string(cfg->madhab, sizeof(cfg->madhab), madhab_str)) {
        log_truncation("madhab");
      }
    }
    char *fajr_angle_str = get_value(ctx, "fajr_angle", calculation);
    char *isha_angle_str = get_value(ctx, "isha_angle", calculation);
    if (fajr_angle_str)
      cfg->fajr_angle = atof(fajr_angle_str);
    if (isha_angle_str)
      cfg->isha_angle = atof(isha_angle_str);
  }

  json_end(ctx);
  free(content);

  return 0;
}

bool config_validate(const Config *cfg) {
  if (!cfg)
    return false;

  // Validate location
  if (cfg->latitude < -90.0 || cfg->latitude > 90.0)
    return false;
  if (cfg->longitude < -180.0 || cfg->longitude > 180.0)
    return false;
  if (cfg->timezone_offset < -12.0 || cfg->timezone_offset > 14.0)
    return false;

  // Validate reminders
  const PrayerConfig *prayers[] = {&cfg->fajr, &cfg->sunrise, &cfg->dhuha, &cfg->dhuhr,
                                   &cfg->asr,  &cfg->maghrib, &cfg->isha};

  for (int i = 0; i < 7; i++) {
    if (prayers[i]->reminder_count < 0 || prayers[i]->reminder_count > MAX_REMINDERS) {
      return false;
    }
    for (int j = 0; j < prayers[i]->reminder_count; j++) {
      if (prayers[i]->reminders[j] < 0 || prayers[i]->reminders[j] > 1440) {
        return false;
      }
    }
  }

  return true;
}

PrayerConfig *config_get_prayer(Config *cfg, const char *prayer_name) {
  if (!cfg || !prayer_name)
    return NULL;

  // Convert to lowercase for comparison
  char name_lower[32];
  copy_string(name_lower, sizeof(name_lower), prayer_name);
  for (int i = 0; name_lower[i]; i++) {
    int tmp = tolower((unsigned char)name_lower[i]);
    name_lower[i] = (char)tmp;
  }

  if (strcmp(name_lower, "fajr") == 0)
    return &cfg->fajr;
  if (strcmp(name_lower, "sunrise") == 0)
    return &cfg->sunrise;
  if (strcmp(name_lower, "dhuha") == 0)
    return &cfg->dhuha;
  if (strcmp(name_lower, "dhuhr") == 0 || strcmp(name_lower, "dhur") == 0)
    return &cfg->dhuhr;
  if (strcmp(name_lower, "asr") == 0)
    return &cfg->asr;
  if (strcmp(name_lower, "maghrib") == 0)
    return &cfg->maghrib;
  if (strcmp(name_lower, "isha") == 0)
    return &cfg->isha;

  return NULL;
}

typedef struct {
  int *reminders;
  int max;
  int count;
} ReminderParseCtx;

static bool reminder_token_cb(const char *token, void *user) {
  ReminderParseCtx *ctx = user;
  if (!token || !ctx) {
    return false;
  }

  while (*token && isspace((unsigned char)*token)) {
    token++;
  }

  if (*token == '\0') {
    return true;
  }

  char *end = NULL;
  long value = strtol(token, &end, 10);
  if (end == token || value <= 0 || value > 1440) {
    return true;
  }

  if (ctx->count >= ctx->max) {
    return false;
  }

  ctx->reminders[ctx->count++] = (int)value;
  return true;
}

int config_parse_reminders(const char *reminder_str, int *reminders, int max_reminders) {
  if (!reminder_str || !reminders)
    return -1;

  if (max_reminders <= 0)
    return 0;

  if (strcmp(reminder_str, "none") == 0 || strcmp(reminder_str, "clear") == 0) {
    return 0;
  }

  char buffer[256];
  ReminderParseCtx ctx = {reminders, max_reminders, 0};
  int parse_result =
      parse_tokens(reminder_str, buffer, sizeof(buffer), ", ", reminder_token_cb, &ctx);
  if (parse_result == -1) {
    return -1;
  }

  return ctx.count;
}

void config_format_reminders(const PrayerConfig *prayer, char *buffer, size_t bufsize) {
  if (!prayer || !buffer || bufsize == 0)
    return;

  buffer[0] = '\0';

  if (prayer->reminder_count == 0) {
    if (!copy_string(buffer, bufsize, "none")) {
      log_truncation("reminders");
    }
    return;
  }

  char temp[16];
  for (int i = 0; i < prayer->reminder_count; i++) {
    snprintf(temp, sizeof(temp), "%d", prayer->reminders[i]);
    if (!append_string(buffer, bufsize, temp)) {
      log_truncation("reminders");
      break;
    }

    if (i < prayer->reminder_count - 1) {
      if (!append_string(buffer, bufsize, ",")) {
        log_truncation("reminders");
        break;
      }
    }
  }
}

MethodParams method_params_from_config(const Config *cfg) {
  CalcMethod method = method_from_string(cfg->calculation_method);
  const MethodParams *base = method_params_get(method);
  MethodParams params = base ? *base : *method_params_get(CALC_KEMENAG);

  if (strcmp(cfg->madhab, "hanafi") == 0)
    params.asr_shadow = ASR_HANAFI;
  else
    params.asr_shadow = ASR_STANDARD;

  if (method == CALC_CUSTOM) {
    if (cfg->fajr_angle > 0)
      params.fajr_angle = cfg->fajr_angle;
    if (cfg->isha_angle > 0)
      params.isha_angle = cfg->isha_angle;
  }

  return params;
}
