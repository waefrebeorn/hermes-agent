/*
 * skill_mgmt.c — Skill management tools for Hermes C.
 *
 * skill_view: load and display a skill's content.
 * skill_manage: create, edit, patch, delete, list skills, manage supporting files.
 * skills_list: search and browse available skills.
 *
 * Maps to Python tools/skill_manager_tool.py actions:
 *   create, edit (full rewrite), patch (find-and-replace),
 *   delete, write_file, remove_file.
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include "skill_provenance.h"
#include "difflib.h"
#include "skill_usage.h"

/* ================================================================
 *  Constants
 * ================================================================ */

#define MAX_NAME_LEN         64
#define MAX_CONTENT_CHARS    100000
#define MAX_FILE_BYTES       (1024 * 1024)   /* 1 MiB */
#define MAX_DESC_LEN         1024
#define MAX_PATH             4096

static const char *ALLOWED_SUBDIRS[] = {
    "references", "templates", "scripts", "assets", NULL
};

/* ================================================================
 *  Helpers
 * ================================================================ */

static char *get_skills_dir(void) {
    static char buf[MAX_PATH];

    /* Check configured skill_search_paths env var first (comma-sep paths) */
    const char *cfg_paths = getenv("HERMES_SKILL_SEARCH_PATHS");
    if (cfg_paths && cfg_paths[0]) {
        /* Return first configured path */
        const char *comma = strchr(cfg_paths, ',');
        size_t len = comma ? (size_t)(comma - cfg_paths) : strlen(cfg_paths);
        if (len > 0 && len < MAX_PATH) {
            memcpy(buf, cfg_paths, len);
            buf[len] = '\0';
            /* Trim trailing whitespace */
            while (len > 0 && (buf[len-1] == ' ' || buf[len-1] == '\t'))
                buf[--len] = '\0';
            struct stat st;
            if (stat(buf, &st) == 0 && S_ISDIR(st.st_mode))
                return buf;
        }
    }

    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = "/tmp";

    /* Try ~/.hermes/skills first */
    snprintf(buf, sizeof(buf), "%s/.hermes/skills", home);

    struct stat st;
    if (stat(buf, &st) == 0 && S_ISDIR(st.st_mode))
        return buf;

    /* Fallback: check direct skills dir */
    snprintf(buf, sizeof(buf), "%s/skills", home);
    if (stat(buf, &st) == 0 && S_ISDIR(st.st_mode))
        return buf;

    /* Default to ~/.hermes/skills even if it doesn't exist yet */
    snprintf(buf, sizeof(buf), "%s/.hermes/skills", home);
    return buf;
}

/* Read file content (basic fread wrapper). Returns malloc'd string or NULL. */
static char *read_skill_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz > MAX_FILE_BYTES) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';
    return buf;
}

/* Write file content. Returns true on success. */
static bool write_skill_file(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    if (!f) return false;
    size_t len = strlen(content);
    size_t n = fwrite(content, 1, len, f);
    fclose(f);
    return n == len;
}

/* Check if a path is within an allowed subdirectory */
static bool is_allowed_subdir(const char *file_path) {
    if (!file_path) return false;
    for (int i = 0; ALLOWED_SUBDIRS[i]; i++) {
        size_t plen = strlen(ALLOWED_SUBDIRS[i]);
        if (strncmp(file_path, ALLOWED_SUBDIRS[i], plen) == 0 &&
            (file_path[plen] == '/' || file_path[plen] == '\0'))
            return true;
    }
    return false;
}

/* Validate skill name (filesystem-safe, lowercase, alphanumeric + ._-) */
static const char *validate_name(const char *name) {
    if (!name || !*name) return "Skill name is required.";
    size_t len = strlen(name);
    if (len > MAX_NAME_LEN) return "Skill name exceeds 64 characters.";
    if (!isalnum((unsigned char)name[0]))
        return "Skill name must start with a letter or digit.";
    for (size_t i = 0; i < len; i++) {
        char c = name[i];
        if (!isalnum((unsigned char)c) && c != '-' && c != '_' && c != '.')
            return "Skill name contains invalid characters. Use a-z, 0-9, -, _, .";
    }
    return NULL;
}

/* Build the full path to a skill's SKILL.md */
static void skill_sk_path(const char *skills_dir, const char *name,
                          char *buf, size_t bufsz) {
    snprintf(buf, bufsz, "%s/%s/SKILL.md", skills_dir, name);
}

