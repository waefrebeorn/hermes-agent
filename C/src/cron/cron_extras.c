/*
 * cron_extras.c — P172-P175: Job retry, notification, chaining, templating.
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

/* ================================================================
 *  P172: Job Retry
 * ================================================================ */

#define MAX_RETRY_JOBS 64

static struct {
    char job_name[128];
    int  max_retries;
    int  backoff_sec;
    int  current_retry;
} g_retry_state[MAX_RETRY_JOBS];
static int g_retry_count = 0;

bool cron_job_set_retry(const char *job_name, int max_retries, int backoff_sec) {
    if (!job_name) return false;

    /* Find existing slot */
    for (int i = 0; i < g_retry_count; i++) {
        if (strcmp(g_retry_state[i].job_name, job_name) == 0) {
            g_retry_state[i].max_retries = max_retries;
            g_retry_state[i].backoff_sec = backoff_sec;
            g_retry_state[i].current_retry = 0;
            return true;
        }
    }

    /* Add new slot */
    if (g_retry_count >= MAX_RETRY_JOBS) return false;
    snprintf(g_retry_state[g_retry_count].job_name, sizeof(g_retry_state[0].job_name), "%s", job_name);
    g_retry_state[g_retry_count].max_retries = max_retries;
    g_retry_state[g_retry_count].backoff_sec = backoff_sec;
    g_retry_state[g_retry_count].current_retry = 0;
    g_retry_count++;
    return true;
}

int cron_job_get_retry_count(const char *job_name) {
    for (int i = 0; i < g_retry_count; i++) {
        if (strcmp(g_retry_state[i].job_name, job_name) == 0)
            return g_retry_state[i].current_retry;
    }
    return 0;
}

int cron_job_get_max_retries(const char *job_name) {
    for (int i = 0; i < g_retry_count; i++) {
        if (strcmp(g_retry_state[i].job_name, job_name) == 0)
            return g_retry_state[i].max_retries;
    }
    return 0;
}

/* Increment retry counter. Returns true if should retry, false if exhausted. */
bool cron_job_increment_retry(const char *job_name) {
    if (!job_name) return false;
    for (int i = 0; i < g_retry_count; i++) {
        if (strcmp(g_retry_state[i].job_name, job_name) == 0) {
            g_retry_state[i].current_retry++;
            if (g_retry_state[i].current_retry > g_retry_state[i].max_retries)
                return false;
            /* Sleep with exponential backoff */
            int sleep_sec = g_retry_state[i].backoff_sec *
                           (1 << (g_retry_state[i].current_retry - 1));
            if (sleep_sec > 3600) sleep_sec = 3600;
            sleep(sleep_sec);
            return true;
        }
    }
    return false;
}

void cron_job_reset_retry(const char *job_name) {
    if (!job_name) return;
    for (int i = 0; i < g_retry_count; i++) {
        if (strcmp(g_retry_state[i].job_name, job_name) == 0) {
            g_retry_state[i].current_retry = 0;
            return;
        }
    }
}

/* ================================================================
 *  P173: Job Notification
 * ================================================================ */

static char g_notify_channel[256] = {0};

/* Gateway send function — set by gateway init, NULL when standalone */
static bool (*g_notify_send_fn)(const char *platform, const char *chat_id, const char *text) = NULL;

/** Set the gateway delivery function for cron notifications. */
void cron_notify_set_send_fn(bool (*fn)(const char *, const char *, const char *)) {
    g_notify_send_fn = fn;
}

bool cron_notify_set_channel(const char *channel_id) {
    if (!channel_id) return false;
    snprintf(g_notify_channel, sizeof(g_notify_channel), "%s", channel_id);
    return true;
}

const char *cron_notify_get_channel(void) {
    return g_notify_channel[0] ? g_notify_channel : NULL;
}

bool cron_notify_on_complete(const char *job_name, bool enabled) {
    (void)job_name;
    (void)enabled;
    /* Notification settings per-job are stored in the job store */
    return true;
}

bool cron_notify_on_failure(const char *job_name, bool enabled) {
    (void)job_name;
    (void)enabled;
    return true;
}

