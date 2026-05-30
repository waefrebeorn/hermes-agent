/*
 * dotenv.c — .env file parser for C.
 * Parses KEY=VALUE lines, handles quotes, comments, exports.
 * MIT License — WuBu Hermes Project
 */

#include "dotenv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ================================================================
 *  Internal store
 * ================================================================ */

typedef struct {
    char *key;
    char *value;
} env_entry_t;

struct env_t {
    env_entry_t *entries;
    size_t count;
    size_t capacity;
};

static env_t *env_create(void) {
    env_t *env = (env_t *)calloc(1, sizeof(env_t));
    if (!env) return NULL;
    env->capacity = 16;
    env->entries = (env_entry_t *)calloc(env->capacity, sizeof(env_entry_t));
    if (!env->entries) { free(env); return NULL; }
    return env;
}

static int env_find(const env_t *env, const char *key) {
    for (size_t i = 0; i < env->count; i++)
        if (strcmp(env->entries[i].key, key) == 0) return (int)i;
    return -1;
}

/* ================================================================
 *  Parsing
 * ================================================================ */

/* Parse a single line: KEY=VALUE or KEY=  (export KEY=VALUE also supported) */
static bool parse_line(const char *line, char **key, char **value) {
    *key = NULL;
    *value = NULL;

    const char *p = line;
    /* Skip leading whitespace */
    while (*p == ' ' || *p == '\t') p++;

    /* Skip 'export ' prefix */
    if (strncmp(p, "export", 6) == 0 && (p[6] == ' ' || p[6] == '\t')) {
        p += 7;
        while (*p == ' ' || *p == '\t') p++;
    }

    /* Skip comments and empty lines */
    if (*p == '#' || *p == '\0') return true; /* silent skip */

    /* Find '=' */
    const char *eq = strchr(p, '=');
    if (!eq) return true; /* no =, skip */

    /* Key: trim trailing spaces */
    size_t key_len = (size_t)(eq - p);
    while (key_len > 0 && (p[key_len-1] == ' ' || p[key_len-1] == '\t')) key_len--;
    if (key_len == 0) return true;

    *key = strndup(p, key_len);
    if (!*key) return false;

    /* Value: everything after = */
    const char *vstart = eq + 1;
    while (*vstart == ' ' || *vstart == '\t') vstart++;

    /* Handle quotes */
    if (*vstart == '"' || *vstart == '\'') {
        char quote = *vstart;
        vstart++;
        const char *vend = strchr(vstart, quote);
        if (vend) {
            *value = strndup(vstart, (size_t)(vend - vstart));
        } else {
            /* Unterminated quote — take rest of line */
            *value = strdup(vstart);
        }
    } else {
        /* Unquoted: strip trailing whitespace and inline comments */
        const char *vend = vstart + strlen(vstart);
        /* Find comment marker (#) not preceded by backslash */
        for (const char *q = vstart; q < vend; q++) {
            if (*q == '#' && (q == vstart || *(q-1) != '\\')) {
                vend = q;
                break;
            }
        }
        /* Trim trailing whitespace */
        while (vend > vstart && (vend[-1] == ' ' || vend[-1] == '\t')) vend--;
        *value = strndup(vstart, (size_t)(vend - vstart));
    }

    return *value != NULL;
}

/* ================================================================
 *  Public API
 * ================================================================ */

env_t *dotenv_parse(const char *input, char **error_msg) {
    if (!input) {
        if (error_msg) *error_msg = strdup("NULL input");
        return NULL;
    }

    env_t *env = env_create();
    if (!env) { if (error_msg) *error_msg = strdup("OOM"); return NULL; }

    const char *p = input;
    while (*p) {
        /* Find end of line */
        const char *nl = strchr(p, '\n');
        size_t line_len = nl ? (size_t)(nl - p) : strlen(p);

        if (line_len > 0) {
            char *line = strndup(p, line_len);
            if (line) {
                char *key, *value;
                if (parse_line(line, &key, &value)) {
                    if (key && value) {
                        dotenv_set(env, key, value);
                        free(key);
                        free(value);
                    } else {
                        free(key);
                    }
                }
                free(line);
            }
        }

        if (nl) p = nl + 1;
        else break;
    }

    return env;
}

env_t *dotenv_load(const char *path, char **error_msg) {
    if (!path) {
        if (error_msg) *error_msg = strdup("NULL path");
        return NULL;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        if (error_msg) {
            char buf[256];
            snprintf(buf, sizeof(buf), "cannot open '%s'", path);
            *error_msg = strdup(buf);
        }
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return NULL; }

    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); if (error_msg) *error_msg = strdup("OOM"); return NULL; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    (void)n;
    fclose(f);
    buf[sz] = '\0';

    env_t *env = dotenv_parse(buf, error_msg);
    free(buf);
    return env;
}

const char *dotenv_get(const env_t *env, const char *key) {
    if (!env || !key) return NULL;
    int idx = env_find(env, key);
    return idx >= 0 ? env->entries[idx].value : NULL;
}

bool dotenv_set(env_t *env, const char *key, const char *value) {
    if (!env || !key || !value) return false;

    int idx = env_find(env, key);
    if (idx >= 0) {
        /* Update existing */
        free(env->entries[idx].value);
        env->entries[idx].value = strdup(value);
        return env->entries[idx].value != NULL;
    }

    /* Add new */
    if (env->count >= env->capacity) {
        env->capacity *= 2;
        env_entry_t *nb = (env_entry_t *)realloc(env->entries,
            env->capacity * sizeof(env_entry_t));
        if (!nb) return false;
        env->entries = nb;
    }

    env->entries[env->count].key = strdup(key);
    env->entries[env->count].value = strdup(value);
    if (!env->entries[env->count].key || !env->entries[env->count].value) {
        free(env->entries[env->count].key);
        free(env->entries[env->count].value);
        return false;
    }
    env->count++;
    return true;
}

bool dotenv_export(env_t *env) {
    if (!env) return false;
    for (size_t i = 0; i < env->count; i++)
        setenv(env->entries[i].key, env->entries[i].value, 1);
    return true;
}

bool dotenv_iter(const env_t *env, size_t *idx, const char **key, const char **value) {
    if (!env || !idx || !key || !value) return false;
    if (*idx >= env->count) return false;
    *key = env->entries[*idx].key;
    *value = env->entries[*idx].value;
    (*idx)++;
    return true;
}

size_t dotenv_count(const env_t *env) {
    return env ? env->count : 0;
}

void dotenv_free(env_t *env) {
    if (!env) return;
    for (size_t i = 0; i < env->count; i++) {
        free(env->entries[i].key);
        free(env->entries[i].value);
    }
    free(env->entries);
    free(env);
}
