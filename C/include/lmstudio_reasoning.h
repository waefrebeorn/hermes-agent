/*
 * lmstudio_reasoning.h — LM Studio reasoning-effort resolution.
 *
 * Maps user's reasoning_config onto LM Studio's OpenAI-compatible
 * vocabulary, then clamps against the model's allowed_options so the
 * server doesn't 400 on an unsupported effort.
 *
 * Pure C port of Python agent/lmstudio_reasoning.py (48 lines).
 */

#ifndef HERMES_LMSTUDIO_REASONING_H
#define HERMES_LMSTUDIO_REASONING_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Valid effort names ────────────────────────────────────── */

/** Return true if *effort* is a valid LM Studio reasoning_effort value. */
bool lmstudio_is_valid_effort(const char *effort);

/** Map an effort alias (off→none, on→medium) or return raw string. */
const char *lmstudio_map_effort_alias(const char *effort);

/* ── Resolution ────────────────────────────────────────────── */

/**
 * resolve_lmstudio_effort — Return the reasoning_effort string to send
 * to LM Studio, or NULL meaning "omit the field".
 *
 * @param enabled         Whether reasoning is enabled (nil→true).
 * @param effort          User's effort choice "low"/"medium"/"high"/NULL.
 * @param allowed_options NULL-terminated array of model's allowed
 *                        options (e.g. {"off","on",NULL}), or NULL
 *                        to skip clamping.
 * @return                Interned string (do NOT free) or NULL.
 *
 * When the user picks a level the model can't honour, returns NULL so
 * the caller omits the field and lets LM Studio use the model's default.
 * When allowed_options is NULL (probe failed), returns the resolved
 * effort without clamping.
 */
const char *resolve_lmstudio_effort(bool enabled,
                                    const char *effort,
                                    const char *const *allowed_options);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_LMSTUDIO_REASONING_H */
