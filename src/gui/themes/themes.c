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
      .surface_alt = ColorHex(COLOR_SURFACE_ALT),
      .on_surface_alt = ColorHex(COLOR_ON_SURFACE_ALT),
      .container = ColorHex(COLOR_CONTAINER),
      .on_container = ColorHex(COLOR_ON_CONTAINER),
      .container_alt = ColorHex(COLOR_CONTAINER_ALT),
      .on_container_alt = ColorHex(COLOR_ON_CONTAINER_ALT),
      .accent = ColorHex(COLOR_ACCENT),
      .on_accent = ColorHex(COLOR_ON_ACCENT),
      .error = ColorHex(COLOR_ERROR),
      .on_error = ColorHex(COLOR_ON_ERROR),
      .border = ColorHex(COLOR_BORDER),
      .disabled = ColorHex(COLOR_DISABLED),
  };

  MuslimtifyThemes dark_theme = {
      .primary = ColorHex(COLOR_PRIMARY_DARK),
      .on_primary = ColorHex(COLOR_ON_PRIMARY_DARK),
      .secondary = ColorHex(COLOR_SECONDARY_DARK),
      .on_secondary = ColorHex(COLOR_ON_SECONDARY_DARK),
      .surface = ColorHex(COLOR_SURFACE_DARK),
      .on_surface = ColorHex(COLOR_ON_SURFACE_DARK),
      .surface_alt = ColorHex(COLOR_SURFACE_ALT_DARK),
      .on_surface_alt = ColorHex(COLOR_ON_SURFACE_ALT_DARK),
      .container = ColorHex(COLOR_CONTAINER_DARK),
      .on_container = ColorHex(COLOR_ON_CONTAINER_DARK),
      .container_alt = ColorHex(COLOR_CONTAINER_ALT_DARK),
      .on_container_alt = ColorHex(COLOR_ON_CONTAINER_ALT_DARK),
      .accent = ColorHex(COLOR_ACCENT_DARK),
      .on_accent = ColorHex(COLOR_ON_ACCENT_DARK),
      .error = ColorHex(COLOR_ERROR_DARK),
      .on_error = ColorHex(COLOR_ON_ERROR_DARK),
      .border = ColorHex(COLOR_BORDER_DARK),
      .disabled = ColorHex(COLOR_DISABLED_DARK),
  };

  return isDark ? dark_theme : light_theme;
}
