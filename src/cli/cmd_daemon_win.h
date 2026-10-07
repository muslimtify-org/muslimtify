#ifndef CMD_DAEMON_WIN_H
#define CMD_DAEMON_WIN_H

#include <stddef.h>

enum { DAEMON_TASK_ACTION_MAX = 1024 };

int build_windows_task_action(const char *exe_dir, char *buffer, size_t buffer_size);

#endif
