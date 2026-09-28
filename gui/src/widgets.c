#include "widgets.h"
#include <assert.h>
#include <ccompose.h>

bool muslimtify_widget_toggle(MuslimtifyThemes *themes, bool *is_active) {
  assert(themes != NULL);

  bool flipped = CC_Clicked("muslimtify_widget_toggle_circle");
  if (flipped)
    *is_active = !*is_active;

  Box("muslimtify_widget_toggle_box",
      .backgroundColor = *is_active ? themes->primary : themes->disabled,
      .cornerRadius = RadiusAll(25),
      .layout = {
          .padding = PadAll(8),
          .childAlignment = {.y = AlignYCenter(), .x = *is_active ? AlignXEnd() : AlignXStart()},
          .sizing = {
              .width = Fixed(70),
          },
      }) {
    Box("muslimtify_widget_toggle_circle",
        .backgroundColor = *is_active ? themes->on_primary : themes->surface,
        .cornerRadius = RadiusAll(25),
        .layout = {
            .sizing = {.width = Fixed(18), .height = Fixed(18)},
        }) {}
  }

  return flipped;
}
