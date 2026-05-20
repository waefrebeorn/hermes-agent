/*
 * cronjob.c — Cron job scheduling tool for Hermes C.
 * Wraps the cron scheduler: list, add, remove jobs.
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Schema */
static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"action\":{\"type\":\"string\",\"description\":\"list | add | remove\"},"
      "\"name\":{\"type\":\"string\",\"description\":\"Job name (required for add/remove)\"},"
      "\"schedule\":{\"type\":\"string\",\"description\":\"Crontab expression or @hourly/@daily/@weekly (required for add)\"},"
      "\"command\":{\"type\":\"string\",\"description\":\"Command to run (required for add)\"}"
    "},"
    "\"required\":[\"action\"]"
"}";

/* Forward declarations from scheduler.c */
bool cron_add_job(const char *name, const char *schedule_expr, const char *command);
void cron_remove_job(const char *name);
char *cron_list_jobs(void);

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
            bool ok = cron_add_job(name, schedule, command ? command : "");
            json_object_set(result, "status", json_new_string(ok ? "added" : "error"));
            if (!ok) json_object_set(result, "error", json_new_string("Failed to add job"));
        }
    } else if (strcmp(action, "remove") == 0) {
        const char *name = json_object_get_string(args, "name", NULL);
        if (!name) {
            json_object_set(result, "error", json_new_string("Missing name"));
        } else {
            cron_remove_job(name);
            json_object_set(result, "status", json_new_string("removed"));
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
        "Manage cron jobs. Actions: list (show all jobs), add (schedule a new job), remove (delete a job). "
        "Schedule uses crontab format: 'min hour day month weekday' or @hourly/@daily/@weekly.",
        SCHEMA, cronjob_handler);
}
