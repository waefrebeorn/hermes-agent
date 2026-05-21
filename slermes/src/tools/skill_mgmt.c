/*
 * skill_mgmt.c — Skill management tools for Slermes C.
 * skill_view: load and display a skill's content.
 * skill_manage: list skills.
 */

#include "slermes.h"
#include "slermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

/* ================================================================
 *  Helpers
 * ================================================================ */

static char *get_skills_dir(void) {
    static char buf[4096];
    const char *home = getenv("SLERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(buf, sizeof(buf), "%s/skills", home);

    struct stat st;
    if (stat(buf, &st) == 0 && S_ISDIR(st.st_mode))
        return buf;

    snprintf(buf, sizeof(buf), "%s/.slermes/skills", home);
    return buf;
}

/* Read file content (basic fread wrapper) */
static char *read_skill_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz > 100 * 1024) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';
    return buf;
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
        char skill_dir[4096];
        snprintf(skill_dir, sizeof(skill_dir), "%s/%s", get_skills_dir(), name);

        char sk_path[4096];
        snprintf(sk_path, sizeof(sk_path), "%s/SKILL.md", skill_dir);

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

        if (file_path) {
            char fp[4096];
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
 *  skill_manage handler (list only for now)
 * ================================================================ */

char *skill_manage_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    (void)args_json;

    const char *skills_dir = get_skills_dir();
    DIR *dir = opendir(skills_dir);
    json_node_t *result = json_new_object();
    json_object_set(result, "skills_dir", json_new_string(skills_dir));
    json_node_t *skills = json_new_array();

    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type == DT_DIR || strstr(entry->d_name, ".md")) {
                char spath[4096];
                snprintf(spath, sizeof(spath), "%s/%s/SKILL.md", skills_dir, entry->d_name);
                struct stat st;
                if (stat(spath, &st) == 0) {
                    json_array_append(skills, json_new_string(entry->d_name));
                }
            }
        }
        closedir(dir);
    }

    json_object_set(result, "skills", skills);
    json_object_set(result, "count", json_new_number((double)json_array_count(skills)));

    char *json_out = json_serialize(result);
    json_free(result);
    return json_out;
}

/* Auto-registration */
void registry_init_skill_view(void) {
    registry_register("skill_view",
        "Load and view a Slermes skill. Returns the SKILL.md content and optionally linked files.",
        "{"
        "\"type\":\"object\","
        "\"properties\":{"
          "\"name\":{\"type\":\"string\",\"description\":\"Skill name\"},"
          "\"file_path\":{\"type\":\"string\",\"description\":\"Optional linked file path within skill\"}"
        "},"
        "\"required\":[\"name\"]"
        "}", skill_view_handler);

    registry_register("skill_manage",
        "List available skills. Returns skill names from the skills directory.",
        "{"
        "\"type\":\"object\","
        "\"properties\":{},"
        "\"required\":[]"
        "}", skill_manage_handler);
}
