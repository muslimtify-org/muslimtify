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
} MuslimtifyThemes;

MuslimtifyThemes get_themes(bool isDark);

#endif // MUSLIMTIFY_GUI_THEMES_H
