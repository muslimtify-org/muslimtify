#ifndef MUSLIMTIFY_GUI_THEMES_H
#define MUSLIMTIFY_GUI_THEMES_H

#include "ccompose.h"
#include <stdbool.h>

typedef struct {
  CC_Color primary;
  CC_Color on_primary;
  CC_Color secondary;
  CC_Color on_secondary;
  CC_Color surface;
  CC_Color on_surface;
  CC_Color error;
  CC_Color on_error;
  CC_Color border;
  CC_Color outline;
  CC_Color disabled;
  CC_Color on_disabled;
} MuslimtifyThemes;

MuslimtifyThemes get_themes(bool isDark);

#endif // MUSLIMTIFY_GUI_THEMES_H
