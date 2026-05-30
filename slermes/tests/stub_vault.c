/* Stub for tool_config_get — returns NULL when vault is not linked */
#include <stddef.h>
#include "hermes_tool_config.h"
const char *tool_config_get(const char *section, const char *key) {
    (void)section; (void)key;
    return NULL;
}
