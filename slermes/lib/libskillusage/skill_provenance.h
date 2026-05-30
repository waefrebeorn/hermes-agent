#ifndef HERMES_SKILL_PROVENANCE_H
#define HERMES_SKILL_PROVENANCE_H

/*
 * skill_provenance.h — Runtime skill write-origin tracking.
 *
 * Port of Python tools/skill_provenance.py.
 *
 * Tracks whether skill writes (skill_manage create/patch/edit) originate
 * from a foreground user action ("foreground") or from the background
 * review fork ("background_review"). The curator only consolidates/prunes
 * skills created under "background_review" (agent-sediment writes).
 *
 * Usage:
 *     // Set before tool execution
 *     skill_provenance_token_t token = skill_provenance_set("background_review");
 *     // ... run tools ...
 *     skill_provenance_reset(token);
 *
 *     // Check inside a tool handler
 *     if (skill_provenance_is_background_review()) {
 *         skill_usage_mark_agent_created(home, name);
 *     }
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/** Maximum length of a provenance origin string */
#define SKILL_PROVENANCE_MAX_ORIGIN 64

/** Default origin for foreground user-directed writes */
#define SKILL_PROVENANCE_FOREGROUND       "foreground"

/** Origin for background review fork writes (agent-sediment) */
#define SKILL_PROVENANCE_BACKGROUND_REVIEW "background_review"

/**
 * Opaque token returned by skill_provenance_set().
 * Pass to skill_provenance_reset() to restore prior value.
 */
typedef struct {
    char saved[SKILL_PROVENANCE_MAX_ORIGIN];
} skill_provenance_token_t;

/**
 * Set the active write origin and return a token for reset.
 * @param origin  Origin string ("foreground" or "background_review").
 *                NULL is treated as "foreground".
 * @return Token holding the previous origin value.
 */
skill_provenance_token_t skill_provenance_set(const char *origin);

/**
 * Restore the write origin to the value saved in token.
 * Must be called with the token returned from a prior skill_provenance_set().
 */
void skill_provenance_reset(skill_provenance_token_t token);

/**
 * Return the active write origin.
 * Default: "foreground" if never set.
 * Returns a pointer to an internal static buffer — NOT thread-safe for
 * concurrent reads + writes of the token. Safe for single-threaded use.
 */
const char *skill_provenance_get(void);

/**
 * Convenience: true iff the active write origin is "background_review".
 */
bool skill_provenance_is_background_review(void);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_SKILL_PROVENANCE_H */
