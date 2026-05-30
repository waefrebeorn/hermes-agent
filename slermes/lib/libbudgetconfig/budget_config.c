/*
 * budget_config.c — Configurable budget constants for tool result persistence.
 * Port of Python tools/budget_config.py.
 *
 * Per-tool resolution: pinned > tool_overrides > default.
 */

#include "budget_config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── Default singleton ── */
static budget_config_t g_default_config = {
    .default_result_size = BUDGET_DEFAULT_RESULT_SIZE_CHARS,
    .turn_budget         = BUDGET_DEFAULT_TURN_BUDGET_CHARS,
    .preview_size        = BUDGET_DEFAULT_PREVIEW_SIZE_CHARS,
    .overrides           = NULL,
};

/* ── Public API ── */

void budget_config_init(budget_config_t *cfg)
{
    if (!cfg) return;
    cfg->default_result_size = BUDGET_DEFAULT_RESULT_SIZE_CHARS;
    cfg->turn_budget         = BUDGET_DEFAULT_TURN_BUDGET_CHARS;
    cfg->preview_size        = BUDGET_DEFAULT_PREVIEW_SIZE_CHARS;
    cfg->overrides           = NULL;
}

void budget_config_set_override(budget_config_t *cfg,
                                 const char *tool_name, int threshold)
{
    if (!cfg || !tool_name) return;

    /* Check if override already exists, update it */
    for (budget_override_t *o = cfg->overrides; o; o = o->next) {
        if (strcmp(o->tool_name, tool_name) == 0) {
            o->threshold = threshold;
            return;
        }
    }

    /* Create new override */
    budget_override_t *o = (budget_override_t *)malloc(sizeof(budget_override_t));
    if (!o) return;
    snprintf(o->tool_name, sizeof(o->tool_name), "%s", tool_name);
    o->threshold = threshold;
    o->next = cfg->overrides;
    cfg->overrides = o;
}

int budget_config_resolve_threshold(const budget_config_t *cfg,
                                     const char *tool_name)
{
    if (!cfg || !tool_name)
        return cfg ? cfg->default_result_size : BUDGET_DEFAULT_RESULT_SIZE_CHARS;

    /* 1. Pinned: read_file always has infinite threshold */
    if (strcmp(tool_name, "read_file") == 0)
        return BUDGET_PINNED_READ_FILE_THRESHOLD;  /* -1 = infinite */

    /* 2. Tool overrides */
    for (const budget_override_t *o = cfg->overrides; o; o = o->next) {
        if (strcmp(o->tool_name, tool_name) == 0)
            return o->threshold;
    }

    /* 3. Default */
    return cfg->default_result_size;
}

void budget_config_cleanup(budget_config_t *cfg)
{
    if (!cfg) return;
    budget_override_t *o = cfg->overrides;
    while (o) {
        budget_override_t *next = o->next;
        free(o);
        o = next;
    }
    cfg->overrides = NULL;
}

const budget_config_t *budget_config_default(void)
{
    return &g_default_config;
}
