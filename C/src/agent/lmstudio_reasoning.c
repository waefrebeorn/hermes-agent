/*
 * lmstudio_reasoning.c — LM Studio reasoning-effort resolution.
 *
 * Port of Python agent/lmstudio_reasoning.py (48 lines).
 * Maps user's reasoning_config onto LM Studio's OpenAI-compatible
 * vocabulary, then clamps against the model's allowed_options.
 *
 * MIT License — WuBu Slermes Project
 */

#include "lmstudio_reasoning.h"
#include <string.h>
#include <stdio.h>

/* ── Valid effort set ──────────────────────────────────────── */

static const char *VALID_EFFORTS[] = {
    "none", "minimal", "low", "medium", "high", "xhigh", NULL
};

bool lmstudio_is_valid_effort(const char *effort) {
    if (!effort || !effort[0]) return false;
    for (int i = 0; VALID_EFFORTS[i]; i++) {
        if (strcmp(effort, VALID_EFFORTS[i]) == 0)
            return true;
    }
    return false;
}

/* ── Alias mapping ─────────────────────────────────────────── */

static const char *ALIAS_FROM[]  = {"off", "on", NULL};
static const char *ALIAS_TO[]    = {"none", "medium"};

const char *lmstudio_map_effort_alias(const char *effort) {
    if (!effort || !effort[0]) return effort;
    for (int i = 0; ALIAS_FROM[i]; i++) {
        if (strcmp(effort, ALIAS_FROM[i]) == 0)
            return ALIAS_TO[i];
    }
    return effort;
}

/* ── Resolution ────────────────────────────────────────────── */

const char *resolve_lmstudio_effort(bool enabled,
                                    const char *effort,
                                    const char *const *allowed_options)
{
    /* Default effort */
    const char *resolved = "medium";

    if (!enabled) {
        resolved = "none";
    } else if (effort && effort[0]) {
        /* Lowercase the effort into a static buffer */
        static char lower[64];
        size_t i;
        for (i = 0; effort[i] && i < sizeof(lower) - 1; i++)
            lower[i] = (char)(effort[i] >= 'A' && effort[i] <= 'Z'
                              ? effort[i] + 32 : effort[i]);
        lower[i] = '\0';

        /* Map aliases — returns either static alias string or lower */
        const char *mapped = lmstudio_map_effort_alias(lower);
        if (lmstudio_is_valid_effort(mapped))
            resolved = mapped;
    }

    /* Clamp against allowed_options if provided */
    if (allowed_options) {
        /* Build the allowed set with aliases applied */
        bool found = false;
        for (int j = 0; allowed_options[j]; j++) {
            const char *mapped_opt = lmstudio_map_effort_alias(allowed_options[j]);
            if (strcmp(resolved, mapped_opt) == 0) {
                found = true;
                break;
            }
        }
        if (!found)
            return NULL; /* Omit field — let LM Studio use model default */
    }

    return resolved;
}
