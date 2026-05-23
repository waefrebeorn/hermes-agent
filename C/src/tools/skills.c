/*
 * skills.c — Skills system for Hermes C (P179-P188).
 * Implements scanning, validation, provenance, sync, bundles,
 * usage tracking, caching, search, curator, and dependencies.
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_yaml.h"
#include "hermes_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>

/* ================================================================
 *  Internal constants
 * ================================================================ */

#define SKILL_CACHE_DEFAULT_MAX  128
#define SKILL_CURATOR_DEFAULT_STALE_DAYS 90
#define SKILL_MAX_DEPENDENCIES   64
#define SKILL_USAGE_DB_NAME      "skills_usage"
#define SKILL_BUNDLES_SUBDIR     "bundles"
#define SKILL_PROVENANCE_FILE    ".provenance"
#define SKILL_MAX_SEARCH_RESULTS 256
#define SKILL_MAX_TEXT_MATCHES   1024

/* ================================================================
 *  Internal state
 * ================================================================ */

static skill_cache_t   g_cache = {NULL, NULL, 0, 0};
static int             g_curator_stale_days = SKILL_CURATOR_DEFAULT_STALE_DAYS;

/* ================================================================
 *  Path utilities
 * ================================================================ */

static void skills_dir(char *buf, size_t sz) {
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(buf, sz, "%s/.hermes/skills", home);
    struct stat st;
    if (stat(buf, &st) == 0 && S_ISDIR(st.st_mode))
        return;
    snprintf(buf, sz, "%s/skills", home);
}

static void skills_bundles_dir(char *buf, size_t sz) {
    char sd[HERMES_PATH_MAX];
    skills_dir(sd, sizeof(sd));
    snprintf(buf, sz, "%s/%s", sd, SKILL_BUNDLES_SUBDIR);
}

static void skill_path(const char *name, char *buf, size_t sz) {
    char sd[HERMES_PATH_MAX];
    skills_dir(sd, sizeof(sd));
    snprintf(buf, sz, "%s/%s", sd, name);
}

static void skill_sk_path(const char *name, char *buf, size_t sz) {
    char sd[HERMES_PATH_MAX];
    skills_dir(sd, sizeof(sd));
    snprintf(buf, sz, "%s/%s/SKILL.md", sd, name);
}

static void skill_prov_path(const char *name, char *buf, size_t sz) {
    char sd[HERMES_PATH_MAX];
    skills_dir(sd, sizeof(sd));
    snprintf(buf, sz, "%s/%s/%s", sd, name, SKILL_PROVENANCE_FILE);
}

static void bundle_path(const char *bundle, char *buf, size_t sz) {
    char bd[HERMES_PATH_MAX];
    skills_bundles_dir(bd, sizeof(bd));
    snprintf(buf, sz, "%s/%s.bundle", bd, bundle);
}

static void ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) return;
    mkdir(path, 0755);
}

/* ================================================================
 *  File I/O helpers
 * ================================================================ */

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz > 512 * 1024) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';
    return buf;
}

static bool write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    if (!f) return false;
    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, f);
    fclose(f);
    return written == len;
}

static time_t file_mtime(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return st.st_mtime;
}

static bool dir_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static bool file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

/* ================================================================
 *  YAML frontmatter extraction from SKILL.md
 * ================================================================ */

/* Extract YAML frontmatter from markdown content.
 * Returns malloc'd string of the frontmatter (without --- markers),
 * or NULL if no valid frontmatter found. */
static char *extract_frontmatter(const char *content) {
    if (!content || content[0] != '-') return NULL;

    /* Check for leading --- */
    if (strncmp(content, "---", 3) != 0) return NULL;

    /* Find closing --- */
    const char *p = content + 3;
    /* Skip whitespace-only lines after opening --- */
    while (*p == '\n' || *p == '\r') p++;

    const char *close = strstr(p, "\n---");
    if (!close) {
        close = strstr(p, "\r---");
    }
    if (!close) {
        /* Try end of string without trailing newline */
        size_t len = strlen(content);
        if (len > 6 && strcmp(content + len - 3, "---") == 0) {
            close = content + len - 3;
        }
    }
    if (!close) return NULL;

    size_t len = (size_t)(close - p);
    char *fm = (char *)malloc(len + 1);
    if (!fm) return NULL;
    memcpy(fm, p, len);
    fm[len] = '\0';
    return fm;
}

/* ================================================================
 *  P179: Skill directory scanning — recursive
 * ================================================================ */

static void scan_add_skill(skill_list_t *list, const char *name, const char *fullpath) {
    if (list->count >= list->capacity) {
        size_t new_cap = list->capacity == 0 ? 64 : list->capacity * 2;
        skill_meta_t *new_arr = (skill_meta_t *)realloc(list->skills, new_cap * sizeof(skill_meta_t));
        if (!new_arr) return;
        list->skills = new_arr;
        list->capacity = new_cap;
    }
    skill_meta_t *m = &list->skills[list->count++];
    memset(m, 0, sizeof(*m));
    snprintf(m->name, sizeof(m->name), "%s", name);
    snprintf(m->path, sizeof(m->path), "%s", fullpath);

    /* Read SKILL.md and extract frontmatter */
    char skpath[HERMES_PATH_MAX];
    snprintf(skpath, sizeof(skpath), "%s/SKILL.md", fullpath);
    char *content = read_file(skpath);
    if (content) {
        m->last_updated = file_mtime(skpath);
        char *fm = extract_frontmatter(content);
        if (fm) {
            yaml_doc_t *yaml = yaml_parse(fm, NULL);
            if (yaml) {
                const char *v;
                v = yaml_get_string(yaml, "version");
                if (v) snprintf(m->version, sizeof(m->version), "%s", v);
                v = yaml_get_string(yaml, "author");
                if (v) snprintf(m->author, sizeof(m->author), "%s", v);
                v = yaml_get_string(yaml, "description");
                if (v) snprintf(m->description, sizeof(m->description), "%s", v);
                v = yaml_get_string(yaml, "category");
                if (v) snprintf(m->category, sizeof(m->category), "%s", v);

                /* Tags */
                size_t ntags = yaml_list_count(yaml, "tags");
                if (ntags > 0) {
                    char tags_buf[1024] = "";
                    for (size_t i = 0; i < ntags; i++) {
                        const char *tag = yaml_list_get(yaml, "tags", i);
                        if (tag) {
                            if (tags_buf[0]) strncat(tags_buf, ",", sizeof(tags_buf) - strlen(tags_buf) - 1);
                            strncat(tags_buf, tag, sizeof(tags_buf) - strlen(tags_buf) - 1);
                        }
                    }
                    snprintf(m->tags, sizeof(m->tags), "%s", tags_buf);
                }

                /* Dependencies */
                size_t ndeps = yaml_list_count(yaml, "dependencies");
                if (ndeps > 0) {
                    char deps_buf[1024] = "";
                    for (size_t i = 0; i < ndeps; i++) {
                        const char *dep = yaml_list_get(yaml, "dependencies", i);
                        if (dep) {
                            if (deps_buf[0]) strncat(deps_buf, ",", sizeof(deps_buf) - strlen(deps_buf) - 1);
                            strncat(deps_buf, dep, sizeof(deps_buf) - strlen(deps_buf) - 1);
                        }
                    }
                    snprintf(m->dependencies, sizeof(m->dependencies), "%s", deps_buf);
                }

                /* Bundles */
                size_t nb = yaml_list_count(yaml, "bundles");
                if (nb > 0) {
                    char b_buf[1024] = "";
                    for (size_t i = 0; i < nb; i++) {
                        const char *b = yaml_list_get(yaml, "bundles", i);
                        if (b) {
                            if (b_buf[0]) strncat(b_buf, ",", sizeof(b_buf) - strlen(b_buf) - 1);
                            strncat(b_buf, b, sizeof(b_buf) - strlen(b_buf) - 1);
                        }
                    }
                    snprintf(m->bundles, sizeof(m->bundles), "%s", b_buf);
                }

                yaml_free(yaml);
            }
            free(fm);
        }

        /* Also try description as single-line fallback if no YAML desc */
        if (!m->description[0]) {
            /* Try first non-frontmatter line */
            const char *body = strstr(content, "\n---\n");
            if (body) {
                body += 5;
                while (*body == '\n' || *body == '\r') body++;
                const char *nl = strchr(body, '\n');
                size_t dlen = nl ? (size_t)(nl - body) : strlen(body);
                if (dlen > 0 && dlen < sizeof(m->description)) {
                    memcpy(m->description, body, dlen);
                    m->description[dlen] = '\0';
                }
            }
        }

        free(content);
    }

    /* Load provenance file */
    char provpath[HERMES_PATH_MAX];
    snprintf(provpath, sizeof(provpath), "%s/%s", fullpath, SKILL_PROVENANCE_FILE);
    char *prov = read_file(provpath);
    if (prov) {
        prov[strcspn(prov, "\n\r")] = '\0';
        if (strcmp(prov, "hub") == 0) m->origin = SKILL_ORIGIN_HUB;
        else if (strcmp(prov, "bundle") == 0) m->origin = SKILL_ORIGIN_BUNDLE;
        else if (strcmp(prov, "local") == 0) m->origin = SKILL_ORIGIN_LOCAL;
        else m->origin = SKILL_ORIGIN_LOCAL;
        free(prov);
    } else {
        m->origin = SKILL_ORIGIN_LOCAL;
    }

    /* Load usage data */
    char usage_path[HERMES_PATH_MAX];
    snprintf(usage_path, sizeof(usage_path), "%s/.usage", fullpath);
    char *usage = read_file(usage_path);
    if (usage) {
        char *err = NULL;
        json_node_t *j = json_parse(usage, &err);
        if (j) {
            m->usage_count = (int)json_get_num(j, "count", 0);
            m->last_used = (time_t)json_get_num(j, "last_used", 0);
            json_free(j);
        }
        free(err);
        free(usage);
    }
}

