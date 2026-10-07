#include "platform/windows/daemon_win.h"

#include "log.h"
#include "platform.h"
#include "toast_activator.h"
#include "version.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>

#define TASK_NAME "muslimtify"
#define SERVICE_PROGRAM "muslimtify-service.exe"

enum { SCHTASKS_TIMEOUT_MS = 60000, SCHTASKS_PIPE_BYTES = 65536, TASK_XML_MAX = 16384 };

static char schtasks_override[PLATFORM_DAEMON_PATH_MAX];

void daemon_win_set_schtasks_path(const char *path) {
  snprintf(schtasks_override, sizeof(schtasks_override), "%s", path ? path : "");
}

static wchar_t *utf8_to_wide(const char *text) {
  int len;
  wchar_t *wide;

  if (!text)
    return NULL;

  len = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
  if (len <= 0)
    return NULL;

  wide = (wchar_t *)malloc((size_t)len * sizeof(wchar_t));
  if (!wide)
    return NULL;

  if (MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, len) <= 0) {
    free(wide);
    return NULL;
  }

  return wide;
}

// The schtasks program as a wide path: the stand-in when a test set one, else
// the one in the system directory. Never a bare name, which CreateProcessW
// would look for beside the running program and in the current directory first.
static wchar_t *schtasks_program(void) {
  if (schtasks_override[0] != '\0')
    return utf8_to_wide(schtasks_override);

  wchar_t dir[MAX_PATH];
  UINT n = GetSystemDirectoryW(dir, MAX_PATH);
  if (n == 0 || n >= MAX_PATH)
    return NULL;

  size_t cap = (size_t)n + wcslen(L"\\schtasks.exe") + 1;
  wchar_t *path = (wchar_t *)malloc(cap * sizeof(wchar_t));
  if (!path)
    return NULL;
  swprintf(path, cap, L"%ls\\schtasks.exe", dir);
  return path;
}

// Runs schtasks with the given arguments and waits for it. Returns 0 when it
// exited 0, 1 when it exited with anything else, and -1 when it could not be
// started or did not finish in time. With a non-NULL output, what it printed is
// stored there and its size in *output_len. Otherwise its output is discarded,
// and it never reads the caller's input or writes to the caller's console.
static int run_schtasks(const char *args, char *output, size_t output_cap, size_t *output_len) {
  int result = -1;
  wchar_t *program = schtasks_program();
  wchar_t *wide_args = utf8_to_wide(args);
  wchar_t *cmd = NULL;
  HANDLE pipe_read = NULL;
  HANDLE pipe_write = NULL;
  HANDLE nul_in = INVALID_HANDLE_VALUE;
  HANDLE nul_out = INVALID_HANDLE_VALUE;
  SECURITY_ATTRIBUTES inherit;
  STARTUPINFOW si;
  PROCESS_INFORMATION pi;

  if (output_len)
    *output_len = 0;

  if (!program || !wide_args) {
    MT_LOGF(MT_LOG_ERROR, "Error: Failed to prepare the schtasks command");
    goto cleanup;
  }

  size_t cmd_len = wcslen(program) + wcslen(wide_args) + 4;
  cmd = (wchar_t *)malloc(cmd_len * sizeof(wchar_t));
  if (!cmd) {
    MT_LOGF(MT_LOG_ERROR, "Error: Out of memory while preparing the schtasks command");
    goto cleanup;
  }
  swprintf(cmd, cmd_len, L"\"%ls\" %ls", program, wide_args);

  ZeroMemory(&inherit, sizeof(inherit));
  inherit.nLength = sizeof(inherit);
  inherit.bInheritHandle = TRUE;

  nul_in = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit,
                       OPEN_EXISTING, 0, NULL);
  nul_out = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit,
                        OPEN_EXISTING, 0, NULL);
  if (nul_in == INVALID_HANDLE_VALUE || nul_out == INVALID_HANDLE_VALUE) {
    MT_LOGF(MT_LOG_ERROR, "Error: Failed to open the null device (error %lu)", GetLastError());
    goto cleanup;
  }

  // vibekit: the output is read after schtasks exits, so it must fit the pipe
  // buffer. Read on a thread if a task XML ever outgrows SCHTASKS_PIPE_BYTES.
  if (output) {
    if (!CreatePipe(&pipe_read, &pipe_write, &inherit, SCHTASKS_PIPE_BYTES) ||
        !SetHandleInformation(pipe_read, HANDLE_FLAG_INHERIT, 0)) {
      MT_LOGF(MT_LOG_ERROR, "Error: Failed to create a pipe for schtasks (error %lu)",
              GetLastError());
      goto cleanup;
    }
  }

  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdInput = nul_in;
  si.hStdOutput = output ? pipe_write : nul_out;
  si.hStdError = nul_out;
  ZeroMemory(&pi, sizeof(pi));

  if (!CreateProcessW(program, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
    DWORD err = GetLastError();
    if (err == ERROR_ACCESS_DENIED)
      MT_LOGF(MT_LOG_ERROR, "Error: Access denied. Try running as administrator.");
    else
      MT_LOGF(MT_LOG_ERROR, "Error: Failed to run schtasks (error %lu)", err);
    goto cleanup;
  }

  // Our copy of the write end must go, or reading below never sees the end.
  if (pipe_write) {
    CloseHandle(pipe_write);
    pipe_write = NULL;
  }

  // Bounded wait: never block an installer indefinitely. schtasks should return
  // in well under a second. If it stalls, terminate it rather than hang.
  if (WaitForSingleObject(pi.hProcess, SCHTASKS_TIMEOUT_MS) != WAIT_OBJECT_0) {
    MT_LOGF(MT_LOG_ERROR, "Error: schtasks did not complete within 60 seconds");
    TerminateProcess(pi.hProcess, 1);
    WaitForSingleObject(pi.hProcess, 5000);
  } else {
    DWORD exit_code = 1;
    if (!GetExitCodeProcess(pi.hProcess, &exit_code))
      exit_code = 1;
    result = exit_code == 0 ? 0 : 1;

    if (output) {
      size_t total = 0;
      DWORD got = 0;
      while (total < output_cap &&
             ReadFile(pipe_read, output + total, (DWORD)(output_cap - total), &got, NULL) &&
             got > 0)
        total += got;
      if (output_len)
        *output_len = total;
    }
  }
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

cleanup:
  if (pipe_read)
    CloseHandle(pipe_read);
  if (pipe_write)
    CloseHandle(pipe_write);
  if (nul_in != INVALID_HANDLE_VALUE)
    CloseHandle(nul_in);
  if (nul_out != INVALID_HANDLE_VALUE)
    CloseHandle(nul_out);
  free(cmd);
  free(wide_args);
  free(program);
  return result;
}

