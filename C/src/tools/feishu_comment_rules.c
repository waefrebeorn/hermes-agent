/*
 * feishu_comment_rules.c — G04: Feishu comment access-control rules.
 *
 * 3-tier rule resolution: exact doc > wildcard "*" > top-level > code defaults.
 * Config: HERMES_HOME/feishu_comment_rules.json (mtime-cached, hot-reload).
 * Pairing store: HERMES_HOME/feishu_comment_pairing.json.
 *
 * Port of Python gateway/platforms/feishu_comment_rules.py (429 LOC).
 */

#include "hermes.h"
#include "hermes_feishu_rules.h"
#include "hermes_json.h"
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>

/* ================================================================
 *  Internal helpers
 * ================================================================ */

/** Get HERMES_HOME directory path. */
static const char *rules_hermes_home(void) {
    static char buf[1024];
    const char *home = getenv("HERMES_HOME");
    if (home && home[0]) {
        snprintf(buf, sizeof(buf), "%s", home);
        return buf;
    }
    home = getenv("HOME");
    if (home && home[0]) {
        snprintf(buf, sizeof(buf), "%s/.hermes", home);
        return buf;
    }
    return "/tmp/.hermes";
}

/* ── Paths ─────────────────────────────────────────────── */

#define RULES_FILENAME   "feishu_comment_rules.json"
#define PAIRING_FILENAME "feishu_comment_pairing.json"

static void rules_file_path(char *buf, size_t bufsz, const char *filename) {
    snprintf(buf, bufsz, "%s/%s", rules_hermes_home(), filename);
}

/* ── Mtime-cached file loading ─────────────────────────── */

typedef struct {
    char   path[1024];
    double mtime;
    json_t *data;       /* parsed JSON, or NULL */
} mtime_cache_t;

static mtime_cache_t g_rules_cache   = { .path = "", .mtime = 0, .data = NULL };
static mtime_cache_t g_pairing_cache = { .path = "", .mtime = 0, .data = NULL };

/** Load a JSON file with mtime caching. Returns borrowed pointer (do not free). */
static json_t *mtime_cache_load(mtime_cache_t *cache, const char *filename) {
    char path[1024];
    rules_file_path(path, sizeof(path), filename);

    struct stat st;
    if (stat(path, &st) != 0) {
        /* File not found — use empty config */
        if (cache->data) { json_free(cache->data); cache->data = NULL; }
        cache->mtime = 0;
        cache->path[0] = '\0';
        return NULL;
    }

    double mtime = (double)st.st_mtime;
    if (mtime == cache->mtime && cache->data && strcmp(cache->path, path) == 0)
        return cache->data;   /* cache hit: same path, same mtime */

    /* Re-read */
    if (cache->data) { json_free(cache->data); cache->data = NULL; }
    snprintf(cache->path, sizeof(cache->path), "%s", path);
    cache->mtime = mtime;
    cache->data = json_parse_file(path, NULL);

    /* If parse fails or isn't an object, treat as empty */
    if (cache->data && cache->data->type != JSON_OBJECT) {
        json_free(cache->data);
        cache->data = NULL;
    }

    return cache->data;
}

/* ── String array helpers ──────────────────────────────── */

/** Parse a JSON array of strings into a malloc'd NULL-terminated array.
 *  Returns NULL if no items or if input is not an array. */
static char **json_array_to_strlist(const json_t *arr, size_t *out_count) {
    if (!arr || arr->type != JSON_ARRAY) {
        *out_count = 0;
        return NULL;
    }
    size_t n = json_len(arr);
    if (n == 0) { *out_count = 0; return NULL; }

    char **list = calloc(n + 1, sizeof(char *));
    if (!list) { *out_count = 0; return NULL; }

    size_t count = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *item = json_get(arr, i);
        if (item && item->type == JSON_STRING && item->str_val && item->str_val[0]) {
            list[count] = strdup(item->str_val);
            if (list[count]) count++;
        }
    }
    list[count] = NULL;
    *out_count = count;
    return list;
}

/** Free a NULL-terminated string array. */
static void strlist_free(char **list) {
    if (!list) return;
    for (size_t i = 0; list[i]; i++) free(list[i]);
    free(list);
}