/* Build the full path to a skill directory */
static void skill_dir_path(const char *skills_dir, const char *name,
                           char *buf, size_t bufsz) {
    snprintf(buf, bufsz, "%s/%s", skills_dir, name);
}

/* Recursive mkdir -p */
static bool mkdir_p(const char *path) {
    char tmp[MAX_PATH];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    return mkdir(tmp, 0755) == 0 || errno == EEXIST;
}

/* ================================================================
 *  skill_view handler
 * ================================================================ */

char *skill_view_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *name = json_object_get_string(args, "name", NULL);
    const char *file_path = json_object_get_string(args, "file_path", NULL);

    json_node_t *result = json_new_object();

    if (!name) {
        json_object_set(result, "error", json_new_string("Missing 'name'"));
    } else {
        char skill_dir[MAX_PATH];
        skill_dir_path(get_skills_dir(), name, skill_dir, sizeof(skill_dir));

        char sk_path[MAX_PATH];
        skill_sk_path(get_skills_dir(), name, sk_path, sizeof(sk_path));

        char *content = read_skill_file(sk_path);
        if (!content) {
            /* Try as single .md file */
            snprintf(sk_path, sizeof(sk_path), "%s/%s.md", get_skills_dir(), name);
            content = read_skill_file(sk_path);
        }

        if (content) {
            json_object_set(result, "name", json_new_string(name));
            json_object_set(result, "content", json_new_string(content));
            free(content);
        } else {
            json_object_set(result, "error", json_new_string("Skill not found"));
        }

        if (file_path && content) {
            char fp[MAX_PATH];
            snprintf(fp, sizeof(fp), "%s/%s", skill_dir, file_path);
            char *fc = read_skill_file(fp);
            if (fc) {
                json_object_set(result, "file_content", json_new_string(fc));
                free(fc);
            }
        }
    }

    char *json_out = json_serialize(result);
    json_free(result);
    json_free(args);
    return json_out;
}

/* ================================================================
 *  skill_manage handler — full CRUD + file management
 * ================================================================ */

/* Create a new skill with SKILL.md structure */
static char *action_create(json_node_t *args, const char *skills_dir) {
    const char *name = json_object_get_string(args, "name", NULL);
    const char *content = json_object_get_string(args, "content", NULL);
    const char *category = json_object_get_string(args, "category", NULL);

    const char *err = validate_name(name);
    if (err) {
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string(err));
        char *s = json_serialize(r); json_free(r); return s;
    }

    if (!content || !*content) {
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string("Content is required"));
        char *s = json_serialize(r); json_free(r); return s;
    }

    if (strlen(content) > MAX_CONTENT_CHARS) {
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string("Content exceeds 100,000 characters"));
        char *s = json_serialize(r); json_free(r); return s;
    }

    if (category && strlen(category) > 64) {
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string("Category name too long"));
        char *s = json_serialize(r); json_free(r); return s;
    }

    /* Build target directory */
    char skill_dir[MAX_PATH];
    if (category && *category) {
        snprintf(skill_dir, sizeof(skill_dir),
                 "%s/%s/%s", skills_dir, category, name);
    } else {
        snprintf(skill_dir, sizeof(skill_dir),
                 "%s/%s", skills_dir, name);
    }

    /* Check if already exists */
    char sk_path[MAX_PATH];
    snprintf(sk_path, sizeof(sk_path), "%s/SKILL.md", skill_dir);
    struct stat st;
    if (stat(sk_path, &st) == 0) {
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string("Skill already exists"));
        char *s = json_serialize(r); json_free(r); return s;
    }

    /* Create directory structure */
    if (!mkdir_p(skill_dir)) {
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string("Failed to create skill directory"));
        char *s = json_serialize(r); json_free(r); return s;
    }

    /* Write SKILL.md */
    /* Backup before editing */
    {
        FILE *fsrc = fopen(sk_path, "rb");
        if (fsrc) {
            fseek(fsrc, 0, SEEK_END);
            long fsz = ftell(fsrc);
            rewind(fsrc);
            if (fsz > 0 && fsz < 10485760) {
                char *bak = (char *)malloc((size_t)fsz + 1);
                if (bak) {
                    size_t n = fread(bak, 1, (size_t)fsz, fsrc);
                    bak[n] = '\0';
                    fclose(fsrc);
                    char bak_path[MAX_PATH];
                    snprintf(bak_path, sizeof(bak_path), "%s.bak", sk_path);
                    write_skill_file(bak_path, bak);
                    free(bak);
                } else { fclose(fsrc); }
            } else { fclose(fsrc); }
        }
    }
    if (!write_skill_file(sk_path, content)) {
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string("Failed to write SKILL.md"));
        char *s = json_serialize(r); json_free(r); return s;
    }

    /* Create subdirectories */
    for (int i = 0; ALLOWED_SUBDIRS[i]; i++) {
        char sub[MAX_PATH];
        snprintf(sub, sizeof(sub), "%s/%s", skill_dir, ALLOWED_SUBDIRS[i]);
        mkdir(sub, 0755);
    }

    /* If background review fork, mark skill as agent-created for curator */
    if (skill_provenance_is_background_review()) {
        const char *home = getenv("HERMES_HOME");
        if (!home) home = getenv("HOME");
        if (home) {
            skill_usage_mark_agent_created(home, name);
        }
    }

    json_node_t *result = json_new_object();
    json_object_set(result, "ok", json_new_bool(true));
    json_object_set(result, "action", json_new_string("create"));
    json_object_set(result, "skill_dir", json_new_string(skill_dir));
    json_object_set(result, "name", json_new_string(name));
    if (category && *category)
        json_object_set(result, "category", json_new_string(category));
    char *s = json_serialize(result); json_free(result); return s;
}

