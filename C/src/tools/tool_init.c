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
void registry_init_patch(void);
void registry_init_exec_code(void);
void registry_init_clarify(void);
void registry_init_memory(void);
void registry_init_todo(void);
void registry_init_process(void);
void registry_init_send_message(void);
void registry_init_cronjob(void);
void registry_init_skill_view(void);

/* Register all tools */
void tools_init_all(void) {
    registry_init_terminal();
    registry_init_file();
    registry_init_web();
    registry_init_skills();
    registry_init_patch();
    registry_init_exec_code();
    registry_init_clarify();
    registry_init_memory();
    registry_init_todo();
    registry_init_process();
    registry_init_send_message();
    registry_init_cronjob();
    registry_init_skill_view();
}