/* ================================================================
 *  _parse_frozenset → json_array_to_strlist (path-dependent helper)
 * ================================================================ */

static char **parse_allow_from(const json_t *obj, const char *key, size_t *out_count) {
    json_t *val = json_obj_get(obj, key);
    return json_array_to_strlist(val, out_count);
}

/* ================================================================
 *  _parse_document_rule → feishu_rules_parse_doc_rule
 * ================================================================ */

static feishu_doc_rule_t parse_doc_rule(const json_t *raw) {
    feishu_doc_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    rule.enabled = -1;  /* inherit by default */
    rule.policy = NULL;
    rule.allow_from = NULL;
    rule.allow_from_count = 0;

    if (!raw || raw->type != JSON_OBJECT)
        return rule;

    /* enabled */
    json_t *en = json_obj_get(raw, "enabled");
    if (en && en->type == JSON_BOOL)
        rule.enabled = en->bool_val ? 1 : 0;

    /* policy */
    const char *pol = json_get_str(raw, "policy", NULL);
    if (pol) {
        /* validate against _VALID_POLICIES */
        size_t len = strlen(pol);
        /* Quick check: "allowlist" or "pairing" */
        if ((len == 9 && strcmp(pol, "allowlist") == 0) ||
            (len == 7 && strcmp(pol, "pairing") == 0)) {
            rule.policy = strdup(pol);
        }
    }

    /* allow_from */
    rule.allow_from = parse_allow_from(raw, "allow_from", &rule.allow_from_count);

    return rule;
}

/** Deep-free a doc rule (does NOT free the struct itself). */
static void doc_rule_free(feishu_doc_rule_t *r) {
    if (r->policy) free(r->policy);
    strlist_free(r->allow_from);
}

/* ================================================================
 *  load_config
 * ================================================================ */

feishu_rules_config_t *feishu_rules_load_config(void) {
    json_t *raw = mtime_cache_load(&g_rules_cache, RULES_FILENAME);
    feishu_rules_config_t *cfg = calloc(1, sizeof(feishu_rules_config_t));
    if (!cfg) return NULL;

    /* Defaults */
    cfg->enabled = true;
    cfg->policy = strdup("pairing");
    cfg->allow_from = NULL;
    cfg->allow_from_count = 0;
    cfg->documents = NULL;
    cfg->doc_count = 0;
    cfg->doc_keys = NULL;

    if (!raw) return cfg;  /* file not found or empty — use defaults */

    /* enabled */
    cfg->enabled = json_get_bool(raw, "enabled", true);

    /* policy */
    const char *pol = json_get_str(raw, "policy", "pairing");
    size_t plen = strlen(pol);
    if ((plen == 9 && strcmp(pol, "allowlist") == 0) ||
        (plen == 7 && strcmp(pol, "pairing") == 0)) {
        free(cfg->policy);
        cfg->policy = strdup(pol);
    }

    /* allow_from (top-level) */
    cfg->allow_from = parse_allow_from(raw, "allow_from", &cfg->allow_from_count);

    /* documents */
    json_t *docs = json_obj_get(raw, "documents");
    if (docs && docs->type == JSON_OBJECT) {
        size_t n = docs->c.count;
        if (n > 0) {
            cfg->documents = calloc(n, sizeof(feishu_doc_rule_t));
            cfg->doc_keys  = calloc(n + 1, sizeof(char *));
            if (cfg->documents && cfg->doc_keys) {
                size_t idx = 0;
                for (size_t i = 0; i < n && idx < n; i++) {
                    const char *k = docs->c.keys[i];
                    json_t *v = docs->c.items[i];
                    if (k && v) {
                        cfg->doc_keys[idx] = strdup(k);
                        cfg->documents[idx] = parse_doc_rule(v);
                        idx++;
                    }
                }
                cfg->doc_count = idx;
                cfg->doc_keys[idx] = NULL;
            }
        }
    }

    return cfg;
}

void feishu_rules_free_config(feishu_rules_config_t *cfg) {
    if (!cfg) return;
    if (cfg->policy) free(cfg->policy);
    strlist_free(cfg->allow_from);
    for (size_t i = 0; i < cfg->doc_count; i++) {
        doc_rule_free(&cfg->documents[i]);
        if (cfg->doc_keys && cfg->doc_keys[i]) free(cfg->doc_keys[i]);
    }
    free(cfg->documents);
    free(cfg->doc_keys);
    free(cfg);
}