/* Edit (full rewrite) an existing skill's SKILL.md */
static char *action_edit(json_node_t *args, const char *skills_dir) {
    const char *name = json_object_get_string(args, "name", NULL);
    const char *content = json_object_get_string(args, "content", NULL);

    if (!name || !*name) {
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string("Missing 'name'"));
        char *s = json_serialize(r); json_free(r); return s;
    }

    if (!content || !*content) {
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string("Content is required"));
        char *s = json_serialize(r); json_free(r); return s;
    }

    if (strlen(content) > MAX_CONTENT_CHARS) {
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string("Content exceeds 100,000 characters"));
        char *s = json_serialize(r); json_free(r); return s;
    }

    char sk_path[MAX_PATH];
    skill_sk_path(skills_dir, name, sk_path, sizeof(sk_path));

    struct stat st;
    if (stat(sk_path, &st) != 0) {
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string("Skill not found"));
        char *s = json_serialize(r); json_free(r); return s;
    }

    /* Backup before editing */
    {
        FILE *fsrc = fopen(sk_path, "rb");
        if (fsrc) {
            fseek(fsrc, 0, SEEK_END);
            long fsz = ftell(fsrc);
            rewind(fsrc);
            if (fsz > 0 && fsz < 10485760) {
                char *bak = (char *)malloc((size_t)fsz + 1);
                if (bak) {
                    size_t n = fread(bak, 1, (size_t)fsz, fsrc);
                    bak[n] = '\0';
                    fclose(fsrc);
                    char bak_path[MAX_PATH];
                    snprintf(bak_path, sizeof(bak_path), "%s.bak", sk_path);
                    write_skill_file(bak_path, bak);
                    free(bak);
                } else { fclose(fsrc); }
            } else { fclose(fsrc); }
        }
    }
    if (!write_skill_file(sk_path, content)) {
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string("Failed to write SKILL.md"));
        char *s = json_serialize(r); json_free(r); return s;
    }

    json_node_t *result = json_new_object();
    json_object_set(result, "ok", json_new_bool(true));
    json_object_set(result, "action", json_new_string("edit"));
    json_object_set(result, "name", json_new_string(name));
    char *s = json_serialize(result); json_free(result); return s;
}

