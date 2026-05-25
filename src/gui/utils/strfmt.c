#include "strfmt.h"
#include <ctype.h>

void toUpper(char *str) {
  while (*str) {
    *str = toupper(*str);
    str++;
  }
}

void toLower(char *str) {
  while (*str) {
    *str = tolower(*str);
    str++;
  }
}

void toTitleCase(char *str) {
  int new_word = 1;

  while (*str) {
    if (isspace((unsigned char)*str)) {
      new_word = 1;
    } else {
      if (new_word) {
        *str = toupper((unsigned char)*str);
        new_word = 0;
      } else {
        *str = tolower((unsigned char)*str);
      }
    }

    str++;
  }
}