/* ================================================================
 *  has_wiki_keys
 * ================================================================ */

bool feishu_rules_has_wiki_keys(const feishu_rules_config_t *cfg) {
    if (!cfg) return false;
    for (size_t i = 0; i < cfg->doc_count; i++) {
        if (cfg->doc_keys && cfg->doc_keys[i] &&
            strncmp(cfg->doc_keys[i], "wiki:", 5) == 0)
            return true;
    }
    return false;
}

/* ================================================================
 *  resolve_rule — field-by-field fallback
 * ================================================================ */

feishu_resolved_rule_t *feishu_rules_resolve_rule(
    const feishu_rules_config_t *cfg,
    const char *file_type,
    const char *file_token,
    const char *wiki_token)
{
    feishu_resolved_rule_t *rule = calloc(1, sizeof(feishu_resolved_rule_t));
    if (!rule) return NULL;

    /* Default values (top-level fallback) */
    rule->enabled = cfg->enabled;
    rule->policy = cfg->policy ? strdup(cfg->policy) : strdup("pairing");
    rule->allow_from = NULL;

    /* Build exact key: "file_type:file_token" */
    char exact_key[256];
    snprintf(exact_key, sizeof(exact_key), "%s:%s", file_type, file_token ? file_token : "");

    const feishu_doc_rule_t *exact = NULL;
    const char *exact_src = NULL;
    char exact_src_buf[512];

    /* Search for exact match */
    for (size_t i = 0; i < cfg->doc_count; i++) {
        if (cfg->doc_keys && cfg->doc_keys[i]) {
            if (strcmp(cfg->doc_keys[i], exact_key) == 0) {
                exact = &cfg->documents[i];
                snprintf(exact_src_buf, sizeof(exact_src_buf), "exact:%s", cfg->doc_keys[i]);
                exact_src = exact_src_buf;
                break;
            }
        }
    }

    /* If no exact match by file_type:file_token, try wiki_token */
    if (!exact && wiki_token && wiki_token[0]) {
        char wiki_key[256];
        snprintf(wiki_key, sizeof(wiki_key), "wiki:%s", wiki_token);
        for (size_t i = 0; i < cfg->doc_count; i++) {
            if (cfg->doc_keys && cfg->doc_keys[i] &&
                strcmp(cfg->doc_keys[i], wiki_key) == 0) {
                exact = &cfg->documents[i];
                snprintf(exact_src_buf, sizeof(exact_src_buf), "exact:%s", cfg->doc_keys[i]);
                exact_src = exact_src_buf;
                break;
            }
        }
    }

    /* Search for wildcard "*" */
    const feishu_doc_rule_t *wildcard = NULL;
    for (size_t i = 0; i < cfg->doc_count; i++) {
        if (cfg->doc_keys && cfg->doc_keys[i] &&
            strcmp(cfg->doc_keys[i], "*") == 0) {
            wildcard = &cfg->documents[i];
            break;
        }
    }

    /* Build layers: exact (if found), wildcard (if found) */
    /* Field-by-field resolution: exact > wildcard > top-level > default */

    struct layer_t {
        const feishu_doc_rule_t *rule;
        const char *source;
    } layers[2];
    int layer_count = 0;
    if (exact) {
        layers[layer_count].rule = exact;
        layers[layer_count].source = exact_src;
        layer_count++;
    }
    if (wildcard) {
        layers[layer_count].rule = wildcard;
        layers[layer_count].source = "wildcard";
        layer_count++;
    }

    const char *best_src = "default";

    /* enabled */
    for (int i = 0; i < layer_count; i++) {
        if (layers[i].rule->enabled >= 0) {
            rule->enabled = (layers[i].rule->enabled == 1);
            best_src = layers[i].source;
            break;
        }
    }
    if (layer_count == 0 || (exact && exact->enabled < 0 && wildcard && wildcard->enabled < 0)) {
        /* All layers have no value — use top-level */
        rule->enabled = cfg->enabled;
        best_src = "top";
    }

    /* policy */
    for (int i = 0; i < layer_count; i++) {
        if (layers[i].rule->policy) {
            free(rule->policy);
            rule->policy = strdup(layers[i].rule->policy);
            best_src = layers[i].source;
            break;
        }
    }

    /* allow_from */
    for (int i = 0; i < layer_count; i++) {
        if (layers[i].rule->allow_from) {
            strlist_free(rule->allow_from);
            rule->allow_from = NULL;
            rule->allow_from_count = 0;
            /* deep copy */
            size_t cnt = 0;
            for (size_t j = 0; layers[i].rule->allow_from[j]; j++) cnt++;
            if (cnt > 0) {
                rule->allow_from = calloc(cnt + 1, sizeof(char *));
                if (rule->allow_from) {
                    for (size_t j = 0; j < cnt; j++) {
                        rule->allow_from[j] = strdup(layers[i].rule->allow_from[j]);
                    }
                    rule->allow_from[cnt] = NULL;
                    rule->allow_from_count = cnt;
                }
            }
            break;
        }
    }
    /* Fallback to top-level if no layer set allow_from */
    if (!rule->allow_from && cfg->allow_from) {
        size_t cnt = cfg->allow_from_count;
        if (cnt > 0) {
            rule->allow_from = calloc(cnt + 1, sizeof(char *));
            if (rule->allow_from) {
                for (size_t j = 0; j < cnt; j++) {
                    rule->allow_from[j] = strdup(cfg->allow_from[j]);
                }
                rule->allow_from[cnt] = NULL;
                rule->allow_from_count = cnt;
            }
        }
    }

    /* Determine match_source: highest-priority tier that contributed */
    /* "exact:" > "wildcard" > "default" */
    int best_tier = 3; /* default */
    for (int i = 0; i < layer_count; i++) {
        const char *src = layers[i].source;
        if (!src) continue;
        if (strncmp(src, "exact:", 6) == 0) { best_tier = 0; best_src = src; break; }
        if (strcmp(src, "wildcard") == 0 && best_tier > 1) { best_tier = 1; best_src = src; }
    }

    if (best_tier == 3) best_src = "default";
    snprintf(rule->match_source, sizeof(rule->match_source), "%s", best_src);

    return rule;
}