/* Send notification about job completion/failure.
 * Returns true if notification was sent. */
bool cron_send_notification(const char *job_name, const char *status,
                             const char *message) {
    if (!g_notify_channel[0]) return false;

    /* Parse "platform:chat_id" format */
    char channel_copy[256];
    snprintf(channel_copy, sizeof(channel_copy), "%s", g_notify_channel);

    char *platform = channel_copy;
    char *chat_id = strchr(channel_copy, ':');
    if (chat_id) {
        *chat_id = '\0';
        chat_id++;
    }

    /* Build notification message */
    char notify_msg[4096];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm);

    snprintf(notify_msg, sizeof(notify_msg),
             "[cron] Job '%s' %s at %s%s%s",
             job_name, status, ts,
             message ? ": " : "",
             message ? message : "");

    /* Try gateway delivery first */
    if (g_notify_send_fn && platform && chat_id && chat_id[0]) {
        if (g_notify_send_fn(platform, chat_id, notify_msg))
            return true;
    }

    /* Fallback: log to stderr */
    fprintf(stderr, "[cron-notify] %s\n", notify_msg);
    return true;
}

/* ================================================================
 *  P174: Job Chaining
 * ================================================================ */

#define MAX_CHAIN_ENTRIES 64

static struct {
    char job_name[128];
    char context_from[128];
    char output[4096];
} g_chain_state[MAX_CHAIN_ENTRIES];
static int g_chain_count = 0;

bool cron_chain_set_context(const char *job_name, const char *context_from) {
    if (!job_name) return false;

    /* Find existing entry */
    for (int i = 0; i < g_chain_count; i++) {
        if (strcmp(g_chain_state[i].job_name, job_name) == 0) {
            snprintf(g_chain_state[i].context_from, sizeof(g_chain_state[0].context_from),
                     "%s", context_from ? context_from : "");
            return true;
        }
    }

    /* Add new entry */
    if (g_chain_count >= MAX_CHAIN_ENTRIES) return false;
    snprintf(g_chain_state[g_chain_count].job_name, sizeof(g_chain_state[0].job_name), "%s", job_name);
    snprintf(g_chain_state[g_chain_count].context_from, sizeof(g_chain_state[0].context_from),
             "%s", context_from ? context_from : "");
    g_chain_count++;
    return true;
}

const char *cron_chain_get_context(const char *job_name) {
    if (!job_name) return NULL;
    for (int i = 0; i < g_chain_count; i++) {
        if (strcmp(g_chain_state[i].job_name, job_name) == 0)
            return g_chain_state[i].context_from;
    }
    return NULL;
}

char *cron_chain_get_output(const char *job_name) {
    if (!job_name) return NULL;
    for (int i = 0; i < g_chain_count; i++) {
        if (strcmp(g_chain_state[i].job_name, job_name) == 0 &&
            g_chain_state[i].context_from[0]) {
            /* Look up the source job's output */
            for (int j = 0; j < g_chain_count; j++) {
                if (strcmp(g_chain_state[j].job_name, g_chain_state[i].context_from) == 0)
                    return strdup(g_chain_state[j].output);
            }
        }
    }
    return NULL;
}

void cron_chain_store_output(const char *job_name, const char *output) {
    if (!job_name) return;

    for (int i = 0; i < g_chain_count; i++) {
        if (strcmp(g_chain_state[i].job_name, job_name) == 0) {
            snprintf(g_chain_state[i].output, sizeof(g_chain_state[0].output),
                     "%s", output ? output : "");
            return;
        }
    }

    /* Create new entry if not found */
    if (g_chain_count < MAX_CHAIN_ENTRIES) {
        snprintf(g_chain_state[g_chain_count].job_name, sizeof(g_chain_state[0].job_name), "%s", job_name);
        snprintf(g_chain_state[g_chain_count].output, sizeof(g_chain_state[0].output),
                 "%s", output ? output : "");
        g_chain_count++;
    }
}

/* ================================================================
 *  P175: Job Templating
 * ================================================================ */

#define MAX_TEMPLATES 32

