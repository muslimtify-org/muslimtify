#include "cache.h"
#include "json.h"
#include "platform.h"
#include "prayer_checker.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "string_util.h"

static char cache_path_buf[PLATFORM_PATH_MAX] = {0};
static bool cache_trunc_logged = false;

static void cache_log_trunc(const char *field) {
  if (!cache_trunc_logged) {
    fprintf(stderr, "cache: truncated field %s\n", field ? field : "(unknown)");
    cache_trunc_logged = true;
  }
}

const char *cache_get_path(void) {
  if (cache_path_buf[0] != '\0') {
    return cache_path_buf;
  }

  const char *dir = platform_cache_dir();
  if (dir[0] != '\0') {
    snprintf(cache_path_buf, sizeof(cache_path_buf), "%s%cnext_prayer.json", dir,
             PLATFORM_PATH_SEP);
  }

  return cache_path_buf;
}

static int ensure_cache_dir(void) {
  const char *dir = platform_cache_dir();
  if (dir[0] == '\0')
    return -1;
  if (platform_mkdir_p(dir) != 0)
    return -1;
  return 0;
}

static char *read_file(const char *path) {
  FILE *f = platform_file_open(path, "r");
  if (!f)
    return NULL;

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

int cache_load(PrayerCache *cache) {
  if (!cache)
    return -1;

  const char *path = cache_get_path();
  char *content = read_file(path);
  if (!content)
    return -1;

  JsonContext *ctx = json_begin();
  if (!ctx) {
    free(content);
    return -1;
  }

  memset(cache, 0, sizeof(*cache));

  char *date_str = get_value(ctx, "date", content);
  if (!date_str) {
    json_end(ctx);
    free(content);
    return -1;
  }
  if (!copy_string(cache->date, sizeof(cache->date), date_str)) {
    cache_log_trunc("date");
  }

  char *triggers = get_value(ctx, "triggers", content);
  if (!triggers || triggers[0] != '[') {
    json_end(ctx);
    free(content);
    return -1;
  }

  // Parse trigger array manually
  // Format: [{"prayer":"X","minute":N,"minutes_before":N,"prayer_time":F}, ...]
  char *p = triggers + 1; // skip '['
  cache->trigger_count = 0;

  while (*p && *p != ']' && cache->trigger_count < MAX_TRIGGERS) {
    // Find next '{'
    while (*p && *p != '{')
      p++;
    if (!*p)
      break;

    // Find matching '}'
    char *obj_start = p;
    int depth = 0;
    char *obj_end = NULL;
    for (char *q = p; *q; q++) {
      if (*q == '{')
        depth++;
      else if (*q == '}') {
        depth--;
        if (depth == 0) {
          obj_end = q;
          break;
        }
      }
    }
    if (!obj_end)
      break;

    // Null-terminate the object temporarily
    char saved = *(obj_end + 1);
    *(obj_end + 1) = '\0';

    CacheTrigger *t = &cache->triggers[cache->trigger_count];

    char *prayer = get_value(ctx, "prayer", obj_start);
    if (prayer) {
      if (!copy_string(t->prayer, sizeof(t->prayer), prayer)) {
        cache_log_trunc("prayer");
      }
    }

    char *minute_str = get_value(ctx, "minute", obj_start);
    if (minute_str)
      t->minute = (int)strtol(minute_str, NULL, 10);

    char *mb_str = get_value(ctx, "minutes_before", obj_start);
    if (mb_str)
      t->minutes_before = (int)strtol(mb_str, NULL, 10);

    char *pt_str = get_value(ctx, "prayer_time", obj_start);
    if (pt_str)
      t->prayer_time = strtod(pt_str, NULL);

    cache->trigger_count++;

    *(obj_end + 1) = saved;
    p = obj_end + 1;
  }

  json_end(ctx);
  free(content);
  return 0;
}

int cache_save(const PrayerCache *cache) {
  if (!cache)
    return -1;
  if (ensure_cache_dir() != 0)
    return -1;

  const char *path = cache_get_path();
  char tmp_path[PLATFORM_PATH_MAX + 4];
  snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

  FILE *f = platform_file_open(tmp_path, "w");
  if (!f)
    return -1;

  fprintf(f, "{\n");
  fprintf(f, "  \"date\": \"%s\",\n", cache->date);
  fprintf(f, "  \"triggers\": [\n");

  for (int i = 0; i < cache->trigger_count; i++) {
    const CacheTrigger *t = &cache->triggers[i];
    fprintf(f,
            "    {\"prayer\": \"%s\", \"minute\": %d, "
            "\"minutes_before\": %d, \"prayer_time\": %.4f}%s\n",
            t->prayer, t->minute, t->minutes_before, t->prayer_time,
            i < cache->trigger_count - 1 ? "," : "");
  }

  fprintf(f, "  ]\n");
  fprintf(f, "}\n");

  int write_err = ferror(f) || fflush(f) != 0;
  if (fclose(f) != 0 || write_err) {
    platform_file_delete(tmp_path);
    return -1;
  }

  if (platform_atomic_rename(tmp_path, path) != 0) {
    platform_file_delete(tmp_path);
    return -1;
  }

  return 0;
}

void cache_invalidate(void) {
  const char *path = cache_get_path();
  platform_file_delete(path);
}

void cache_reset_path(void) {
  cache_path_buf[0] = '\0';
  platform_reset_cached_paths();
}

static int compare_triggers(const void *a, const void *b) {
  const CacheTrigger *ta = (const CacheTrigger *)a;
  const CacheTrigger *tb = (const CacheTrigger *)b;
  return ta->minute - tb->minute;
}

int cache_build_triggers(PrayerCache *cache, const Config *cfg, const struct PrayerTimes *times,
                         int current_minute, const char *date_str) {
  if (!cache || !cfg || !times || !date_str)
    return 0;

  memset(cache, 0, sizeof(*cache));
  if (!copy_string(cache->date, sizeof(cache->date), date_str)) {
    cache_log_trunc("date");
  }

  PrayerType prayer_types[] = {PRAYER_FAJR, PRAYER_SUNRISE, PRAYER_DHUHA, PRAYER_DHUHR,
                               PRAYER_ASR,  PRAYER_MAGHRIB, PRAYER_ISHA};

  for (int i = 0; i < 7; i++) {
    PrayerType type = prayer_types[i];
    if (!prayer_is_enabled(cfg, type))
      continue;

    double pt = prayer_get_time(times, type);
    int prayer_min = (int)ceil(pt * 60.0);
    const char *name = prayer_get_name(type);
    const PrayerConfig *pcfg = prayer_get_config(cfg, type);

    // Add exact prayer time
    if (prayer_min >= current_minute && cache->trigger_count < MAX_TRIGGERS) {
      CacheTrigger *t = &cache->triggers[cache->trigger_count];
      if (!copy_string(t->prayer, sizeof(t->prayer), name)) {
        cache_log_trunc("prayer");
      }
      t->minute = prayer_min;
      t->minutes_before = 0;
      t->prayer_time = pt;
      cache->trigger_count++;
    }

    // Add reminders
    for (int j = 0; j < pcfg->reminder_count; j++) {
      int reminder_min = prayer_min - pcfg->reminders[j];
      if (reminder_min < 0)
        reminder_min += 24 * 60;

      if (reminder_min >= current_minute && cache->trigger_count < MAX_TRIGGERS) {
        CacheTrigger *t = &cache->triggers[cache->trigger_count];
        if (!copy_string(t->prayer, sizeof(t->prayer), name)) {
          cache_log_trunc("prayer");
        }
        t->minute = reminder_min;
        t->minutes_before = pcfg->reminders[j];
        t->prayer_time = pt;
        cache->trigger_count++;
      }
    }
  }

  // Sort triggers by minute ascending
  if (cache->trigger_count > 1) {
    qsort(cache->triggers, (size_t)cache->trigger_count, sizeof(CacheTrigger), compare_triggers);
  }

  return cache->trigger_count;
}

void cache_remove_trigger(PrayerCache *cache, int index) {
  if (!cache || index < 0 || index >= cache->trigger_count)
    return;

  for (int i = index; i < cache->trigger_count - 1; i++) {
    cache->triggers[i] = cache->triggers[i + 1];
  }
  cache->trigger_count--;
}
