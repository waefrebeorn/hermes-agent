/*
 * cronjob.c — Cron job scheduling tool for Hermes C.
 * Wraps the cron scheduler: list, add, remove, config, update actions.
 * F26: Job notifications (notify_on_complete, notify_on_failure)
 * F28: Schedule validation (cron_parse at creation)
 * F29: Job retry with backoff (max_retries, backoff_sec)
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Schema — extended with notification, retry, pause/resume/run, update fields */
static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"action\":{\"type\":\"string\",\"description\":\"list | add | remove | config | update | pause | resume | run\"},"
      "\"name\":{\"type\":\"string\",\"description\":\"Job name (required for add/remove/config/update/pause/resume/run)\"},"
      "\"schedule\":{\"type\":\"string\",\"description\":\"Crontab expression or @hourly/@daily/@weekly (required for add, optional for update)\"},"
      "\"command\":{\"type\":\"string\",\"description\":\"Command to run (required for add, optional for update)\"},"
      "\"context_from\":{\"type\":\"string\",\"description\":\"Chain input: job name whose output becomes context for this job\"},"
      "\"notify_on_complete\":{\"type\":\"boolean\",\"description\":\"Send notification on successful completion\"},"
      "\"notify_on_failure\":{\"type\":\"boolean\",\"description\":\"Send notification on job failure\"},"
      "\"retry\":{\"type\":\"integer\",\"description\":\"Max retries on failure (0=no retry)\",\"default\":0},"
      "\"backoff\":{\"type\":\"integer\",\"description\":\"Base backoff seconds between retries (exponential)\",\"default\":60}"
    "},"
    "\"required\":[\"action\"]"
"}";

/* Forward declarations from scheduler.c */
bool cron_add_job(const char *name, const char *schedule_expr, const char *command);
void cron_remove_job(const char *name);
char *cron_list_jobs(void);
void cron_run_job(const char *name, const char *command);

/* Forward declarations from cron_extras.c */
bool cron_job_set_retry(const char *job_name, int max_retries, int backoff_sec);
bool cron_notify_on_complete(const char *job_name, bool enabled);
bool cron_notify_on_failure(const char *job_name, bool enabled);
bool cron_chain_set_context(const char *job_name, const char *context_from);

/* Forward declarations from cron_sqlite.c for pause/resume/run */
struct cron_sqlite_store_t;
extern struct cron_sqlite_store_t *g_cron_store;
bool cron_sqlite_update_job(struct cron_sqlite_store_t *store, const char *name,
                             const char *field, const char *value);
char *cron_sqlite_get_command(struct cron_sqlite_store_t *store, const char *name);

/* F28: Validate cron schedule expression using libcron */
#include "../lib/libcron/cron.h"  /* for cron_parse */

static const char *cron_validate_schedule(const char *schedule) {
    if (!schedule || !schedule[0])
        return "Schedule expression is empty";

    /* Allow @-prefixed specials without full parse */
    if (schedule[0] == '@') {
        if (strcmp(schedule, "@hourly") == 0 ||
            strcmp(schedule, "@daily") == 0 ||
            strcmp(schedule, "@weekly") == 0 ||
            strcmp(schedule, "@monthly") == 0 ||
            strcmp(schedule, "@yearly") == 0) {
            return NULL; /* valid */
        }
        return "Unknown @-schedule (use @hourly/@daily/@weekly/@monthly/@yearly)";
    }

    /* Parse standard cron expression */
    char *err_msg = NULL;
    cron_expr_t *cexpr = cron_parse(schedule, &err_msg);
    if (!cexpr) {
        static char err_buf[256];
        snprintf(err_buf, sizeof(err_buf), "Invalid schedule: %s",
                 err_msg ? err_msg : "parse error");
        free(err_msg);
        return err_buf;
    }
    cron_free(cexpr);
    free(err_msg);
    return NULL; /* valid */
}