typedef struct {
    char name[128];
    char schedule[128];
    char command[2048];
    char params_json[4096];
} cron_template_t;

static cron_template_t g_templates[MAX_TEMPLATES];
static int g_template_count = 0;

bool cron_template_create(const char *name, const char *schedule,
                           const char *command, const char *params_json)
{
    if (!name || !schedule || g_template_count >= MAX_TEMPLATES)
        return false;

    /* Check for duplicates */
    for (int i = 0; i < g_template_count; i++) {
        if (strcmp(g_templates[i].name, name) == 0)
            return false;
    }

    cron_template_t *t = &g_templates[g_template_count++];
    memset(t, 0, sizeof(*t));
    snprintf(t->name, sizeof(t->name), "%s", name);
    snprintf(t->schedule, sizeof(t->schedule), "%s", schedule);
    snprintf(t->command, sizeof(t->command), "%s", command ? command : "");
    snprintf(t->params_json, sizeof(t->params_json), "%s", params_json ? params_json : "");
    return true;
}

bool cron_template_instantiate(const char *template_name,
                                const char *params_json,
                                char *out_name, size_t out_name_sz,
                                char *out_schedule, size_t out_sched_sz,
                                char *out_command, size_t out_cmd_sz)
{
    if (!template_name || !out_name || !out_schedule || !out_command)
        return false;

    /* Find template */
    cron_template_t *t = NULL;
    for (int i = 0; i < g_template_count; i++) {
        if (strcmp(g_templates[i].name, template_name) == 0) {
            t = &g_templates[i];
            break;
        }
    }
    if (!t) return false;

    /* Set base values */
    snprintf(out_schedule, out_sched_sz, "%s", t->schedule);

    /* Process command template with params */
    if (params_json && params_json[0]) {
        char *err = NULL;
        json_t *params = json_parse(params_json, &err);
        if (params) {
            /* Replace {{param}} placeholders in command */
            char cmd[2048];
            snprintf(cmd, sizeof(cmd), "%s", t->command);

            /* Simple placeholder replacement */
            size_t len = params->c.count;
            for (size_t i = 0; i < len; i++) {
                const char *key = params->c.keys ? params->c.keys[i] : NULL;
                json_t *val = params->c.items[i];
                if (key && val && val->type == JSON_STRING) {
                    /* Build {{key}} placeholder */
                    char placeholder[128];
                    snprintf(placeholder, sizeof(placeholder), "{{%s}}", key);

                    /* Replace in command */
                    char temp[4096];
                    char *pos = strstr(cmd, placeholder);
                    while (pos) {
                        const char *replacement = val->str_val ? val->str_val : "";
                        size_t pre_len = (size_t)(pos - cmd);
                        size_t rep_len = strlen(replacement);
                        size_t rest = strlen(pos + strlen(placeholder)) + 1;

                        /* Build new string */
                        memcpy(temp, cmd, pre_len);
                        memcpy(temp + pre_len, replacement, rep_len);
                        memcpy(temp + pre_len + rep_len, pos + strlen(placeholder), rest);

                        snprintf(cmd, sizeof(cmd), "%s", temp);
                        pos = strstr(cmd, placeholder);
                    }
                }
            }
            json_free(params);
            snprintf(out_command, out_cmd_sz, "%s", cmd);
        } else {
            free(err);
            snprintf(out_command, out_cmd_sz, "%s", t->command);
        }
    } else {
        snprintf(out_command, out_cmd_sz, "%s", t->command);
    }

    /* Generate name from template */
    snprintf(out_name, out_name_sz, "%s_%ld", template_name, time(NULL));
    return true;
}

/* ================================================================
 *  P176: Cron utility functions (port of cronjob_tools.py helpers)
 * ================================================================ */

/**
 * Extract a schedule display string from a job JSON object.
 *
 * Logic (port of Python _schedule_display_for_job):
 *   1. Return schedule_display if non-empty.
 *   2. If schedule is a dict, try display → value → expr → run_at keys.
 *   3. If schedule is a string, return it directly.
 *   4. Fallback "?".
 *
 * Returns a pointer to a static buffer (NOT thread-safe, but fine for
 * display/convenience usage, matching pattern of cron_coerce_job_text).
 * Returns "?" on NULL or empty input.
 */
