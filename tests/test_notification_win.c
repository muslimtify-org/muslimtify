#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

static int total = 0;
static int failures = 0;

BOOL notification_win_resolve_toast_icon_path_for_test(const wchar_t *base_dir, wchar_t *buffer,
                                                       size_t buffer_size);
wchar_t *notification_win_build_toast_xml_for_test(const wchar_t *base_dir, const wchar_t *wtitle,
                                                   const wchar_t *wmsg, const char *urgency);
wchar_t *notification_win_build_adhan_xml_for_test(const wchar_t *wtitle, const wchar_t *wmsg);
BOOL notification_win_resolve_adhan_path_for_test(const wchar_t *base_dir, wchar_t *buffer,
                                                  size_t buffer_size);

static void report_result(const char *label, bool pass) {
  total++;
  if (pass) {
    printf("  PASS: %s\n", label);
  } else {
    printf("  FAIL: %s\n", label);
    failures++;
  }
}

static bool wide_equals(const wchar_t *a, const wchar_t *b) {
  return a != NULL && b != NULL && wcscmp(a, b) == 0;
}

static bool join_path(const wchar_t *base_dir, const wchar_t *relative, wchar_t *buffer,
                      size_t buffer_size) {
  int written;

  if (!base_dir || !relative || !buffer || buffer_size == 0)
    return false;

  written = swprintf(buffer, buffer_size, L"%ls\\%ls", base_dir, relative);
  return written > 0 && (size_t)written < buffer_size;
}

static bool canonicalize_path(const wchar_t *path, wchar_t *buffer, size_t buffer_size) {
  DWORD len;
  wchar_t *file_part = NULL;

  if (!path || !buffer || buffer_size == 0)
    return false;

  len = GetFullPathNameW(path, (DWORD)buffer_size, buffer, &file_part);
  return len > 0 && len < buffer_size && file_part != NULL;
}

static bool ensure_parent_dirs(const wchar_t *path) {
  wchar_t full_path[MAX_PATH];
  wchar_t *file_part = NULL;
  wchar_t *cursor;
  DWORD len;

  len = GetFullPathNameW(path, MAX_PATH, full_path, &file_part);
  if (len == 0 || len >= MAX_PATH || !file_part)
    return false;

  *file_part = L'\0';
  for (cursor = full_path + 3; *cursor != L'\0'; cursor++) {
    if (*cursor == L'\\' || *cursor == L'/') {
      wchar_t saved = *cursor;
      *cursor = L'\0';
      CreateDirectoryW(full_path, NULL);
      *cursor = saved;
    }
  }

  if (full_path[0] != L'\0')
    CreateDirectoryW(full_path, NULL);

  return true;
}

