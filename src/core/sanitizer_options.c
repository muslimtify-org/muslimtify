// Baked-in LeakSanitizer suppressions for third-party startup leaks.
//
// libnotify (and, in the GUI, raylib/GLFW) connect to the session bus, which
// makes D-Bus and GLib perform one-time global allocations they never free
// before exit. These are not muslimtify bugs and cannot be freed from our
// code, so we suppress them by module name. Defining __lsan_default_suppressions
// bakes the list into every sanitized binary, so no LSAN_OPTIONS env var is
// needed. Active only when AddressSanitizer is compiled in (Debug builds);
// a no-op otherwise.
//
// Note: leaks from dlopen'd GPU/GL drivers (pulled in by raylib at window
// creation) show up as "<unknown module>" and cannot be matched by name —
// the GUI binary disables leak detection outright instead (see muslimtify_gui.c).

#if defined(__SANITIZE_ADDRESS__) || (defined(__has_feature) && __has_feature(address_sanitizer))

const char *__lsan_default_suppressions(void);

const char *__lsan_default_suppressions(void) {
  return "leak:libdbus-1\n"
         "leak:libnotify\n"
         "leak:libglib-2.0\n"
         "leak:libgobject-2.0\n"
         "leak:libgio-2.0\n";
}

#else
// Avoid an empty translation unit (ISO C / -Wpedantic) on non-sanitized builds.
typedef int muslimtify_sanitizer_options_not_used;
#endif
