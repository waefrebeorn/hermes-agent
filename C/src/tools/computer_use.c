/*
 * computer_use.c — Computer use tool stub for Hermes C.
 * Real implementation requires macOS cua-driver MCP.
 * On Linux/WSL, returns platform-unavailable error.
 * MIT License — WuBu Hermes Project
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *handle_computer_use(const char *args_json, const char *task_id) {
    (void)args_json;
    (void)task_id;
    return strdup(
        "{\"error\":\"computer_use is not available on this platform. "
        "The computer_use tool requires macOS with cua-driver installed. "
        "On Linux/WSL (your current environment), it is not supported. "
        "Install Hermes on macOS or configure a macOS remote agent to use this feature.\"}");
}

void registry_init_computer_use(void) {
    registry_register("computer_use",
        "Control the user's local computer via mouse/keyboard/screenshot actions. "
        "Requires macOS with cua-driver installed. Not available on Linux/WSL.",
        "{\"type\":\"object\",\"properties\":{\"action\":{\"type\":\"string\",\"enum\":[\"key\",\"type\",\"mouse_move\",\"left_click\",\"left_click_drag\",\"right_click\",\"middle_click\",\"double_click\",\"triple_click\",\"scroll\",\"wait\",\"screenshot\",\"drag\",\"cursor_position\"],\"description\":\"Action to perform.\"},\"coordinate\":{\"type\":\"array\",\"items\":{\"type\":\"number\"},\"minItems\":2,\"maxItems\":2,\"description\":\"[x, y] coordinate for mouse actions.\"},\"text\":{\"type\":\"string\",\"description\":\"Text to type (for type action).\"},\"query\":{\"type\":\"string\",\"description\":\"Search query or application name.\"},\"scroll_amount\":{\"type\":\"number\",\"description\":\"Scroll amount in pixels.\"},\"scroll_direction\":{\"type\":\"string\",\"enum\":[\"up\",\"down\",\"left\",\"right\"],\"description\":\"Scroll direction.\"}},\"required\":[\"action\"]}",
        handle_computer_use);
}
