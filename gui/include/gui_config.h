#ifndef MUSLIMTIFY_GUI_CONFIG_H
#define MUSLIMTIFY_GUI_CONFIG_H

#include <stdbool.h>

// GUI-only user preferences, stored as gui.json next to the CLI config.json.
// To add a field: extend the struct, gui_config_default, write_gui_json and
// gui_config_load in gui_config.c.
typedef struct {
  bool prefer_dark;
} GuiConfig;

GuiConfig gui_config_default(void);

// Fills cfg from gui.json. A missing file, missing key or bad value keeps the
// default. Returns 0 on success, -1 if the file is unreadable or not a JSON
// object, in which case cfg holds the defaults.
int gui_config_load(GuiConfig *cfg);

// Writes cfg to gui.json atomically. Returns 0 on success, -1 on error.
int gui_config_save(const GuiConfig *cfg);

const char *gui_config_get_path(void);

#endif // MUSLIMTIFY_GUI_CONFIG_H