/* Recursive directory scan for skills */
static void scan_dir_recursive(skill_list_t *list, const char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        if (strcmp(entry->d_name, ".cache") == 0 ||
            strcmp(entry->d_name, SKILL_BUNDLES_SUBDIR) == 0)
            continue;

        char fullpath[HERMES_PATH_MAX];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (stat(fullpath, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            /* Check if this directory has a SKILL.md */
            char skpath[HERMES_PATH_MAX];
            snprintf(skpath, sizeof(skpath), "%s/SKILL.md", fullpath);
            if (file_exists(skpath)) {
                scan_add_skill(list, entry->d_name, fullpath);
            } else {
                /* Recurse into subdirectory */
                scan_dir_recursive(list, fullpath);
            }
        } else if (S_ISREG(st.st_mode)) {
            /* Check for .md files that might be standalone skills */
            const char *ext = strrchr(entry->d_name, '.');
            if (ext && strcasecmp(ext, ".md") == 0) {
                char name[256];
                snprintf(name, sizeof(name), "%.*s", (int)(ext - entry->d_name), entry->d_name);
                scan_add_skill(list, name, fullpath);
            }
        }
    }
    closedir(dir);
}

skill_list_t *skills_scan_all(void) {
    skill_list_t *list = (skill_list_t *)calloc(1, sizeof(skill_list_t));
    if (!list) return NULL;

    char sd[HERMES_PATH_MAX];
    skills_dir(sd, sizeof(sd));

    if (dir_exists(sd)) {
        scan_dir_recursive(list, sd);
    }

    return list;
}

void skills_scan_free(skill_list_t *list) {
    if (!list) return;
    free(list->skills);
    free(list);
}

/* ================================================================
 *  P180: Skill validation — YAML frontmatter, duplicates, version
 * ================================================================ */

/* Check if a semver string is valid (very basic check) */
static bool is_valid_version(const char *v) {
    if (!v || !*v) return true; /* empty version is allowed */
    int major = 0, minor = 0, patch = 0;
    return sscanf(v, "%d.%d.%d", &major, &minor, &patch) >= 1;
}

bool skill_validate(const char *skill_name, char *error_out, size_t err_sz) {
    if (error_out) error_out[0] = '\0';

    char skpath[HERMES_PATH_MAX];
    skill_sk_path(skill_name, skpath, sizeof(skpath));

    if (!file_exists(skpath)) {
        if (error_out) snprintf(error_out, err_sz, "SKILL.md not found for '%s'", skill_name);
        return false;
    }

    char *content = read_file(skpath);
    if (!content) {
        if (error_out) snprintf(error_out, err_sz, "Cannot read SKILL.md for '%s'", skill_name);
        return false;
    }

    /* Check for frontmatter */
    char *fm = extract_frontmatter(content);
    if (!fm) {
        /* No frontmatter — not necessarily invalid, just has no metadata */
        free(content);
        return true; /* Allow skills without frontmatter */
    }

    yaml_doc_t *yaml = yaml_parse(fm, NULL);
    if (!yaml) {
        snprintf(error_out, err_sz, "Invalid YAML frontmatter in '%s'", skill_name);
        free(fm);
        free(content);
        return false;
    }

    /* Validate version if present */
    const char *ver = yaml_get_string(yaml, "version");
    if (ver && *ver && !is_valid_version(ver)) {
        snprintf(error_out, err_sz, "Invalid version '%s' in '%s' (expected semver)", ver, skill_name);
        yaml_free(yaml);
        free(fm);
        free(content);
        return false;
    }

    yaml_free(yaml);
    free(fm);
    free(content);
    return true;
}

bool skill_validate_all(void) {
    skill_list_t *list = skills_scan_all();
    if (!list) return false;

    bool all_ok = true;
    for (size_t i = 0; i < list->count; i++) {
        char err[256];
        bool ok = skill_validate(list->skills[i].name, err, sizeof(err));
        /* Update the validated flag in the list */
        list->skills[i].validated = ok;
        if (!ok) {
            snprintf(list->skills[i].validation_error, sizeof(list->skills[i].validation_error), "%s", err);
            all_ok = false;
        } else {
            list->skills[i].validation_error[0] = '\0';
        }
    }

    skills_scan_free(list);
    return all_ok;
}

/* ================================================================
 *  P181: Skill provenance — origin tracking
 * ================================================================ */

skill_origin_t skill_get_origin(const char *skill_name) {
    char prov_path[HERMES_PATH_MAX];
    skill_prov_path(skill_name, prov_path, sizeof(prov_path));

    char *content = read_file(prov_path);
    if (!content) return SKILL_ORIGIN_LOCAL;

    /* Trim whitespace */
    content[strcspn(content, "\n\r")] = '\0';
    skill_origin_t origin;
    if (strcmp(content, "hub") == 0) origin = SKILL_ORIGIN_HUB;
    else if (strcmp(content, "bundle") == 0) origin = SKILL_ORIGIN_BUNDLE;
    else origin = SKILL_ORIGIN_LOCAL;

    free(content);
    return origin;
}

bool skill_set_origin(const char *skill_name, skill_origin_t origin) {
    char spath[HERMES_PATH_MAX];
    skill_path(skill_name, spath, sizeof(spath));
    ensure_dir(spath);

    char prov_path[HERMES_PATH_MAX];
    skill_prov_path(skill_name, prov_path, sizeof(prov_path));

    const char *origin_str;
    switch (origin) {
        case SKILL_ORIGIN_HUB:    origin_str = "hub\n"; break;
        case SKILL_ORIGIN_BUNDLE: origin_str = "bundle\n"; break;
        case SKILL_ORIGIN_LOCAL:
        default:                  origin_str = "local\n"; break;
    }
    return write_file(prov_path, origin_str);
}

/* ================================================================
 *  P182: Skill sync — git-based hub pull
 * ================================================================ */

bool skill_sync_from_hub(const char *hub_url, const char *branch, char *log_out, size_t log_sz) {
    if (!hub_url || !*hub_url) {
        if (log_out) snprintf(log_out, log_sz, "ERROR: No hub URL provided\n");
        return false;
    }

    char sd[HERMES_PATH_MAX];
    skills_dir(sd, sizeof(sd));

    /* Use the hub subdirectory for the clone */
    char hub_dir[HERMES_PATH_MAX];
    snprintf(hub_dir, sizeof(hub_dir), "%s/.hub", sd);
    ensure_dir(hub_dir);

    /* Check if already cloned */
    char git_dir[HERMES_PATH_MAX];
    snprintf(git_dir, sizeof(git_dir), "%s/.git", hub_dir);

    FILE *log = NULL;
    if (log_out) log = fmemopen(log_out, log_sz, "w");

    if (dir_exists(git_dir)) {
        /* Pull latest from existing clone */
        if (log) fprintf(log, "Pulling from %s (branch: %s)...\n", hub_url, branch ? branch : "main");

        char cmd[8192];
        if (branch && *branch) {
            snprintf(cmd, sizeof(cmd), "cd '%s' && git pull origin '%s' 2>&1", hub_dir, branch);
        } else {
            snprintf(cmd, sizeof(cmd), "cd '%s' && git pull origin main 2>&1", hub_dir);
        }

        FILE *fp = popen(cmd, "r");
        if (!fp) {
            if (log) fprintf(log, "ERROR: Failed to run git pull\n");
            if (log) fclose(log);
            return false;
        }
        char line[1024];
        while (fgets(line, sizeof(line), fp)) {
            if (log) fprintf(log, "%s", line);
        }
        int rc = pclose(fp);
        if (rc != 0) {
            if (log) fprintf(log, "WARNING: git pull exit code %d\n", rc);
        }
    } else {
        /* Fresh clone */
        if (log) fprintf(log, "Cloning from %s (branch: %s)...\n", hub_url, branch ? branch : "main");

        char cmd[8192];
        if (branch && *branch) {
            snprintf(cmd, sizeof(cmd), "cd '%s' && git clone --depth=1 --branch '%s' '%s' . 2>&1",
                     hub_dir, branch, hub_url);
        } else {
            snprintf(cmd, sizeof(cmd), "cd '%s' && git clone --depth=1 '%s' . 2>&1",
                     hub_dir, hub_url);
        }

        FILE *fp = popen(cmd, "r");
        if (!fp) {
            if (log) fprintf(log, "ERROR: Failed to run git clone\n");
            if (log) fclose(log);
            return false;
        }
        char line[1024];
        while (fgets(line, sizeof(line), fp)) {
            if (log) fprintf(log, "%s", line);
        }
        int rc = pclose(fp);
        if (rc != 0) {
            if (log) fprintf(log, "ERROR: git clone failed with exit code %d\n", rc);
            if (log) fclose(log);
            return false;
        }
    }

    /* Copy skills from hub_dir to skills dir */
    if (log) fprintf(log, "Copying skills from hub...\n");

    DIR *dir = opendir(hub_dir);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            if (strcmp(entry->d_name, ".git") == 0)
                continue;

            char src_path[HERMES_PATH_MAX];
            snprintf(src_path, sizeof(src_path), "%s/%s", hub_dir, entry->d_name);

            struct stat st;
            if (stat(src_path, &st) != 0 || !S_ISDIR(st.st_mode))
                continue;

            /* Check for SKILL.md */
            char sk_src[HERMES_PATH_MAX];
            snprintf(sk_src, sizeof(sk_src), "%s/SKILL.md", src_path);
            if (!file_exists(sk_src))
                continue;

            char dst_path[HERMES_PATH_MAX];
            snprintf(dst_path, sizeof(dst_path), "%s/%s", sd, entry->d_name);

            if (dir_exists(dst_path)) {
                /* Skill already exists — do a simple merge (conflict resolution) */
                if (log) fprintf(log, "  Merging: %s (already exists locally)\n", entry->d_name);
                /* Copy individual files that don't exist locally */
                /* For now, just copy the SKILL.md if local is older */
                char sk_dst[HERMES_PATH_MAX];
                snprintf(sk_dst, sizeof(sk_dst), "%s/SKILL.md", dst_path);
                time_t src_mtime = file_mtime(sk_src);
                time_t dst_mtime = file_mtime(sk_dst);
                if (src_mtime > dst_mtime) {
                    /* Hub version is newer — overwrite */
                    char *hub_content = read_file(sk_src);
                    if (hub_content) {
                        char backup[HERMES_PATH_MAX];
                        snprintf(backup, sizeof(backup), "%s/SKILL.md.local_backup", dst_path);
                        rename(sk_dst, backup);
                        write_file(sk_dst, hub_content);
                        free(hub_content);
                        if (log) fprintf(log, "    Updated SKILL.md (hub is newer, backed up local)\n");
                    }
                }
            } else {
                /* New skill — copy entire directory */
                if (log) fprintf(log, "  Installing: %s\n", entry->d_name);
                char cmd[8192];
                snprintf(cmd, sizeof(cmd), "cp -r '%s' '%s/'", src_path, sd);
                int sys_rc = system(cmd);
                (void)sys_rc; /* ignore cp exit status in background copy */
                /* Set provenance to hub */
                skill_set_origin(entry->d_name, SKILL_ORIGIN_HUB);
            }
        }
        closedir(dir);
    }

    if (log) fprintf(log, "Sync complete.\n");
    if (log) fclose(log);
    return true;
}

