/*
 * tool_init.c — Tool initialization for Hermes C.
 * Auto-discovery: calls all registry_init_*() functions.
 */

#include "hermes.h"

/* Tool init declarations (defined in each tool's .c file) */
void registry_init_terminal(void);
void registry_init_file(void);
void registry_init_web(void);
void registry_init_skills(void);

/* Register all tools */
void tools_init_all(void) {
    registry_init_terminal();
    registry_init_file();
    registry_init_web();
    registry_init_skills();
}