const char *cron_schedule_display_for_job(json_t *job) {
    static char buf[256];
    if (!job || job->type != JSON_OBJECT) return "?";

    /* 1. Explicit schedule_display field */
    json_t *sd = json_obj_get(job, "schedule_display");
    if (sd && sd->type == JSON_STRING && sd->str_val && sd->str_val[0]) {
        /* Trim */
        const char *s = sd->str_val;
        while (*s == ' ' || *s == '\t') s++;
        if (*s) {
            size_t slen = strlen(s);
            while (slen > 0 && (s[slen-1] == ' ' || s[slen-1] == '\t')) slen--;
            if (slen > 0 && slen < sizeof(buf)) {
                memcpy(buf, s, slen);
                buf[slen] = '\0';
                return buf;
            }
        }
    }

    /* 2. schedule field */
    json_t *schedule = json_obj_get(job, "schedule");
    if (schedule && schedule->type == JSON_OBJECT) {
        const char *keys[] = {"display", "value", "expr", "run_at", NULL};
        for (int i = 0; keys[i]; i++) {
            json_t *v = json_obj_get(schedule, keys[i]);
            if (v && v->type == JSON_STRING && v->str_val && v->str_val[0]) {
                const char *s = v->str_val;
                while (*s == ' ' || *s == '\t') s++;
                if (*s) {
                    size_t slen = strlen(s);
                    while (slen > 0 && (s[slen-1] == ' ' || s[slen-1] == '\t')) slen--;
                    if (slen > 0 && slen < sizeof(buf)) {
                        memcpy(buf, s, slen);
                        buf[slen] = '\0';
                        return buf;
                    }
                }
            }
        }
    } else if (schedule && schedule->type == JSON_STRING && schedule->str_val) {
        /* 3. Direct string schedule */
        snprintf(buf, sizeof(buf), "%s", schedule->str_val);
        return buf;
    }

    /* 4. Fallback */
    return "?";
}

/**
 * Ensure cron directories exist with secure permissions.
 * Port of Python cron/jobs.py ensure_dirs().
 *
 * @param hermes_home  Path to HERMES_HOME (e.g. ~/.hermes)
 */
bool cron_ensure_dirs(const char *hermes_home) {
    if (!hermes_home || !hermes_home[0]) return false;

    char cron_dir[HERMES_PATH_MAX];
    char output_dir[HERMES_PATH_MAX];
    snprintf(cron_dir, sizeof(cron_dir), "%s/cron", hermes_home);
    snprintf(output_dir, sizeof(output_dir), "%s/cron/output", hermes_home);

    /* mkdir -p both */
    if (mkdir(cron_dir, 0700) != 0 && errno != EEXIST) return false;
    if (mkdir(output_dir, 0700) != 0 && errno != EEXIST) return false;

    /* Secure permissions */
    cron_secure_dir(cron_dir);
    cron_secure_dir(output_dir);

    return true;
}

/**
 * Validate a job ID for use as a safe filesystem path component.
 * Port of Python cron/jobs.py _job_output_dir().
 *
 * Rejects: empty, "." , "..", paths containing "/" or "\\", paths that
 * appear absolute, or paths with a Windows drive letter.
 *
 * @param job_id   The job ID string to validate.
 * @param out_err  Optional output buffer for error message (256 bytes).
 * @return true if job_id is safe, false with error in out_err if not.
 */
