/*
 * skill_usage.c — Skill usage telemetry + provenance tracking for Hermes C.
 * Port of Python tools/skill_usage.py.
 *
 * Tracks per-skill usage metadata in a sidecar JSON file
 * (~/.hermes/skills/.usage.json) keyed by skill name.
 * All counter bumps are best-effort: errors are returned but never
 * abort the caller.
 */

#include "skill_usage.h"
#include "json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

/* ================================================================
 *  Helpers
 * ================================================================ */

void skill_usage_now_iso(char *out_buf)
{
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    strftime(out_buf, 32, "%Y-%m-%dT%H:%M:%S", &tm);
}

const char *skill_usage_file_path(const char *hermes_home, char *out_path)
{
    snprintf(out_path, SKILL_USAGE_MAX_PATH, "%s/skills/.usage.json", hermes_home);
    return out_path;
}

const char *skill_usage_archive_dir(const char *hermes_home, char *out_path)
{
    snprintf(out_path, SKILL_USAGE_MAX_PATH, "%s/skills/.archive", hermes_home);
    return out_path;
}

/* Parse a JSON record into a skill_usage_record_t */
static void record_from_json(const json_t *j, skill_usage_record_t *r)
{
    const char *val;

    val = json_get_str(j, "created_by", NULL);
    if (val) strncpy(r->created_by, val, sizeof(r->created_by) - 1);

    r->use_count   = (int)json_get_num(j, "use_count", 0);
    r->view_count  = (int)json_get_num(j, "view_count", 0);
    r->patch_count = (int)json_get_num(j, "patch_count", 0);

    val = json_get_str(j, "last_used_at", NULL);
    if (val) strncpy(r->last_used_at, val, sizeof(r->last_used_at) - 1);

    val = json_get_str(j, "last_viewed_at", NULL);
    if (val) strncpy(r->last_viewed_at, val, sizeof(r->last_viewed_at) - 1);

    val = json_get_str(j, "last_patched_at", NULL);
    if (val) strncpy(r->last_patched_at, val, sizeof(r->last_patched_at) - 1);

    val = json_get_str(j, "created_at", NULL);
    if (val) strncpy(r->created_at, val, sizeof(r->created_at) - 1);

    val = json_get_str(j, "state", SKILL_USAGE_STATE_ACTIVE);
    strncpy(r->state, val, sizeof(r->state) - 1);

    r->pinned = json_get_bool(j, "pinned", false);

    val = json_get_str(j, "archived_at", NULL);
    if (val) strncpy(r->archived_at, val, sizeof(r->archived_at) - 1);
}

/* Serialize a skill_usage_record_t to a JSON object */
static json_t *record_to_json(const skill_usage_record_t *r)
{
    json_t *j = json_object();
    if (!j) return NULL;

    if (r->created_by[0])
        json_set(j, "created_by", json_string(r->created_by));
    json_set(j, "use_count", json_number(r->use_count));
    json_set(j, "view_count", json_number(r->view_count));
    json_set(j, "patch_count", json_number(r->patch_count));

    if (r->last_used_at[0])
        json_set(j, "last_used_at", json_string(r->last_used_at));
    if (r->last_viewed_at[0])
        json_set(j, "last_viewed_at", json_string(r->last_viewed_at));
    if (r->last_patched_at[0])
        json_set(j, "last_patched_at", json_string(r->last_patched_at));
    if (r->created_at[0])
        json_set(j, "created_at", json_string(r->created_at));

    json_set(j, "state", json_string(r->state));
    json_set(j, "pinned", json_bool(r->pinned));

    if (r->archived_at[0])
        json_set(j, "archived_at", json_string(r->archived_at));

    return j;
}

/* ================================================================
 *  I/O
 * ================================================================ */

