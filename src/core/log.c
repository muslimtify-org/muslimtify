#include "log.h"

#include <stdio.h>

static MtLogHandler current_handler = mt_log_stderr;
static void *current_user_data = NULL;

void mt_log_stderr(MtLogLevel level, const char *message, void *user_data) {
  (void)level;
  (void)user_data;
  fputs(message, stderr);
  fputc('\n', stderr);
}

void mt_log_set_handler(MtLogHandler handler, void *user_data) {
  current_handler = handler;
  current_user_data = user_data;
}

void mt_log(MtLogLevel level, const char *message) {
  if (!message || !current_handler)
    return;
  current_handler(level, message, current_user_data);
}
