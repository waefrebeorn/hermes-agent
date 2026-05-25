/*
 * skills_sync.c — Manifest-based seeding and updating of bundled skills.
 * Port of Python tools/skills_sync.py.
 *
 * Copies bundled skills from the repo's skills/ directory into
 * ~/.hermes/skills/ and uses a manifest to track which skills have
 * been synced and their origin hash.
 */

#include "skills_sync.h"
#include "hash.h"
#include "json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

/* ─── Constants ──────────────────────────────────────────── */

#define MAX_FILE_SIZE (10 * 1024 * 1024) /* 10 MB max per file for hashing */
#define MAX_HASH_BUF  (100 * 1024 * 1024) /* 100 MB max total for dir hashing */

/* ─── Path helpers ───────────────────────────────────────── */

const char *skills_sync_manifest_path(const char *hermes_home, char *out_path)
{
    snprintf(out_path, SKILLS_SYNC_MAX_PATH, "%s/skills/.bundled_manifest", hermes_home);
    return out_path;
}

/* ─── Manifest I/O ───────────────────────────────────────── */

void skills_sync_read_manifest(const char *hermes_home,
                                skills_sync_manifest_t *manifest)
{
    memset(manifest, 0, sizeof(*manifest));
    char path[SKILLS_SYNC_MAX_PATH];
    skills_sync_manifest_path(hermes_home, path);

    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[SKILLS_SYNC_MAX_LINE];
    while (fgets(line, sizeof(line), f) && manifest->count < SKILLS_SYNC_MAX_ENTRIES) {
        /* Strip trailing newline/cr */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';
        if (len == 0) continue;

        skills_sync_entry_t *e = &manifest->entries[manifest->count];

        if (strchr(line, ':')) {
            /* v2 format: name:hash */
            char *colon = strchr(line, ':');
            size_t name_len = (size_t)(colon - line);
            if (name_len >= SKILLS_SYNC_MAX_NAME) name_len = SKILLS_SYNC_MAX_NAME - 1;
            memcpy(e->name, line, name_len);
            e->name[name_len] = '\0';
            strncpy(e->hash, colon + 1, SKILLS_SYNC_MAX_HASH - 1);
        } else {
            /* v1 format: plain name */
            strncpy(e->name, line, SKILLS_SYNC_MAX_NAME - 1);
            e->hash[0] = '\0'; /* empty hash = needs migration */
        }
        manifest->count++;
    }
    fclose(f);
}

int skills_sync_write_manifest(const char *hermes_home,
                                const skills_sync_manifest_t *manifest)
{
    char path[SKILLS_SYNC_MAX_PATH];
    skills_sync_manifest_path(hermes_home, path);

    /* Ensure parent dir exists */
    char dir[SKILLS_SYNC_MAX_PATH];
    snprintf(dir, sizeof(dir), "%s/skills", hermes_home);
    mkdir(dir, 0755);

    /* Build content in memory */
    size_t cap = 8192;
    char *data = malloc(cap);
    if (!data) return -1;
    size_t pos = 0;

    /* Sort entries by name for deterministic output */
    /* Simple bubble sort — small N */
    skills_sync_entry_t sorted[SKILLS_SYNC_MAX_ENTRIES];
    memcpy(sorted, manifest->entries, manifest->count * sizeof(skills_sync_entry_t));
    for (int i = 0; i < manifest->count - 1; i++) {
        for (int j = 0; j < manifest->count - i - 1; j++) {
            if (strcmp(sorted[j].name, sorted[j+1].name) > 0) {
                skills_sync_entry_t tmp = sorted[j];
                sorted[j] = sorted[j+1];
                sorted[j+1] = tmp;
            }
        }
    }

    for (int i = 0; i < manifest->count; i++) {
        int needed = snprintf(NULL, 0, "%s:%s\n", sorted[i].name, sorted[i].hash);
        if (pos + needed + 1 > cap) {
            cap = cap * 2 + needed + 256;
            char *nb = realloc(data, cap);
            if (!nb) { free(data); return -1; }
            data = nb;
        }
        pos += sprintf(data + pos, "%s:%s\n", sorted[i].name, sorted[i].hash);
    }

    if (pos > 0 && data[pos-1] != '\n') {
        data[pos++] = '\n';
        data[pos] = '\0';
    }

    /* Atomic write: mkstemp -> write -> rename */
    char tmp_path[SKILLS_SYNC_MAX_PATH + 32];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmpXXXXXX", path);

    int fd = mkstemp(tmp_path);
    if (fd < 0) {
        free(data);
        return -1;
    }

    ssize_t written = write(fd, data, pos);
    (void)written;
    fsync(fd);
    close(fd);
    free(data);

    if (rename(tmp_path, path) != 0) {
        unlink(tmp_path);
        return -1;
    }

    return 0;
}