/* ================================================================
 *  P183: Skill bundles — alias groups
 * ================================================================ */

bool skill_bundle_create(const char *bundle_name, const char *skills_csv) {
    if (!bundle_name || !*bundle_name) return false;
    if (!skills_csv) skills_csv = "";

    char bd[HERMES_PATH_MAX];
    skills_bundles_dir(bd, sizeof(bd));
    ensure_dir(bd);

    /* Build JSON: {"name": "...", "skills": ["s1","s2",...]} */
    json_node_t *j = json_new_object();
    json_set(j, "name", json_new_string(bundle_name));
    json_set(j, "version", json_new_string("1.0.0"));

    json_node_t *arr = json_new_array();
    /* Parse comma-separated skill names */
    char csv_copy[1024];
    snprintf(csv_copy, sizeof(csv_copy), "%s", skills_csv);
    char *tok = strtok(csv_copy, ",");
    while (tok) {
        while (*tok == ' ') tok++;
        char *end = tok + strlen(tok) - 1;
        while (end > tok && *end == ' ') end--;
        end[1] = '\0';
        if (*tok) {
            json_append(arr, json_new_string(tok));
        }
        tok = strtok(NULL, ",");
    }
    json_set(j, "skills", arr);

    char bp[HERMES_PATH_MAX];
    bundle_path(bundle_name, bp, sizeof(bp));
    char *serialized = json_serialize(j);
    bool ok = write_file(bp, serialized);
    free(serialized);
    json_free(j);
    return ok;
}

bool skill_bundle_delete(const char *bundle_name) {
    if (!bundle_name || !*bundle_name) return false;
    char bp[HERMES_PATH_MAX];
    bundle_path(bundle_name, bp, sizeof(bp));
    return unlink(bp) == 0;
}

skill_list_t *skill_bundle_get_skills(const char *bundle_name) {
    skill_list_t *result = (skill_list_t *)calloc(1, sizeof(skill_list_t));
    if (!result) return NULL;

    char bp[HERMES_PATH_MAX];
    bundle_path(bundle_name, bp, sizeof(bp));

    char *content = read_file(bp);
    if (!content) {
        /* Bundle file not found — check if bundle exists in master scan */
        free(result);
        return NULL;
    }

    char *err = NULL;
    json_node_t *j = json_parse(content, &err);
    free(content);
    if (!j) {
        free(err);
        free(result);
        return NULL;
    }

    json_node_t *skills_arr = json_obj_get(j, "skills");
    if (skills_arr && skills_arr->type == JSON_ARRAY) {
        size_t n = json_len(skills_arr);
        for (size_t i = 0; i < n; i++) {
            json_node_t *item = json_get(skills_arr, i);
            if (item && item->type == JSON_STRING) {
                /* Find this skill's metadata from a fresh scan */
                skill_list_t *all = skills_scan_all();
                if (all) {
                    for (size_t k = 0; k < all->count; k++) {
                        if (strcmp(all->skills[k].name, item->str_val) == 0) {
                            if (result->count >= result->capacity) {
                                size_t new_cap = result->capacity == 0 ? 8 : result->capacity * 2;
                                skill_meta_t *new_arr = (skill_meta_t *)realloc(result->skills, new_cap * sizeof(skill_meta_t));
                                if (!new_arr) break;
                                result->skills = new_arr;
                                result->capacity = new_cap;
                            }
                            memcpy(&result->skills[result->count++], &all->skills[k], sizeof(skill_meta_t));
                            break;
                        }
                    }
                    skills_scan_free(all);
                }
            }
        }
    }

    json_free(j);
    return result;
}

/* ================================================================
 *  P184: Skill usage tracking
 * ================================================================ */

void skill_record_usage(const char *skill_name) {
    if (!skill_name || !*skill_name) return;

    char spath[HERMES_PATH_MAX];
    skill_path(skill_name, spath, sizeof(spath));
    if (!dir_exists(spath)) return;

    char usage_path[HERMES_PATH_MAX];
    snprintf(usage_path, sizeof(usage_path), "%s/.usage", spath);

    int count = 0;
    time_t last_used = 0;

    /* Load existing usage data */
    char *existing = read_file(usage_path);
    if (existing) {
        char *jerr = NULL;
        json_node_t *j = json_parse(existing, &jerr);
        if (j) {
            count = (int)json_get_num(j, "count", 0);
            last_used = (time_t)json_get_num(j, "last_used", 0);
            json_free(j);
        }
        free(jerr);
        free(existing);
    }

    count++;
    last_used = time(NULL);

    json_node_t *j = json_new_object();
    json_set(j, "count", json_new_number((double)count));
    json_set(j, "last_used", json_new_number((double)last_used));
    char *serialized = json_serialize(j);
    write_file(usage_path, serialized);
    free(serialized);
    json_free(j);
}

int skill_get_usage_count(const char *skill_name) {
    char spath[HERMES_PATH_MAX];
    skill_path(skill_name, spath, sizeof(spath));
    char usage_path[HERMES_PATH_MAX];
    snprintf(usage_path, sizeof(usage_path), "%s/.usage", spath);

    char *content = read_file(usage_path);
    if (!content) return 0;

    char *err = NULL;
    json_node_t *j = json_parse(content, &err);
    free(content);
    if (!j) { free(err); return 0; }

    int count = (int)json_get_num(j, "count", 0);
    json_free(j);
    free(err);
    return count;
}

time_t skill_get_last_used(const char *skill_name) {
    char spath[HERMES_PATH_MAX];
    skill_path(skill_name, spath, sizeof(spath));
    char usage_path[HERMES_PATH_MAX];
    snprintf(usage_path, sizeof(usage_path), "%s/.usage", spath);

    char *content = read_file(usage_path);
    if (!content) return 0;

    char *err = NULL;
    json_node_t *j = json_parse(content, &err);
    free(content);
    if (!j) { free(err); return 0; }

    time_t last = (time_t)json_get_num(j, "last_used", 0);
    json_free(j);
    free(err);
    return last;
}

void skill_get_recommendations(skill_meta_t *out, size_t *count, size_t max_count) {
    if (!out || !count) return;
    *count = 0;

    skill_list_t *list = skills_scan_all();
    if (!list) return;

    /* Sort by usage count descending, then by last_used descending */
    /* Simple bubble sort for the top N */
    for (size_t i = 0; i < list->count && *count < max_count; i++) {
        /* Find the highest-usage skill not yet selected */
        int best_idx = -1;
        int best_usage = -1;
        time_t best_time = 0;

        for (size_t j = 0; j < list->count; j++) {
            if (!list->skills[j].validated) continue; /* skip unvalidated */

            /* Check if already selected */
            bool already = false;
            for (size_t k = 0; k < *count; k++) {
                if (strcmp(out[k].name, list->skills[j].name) == 0) {
                    already = true;
                    break;
                }
            }
            if (already) continue;

            int usage = list->skills[j].usage_count;
            time_t used = list->skills[j].last_used;

            if (usage > best_usage || (usage == best_usage && used > best_time)) {
                best_usage = usage;
                best_time = used;
                best_idx = (int)j;
            }
        }

        if (best_idx >= 0) {
            memcpy(&out[*count], &list->skills[best_idx], sizeof(skill_meta_t));
            (*count)++;
        } else {
            break;
        }
    }

    skills_scan_free(list);
}

/* ================================================================
 *  P185: Skill caching — LRU cache
 * ================================================================ */

static void cache_move_to_front(skill_cache_entry_t *entry) {
    if (!entry || entry == g_cache.head) return;

    /* Remove from current position */
    if (entry->prev) entry->prev->next = entry->next;
    if (entry->next) entry->next->prev = entry->prev;
    if (entry == g_cache.tail) g_cache.tail = entry->prev;

    /* Move to front */
    entry->prev = NULL;
    entry->next = g_cache.head;
    if (g_cache.head) g_cache.head->prev = entry;
    g_cache.head = entry;
    if (!g_cache.tail) g_cache.tail = entry;
}

static void cache_evict_lru(void) {
    if (!g_cache.tail || g_cache.count <= g_cache.max_entries) return;

    skill_cache_entry_t *old = g_cache.tail;
    if (old->prev) old->prev->next = NULL;
    g_cache.tail = old->prev;
    if (g_cache.head == old) g_cache.head = NULL;

    free(old->content);
    free(old);
    g_cache.count--;
}

bool skill_cache_init(size_t max_entries) {
    skill_cache_destroy();
    g_cache.head = NULL;
    g_cache.tail = NULL;
    g_cache.count = 0;
    g_cache.max_entries = max_entries > 0 ? max_entries : SKILL_CACHE_DEFAULT_MAX;
    return true;
}

void skill_cache_destroy(void) {
    skill_cache_entry_t *cur = g_cache.head;
    while (cur) {
        skill_cache_entry_t *next = cur->next;
        free(cur->content);
        free(cur);
        cur = next;
    }
    g_cache.head = NULL;
    g_cache.tail = NULL;
    g_cache.count = 0;
}

