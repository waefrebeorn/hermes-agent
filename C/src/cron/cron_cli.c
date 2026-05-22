/*
 * cron_cli.c — P176: Scheduler CLI handler for Hermes C cron.
 *
 * Full /cron command support: list, add, edit, remove, pause, resume, run-now.
 * Returns JSON responses for agent integration.
 */

#include "hermes.h"
#include "hermes_json.h"
#include "../cron/scheduler.h" /* Internal header for scheduler state */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations from scheduler.c */
extern cron_sqlite_store_t *g_cron_store;
void cron_run_job(const char *name, const char *command);

/* ================================================================
 *  CLI Command Handler
 * ================================================================ */

char *cron_cmd_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *action = json_get_str(args, "action", "list");
    const char *name = json_get_str(args, "name", NULL);
    const char *schedule = json_get_str(args, "schedule", NULL);
    const char *command = json_get_str(args, "command", NULL);
    const char *field = json_get_str(args, "field", NULL);
    const char *value = json_get_str(args, "value", NULL);

    json_t *result = json_object();
    json_set(result, "action", json_string(action));

    if (strcmp(action, "list") == 0) {
        json_t *jobs = json_array();
        if (g_cron_store) {
            int count = cron_sqlite_count(g_cron_store);
            for (int i = 0; i < count; i++) {
                cron_job_entry_t *e = cron_sqlite_get(g_cron_store, i);
                if (!e) continue;
                json_t *j = json_object();
                json_set(j, "name", json_string(e->name));
                json_set(j, "schedule", json_string(e->schedule));
                json_set(j, "command", json_string(e->command));
                json_set(j, "active", json_bool(e->active));
                json_set(j, "retry_count", json_number(e->retry_count));
                json_set(j, "max_retries", json_number(e->max_retries));
                json_set(j, "last_run", json_number((double)e->last_run));
                json_set(j, "chain_from", json_string(e->chain_from));
                json_set(j, "template_name", json_string(e->template_name));
                json_set(j, "script_type", json_string(e->script_type));
                json_append(jobs, j);
            }
        }
        json_set(result, "jobs", jobs);
        json_set(result, "status", json_string("ok"));

    } else if (strcmp(action, "add") == 0) {
        if (!name || !schedule) {
            json_set(result, "error", json_string("Missing name or schedule"));
            json_set(result, "status", json_string("error"));
        } else {
            bool ok = cron_sqlite_save_job(g_cron_store, name, schedule,
                                            command ? command : "",
                                            true, 0, 0, NULL, NULL, NULL);
            if (ok) {
                json_set(result, "status", json_string("added"));
            } else {
                json_set(result, "status", json_string("error"));
                json_set(result, "error", json_string("Failed to add job"));
            }
        }

    } else if (strcmp(action, "edit") == 0) {
        if (!name || !field) {
            json_set(result, "error", json_string("Missing name or field"));
            json_set(result, "status", json_string("error"));
        } else {
            bool ok = cron_sqlite_update_job(g_cron_store, name, field, value);
            json_set(result, "status", json_string(ok ? "updated" : "error"));
            if (!ok) json_set(result, "error", json_string("Job or field not found"));
        }

    } else if (strcmp(action, "remove") == 0) {
        if (!name) {
            json_set(result, "error", json_string("Missing name"));
            json_set(result, "status", json_string("error"));
        } else {
            bool ok = cron_sqlite_delete_job(g_cron_store, name);
            json_set(result, "status", json_string(ok ? "removed" : "error"));
            if (!ok) json_set(result, "error", json_string("Job not found"));
        }

    } else if (strcmp(action, "pause") == 0) {
        if (!name) {
            json_set(result, "error", json_string("Missing name"));
            json_set(result, "status", json_string("error"));
        } else {
            bool ok = cron_sqlite_update_job(g_cron_store, name, "active", "false");
            json_set(result, "status", json_string(ok ? "paused" : "error"));
        }

    } else if (strcmp(action, "resume") == 0) {
        if (!name) {
            json_set(result, "error", json_string("Missing name"));
            json_set(result, "status", json_string("error"));
        } else {
            bool ok = cron_sqlite_update_job(g_cron_store, name, "active", "true");
            json_set(result, "status", json_string(ok ? "resumed" : "error"));
        }

    } else if (strcmp(action, "run-now") == 0) {
        if (!name) {
            json_set(result, "error", json_string("Missing name"));
            json_set(result, "status", json_string("error"));
        } else {
            /* Find job and run it */
            cron_job_entry_t *e = cron_sqlite_find(g_cron_store, name);
            if (e) {
                cron_run_job(name, e->command);
                json_set(result, "status", json_string("triggered"));
            } else {
                json_set(result, "status", json_string("error"));
                json_set(result, "error", json_string("Job not found"));
            }
        }

    } else {
        json_set(result, "error", json_string("Unknown action"));
        json_set(result, "status", json_string("error"));
    }

    char *json_out = json_serialize(result);
    json_free(result);
    json_free(args);
    return json_out;
}

/* Register cron CLI tool */
void registry_init_cron_cmd(void) {
    registry_register("cron_cmd",
        "Manage cron jobs. Actions: list, add (name, schedule, command), "
        "edit (name, field, value), remove (name), pause (name), "
        "resume (name), run-now (name).",
        "{"
        "\"type\":\"object\","
        "\"properties\":{"
          "\"action\":{\"type\":\"string\",\"description\":\"list|add|edit|remove|pause|resume|run-now\"},"
          "\"name\":{\"type\":\"string\",\"description\":\"Job name\"},"
          "\"schedule\":{\"type\":\"string\",\"description\":\"Cron expression\"},"
          "\"command\":{\"type\":\"string\",\"description\":\"Command to run\"},"
          "\"field\":{\"type\":\"string\",\"description\":\"Field to edit (e.g., schedule, command, active)\"},"
          "\"value\":{\"type\":\"string\",\"description\":\"New value\"}"
        "},"
        "\"required\":[\"action\"]"
        "}",
        cron_cmd_handler);
}