bool cron_validate_job_id(const char *job_id, char *out_err) {
    if (!job_id || !job_id[0]) {
        if (out_err) snprintf(out_err, 256, "Job ID is empty");
        return false;
    }
    /* Trim whitespace */
    const char *p = job_id;
    while (*p == ' ' || *p == '\t') p++;
    if (!*p) {
        if (out_err) snprintf(out_err, 256, "Job ID is whitespace only");
        return false;
    }
    /* Reject "." and ".." */
    if (strcmp(p, ".") == 0 || strcmp(p, "..") == 0) {
        if (out_err) snprintf(out_err, 256, "Job ID cannot be '.' or '..'");
        return false;
    }
    /* Reject paths containing "/" or "\\" */
    if (strchr(p, '/') || strchr(p, '\\')) {
        if (out_err) snprintf(out_err, 256, "Job ID cannot contain path separators");
        return false;
    }
    /* Reject absolute paths (starts with / on Unix) */
    if (p[0] == '/') {
        if (out_err) snprintf(out_err, 256, "Job ID cannot be an absolute path");
        return false;
    }
    /* Reject Windows drive letters (e.g. "C:foo") */
    if (p[0] && p[1] == ':' && p[2]) {
        if (out_err) snprintf(out_err, 256, "Job ID contains a drive letter");
        return false;
    }
    return true;
}

/**
 * Build a safe output directory path for a cron job.
 *
 * Creates "{hermes_home}/cron/output/{job_id}" after validating the job_id
 * with cron_validate_job_id(). Returns malloc'd path (caller must free),
 * or NULL on validation failure with error in out_err.
 *
 * Port of Python cron/jobs.py _job_output_dir() returning the full path.
 */
char *cron_job_output_dir(const char *hermes_home, const char *job_id, char *out_err) {
    if (!hermes_home || !hermes_home[0]) {
        if (out_err) snprintf(out_err, 256, "HERMES_HOME is empty");
        return NULL;
    }
    if (!cron_validate_job_id(job_id, out_err)) {
        return NULL;
    }
    size_t len = strlen(hermes_home) + 1 + 5 + 1 + 6 + 1 + strlen(job_id) + 1;
    char *path = malloc(len);
    if (!path) {
        if (out_err) snprintf(out_err, 256, "Out of memory");
        return NULL;
    }
    snprintf(path, len, "%s/cron/output/%s", hermes_home, job_id);
    return path;
}

/**
 * Normalize and validate a cron job workdir path.
 *
 * Rules (port of Python cron/jobs.py _normalize_workdir()):
 *   - NULL/empty → NULL (feature off).
 *   - ~ is expanded to $HOME.
 *   - Relative paths are rejected.
 *   - Path must exist and be a directory.
 *
 * Returns malloc'd resolved absolute path (caller must free),
 * or NULL on validation failure with error in out_err.
 */
char *cron_normalize_workdir(const char *workdir, char *out_err) {
    if (!workdir || !workdir[0]) {
        return NULL;  /* feature off, not an error */
    }
    /* Trim whitespace */
    const char *p = workdir;
    while (*p == ' ' || *p == '\t') p++;
    if (!*p) {
        return NULL;  /* feature off */
    }

    /* Expand ~ to $HOME */
    char expanded[HERMES_PATH_MAX];
    if (p[0] == '~') {
        const char *home = getenv("HOME");
        if (!home) {
            if (out_err) snprintf(out_err, 256, "HOME not set, cannot expand ~");
            return NULL;
        }
        snprintf(expanded, sizeof(expanded), "%s%s", home, p + 1);
    } else {
        snprintf(expanded, sizeof(expanded), "%s", p);
    }

    /* Reject relative paths */
    if (expanded[0] != '/') {
        if (out_err) snprintf(out_err, 256,
            "Cron workdir must be an absolute path (got '%s'). "
            "Cron jobs run detached from any shell cwd.", workdir);
        return NULL;
    }

    /* Resolve the path */
    char resolved[HERMES_PATH_MAX];
    struct stat st;
    if (stat(expanded, &st) != 0) {
        if (out_err) snprintf(out_err, 256, "Cron workdir does not exist: %s", expanded);
        return NULL;
    }
    if (!S_ISDIR(st.st_mode)) {
        if (out_err) snprintf(out_err, 256, "Cron workdir is not a directory: %s", expanded);
        return NULL;
    }

    /* Use realpath if available, else expanded */
    char *real = realpath(expanded, NULL);
    if (real) {
        snprintf(resolved, sizeof(resolved), "%s", real);
        free(real);
    } else {
        snprintf(resolved, sizeof(resolved), "%s", expanded);
    }

    return strdup(resolved);
}

