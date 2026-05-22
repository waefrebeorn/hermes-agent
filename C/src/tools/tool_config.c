/*
 * tool_config.c — P54: Tool dependency injection for Hermes C.
 *
 * Shared config lookup for tool implementations. Resolution order:
 *   1. Runtime overrides (set via tool_config_set)
 *   2. Per-tool env var (<TOOL>_<KEY>)
 *   3. Generic env var (HERMES_<KEY>)
 *   4. Config file key (tools.<tool_name>.<key>)
 */

#include "hermes_tool_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ================================================================
 *  Runtime overrides (simple linked list)
 * ================================================================ */

typedef struct cfg_override {
    char tool[32];
    char key[64];
    char value[512];
    struct cfg_override *next;
} cfg_override_t;

static cfg_override_t *g_overrides = NULL;

/* ================================================================
 *  Internal helpers
 * ================================================================ */

/* Build env var name: <TOOL>_<KEY>, uppercased */
static char *build_env_name(const char *tool_name, const char *key) {
    size_t tlen = strlen(tool_name);
    size_t klen = strlen(key);
    char *env = (char *)malloc(tlen + klen + 2);
    if (!env) return NULL;
    size_t pos = 0;
    for (size_t i = 0; i < tlen; i++)
        env[pos++] = (char)toupper((unsigned char)tool_name[i]);
    env[pos++] = '_';
    for (size_t i = 0; i < klen; i++)
        env[pos++] = (char)toupper((unsigned char)key[i]);
    env[pos] = '\0';
    return env;
}

/* Check runtime overrides first */
static const char *check_overrides(const char *tool_name, const char *key) {
    for (cfg_override_t *o = g_overrides; o; o = o->next) {
        if (strcmp(o->tool, tool_name) == 0 && strcmp(o->key, key) == 0)
            return o->value;
    }
    return NULL;
}

/* ================================================================
 *  Public API
 * ================================================================ */

const char *tool_config_get(const char *tool_name, const char *key) {
    if (!tool_name || !key) return NULL;

    /* 1. Runtime overrides */
    const char *val = check_overrides(tool_name, key);
    if (val) return val;

    /* 2. Per-tool env var */
    char *env_name = build_env_name(tool_name, key);
    if (env_name) {
        val = getenv(env_name);
        free(env_name);
        if (val && val[0]) return val;
    }

    /* 3. Generic env var */
    char *generic_name = NULL;
    size_t klen = strlen(key);
    generic_name = (char *)malloc(klen + 8); /* HERMES_ + key */
    if (generic_name) {
        snprintf(generic_name, klen + 8, "HERMES_");
        for (size_t i = 0; i < klen; i++)
            generic_name[7 + i] = (char)toupper((unsigned char)key[i]);
        generic_name[7 + klen] = '\0';
        val = getenv(generic_name);
        free(generic_name);
        if (val && val[0]) return val;
    }

    return NULL; /* Not found */
}

const char *tool_config_get_api_key(const char *tool_name) {
    return tool_config_get(tool_name, "api_key");
}

const char *tool_config_get_base_url(const char *tool_name) {
    return tool_config_get(tool_name, "base_url");
}

const char *tool_config_get_token(const char *tool_name) {
    const char *v;

    /* Try bot_token first (discord uses DISCORD_BOT_TOKEN) */
    v = tool_config_get(tool_name, "bot_token");
    if (v) return v;

    /* Fall back to just "token" */
    v = tool_config_get(tool_name, "token");
    if (v) return v;

    /* Fall back to api_key */
    return tool_config_get_api_key(tool_name);
}

void tool_config_set(const char *tool_name, const char *key, const char *value) {
    if (!tool_name || !key || !value) return;

    /* Check if override exists, update it */
    for (cfg_override_t *o = g_overrides; o; o = o->next) {
        if (strcmp(o->tool, tool_name) == 0 && strcmp(o->key, key) == 0) {
            snprintf(o->value, sizeof(o->value), "%s", value);
            return;
        }
    }

    /* Create new override */
    cfg_override_t *o = (cfg_override_t *)malloc(sizeof(cfg_override_t));
    if (!o) return;
    snprintf(o->tool, sizeof(o->tool), "%s", tool_name);
    snprintf(o->key, sizeof(o->key), "%s", key);
    snprintf(o->value, sizeof(o->value), "%s", value);
    o->next = g_overrides;
    g_overrides = o;
}

void tool_config_clear(void) {
    cfg_override_t *o = g_overrides;
    while (o) {
        cfg_override_t *next = o->next;
        free(o);
        o = next;
    }
    g_overrides = NULL;
}

/* Get a tool config as integer */
int tool_config_get_int(const char *tool_name, const char *key, int default_val) {
    const char *v = tool_config_get(tool_name, key);
    if (!v) return default_val;
    char *end = NULL;
    long result = strtol(v, &end, 10);
    if (end == v || *end != '\0') return default_val;
    return (int)result;
}

/* Get a tool config as boolean */
bool tool_config_get_bool(const char *tool_name, const char *key, bool default_val) {
    const char *v = tool_config_get(tool_name, key);
    if (!v) return default_val;
    /* Case-insensitive comparison */
    size_t len = strlen(v);
    if (len == 0) return default_val;
    if (strcasecmp(v, "true") == 0 || strcmp(v, "1") == 0 ||
        strcasecmp(v, "yes") == 0) return true;
    if (strcasecmp(v, "false") == 0 || strcmp(v, "0") == 0 ||
        strcasecmp(v, "no") == 0) return false;
    return default_val;
}