/* Patch — find-and-replace in SKILL.md or supporting file */
static char *action_patch(json_node_t *args, const char *skills_dir) {
    const char *name = json_object_get_string(args, "name", NULL);
    const char *old_text = json_object_get_string(args, "old_string", NULL);
    const char *new_text = json_object_get_string(args, "new_string", NULL);
    const char *file_path = json_object_get_string(args, "file_path", NULL);
    bool replace_all = json_object_get_bool(args, "replace_all", false);

    if (!name || !*name) {
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string("Missing 'name'"));
        char *s = json_serialize(r); json_free(r); return s;
    }
    if (!old_text) {
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string("Missing 'old_string'"));
        char *s = json_serialize(r); json_free(r); return s;
    }

    /* Determine target file */
    char target_path[MAX_PATH];
    if (file_path && *file_path) {
        char skill_dir[MAX_PATH];
        skill_dir_path(skills_dir, name, skill_dir, sizeof(skill_dir));
        snprintf(target_path, sizeof(target_path),
                 "%s/%s", skill_dir, file_path);
    } else {
        skill_sk_path(skills_dir, name, target_path, sizeof(target_path));
    }

    struct stat st;
    if (stat(target_path, &st) != 0) {
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string("Target file not found"));
        char *s = json_serialize(r); json_free(r); return s;
    }

    char *content = read_skill_file(target_path);
    if (!content) {
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string("Failed to read target file"));
        char *s = json_serialize(r); json_free(r); return s;
    }

    /* Perform replacement */
    size_t old_len = strlen(old_text);
    size_t new_len = new_text ? strlen(new_text) : 0;

    /* Count occurrences */
    int count = 0;
    const char *pos = content;
    while ((pos = strstr(pos, old_text)) != NULL) { count++; pos += old_len; }

    if (count == 0) {
        free(content);
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string("old_string not found in file"));
        char *s = json_serialize(r); json_free(r); return s;
    }

    if (!replace_all && count > 1) {
        free(content);
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string(
            "old_string matches multiple times. Set replace_all=true or provide more context."));
        char *s = json_serialize(r); json_free(r); return s;
    }

    /* Build replacement string */
    size_t content_len = strlen(content);
    size_t result_len = content_len + (size_t)count * (new_len - old_len) + 1;
    char *result_str = (char *)malloc(result_len);
    if (!result_str) { free(content);
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string("OOM"));
        char *s = json_serialize(r); json_free(r); return s;
    }

    char *dst = result_str;
    const char *src = content;
    int replaced = 0;
    while (*src) {
        if (replaced < count || replace_all) {
            const char *match = strstr(src, old_text);
            if (match) {
                size_t pre_len = (size_t)(match - src);
                memcpy(dst, src, pre_len);
                dst += pre_len;
                if (new_text) {
                    memcpy(dst, new_text, new_len);
                    dst += new_len;
                }
                src = match + old_len;
                replaced++;
                continue;
            }
        }
        *dst++ = *src++;
    }
    *dst = '\0';
    free(content);

    if (!write_skill_file(target_path, result_str)) {
        free(result_str);
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string("Failed to write file"));
        char *s = json_serialize(r); json_free(r); return s;
    }

    json_node_t *result = json_new_object();
    json_object_set(result, "ok", json_new_bool(true));
    json_object_set(result, "action", json_new_string("patch"));
    json_object_set(result, "name", json_new_string(name));
    json_object_set(result, "replacements", json_new_number((double)replaced));
    if (file_path)
        json_object_set(result, "file_path", json_new_string(file_path));
    char *s = json_serialize(result); json_free(result);
    free(result_str);
    return s;
}

/* Delete a skill directory entirely */
static char *action_delete(json_node_t *args, const char *skills_dir) {
    const char *name = json_object_get_string(args, "name", NULL);

    if (!name || !*name) {
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string("Missing 'name'"));
        char *s = json_serialize(r); json_free(r); return s;
    }

    char skill_dir[MAX_PATH];
    skill_dir_path(skills_dir, name, skill_dir, sizeof(skill_dir));

    /* Check if skill directory exists */
    struct stat st;
    if (stat(skill_dir, &st) != 0) {
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string("Skill not found"));
        char *s = json_serialize(r); json_free(r); return s;
    }

    /* Build rm -rf command */
    char cmd[MAX_PATH];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", skill_dir);
    int ret = system(cmd);

    json_node_t *result = json_new_object();
    json_object_set(result, "ok", json_new_bool(ret == 0));
    json_object_set(result, "action", json_new_string("delete"));
    json_object_set(result, "name", json_new_string(name));
    if (ret != 0)
        json_object_set(result, "error", json_new_string("Failed to remove skill directory"));
    char *s = json_serialize(result); json_free(result); return s;
}