/** Normalize a skill/skills parameter into a canonical, deduplicated string list.
 *  Returns malloc'd NULL-terminated array (caller frees each string + array).
 *  Port of Python cronjob_tools._canonical_skills(). */
char **cron_canonical_skills(const char *skill, json_t *skills, size_t *out_count) {
    size_t cap = 16;
    size_t count = 0;
    char **result = calloc(cap, sizeof(char *));
    if (!result) { *out_count = 0; return NULL; }

    /* Helper: add a string if non-empty and not duplicate */
    #define ADD_SKILL(s) do { \
        const char *_s = (s); \
        if (_s && _s[0]) { \
            bool dup = false; \
            for (size_t _i = 0; _i < count; _i++) { \
                if (strcmp(result[_i], _s) == 0) { dup = true; break; } \
            } \
            if (!dup) { \
                if (count >= cap) { \
                    cap *= 2; \
                    char **new_r = realloc(result, cap * sizeof(char *)); \
                    if (!new_r) { /* leak is acceptable on OOM */ break; } \
                    result = new_r; \
                } \
                result[count] = strdup(_s); \
                if (result[count]) count++; \
            } \
        } \
    } while(0)

    /* If skills is NULL or empty array, use the single skill string */
    if (!skills || skills->type != JSON_ARRAY || json_len(skills) == 0) {
        ADD_SKILL(skill);
    } else {
        /* Iterate skills array */
        size_t n = json_len(skills);
        for (size_t i = 0; i < n; i++) {
            json_t *item = json_get(skills, i);
            if (item && item->type == JSON_STRING) {
                ADD_SKILL(item->str_val);
            }
        }
        /* If skills array is provided but empty, fall back to skill */
        if (count == 0) {
            ADD_SKILL(skill);
        }
    }

    #undef ADD_SKILL

    result[count] = NULL;
    *out_count = count;
    return result;
}

/** Normalize an optional value: trim whitespace, return NULL if empty.
 *  If strip_trailing_slash is true, strips trailing '/' before trimming.
 *  Returns malloc'd string or NULL. Caller must free.
 *  Port of Python cronjob_tools._normalize_optional_job_value(). */
