#ifndef HERMES_TOOL_CONFIG_H
#define HERMES_TOOL_CONFIG_H

/*
 * hermes_tool_config.h — P54: Tool dependency injection.
 *
 * Provides a shared config lookup for tool implementations.
 * Tools get API keys, endpoints, and settings from a single source
 * rather than calling getenv() directly with inconsistent names.
 *
 * Resolution order (first wins):
 *   1. Per-tool env var: <TOOL>_<KEY> (e.g., DISCORD_BOT_TOKEN)
 *   2. Generic env var: HERMES_<KEY> (e.g., HERMES_API_KEY)
 *   3. Config file key: tools.<tool_name>.<key>
 *   4. Vault (encrypted credential store via vault_retrieve)
 *   5. Default value (if provided)
 */

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Get a tool-specific config string.
 * tool_name: "discord", "feishu", "yuanbao", etc.
 * key: config key name (e.g., "api_key", "bot_token", "webhook_url")
 * Returns config value or NULL if not found. Returned pointer is
 * valid until next call (internal static buffer). */
const char *tool_config_get(const char *tool_name, const char *key);

/* Convenience: get API key for a tool.
 * Checks <TOOL>_API_KEY and HERMES_API_KEY env vars. */
const char *tool_config_get_api_key(const char *tool_name);

/* Convenience: get base URL for a tool.
 * Checks <TOOL>_BASE_URL and HERMES_BASE_URL env vars. */
const char *tool_config_get_base_url(const char *tool_name);

/* Convenience: get bot token for a tool (discord, telegram, etc.)
 * Checks <TOOL>_BOT_TOKEN, <TOOL>_TOKEN env vars. */
const char *tool_config_get_token(const char *tool_name);

/* Set a tool config value at runtime (for testing or in-memory overrides).
 * key and value are copied internally. */
void tool_config_set(const char *tool_name, const char *key, const char *value);

/* Clear all runtime overrides. */
void tool_config_clear(void);

/* Get a tool config as integer. Returns default_val if not found or invalid. */
int tool_config_get_int(const char *tool_name, const char *key, int default_val);

/* Get a tool config as boolean. Returns default_val if not found.
 * Accepts: \"true\"/\"false\", \"1\"/\"0\", \"yes\"/\"no\" (case-insensitive). */
bool tool_config_get_bool(const char *tool_name, const char *key, bool default_val);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_TOOL_CONFIG_H */
