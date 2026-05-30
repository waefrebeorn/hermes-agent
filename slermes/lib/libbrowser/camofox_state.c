/*
 * camofox_state.c — Hermes-managed Camofox browser state helpers.
 *
 * Port of Python tools/browser_camofox_state.py.
 *
 * Uses libuuid for UUID5 generation and libpath for path construction.
 */

#include "camofox_state.h"
#include "uuid.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

const char *camofox_state_dir(const char *hermes_home, char out_path[CAMOFOX_PATH_MAX])
{
    if (!hermes_home || !out_path)
        return NULL;
    snprintf(out_path, CAMOFOX_PATH_MAX, "%s/%s/%s",
             hermes_home, CAMOFOX_STATE_DIR_NAME, CAMOFOX_STATE_SUBDIR);
    return out_path;
}

bool camofox_gen_identity(const char *hermes_home,
                           const char *task_id,
                           char out_user_id[CAMOFOX_USER_ID_MAX],
                           char out_session_key[CAMOFOX_SESSION_KEY_MAX])
{
    if (!hermes_home) {
        if (out_user_id) out_user_id[0] = '\0';
        if (out_session_key) out_session_key[0] = '\0';
        return false;
    }

    if (!task_id || !*task_id)
        task_id = "default";

    /* Build namespace and name strings */
    char scope_root[CAMOFOX_PATH_MAX];
    snprintf(scope_root, sizeof(scope_root), "%s/%s/%s",
             hermes_home, CAMOFOX_STATE_DIR_NAME, CAMOFOX_STATE_SUBDIR);

    /* Generate user_id: UUID5(camofox-user:<scope_root>), first 10 hex chars */
    char user_name[CAMOFOX_PATH_MAX + 32];
    snprintf(user_name, sizeof(user_name), "camofox-user:%s", scope_root);

    char *user_uuid_str = uuid_v5((const uint8_t *)UUID_NS_URL, user_name, strlen(user_name));
    if (!user_uuid_str) {
        if (out_user_id) out_user_id[0] = '\0';
        if (out_session_key) out_session_key[0] = '\0';
        return false;
    }

    /* Extract first 10 hex chars (skip "hermes_" prefix in output) */
    if (out_user_id) {
        /* Copy first 10 non-hyphen hex chars from the UUID */
        int dst = 0;
        out_user_id[0] = 'h'; out_user_id[1] = 'e'; out_user_id[2] = 'r';
        out_user_id[3] = 'm'; out_user_id[4] = 'e'; out_user_id[5] = 's';
        out_user_id[6] = '_';
        dst = 7;
        for (int src = 0; user_uuid_str[src] && dst < (int)CAMOFOX_USER_ID_MAX - 1; src++) {
            char c = user_uuid_str[src];
            if (c != '-') {
                out_user_id[dst++] = c;
                if (dst - 7 >= 10) break;  /* 10 hex chars after "hermes_" */
            }
        }
        out_user_id[dst] = '\0';
    }
    free(user_uuid_str);

    /* Generate session_key: UUID5(camofox-session:<scope_root>:<task_id>), first 16 hex chars */
    char session_name[CAMOFOX_PATH_MAX + 64];
    snprintf(session_name, sizeof(session_name), "camofox-session:%s:%s", scope_root, task_id);

    char *session_uuid_str = uuid_v5((const uint8_t *)UUID_NS_URL, session_name, strlen(session_name));
    if (!session_uuid_str) {
        if (out_session_key) out_session_key[0] = '\0';
        return false;
    }

    if (out_session_key) {
        int dst = 0;
        out_session_key[0] = 't'; out_session_key[1] = 'a';
        out_session_key[2] = 's'; out_session_key[3] = 'k';
        out_session_key[4] = '_';
        dst = 5;
        for (int src = 0; session_uuid_str[src] && dst < (int)CAMOFOX_SESSION_KEY_MAX - 1; src++) {
            char c = session_uuid_str[src];
            if (c != '-') {
                out_session_key[dst++] = c;
                if (dst - 5 >= 16) break;  /* 16 hex chars after "task_" */
            }
        }
        out_session_key[dst] = '\0';
    }
    free(session_uuid_str);

    return true;
}

/* ── Session Persistence ─────────────────────────────────────── */

