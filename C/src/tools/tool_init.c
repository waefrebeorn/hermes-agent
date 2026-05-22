/*
 * tool_init.c — Tool initialization for Hermes C.
 * Auto-discovery: calls all registry_init_*() functions.
 */

#include "hermes.h"

/* P168: File sandbox init */
void sandbox_init(void);

/* O14: Sandbox escape detection init */
void sandbox_escape_init(void);

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
void registry_init_session_search(void);
void registry_init_session_crud(void);
void registry_init_tts(void);
void registry_init_vision(void);
void registry_init_delegate(void);
void registry_init_x_search(void);
void registry_init_browser(void);
void registry_init_approval(void);
void registry_init_voice(void);
void registry_init_image_gen(void);
void registry_init_homeassistant(void);
void registry_init_kanban(void);
void registry_init_computer_use(void);
void registry_init_discord(void);
void registry_init_mcp(void);
void registry_init_file_batch(void);

/* Register all tools */
void tools_init_all(void) {
    /* P168: Initialize file sandbox before any tool registration */
    sandbox_init();

    /* O14: Initialize sandbox escape detection */
    sandbox_escape_init();

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
    registry_init_session_search();
    registry_init_session_crud();
    registry_init_tts();
    registry_init_vision();
    registry_init_delegate();
    registry_init_x_search();
    registry_init_browser();
    registry_init_approval();
    registry_init_voice();
    registry_init_image_gen();
    registry_init_homeassistant();
    registry_init_kanban();
    registry_init_computer_use();
    registry_init_discord();
    registry_init_mcp();

    /* F15: Batch file ops */
    registry_init_file_batch();

    /* P150: Assign toolsets for enabled/disabled filtering */
    registry_set_toolset("browser_navigate", "browser");
    registry_set_toolset("browser_snapshot", "browser");
    registry_set_toolset("browser_back", "browser");
    registry_set_toolset("browser_forward", "browser");
    registry_set_toolset("browser_click", "browser");
    registry_set_toolset("browser_type", "browser");
    registry_set_toolset("browser_scroll", "browser");
    registry_set_toolset("browser_get_images", "browser");
    registry_set_toolset("browser_press", "browser");
    registry_set_toolset("browser_vision", "browser");
    registry_set_toolset("browser_console", "browser");
    registry_set_toolset("browser_dialog", "browser");
    registry_set_toolset("browser_cdp", "browser");

    registry_set_toolset("kanban_show", "kanban");
    registry_set_toolset("kanban_list", "kanban");
    registry_set_toolset("kanban_complete", "kanban");
    registry_set_toolset("kanban_block", "kanban");
    registry_set_toolset("kanban_heartbeat", "kanban");
    registry_set_toolset("kanban_comment", "kanban");
    registry_set_toolset("kanban_create", "kanban");
    registry_set_toolset("kanban_link", "kanban");
    registry_set_toolset("kanban_unblock", "kanban");

    registry_set_toolset("ha_list_entities", "homeassistant");
    registry_set_toolset("ha_get_state", "homeassistant");
    registry_set_toolset("ha_list_services", "homeassistant");
    registry_set_toolset("ha_call_service", "homeassistant");

    registry_set_toolset("voice_listen", "voice");
    registry_set_toolset("voice_speak", "voice");

    registry_set_toolset("image_generate", "image_gen");
    registry_set_toolset("cronjob", "cron");
    registry_set_toolset("cron_cmd", "cron");
    registry_set_toolset("memory", "memory");
    registry_set_toolset("delegate_task", "delegate");
    registry_set_toolset("send_message", "send_message");
    registry_set_toolset("computer_use", "computer_use");
}