void feishu_rules_free_resolved(feishu_resolved_rule_t *rule) {
    if (!rule) return;
    if (rule->policy) free(rule->policy);
    strlist_free(rule->allow_from);
    free(rule);
}

/* ================================================================
 *  Pairing store
 * ================================================================ */

/** Load pairing approved set. Returns NULL-terminated array of open_ids. */
static char **load_pairing_approved(size_t *out_count) {
    json_t *data = mtime_cache_load(&g_pairing_cache, PAIRING_FILENAME);
    *out_count = 0;
    if (!data) return NULL;

    json_t *approved = json_obj_get(data, "approved");
    if (!approved || approved->type != JSON_OBJECT) return NULL;

    size_t n = approved->c.count;
    if (n == 0) return NULL;

    char **list = calloc(n + 1, sizeof(char *));
    if (!list) return NULL;

    size_t idx = 0;
    for (size_t i = 0; i < n; i++) {
        const char *key = approved->c.keys[i];
        json_t *val = approved->c.items[i];
        if (key && val && val->type != JSON_NULL) {
            list[idx] = strdup(key);
            if (list[idx]) idx++;
        }
    }
    list[idx] = NULL;
    *out_count = idx;
    return list;
}

/** Atomic save: write to .tmp, then rename. */
static bool save_pairing_json(json_t *data) {
    char path[1024], tmp_path[1024];
    rules_file_path(path, sizeof(path), PAIRING_FILENAME);
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    /* Ensure directory exists */
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s", rules_hermes_home());
    struct stat st;
    if (stat(dir, &st) != 0) {
        mkdir(dir, 0700);
    }

    /* Clean up null-valued entries before saving (since libjson has no delete) */
    json_t *approved = json_obj_get(data, "approved");
    if (approved && approved->type == JSON_OBJECT) {
        /* Build a new clean approved object without null values */
        json_t *cleaned = json_object();
        size_t n = approved->c.count;
        for (size_t i = 0; i < n; i++) {
            const char *k = approved->c.keys[i];
            json_t *v = approved->c.items[i];
            if (k && v && v->type != JSON_NULL) {
                json_set(cleaned, k, json_copy(v));
            }
        }
        json_set(data, "approved", cleaned);
    }

    char *json_str = json_serialize_pretty(data, 2);
    if (!json_str) return false;

    FILE *f = fopen(tmp_path, "w");
    if (!f) {
        free(json_str);
        return false;
    }
    fprintf(f, "%s\n", json_str);
    fclose(f);
    free(json_str);

    if (rename(tmp_path, path) != 0) {
        unlink(tmp_path);
        return false;
    }

    /* Invalidate mtime cache */
    g_pairing_cache.mtime = 0;
    if (g_pairing_cache.data) {
        json_free(g_pairing_cache.data);
        g_pairing_cache.data = NULL;
    }

    return true;
}

