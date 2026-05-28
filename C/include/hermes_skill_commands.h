/*
 * hermes_skill_commands.h — Skill slash-command support for Hermes C.
 *
 * Port of Python agent/skill_commands.py: scan skills dir, build /slug
 * mapping, resolve user input, build formatted invocation messages.
 */
#ifndef HERMES_SKILL_COMMANDS_H
#define HERMES_SKILL_COMMANDS_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Types
 * ================================================================ */

#define SKILL_CMD_NAME_MAX    64
#define SKILL_CMD_DESC_MAX    512
#define SKILL_CMD_PATH_MAX    4096
#define SKILL_CMD_SLUG_MAX    128
#define MAX_SKILL_COMMANDS    256

/* A single skill command entry (/slug → info) */
typedef struct {
    char slug[SKILL_CMD_SLUG_MAX];       /* "/slug-name" */
    char name[SKILL_CMD_NAME_MAX];        /* original display name from frontmatter */
    char description[SKILL_CMD_DESC_MAX]; /* frontmatter description */
    char skill_path[SKILL_CMD_PATH_MAX];  /* absolute path to skill dir */
} skill_cmd_entry_t;

/* ================================================================
 *  Public API
 * ================================================================ */

/* Scan ~/.hermes/skills/ and rebuild the /slug cache.
 * Returns the number of skill commands found (0 if none or error). */
int skill_cmd_scan(void);

/* Get a skill command entry by /slug key (including leading slash).
 * Returns pointer to internal entry, or NULL if not found. */
const skill_cmd_entry_t *skill_cmd_get(const char *slug);

/* Get all cached skill commands. Sets *count to number of entries.
 * Returns pointer to internal array (valid until next scan). */
const skill_cmd_entry_t *skill_cmd_get_all(int *count);

/* Resolve a user-typed /command to its canonical /slug key.
 * Normalizes underscores to hyphens, case-insensitive.
 * Returns pointer to internal slug string, or NULL if no match. */
const char *skill_cmd_resolve(const char *command);

/* Build a formatted skill invocation message for the agent.
 * Loads the SKILL.md, strips frontmatter, formats with activation
 * note, skill directory, and supporting file references.
 *
 * Returns malloc'd string (caller must free) or NULL on error. */
char *skill_cmd_build_message(const char *slug, const char *user_args);

/* Re-scan skills dir and return number of changes (additions + removals).
 * Sets *added to number of new skills, *removed to number of removed.
 * Pass NULL for added/removed if not needed. */
int skill_cmd_rescan(int *added, int *removed);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_SKILL_COMMANDS_H */
