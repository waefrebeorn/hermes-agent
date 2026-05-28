/*
 * plugin_skills.c — Skills management plugin (PLUGIN_SKILL).
 *
 * Scans ~/.hermes/skills/ for skill directories containing SKILL.md files,
 * parses YAML frontmatter, and provides tools to list/describe/search skills.
 *
 * Build:
 *   gcc -O2 -fPIC -shared -I ../../include -I ../../lib/libplugin \
 *       plugin_skills.c -o plugin_skills.so
 */

#include "plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

/* ================================================================
 *  Constants
 * ================================================================ */

#define MAX_SKILLS           256
#define SKILL_NAME_MAX       128
#define SKILL_DESC_MAX       512
#define SKILL_PATH_MAX       4096
#define LINE_MAX             4096

/* ================================================================
 *  Skill entry
 * ================================================================ */

typedef struct {
    char name[SKILL_NAME_MAX];
    char description[SKILL_DESC_MAX];
    char path[SKILL_PATH_MAX];
    char category[SKILL_NAME_MAX];
    int has_references;
    int has_templates;
    int has_scripts;
} skill_entry_t;

static skill_entry_t g_skills[MAX_SKILLS];
static int g_skill_count = 0;
static char g_skills_dir[SKILL_PATH_MAX];
static time_t g_last_scan = 0;

/* ================================================================
 *  Helpers
 * ================================================================ */

/* Trim trailing whitespace */
static void trim_right(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' || s[len-1] == '\n' || s[len-1] == '\r'))
        s[--len] = '\0';
}

/* Extract YAML frontmatter value for a key: looks for "key: value" pattern */
static int extract_frontmatter(const char *line, const char *key, char *out, size_t out_sz) {
    size_t klen = strlen(key);
    /* Skip whitespace */
    while (*line == ' ' || *line == '\t') line++;
    if (strncmp(line, key, klen) != 0) return 0;
    line += klen;
    /* Skip colon + whitespace */
    while (*line == ':' || *line == ' ' || *line == '\t') line++;
    /* Handle multi-line > marker */
    if (*line == '>') {
        /* For multi-line description, just take a shortened version */
        snprintf(out, out_sz, "(multi-line)");
        return 1;
    }
    snprintf(out, out_sz, "%s", line);
    trim_right(out);
    return 1;
}

/* Read a SKILL.md file and extract frontmatter */
static int read_skill_file(const char *dirpath, skill_entry_t *skill) {
    char md_path[SKILL_PATH_MAX];
    snprintf(md_path, sizeof(md_path), "%s/SKILL.md", dirpath);

    FILE *f = fopen(md_path, "r");
    if (!f) return 0;

    /* Extract directory name as skill name */
    const char *last_slash = strrchr(dirpath, '/');
    const char *dirname = last_slash ? last_slash + 1 : dirpath;
    snprintf(skill->name, sizeof(skill->name), "%s", dirname);
    snprintf(skill->path, sizeof(skill->path), "%s", dirpath);
    skill->description[0] = '\0';
    skill->category[0] = '\0';

    /* Parse YAML frontmatter (between --- delimiters) */
    char line[LINE_MAX];
    int in_frontmatter = 0;
    int frontmatter_ended = 0;
    int found_name = 0, found_desc = 0;

    while (fgets(line, sizeof(line), f) && !frontmatter_ended) {
        trim_right(line);

        if (strcmp(line, "---") == 0) {
            if (!in_frontmatter) {
                in_frontmatter = 1;
                continue;
            } else {
                frontmatter_ended = 1;
                continue;
            }
        }

        if (in_frontmatter) {
            if (!found_name && extract_frontmatter(line, "name", skill->name, sizeof(skill->name)))
                found_name = 1;
            if (!found_desc && extract_frontmatter(line, "description", skill->description, sizeof(skill->description)))
                found_desc = 1;
            if (strlen(skill->category) == 0)
                extract_frontmatter(line, "category", skill->category, sizeof(skill->category));
        }
    }
    fclose(f);

    /* Check for subdirectories */
    char ref_path[SKILL_PATH_MAX];
    snprintf(ref_path, sizeof(ref_path), "%s/references", dirpath);
    struct stat st;
    skill->has_references = (stat(ref_path, &st) == 0 && S_ISDIR(st.st_mode)) ? 1 : 0;

    snprintf(ref_path, sizeof(ref_path), "%s/templates", dirpath);
    skill->has_templates = (stat(ref_path, &st) == 0 && S_ISDIR(st.st_mode)) ? 1 : 0;

    snprintf(ref_path, sizeof(ref_path), "%s/scripts", dirpath);
    skill->has_scripts = (stat(ref_path, &st) == 0 && S_ISDIR(st.st_mode)) ? 1 : 0;

    return 1;
}