bool feishu_rules_pairing_add(const char *user_open_id) {
    if (!user_open_id || !user_open_id[0]) return false;

    json_t *data = mtime_cache_load(&g_pairing_cache, PAIRING_FILENAME);
    if (!data) {
        /* Create new data object */
        data = json_object();
        json_set(data, "approved", json_object());
        /* We can't fake the cache, so we must store it. Use a temp json object. */
    }

    /* Ensure 'approved' is an object */
    json_t *approved = json_obj_get(data, "approved");
    if (!approved || approved->type != JSON_OBJECT) {
        json_set(data, "approved", json_object());
        approved = json_obj_get(data, "approved");
    }

    /* Check if already present */
    if (json_obj_get(approved, user_open_id))
        return false;

    /* Add with approved_at timestamp */
    json_t *meta = json_object();
    json_set(meta, "approved_at", json_number((double)time(NULL)));
    json_set(approved, user_open_id, meta);

    return save_pairing_json(data);
}

bool feishu_rules_pairing_remove(const char *user_open_id) {
    if (!user_open_id || !user_open_id[0]) return false;

    json_t *data = mtime_cache_load(&g_pairing_cache, PAIRING_FILENAME);
    if (!data) return false;

    json_t *approved = json_obj_get(data, "approved");
    if (!approved || approved->type != JSON_OBJECT) return false;

    if (!json_obj_get(approved, user_open_id))
        return false;

    /* Remove by setting to null (libjson may not have delete) */
    json_set(approved, user_open_id, json_null());

    return save_pairing_json(data);
}

char *feishu_rules_pairing_list(void) {
    json_t *data = mtime_cache_load(&g_pairing_cache, PAIRING_FILENAME);
    if (!data) {
        /* Return empty object */
        return strdup("{}");
    }

    json_t *approved = json_obj_get(data, "approved");
    if (!approved || approved->type != JSON_OBJECT) {
        return strdup("{}");
    }

    /* Serialize just the approved object */
    char *json_str = json_serialize_pretty(approved, 2);
    return json_str;
}

/* ================================================================
 *  is_user_allowed
 * ================================================================ */

bool feishu_rules_is_user_allowed(const feishu_resolved_rule_t *rule,
                                   const char *user_open_id)
{
    if (!rule || !user_open_id) return false;

    /* Check allow_from first */
    if (rule->allow_from) {
        for (size_t i = 0; rule->allow_from[i]; i++) {
            if (strcmp(rule->allow_from[i], user_open_id) == 0)
                return true;
        }
    }

    /* Check pairing policy */
    if (rule->policy && strcmp(rule->policy, "pairing") == 0) {
        size_t count = 0;
        char **approved = load_pairing_approved(&count);
        if (!approved) return false;

        bool found = false;
        for (size_t i = 0; approved[i]; i++) {
            if (strcmp(approved[i], user_open_id) == 0) {
                found = true;
                break;
            }
        }
        strlist_free(approved);
        return found;
    }

    return false;
}

/* ================================================================
 *  Cache invalidation
 * ================================================================ */

void feishu_rules_invalidate_cache(void) {
    g_rules_cache.mtime = 0;
    if (g_rules_cache.data) {
        json_free(g_rules_cache.data);
        g_rules_cache.data = NULL;
    }
    g_pairing_cache.mtime = 0;
    if (g_pairing_cache.data) {
        json_free(g_pairing_cache.data);
        g_pairing_cache.data = NULL;
    }
}
