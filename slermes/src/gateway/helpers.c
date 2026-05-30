/*
 * helpers.c — Shared gateway helper utilities.
 * Port of Python gateway/platforms/helpers.py.
 */

#include "gateway_helpers.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ================================================================
 *  Message Deduplicator
 * ================================================================ */

void msg_dedup_init(msg_dedup_t *d) {
    msg_dedup_init_custom(d, 2000, 300.0);
}

void msg_dedup_init_custom(msg_dedup_t *d, int max_size, double ttl_seconds) {
    if (!d) return;
    d->msg_ids = NULL;
    d->timestamps = NULL;
    d->count = 0;
    d->max_size = max_size > 0 ? max_size : 2000;
    d->ttl_seconds = ttl_seconds > 0 ? ttl_seconds : 300.0;
}

bool msg_dedup_is_duplicate(msg_dedup_t *d, const char *msg_id) {
    if (!d || !msg_id || !*msg_id) return false;

    double now = (double)time(NULL);

    /* Scan existing entries */
    for (int i = 0; i < d->count; i++) {
        if (strcmp(d->msg_ids[i], msg_id) == 0) {
            if (now - d->timestamps[i] < d->ttl_seconds) {
                /* Update timestamp and return duplicate */
                d->timestamps[i] = now;
                return true;
            }
            /* Entry expired — remove it (swap with last) */
            free(d->msg_ids[i]);
            d->msg_ids[i] = d->msg_ids[d->count - 1];
            d->timestamps[i] = d->timestamps[d->count - 1];
            d->count--;
            break;
        }
    }

    /* Add new entry */
    char **new_ids = realloc(d->msg_ids, (d->count + 1) * sizeof(char *));
    double *new_ts = realloc(d->timestamps, (d->count + 1) * sizeof(double));
    if (!new_ids || !new_ts) {
        free(new_ids); free(new_ts);
        return false;
    }
    d->msg_ids = new_ids;
    d->timestamps = new_ts;
    d->msg_ids[d->count] = strdup(msg_id);
    d->timestamps[d->count] = now;
    d->count++;

    /* Prune if over max_size */
    if (d->count > d->max_size) {
        double cutoff = now - d->ttl_seconds;
        int write_idx = 0;
        for (int i = 0; i < d->count; i++) {
            if (d->timestamps[i] > cutoff) {
                d->msg_ids[write_idx] = d->msg_ids[i];
                d->timestamps[write_idx] = d->timestamps[i];
                write_idx++;
            } else {
                free(d->msg_ids[i]);
            }
        }
        d->count = write_idx;

        /* If still over max after TTL prune, keep newest */
        if (d->count > d->max_size) {
            /* Simple approach: keep last max_size entries */
            int remove = d->count - d->max_size;
            for (int i = 0; i < remove; i++) {
                free(d->msg_ids[i]);
            }
            for (int i = remove; i < d->count; i++) {
                d->msg_ids[i - remove] = d->msg_ids[i];
                d->timestamps[i - remove] = d->timestamps[i];
            }
            d->count = d->max_size;
        }

        /* Shrink arrays */
        char **shrunk_ids = realloc(d->msg_ids, d->count * sizeof(char *));
        double *shrunk_ts = realloc(d->timestamps, d->count * sizeof(double));
        if (shrunk_ids) d->msg_ids = shrunk_ids;
        if (shrunk_ts) d->timestamps = shrunk_ts;
    }

    return false;
}

void msg_dedup_clear(msg_dedup_t *d) {
    if (!d) return;
    for (int i = 0; i < d->count; i++)
        free(d->msg_ids[i]);
    free(d->msg_ids);
    free(d->timestamps);
    d->msg_ids = NULL;
    d->timestamps = NULL;
    d->count = 0;
}

void msg_dedup_destroy(msg_dedup_t *d) {
    msg_dedup_clear(d);
}

/* ================================================================
 *  Markdown Stripping
 * ================================================================ */

typedef struct {
    const char *pattern;
    const char *replacement;
} md_rule_t;

/* Apply a single replacement rule to a dynamically allocated buffer.
 * Returns new buffer (caller must free). */
static char *apply_rule(const char *text, const md_rule_t *rule) {
    if (!text || !rule) return NULL;
    const char *pat = rule->pattern;
    const char *repl = rule->replacement;
    size_t pat_len = strlen(pat);

    /* Count occurrences */
    int count = 0;
    const char *p = text;
    while ((p = strstr(p, pat)) != NULL) {
        count++;
        p += pat_len;
    }
    if (count == 0) return strdup(text);

    /* Build result */
    size_t repl_len = strlen(repl);
    size_t result_cap = strlen(text) + (repl_len * (size_t)count) + 1;
    char *result = malloc(result_cap);
    if (!result) return NULL;

    const char *src = text;
    char *dst = result;
    while (*src) {
        const char *found = strstr(src, pat);
        if (found) {
            size_t copy_len = (size_t)(found - src);
            memcpy(dst, src, copy_len);
            dst += copy_len;
            memcpy(dst, repl, repl_len);
            dst += repl_len;
            src = found + pat_len;
        } else {
            size_t remaining = strlen(src);
            memcpy(dst, src, remaining);
            dst += remaining;
            break;
        }
    }
    *dst = '\0';
    return result;
}

/* Strip markdown formatting patterns.
 * This is a simplified C port of Python's regex-based strip_markdown().
 * Handles the most common patterns. */
