#ifndef CLI_INTERNAL_H
#define CLI_INTERNAL_H

#include <stdbool.h>
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

typedef enum { OUTPUT_TABLE, OUTPUT_JSON, OUTPUT_HEADLESS } OutputMode;

// Scan argv for --json / --headless; set *out (default OUTPUT_TABLE).
// Returns 0 on success, non-zero if both are present (after printing the
// mutual-exclusion error to stderr).
int cli_parse_output_mode(int argc, char **argv, OutputMode *out);

// True if argv contains --help or -h.
bool cli_wants_help(int argc, char **argv);

// Print an "Unknown prayer" error plus the list of valid prayer names; returns 1.
int cli_unknown_prayer(const char *name);

int handle_show(int argc, char **argv);
int handle_config(int argc, char **argv);
int handle_location(int argc, char **argv);
int handle_offset(int argc, char **argv);
int handle_daemon(int argc, char **argv);
int handle_notification(int argc, char **argv);
int handle_method(int argc, char **argv);
int handle_madzhab(int argc, char **argv);
int handle_version(int argc, char **argv);
int handle_help(int argc, char **argv);

#endif // CLI_INTERNAL_H
