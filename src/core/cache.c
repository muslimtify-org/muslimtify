#define _POSIX_C_SOURCE 200809L
#include "cache.h"
#include "check_cycle.h"
#include "json.h"
#include "platform.h"
#include "prayer_checker.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "string_util.h"

// Refuse to load a cache file larger than this; a sane cache is a few KB.
#define MAX_CACHE_FILE_BYTES (1024L * 1024L)

// Bumped whenever the on-disk cache format changes in a way older readers would
// misinterpret. Version 2 introduced JSON-escaped strings; a version-1 file could
// contain an unescaped quote or brace and would be silently mis-parsed, so files
// without a matching version are rejected and rebuilt from config.
#define CACHE_FORMAT_VERSION 2

static char cache_path_buf[PLATFORM_PATH_MAX] = {0};
static bool cache_trunc_logged = false;

static void cache_log_trunc(const char *field) {
  if (!cache_trunc_logged) {
    fprintf(stderr, "cache: truncated field %s\n", field ? field : "(unknown)");
    cache_trunc_logged = true;
  }
}

// Separate from cache_trunc_logged: a field truncation and a dropped trigger are
// different failure modes, and letting one flag silence both would hide whichever
// happened second.
static bool cache_capacity_logged = false;

static void cache_log_capacity_drop(const char *prayer, int minutes_before) {
  if (!cache_capacity_logged) {
    if (minutes_before == 0) {
      fprintf(stderr, "cache: capacity reached, dropped %s adhan trigger\n",
              prayer ? prayer : "(unknown)");
    } else {
      fprintf(stderr, "cache: capacity reached, dropped %s reminder (%d min before)\n",
              prayer ? prayer : "(unknown)", minutes_before);
    }
    cache_capacity_logged = true;
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
  if (size > MAX_CACHE_FILE_BYTES) {
    fprintf(stderr, "cache: file too large (%ld bytes), refusing to load\n", size);
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

  // Reject any cache not written by this format version, including pre-versioning
  // files, which may contain unescaped strings this reader would mis-parse.
  char *version_str = get_value(ctx, "version", content);
  if (!version_str || strtol(version_str, NULL, 10) != CACHE_FORMAT_VERSION) {
    json_end(ctx);
    free(content);
    return -1;
  }

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

    // Find the matching '}', ignoring braces that appear inside JSON strings.
    // JSON has no escape for '{' or '}', so a legitimately escaped adhan path
    // can contain either; counting raw bytes would end the object early.
    char *obj_start = p;
    int depth = 0;
    char *obj_end = NULL;
    bool in_string = false;
    for (char *q = p; *q; q++) {
      if (in_string) {
        if (*q == '\\' && *(q + 1) != '\0')
          q++; // skip the escaped character, including a literal '\"'
        else if (*q == '"')
          in_string = false;
        continue;
      }
      if (*q == '"') {
        in_string = true;
      } else if (*q == '{') {
        depth++;
      } else if (*q == '}') {
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

    // A trigger object missing any key means the file is corrupt. Reject the
    // whole cache rather than continuing with a silently short trigger list:
    // run_check_cycle treats a failed load as invalid and rebuilds from config.
    char *prayer = get_value(ctx, "prayer", obj_start);
    char *minute_str = get_value(ctx, "minute", obj_start);
    char *mb_str = get_value(ctx, "minutes_before", obj_start);
    char *pt_str = get_value(ctx, "prayer_time", obj_start);
    char *ae_str = get_value(ctx, "adhan_enabled", obj_start);
    char *adhan_str = get_value(ctx, "adhan", obj_start);
    if (!prayer || !minute_str || !mb_str || !pt_str || !ae_str || !adhan_str) {
      *(obj_end + 1) = saved;
      json_end(ctx);
      free(content);
      return -1;
    }

    if (!copy_string(t->prayer, sizeof(t->prayer), prayer)) {
      cache_log_trunc("prayer");
    }
    t->minute = (int)strtol(minute_str, NULL, 10);
    t->minutes_before = (int)strtol(mb_str, NULL, 10);
    t->prayer_time = strtod(pt_str, NULL);
    t->adhan_enabled = strcmp(ae_str, "true") == 0;
    if (!copy_string(t->adhan, sizeof(t->adhan), adhan_str)) {
      cache_log_trunc("adhan");
    }

    cache->trigger_count++;

    *(obj_end + 1) = saved;
    p = obj_end + 1;

    // Between trigger objects only whitespace and a single ',' may appear, and the
    // array must end with ']'. Checking this makes the scan's safety explicit rather
    // than incidental, and rejects a truncated file instead of silently accepting a
    // partial trigger list.
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
      p++;
    if (*p == ',') {
      p++;
    } else if (*p != ']') {
      json_end(ctx);
      free(content);
      return -1;
    }
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

  platform_restrict_to_owner(f);

  fprintf(f, "{\n");
  fprintf(f, "  \"version\": %d,\n", CACHE_FORMAT_VERSION);
  fprintf(f, "  \"date\": ");
  json_write_escaped(f, cache->date);
  fprintf(f, ",\n");
  fprintf(f, "  \"triggers\": [\n");

  for (int i = 0; i < cache->trigger_count; i++) {
    const CacheTrigger *t = &cache->triggers[i];
    fprintf(f, "    {\"prayer\": ");
    json_write_escaped(f, t->prayer);
    fprintf(f,
            ", \"minute\": %d, \"minutes_before\": %d, \"prayer_time\": %.4f, "
            "\"adhan_enabled\": %s, \"adhan\": ",
            t->minute, t->minutes_before, t->prayer_time, t->adhan_enabled ? "true" : "false");
    json_write_escaped(f, t->adhan);
    fprintf(f, "}%s\n", i < cache->trigger_count - 1 ? "," : "");
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

  // An unparseable date means the caller has a bug. Scheduling nothing is the
  // safe response, since we have no day to anchor D-1/D+1 against.
  int year, month, day;
  // The return value is checked on the same line, this is the conversion check.
  // NOLINTNEXTLINE(bugprone-unchecked-string-to-number-conversion)
  if (sscanf(date_str, "%d-%d-%d", &year, &month, &day) != 3)
    return 0;
  long day_num = mt_days_from_civil(year, month, day);

  memset(cache, 0, sizeof(*cache));
  if (!copy_string(cache->date, sizeof(cache->date), date_str)) {
    cache_log_trunc("date");
  }

  PrayerType prayer_types[] = {PRAYER_FAJR, PRAYER_DHUHR, PRAYER_ASR, PRAYER_MAGHRIB, PRAYER_ISHA};

  // A trigger belongs to the day its instant falls on. That set is assembled
  // from three source days, D-1, D and D+1, because a prayer time can carry a
  // day offset of at most one day either way. A prayer whose instant spills
  // out of day D's minute range belongs to a neighbouring day instead, and is
  // moved there rather than copied, so it is scheduled exactly once.
  //
  // The three source days are walked twice, once for exact prayer times and
  // once for reminders, so every adhan is inserted before any reminder can
  // fill the array. On a discontinuity day a single calendar day can hold two
  // complete occurrences of the same prayer: at Reykjavik on 2026-04-12 the
  // isha of 04-11 spills in at 00:58 and the isha of 04-12 does not spill out,
  // and with MAX_REMINDERS configured that day needs 66 entries against a
  // MAX_TRIGGERS of 64. An adhan is what a person acts on, so the capacity
  // guard must never be free to drop one just because reminders happened to
  // be assembled first. Walking the days twice costs nothing that matters,
  // since a rebuild happens once a day.
  for (int pass = 0; pass < 2; pass++) {
    for (int day_delta = -1; day_delta <= 1; day_delta++) {
      struct PrayerTimes source_times;
      const struct PrayerTimes *source;
      if (day_delta == 0) {
        source = times;
      } else {
        int sy, sm, sd;
        mt_civil_from_days(day_num + day_delta, &sy, &sm, &sd);
        source_times = prayer_times_for_config(cfg, sy, sm, sd);
        source = &source_times;
      }

      for (int i = 0; i < PRAYER_COUNT; i++) {
        PrayerType type = prayer_types[i];
        if (!prayer_is_enabled(cfg, type))
          continue;

        double pt = prayer_get_time(source, type);
        // A prayer that does not occur has no time to schedule. Above roughly 66
        // degrees the C library reports a non-finite value for an event the Sun
        // never reaches, and converting that to int is undefined behaviour: on
        // x86-64 (int)ceil(NAN * 60.0) is -2147483648, which happens to fail the
        // bounds check below rather than firing a notification at a wild minute.
        // Relying on that is not a guard, so this is.
        if (!isfinite(pt))
          continue;
        int instant_min = (int)ceil((pt + 24.0 * day_delta) * 60.0);
        const char *name = prayer_get_name(type);
        const PrayerConfig *pcfg = prayer_get_config(cfg, type);

        if (pass == 0) {
          // Add exact prayer time, only when its instant actually falls on day D.
          // A trigger up to CATCHUP_MAX_MIN in the past is kept too, so a
          // rebuild right after it fires does not drop it before
          // trigger_catchup_action ever sees it.
          if (instant_min >= 0 && instant_min < 1440 &&
              instant_min >= current_minute - CATCHUP_MAX_MIN) {
            if (cache->trigger_count < MAX_TRIGGERS) {
              CacheTrigger *t = &cache->triggers[cache->trigger_count];
              if (!copy_string(t->prayer, sizeof(t->prayer), name)) {
                cache_log_trunc("prayer");
              }
              t->minute = instant_min;
              t->minutes_before = 0;
              t->prayer_time = pt;
              t->adhan_enabled = pcfg->adhan_enabled;
              if (!copy_string(t->adhan, sizeof(t->adhan), pcfg->adhan)) {
                cache_log_trunc("adhan");
              }
              cache->trigger_count++;
            } else {
              cache_log_capacity_drop(name, 0);
            }
          }
          continue;
        }

        // Add reminders, same day-D membership rule as the exact trigger above.
        for (int j = 0; j < pcfg->reminder_count; j++) {
          int reminder_min = instant_min - pcfg->reminders[j];
          if (reminder_min < 0 || reminder_min >= 1440)
            continue;
          if (reminder_min < current_minute - CATCHUP_MAX_MIN)
            continue;

          if (cache->trigger_count < MAX_TRIGGERS) {
            CacheTrigger *t = &cache->triggers[cache->trigger_count];
            if (!copy_string(t->prayer, sizeof(t->prayer), name)) {
              cache_log_trunc("prayer");
            }
            t->minute = reminder_min;
            t->minutes_before = pcfg->reminders[j];
            t->prayer_time = pt;
            cache->trigger_count++;
          } else {
            cache_log_capacity_drop(name, pcfg->reminders[j]);
          }
        }
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
