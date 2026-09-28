#ifndef MUSLIMTIFY_GUI_WIDGETS_H
#define MUSLIMTIFY_GUI_WIDGETS_H

#include "gui_config.h"
#include <ccompose.h>
#include <stdbool.h>
#include <themes.h>

typedef struct ButtonClicked ButtonClicked;

// Returns true on the frame the toggle was clicked and *is_active flipped.
bool muslimtify_widget_toggle(MuslimtifyThemes *themes, bool *is_active);

// Returns true when cfg->prefer_dark changed this frame. The change is already
// saved, the caller rebuilds the theme.
bool muslimtify_widget_header(MuslimtifyThemes *themes, GuiConfig *cfg);

bool muslimtify_widget_content(MuslimtifyThemes *themes, GuiConfig *cfg);

#endif // MUSLIMTIFY_GUI_WIDGETS_H