void skill_usage_load(const char *hermes_home, skill_usage_map_t *out_map)
{
    memset(out_map, 0, sizeof(*out_map));
    char path[SKILL_USAGE_MAX_PATH];
    skill_usage_file_path(hermes_home, path);

    struct stat st;
    if (stat(path, &st) != 0)
        return; /* file doesn't exist — empty map */

    char *err = NULL;
    json_t *root = json_parse_file(path, &err);
    if (!root) {
        /* Corrupt or unreadable — return empty map */
        if (err) free(err);
        return;
    }

    /* Read each key-value pair as a record */
    /* json_parse_file returns a JSON object: { "skill_name": { ... }, ... } */
    /* We iterate by serializing, parsing keys... */
    /* Since our json lib doesn't have iteration, we serialize and re-parse */
    /* Actually, let's use json_get_str with empty defaults — need iteration */
    /* Alternative: serialize to string, parse keys with simple regex */

    /* Simple approach: json lib has json_get() for array access but not
     * object iteration. We'll serialize and parse key names manually. */
    char *text = json_serialize(root);
    if (!text) {
        json_free(root);
        return;
    }

    /* Parse records by scanning the JSON object keys */
    /* Format: {"name":{...}, "name2":{...}} */
    /* Scan for quoted keys: "skill_name": */
    const char *p = text;
    while (*p && out_map->count < SKILL_USAGE_MAX_SKILLS) {
        /* Find next key start */
        while (*p && *p != '"') p++;
        if (!*p) break;
        p++; /* skip opening quote */

        /* Read key (skill name) */
        char key[SKILL_USAGE_MAX_NAME];
        int ki = 0;
        while (*p && *p != '"' && ki < (int)sizeof(key) - 1)
            key[ki++] = *p++;
        key[ki] = '\0';
        if (!*p) break;
        p++; /* skip closing quote */

        /* Expect ':' then '{' for the value */
        while (*p && *p == ':') p++;
        while (*p && *p == ' ') p++;
        if (*p != '{') break;

        /* Find matching closing brace */
        int depth = 0;
        const char *val_start = p;
        const char *val_end = p;
        while (*p) {
            if (*p == '{') depth++;
            if (*p == '}') {
                depth--;
                if (depth == 0) { val_end = p + 1; break; }
            }
            p++;
        }

        if (val_end > val_start) {
            /* Parse the value as JSON */
            size_t vlen = (size_t)(val_end - val_start);
            char *vstr = malloc(vlen + 1);
            if (vstr) {
                memcpy(vstr, val_start, vlen);
                vstr[vlen] = '\0';
                char *verr = NULL;
                json_t *vj = json_parse(vstr, &verr);
                if (vj) {
                    skill_usage_record_t *r = &out_map->records[out_map->count];
                    strncpy(r->name, key, sizeof(r->name) - 1);
                    record_from_json(vj, r);
                    out_map->count++;
                    json_free(vj);
                }
                free(vstr);
                if (verr) free(verr);
            }
        }

        p = val_end;
    }

    free(text);
    json_free(root);
}

int skill_usage_save(const char *hermes_home, const skill_usage_map_t *map)
{
    /* Build JSON object */
    json_t *root = json_object();
    if (!root) return -1;

    for (int i = 0; i < map->count; i++) {
        json_t *jrec = record_to_json(&map->records[i]);
        if (jrec) {
            json_set(root, map->records[i].name, jrec);
        }
    }

    char *text = json_serialize_pretty(root, 2);
    json_free(root);
    if (!text) return -1;

    /* Atomic write: mkstemp -> write -> rename */
    char path[SKILL_USAGE_MAX_PATH];
    skill_usage_file_path(hermes_home, path);

    /* Ensure parent dirs exist */
    mkdir(hermes_home, 0755);
    char dir[SKILL_USAGE_MAX_PATH];
    snprintf(dir, sizeof(dir), "%s/skills", hermes_home);
    mkdir(dir, 0755);

    char tmp_path[SKILL_USAGE_MAX_PATH + 32];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmpXXXXXX", path);

    int fd = mkstemp(tmp_path);
    if (fd < 0) {
        free(text);
        return -1;
    }

    size_t len = strlen(text);
    ssize_t written = write(fd, text, len);
    (void)written;
    fsync(fd);
    close(fd);
    free(text);

    if (rename(tmp_path, path) != 0) {
        unlink(tmp_path);
        return -1;
    }

    return 0;
}

/* ================================================================
 *  Record operations
 * ================================================================ */

int skill_usage_find(const skill_usage_map_t *map, const char *skill_name)
{
    if (!skill_name || !*skill_name || !map) return -1;
    for (int i = 0; i < map->count; i++) {
        if (strcmp(map->records[i].name, skill_name) == 0)
            return i;
    }
    return -1;
}

void skill_usage_get_record(const skill_usage_map_t *map,
                             const char *skill_name,
                             skill_usage_record_t *out_record)
{
    int idx = skill_usage_find(map, skill_name);
    if (idx >= 0) {
        *out_record = map->records[idx];
    } else {
        /* Return empty default with name set */
        memset(out_record, 0, sizeof(*out_record));
        if (skill_name)
            strncpy(out_record->name, skill_name, sizeof(out_record->name) - 1);
        strncpy(out_record->state, SKILL_USAGE_STATE_ACTIVE, sizeof(out_record->state) - 1);
        skill_usage_now_iso(out_record->created_at);
    }
}

/* Internal: load map, find/create record, mutate it, save.
 * Returns the index of the record in the map (for further mutations)
 * or -1 on error. */