int daemon_win_task_action(const char *service_path, char *buffer, size_t buffer_size) {
  if (!service_path || service_path[0] == '\0' || !buffer || buffer_size == 0)
    return -1;

  int written = snprintf(buffer, buffer_size, "\"%s\"", service_path);
  if (written < 0 || (size_t)written >= buffer_size)
    return -1;
  return written;
}

bool daemon_win_task_xml_enabled(const char *xml, size_t length) {
  if (!xml)
    return true;

  // Dropping the zero bytes turns UTF-16 into the ASCII the single-byte form
  // already is, which is all the element names below need.
  char text[TASK_XML_MAX];
  size_t n = 0;
  for (size_t i = 0; i < length && n + 1 < sizeof(text); i++) {
    if (xml[i] != '\0')
      text[n++] = xml[i];
  }
  text[n] = '\0';

  // A trigger has an <Enabled> of its own, so only the settings block counts.
  const char *settings = strstr(text, "<Settings>");
  if (!settings)
    return true;
  const char *end = strstr(settings, "</Settings>");
  const char *off = strstr(settings, "<Enabled>false</Enabled>");
  return !(off && (!end || off < end));
}

static bool is_file(const char *path) {
  wchar_t *wide = utf8_to_wide(path);
  if (!wide)
    return false;
  DWORD attrs = GetFileAttributesW(wide);
  free(wide);
  return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

// The program the scheduled task will run. The caller does not choose it: it is
// the muslimtify-service.exe found in a fixed list of places, so a frontend can
// never install itself, or anything else, as the task. A test passes a path of
// its own. Returns NULL, after logging why, when there is no usable program.
static const char *resolve_service_program(const char *binary_path, char *found, size_t cap) {
  if (binary_path) {
    if (binary_path[0] != '\0' && is_file(binary_path))
      return binary_path;
    MT_LOGF(MT_LOG_ERROR, "Error: %s is not an existing program", binary_path);
    return NULL;
  }

  // Beside the running program first: that is the command line tool itself, a
  // frontend installed next to it, or a build run from its build directory.
  // Then where this build installs to.
  const char *dirs[2] = {platform_exe_dir(), MUSLIMTIFY_INSTALL_BINDIR};
  for (size_t i = 0; i < 2; i++) {
    if (!dirs[i] || dirs[i][0] == '\0')
      continue;
    int n = snprintf(found, cap, "%s%c" SERVICE_PROGRAM, dirs[i], PLATFORM_PATH_SEP);
    if (n <= 0 || (size_t)n >= cap)
      continue;
    // The install directory comes from CMake with forward slashes.
    for (char *p = found; *p; p++) {
      if (*p == '/')
        *p = PLATFORM_PATH_SEP;
    }
    if (is_file(found))
      return found;
  }

  MT_LOGF(MT_LOG_ERROR, "Error: Cannot find " SERVICE_PROGRAM " in any known location");
  return NULL;
}

PlatformDaemonResult platform_daemon_install(const char *binary_path, PlatformDaemonInstall *out) {
  memset(out, 0, sizeof(*out));

  char found[PLATFORM_DAEMON_PATH_MAX];
  binary_path = resolve_service_program(binary_path, found, sizeof(found));
  if (!binary_path)
    return PLATFORM_DAEMON_BINARY_INVALID;
  snprintf(out->binary_path, sizeof(out->binary_path), "%s", binary_path);

  char action[DAEMON_TASK_ACTION_MAX];
  if (daemon_win_task_action(binary_path, action, sizeof(action)) < 0) {
    MT_LOGF(MT_LOG_ERROR, "Error: Failed to build the scheduled task action");
    return PLATFORM_DAEMON_MANAGER_FAILED;
  }

  /* /it (interactive token) registers the task to run only while the user is
     logged on and, crucially, WITHOUT a stored password. Without it schtasks
     prompts "Enter the run as password for user ..." and, in a non-interactive
     context (e.g. winget's unattended-install validation VM), blocks on stdin
     forever -- hanging the installer until it times out. Interactive-only is
     also the correct mode for a notification daemon, which needs a logged-on
     session to show toasts. */
  char args[DAEMON_TASK_ACTION_MAX + 128];
  snprintf(args, sizeof(args), "/create /tn \"" TASK_NAME "\" /tr %s /sc minute /mo 1 /it /f",
           action);
  if (run_schtasks(args, NULL, 0, NULL) != 0) {
    MT_LOGF(MT_LOG_ERROR, "Error: Failed to create the scheduled task '" TASK_NAME "'");
    return PLATFORM_DAEMON_MANAGER_FAILED;
  }

  // The adhan notification's Stop button routes back through the activator.
  // The task works without it, so a failure here does not fail the install.
  if (register_toast_activator() != 0)
    MT_LOGF(MT_LOG_WARNING,
            "Warning: could not register the toast Stop button "
            "(notifications still work, stop via 'muslimtify notification --adhan stop').");

  return PLATFORM_DAEMON_OK;
}

PlatformDaemonResult platform_daemon_uninstall(PlatformDaemonUninstall *out) {
  memset(out, 0, sizeof(*out));

  int query = run_schtasks("/query /tn \"" TASK_NAME "\"", NULL, 0, NULL);
  if (query < 0)
    return PLATFORM_DAEMON_MANAGER_FAILED;

  PlatformDaemonResult result = PLATFORM_DAEMON_OK;
  if (query == 0) {
    if (run_schtasks("/delete /tn \"" TASK_NAME "\" /f", NULL, 0, NULL) == 0) {
      out->unit_removed = true;
    } else {
      MT_LOGF(MT_LOG_ERROR, "Error: Failed to delete the scheduled task '" TASK_NAME "'");
      result = PLATFORM_DAEMON_MANAGER_FAILED;
    }
  }

  unregister_toast_activator();
  return result;
}

PlatformDaemonResult platform_daemon_status(PlatformDaemonStatus *out) {
  memset(out, 0, sizeof(*out));

  char xml[TASK_XML_MAX];
  size_t length = 0;
  int query = run_schtasks("/query /tn \"" TASK_NAME "\" /xml", xml, sizeof(xml), &length);
  if (query < 0)
    return PLATFORM_DAEMON_MANAGER_FAILED;
  if (query != 0)
    return PLATFORM_DAEMON_OK;

  // The task runs for a moment once a minute, so "running" means it exists and
  // the scheduler will fire it, not that an instance is executing right now.
  out->installed = true;
  out->enabled = daemon_win_task_xml_enabled(xml, length);
  out->running = out->enabled;
  return PLATFORM_DAEMON_OK;
}
