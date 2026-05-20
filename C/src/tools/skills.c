/*
 * skills.c — Skill management tool for Hermes C.
 * Lists, loads skills from ~/.hermes/skills/.
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

/* Schema */
static const char *SCHEMA_LIST = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"category\":{\"type\":\"string\",\"description\":\"Optional category filter\"}"
    "},"
    "\"required\":[]"
"}";

/* ================================================================
 *  Skills directory scanner
 * ================================================================ */

static char *get_skills_dir(void) {
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) return strdup("/tmp/.hermes/skills");
    static char buf[4096];
    snprintf(buf, sizeof(buf), "%s/.hermes/skills", home);

    /* Check if exists */
    struct stat st;
    if (stat(buf, &st) == 0 && S_ISDIR(st.st_mode))
        return buf;

    /* Try a different path */
    snprintf(buf, sizeof(buf), "%s/skills", home);
    if (stat(buf, &st) == 0 && S_ISDIR(st.st_mode))
        return buf;

    return buf;
}

/* ================================================================
 *  Handler
 * ================================================================ */

char *skills_list_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    (void)args_json;

    const char *skills_dir = get_skills_dir();
    DIR *dir = opendir(skills_dir);
    if (!dir) {
        json_node_t *r = json_new_object();
        json_object_set(r, "error", json_new_string("No skills directory found"));
        json_object_set(r, "skills_dir", json_new_string(skills_dir));
        json_object_set(r, "skills", json_new_array());
        char *out = json_serialize(r);
        json_free(r);
        return out;
    }

    json_node_t *result = json_new_object();
    json_object_set(result, "skills_dir", json_new_string(skills_dir));
    json_node_t *skills = json_new_array();

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR || strstr(entry->d_name, ".md")) {
            /* Check for SKILL.md */
            char spath[4096];
            snprintf(spath, sizeof(spath), "%s/%s/SKILL.md", skills_dir, entry->d_name);
            struct stat st;
            if (stat(spath, &st) == 0) {
                json_array_append(skills, json_new_string(entry->d_name));
            }
        }
    }
    closedir(dir);

    json_object_set(result, "skills", skills);
    char *out = json_serialize(result);
    json_free(result);
    return out;
}

/* Auto-registration */
void registry_init_skills(void) {
    registry_register("skills_list",
        "List available Hermes skills. Returns array of skill names from the skills directory.",
        SCHEMA_LIST, skills_list_handler);
}
