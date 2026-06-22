#include "app/app.h"

// raylib (via GLFW) dlopens the GPU/GL driver at window creation, which leaks
// one-time global state that LeakSanitizer reports as "<unknown module>" — not
// suppressible by name and not our bug. Leak-at-exit is also of little value
// for an interactive GUI. Disable only leak detection here while keeping the
// rest of ASan (use-after-free, buffer overflow) and UBSan active. Scoped to
// this binary; the CLI and daemon keep full leak detection. Active only under
// AddressSanitizer (Debug builds); a no-op otherwise.
// GCC lacks __has_feature; the preprocessor can't short-circuit a function-like
// macro test, so provide a no-op fallback before using it.
#ifndef __has_feature
#define __has_feature(x) 0
#endif

#if defined(__SANITIZE_ADDRESS__) || __has_feature(address_sanitizer)
const char *__asan_default_options(void);
const char *__asan_default_options(void) {
  return "detect_leaks=0";
}
#endif

int main(void) {
  AppRun();
  return 0;
}