static bool ensure_session_dir(const char *hermes_home) {
    char dir[CAMOFOX_PATH_MAX];
    const char *base = camofox_state_dir(hermes_home, dir);
    if (!base) return false;

    /* Create directory chain: every component including the final one */
    char *p = dir;
    if (*p == '/') p++;
    while (*p) {
        while (*p && *p != '/') p++;
        if (!*p) break;
        char saved = *p;
        *p = '\0';
        mkdir(dir, 0755);
        *p = saved;
        p++;
    }
    /* Create final component (no trailing '/') */
    mkdir(dir, 0755);

    /* Now create the sessions subdir chain */
    size_t len = strlen(dir);
    snprintf(dir + len, CAMOFOX_PATH_MAX - len, "/sessions");
    p = dir;
    if (*p == '/') p++;
    while (*p) {
        while (*p && *p != '/') p++;
        if (!*p) break;
        char saved = *p;
        *p = '\0';
        mkdir(dir, 0755);
        *p = saved;
        p++;
    }
    /* Create final sessions dir */
    mkdir(dir, 0755);
    return true;
}

static char *session_file_path(const char *hermes_home,
                                const char *task_id,
                                char out[CAMOFOX_PATH_MAX]) {
    const char *base = camofox_state_dir(hermes_home, out);
    if (!base) return NULL;
    size_t len = strlen(out);
    snprintf(out + len, CAMOFOX_PATH_MAX - len, "/sessions/%s.json", task_id);
    return out;
}

bool camofox_save_session(const char *hermes_home,
                           const char *task_id,
                           const char *cdp_url) {
    if (!hermes_home || !task_id || !cdp_url) return false;
    if (!ensure_session_dir(hermes_home)) return false;

    char path[CAMOFOX_PATH_MAX];
    if (!session_file_path(hermes_home, task_id, path)) return false;

    char user_id[CAMOFOX_USER_ID_MAX] = "";
    char session_key[CAMOFOX_SESSION_KEY_MAX] = "";
    camofox_gen_identity(hermes_home, task_id, user_id, session_key);

    /* Build JSON: {"cdp_url":"...","user_id":"...","session_key":"...","task_id":"..."} */
    char json[4096];
    int n = snprintf(json, sizeof(json),
        "{\"cdp_url\":\"%s\",\"user_id\":\"%s\","
        "\"session_key\":\"%s\",\"task_id\":\"%s\","
        "\"version\":1}",
        cdp_url, user_id, session_key, task_id);
    if (n < 0 || (size_t)n >= sizeof(json)) return false;

    FILE *f = fopen(path, "w");
    if (!f) return false;
    size_t written = fwrite(json, 1, (size_t)n, f);
    fclose(f);
    return written == (size_t)n;
}

bool camofox_load_session(const char *hermes_home,
                           const char *task_id,
                           char out_cdp_url[CAMOFOX_PATH_MAX]) {
    if (!hermes_home || !task_id || !out_cdp_url) {
        if (out_cdp_url) out_cdp_url[0] = '\0';
        return false;
    }

    char path[CAMOFOX_PATH_MAX];
    if (!session_file_path(hermes_home, task_id, path)) return false;

    FILE *f = fopen(path, "r");
    if (!f) { out_cdp_url[0] = '\0'; return false; }

    char buf[8192];
    size_t nr = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[nr] = '\0';

    /* Simple JSON key-value extract — find "cdp_url":"..." */
    const char *key = strstr(buf, "\"cdp_url\":\"");
    if (!key) { out_cdp_url[0] = '\0'; return false; }

    key += 11; /* skip past "cdp_url":" */
    int dst = 0;
    while (*key && *key != '"' && dst < CAMOFOX_PATH_MAX - 1) {
        if (*key == '\\' && *(key+1) == '"') { key++; }  /* skip escaped quote */
        out_cdp_url[dst++] = *key++;
    }
    out_cdp_url[dst] = '\0';
    return dst > 0;
}

bool camofox_delete_session(const char *hermes_home,
                             const char *task_id) {
    if (!hermes_home || !task_id) return false;
    char path[CAMOFOX_PATH_MAX];
    if (!session_file_path(hermes_home, task_id, path)) return false;
    return unlink(path) == 0 || errno == ENOENT;
}
