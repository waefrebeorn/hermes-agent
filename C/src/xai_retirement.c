/**
 * @file xai_retirement.c
 * @brief L04: Detect retired xAI models in Hermes config.
 *
 * xAI retired several models on May 15, 2026.
 * Source: https://docs.x.ai/developers/migration/may-15-retirement
 *
 * Walks all model slots in hermes_config_t and returns issues for
 * any reference to a retired xAI model. Pure logic — no I/O.
 */
#include "hermes_xai_retirement.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ================================================================
 *  Retired model table
 *  Per xAI migration guide (May 15, 2026)
 * ================================================================ */
typedef struct {
    const char *retired;
    const char *replacement;
    const char *reasoning_effort; /* NULL = default, "none" = force none */
    const char *note;
} retired_model_entry_t;

static const retired_model_entry_t RETIRED_MODELS[] = {
    {"grok-4",                      "grok-4.3", NULL,
     "ambiguous (reasoning vs non-reasoning) — defaulting to grok-4.3"},
    {"grok-4-0709",                 "grok-4.3", NULL, NULL},
    {"grok-4-fast",                 "grok-4.3", NULL,
     "ambiguous variant — verify reasoning vs non-reasoning intent"},
    {"grok-4-fast-reasoning",       "grok-4.3", NULL, NULL},
    {"grok-4-fast-non-reasoning",   "grok-4.3", "none", NULL},
    {"grok-4-1-fast",               "grok-4.3", NULL,
     "ambiguous variant — verify reasoning vs non-reasoning intent"},
    {"grok-4-1-fast-reasoning",     "grok-4.3", NULL, NULL},
    {"grok-4-1-fast-non-reasoning", "grok-4.3", "none", NULL},
    {"grok-code-fast-1",            "grok-4.3", NULL, NULL},
    {"grok-imagine-image-pro",      "grok-imagine-image-quality", NULL, NULL},
    {NULL, NULL, NULL, NULL} /* sentinel */
};

/* ================================================================
 *  Helpers
 * ================================================================ */

/** Static buffer for normalize result — not thread-safe. */
static char g_normalize_buf[128];

const char *hermes_xai_normalize(const char *model) {
    if (!model || !model[0]) return "";
    const char *p = model;
    /* Skip leading whitespace */
    while (*p && isspace((unsigned char)*p)) p++;
    /* Copy, lowercase */
    size_t i = 0;
    const char *prefixes[] = {"x-ai/", "xai/", NULL};
    for (int pi = 0; prefixes[pi]; pi++) {
        size_t plen = strlen(prefixes[pi]);
        if (strncasecmp(p, prefixes[pi], plen) == 0) {
            p += plen;
            break;
        }
    }
    for (; *p && i < sizeof(g_normalize_buf) - 1; p++) {
        g_normalize_buf[i++] = (char)tolower((unsigned char)*p);
    }
    /* Trim trailing whitespace */
    while (i > 0 && isspace((unsigned char)g_normalize_buf[i - 1])) i--;
    g_normalize_buf[i] = '\0';
    return g_normalize_buf;
}

bool hermes_xai_looks_like(const char *model) {
    if (!model || !model[0]) return false;
    const char *norm = hermes_xai_normalize(model);
    return strncmp(norm, "grok-", 5) == 0;
}

/* ================================================================
 *  Check a single model slot, add issue if retired
 * ================================================================ */
static void check_slot(const char *path, const char *model_val,
                       xai_retirement_result_t *result) {
    if (!hermes_xai_looks_like(model_val)) return;
    const char *norm = hermes_xai_normalize(model_val);

    for (int i = 0; RETIRED_MODELS[i].retired; i++) {
        if (strcmp(norm, RETIRED_MODELS[i].retired) != 0) continue;
        if (result->count >= MAX_RETIREMENT_ISSUES) return;

        xai_retirement_issue_t *issue = &result->issues[result->count++];
        snprintf(issue->config_path, sizeof(issue->config_path), "%s", path);
        snprintf(issue->current_model, sizeof(issue->current_model), "%s", model_val);
        snprintf(issue->replacement, sizeof(issue->replacement), "%s",
                 RETIRED_MODELS[i].replacement);
        if (RETIRED_MODELS[i].reasoning_effort)
            snprintf(issue->reasoning_effort, sizeof(issue->reasoning_effort), "%s",
                     RETIRED_MODELS[i].reasoning_effort);
        else
            issue->reasoning_effort[0] = '\0';
        if (RETIRED_MODELS[i].note)
            snprintf(issue->note, sizeof(issue->note), "%s",
                     RETIRED_MODELS[i].note);
        else
            issue->note[0] = '\0';
        return;
    }
}

/* ================================================================
 *  Walk all config slots
 * ================================================================ */