static int _mutate_load(const char *hermes_home,
                         skill_usage_map_t *map,
                         const char *skill_name)
{
    if (!skill_name || !*skill_name) return -1;

    skill_usage_load(hermes_home, map);

    int idx = skill_usage_find(map, skill_name);
    if (idx < 0) {
        /* Create new record */
        if (map->count >= SKILL_USAGE_MAX_SKILLS) return -1;
        idx = map->count;
        skill_usage_record_t *r = &map->records[idx];
        memset(r, 0, sizeof(*r));
        strncpy(r->name, skill_name, sizeof(r->name) - 1);
        strncpy(r->state, SKILL_USAGE_STATE_ACTIVE, sizeof(r->state) - 1);
        skill_usage_now_iso(r->created_at);
        map->count++;
    }

    return idx;
}

static int _mutate_save(const char *hermes_home,
                         skill_usage_map_t *map,
                         int idx)
{
    (void)idx; /* if save fails, the caller still returns error */
    return skill_usage_save(hermes_home, map);
}

/* ================================================================
 *  Counter bumps
 * ================================================================ */

int skill_usage_bump_view(const char *hermes_home, const char *skill_name)
{
    skill_usage_map_t map;
    int idx = _mutate_load(hermes_home, &map, skill_name);
    if (idx < 0) return -1;

    skill_usage_record_t *r = &map.records[idx];
    r->view_count++;
    skill_usage_now_iso(r->last_viewed_at);

    return _mutate_save(hermes_home, &map, idx);
}

int skill_usage_bump_use(const char *hermes_home, const char *skill_name)
{
    skill_usage_map_t map;
    int idx = _mutate_load(hermes_home, &map, skill_name);
    if (idx < 0) return -1;

    skill_usage_record_t *r = &map.records[idx];
    r->use_count++;
    skill_usage_now_iso(r->last_used_at);

    return _mutate_save(hermes_home, &map, idx);
}

int skill_usage_bump_patch(const char *hermes_home, const char *skill_name)
{
    skill_usage_map_t map;
    int idx = _mutate_load(hermes_home, &map, skill_name);
    if (idx < 0) return -1;

    skill_usage_record_t *r = &map.records[idx];
    r->patch_count++;
    skill_usage_now_iso(r->last_patched_at);

    return _mutate_save(hermes_home, &map, idx);
}

/* ================================================================
 *  Provenance & lifecycle
 * ================================================================ */

int skill_usage_mark_agent_created(const char *hermes_home, const char *skill_name)
{
    skill_usage_map_t map;
    int idx = _mutate_load(hermes_home, &map, skill_name);
    if (idx < 0) return -1;

    skill_usage_record_t *r = &map.records[idx];
    strncpy(r->created_by, "agent", sizeof(r->created_by) - 1);

    return _mutate_save(hermes_home, &map, idx);
}

int skill_usage_set_state(const char *hermes_home, const char *skill_name,
                           const char *state)
{
    if (!state) return -1;
    if (strcmp(state, SKILL_USAGE_STATE_ACTIVE) != 0 &&
        strcmp(state, SKILL_USAGE_STATE_STALE) != 0 &&
        strcmp(state, SKILL_USAGE_STATE_ARCHIVED) != 0)
        return -1; /* invalid state */

    skill_usage_map_t map;
    int idx = _mutate_load(hermes_home, &map, skill_name);
    if (idx < 0) return -1;

    skill_usage_record_t *r = &map.records[idx];
    strncpy(r->state, state, sizeof(r->state) - 1);

    if (strcmp(state, SKILL_USAGE_STATE_ARCHIVED) == 0) {
        skill_usage_now_iso(r->archived_at);
    } else if (strcmp(state, SKILL_USAGE_STATE_ACTIVE) == 0) {
        r->archived_at[0] = '\0';
    }

    return _mutate_save(hermes_home, &map, idx);
}

int skill_usage_set_pinned(const char *hermes_home, const char *skill_name,
                            bool pinned)
{
    skill_usage_map_t map;
    int idx = _mutate_load(hermes_home, &map, skill_name);
    if (idx < 0) return -1;

    map.records[idx].pinned = pinned;

    return _mutate_save(hermes_home, &map, idx);
}

int skill_usage_forget(const char *hermes_home, const char *skill_name)
{
    if (!skill_name || !*skill_name) return -1;

    skill_usage_map_t map;
    skill_usage_load(hermes_home, &map);

    int idx = skill_usage_find(&map, skill_name);
    if (idx < 0) return 0; /* nothing to forget */

    /* Shift remaining records left */
    for (int i = idx; i < map.count - 1; i++) {
        map.records[i] = map.records[i + 1];
    }
    map.count--;

    return skill_usage_save(hermes_home, &map);
}

