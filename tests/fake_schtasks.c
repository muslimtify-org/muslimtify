#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Stands in for schtasks.exe in tests. Appends its arguments as one line to the
// file named by MT_FAKE_LOG, exits 1 when its subcommand is listed in the
// space-separated MT_FAKE_FAIL, and prints a task XML for a successful
// /query /xml, disabled when MT_FAKE_DISABLED is 1.
int main(int argc, char **argv) {
  const char *log_path = getenv("MT_FAKE_LOG");
  if (log_path && log_path[0] != '\0') {
    FILE *log = fopen(log_path, "a");
    if (log) {
      for (int i = 1; i < argc; i++)
        fprintf(log, "%s%s", i > 1 ? " " : "", argv[i]);
      fputc('\n', log);
      fclose(log);
    }
  }

  if (argc < 2)
    return 1;

  const char *fail = getenv("MT_FAKE_FAIL");
  if (fail) {
    char padded[256];
    char key[64];
    snprintf(padded, sizeof(padded), " %s ", fail);
    snprintf(key, sizeof(key), " %s ", argv[1]);
    if (strstr(padded, key))
      return 1;
  }

  bool wants_xml = false;
  for (int i = 2; i < argc; i++) {
    if (strcmp(argv[i], "/xml") == 0)
      wants_xml = true;
  }

  if (strcmp(argv[1], "/query") == 0 && wants_xml) {
    const char *disabled = getenv("MT_FAKE_DISABLED");
    bool is_disabled = disabled && strcmp(disabled, "1") == 0;
    printf("<?xml version=\"1.0\" encoding=\"UTF-16\"?>\n"
           "<Task version=\"1.2\">\n"
           "  <Settings>\n"
           "%s"
           "    <MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>\n"
           "  </Settings>\n"
           "  <Triggers>\n"
           "    <TimeTrigger>\n"
           "      <Enabled>true</Enabled>\n"
           "    </TimeTrigger>\n"
           "  </Triggers>\n"
           "</Task>\n",
           is_disabled ? "    <Enabled>false</Enabled>\n" : "");
  }
  return 0;
}