xai_retirement_result_t hermes_xai_find_retired(const hermes_config_t *cfg) {
    xai_retirement_result_t result = {0};
    if (!cfg) return result;

    /* principal.model */
    check_slot("principal.model", cfg->model, &result);

    /* delegation.model */
    if (cfg->delegation.child_model[0])
        check_slot("delegation.model", cfg->delegation.child_model, &result);

    /* tts.xai.model — check if any TTS provider is xAI */
    /* (No direct model slot in tts_config_t — TTS uses provider config) */

    /* auxiliary.* — walk all 11 sub-tasks */
    /* Vision */
    if (cfg->auxiliary.vision.model[0])
        check_slot("auxiliary.vision.model", cfg->auxiliary.vision.model, &result);
    /* Web extract */
    if (cfg->auxiliary.web_extract.model[0])
        check_slot("auxiliary.web_extract.model", cfg->auxiliary.web_extract.model, &result);
    /* Compression */
    if (cfg->auxiliary.compression.model[0])
        check_slot("auxiliary.compression.model", cfg->auxiliary.compression.model, &result);
    /* Skills hub */
    if (cfg->auxiliary.skills_hub.model[0])
        check_slot("auxiliary.skills_hub.model", cfg->auxiliary.skills_hub.model, &result);
    /* Approval */
    if (cfg->auxiliary.approval.model[0])
        check_slot("auxiliary.approval.model", cfg->auxiliary.approval.model, &result);
    /* MCP */
    if (cfg->auxiliary.mcp.model[0])
        check_slot("auxiliary.mcp.model", cfg->auxiliary.mcp.model, &result);
    /* Title generation */
    if (cfg->auxiliary.title_generation.model[0])
        check_slot("auxiliary.title_generation.model", cfg->auxiliary.title_generation.model, &result);
    /* Triage specifier */
    if (cfg->auxiliary.triage_specifier.model[0])
        check_slot("auxiliary.triage_specifier.model", cfg->auxiliary.triage_specifier.model, &result);
    /* Kanban decomposer */
    if (cfg->auxiliary.kanban_decomposer.model[0])
        check_slot("auxiliary.kanban_decomposer.model", cfg->auxiliary.kanban_decomposer.model, &result);
    /* Profile describer */
    if (cfg->auxiliary.profile_describer.model[0])
        check_slot("auxiliary.profile_describer.model", cfg->auxiliary.profile_describer.model, &result);
    /* Curator */
    if (cfg->auxiliary.curator.model[0])
        check_slot("auxiliary.curator.model", cfg->auxiliary.curator.model, &result);

    return result;
}

/* ================================================================
 *  Format helpers
 * ================================================================ */
char *hermes_xai_format_issue(const xai_retirement_issue_t *issue) {
    if (!issue) return NULL;

    char buf[1024];
    int n;

    if (issue->reasoning_effort[0] && issue->note[0]) {
        n = snprintf(buf, sizeof(buf),
            "%s: model '%s' was retired %s. "
            "Replace with: %s (set reasoning_effort=%s) — %s\n"
            "  See: %s",
            issue->config_path, issue->current_model,
            XAI_RETIREMENT_DATE,
            issue->replacement, issue->reasoning_effort,
            issue->note,
            XAI_MIGRATION_GUIDE_URL);
    } else if (issue->reasoning_effort[0]) {
        n = snprintf(buf, sizeof(buf),
            "%s: model '%s' was retired %s. "
            "Replace with: %s (set reasoning_effort=%s)\n"
            "  See: %s",
            issue->config_path, issue->current_model,
            XAI_RETIREMENT_DATE,
            issue->replacement, issue->reasoning_effort,
            XAI_MIGRATION_GUIDE_URL);
    } else if (issue->note[0]) {
        n = snprintf(buf, sizeof(buf),
            "%s: model '%s' was retired %s. "
            "Replace with: %s — %s\n"
            "  See: %s",
            issue->config_path, issue->current_model,
            XAI_RETIREMENT_DATE,
            issue->replacement, issue->note,
            XAI_MIGRATION_GUIDE_URL);
    } else {
        n = snprintf(buf, sizeof(buf),
            "%s: model '%s' was retired %s. "
            "Replace with: %s\n"
            "  See: %s",
            issue->config_path, issue->current_model,
            XAI_RETIREMENT_DATE,
            issue->replacement,
            XAI_MIGRATION_GUIDE_URL);
    }

    if (n < 0) return strdup("");
    if ((size_t)n >= sizeof(buf)) {
        char *big = (char *)malloc((size_t)n + 1);
        if (!big) return NULL;
        if (issue->reasoning_effort[0] && issue->note[0])
            snprintf(big, (size_t)n + 1,
                "%s: model '%s' was retired %s. "
                "Replace with: %s (set reasoning_effort=%s) — %s\n"
                "  See: %s",
                issue->config_path, issue->current_model,
                XAI_RETIREMENT_DATE,
                issue->replacement, issue->reasoning_effort,
                issue->note, XAI_MIGRATION_GUIDE_URL);
        else if (issue->reasoning_effort[0])
            snprintf(big, (size_t)n + 1,
                "%s: model '%s' was retired %s. "
                "Replace with: %s (set reasoning_effort=%s)\n"
                "  See: %s",
                issue->config_path, issue->current_model,
                XAI_RETIREMENT_DATE,
                issue->replacement, issue->reasoning_effort,
                XAI_MIGRATION_GUIDE_URL);
        else if (issue->note[0])
            snprintf(big, (size_t)n + 1,
                "%s: model '%s' was retired %s. "
                "Replace with: %s — %s\n"
                "  See: %s",
                issue->config_path, issue->current_model,
                XAI_RETIREMENT_DATE,
                issue->replacement, issue->note,
                XAI_MIGRATION_GUIDE_URL);
        else
            snprintf(big, (size_t)n + 1,
                "%s: model '%s' was retired %s. "
                "Replace with: %s\n"
                "  See: %s",
                issue->config_path, issue->current_model,
                XAI_RETIREMENT_DATE,
                issue->replacement,
                XAI_MIGRATION_GUIDE_URL);
        return big;
    }
    return strdup(buf);
}
