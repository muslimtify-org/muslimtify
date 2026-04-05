#ifndef CLI_INTERNAL_H
#define CLI_INTERNAL_H

#include "config.h"
#include <string.h>

typedef int (*HandlerFn)(int argc, char **argv);

typedef struct {
  const char *name;
  HandlerFn handler;
} CommandEntry;

static inline const CommandEntry *dispatch_lookup(const CommandEntry *table, int n,
                                                  const char *name) {
  for (int i = 0; i < n; i++) {
    if (strcmp(table[i].name, name) == 0)
      return &table[i];
  }
  return NULL;
}

#define DISPATCH_N(table) ((int)(sizeof(table) / sizeof((table)[0])))

int handle_show(int argc, char **argv);
int handle_check(int argc, char **argv);
int handle_next(int argc, char **argv);
int handle_config(int argc, char **argv);
int handle_location(int argc, char **argv);
int handle_enable(int argc, char **argv);
int handle_disable(int argc, char **argv);
int handle_list(int argc, char **argv);
int handle_reminder(int argc, char **argv);
int handle_daemon(int argc, char **argv);
int handle_notification(int argc, char **argv);
int handle_run(int argc, char **argv);
int handle_method(int argc, char **argv);
int handle_version(int argc, char **argv);
int handle_help(int argc, char **argv);

#ifdef MUSLIMTIFY_CMD_RUN_TEST
int cmd_run_resolve_gui_path_for_test(const char *exe_dir, char *buffer, size_t buffer_size);
#endif

#endif // CLI_INTERNAL_H