char *strip_markdown(const char *text) {
    if (!text) return NULL;

    char *result = strdup(text);
    if (!result) return NULL;

    /* Define replacement rules (order matters) */
    md_rule_t rules[] = {
        /* Code blocks first (remove ``` lines) */
        {"```", ""},
        /* Inline code: `text` → text */
        {"`", ""},
        /* Bold: **text** → text (remove **) */
        {"**", ""},
        /* Italic: *text* → text (remove single *) */
        /* Headings: # text → text */
        {"#", ""},
        /* Link: [text](url) → text — remove ](url) part */
        {"[", ""},
        {"](", " "},
        {")", ""},
        /* Multiple newlines → double newline */
        {"\n\n\n", "\n\n"},
        {"\n\n\n", "\n\n"},
    };
    int n_rules = sizeof(rules) / sizeof(rules[0]);

    for (int i = 0; i < n_rules; i++) {
        char *next = apply_rule(result, &rules[i]);
        if (next) {
            free(result);
            result = next;
        }
    }

    /* Trim leading/trailing whitespace */
    while (*result == ' ' || *result == '\n' || *result == '\t') {
        memmove(result, result + 1, strlen(result));
    }
    size_t len = strlen(result);
    while (len > 0 && (result[len-1] == ' ' || result[len-1] == '\n' || result[len-1] == '\t')) {
        result[--len] = '\0';
    }

    return result;
}

/* ================================================================
 *  Phone Number Redaction
 * ================================================================ */

char *redact_phone(const char *phone) {
    if (!phone) return strdup("<none>");
    if (strcmp(phone, "<none>") == 0) return strdup("<none>");

    size_t len = strlen(phone);
    if (len <= 4) return strdup("****");
    if (len <= 8) {
        /* Show first 2 and last 2 */
        char *result = malloc(10);
        if (!result) return NULL;
        snprintf(result, 10, "%.2s****%.2s", phone, phone + len - 2);
        return result;
    }
    /* Show first 4 and last 4 */
    char *result = malloc(14);
    if (!result) return NULL;
    snprintf(result, 14, "%.4s****%.4s", phone, phone + len - 4);
    return result;
}

/* ================================================================
 *  Thread Participation Tracker
 * ================================================================ */

void thread_tracker_init(thread_tracker_t *t, const char *platform,
                         const char *state_dir) {
    if (!t) return;
    t->thread_ids = NULL;
    t->count = 0;
    t->max_tracked = 500;
    if (platform)
        snprintf(t->platform, sizeof(t->platform), "%s", platform);
    else
        t->platform[0] = '\0';
    if (state_dir)
        snprintf(t->state_dir, sizeof(t->state_dir), "%s", state_dir);
    else
        t->state_dir[0] = '\0';
}

void thread_tracker_load(thread_tracker_t *t) {
    if (!t || !t->state_dir[0] || !t->platform[0]) return;

    char path[1024];
    snprintf(path, sizeof(path), "%s/%s_threads.json",
             t->state_dir, t->platform);

    FILE *f = fopen(path, "r");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz <= 0) { fclose(f); return; }
    rewind(f);
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';

    char *jerr = NULL;
    json_node_t *json = json_parse(buf, &jerr);
    free(buf); free(jerr);
    if (!json || json->type != JSON_ARRAY) {
        if (json) json_free(json);
        return;
    }

    size_t count = json_len(json);
    for (size_t i = 0; i < count && t->count < t->max_tracked; i++) {
        json_t *elem = json_get(json, (int)i);
        const char *tid = (elem && elem->type == JSON_STRING) ? elem->str_val : NULL;
        if (tid) {
            char **new_ids = realloc(t->thread_ids,
                                     (t->count + 1) * sizeof(char *));
            if (!new_ids) break;
            t->thread_ids = new_ids;
            t->thread_ids[t->count] = strdup(tid);
            t->count++;
        }
    }
    json_free(json);
}

void thread_tracker_mark(thread_tracker_t *t, const char *thread_id) {
    if (!t || !thread_id || !*thread_id) return;

    /* Check if already tracked */
    for (int i = 0; i < t->count; i++) {
        if (strcmp(t->thread_ids[i], thread_id) == 0)
            return; /* Already tracked, no need to persist */
    }

    /* Add new */
    char **new_ids = realloc(t->thread_ids, (t->count + 1) * sizeof(char *));
    if (!new_ids) return;
    t->thread_ids = new_ids;
    t->thread_ids[t->count] = strdup(thread_id);
    t->count++;

    /* Prune if over max */
    int start = 0;
    if (t->count > t->max_tracked) {
        start = t->count - t->max_tracked;
        for (int i = 0; i < start; i++)
            free(t->thread_ids[i]);
        for (int i = start; i < t->count; i++)
            t->thread_ids[i - start] = t->thread_ids[i];
        t->count = t->max_tracked;
    }

    /* Persist to JSON file */
    if (t->state_dir[0] && t->platform[0]) {
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s_threads.json",
                 t->state_dir, t->platform);

        json_node_t *json = json_array();
        if (json) {
            for (int i = 0; i < t->count; i++)
                json_append(json, json_string(t->thread_ids[i]));
            char *serialized = json_serialize(json);
            if (serialized) {
                FILE *f = fopen(path, "w");
                if (f) {
                    fputs(serialized, f);
                    fclose(f);
                }
                free(serialized);
            }
            json_free(json);
        }
    }
}

bool thread_tracker_has(thread_tracker_t *t, const char *thread_id) {
    if (!t || !thread_id) return false;
    for (int i = 0; i < t->count; i++) {
        if (strcmp(t->thread_ids[i], thread_id) == 0)
            return true;
    }
    return false;
}

void thread_tracker_destroy(thread_tracker_t *t) {
    if (!t) return;
    for (int i = 0; i < t->count; i++)
        free(t->thread_ids[i]);
    free(t->thread_ids);
    t->thread_ids = NULL;
    t->count = 0;
}