/* Write a supporting file (references/templates/scripts/assets) */
static char *action_write_file(json_node_t *args, const char *skills_dir) {
    const char *name = json_object_get_string(args, "name", NULL);
    const char *file_path = json_object_get_string(args, "file_path", NULL);
    const char *file_content = json_object_get_string(args, "file_content", NULL);

    if (!name || !*name) {
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string("Missing 'name'"));
        char *s = json_serialize(r); json_free(r); return s;
    }
    if (!file_path || !*file_path) {
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string("Missing 'file_path'"));
        char *s = json_serialize(r); json_free(r); return s;
    }
    if (!file_content) {
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string("Missing 'file_content'"));
        char *s = json_serialize(r); json_free(r); return s;
    }

    if (!is_allowed_subdir(file_path)) {
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string(
            "file_path must be under references/, templates/, scripts/, or assets/"));
        char *s = json_serialize(r); json_free(r); return s;
    }

    size_t content_len = strlen(file_content);
    if (content_len > (size_t)MAX_FILE_BYTES) {
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string("File content exceeds 1 MiB"));
        char *s = json_serialize(r); json_free(r); return s;
    }

    char skill_dir[MAX_PATH];
    skill_dir_path(skills_dir, name, skill_dir, sizeof(skill_dir));

    char target[MAX_PATH];
    snprintf(target, sizeof(target), "%s/%s", skill_dir, file_path);

    /* Ensure parent directory exists */
    char *last_slash = strrchr(target, '/');
    if (last_slash) {
        *last_slash = '\0';
        mkdir_p(target);
        *last_slash = '/';
    }

    if (!write_skill_file(target, file_content)) {
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string("Failed to write file"));
        char *s = json_serialize(r); json_free(r); return s;
    }

    json_node_t *result = json_new_object();
    json_object_set(result, "ok", json_new_bool(true));
    json_object_set(result, "action", json_new_string("write_file"));
    json_object_set(result, "name", json_new_string(name));
    json_object_set(result, "file_path", json_new_string(file_path));
    json_object_set(result, "bytes", json_new_number((double)content_len));
    char *s = json_serialize(result); json_free(result); return s;
}

/* Remove a supporting file */
static char *action_remove_file(json_node_t *args, const char *skills_dir) {
    const char *name = json_object_get_string(args, "name", NULL);
    const char *file_path = json_object_get_string(args, "file_path", NULL);

    if (!name || !*name) {
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string("Missing 'name'"));
        char *s = json_serialize(r); json_free(r); return s;
    }
    if (!file_path || !*file_path) {
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string("Missing 'file_path'"));
        char *s = json_serialize(r); json_free(r); return s;
    }

    if (!is_allowed_subdir(file_path)) {
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string(
            "file_path must be under references/, templates/, scripts/, or assets/"));
        char *s = json_serialize(r); json_free(r); return s;
    }

    char skill_dir[MAX_PATH];
    skill_dir_path(skills_dir, name, skill_dir, sizeof(skill_dir));

    char target[MAX_PATH];
    snprintf(target, sizeof(target), "%s/%s", skill_dir, file_path);

    if (unlink(target) != 0) {
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string("File not found or could not be removed"));
        char *s = json_serialize(r); json_free(r); return s;
    }

    json_node_t *result = json_new_object();
    json_object_set(result, "ok", json_new_bool(true));
    json_object_set(result, "action", json_new_string("remove_file"));
    json_object_set(result, "name", json_new_string(name));
    json_object_set(result, "file_path", json_new_string(file_path));
    char *s = json_serialize(result); json_free(result); return s;
}

/* Action: deps — check skill dependencies.
 * Reads SKILL.md YAML frontmatter, finds depends_on, resolves each. */