int skills_sync_find_manifest(const skills_sync_manifest_t *manifest,
                               const char *skill_name)
{
    if (!skill_name || !*skill_name) return -1;
    for (int i = 0; i < manifest->count; i++) {
        if (strcmp(manifest->entries[i].name, skill_name) == 0)
            return i;
    }
    return -1;
}

/* ─── SKILL.md name parsing ──────────────────────────────── */

void skills_sync_read_skill_name(const char *path,
                                  const char *fallback,
                                  char *out_name)
{
    strncpy(out_name, fallback ? fallback : "", SKILLS_SYNC_MAX_NAME - 1);

    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[4096];
    int in_frontmatter = 0;

    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';

        if (strcmp(line, "---") == 0) {
            if (in_frontmatter) break;
            in_frontmatter = 1;
            continue;
        }

        if (in_frontmatter && strncmp(line, "name:", 5) == 0) {
            const char *val = line + 5;
            while (*val == ' ') val++;
            if (*val == '"' || *val == '\'') {
                val++;
                const char *end = val;
                while (*end && *end != '"' && *end != '\'') end++;
                size_t vlen = (size_t)(end - val);
                if (vlen >= SKILLS_SYNC_MAX_NAME) vlen = SKILLS_SYNC_MAX_NAME - 1;
                memcpy(out_name, val, vlen);
                out_name[vlen] = '\0';
            } else {
                strncpy(out_name, val, SKILLS_SYNC_MAX_NAME - 1);
            }
            fclose(f);
            return;
        }
    }
    fclose(f);
}

/* ─── Directory hashing ──────────────────────────────────── */