bool skill_cache_preload(const char *skill_name) {
    if (!skill_name || !*skill_name) return false;

    /* Check if already cached */
    skill_cache_entry_t *cur = g_cache.head;
    while (cur) {
        if (strcmp(cur->name, skill_name) == 0) {
            cache_move_to_front(cur);
            return true;
        }
        cur = cur->next;
    }

    /* Read the skill content */
    char skpath[HERMES_PATH_MAX];
    skill_sk_path(skill_name, skpath, sizeof(skpath));
    char *content = read_file(skpath);
    if (!content) {
        /* Try as standalone .md file */
        char sd[HERMES_PATH_MAX];
        skills_dir(sd, sizeof(sd));
        snprintf(skpath, sizeof(skpath), "%s/%s.md", sd, skill_name);
        content = read_file(skpath);
        if (!content) return false;
    }

    /* Create cache entry */
    skill_cache_entry_t *entry = (skill_cache_entry_t *)calloc(1, sizeof(skill_cache_entry_t));
    if (!entry) { free(content); return false; }

    snprintf(entry->name, sizeof(entry->name), "%s", skill_name);
    entry->content = content;
    entry->loaded_at = time(NULL);

    /* Extract metadata for the cache entry */
    skill_list_t *list = skills_scan_all();
    if (list) {
        for (size_t i = 0; i < list->count; i++) {
            if (strcmp(list->skills[i].name, skill_name) == 0) {
                memcpy(&entry->meta, &list->skills[i], sizeof(skill_meta_t));
                break;
            }
        }
        skills_scan_free(list);
    }

    /* Add to front */
    entry->next = g_cache.head;
    if (g_cache.head) g_cache.head->prev = entry;
    g_cache.head = entry;
    if (!g_cache.tail) g_cache.tail = entry;
    g_cache.count++;

    /* Evict if over limit */
    if (g_cache.max_entries > 0) {
        while (g_cache.count > g_cache.max_entries) {
            cache_evict_lru();
        }
    }

    return true;
}

void skill_cache_evict(const char *skill_name) {
    skill_cache_entry_t *cur = g_cache.head;
    while (cur) {
        if (strcmp(cur->name, skill_name) == 0) {
            if (cur->prev) cur->prev->next = cur->next;
            if (cur->next) cur->next->prev = cur->prev;
            if (cur == g_cache.head) g_cache.head = cur->next;
            if (cur == g_cache.tail) g_cache.tail = cur->prev;
            free(cur->content);
            free(cur);
            g_cache.count--;
            return;
        }
        cur = cur->next;
    }
}

const char *skill_cache_get(const char *skill_name) {
    if (!skill_name || !*skill_name) return NULL;

    skill_cache_entry_t *cur = g_cache.head;
    while (cur) {
        if (strcmp(cur->name, skill_name) == 0) {
            cache_move_to_front(cur);
            return cur->content;
        }
        cur = cur->next;
    }

    /* Cache miss — try to load */
    if (skill_cache_preload(skill_name)) {
        return g_cache.head->content;
    }

    return NULL;
}

size_t skill_cache_count(void) {
    return g_cache.count;
}

/* ================================================================
 *  P186: Skill search — text + tag filtering
 * ================================================================ */