static char *action_deps(json_node_t *args, const char *skills_dir)
{
    const char *name = json_object_get_string(args, "name", NULL);
    if (!name) {
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string("name required"));
        char *s = json_serialize(r); json_free(r); return s;
    }

    char skill_path[MAX_PATH];
    snprintf(skill_path, sizeof(skill_path), "%s/%s/SKILL.md", skills_dir, name);
    char *content = read_skill_file(skill_path);
    if (!content) {
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string("Skill not found or cannot read SKILL.md"));
        char *s = json_serialize(r); json_free(r); return s;
    }

    /* Parse YAML frontmatter between --- markers */
    if (strncmp(content, "---", 3) != 0) {
        free(content);
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string("No YAML frontmatter"));
        json_object_set(r, "skill", json_new_string(name));
        char *s = json_serialize(r); json_free(r); return s;
    }

    const char *fm_start = content + 3;
    const char *nl = strchr(fm_start, '\n');
    if (!nl) { free(content); return strdup("{\"error\":\"Malformed frontmatter\"}"); }
    fm_start = nl + 1;

    const char *fm_end = strstr(fm_start, "\n---");
    if (!fm_end) { free(content); return strdup("{\"error\":\"No closing --- in frontmatter\"}"); }

    size_t fm_len = (size_t)(fm_end - fm_start);
    char *fm = malloc(fm_len + 1);
    if (!fm) { free(content); return strdup("{\"error\":\"OOM\"}"); }
    memcpy(fm, fm_start, fm_len);
    fm[fm_len] = '\0';

    json_node_t *result = json_new_object();
    json_object_set(result, "skill", json_new_string(name));
    json_node_t *deps = json_new_array();

    char *line_save = NULL;
    char *line = strtok_r(fm, "\n", &line_save);
    bool in_deps = false;

    while (line) {
        while (*line == ' ') line++;
        if (strncmp(line, "depends_on:", 11) == 0) {
            in_deps = true;
            const char *rest = line + 11;
            while (*rest == ' ') rest++;
            if (*rest == '-') {
                while (*rest == ' ') rest++;
                if (*rest == '-') { rest++; while (*rest == ' ') rest++; }
                if (*rest) {
                    size_t dlen = strlen(rest);
                    while (dlen > 0 && (rest[dlen-1] == ' ' || rest[dlen-1] == '\r')) dlen--;
                    char *dn = strndup(rest, dlen);
                    if (dn) { json_append(deps, json_new_string(dn)); free(dn); }
                }
            }
        } else if (in_deps && line[0] == '-') {
            const char *rest = line + 1;
            while (*rest == ' ') rest++;
            if (*rest) {
                size_t dlen = strlen(rest);
                while (dlen > 0 && (rest[dlen-1] == ' ' || rest[dlen-1] == '\r')) dlen--;
                char *dn = strndup(rest, dlen);
                if (dn) { json_append(deps, json_new_string(dn)); free(dn); }
            }
        } else if (in_deps && line[0] != ' ' && line[0] != '\t') {
            in_deps = false;
        }
        line = strtok_r(NULL, "\n", &line_save);
    }

    json_object_set(result, "dependencies", deps);

    json_node_t *statuses = json_new_object();
    int found = 0, missing = 0;
    for (size_t i = 0; i < json_len(deps); i++) {
        json_node_t *dep = json_get(deps, (int)i);
        const char *dn = NULL;
        if (dep && dep->type == JSON_STRING) dn = dep->str_val;
        if (!dn || !*dn) continue;
        struct stat st;
        char dp[MAX_PATH];
        snprintf(dp, sizeof(dp), "%s/%s/SKILL.md", skills_dir, dn);
        if (stat(dp, &st) == 0) { json_object_set(statuses, dn, json_new_string("found")); found++; }
        else { json_object_set(statuses, dn, json_new_string("missing")); missing++; }
    }
    json_object_set(result, "status", statuses);
    json_object_set(result, "found", json_new_number((double)found));
    json_object_set(result, "missing", json_new_number((double)missing));

    char *json_out = json_serialize(result);
    json_free(result);
    free(fm);
    free(content);
    return json_out;
}

