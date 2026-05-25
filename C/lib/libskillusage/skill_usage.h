#ifndef HERMES_SKILL_USAGE_H
#define HERMES_SKILL_USAGE_H

/*
 * skill_usage.h — Skill usage telemetry + provenance tracking.
 * Port of Python tools/skill_usage.py.
 *
 * Tracks per-skill usage metadata in a sidecar JSON file
 * (~/.hermes/skills/.usage.json) keyed by skill name. Counters are
 * bumped by skill_view/skill_manage; the curator reads activity
 * timestamps to decide lifecycle transitions.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

/** Maximum skill name length */
#define SKILL_USAGE_MAX_NAME 128

/** Maximum JSON value string length */
#define SKILL_USAGE_MAX_VALUE 128

/** Maximum field string length */
#define SKILL_USAGE_MAX_FIELD 64

/** Maximum number of skills in the usage map */
#define SKILL_USAGE_MAX_SKILLS 64

/** Maximum path length */
#define SKILL_USAGE_MAX_PATH 1024

/** Valid lifecycle states */
#define SKILL_USAGE_STATE_ACTIVE   "active"
#define SKILL_USAGE_STATE_STALE    "stale"
#define SKILL_USAGE_STATE_ARCHIVED "archived"

/**
 * A single skill usage record.
 */
typedef struct {
    char    name[SKILL_USAGE_MAX_NAME];     /**< Skill name */
    char    created_by[SKILL_USAGE_MAX_FIELD]; /**< "agent" or NULL */
    int     use_count;                       /**< Number of times used */
    int     view_count;                      /**< Number of times viewed */
    int     patch_count;                     /**< Number of times patched */
    char    last_used_at[SKILL_USAGE_MAX_VALUE];   /**< ISO-8601 or NULL */
    char    last_viewed_at[SKILL_USAGE_MAX_VALUE]; /**< ISO-8601 or NULL */
    char    last_patched_at[SKILL_USAGE_MAX_VALUE];/**< ISO-8601 or NULL */
    char    created_at[SKILL_USAGE_MAX_VALUE];      /**< ISO-8601 creation time */
    char    state[SKILL_USAGE_MAX_FIELD];    /**< Lifecycle state: active/stale/archived */
    bool    pinned;                          /**< Opt-out from auto transitions */
    char    archived_at[SKILL_USAGE_MAX_VALUE];     /**< ISO-8601 or NULL */
} skill_usage_record_t;

/**
 * The full usage map loaded from the sidecar file.
 */
typedef struct {
    skill_usage_record_t records[SKILL_USAGE_MAX_SKILLS];
    int count;                               /**< Number of valid records */
} skill_usage_map_t;

/**
 * Get the path to the .usage.json sidecar file.
 * @param hermes_home  Path to HERMES_HOME.
 * @param out_path     Buffer for the resulting path (SKILL_USAGE_MAX_PATH).
 * @return out_path.
 */
const char *skill_usage_file_path(const char *hermes_home, char *out_path);

/**
 * Get the path to the skills archive directory.
 * @param hermes_home  Path to HERMES_HOME.
 * @param out_path     Buffer for the resulting path (SKILL_USAGE_MAX_PATH).
 * @return out_path.
 */
const char *skill_usage_archive_dir(const char *hermes_home, char *out_path);

/**
 * Load the entire .usage.json map.
 * Returns empty map on missing/corrupt file.
 * @param out_map  [out] Populated skill_usage_map_t.
 */
void skill_usage_load(const char *hermes_home, skill_usage_map_t *out_map);

/**
 * Save the usage map atomically (tempfile + rename).
 * @return 0 on success, -1 on error.
 */
int skill_usage_save(const char *hermes_home, const skill_usage_map_t *map);

/**
 * Get a record for a skill name, creating a fresh empty one if missing.
 * @param out_record  [out] Populated record (caller should check name[0]).
 */
void skill_usage_get_record(const skill_usage_map_t *map,
                             const char *skill_name,
                             skill_usage_record_t *out_record);

/**
 * Find a record index by skill name.
 * @return Index in map->records, or -1 if not found.
 */
int skill_usage_find(const skill_usage_map_t *map, const char *skill_name);

/**
 * Bump view_count and last_viewed_at for a skill.
 * Loads, mutates, saves atomically.
 * @return 0 on success, -1 on error.
 */
int skill_usage_bump_view(const char *hermes_home, const char *skill_name);

/**
 * Bump use_count and last_used_at for a skill.
 * @return 0 on success, -1 on error.
 */
int skill_usage_bump_use(const char *hermes_home, const char *skill_name);

/**
 * Bump patch_count and last_patched_at for a skill.
 * @return 0 on success, -1 on error.
 */
int skill_usage_bump_patch(const char *hermes_home, const char *skill_name);

/**
 * Mark a skill as agent-created (opts into curator management).
 * @return 0 on success, -1 on error.
 */
int skill_usage_mark_agent_created(const char *hermes_home, const char *skill_name);

/**
 * Set lifecycle state for a skill.
 * Valid states: SKILL_USAGE_STATE_ACTIVE, _STALE, _ARCHIVED.
 * @return 0 on success, -1 on error/invalid state.
 */
int skill_usage_set_state(const char *hermes_home, const char *skill_name,
                           const char *state);

/**
 * Set pinned flag for a skill.
 * @return 0 on success, -1 on error.
 */
int skill_usage_set_pinned(const char *hermes_home, const char *skill_name,
                            bool pinned);

/**
 * Drop a skill's usage entry entirely.
 * @return 0 on success, -1 on error.
 */
int skill_usage_forget(const char *hermes_home, const char *skill_name);

/**
 * Move a skill directory to the archive.
 * @param hermes_home  Path to HERMES_HOME.
 * @param skill_name   Skill name to archive.
 * @param out_msg      Buffer for result message (SKILL_USAGE_MAX_VALUE).
 * @return 0 on success, -1 on error.
 */
int skill_usage_archive(const char *hermes_home, const char *skill_name,
                         char *out_msg);

/**
 * Restore a skill from the archive back to ~/.hermes/skills/.
 * @param hermes_home  Path to HERMES_HOME.
 * @param skill_name   Skill name to restore.
 * @param out_msg      Buffer for result message (SKILL_USAGE_MAX_VALUE).
 * @return 0 on success, -1 on error.
 */
int skill_usage_restore(const char *hermes_home, const char *skill_name,
                          char *out_msg);

/**
 * Return the latest activity timestamp from a record (view/use/patch).
 * @param record     The usage record.
 * @param out_buf    Buffer for result (SKILL_USAGE_MAX_VALUE).
 * @return out_buf, or NULL if no activity timestamps found.
 */
const char *skill_usage_latest_activity(const skill_usage_record_t *record,
                                         char *out_buf);

/**
 * Return total activity count (use + view + patch).
 */
int skill_usage_activity_count(const skill_usage_record_t *record);

/**
 * Generate an ISO-8601 timestamp string.
 * @param out_buf  Buffer (minimum 32 bytes).
 */
void skill_usage_now_iso(char *out_buf);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_SKILL_USAGE_H */