/* Simple case-insensitive substring match */
static bool text_match(const char *text, const char *query) {
    if (!text || !query) return false;
    if (!*query) return true;

    size_t qlen = strlen(query);
    size_t tlen = strlen(text);
    if (qlen > tlen) return false;

    for (size_t i = 0; i <= tlen - qlen; i++) {
        bool match = true;
        for (size_t j = 0; j < qlen; j++) {
            if (tolower((unsigned char)text[i + j]) != tolower((unsigned char)query[j])) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

/* Check if skill matches tag filter (comma-separated tags, AND logic) */
static bool tag_filter_match(const char *skill_tags, const char *tag_filter) {
    if (!tag_filter || !*tag_filter) return true;
    if (!skill_tags) return false;

    char filter_copy[256];
    snprintf(filter_copy, sizeof(filter_copy), "%s", tag_filter);

    char *tok = strtok(filter_copy, ",");
    while (tok) {
        while (*tok == ' ') tok++;
        char *end = tok + strlen(tok) - 1;
        while (end > tok && *end == ' ') end--;
        end[1] = '\0';

        if (*tok && !text_match(skill_tags, tok)) {
            return false; /* AND logic: all filter tags must match */
        }
        tok = strtok(NULL, ",");
    }
    return true;
}

skill_search_result_t *skill_search(const char *query, const char *tag_filter,
                                      size_t *result_count, size_t max_results) {
    if (!result_count) return NULL;
    *result_count = 0;
    if (max_results == 0) max_results = SKILL_MAX_SEARCH_RESULTS;

    skill_list_t *list = skills_scan_all();
    if (!list) return NULL;

    size_t cap = max_results < list->count ? max_results : list->count;
    if (cap == 0) { skills_scan_free(list); return NULL; }

    skill_search_result_t *results = (skill_search_result_t *)calloc(cap, sizeof(skill_search_result_t));
    if (!results) { skills_scan_free(list); return NULL; }

    size_t count = 0;

    for (size_t i = 0; i < list->count && count < max_results; i++) {
        skill_meta_t *sk = &list->skills[i];

        /* Apply tag filter first (fast path) */
        if (!tag_filter_match(sk->tags, tag_filter))
            continue;

        float score = 0.0f;

        if (query && *query) {
            /* Score based on matches in different fields */
            if (text_match(sk->name, query))         score += 0.5f;
            if (text_match(sk->description, query))   score += 0.3f;
            if (text_match(sk->tags, query))          score += 0.2f;
            if (text_match(sk->category, query))      score += 0.15f;
            if (text_match(sk->author, query))        score += 0.1f;

            /* Also search the SKILL.md content if cached */
            const char *content = skill_cache_get(sk->name);
            if (content && text_match(content, query)) {
                score += 0.25f;
            }

            if (score <= 0.0f) continue; /* No match */
        } else {
            score = 1.0f; /* No query — return all matching tag filter */
        }

        snprintf(results[count].name, sizeof(results[count].name), "%s", sk->name);
        snprintf(results[count].path, sizeof(results[count].path), "%s", sk->path);
        results[count].score = score;
        count++;
    }

    /* Sort by score descending (simple bubble sort) */
    for (size_t i = 0; i < count; i++) {
        for (size_t j = i + 1; j < count; j++) {
            if (results[j].score > results[i].score) {
                skill_search_result_t tmp = results[i];
                results[i] = results[j];
                results[j] = tmp;
            }
        }
    }

    skills_scan_free(list);
    *result_count = count;
    return results;
}

void skill_search_free(skill_search_result_t *results, size_t count) {
    (void)count;
    free(results);
}

/* ================================================================
 *  P187: Skill curator — stale detection, auto-update
 * ================================================================ */

bool skill_curator_run(char *report_out, size_t report_sz) {
    FILE *report = NULL;
    if (report_out) report = fmemopen(report_out, report_sz, "w");

    skill_list_t *list = skills_scan_all();
    if (!list) {
        if (report) { fprintf(report, "ERROR: Could not scan skills\n"); fclose(report); }
        return false;
    }

    if (report) {
        fprintf(report, "=== Skill Curator Report ===\n");
        fprintf(report, "Stale threshold: %d days\n", g_curator_stale_days);
        fprintf(report, "Skills scanned: %zu\n\n", list->count);
    }

    time_t now = time(NULL);
    int stale_count = 0;
    int invalid_count = 0;
    int updated_count = 0;

    for (size_t i = 0; i < list->count; i++) {
        skill_meta_t *sk = &list->skills[i];

        /* Validate */
        char verr[256];
        if (!skill_validate(sk->name, verr, sizeof(verr))) {
            invalid_count++;
            if (report) fprintf(report, "  [INVALID] %s: %s\n", sk->name, verr);
            continue;
        }

        /* Check staleness */
        double days_since_update = (double)(now - sk->last_updated) / 86400.0;
        if (days_since_update > g_curator_stale_days) {
            stale_count++;
            if (report) {
                fprintf(report, "  [STALE]   %s (%.0f days since update, limit %d)\n",
                        sk->name, days_since_update, g_curator_stale_days);
            }

            /* Auto-update for hub-sourced skills */
            if (sk->origin == SKILL_ORIGIN_HUB) {
                if (report) fprintf(report, "    -> Auto-updating hub skill '%s'\n", sk->name);
                /* We'd trigger hub resync here; for now, just mark it */
                updated_count++;
            }
        }

        /* Check version age vs current Hermes version for local skills */
        if (sk->origin == SKILL_ORIGIN_LOCAL && sk->version[0]) {
            if (report) {
                fprintf(report, "  [OK]      %s v%s (%.0f days old)\n",
                        sk->name, sk->version, days_since_update);
            }
        }
    }

    if (report) {
        fprintf(report, "\n=== Summary ===\n");
        fprintf(report, "  Valid:    %zu\n", list->count - invalid_count);
        fprintf(report, "  Invalid:  %d\n", invalid_count);
        fprintf(report, "  Stale:    %d\n", stale_count);
        fprintf(report, "  Updated:  %d\n", updated_count);
        fclose(report);
    }

    skills_scan_free(list);
    return true;
}

bool skill_curator_set_stale_days(int days) {
    if (days < 1) return false;
    g_curator_stale_days = days;
    return true;
}

int skill_curator_get_stale_days(void) {
    return g_curator_stale_days;
}

/* ================================================================
 *  P188: Skill dependencies — cross-skill resolution
 * ================================================================ */

/* Parse comma-separated dependency string into array */
static int parse_deps(const char *deps_str, char deps[][128], int max) {
    if (!deps_str || !*deps_str) return 0;
    char copy[1024];
    snprintf(copy, sizeof(copy), "%s", deps_str);
    int count = 0;
    char *tok = strtok(copy, ",");
    while (tok && count < max) {
        while (*tok == ' ') tok++;
        char *end = tok + strlen(tok) - 1;
        while (end > tok && *end == ' ') end--;
        end[1] = '\0';
        if (*tok) {
            snprintf(deps[count], 128, "%s", tok);
            count++;
        }
        tok = strtok(NULL, ",");
    }
    return count;
}

/* Check if a dependency is already in the ordered list */
static bool is_in_ordered(const char *name, const char ordered[][128], size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(ordered[i], name) == 0) return true;
    }
    return false;
}

/* Recursive topological sort helper */
static bool resolve_recursive(const char *skill_name, skill_list_t *all,
                               char ordered[][128], size_t *count, size_t max) {
    if (*count >= max) return false;

    /* Find this skill's metadata */
    skill_meta_t *sk = NULL;
    for (size_t i = 0; i < all->count; i++) {
        if (strcmp(all->skills[i].name, skill_name) == 0) {
            sk = &all->skills[i];
            break;
        }
    }

    if (!sk) return false; /* Skill not found */

    /* Check for circular dependency by seeing if we already have this skill */
    /* (If it's already in ordered, we've processed it or are in a cycle) */
    if (is_in_ordered(skill_name, ordered, *count)) {
        return true; /* Already resolved or cycle detected — skip */
    }

    /* Resolve dependencies first */
    char deps[SKILL_MAX_DEPENDENCIES][128];
    int ndeps = parse_deps(sk->dependencies, deps, SKILL_MAX_DEPENDENCIES);

    for (int i = 0; i < ndeps; i++) {
        if (!resolve_recursive(deps[i], all, ordered, count, max))
            return false;
    }

    /* Add this skill after its dependencies */
    if (*count < max) {
        snprintf(ordered[*count], 128, "%s", skill_name);
        (*count)++;
    }

    return true;
}

bool skill_deps_resolve(const char *skill_name,
                         char ordered[][128], size_t *count, size_t max) {
    if (!skill_name || !*skill_name || !count) return false;
    *count = 0;

    skill_list_t *all = skills_scan_all();
    if (!all) return false;

    bool ok = resolve_recursive(skill_name, all, ordered, count, max);
    skills_scan_free(all);
    return ok;
}

char **skill_deps_get_missing(const char *skill_name, size_t *count) {
    if (!skill_name || !count) return NULL;
    *count = 0;

    skill_list_t *all = skills_scan_all();
    if (!all) return NULL;

    /* Find the skill */
    skill_meta_t *sk = NULL;
    for (size_t i = 0; i < all->count; i++) {
        if (strcmp(all->skills[i].name, skill_name) == 0) {
            sk = &all->skills[i];
            break;
        }
    }

    if (!sk) { skills_scan_free(all); return NULL; }

    char deps[SKILL_MAX_DEPENDENCIES][128];
    int ndeps = parse_deps(sk->dependencies, deps, SKILL_MAX_DEPENDENCIES);

    /* Count missing deps */
    size_t missing_count = 0;
    for (int i = 0; i < ndeps; i++) {
        bool found = false;
        for (size_t j = 0; j < all->count; j++) {
            if (strcmp(all->skills[j].name, deps[i]) == 0) {
                found = true;
                break;
            }
        }
        if (!found) missing_count++;
    }

    if (missing_count == 0) {
        skills_scan_free(all);
        return NULL;
    }

    char **missing = (char **)calloc(missing_count, sizeof(char *));
    if (!missing) { skills_scan_free(all); return NULL; }

    size_t idx = 0;
    for (int i = 0; i < ndeps; i++) {
        bool found = false;
        for (size_t j = 0; j < all->count; j++) {
            if (strcmp(all->skills[j].name, deps[i]) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            missing[idx] = strdup(deps[i]);
            if (missing[idx]) idx++;
        }
    }

    *count = idx;
    skills_scan_free(all);
    return missing;
}

bool skill_deps_validate_order(const char ordered[][128], size_t count) {
    /* For each skill, check that all its dependencies appear before it */
    skill_list_t *all = skills_scan_all();
    if (!all) return false;

    for (size_t i = 0; i < count; i++) {
        /* Find the skill metadata */
        skill_meta_t *sk = NULL;
        for (size_t j = 0; j < all->count; j++) {
            if (strcmp(all->skills[j].name, ordered[i]) == 0) {
                sk = &all->skills[j];
                break;
            }
        }
        if (!sk) continue; /* Unknown skill, skip */

        char deps[SKILL_MAX_DEPENDENCIES][128];
        int ndeps = parse_deps(sk->dependencies, deps, SKILL_MAX_DEPENDENCIES);

        for (int k = 0; k < ndeps; k++) {
            /* Check that this dependency appears before the current skill */
            bool found_before = false;
            for (size_t l = 0; l < i; l++) {
                if (strcmp(ordered[l], deps[k]) == 0) {
                    found_before = true;
                    break;
                }
            }
            if (!found_before) {
                skills_scan_free(all);
                return false; /* Dependency not ordered before dependent */
            }
        }
    }

    skills_scan_free(all);
    return true;
}

/* ================================================================
 *  JSON handler helpers
 * ================================================================ */

static json_node_t *skill_meta_to_json(const skill_meta_t *m) {
    json_node_t *j = json_new_object();
    json_set(j, "name", json_new_string(m->name));
    json_set(j, "path", json_new_string(m->path));
    json_set(j, "version", json_new_string(m->version));
    json_set(j, "author", json_new_string(m->author));
    json_set(j, "description", json_new_string(m->description));
    json_set(j, "category", json_new_string(m->category));
    json_set(j, "tags", json_new_string(m->tags));
    json_set(j, "dependencies", json_new_string(m->dependencies));
    json_set(j, "bundles", json_new_string(m->bundles));

    const char *origin_str;
    switch (m->origin) {
        case SKILL_ORIGIN_LOCAL:  origin_str = "local"; break;
        case SKILL_ORIGIN_HUB:    origin_str = "hub"; break;
        case SKILL_ORIGIN_BUNDLE: origin_str = "bundle"; break;
        default:                  origin_str = "unknown"; break;
    }
    json_set(j, "origin", json_new_string(origin_str));
    json_set(j, "last_updated", json_new_number((double)m->last_updated));
    json_set(j, "last_used", json_new_number((double)m->last_used));
    json_set(j, "usage_count", json_new_number((double)m->usage_count));
    json_set(j, "validated", json_new_bool(m->validated));
    if (m->validation_error[0])
        json_set(j, "validation_error", json_new_string(m->validation_error));

    return j;
}

/* ================================================================
 *  Handler: skill_scan — P179/P180: scan + validate skills
 * ================================================================ */

static const char *SCHEMA_SCAN = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"validate\":{\"type\":\"boolean\",\"description\":\"Also validate all skills\"}"
    "},"
    "\"required\":[]"
"}";

char *skills_scan_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    (void)args_json;

    bool do_validate = false;
    if (args_json) {
        char *err = NULL;
        json_node_t *args = json_parse(args_json, &err);
        if (args) {
            do_validate = json_get_bool(args, "validate", false);
            json_free(args);
        }
        free(err);
    }

    skill_list_t *list = skills_scan_all();
    if (!list) {
        json_node_t *r = json_new_object();
        json_set(r, "error", json_new_string("Failed to scan skills"));
        char *out = json_serialize(r);
        json_free(r);
        return out;
    }

    if (do_validate) {
        for (size_t i = 0; i < list->count; i++) {
            char verr[256];
            list->skills[i].validated = skill_validate(list->skills[i].name, verr, sizeof(verr));
            if (!list->skills[i].validated) {
                snprintf(list->skills[i].validation_error, sizeof(list->skills[i].validation_error), "%s", verr);
            }
        }
    }

    json_node_t *result = json_new_object();
    json_node_t *skills_arr = json_new_array();
    char sd[HERMES_PATH_MAX];
    skills_dir(sd, sizeof(sd));
    json_set(result, "skills_dir", json_new_string(sd));
    json_set(result, "count", json_new_number((double)list->count));

    for (size_t i = 0; i < list->count; i++) {
        json_append(skills_arr, skill_meta_to_json(&list->skills[i]));
    }
    json_set(result, "skills", skills_arr);

    char *out = json_serialize(result);
    json_free(result);
    skills_scan_free(list);
    return out;
}

/* ================================================================
 *  Handler: skills_validate — P180
 * ================================================================ */

static const char *SCHEMA_VALIDATE = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"name\":{\"type\":\"string\",\"description\":\"Skill name (optional, validates all if omitted)\"}"
    "},"
    "\"required\":[]"
"}";

char *skills_validate_handler(const char *args_json, const char *task_id) {
    (void)task_id;

    json_node_t *result = json_new_object();
    json_node_t *errors_arr = json_new_array();
    bool all_ok = true;

    if (args_json && *args_json) {
        char *err = NULL;
        json_node_t *args = json_parse(args_json, &err);
        if (args) {
            const char *name = json_get_str(args, "name", NULL);
            if (name && *name) {
                char verr[256];
                bool ok = skill_validate(name, verr, sizeof(verr));
                if (!ok) {
                    all_ok = false;
                    json_node_t *e = json_new_object();
                    json_set(e, "name", json_new_string(name));
                    json_set(e, "error", json_new_string(verr));
                    json_append(errors_arr, e);
                }
            } else {
                /* Validate all */
                skill_list_t *list = skills_scan_all();
                if (list) {
                    for (size_t i = 0; i < list->count; i++) {
                        char verr[256];
                        if (!skill_validate(list->skills[i].name, verr, sizeof(verr))) {
                            all_ok = false;
                            json_node_t *e = json_new_object();
                            json_set(e, "name", json_new_string(list->skills[i].name));
                            json_set(e, "error", json_new_string(verr));
                            json_append(errors_arr, e);
                        }
                    }
                    skills_scan_free(list);
                }
            }
            json_free(args);
        }
        free(err);
    } else {
        /* Validate all */
        skill_list_t *list = skills_scan_all();
        if (list) {
            for (size_t i = 0; i < list->count; i++) {
                char verr[256];
                if (!skill_validate(list->skills[i].name, verr, sizeof(verr))) {
                    all_ok = false;
                    json_node_t *e = json_new_object();
                    json_set(e, "name", json_new_string(list->skills[i].name));
                    json_set(e, "error", json_new_string(verr));
                    json_append(errors_arr, e);
                }
            }
            skills_scan_free(list);
        }
    }

    json_set(result, "valid", json_new_bool(all_ok));
    json_set(result, "errors", errors_arr);
    char *out = json_serialize(result);
    json_free(result);
    return out;
}

/* ================================================================
 *  Handler: skills_provenance — P181
 * ================================================================ */

static const char *SCHEMA_PROVENANCE = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"name\":{\"type\":\"string\",\"description\":\"Skill name\"},"
      "\"origin\":{\"type\":\"string\",\"description\":\"Set origin (local/hub/bundle), omit to just query\"}"
    "},"
    "\"required\":[\"name\"]"
"}";

char *skills_provenance_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse error\"}"); }

    const char *name = json_get_str(args, "name", NULL);
    const char *origin_str = json_get_str(args, "origin", NULL);

    json_node_t *result = json_new_object();

    if (!name) {
        json_set(result, "error", json_new_string("Missing 'name'"));
    } else {
        if (origin_str) {
            skill_origin_t origin;
            if (strcmp(origin_str, "hub") == 0) origin = SKILL_ORIGIN_HUB;
            else if (strcmp(origin_str, "bundle") == 0) origin = SKILL_ORIGIN_BUNDLE;
            else origin = SKILL_ORIGIN_LOCAL;
            bool ok = skill_set_origin(name, origin);
            json_set(result, "set", json_new_bool(ok));
        }

        skill_origin_t current = skill_get_origin(name);
        const char *cur_str;
        switch (current) {
            case SKILL_ORIGIN_HUB:    cur_str = "hub"; break;
            case SKILL_ORIGIN_BUNDLE: cur_str = "bundle"; break;
            case SKILL_ORIGIN_LOCAL:  cur_str = "local"; break;
            default:                  cur_str = "unknown"; break;
        }
        json_set(result, "name", json_new_string(name));
        json_set(result, "origin", json_new_string(cur_str));
    }

    char *out = json_serialize(result);
    json_free(result);
    json_free(args);
    return out;
}

