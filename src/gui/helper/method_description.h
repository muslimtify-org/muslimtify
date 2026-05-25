#ifndef MUSLIMTIFY_METHOD_DESCRIPTION_H
#define MUSLIMTIFY_METHOD_DESCRIPTION_H

#include <string.h>

typedef struct {
  const char *key;
  const char *description;
} MethodDescription;

static const MethodDescription METHOD_DESCRIPTIONS[] = {
    {"mwl", "Muslim World League, based in Mecca"},
    {"makkah", "Umm al-Qura University \xE2\x80\x94 official method of Saudi Arabia"},
    {"isna", "Islamic Society of North America"},
    {"egypt", "Egyptian General Authority of Survey"},
    {"karachi", "University of Islamic Sciences, Karachi, Pakistan"},
    {"turkey", "Presidency of Religious Affairs (Diyanet), Turkey"},
    {"singapore", "Islamic Religious Council of Singapore (MUIS)"},
    {"jakim", "Department of Islamic Development Malaysia (JAKIM)"},
    {"kemenag", "Ministry of Religious Affairs of the Republic of Indonesia"},
    {"france", "Union of Islamic Organisations of France (UOIF)"},
    {"russia", "Spiritual Administration of Muslims of Russia"},
    {"dubai", "General Authority of Islamic Affairs and Endowments, Dubai"},
    {"qatar", "Ministry of Endowments and Islamic Affairs, Qatar"},
    {"kuwait", "Ministry of Awqaf and Islamic Affairs, Kuwait"},
    {"jordan", "Ministry of Awqaf and Islamic Affairs, Jordan"},
    {"gulf", "Standard method used across the Gulf region"},
    {"tunisia", "Ministry of Religious Affairs, Tunisia"},
    {"algeria", "Ministry of Religious Affairs and Endowments, Algeria"},
    {"morocco", "Ministry of Habous and Islamic Affairs, Morocco"},
    {"portugal", "Islamic Community of Lisbon, Portugal"},
    {"moonsighting", "Moonsighting Committee Worldwide"},
    {"custom", "User-defined Fajr and Isha angles"},
};

static inline MethodDescription getMethodDescription(const char *key) {
  size_t size = sizeof(METHOD_DESCRIPTIONS) / sizeof(MethodDescription);

  for (size_t i = 0; i < size; i++) {
    if (strcmp(METHOD_DESCRIPTIONS[i].key, key) == 0) {
      const char *description = METHOD_DESCRIPTIONS[i].description;
      return (MethodDescription){.key = key, .description = description};
    }
  }

  // Fallback to Kemenag as default
  return (MethodDescription){.key = METHOD_DESCRIPTIONS[8].key,
                             .description = METHOD_DESCRIPTIONS[8].description};
}

#endif // MUSLIMTIFY_METHOD_DESCRIPTION_H
