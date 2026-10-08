#ifndef CLI_H
#define CLI_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Run CLI: dispatches argv[1] (or default "show") via dispatch table.
 * Returns: 0 on success, non-zero on error.
 */
int cli_run(int argc, char **argv);

/**
 * Print help message
 */
void cli_print_help(void);

#ifdef __cplusplus
}
#endif

#endif // CLI_H