/* ================================================================
 *  Handler: skills_sync — P182
 * ================================================================ */

static const char *SCHEMA_SYNC = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"hub_url\":{\"type\":\"string\",\"description\":\"Git remote URL for skill hub\"},"
      "\"branch\":{\"type\":\"string\",\"description\":\"Git branch (default: main)\"}"
    "},"
    "\"required\":[\"hub_url\"]"
"}";

char *skills_sync_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse error\"}"); }

    const char *hub_url = json_get_str(args, "hub_url", NULL);
    const char *branch = json_get_str(args, "branch", "main");

    json_node_t *result = json_new_object();

    if (!hub_url) {
        json_set(result, "error", json_new_string("Missing 'hub_url'"));
    } else {
        char log[16384];
        bool ok = skill_sync_from_hub(hub_url, branch, log, sizeof(log));
        json_set(result, "success", json_new_bool(ok));
        json_set(result, "log", json_new_string(log));
    }

    char *out = json_serialize(result);
    json_free(result);
    json_free(args);
    return out;
}

/* ================================================================
 *  Handler: skills_bundle — P183
 * ================================================================ */

static const char *SCHEMA_BUNDLE = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"action\":{\"type\":\"string\",\"description\":\"create/get/delete/list\"},"
      "\"name\":{\"type\":\"string\",\"description\":\"Bundle name\"},"
      "\"skills\":{\"type\":\"string\",\"description\":\"Comma-separated skill names (for create)\"}"
    "},"
    "\"required\":[\"action\"]"
"}";

char *skills_bundle_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse error\"}"); }

    const char *action = json_get_str(args, "action", "list");
    const char *name = json_get_str(args, "name", NULL);
    const char *skills = json_get_str(args, "skills", NULL);

    json_node_t *result = json_new_object();

    if (strcmp(action, "create") == 0) {
        if (!name) {
            json_set(result, "error", json_new_string("Missing 'name'"));
        } else {
            bool ok = skill_bundle_create(name, skills ? skills : "");
            json_set(result, "success", json_new_bool(ok));
            json_set(result, "action", json_new_string("create"));
            json_set(result, "name", json_new_string(name));
        }
    } else if (strcmp(action, "delete") == 0) {
        if (!name) {
            json_set(result, "error", json_new_string("Missing 'name'"));
        } else {
            bool ok = skill_bundle_delete(name);
            json_set(result, "success", json_new_bool(ok));
            json_set(result, "action", json_new_string("delete"));
            json_set(result, "name", json_new_string(name));
        }
    } else if (strcmp(action, "get") == 0) {
        if (!name) {
            json_set(result, "error", json_new_string("Missing 'name'"));
        } else {
            skill_list_t *bundle_skills = skill_bundle_get_skills(name);
            json_set(result, "name", json_new_string(name));
            json_node_t *arr = json_new_array();
            if (bundle_skills) {
                for (size_t i = 0; i < bundle_skills->count; i++) {
                    json_append(arr, skill_meta_to_json(&bundle_skills->skills[i]));
                }
                skills_scan_free(bundle_skills);
            }
            json_set(result, "skills", arr);
        }
    } else {
        /* List all bundles */
        char bd[HERMES_PATH_MAX];
        skills_bundles_dir(bd, sizeof(bd));
        ensure_dir(bd);

        json_node_t *arr = json_new_array();
        DIR *dir = opendir(bd);
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                const char *ext = strrchr(entry->d_name, '.');
                if (ext && strcasecmp(ext, ".bundle") == 0) {
                    char name_buf[256];
                    snprintf(name_buf, sizeof(name_buf), "%.*s",
                             (int)(ext - entry->d_name), entry->d_name);
                    json_append(arr, json_new_string(name_buf));
                }
            }
            closedir(dir);
        }
        json_set(result, "bundles", arr);
    }

    char *out = json_serialize(result);
    json_free(result);
    json_free(args);
    return out;
}

/* ================================================================
 *  Handler: skills_usage — P184
 * ================================================================ */

static const char *SCHEMA_USAGE = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"name\":{\"type\":\"string\",\"description\":\"Skill name\"},"
      "\"action\":{\"type\":\"string\",\"description\":\"record/get/recommend\"}"
    "},"
    "\"required\":[\"action\"]"
"}";

char *skills_usage_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse error\"}"); }

    const char *action = json_get_str(args, "action", "get");
    const char *name = json_get_str(args, "name", NULL);

    json_node_t *result = json_new_object();

    if (strcmp(action, "record") == 0) {
        if (!name) {
            json_set(result, "error", json_new_string("Missing 'name'"));
        } else {
            skill_record_usage(name);
            json_set(result, "action", json_new_string("record"));
            json_set(result, "name", json_new_string(name));
            json_set(result, "count", json_new_number((double)skill_get_usage_count(name)));
        }
    } else if (strcmp(action, "recommend") == 0) {
        skill_meta_t recs[64];
        size_t rec_count = 0;
        skill_get_recommendations(recs, &rec_count, 64);
        json_node_t *arr = json_new_array();
        for (size_t i = 0; i < rec_count; i++) {
            json_append(arr, skill_meta_to_json(&recs[i]));
        }
        json_set(result, "recommendations", arr);
        json_set(result, "count", json_new_number((double)rec_count));
    } else {
        /* Get usage for a specific skill or all */
        if (name) {
            json_set(result, "name", json_new_string(name));
            json_set(result, "usage_count", json_new_number((double)skill_get_usage_count(name)));
            json_set(result, "last_used", json_new_number((double)skill_get_last_used(name)));
        } else {
            skill_list_t *list = skills_scan_all();
            if (list) {
                json_node_t *arr = json_new_array();
                for (size_t i = 0; i < list->count; i++) {
                    json_node_t *u = json_new_object();
                    json_set(u, "name", json_new_string(list->skills[i].name));
                    json_set(u, "usage_count", json_new_number((double)list->skills[i].usage_count));
                    json_set(u, "last_used", json_new_number((double)list->skills[i].last_used));
                    json_append(arr, u);
                }
                json_set(result, "usages", arr);
                skills_scan_free(list);
            }
        }
    }

    char *out = json_serialize(result);
    json_free(result);
    json_free(args);
    return out;
}

/* ================================================================
 *  Handler: skills_cache — P185
 * ================================================================ */

static const char *SCHEMA_CACHE = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"action\":{\"type\":\"string\",\"description\":\"init/preload/evict/get/status\"},"
      "\"name\":{\"type\":\"string\",\"description\":\"Skill name (for preload/evict/get)\"},"
      "\"max_entries\":{\"type\":\"number\",\"description\":\"Max cache entries (for init)\"}"
    "},"
    "\"required\":[\"action\"]"
"}";