/* Simple recursive directory walk for hashing */
static char *_hash_dir_recursive(const char *dir_path,
                                 char *buf, size_t *pos, size_t *cap)
{
    DIR *d = opendir(dir_path);
    if (!d) return buf;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char full[SKILLS_SYNC_MAX_PATH];
        snprintf(full, sizeof(full), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (stat(full, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            buf = _hash_dir_recursive(full, buf, pos, cap);
        } else if (S_ISREG(st.st_mode) && st.st_size <= MAX_FILE_SIZE) {
            /* Add relative path to hash */
            size_t name_len = strlen(entry->d_name);
            if (*pos + name_len + 1 > *cap) {
                *cap = *cap * 2 + name_len + 256;
                if (*cap > MAX_HASH_BUF) { *cap = MAX_HASH_BUF; break; }
                char *nb = realloc(buf, *cap);
                if (!nb) break;
                buf = nb;
            }
            memcpy(buf + *pos, entry->d_name, name_len);
            *pos += name_len;

            /* Read file and append contents */
            FILE *f = fopen(full, "rb");
            if (f) {
                size_t file_size = (size_t)st.st_size;
                if (*pos + file_size > *cap) {
                    *cap = *pos + file_size + 1024;
                    if (*cap > MAX_HASH_BUF) { *cap = MAX_HASH_BUF; }
                    char *nb = realloc(buf, *cap);
                    if (!nb) { fclose(f); break; }
                    buf = nb;
                }
                size_t n = fread(buf + *pos, 1, file_size, f);
                *pos += n;
                fclose(f);
            }
        }
    }
    closedir(d);
    return buf;
}

void skills_sync_dir_hash(const char *dir_path, char *out_hash)
{
    out_hash[0] = '\0';

    struct stat st;
    if (stat(dir_path, &st) != 0 || !S_ISDIR(st.st_mode))
        return;

    size_t cap = 65536;
    char *buf = malloc(cap);
    if (!buf) return;
    size_t pos = 0;

    buf = _hash_dir_recursive(dir_path, buf, &pos, &cap);

    if (pos > 0) {
        char *hex = hash_md5_hex((unsigned char *)buf, pos);
        if (hex) {
            snprintf(out_hash, SKILLS_SYNC_MAX_HASH, "%s", hex);
            free(hex);
        }
    }
    free(buf);
}

/* ─── Directory operations ───────────────────────────────── */

int skills_sync_copy_tree(const char *src, const char *dest)
{
    /* Create parent directory first */
    char parent[SKILLS_SYNC_MAX_PATH];
    snprintf(parent, sizeof(parent), "%s", dest);
    char *slash = strrchr(parent, '/');
    if (slash) {
        *slash = '\0';
        char cmd[SKILLS_SYNC_MAX_PATH + 32];
        snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", parent);
        system(cmd);
    }

    /* Use system cp -a for reliability */
    size_t cmd_len = strlen(src) + strlen(dest) + 64;
    char *cmd = malloc(cmd_len);
    if (!cmd) return -1;
    snprintf(cmd, cmd_len, "cp -a '%s' '%s'", src, dest);
    int ret = system(cmd);
    free(cmd);
    return (ret == 0) ? 0 : -1;
}

int skills_sync_remove_tree(const char *path)
{
    size_t cmd_len = strlen(path) + 32;
    char *cmd = malloc(cmd_len);
    if (!cmd) return -1;
    snprintf(cmd, cmd_len, "rm -rf '%s'", path);
    int ret = system(cmd);
    free(cmd);
    return (ret == 0) ? 0 : -1;
}

/* ─── Skill discovery ────────────────────────────────────── */

/* Callback for discovering SKILL.md files */
typedef struct {
    char names[SKILLS_SYNC_MAX_ENTRIES][SKILLS_SYNC_MAX_NAME];
    char dirs[SKILLS_SYNC_MAX_ENTRIES][SKILLS_SYNC_MAX_PATH];
    int count;
    const char *bundled_dir;
} discover_ctx_t;

static discover_ctx_t g_discover_ctx;

static void _discover_skills_in_dir(const char *dir_path, discover_ctx_t *ctx)
{
    DIR *d = opendir(dir_path);
    if (!d) return;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL && ctx->count < SKILLS_SYNC_MAX_ENTRIES) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char full[SKILLS_SYNC_MAX_PATH];
        snprintf(full, sizeof(full), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (stat(full, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            /* Check for SKILL.md inside */
            char skill_path[SKILLS_SYNC_MAX_PATH];
            snprintf(skill_path, sizeof(skill_path), "%s/SKILL.md", full);
            struct stat sst;
            if (stat(skill_path, &sst) == 0 && S_ISREG(sst.st_mode)) {
                /* Found a skill! */
                skills_sync_read_skill_name(skill_path, entry->d_name,
                                             ctx->names[ctx->count]);
                strncpy(ctx->dirs[ctx->count], full, SKILLS_SYNC_MAX_PATH - 1);
                ctx->count++;
            } else {
                /* Recurse into subdirectory (category nesting) */
                _discover_skills_in_dir(full, ctx);
            }
        }
    }
    closedir(d);
}

static void _compute_relative_dest(const char *skill_dir_path,
                                    const char *bundled_dir,
                                    const char *skills_dir,
                                    char *out_dest)
{
    out_dest[0] = '\0';
    size_t bd_len = strlen(bundled_dir);
    if (strncmp(skill_dir_path, bundled_dir, bd_len) == 0) {
        const char *rel = skill_dir_path + bd_len;
        if (*rel == '/') rel++;
        snprintf(out_dest, SKILLS_SYNC_MAX_PATH, "%s/%s", skills_dir, rel);
    } else {
        snprintf(out_dest, SKILLS_SYNC_MAX_PATH, "%s/%s", skills_dir,
                 skill_dir_path);
    }
}

/* ─── Main sync function ─────────────────────────────────── */

int skills_sync(const char *hermes_home,
                 const char *bundled_dir,
                 bool quiet,
                 skills_sync_result_t *out_result)
{
    memset(out_result, 0, sizeof(*out_result));

    /* Check bundled dir exists */
    struct stat bd_st;
    if (stat(bundled_dir, &bd_st) != 0 || !S_ISDIR(bd_st.st_mode))
        return 0; /* Not an error — no bundled dir = nothing to sync */

    /* Ensure user skills dir exists */
    char skills_dir[SKILLS_SYNC_MAX_PATH];
    snprintf(skills_dir, sizeof(skills_dir), "%s/skills", hermes_home);
    mkdir(skills_dir, 0755);

    /* Read manifest */
    skills_sync_manifest_t manifest;
    skills_sync_read_manifest(hermes_home, &manifest);

    /* Discover bundled skills */
    discover_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.bundled_dir = bundled_dir;
    _discover_skills_in_dir(bundled_dir, &ctx);

    out_result->total_bundled = ctx.count;

    /* Allocate arrays */
    out_result->copied = calloc(ctx.count, sizeof(char *));
    out_result->updated = calloc(ctx.count, sizeof(char *));
    out_result->user_modified = calloc(ctx.count, sizeof(char *));
    out_result->cleaned = calloc(ctx.count, sizeof(char *));
    if (!out_result->copied || !out_result->updated ||
        !out_result->user_modified || !out_result->cleaned) {
        skills_sync_free_result(out_result);
        return -1;
    }

    /* Track bundled names set */
    char bundled_names[SKILLS_SYNC_MAX_ENTRIES][SKILLS_SYNC_MAX_NAME];
    int bundled_count = ctx.count;
    for (int i = 0; i < ctx.count; i++)
        strncpy(bundled_names[i], ctx.names[i], SKILLS_SYNC_MAX_NAME - 1);

    /* Process each bundled skill */
    for (int i = 0; i < ctx.count; i++) {
        const char *skill_name = ctx.names[i];
        const char *skill_src = ctx.dirs[i];

        char dest[SKILLS_SYNC_MAX_PATH];
        _compute_relative_dest(skill_src, bundled_dir, skills_dir, dest);

        char bundled_hash[SKILLS_SYNC_MAX_HASH];
        skills_sync_dir_hash(skill_src, bundled_hash);

        int m_idx = skills_sync_find_manifest(&manifest, skill_name);

        if (m_idx < 0) {
            /* ── New skill ── */
            struct stat dest_st;
            if (stat(dest, &dest_st) == 0) {
                /* User already has a skill with this name */
                out_result->skipped++;
                char user_hash[SKILLS_SYNC_MAX_HASH];
                skills_sync_dir_hash(dest, user_hash);
                if (strcmp(user_hash, bundled_hash) == 0) {
                    /* Byte-identical — safe to track */
                    int ni = manifest.count;
                    strncpy(manifest.entries[ni].name, skill_name,
                            SKILLS_SYNC_MAX_NAME - 1);
                    strncpy(manifest.entries[ni].hash, bundled_hash,
                            SKILLS_SYNC_MAX_HASH - 1);
                    manifest.count++;
                } else if (!quiet) {
                    printf("  ⚠ %s: bundled version shipped but you already "
                           "have a local skill by this name — yours was kept.\n",
                           skill_name);
                }
            } else {
                /* Fresh copy */
                dest[0] = '\0';
                _compute_relative_dest(skill_src, bundled_dir, skills_dir, dest);

                if (skills_sync_copy_tree(skill_src, dest) == 0) {
                    out_result->copied[out_result->copied_count] = strdup(skill_name);
                    out_result->copied_count++;

                    int ni = manifest.count;
                    strncpy(manifest.entries[ni].name, skill_name,
                            SKILLS_SYNC_MAX_NAME - 1);
                    strncpy(manifest.entries[ni].hash, bundled_hash,
                            SKILLS_SYNC_MAX_HASH - 1);
                    manifest.count++;

                    if (!quiet)
                        printf("  + %s\n", skill_name);
                } else if (!quiet) {
                    printf("  ! Failed to copy %s\n", skill_name);
                }
            }

        } else {
            /* ── Existing skill ── */
            char *origin_hash = manifest.entries[m_idx].hash;
            struct stat dest_st;

            if (stat(dest, &dest_st) == 0) {
                /* On disk */
                char user_hash[SKILLS_SYNC_MAX_HASH];
                skills_sync_dir_hash(dest, user_hash);

                if (!origin_hash[0]) {
                    /* v1 migration: no origin hash. Set baseline */
                    strncpy(manifest.entries[m_idx].hash, user_hash,
                            SKILLS_SYNC_MAX_HASH - 1);
                    if (strcmp(user_hash, bundled_hash) == 0)
                        out_result->skipped++;
                    else
                        out_result->skipped++;
                    continue;
                }

                if (strcmp(user_hash, origin_hash) != 0) {
                    /* User modified this skill */
                    out_result->user_modified[out_result->user_modified_count] =
                        strdup(skill_name);
                    out_result->user_modified_count++;
                    if (!quiet)
                        printf("  ~ %s (user-modified, skipping)\n", skill_name);
                    continue;
                }

                /* User copy matches origin — check if bundled changed */
                if (strcmp(bundled_hash, origin_hash) != 0) {
                    /* Move old to backup */
                    char backup[SKILLS_SYNC_MAX_PATH];
                    snprintf(backup, sizeof(backup), "%s.bak", dest);
                    rename(dest, backup);

                    if (skills_sync_copy_tree(skill_src, dest) == 0) {
                        strncpy(manifest.entries[m_idx].hash, bundled_hash,
                                SKILLS_SYNC_MAX_HASH - 1);
                        out_result->updated[out_result->updated_count] =
                            strdup(skill_name);
                        out_result->updated_count++;
                        skills_sync_remove_tree(backup);
                        if (!quiet)
                            printf("  ↑ %s (updated)\n", skill_name);
                    } else {
                        /* Restore from backup */
                        if (stat(dest, &dest_st) != 0)
                            rename(backup, dest);
                        if (!quiet)
                            printf("  ! Failed to update %s\n", skill_name);
                    }
                } else {
                    out_result->skipped++;
                }
            } else {
                /* In manifest but not on disk — user deleted it */
                out_result->skipped++;
            }
        }
    }

    /* Clean stale manifest entries (skills removed from bundled) */
    for (int i = 0; i < manifest.count; i++) {
        bool found = false;
        for (int j = 0; j < bundled_count; j++) {
            if (strcmp(manifest.entries[i].name, bundled_names[j]) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            out_result->cleaned[out_result->cleaned_count] =
                strdup(manifest.entries[i].name);
            out_result->cleaned_count++;
            /* Remove by shifting */
            for (int k = i; k < manifest.count - 1; k++)
                manifest.entries[k] = manifest.entries[k+1];
            manifest.count--;
            i--; /* re-check this index */
        }
    }

    /* Also copy DESCRIPTION.md files for categories */
    DIR *bundled_d = opendir(bundled_dir);
    if (bundled_d) {
        struct dirent *bentry;
        while ((bentry = readdir(bundled_d)) != NULL) {
            if (strcmp(bentry->d_name, ".") == 0 || strcmp(bentry->d_name, "..") == 0)
                continue;
            char desc_src[SKILLS_SYNC_MAX_PATH];
            snprintf(desc_src, sizeof(desc_src), "%s/%s/DESCRIPTION.md",
                     bundled_dir, bentry->d_name);
            struct stat ds_st;
            if (stat(desc_src, &ds_st) == 0 && S_ISREG(ds_st.st_mode)) {
                char desc_dest[SKILLS_SYNC_MAX_PATH];
                snprintf(desc_dest, sizeof(desc_dest), "%s/%s/DESCRIPTION.md",
                         skills_dir, bentry->d_name);
                struct stat dd_st;
                if (stat(desc_dest, &dd_st) != 0) {
                    skills_sync_copy_tree(desc_src, desc_dest);
                }
            }
        }
        closedir(bundled_d);
    }

    /* Write updated manifest */
    skills_sync_write_manifest(hermes_home, &manifest);

    return 0;
}

void skills_sync_free_result(skills_sync_result_t *result)
{
    if (!result) return;
    for (int i = 0; i < result->copied_count; i++) free(result->copied[i]);
    for (int i = 0; i < result->updated_count; i++) free(result->updated[i]);
    for (int i = 0; i < result->user_modified_count; i++) free(result->user_modified[i]);
    for (int i = 0; i < result->cleaned_count; i++) free(result->cleaned[i]);
    free(result->copied);
    free(result->updated);
    free(result->user_modified);
    free(result->cleaned);
    memset(result, 0, sizeof(*result));
}

/* ─── Reset function ─────────────────────────────────────── */

int skills_sync_reset(const char *hermes_home,
                       const char *bundled_dir,
                       const char *name,
                       bool restore,
                       char *out_msg,
                       char *out_action)
{
    out_msg[0] = '\0';
    out_action[0] = '\0';

    skills_sync_manifest_t manifest;
    skills_sync_read_manifest(hermes_home, &manifest);

    int m_idx = skills_sync_find_manifest(&manifest, name);
    bool in_manifest = (m_idx >= 0);

    /* Check if skill exists in bundled */
    discover_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.bundled_dir = bundled_dir;
    _discover_skills_in_dir(bundled_dir, &ctx);
    bool is_bundled = false;
    char bundled_src[SKILLS_SYNC_MAX_PATH] = "";
    for (int i = 0; i < ctx.count; i++) {
        if (strcmp(ctx.names[i], name) == 0) {
            is_bundled = true;
            strncpy(bundled_src, ctx.dirs[i], SKILLS_SYNC_MAX_PATH - 1);
            break;
        }
    }

    if (!in_manifest && !is_bundled) {
        snprintf(out_msg, 512,
                 "'%s' is not a tracked bundled skill. Nothing to reset.", name);
        strncpy(out_action, "not_in_manifest", 64);
        return -1;
    }

    /* Step 1: drop manifest entry */
    if (in_manifest) {
        for (int i = m_idx; i < manifest.count - 1; i++)
            manifest.entries[i] = manifest.entries[i+1];
        manifest.count--;
        skills_sync_write_manifest(hermes_home, &manifest);
    }

    /* Step 2: optionally delete user copy */
    bool deleted_user_copy = false;
    if (restore) {
        if (!is_bundled) {
            snprintf(out_msg, 512,
                     "'%s' has no bundled source — manifest entry cleared "
                     "but cannot restore from bundled (skill was removed upstream).", name);
            strncpy(out_action, "bundled_missing", 64);
            return -1;
        }
        char skills_dir[SKILLS_SYNC_MAX_PATH];
        snprintf(skills_dir, sizeof(skills_dir), "%s/skills", hermes_home);
        char dest[SKILLS_SYNC_MAX_PATH];
        _compute_relative_dest(bundled_src, bundled_dir, skills_dir, dest);

        struct stat dest_st;
        if (stat(dest, &dest_st) == 0) {
            if (skills_sync_remove_tree(dest) == 0)
                deleted_user_copy = true;
        }
    }

    /* Step 3: re-run sync */
    skills_sync_result_t sync_result;
    skills_sync(hermes_home, bundled_dir, true, &sync_result);
    skills_sync_free_result(&sync_result);

    if (restore && deleted_user_copy) {
        snprintf(out_msg, 512, "Restored '%s' from bundled source.", name);
        strncpy(out_action, "restored", 64);
    } else if (restore) {
        snprintf(out_msg, 512, "Restored '%s' (re-copied from bundled).", name);
        strncpy(out_action, "restored", 64);
    } else {
        snprintf(out_msg, 512,
                 "Cleared manifest entry for '%s'. Future syncs will re-baseline.", name);
        strncpy(out_action, "manifest_cleared", 64);
    }
    return 0;
}