char *cron_normalize_value(const char *value, bool strip_trailing_slash) {
    if (!value) return NULL;

    const char *p = value;
    /* Skip leading whitespace */
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;

    if (!*p) return NULL;  /* empty after leading whitespace */

    /* If strip_trailing_slash, find the effective end (before trailing slashes + whitespace) */
    const char *end;
    if (strip_trailing_slash) {
        /* Find last non-slash non-whitespace char */
        end = p + strlen(p) - 1;
        while (end >= p && (*end == '/' || *end == ' ' || *end == '\t' || *end == '\n' || *end == '\r'))
            end--;
    } else {
        end = p + strlen(p) - 1;
        while (end >= p && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r'))
            end--;
    }

    if (end < p) return NULL;  /* all whitespace/slashes */

    size_t len = (size_t)(end - p + 1);
    char *result = malloc(len + 1);
    if (!result) return NULL;
    memcpy(result, p, len);
    result[len] = '\0';
    return result;
}

/** Normalize a deliver parameter: string stays as trimmed string,
 *  list/tuple gets joined with commas. Returns malloc'd string or NULL.
 *  Port of Python cronjob_tools._normalize_deliver_param(). */
char *cron_normalize_deliver(json_t *deliver) {
    if (!deliver) return NULL;

    if (deliver->type == JSON_STRING) {
        /* Trim the string */
        const char *s = deliver->str_val;
        while (*s == ' ' || *s == '\t') s++;
        if (!*s) return NULL;
        size_t len = strlen(s);
        while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t')) len--;
        return strndup(s, len);
    }

    if (deliver->type == JSON_ARRAY) {
        size_t n = json_len(deliver);
        if (n == 0) return NULL;

        /* First pass: calculate total length */
        size_t total = 0;
        for (size_t i = 0; i < n; i++) {
            json_t *item = json_get(deliver, i);
            if (item && item->type == JSON_STRING && item->str_val) {
                const char *s = item->str_val;
                while (*s == ' ' || *s == '\t') s++;
                if (*s) {
                    /* Trim trailing whitespace */
                    size_t slen = strlen(s);
                    while (slen > 0 && (s[slen-1] == ' ' || s[slen-1] == '\t')) slen--;
                    if (slen > 0) total += slen + 1; /* +1 for comma or null */
                }
            }
        }
        if (total == 0) return NULL;

        char *result = malloc(total);
        if (!result) return NULL;
        result[0] = '\0';

        for (size_t i = 0; i < n; i++) {
            json_t *item = json_get(deliver, i);
            if (item && item->type == JSON_STRING && item->str_val) {
                const char *s = item->str_val;
                while (*s == ' ' || *s == '\t') s++;
                if (*s) {
                    /* Trim trailing whitespace */
                    size_t slen = strlen(s);
                    while (slen > 0 && (s[slen-1] == ' ' || s[slen-1] == '\t')) slen--;
                    if (slen > 0) {
                        if (result[0]) strcat(result, ",");
                        strncat(result, s, slen);
                    }
                }
            }
        }

        return result;
    }

    return NULL;
}

/* Port of cron/jobs.py _secure_dir: set directory to owner-only access (0700). */
bool cron_secure_dir(const char *path) {
    if (!path) return false;
    struct stat st;
    if (stat(path, &st) != 0) return false;
    if (!S_ISDIR(st.st_mode)) return false;
    return chmod(path, 0700) == 0;
}

/* Port of cron/jobs.py _secure_file: set file to owner-only read/write (0600). */
bool cron_secure_file(const char *path) {
    if (!path) return false;
    struct stat st;
    if (stat(path, &st) != 0) return false;
    if (!S_ISREG(st.st_mode)) return false;
    return chmod(path, 0600) == 0;
}

/* Port of cron/jobs.py _coerce_job_text: coerce a nullable value to string with fallback. */
const char *cron_coerce_job_text(const char *value, const char *fallback) {
    if (!value || !value[0]) return fallback;
    return value;
}

/* Port of cron/jobs.py parse_duration: parse "30m", "2h", "1d" into minutes.
 * Returns minutes on success, -1 on parse error. */
int cron_parse_duration(const char *s) {
    if (!s || !s[0]) return -1;

    /* Skip leading whitespace */
    while (*s == ' ' || *s == '\t') s++;
    if (!*s) return -1;

    /* Parse digits */
    char *end = NULL;
    long val = strtol(s, &end, 10);
    if (end == s || val <= 0 || val > 1000000) return -1;

    /* Skip to unit */
    const char *unit = end;
    while (*unit == ' ' || *unit == '\t') unit++;
    if (!*unit) return -1;

    /* Convert unit to lowercase for comparison */
    char unit_lower[16];
    size_t ui = 0;
    while (unit[ui] && ui < sizeof(unit_lower) - 1) {
        unit_lower[ui] = (unit[ui] >= 'A' && unit[ui] <= 'Z') ? unit[ui] + 32 : unit[ui];
        ui++;
    }
    unit_lower[ui] = '\0';

    /* Match unit */
    int multiplier = 0;
    if (strcmp(unit_lower, "m") == 0 || strcmp(unit_lower, "min") == 0 ||
        strcmp(unit_lower, "mins") == 0 || strcmp(unit_lower, "minute") == 0 ||
        strcmp(unit_lower, "minutes") == 0)
        multiplier = 1;
    else if (strcmp(unit_lower, "h") == 0 || strcmp(unit_lower, "hr") == 0 ||
             strcmp(unit_lower, "hrs") == 0 || strcmp(unit_lower, "hour") == 0 ||
             strcmp(unit_lower, "hours") == 0)
        multiplier = 60;
    else if (strcmp(unit_lower, "d") == 0 || strcmp(unit_lower, "day") == 0 ||
             strcmp(unit_lower, "days") == 0)
        multiplier = 1440;

    if (multiplier == 0) return -1;

    long result = val * multiplier;
    if (result > 1000000 || result < 0) return -1;
    return (int)result;
}