char *cronjob_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *action = json_object_get_string(args, "action", "list");
    json_node_t *result = json_new_object();

    if (strcmp(action, "list") == 0) {
        char *list_json = cron_list_jobs();
        if (list_json) {
            char *pe = NULL;
            json_node_t *list = json_parse(list_json, &pe);
            if (list) {
                json_object_set(result, "jobs", list);
            } else {
                json_object_set(result, "jobs", json_new_array());
                free(pe);
            }
            free(list_json);
        } else {
            json_object_set(result, "jobs", json_new_array());
        }

    } else if (strcmp(action, "add") == 0) {
        const char *name = json_object_get_string(args, "name", NULL);
        const char *schedule = json_object_get_string(args, "schedule", NULL);
        const char *command = json_object_get_string(args, "command", NULL);

        if (!name || !schedule) {
            json_object_set(result, "error", json_new_string("Missing name or schedule"));
        } else {
            /* F28: Validate schedule expression before adding */
            const char *validation_error = cron_validate_schedule(schedule);
            if (validation_error) {
                json_object_set(result, "status", json_new_string("error"));
                json_object_set(result, "error", json_new_string(validation_error));
            } else {
                bool ok = cron_add_job(name, schedule, command ? command : "");
                if (ok) {
                    json_object_set(result, "status", json_new_string("added"));

                    /* F26: Configure notifications */
                    if (json_object_get_bool(args, "notify_on_complete", false))
                        cron_notify_on_complete(name, true);
                    if (json_object_get_bool(args, "notify_on_failure", true))
                        cron_notify_on_failure(name, true);

                    /* F29: Configure retry */
                    int retry = (int)json_object_get_number(args, "retry", 0);
                    int backoff = (int)json_object_get_number(args, "backoff", 60);
                    if (retry > 0) {
                        if (cron_job_set_retry(name, retry, backoff)) {
                            json_object_set(result, "retry", json_new_number((double)retry));
                            json_object_set(result, "backoff_sec", json_new_number((double)backoff));
                        }
                    }

                    /* F27: Configure job chaining */
                    const char *context_from = json_object_get_string(args, "context_from", NULL);
                    if (context_from && context_from[0]) {
                        if (cron_chain_set_context(name, context_from)) {
                            json_object_set(result, "context_from", json_new_string(context_from));
                        }
                    }
                } else {
                    json_object_set(result, "status", json_new_string("error"));
                    json_object_set(result, "error", json_new_string("Failed to add job"));
                }
            }
        }

    } else if (strcmp(action, "remove") == 0) {
        const char *name = json_object_get_string(args, "name", NULL);
        if (!name) {
            json_object_set(result, "error", json_new_string("Missing name"));
        } else {
            cron_remove_job(name);
            json_object_set(result, "status", json_new_string("removed"));
        }

    } else if (strcmp(action, "config") == 0) {
        const char *name = json_object_get_string(args, "name", NULL);
        if (!name) {
            json_object_set(result, "error", json_new_string("Missing name"));
        } else {
            /* Toggle notification and retry settings for existing job */
            bool has_notify_complete = json_obj_get(args, "notify_on_complete") != NULL;
            bool has_notify_failure = json_obj_get(args, "notify_on_failure") != NULL;
            bool has_retry = json_obj_get(args, "retry") != NULL;

            if (has_notify_complete)
                cron_notify_on_complete(name, json_object_get_bool(args, "notify_on_complete", false));
            if (has_notify_failure)
                cron_notify_on_failure(name, json_object_get_bool(args, "notify_on_failure", true));
            if (has_retry) {
                int retry = (int)json_object_get_number(args, "retry", 0);
                int backoff = (int)json_object_get_number(args, "backoff", 60);
                cron_job_set_retry(name, retry, backoff);
            }

            json_object_set(result, "status", json_new_string("configured"));
        }

    } else if (strcmp(action, "update") == 0) {
        const char *name = json_object_get_string(args, "name", NULL);
        if (!name) {
            json_object_set(result, "error", json_new_string("Missing name"));
        } else {
            bool any_update = false;
            /* Update schedule if provided */
            const char *schedule = json_object_get_string(args, "schedule", NULL);
            if (schedule && schedule[0]) {
                const char *validation_error = cron_validate_schedule(schedule);
                if (validation_error) {
                    json_object_set(result, "error", json_new_string(validation_error));
                } else {
                    any_update = cron_sqlite_update_job(g_cron_store, name, "schedule", schedule);
                }
            }
            /* Update command (prompt/script) if provided */
            const char *command = json_object_get_string(args, "command", NULL);
            if (command && command[0]) {
                if (cron_sqlite_update_job(g_cron_store, name, "command", command))
                    any_update = true;
            }
            /* Update notification settings using cron_extras API */
            if (json_obj_get(args, "notify_on_complete") != NULL) {
                bool val = json_object_get_bool(args, "notify_on_complete", false);
                if (cron_notify_on_complete(name, val))
                    any_update = true;
                cron_sqlite_update_job(g_cron_store, name, "notify_on_complete", val ? "true" : "false");
            }
            if (json_obj_get(args, "notify_on_failure") != NULL) {
                bool val = json_object_get_bool(args, "notify_on_failure", true);
                if (cron_notify_on_failure(name, val))
                    any_update = true;
                cron_sqlite_update_job(g_cron_store, name, "notify_on_failure", val ? "true" : "false");
            }
            /* Update retry settings using cron_extras API */
            if (json_obj_get(args, "retry") != NULL) {
                int retry = (int)json_object_get_number(args, "retry", 0);
                int backoff = (int)json_object_get_number(args, "backoff", 60);
                if (cron_job_set_retry(name, retry, backoff))
                    any_update = true;
                char buf[32];
                snprintf(buf, sizeof(buf), "%d", retry);
                cron_sqlite_update_job(g_cron_store, name, "max_retries", buf);
                snprintf(buf, sizeof(buf), "%d", backoff);
                cron_sqlite_update_job(g_cron_store, name, "backoff", buf);
            }
            /* Update context_from (job chaining) */
            if (json_obj_get(args, "context_from") != NULL) {
                const char *cf = json_object_get_string(args, "context_from", NULL);
                if (cf && cf[0]) {
                    if (cron_chain_set_context(name, cf))
                        any_update = true;
                    cron_sqlite_update_job(g_cron_store, name, "chain_from", cf);
                }
            }
            if (any_update) {
                json_object_set(result, "status", json_new_string("updated"));
            } else {
                /* Schedule validation error may have already set error */
                if (!json_obj_get(result, "error"))
                    json_object_set(result, "error", json_new_string("No changes or job not found"));
            }
        }

    } else if (strcmp(action, "pause") == 0) {
        const char *name = json_object_get_string(args, "name", NULL);
        if (!name) {
            json_object_set(result, "error", json_new_string("Missing name"));
        } else {
            bool ok = cron_sqlite_update_job(g_cron_store, name, "active", "false");
            json_object_set(result, "status", json_new_string(ok ? "paused" : "error"));
            if (!ok) json_object_set(result, "error", json_new_string("Job not found"));
        }

    } else if (strcmp(action, "resume") == 0) {
        const char *name = json_object_get_string(args, "name", NULL);
        if (!name) {
            json_object_set(result, "error", json_new_string("Missing name"));
        } else {
            bool ok = cron_sqlite_update_job(g_cron_store, name, "active", "true");
            json_object_set(result, "status", json_new_string(ok ? "resumed" : "error"));
            if (!ok) json_object_set(result, "error", json_new_string("Job not found"));
        }

    } else if (strcmp(action, "run") == 0) {
        const char *name = json_object_get_string(args, "name", NULL);
        if (!name) {
            json_object_set(result, "error", json_new_string("Missing name"));
        } else {
            char *command = cron_sqlite_get_command(g_cron_store, name);
            if (command) {
                cron_run_job(name, command);
                free(command);
                json_object_set(result, "status", json_new_string("triggered"));
            } else {
                json_object_set(result, "status", json_new_string("error"));
                json_object_set(result, "error", json_new_string("Job not found"));
            }
        }

    } else {
        json_object_set(result, "error", json_new_string("Unknown action"));
    }

    char *json_out = json_serialize(result);
    json_free(result);
    json_free(args);
    return json_out;
}

void registry_init_cronjob(void) {
    registry_register("cronjob",
        "Manage cron jobs. Actions: list (show all jobs), add (schedule a new job), "
        "remove (delete a job), config (update notification/retry settings), "
        "update (edit schedule, command, notification, retry, chaining of an existing job), "
        "pause (disable without removing), resume (re-enable), run (trigger immediately). "
        "Supports schedule validation, per-job notifications (notify_on_complete, "
        "notify_on_failure), and automatic retry with exponential backoff.",
        SCHEMA, cronjob_handler);
}