/* Scan skills directory */
static int scan_skills(void) {
    g_skill_count = 0;
    g_last_scan = time(NULL);

    struct stat st;
    if (stat(g_skills_dir, &st) != 0 || !S_ISDIR(st.st_mode))
        return 0;

    DIR *d = opendir(g_skills_dir);
    if (!d) return 0;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL && g_skill_count < MAX_SKILLS) {
        if (entry->d_name[0] == '.') continue;

        /* Check if this entry is a directory */
        char full_path[SKILL_PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", g_skills_dir, entry->d_name);
        if (stat(full_path, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        /* Check for SKILL.md */
        if (read_skill_file(full_path, &g_skills[g_skill_count])) {
            g_skill_count++;
        }
    }
    closedir(d);
    return g_skill_count;
}

/* Find skill by name */
static skill_entry_t *find_skill(const char *name) {
    for (int i = 0; i < g_skill_count; i++) {
        if (strcmp(g_skills[i].name, name) == 0)
            return &g_skills[i];
    }
    return NULL;
}

/* ================================================================
 *  Tool interface functions
 * ================================================================ */

/* Tool 0: list_skills — list all available skills */
static char *tool_list_skills(const char *args_json) {
    (void)args_json;
    scan_skills();

    char buf[16384];
    int pos = snprintf(buf, sizeof(buf),
        "{\"count\":%d,\"skills_dir\":\"%s\",\"skills\":[",
        g_skill_count, g_skills_dir);

    for (int i = 0; i < g_skill_count; i++) {
        if (i > 0) {
            int r = snprintf(buf + pos, sizeof(buf) - (size_t)pos, ",");
            if (r < 0 || (size_t)r >= sizeof(buf) - (size_t)pos) break;
            pos += r;
        }
        int r = snprintf(buf + pos, sizeof(buf) - (size_t)pos,
            "{\"name\":\"%s\",\"description\":\"%s\",\"category\":\"%s\","
            "\"has_references\":%d,\"has_templates\":%d,\"has_scripts\":%d}",
            g_skills[i].name, g_skills[i].description, g_skills[i].category,
            g_skills[i].has_references, g_skills[i].has_templates, g_skills[i].has_scripts);
        if (r < 0 || (size_t)r >= sizeof(buf) - (size_t)pos) break;
        pos += r;
    }
    snprintf(buf + pos, sizeof(buf) - (size_t)pos, "]}");
    return strdup(buf);
}

/* Tool 1: get_skill — get details of a specific skill */
static char *tool_get_skill(const char *args_json) {
    if (g_skill_count == 0) scan_skills();
    if (g_skill_count == 0)
        return strdup("{\"error\":\"no skills found\"}");

    /* Parse skill name from args */
    const char *name = NULL;
    if (args_json) {
        const char *n = strstr(args_json, "\"name\"");
        if (n) {
            n += 6; while (*n && *n != ':') n++; if (*n) n++;
            while (*n && *n != '"') n++; if (*n) n++;
            const char *end = strchr(n, '"');
            if (end) {
                static char name_buf[256];
                size_t len = (size_t)(end - n);
                if (len > 0 && len < sizeof(name_buf)) {
                    memcpy(name_buf, n, len);
                    name_buf[len] = '\0';
                    name = name_buf;
                }
            }
        }
    }

    if (!name) {
        /* Return first skill */
        name = g_skills[0].name;
    }

    skill_entry_t *skill = find_skill(name);
    if (!skill) {
        char err[512];
        snprintf(err, sizeof(err), "{\"error\":\"skill '%s' not found\"}", name);
        return strdup(err);
    }

    /* Read SKILL.md content (first 2KB) */
    char md_path[SKILL_PATH_MAX];
    snprintf(md_path, sizeof(md_path), "%s/SKILL.md", skill->path);
    FILE *f = fopen(md_path, "r");
    char content[2048] = {0};
    if (f) {
        size_t n = fread(content, 1, sizeof(content) - 1, f);
        content[n] = '\0';
        fclose(f);
    }

    char buf[8192];
    snprintf(buf, sizeof(buf),
        "{\"name\":\"%s\",\"description\":\"%s\",\"category\":\"%s\","
        "\"path\":\"%s\",\"has_references\":%d,\"has_templates\":%d,\"has_scripts\":%d,"
        "\"content\":\"%s\"}",
        skill->name, skill->description, skill->category,
        skill->path, skill->has_references, skill->has_templates, skill->has_scripts,
        content);
    return strdup(buf);
}

/* Tool 2: search_skills — search skills by keyword */
static char *tool_search_skills(const char *args_json) {
    if (g_skill_count == 0) scan_skills();

    /* Parse query from args */
    const char *query = NULL;
    char query_buf[256] = {0};
    if (args_json) {
        const char *q = strstr(args_json, "\"query\"");
        if (q) {
            q += 6; while (*q && *q != ':') q++; if (*q) q++;
            while (*q && *q != '"') q++; if (*q) q++;
            const char *end = strchr(q, '"');
            if (end) {
                size_t len = (size_t)(end - q);
                if (len > 0 && len < sizeof(query_buf)) {
                    memcpy(query_buf, q, len);
                    query = query_buf;
                }
            }
        }
    }

    char buf[16384];
    int pos = snprintf(buf, sizeof(buf), "{\"query\":\"%s\",\"count\":", query ? query : "");
    int matches = 0;
    pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "0,\"matches\":[");

    for (int i = 0; i < g_skill_count; i++) {
        int match = 0;
        if (!query || query[0] == '\0') {
            match = 1;  /* No query = return all */
        } else {
            if (strstr(g_skills[i].name, query) ||
                strstr(g_skills[i].description, query) ||
                strstr(g_skills[i].category, query))
                match = 1;
        }
        if (!match) continue;

        if (matches > 0) {
            int r = snprintf(buf + pos, sizeof(buf) - (size_t)pos, ",");
            if (r < 0 || (size_t)r >= sizeof(buf) - (size_t)pos) break;
            pos += r;
        }
        int r = snprintf(buf + pos, sizeof(buf) - (size_t)pos,
            "{\"name\":\"%s\",\"description\":\"%s\",\"category\":\"%s\"}",
            g_skills[i].name, g_skills[i].description, g_skills[i].category);
        if (r < 0 || (size_t)r >= sizeof(buf) - (size_t)pos) break;
        pos += r;
        matches++;
    }

    /* Fix count */
    char count_str[16];
    snprintf(count_str, sizeof(count_str), "%d", matches);
    /* Replace "count\":0" with actual count */
    char *count_pos = strstr(buf, "\"count\":0");
    if (count_pos) {
        memcpy(count_pos + 7, count_str, strlen(count_str));
    }

    snprintf(buf + pos, sizeof(buf) - (size_t)pos, "]}");
    return strdup(buf);
}

/* ================================================================
 *  Plugin interface table
 * ================================================================ */

static plugin_interface_t g_interface;

void *plugin_get_interface(void) {
    return &g_interface;
}

/* ================================================================
 *  Plugin metadata
 * ================================================================ */

const char *plugin_meta_name(void) {
    return "skills";
}

const char *plugin_meta_version(void) {
    return "1.0.0";
}

const char *plugin_meta_type(void) {
    return "skills";
}

const char *plugin_meta_description(void) {
    return "Skills management — scan, list, search, and inspect Hermes skill files";
}

/* ================================================================
 *  Dependencies
 * ================================================================ */

int plugin_deps_count(void) {
    return 0;
}

const plugin_dep_t *plugin_deps_list(void) {
    return NULL;
}

/* ================================================================
 *  Lifecycle
 * ================================================================ */

int plugin_init(void) {
    memset(&g_interface, 0, sizeof(g_interface));
    g_interface.type = PLUGIN_SKILL;

    /* Determine skills directory */
    const char *home = getenv("SLERMES_HOME");
    if (!home) home = getenv("HOME");
    if (home) {
        snprintf(g_skills_dir, sizeof(g_skills_dir),
                 "%s/.hermes/skills", home);
    } else {
        snprintf(g_skills_dir, sizeof(g_skills_dir),
                 "/tmp/.hermes/skills");
    }

    /* Initial scan */
    scan_skills();
    return 0;
}

int plugin_cleanup(void) {
    memset(&g_interface, 0, sizeof(g_interface));
    return 0;
}