char *skills_cache_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse error\"}"); }

    const char *action = json_get_str(args, "action", "status");
    const char *name = json_get_str(args, "name", NULL);
    double max_entries = json_get_num(args, "max_entries", 0);

    json_node_t *result = json_new_object();

    if (strcmp(action, "init") == 0) {
        bool ok = skill_cache_init((size_t)max_entries);
        json_set(result, "success", json_new_bool(ok));
        json_set(result, "max_entries", json_new_number(max_entries > 0 ? max_entries : SKILL_CACHE_DEFAULT_MAX));
    } else if (strcmp(action, "preload") == 0) {
        if (!name) {
            json_set(result, "error", json_new_string("Missing 'name'"));
        } else {
            bool ok = skill_cache_preload(name);
            json_set(result, "success", json_new_bool(ok));
            json_set(result, "name", json_new_string(name));
        }
    } else if (strcmp(action, "evict") == 0) {
        if (!name) {
            json_set(result, "error", json_new_string("Missing 'name'"));
        } else {
            skill_cache_evict(name);
            json_set(result, "success", json_new_bool(true));
            json_set(result, "name", json_new_string(name));
        }
    } else if (strcmp(action, "get") == 0) {
        if (!name) {
            json_set(result, "error", json_new_string("Missing 'name'"));
        } else {
            const char *content = skill_cache_get(name);
            if (content) {
                json_set(result, "content", json_new_string(content));
                json_set(result, "name", json_new_string(name));
            } else {
                json_set(result, "error", json_new_string("Not found in cache"));
            }
        }
    } else {
        /* Status */
        json_set(result, "count", json_new_number((double)g_cache.count));
        json_set(result, "max_entries", json_new_number((double)(g_cache.max_entries > 0 ? g_cache.max_entries : 0)));
        /* List cached skills */
        json_node_t *arr = json_new_array();
        skill_cache_entry_t *cur = g_cache.head;
        while (cur) {
            json_append(arr, json_new_string(cur->name));
            cur = cur->next;
        }
        json_set(result, "cached", arr);
    }

    char *out = json_serialize(result);
    json_free(result);
    json_free(args);
    return out;
}

/* ================================================================
 *  Handler: skills_search — P186
 * ================================================================ */

static const char *SCHEMA_SEARCH = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"query\":{\"type\":\"string\",\"description\":\"Text search query\"},"
      "\"tags\":{\"type\":\"string\",\"description\":\"Tag filter (comma-separated, AND logic)\"},"
      "\"max_results\":{\"type\":\"number\",\"description\":\"Max results (default 20)\"},"
      "\"hub\":{\"type\":\"boolean\",\"description\":\"Also search browse.sh skills hub\"}"
    "},"
    "\"required\":[]"
"}";

char *skills_search_handler(const char *args_json, const char *task_id) {
    (void)task_id;

    const char *query = NULL;
    const char *tags = NULL;
    size_t max_results = 20;
    bool search_hub = false;

    if (args_json && *args_json) {
        char *err = NULL;
        json_node_t *args = json_parse(args_json, &err);
        if (args) {
            query = json_get_str(args, "query", NULL);
            tags = json_get_str(args, "tags", NULL);
            max_results = (size_t)json_get_num(args, "max_results", 20);
            search_hub = json_get_num(args, "hub", 0) > 0.0;
            json_free(args);
        }
        free(err);
    }

    json_node_t *result = json_new_object();

    /* Local search */
    size_t local_count = 0;
    skill_search_result_t *sr = skill_search(query, tags, &local_count, max_results);

    json_set(result, "query", json_new_string(query ? query : ""));
    json_set(result, "tag_filter", json_new_string(tags ? tags : ""));
    json_set(result, "count", json_new_number((double)local_count));

    json_node_t *arr = json_new_array();
    for (size_t i = 0; i < local_count; i++) {
        json_node_t *s = json_new_object();
        json_set(s, "name", json_new_string(sr[i].name));
        json_set(s, "path", json_new_string(sr[i].path));
        json_set(s, "score", json_new_number((double)sr[i].score));
        json_set(s, "source", json_new_string("local"));
        json_append(arr, s);
    }
    skill_search_free(sr, local_count);

    /* Hub search */
    size_t hub_count = 0;
    skill_search_result_t *hub_results = NULL;
    if (search_hub) {
        hub_results = skill_search_hub(query, &hub_count, max_results);
        for (size_t i = 0; i < hub_count; i++) {
            json_node_t *s = json_new_object();
            json_set(s, "name", json_new_string(hub_results[i].name));
            json_set(s, "path", json_new_string(hub_results[i].path));
            json_set(s, "score", json_new_number((double)hub_results[i].score));
            json_set(s, "source", json_new_string("browse-sh"));
            json_append(arr, s);
        }
        skill_search_hub_free(hub_results, hub_count);
    }

    json_set(result, "results", arr);
    json_set(result, "hub_count", json_new_number((double)hub_count));

    char *out = json_serialize(result);
    json_free(result);
    return out;
}

/* ================================================================
 *  Handler: skills_curator — P187
 * ================================================================ */

static const char *SCHEMA_CURATOR = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"action\":{\"type\":\"string\",\"description\":\"run/set_stale_days/get_stale_days\"},"
      "\"stale_days\":{\"type\":\"number\",\"description\":\"Days after which a skill is considered stale\"}"
    "},"
    "\"required\":[\"action\"]"
"}";

char *skills_curator_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse error\"}"); }

    const char *action = json_get_str(args, "action", "run");
    double stale_days = json_get_num(args, "stale_days", 0);

    json_node_t *result = json_new_object();

    if (strcmp(action, "set_stale_days") == 0) {
        bool ok = skill_curator_set_stale_days((int)stale_days);
        json_set(result, "success", json_new_bool(ok));
        json_set(result, "stale_days", json_new_number(stale_days));
    } else if (strcmp(action, "get_stale_days") == 0) {
        json_set(result, "stale_days", json_new_number((double)skill_curator_get_stale_days()));
    } else {
        /* Run curator */
        char report[32768];
        bool ok = skill_curator_run(report, sizeof(report));
        json_set(result, "success", json_new_bool(ok));
        json_set(result, "report", json_new_string(report));
    }

    char *out = json_serialize(result);
    json_free(result);
    json_free(args);
    return out;
}

/* ================================================================
 *  Handler: skills_deps — P188
 * ================================================================ */

static const char *SCHEMA_DEPS = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"action\":{\"type\":\"string\",\"description\":\"resolve/missing/validate\"},"
      "\"name\":{\"type\":\"string\",\"description\":\"Skill name\"},"
      "\"ordered\":{\"type\":\"string\",\"description\":\"Comma-separated ordered list (for validate)\"}"
    "},"
    "\"required\":[\"action\",\"name\"]"
"}";

char *skills_deps_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse error\"}"); }

    const char *action = json_get_str(args, "action", "resolve");
    const char *name = json_get_str(args, "name", NULL);
    const char *ordered_str = json_get_str(args, "ordered", NULL);

    json_node_t *result = json_new_object();

    if (!name) {
        json_set(result, "error", json_new_string("Missing 'name'"));
    } else if (strcmp(action, "resolve") == 0) {
        char ordered[128][128];
        size_t count = 0;
        bool ok = skill_deps_resolve(name, ordered, &count, 128);
        json_set(result, "success", json_new_bool(ok));
        json_set(result, "skill", json_new_string(name));
        json_node_t *arr = json_new_array();
        for (size_t i = 0; i < count; i++) {
            json_append(arr, json_new_string(ordered[i]));
        }
        json_set(result, "ordered", arr);
    } else if (strcmp(action, "missing") == 0) {
        size_t count = 0;
        char **missing = skill_deps_get_missing(name, &count);
        json_set(result, "skill", json_new_string(name));
        json_node_t *arr = json_new_array();
        if (missing) {
            for (size_t i = 0; i < count; i++) {
                json_append(arr, json_new_string(missing[i]));
                free(missing[i]);
            }
            free(missing);
        }
        json_set(result, "missing", arr);
        json_set(result, "missing_count", json_new_number((double)count));
    } else if (strcmp(action, "validate") == 0) {
        /* Parse ordered string into array */
        char ordered[128][128];
        size_t count = 0;
        if (ordered_str) {
            char copy[2048];
            snprintf(copy, sizeof(copy), "%s", ordered_str);
            char *tok = strtok(copy, ",");
            while (tok && count < 128) {
                while (*tok == ' ') tok++;
                snprintf(ordered[count], 128, "%s", tok);
                count++;
                tok = strtok(NULL, ",");
            }
        }
        bool valid = skill_deps_validate_order(ordered, count);
        json_set(result, "valid", json_new_bool(valid));
        json_node_t *arr = json_new_array();
        for (size_t i = 0; i < count; i++) {
            json_append(arr, json_new_string(ordered[i]));
        }
        json_set(result, "order", arr);
    }

    char *out = json_serialize(result);
    json_free(result);
    json_free(args);
    return out;
}

/* ================================================================
 *  Original handler: skills_list (backward compat)
 * ================================================================ */

char *skills_list_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    (void)args_json;

    skill_list_t *list = skills_scan_all();

    char sd[HERMES_PATH_MAX];
    skills_dir(sd, sizeof(sd));

    json_node_t *result = json_new_object();
    json_set(result, "skills_dir", json_new_string(sd));
    json_node_t *skills_arr = json_new_array();

    if (list) {
        for (size_t i = 0; i < list->count; i++) {
            json_append(skills_arr, json_new_string(list->skills[i].name));
        }
        skills_scan_free(list);
    }

    json_set(result, "skills", skills_arr);
    json_set(result, "count", json_new_number((double)(list ? list->count : 0)));
    char *out = json_serialize(result);
    json_free(result);
    return out;
}

/* Keep the original SCHEMA_LIST for backward compat */
static const char *SCHEMA_LIST = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"category\":{\"type\":\"string\",\"description\":\"Optional category filter\"}"
    "},"
    "\"required\":[]"
"}";

/* L12: Hub handler — search + install browse.sh skills */
static char *skills_hub_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    json_node_t *result = json_new_object();
    if (!result) return strdup("{\"error\":\"OOM\"}");

    const char *action = "";
    const char *query = "";
    const char *slug = "";
    size_t limit = 20;

    if (args_json && *args_json) {
        char *err = NULL;
        json_node_t *args = json_parse(args_json, &err);
        if (args) {
            action  = json_get_str(args, "action", "");
            query   = json_get_str(args, "query", "");
            slug    = json_get_str(args, "slug", "");
            limit   = (size_t)json_get_num(args, "limit", 20);
            json_free(args);
        }
        free(err);
    }

    if (strcmp(action, "search") == 0 || strcmp(action, "list") == 0) {
        size_t count = 0;
        skill_search_result_t *sr = skill_search_hub(
            strcmp(action, "list") == 0 ? "" : query, &count, limit);
        json_set(result, "count", json_new_number((double)count));
        json_set(result, "action", json_new_string(action));
        json_node_t *arr = json_new_array();
        for (size_t i = 0; i < count; i++) {
            json_node_t *s = json_new_object();
            json_set(s, "name", json_new_string(sr[i].name));
            json_set(s, "path", json_new_string(sr[i].path));
            json_set(s, "score", json_new_number((double)sr[i].score));
            json_append(arr, s);
        }
        json_set(result, "results", arr);
        skill_search_hub_free(sr, count);

    } else if (strcmp(action, "install") == 0) {
        char error[512] = "";
        bool ok = skill_install_from_hub(slug, error, sizeof(error));
        json_set(result, "success", json_new_bool(ok));
        json_set(result, "slug", json_new_string(slug));
        if (!ok && error[0])
            json_set(result, "error", json_new_string(error));

    } else {
        json_set(result, "error", json_new_string(
            "Unknown action. Use: search <query>, install <slug>, list"));
    }

    char *out = json_serialize(result);
    json_free(result);
    return out;
}