/* ================================================================
 *  Archive / restore
 * ================================================================ */

int skill_usage_archive(const char *hermes_home, const char *skill_name,
                         char *out_msg)
{
    out_msg[0] = '\0';

    /* Find the skill directory */
    char skills_dir[SKILL_USAGE_MAX_PATH];
    snprintf(skills_dir, sizeof(skills_dir), "%s/skills", hermes_home);

    char src[SKILL_USAGE_MAX_PATH];
    snprintf(src, sizeof(src), "%s/%s", skills_dir, skill_name);

    struct stat st;
    if (stat(src, &st) != 0 || !S_ISDIR(st.st_mode)) {
        snprintf(out_msg, SKILL_USAGE_MAX_VALUE,
                 "skill '%s' not found", skill_name);
        return -1;
    }

    /* Ensure archive dir exists */
    char archive_dir[SKILL_USAGE_MAX_PATH];
    skill_usage_archive_dir(hermes_home, archive_dir);
    mkdir(archive_dir, 0755);

    char dest[SKILL_USAGE_MAX_PATH];
    snprintf(dest, sizeof(dest), "%s/%s", archive_dir, skill_name);

    /* If dest exists, append timestamp */
    if (stat(dest, &st) == 0) {
        char ts[32];
        skill_usage_now_iso(ts);
        /* Replace colons with dashes for filesystem safety */
        for (char *c = ts; *c; c++) if (*c == ':') *c = '-';
        snprintf(dest, sizeof(dest), "%s/%s-%s", archive_dir, skill_name, ts);
    }

    if (rename(src, dest) != 0) {
        snprintf(out_msg, SKILL_USAGE_MAX_VALUE,
                 "failed to archive: %s", strerror(errno));
        return -1;
    }

    skill_usage_set_state(hermes_home, skill_name, SKILL_USAGE_STATE_ARCHIVED);

    snprintf(out_msg, SKILL_USAGE_MAX_VALUE, "archived to %s", dest);
    return 0;
}

int skill_usage_restore(const char *hermes_home, const char *skill_name,
                          char *out_msg)
{
    out_msg[0] = '\0';

    char archive_dir[SKILL_USAGE_MAX_PATH];
    skill_usage_archive_dir(hermes_home, archive_dir);

    char src[SKILL_USAGE_MAX_PATH];
    snprintf(src, sizeof(src), "%s/%s", archive_dir, skill_name);

    struct stat st;
    if (stat(src, &st) != 0 || !S_ISDIR(st.st_mode)) {
        snprintf(out_msg, SKILL_USAGE_MAX_VALUE,
                 "skill '%s' not found in archive", skill_name);
        return -1;
    }

    char skills_dir[SKILL_USAGE_MAX_PATH];
    snprintf(skills_dir, sizeof(skills_dir), "%s/skills", hermes_home);

    char dest[SKILL_USAGE_MAX_PATH];
    snprintf(dest, sizeof(dest), "%s/%s", skills_dir, skill_name);

    if (stat(dest, &st) == 0) {
        snprintf(out_msg, SKILL_USAGE_MAX_VALUE,
                 "destination already exists: %s", dest);
        return -1;
    }

    if (rename(src, dest) != 0) {
        snprintf(out_msg, SKILL_USAGE_MAX_VALUE,
                 "failed to restore: %s", strerror(errno));
        return -1;
    }

    skill_usage_set_state(hermes_home, skill_name, SKILL_USAGE_STATE_ACTIVE);

    snprintf(out_msg, SKILL_USAGE_MAX_VALUE, "restored to %s", dest);
    return 0;
}

/* ================================================================
 *  Derived fields
 * ================================================================ */

const char *skill_usage_latest_activity(const skill_usage_record_t *record,
                                         char *out_buf)
{
    out_buf[0] = '\0';

    /* Compare timestamps lexicographically (ISO-8601 sorts by time) */
    const char *latest = NULL;

    if (record->last_used_at[0])
        latest = record->last_used_at;
    if (record->last_viewed_at[0] &&
        (!latest || strcmp(record->last_viewed_at, latest) > 0))
        latest = record->last_viewed_at;
    if (record->last_patched_at[0] &&
        (!latest || strcmp(record->last_patched_at, latest) > 0))
        latest = record->last_patched_at;

    if (latest) {
        strncpy(out_buf, latest, SKILL_USAGE_MAX_VALUE - 1);
        return out_buf;
    }

    return NULL;
}

int skill_usage_activity_count(const skill_usage_record_t *record)
{
    return record->use_count + record->view_count + record->patch_count;
}
