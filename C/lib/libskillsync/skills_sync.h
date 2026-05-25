#ifndef HERMES_SKILLS_SYNC_H
#define HERMES_SKILLS_SYNC_H

/*
 * skills_sync.h — Manifest-based seeding and updating of bundled skills.
 * Port of Python tools/skills_sync.py.
 *
 * Copies bundled skills from the repo's skills/ directory into
 * ~/.hermes/skills/ and uses a manifest to track which skills have
 * been synced and their origin hash.
 *
 * Manifest format: each line is "skill_name:origin_hash" where
 * origin_hash is the MD5 of the bundled skill directory contents
 * at the time it was last synced to the user dir.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

/** Maximum skill name length */
#define SKILLS_SYNC_MAX_NAME 128

/** Maximum manifest path length */
#define SKILLS_SYNC_MAX_PATH 1024

/** Maximum hash string length (MD5 hex = 32) */
#define SKILLS_SYNC_MAX_HASH 33

/** Maximum manifest entries */
#define SKILLS_SYNC_MAX_ENTRIES 256

/** Maximum description text length */
#define SKILLS_SYNC_MAX_DESC 1024

/** Maximum line length in manifest */
#define SKILLS_SYNC_MAX_LINE 1024

/**
 * A single manifest entry.
 */
typedef struct {
    char name[SKILLS_SYNC_MAX_NAME];    /**< Skill name */
    char hash[SKILLS_SYNC_MAX_HASH];    /**< Origin MD5 hash (32 hex chars) */
} skills_sync_entry_t;

/**
 * Full manifest loaded from .bundled_manifest.
 */
typedef struct {
    skills_sync_entry_t entries[SKILLS_SYNC_MAX_ENTRIES];
    int count;                           /**< Number of valid entries */
} skills_sync_manifest_t;

/**
 * Result of a sync_skills() operation.
 */
typedef struct {
    char **copied;                        /**< Newly copied skill names (caller frees each + array) */
    int copied_count;
    char **updated;                       /**< Updated skill names (caller frees each + array) */
    int updated_count;
    char **user_modified;                 /**< User-modified skill names (caller frees each + array) */
    int user_modified_count;
    char **cleaned;                       /**< Stale manifest entries removed (caller frees each + array) */
    int cleaned_count;
    int skipped;                          /**< Skills unchanged or skipped */
    int total_bundled;                    /**< Total bundled skills found */
} skills_sync_result_t;

/**
 * Get the path to the manifest file (.bundled_manifest).
 * @param hermes_home  Path to HERMES_HOME.
 * @param out_path     Buffer for result (SKILLS_SYNC_MAX_PATH).
 * @return out_path.
 */
const char *skills_sync_manifest_path(const char *hermes_home, char *out_path);

/**
 * Read the manifest file into memory.
 * Handles both v1 (plain names) and v2 (name:hash) formats.
 * @param manifest  [out] Populated manifest struct.
 */
void skills_sync_read_manifest(const char *hermes_home,
                                skills_sync_manifest_t *manifest);

/**
 * Write the manifest file atomically (tempfile + rename) in v2 format.
 * @return 0 on success, -1 on error.
 */
int skills_sync_write_manifest(const char *hermes_home,
                                const skills_sync_manifest_t *manifest);

/**
 * Find a manifest entry by skill name.
 * @return Index in entries[] or -1 if not found.
 */
int skills_sync_find_manifest(const skills_sync_manifest_t *manifest,
                               const char *skill_name);

/**
 * Read the `name:` field from a SKILL.md YAML frontmatter.
 * @param path       Path to SKILL.md file.
 * @param fallback   Fallback name if frontmatter parsing fails.
 * @param out_name   Buffer for result (SKILLS_SYNC_MAX_NAME).
 */
void skills_sync_read_skill_name(const char *path,
                                  const char *fallback,
                                  char *out_name);

/**
 * Compute an MD5 hash of all file contents in a directory (recursive).
 * @param dir_path  Path to directory.
 * @param out_hash  Buffer for 32-char hex hash + null terminator.
 */
void skills_sync_dir_hash(const char *dir_path, char *out_hash);

/**
 * Copy a directory tree recursively.
 * @param src   Source path.
 * @param dest  Destination path (must not exist).
 * @return 0 on success, -1 on error.
 */
int skills_sync_copy_tree(const char *src, const char *dest);

/**
 * Remove a directory tree recursively.
 * @param path  Directory path to remove.
 * @return 0 on success, -1 on error.
 */
int skills_sync_remove_tree(const char *path);

/**
 * Sync bundled skills into ~/.hermes/skills/ using the manifest.
 *
 * @param hermes_home   Path to HERMES_HOME.
 * @param bundled_dir   Path to bundled skills directory (from repo).
 * @param quiet         If true, no output to stdout.
 * @param out_result    [out] Sync result (caller must free with skills_sync_free_result).
 * @return 0 on success, -1 on error.
 */
int skills_sync(const char *hermes_home,
                 const char *bundled_dir,
                 bool quiet,
                 skills_sync_result_t *out_result);

/**
 * Free a sync result struct.
 */
void skills_sync_free_result(skills_sync_result_t *result);

/**
 * Reset a bundled skill's manifest tracking so future syncs work normally.
 *
 * @param hermes_home   Path to HERMES_HOME.
 * @param bundled_dir   Path to bundled skills directory.
 * @param name          Skill name to reset.
 * @param restore       If true, also delete user copy and re-sync from bundled.
 * @param out_msg       Buffer for result message (512 chars).
 * @param out_action    [out] Action string: "manifest_cleared", "restored", or error.
 * @return 0 on success, -1 on error.
 */
int skills_sync_reset(const char *hermes_home,
                       const char *bundled_dir,
                       const char *name,
                       bool restore,
                       char *out_msg,
                       char *out_action);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_SKILLS_SYNC_H */