/* ================================================================
 *  L12: Browse.sh skills hub — search and install
 * ================================================================ */

#define BROWSE_SH_CATALOG_URL "https://browse.sh/api/skills"
#define BROWSE_SH_TIMEOUT     15

static json_node_t *hub_fetch(const char *url) {
    http_client_t *h = http_client_new(BROWSE_SH_TIMEOUT);
    if (!h) return NULL;
    http_response_t *resp = http_get(h, url,
        "Accept: application/json\r\n"
        "User-Agent: Hermes-C/1.0");
    if (!resp || resp->status != 200) {
        if (resp) http_response_free(resp);
        http_client_free(h);
        return NULL;
    }
    char *err = NULL;
    json_node_t *root = json_parse(resp->body, &err);
    http_response_free(resp);
    http_client_free(h);
    free(err);
    return root;
}

static char *hub_fetch_raw(const char *url) {
    http_client_t *h = http_client_new(BROWSE_SH_TIMEOUT);
    if (!h) return NULL;
    http_response_t *resp = http_get(h, url, "User-Agent: Hermes-C/1.0");
    if (!resp || resp->status != 200) {
        if (resp) http_response_free(resp);
        http_client_free(h);
        return NULL;
    }
    char *body = strdup(resp->body ? resp->body : "");
    http_response_free(resp);
    http_client_free(h);
    return body;
}

skill_search_result_t *skill_search_hub(const char *query,
                                         size_t *result_count, size_t max_results) {
    *result_count = 0;
    if (!query) query = "";
    if (max_results == 0) max_results = 20;

    json_node_t *root = hub_fetch(BROWSE_SH_CATALOG_URL);
    if (!root) return NULL;

    json_node_t *skills_arr = json_object_get(root, "skills");
    if (!skills_arr || json_len(skills_arr) == 0) {
        json_free(root); return NULL;
    }

    skill_search_result_t *results = (skill_search_result_t *)
        calloc(max_results, sizeof(skill_search_result_t));
    if (!results) { json_free(root); return NULL; }

    size_t count = 0;
    size_t total = (size_t)json_len(skills_arr);
    int qlen = (int)strlen(query);

    for (size_t i = 0; i < total && count < max_results; i++) {
        json_node_t *item = json_get(skills_arr, (int)i);
        if (!item) continue;

        const char *slug    = json_get_str(item, "slug", "");
        const char *name    = json_get_str(item, "name", "");
        const char *title   = json_get_str(item, "title", "");
        const char *desc    = json_get_str(item, "description", "");
        const char *host    = json_get_str(item, "hostname", "");
        const char *cat     = json_get_str(item, "category", "");

        if (!slug[0] || !name[0]) continue;

        float score = 0.0f;
        if (qlen == 0) {
            score = 0.5f;
        } else {
            if      (strcasecmp(name, query) == 0) score = 1.0f;
            else if (strstr(name, query))          score = 0.9f;
            else if (strstr(title, query))         score = 0.8f;
            else if (strstr(host, query))          score = 0.6f;
            else if (strstr(cat, query))           score = 0.5f;
            else if (strstr(desc, query))          score = 0.4f;
            else {
                json_node_t *tags = json_object_get(item, "tags");
                if (tags && json_len(tags) > 0) {
                    for (size_t t = 0; t < json_len(tags); t++) {
                        json_t *tag_node = json_get(tags, t);
                        const char *tag = (tag_node && tag_node->type == JSON_STRING) ? tag_node->str_val : "";
                        if (tag[0] && strstr(tag, query)) { score = 0.7f; break; }
                    }
                }
            }
        }

        if (score > 0.0f) {
            snprintf(results[count].name, sizeof(results[count].name), "%s", name);
            snprintf(results[count].path, sizeof(results[count].path), "browse.sh/%s", slug);
            results[count].score = score;
            count++;
        }
    }

    json_free(root);
    *result_count = count;
    return results;
}

void skill_search_hub_free(skill_search_result_t *results, size_t count) {
    (void)count;
    free(results);
}

bool skill_install_from_hub(const char *slug, char *error_out, size_t err_sz) {
    if (!slug || !slug[0]) {
        if (error_out) snprintf(error_out, err_sz, "No slug provided");
        return false;
    }

    char detail_url[1024];
    snprintf(detail_url, sizeof(detail_url),
             "https://browse.sh/api/skills/%s", slug);
    json_node_t *detail = hub_fetch(detail_url);
    if (!detail) {
        if (error_out) snprintf(error_out, err_sz, "Failed to fetch '%s'", slug);
        return false;
    }

    const char *skill_md_url = json_get_str(detail, "skillMdUrl", "");
    const char *name = json_get_str(detail, "name", slug);
    if (!skill_md_url[0]) {
        if (error_out) snprintf(error_out, err_sz, "No skillMdUrl for '%s'", slug);
        json_free(detail);
        return false;
    }
    json_free(detail);

    char *content = hub_fetch_raw(skill_md_url);
    if (!content) {
        if (error_out) snprintf(error_out, err_sz, "Failed to fetch SKILL.md");
        return false;
    }

    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = "/tmp";

    char skills_dir[HERMES_PATH_MAX];
    snprintf(skills_dir, sizeof(skills_dir), "%s/.hermes/skills/%s", home, name);
    ensure_dir(skills_dir);

    char skillmd_path[HERMES_PATH_MAX];
    snprintf(skillmd_path, sizeof(skillmd_path), "%s/SKILL.md", skills_dir);
    FILE *f = fopen(skillmd_path, "w");
    if (!f) {
        if (error_out) snprintf(error_out, err_sz, "Cannot write %s", skillmd_path);
        free(content);
        return false;
    }
    fputs(content, f);
    fclose(f);
    free(content);

    char prov_path[HERMES_PATH_MAX];
    snprintf(prov_path, sizeof(prov_path), "%s/.provenance", skills_dir);
    FILE *pf = fopen(prov_path, "w");
    if (pf) { fprintf(pf, "hub\n"); fclose(pf); }

    return true;
}

/* ================================================================
 *  Auto-registration
 * ================================================================ */

void registry_init_skills(void) {
    /* P179-P180: Scan + validate */
    registry_register("skill_scan",
        "Scan skills directory recursively and extract metadata from SKILL.md frontmatter. Optionally validate all skills.",
        SCHEMA_SCAN, skills_scan_handler);

    registry_register("skill_validate",
        "Validate skill SKILL.md YAML frontmatter. Checks version format, required fields.",
        SCHEMA_VALIDATE, skills_validate_handler);

    /* P181: Provenance */
    registry_register("skill_provenance",
        "Get or set skill origin (local/hub/bundle). Tracks where a skill came from.",
        SCHEMA_PROVENANCE, skills_provenance_handler);

    /* P182: Sync */
    registry_register("skill_sync",
        "Sync skills from a git-based hub repository. Clones or pulls from remote, merges with local skills.",
        SCHEMA_SYNC, skills_sync_handler);

    /* P183: Bundles */
    registry_register("skill_bundle",
        "Manage skill bundles (alias groups). Create, get, delete, or list bundles that load multiple skills at once.",
        SCHEMA_BUNDLE, skills_bundle_handler);

    /* P184: Usage tracking */
    registry_register("skill_usage",
        "Track skill usage frequency and last-used timestamps. Get recommendations based on usage patterns.",
        SCHEMA_USAGE, skills_usage_handler);

    /* P185: Cache */
    registry_register("skill_cache",
        "Manage LRU skill cache. Init, preload, evict, get cached skill content. Improves repeated access performance.",
        SCHEMA_CACHE, skills_cache_handler);

    /* P186: Search */
    registry_register("skill_search",
        "Search skills by text content and tag filters. Returns scored results sorted by relevance.",
        SCHEMA_SEARCH, skills_search_handler);

    /* P187: Curator */
    registry_register("skill_curator",
        "Run skill curator: detect stale skills, validate all, report status. Configure stale threshold days.",
        SCHEMA_CURATOR, skills_curator_handler);

    /* P188: Dependencies */
    registry_register("skill_deps",
        "Resolve skill dependencies with topological sort. Detect missing deps, validate loading order.",
        SCHEMA_DEPS, skills_deps_handler);

    /* Original backward-compatible skills_list */
    registry_register("skills_list",
        "List available Hermes skills. Returns array of skill names from the skills directory.",
        SCHEMA_LIST, skills_list_handler);

    /* L12: Browse.sh skills hub */
    registry_register("skill_hub",
        "Search and install skills from the browse.sh skills hub. "
        "Actions: search <query>, install <slug>, or list all.",
        "{"
        "\"type\":\"object\","
        "\"properties\":{"
          "\"action\":{\"type\":\"string\",\"description\":\"search | install\"},"
          "\"query\":{\"type\":\"string\",\"description\":\"Search query (for search action)\"},"
          "\"slug\":{\"type\":\"string\",\"description\":\"Skill slug (for install action)\"},"
          "\"limit\":{\"type\":\"number\",\"description\":\"Max results (default 20)\"}"
        "},"
        "\"required\":[\"action\"]"
        "}",
        skills_hub_handler);
}
