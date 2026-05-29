#include "dashboard_content.h"

#include "calculation_profile_card.h"
#include "ccompose.h"
#include "components/topbar.h"
#include "daily_schedule.h"
#include "prayer_card.h"

void DashboardContent(void) {
  Column("Body", .layout = {.sizing = {.height = Grow(), .width = Grow()}}) {

    TopBar("Dashboard");
    Column("ContentContainer", .layout = {.sizing = {.height = Grow(), .width = Grow()}}) {
      Row("Containter1",
          .layout = {.sizing = {.width = Grow()},
                     .childGap = 16,
                     .padding = PadOnly(.left = 48, .right = 48, .top = 48, .bottom = 24)}) {
        PrayerCard();
        CalculationProfileCard();
      }
      DailySchedule();
    }
  }
}
