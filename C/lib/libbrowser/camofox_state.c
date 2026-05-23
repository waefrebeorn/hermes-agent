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
