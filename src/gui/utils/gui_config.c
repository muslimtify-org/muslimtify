#include "gui_config.h"
#include <time.h>

static Config g_guiConfig;
static CurrentDate g_currentDate;

Config *guiGetConfig(void) {
  return &g_guiConfig;
}

CurrentDate getCurrentDate(void) {
  return g_currentDate;
}

static CurrentDate loadCurrentDate() {
  time_t now = time(NULL);
  struct tm *local = localtime(&now);
  char date[64];
  if (strftime(date, sizeof(date), "%A, %d %B %Y", local) == 0) {
    return (CurrentDate){0};
  }
  size_t dateSize = strlen(date);
  CurrentDate currentDate = {0};
  memcpy(currentDate.date, date, dateSize);
  currentDate.date[dateSize] = '\0';
  currentDate.dateSize = dateSize;

  return currentDate;
}

void guiLoadConfig(void) {
  if (config_load(&g_guiConfig) != 0) {
    g_guiConfig = config_default();
  }
  g_currentDate = loadCurrentDate();
}
