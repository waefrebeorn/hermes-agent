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

    /* Build notification message */
    char notify_msg[4096];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm);

    snprintf(notify_msg, sizeof(notify_msg),
             "{\"event\":\"cron_job\",\"job\":\"%s\",\"status\":\"%s\","
             "\"time\":\"%s\",\"message\":\"%s\",\"channel\":\"%s\"}",
             job_name, status, ts,
             message ? message : "",
             g_notify_channel);

    /* TODO: In production, this would send via gateway to the channel.
     * For now, log the notification. */
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
            size_t len = json_len(params);
            for (size_t i = 0; i < len; i++) {
                const char *key = json_obj_get_key(params, i);
                json_t *val = json_get(params, i);
                if (key && val && json_is_string(val)) {
                    /* Build {{key}} placeholder */
                    char placeholder[128];
                    snprintf(placeholder, sizeof(placeholder), "{{%s}}", key);

                    /* Replace in command */
                    char temp[4096];
                    char *pos = strstr(cmd, placeholder);
                    while (pos) {
                        const char *replacement = json_get_str(val, NULL, "");
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
