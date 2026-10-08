#ifndef DAEMON_WIN_H
#define DAEMON_WIN_H

#include <stdbool.h>
#include <stddef.h>

enum { DAEMON_TASK_ACTION_MAX = 1024 };

/* Writes the scheduled task's action for a service program: its path in
 * escaped double quotes, inside the double quotes schtasks strips from /tr, so
 * a path with a space stays one command. Returns the number of bytes written
 * (>0) or -1 on error/truncation. Pure: no I/O, no globals. */
int daemon_win_task_action(const char *service_path, char *buffer, size_t buffer_size);

/* False only when the <Settings> block of a task XML holds
 * <Enabled>false</Enabled>. A task without that element is enabled. Takes the
 * bytes schtasks printed for /query /xml, single-byte or UTF-16. Pure. */
bool daemon_win_task_xml_enabled(const char *xml, size_t length);

/* Replace the schtasks program the library runs. NULL restores the one in the
 * system directory. Exposed so a test can run a stand-in. */
void daemon_win_set_schtasks_path(const char *path);

#endif
