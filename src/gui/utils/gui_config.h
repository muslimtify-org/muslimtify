#ifndef MUSLIMTIFY_GUI_CONFIG_H
#define MUSLIMTIFY_GUI_CONFIG_H

#include "config.h"

typedef struct {
  char date[64];
  size_t dateSize;
} CurrentDate;

Config *guiGetConfig(void);
CurrentDate getCurrentDate(void);

void guiLoadConfig(void);

#endif // MUSLIMTIFY_GUI_CONFIG_H
