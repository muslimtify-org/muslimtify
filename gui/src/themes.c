#include "themes.h"
#include "colors.h"

MuslimtifyThemes get_themes(bool isDark) {
  MuslimtifyThemes light_theme = {
      .primary = ColorHex(COLOR_PRIMARY),
      .on_primary = ColorHex(COLOR_ON_PRIMARY),
      .secondary = ColorHex(COLOR_SECONDARY),
      .on_secondary = ColorHex(COLOR_ON_SECONDARY),
      .surface = ColorHex(COLOR_SURFACE),
      .on_surface = ColorHex(COLOR_ON_SURFACE),
      .error = ColorHex(COLOR_ERROR),
      .on_error = ColorHex(COLOR_ON_ERROR),
      .border = ColorHex(COLOR_BORDER),
      .outline = ColorHex(COLOR_BORDER_ALPHA)
  };

  MuslimtifyThemes dark_theme = {
      .primary = ColorHex(COLOR_PRIMARY_DARK),
      .on_primary = ColorHex(COLOR_ON_PRIMARY_DARK),
      .secondary = ColorHex(COLOR_SECONDARY_DARK),
      .on_secondary = ColorHex(COLOR_ON_SECONDARY_DARK),
      .surface = ColorHex(COLOR_SURFACE_DARK),
      .on_surface = ColorHex(COLOR_ON_SURFACE_DARK),
      .error = ColorHex(COLOR_ERROR_DARK),
      .on_error = ColorHex(COLOR_ON_ERROR_DARK),
      .border = ColorHex(COLOR_BORDER_DARK),
      .outline = ColorHex(COLOR_BORDER_ALPHA_DARK)
  };

  return isDark ? dark_theme : light_theme;
}