/* Main skill_manage handler — routes to action */
char *skill_manage_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    /* NULL args → default to listing all skills */
    if (!args_json) args_json = "{}";

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *action = json_object_get_string(args, "action", NULL);
    const char *skills_dir = get_skills_dir();

    char *result = NULL;

    if (!action) {
        /* Default: list skills (backward compat) */
        DIR *dir = opendir(skills_dir);
        json_node_t *res = json_new_object();
        json_object_set(res, "skills_dir", json_new_string(skills_dir));
        json_node_t *skills = json_new_array();

        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_type == DT_DIR || strstr(entry->d_name, ".md")) {
                    char spath[MAX_PATH];
                    snprintf(spath, sizeof(spath), "%s/%s/SKILL.md", skills_dir, entry->d_name);
                    struct stat st;
                    if (stat(spath, &st) == 0) {
                        json_array_append(skills, json_new_string(entry->d_name));
                    }
                }
            }
            closedir(dir);
        }

        json_object_set(res, "skills", skills);
        json_object_set(res, "count", json_new_number((double)json_array_count(skills)));
        result = json_serialize(res);
        json_free(res);
        json_free(args);
        return result;
    }

    if (strcmp(action, "create") == 0)
        result = action_create(args, skills_dir);
    else if (strcmp(action, "edit") == 0)
        result = action_edit(args, skills_dir);
    else if (strcmp(action, "patch") == 0)
        result = action_patch(args, skills_dir);
    else if (strcmp(action, "delete") == 0)
        result = action_delete(args, skills_dir);
    else if (strcmp(action, "write_file") == 0)
        result = action_write_file(args, skills_dir);
    else if (strcmp(action, "remove_file") == 0)
        result = action_remove_file(args, skills_dir);
    else if (strcmp(action, "deps") == 0)
        result = action_deps(args, skills_dir);
    else if (strcmp(action, "diff") == 0) {
        const char *name = json_object_get_string(args, "name", NULL);
        if (!name || !*name) {
            json_node_t *r = json_new_object();
            json_object_set(r, "error", json_new_string("Missing 'name'"));
            char *s = json_serialize(r); json_free(r); return s;
        }
        char sk_path[MAX_PATH];
        skill_sk_path(skills_dir, name, sk_path, sizeof(sk_path));
        char bak_path[MAX_PATH];
        snprintf(bak_path, sizeof(bak_path), "%s.bak", sk_path);

        char *current = read_skill_file(sk_path);
        char *backup = read_skill_file(bak_path);

        json_node_t *r = json_new_object();
        json_object_set(r, "name", json_new_string(name));

        if (!backup) {
            json_object_set(r, "diff", json_new_string("No backup available (first edit since backup system)"));
        } else if (!current) {
            json_object_set(r, "diff", json_new_string("Current SKILL.md not found"));
        } else if (strcmp(current, backup) == 0) {
            json_object_set(r, "diff", json_new_string("No changes since last backup (identical)"));
        } else {
            char *diff_str = difflib_unified_diff(backup, current, 3);
            if (diff_str) {
                json_object_set(r, "diff", json_new_string(diff_str));
                free(diff_str);
            } else {
                json_object_set(r, "diff", json_new_string("Diff generation failed"));
            }
        }

        free(current);
        free(backup);
        result = json_serialize(r);
        json_free(r);
    } else {
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string(
            "Unknown action. Valid: create, edit, patch, delete, write_file, remove_file, deps, diff"));
        result = json_serialize(r);
        json_free(r);
    }

    json_free(args);
    return result;
}

/* ================================================================
 *  Auto-registration
 * ================================================================ */

void registry_init_skill_view(void) {
    registry_register("skill_view",
        "Load and view a Hermes skill. Returns the SKILL.md content and optionally linked files.",
        "{"
        "\"type\":\"object\","
        "\"properties\":{"
          "\"name\":{\"type\":\"string\",\"description\":\"Skill name\"},"
          "\"file_path\":{\"type\":\"string\",\"description\":\"Optional linked file path within skill\"}"
        "},"
        "\"required\":[\"name\"]"
        "}", skill_view_handler);

    registry_register("skill_manage",
        "Manage skills: create, edit (full rewrite), patch (find-and-replace), "
        "delete, write_file (supporting file), remove_file. "
        "Omit 'action' to list skills.",
        "{"
        "\"type\":\"object\","
        "\"properties\":{"
          "\"action\":{\"type\":\"string\",\"enum\":[\"create\",\"edit\",\"patch\",\"delete\",\"write_file\",\"remove_file\"],"
            "\"description\":\"Omit to list skills\"},"
          "\"name\":{\"type\":\"string\",\"description\":\"Skill name\"},"
          "\"content\":{\"type\":\"string\",\"description\":\"SKILL.md content (for create/edit)\"},"
          "\"category\":{\"type\":\"string\",\"description\":\"Category directory (for create)\"},"
          "\"old_string\":{\"type\":\"string\",\"description\":\"Text to find (for patch)\"},"
          "\"new_string\":{\"type\":\"string\",\"description\":\"Replacement text (for patch)\"},"
          "\"replace_all\":{\"type\":\"boolean\",\"description\":\"Replace all occurrences (for patch)\"},"
          "\"file_path\":{\"type\":\"string\",\"description\":\"Supporting file path (for write_file/remove_file/patch)\"},"
          "\"file_content\":{\"type\":\"string\",\"description\":\"File content (for write_file)\"}"
        "},"
        "\"required\":[]"
        "}", skill_manage_handler);
}