static bool create_empty_file(const wchar_t *path) {
  HANDLE file;
  wchar_t canonical[MAX_PATH];

  if (!canonicalize_path(path, canonical, sizeof(canonical) / sizeof(canonical[0])))
    return false;
  if (!ensure_parent_dirs(canonical))
    return false;

  file = CreateFileW(canonical, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (file == INVALID_HANDLE_VALUE)
    return false;

  CloseHandle(file);
  return true;
}

static bool make_test_root_dir(const wchar_t *suffix, wchar_t *buffer, size_t buffer_size) {
  wchar_t temp_path[MAX_PATH];
  DWORD len = GetTempPathW(MAX_PATH, temp_path);
  int written;

  if (len == 0 || len >= MAX_PATH || !suffix || !buffer || buffer_size == 0)
    return false;

  written = swprintf(buffer, buffer_size, L"%lsmuslimtify-toast-icon-%lu-%ls", temp_path,
                     (unsigned long)GetCurrentProcessId(), suffix);
  if (written <= 0 || (size_t)written >= buffer_size)
    return false;

  return CreateDirectoryW(buffer, NULL) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
}

static void test_installed_layout_resolution_preference(void) {
  printf("test_installed_layout_resolution_preference\n");

  wchar_t root_dir[MAX_PATH];
  wchar_t exe_dir[MAX_PATH];
  wchar_t preferred_path[MAX_PATH];
  wchar_t fallback_path[MAX_PATH];
  wchar_t canonical_first[MAX_PATH];
  wchar_t canonical_second[MAX_PATH];
  wchar_t resolved[MAX_PATH];

  if (!make_test_root_dir(L"installed", root_dir, sizeof(root_dir) / sizeof(root_dir[0]))) {
    report_result("create base dir", false);
    return;
  }
  if (!join_path(root_dir, L"bin", exe_dir, sizeof(exe_dir) / sizeof(exe_dir[0]))) {
    report_result("build executable dir", false);
    return;
  }
  if (!join_path(exe_dir, L"..\\share\\icons\\hicolor\\128x128\\apps\\muslimtify.png",
                 preferred_path, sizeof(preferred_path) / sizeof(preferred_path[0]))) {
    report_result("build preferred installed icon path", false);
    return;
  }
  if (!canonicalize_path(preferred_path, canonical_first,
                         sizeof(canonical_first) / sizeof(canonical_first[0]))) {
    report_result("canonicalize preferred installed icon path", false);
    return;
  }
  if (!join_path(exe_dir, L"..\\share\\pixmaps\\muslimtify.png", fallback_path,
                 sizeof(fallback_path) / sizeof(fallback_path[0]))) {
    report_result("build fallback installed icon path", false);
    return;
  }
  if (!canonicalize_path(fallback_path, canonical_second,
                         sizeof(canonical_second) / sizeof(canonical_second[0]))) {
    report_result("canonicalize fallback installed icon path", false);
    return;
  }

  report_result("create preferred installed icon", create_empty_file(canonical_first));
  report_result("create fallback installed icon", create_empty_file(canonical_second));

  resolved[0] = L'\0';
  report_result("resolver returns success",
                notification_win_resolve_toast_icon_path_for_test(
                    exe_dir, resolved, sizeof(resolved) / sizeof(resolved[0])));
  report_result("resolver prefers installed icon path", wide_equals(resolved, preferred_path));
}

static void test_development_layout_resolution_preference(void) {
  printf("test_development_layout_resolution_preference\n");

  wchar_t root_dir[MAX_PATH];
  wchar_t exe_dir[MAX_PATH];
  wchar_t preferred_path[MAX_PATH];
  wchar_t canonical_first[MAX_PATH];
  wchar_t resolved[MAX_PATH];

  if (!make_test_root_dir(L"development", root_dir, sizeof(root_dir) / sizeof(root_dir[0]))) {
    report_result("create base dir", false);
    return;
  }
  if (!join_path(root_dir, L"build\\bin\\Debug", exe_dir, sizeof(exe_dir) / sizeof(exe_dir[0]))) {
    report_result("build executable dir", false);
    return;
  }
  if (!join_path(exe_dir, L"..\\..\\..\\assets\\muslimtify.png", preferred_path,
                 sizeof(preferred_path) / sizeof(preferred_path[0]))) {
    report_result("build preferred development icon path", false);
    return;
  }
  if (!canonicalize_path(preferred_path, canonical_first,
                         sizeof(canonical_first) / sizeof(canonical_first[0]))) {
    report_result("canonicalize preferred development icon path", false);
    return;
  }

  report_result("create preferred development icon", create_empty_file(canonical_first));

  resolved[0] = L'\0';
  report_result("resolver returns success",
                notification_win_resolve_toast_icon_path_for_test(
                    exe_dir, resolved, sizeof(resolved) / sizeof(resolved[0])));
  report_result("resolver prefers development icon path", wide_equals(resolved, preferred_path));
}

static void test_no_icon_fallback_behavior(void) {
  printf("test_no_icon_fallback_behavior\n");

  wchar_t root_dir[MAX_PATH];
  wchar_t exe_dir[MAX_PATH];
  wchar_t *xml;

  if (!make_test_root_dir(L"fallback", root_dir, sizeof(root_dir) / sizeof(root_dir[0]))) {
    report_result("create base dir", false);
    return;
  }
  if (!join_path(root_dir, L"build\\bin\\Release", exe_dir, sizeof(exe_dir) / sizeof(exe_dir[0]))) {
    report_result("build executable dir", false);
    return;
  }

  xml = notification_win_build_toast_xml_for_test(exe_dir, L"Prayer Time: Isha",
                                                  L"It's time for Isha prayer", "critical");
  report_result("caller still builds toast XML without an icon", xml != NULL);
  report_result("caller omits the image node when icon resolution fails",
                xml != NULL && wcsstr(xml, L"<image ") == NULL);
  report_result("caller still includes toast text payload",
                xml != NULL && wcsstr(xml, L"<text>Prayer Time: Isha</text>") != NULL);
  free(xml);
}

static void test_adhan_xml_has_stop_action_and_silent_audio(void) {
  printf("test_adhan_xml_has_stop_action_and_silent_audio\n");

  wchar_t *xml = notification_win_build_adhan_xml_for_test(L"Prayer Time: Fajr",
                                                           L"It's time for Fajr prayer");
  report_result("adhan XML builds", xml != NULL);
  report_result("adhan XML has a Stop action",
                xml != NULL && wcsstr(xml, L"arguments=\"stop\"") != NULL);
  report_result("adhan XML silences the toast's own sound",
                xml != NULL && wcsstr(xml, L"<audio silent=\"true\"/>") != NULL);
  free(xml);
}

static void test_adhan_path_installed_layout(void) {
  printf("test_adhan_path_installed_layout\n");

  wchar_t root_dir[MAX_PATH];
  wchar_t exe_dir[MAX_PATH];
  wchar_t adhan_path[MAX_PATH];
  wchar_t resolved[MAX_PATH];

  if (!make_test_root_dir(L"adhan", root_dir, sizeof(root_dir) / sizeof(root_dir[0]))) {
    report_result("create base dir", false);
    return;
  }
  if (!join_path(root_dir, L"bin", exe_dir, sizeof(exe_dir) / sizeof(exe_dir[0]))) {
    report_result("build executable dir", false);
    return;
  }
  if (!join_path(exe_dir, L"..\\share\\muslimtify\\adhan.mp3", adhan_path,
                 sizeof(adhan_path) / sizeof(adhan_path[0]))) {
    report_result("build installed adhan path", false);
    return;
  }

  report_result("create installed adhan file", create_empty_file(adhan_path));

  resolved[0] = L'\0';
  report_result("adhan resolver returns success",
                notification_win_resolve_adhan_path_for_test(
                    exe_dir, resolved, sizeof(resolved) / sizeof(resolved[0])));
  report_result("adhan resolver finds installed adhan", wide_equals(resolved, adhan_path));
}

int main(void) {
  printf("=== notification_win icon resolution tests ===\n\n");

  test_installed_layout_resolution_preference();
  test_development_layout_resolution_preference();
  test_no_icon_fallback_behavior();
  test_adhan_xml_has_stop_action_and_silent_audio();
  test_adhan_path_installed_layout();

  printf("\n%d/%d tests passed\n", total - failures, total);
  return failures > 0 ? 1 : 0;
}

#endif
