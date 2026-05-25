/*
 * scheduler.c — Minimal cron scheduler for Hermes C.
 * Parses crontab-style schedules, manages job ticks.
 * Phase 5: basic schedule parsing + execution loop.
 * Supports JSON persistence of jobs to disk.
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Simple crontab parser: supports slash-N syntax like every 5 min */
typedef enum { MINUTE, HOUR, DAY, MONTH, WEEKDAY } crontab_field_t;

typedef struct {
    int minute;   /* -1 = every */
    int hour;
    int day;
    int month;
    int weekday;
    int interval_minutes; /* slash-N format aka every N minutes */
} cron_schedule_t;

typedef struct cron_job_t {
    char             name[128];
    char             command[1024];
    char             schedule_expr[256]; /* original expression for save/display */
    cron_schedule_t  schedule;
    time_t           last_run;
    bool             active;
    struct cron_job_t *next;
} cron_job_t;

static cron_job_t *g_jobs = NULL;

/* Forward declarations for functions defined below */
bool cron_save_jobs(void);

/* ================================================================
 *  Schedule parsing
 * ================================================================ */

static bool parse_cron_field(const char *str, int *value, int *interval) {
    *interval = 0;
    if (!str || !*str) return false;

    if (str[0] == '*' && str[1] == '/') {
        *interval = atoi(str + 2);
        if (*interval <= 0) *interval = 5;
        *value = -1;
        return true;
    }
    if (str[0] == '*') {
        *value = -1;
        return true;
    }
    *value = atoi(str);
    return true;
}

static bool parse_schedule(const char *expr, cron_schedule_t *sched) {
    memset(sched, 0, sizeof(*sched));
    if (!expr) return false;

    char copy[256];
    snprintf(copy, sizeof(copy), "%s", expr);

    char *fields[5];
    int n = 0;
    char *tok = strtok(copy, " \t");
    while (tok && n < 5) {
        fields[n++] = tok;
        tok = strtok(NULL, " \t");
    }
    if (n < 5) return false;

    int interval = 0;
    parse_cron_field(fields[0], &sched->minute, &interval);
    sched->interval_minutes = interval;
    parse_cron_field(fields[1], &sched->hour, &interval);
    parse_cron_field(fields[2], &sched->day, &interval);
    parse_cron_field(fields[3], &sched->month, &interval);
    parse_cron_field(fields[4], &sched->weekday, &interval);
    return true;
}

static bool should_run(cron_schedule_t *sched, time_t now, time_t last_run) {
    struct tm *tm = localtime(&now);
    if (!tm) return false;

    /* Check interval mode */
    if (sched->interval_minutes > 0) {
        if (last_run == 0) return true;
        return difftime(now, last_run) >= sched->interval_minutes * 60;
    }

    /* Check all fields */
    if (sched->minute >= 0 && sched->minute != tm->tm_min) return false;
    if (sched->hour >= 0 && sched->hour != tm->tm_hour) return false;
    if (sched->day >= 0 && sched->day != tm->tm_mday) return false;
    if (sched->month >= 0 && sched->month != tm->tm_mon + 1) return false;
    if (sched->weekday >= 0 && sched->weekday != tm->tm_wday) return false;

    return true;
}

/* ================================================================
 *  Job management
 * ================================================================ */

bool cron_add_job(const char *name, const char *schedule_expr,
                   const char *command)
{
    if (!name || !schedule_expr) return false;

    cron_job_t *job = (cron_job_t *)calloc(1, sizeof(cron_job_t));
    if (!job) return false;

    snprintf(job->name, sizeof(job->name), "%s", name);
    snprintf(job->command, sizeof(job->command), "%s", command ? command : "");
    snprintf(job->schedule_expr, sizeof(job->schedule_expr), "%s", schedule_expr);
    job->active = true;
    job->last_run = 0;

    if (!parse_schedule(schedule_expr, &job->schedule)) {
        free(job);
        return false;
    }

    /* Add to list */
    job->next = g_jobs;
    g_jobs = job;
    cron_save_jobs();
    return true;
}

void cron_remove_job(const char *name) {
    cron_job_t **pp = &g_jobs;
    while (*pp) {
        cron_job_t *job = *pp;
        if (strcmp(job->name, name) == 0) {
            *pp = job->next;
            free(job);
            cron_save_jobs();
            return;
        }
        pp = &job->next;
    }
}

/* ================================================================
 *  Persistence (save/load jobs JSON)
 * ================================================================ */

#define CRON_JOBS_FILE "cron_jobs.json"

static char jobs_path[4096] = "";

static void cron_set_path(void) {
    if (jobs_path[0]) return;
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = ".";
    snprintf(jobs_path, sizeof(jobs_path), "%s/%s", home, CRON_JOBS_FILE);
}

bool cron_save_jobs(void) {
    cron_set_path();
    json_node_t *arr = json_new_array();

    for (cron_job_t *job = g_jobs; job; job = job->next) {
        json_node_t *j = json_new_object();
        json_object_set(j, "name", json_new_string(job->name));
        json_object_set(j, "schedule", json_new_string(job->schedule_expr));
        json_object_set(j, "command", json_new_string(job->command));
        json_object_set(j, "active", json_new_bool(job->active));
        json_object_set(j, "last_run", json_new_number((double)job->last_run));
        json_array_append(arr, j);
    }

    char *json_str = json_serialize(arr);
    json_free(arr);
    if (!json_str) return false;

    FILE *f = fopen(jobs_path, "w");
    if (!f) { free(json_str); return false; }
    fputs(json_str, f);
    fclose(f);
    free(json_str);
    return true;
}

bool cron_load_jobs(void) {
    cron_set_path();
    FILE *f = fopen(jobs_path, "r");
    if (!f) return false;  /* No file yet, not an error */

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz <= 0) { fclose(f); return true; }

    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return false; }
    size_t r = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[r] = '\0';

    char *err = NULL;
    json_node_t *arr = json_parse(buf, &err);
    free(buf);
    if (!arr) { free(err); return false; }

    time_t now = time(NULL);

    for (size_t i = 0; i < (size_t)json_array_count(arr); i++) {
        json_node_t *j = json_array_get(arr, i);
        if (!j) continue;

        const char *name = json_object_get_string(j, "name", "unknown");
        const char *schedule = json_object_get_string(j, "schedule", "0 * * * *");
        const char *command = json_object_get_string(j, "command", "");
        bool active = json_object_get_bool(j, "active", true);

        if (cron_add_job(name, schedule, command)) {
            /* Set last_run so jobs don't all fire immediately on restart */
            cron_job_t *job = g_jobs;  /* just added, it's at head */
            if (job) {
                job->last_run = now;
                double lr = json_object_get_number(j, "last_run", (double)now);
                if (lr < (double)now && lr > 0)
                    job->last_run = (time_t)lr;
                job->active = active;
            }
        }
    }

    json_free(arr);
    return true;
}

/* Return JSON array of all jobs (for listing) */
char *cron_get_jobs_json(void) {
    json_node_t *arr = json_new_array();
    for (cron_job_t *job = g_jobs; job; job = job->next) {
        json_node_t *j = json_new_object();
        json_object_set(j, "name", json_new_string(job->name));
        json_object_set(j, "schedule", json_new_string(job->schedule_expr));
        json_object_set(j, "command", json_new_string(job->command));
        json_object_set(j, "active", json_new_bool(job->active));
        json_object_set(j, "last_run", json_new_number((double)job->last_run));
        json_array_append(arr, j);
    }
    char *json_str = json_serialize(arr);
    json_free(arr);
    return json_str;
}

/* ================================================================
 *  Scheduler loop
 * ================================================================ */

int cron_run_loop(int interval_sec) {
    printf("[cron] Scheduler started (interval: %ds)\n", interval_sec);

    while (true) {
        time_t now = time(NULL);

        for (cron_job_t *job = g_jobs; job; job = job->next) {
            if (!job->active) continue;
            if (should_run(&job->schedule, now, job->last_run)) {
                printf("[cron] Running job: %s\n", job->name);
                job->last_run = now;

                if (job->command[0]) {
                    int rc = system(job->command);
                    printf("[cron] Job '%s' exit: %d\n", job->name, rc);
                }
            }
        }

        sleep(interval_sec > 0 ? interval_sec : 60);
    }

    return 0;
}

/* ================================================================
 *  Legacy entry point
 * ================================================================ */

int hermes_cron_main(int argc, char **argv) {
    (void)argc; (void)argv;

    /* Load persisted jobs from disk */
    cron_load_jobs();

    /* Add default jobs from environment or config */
    const char *jobs_env = getenv("HERMES_CRON_JOBS");
    if (jobs_env) {
        /* Format: "name1|schedule1|cmd1;name2|schedule2|cmd2" */
        char copy[4096];
        snprintf(copy, sizeof(copy), "%s", jobs_env);
        char *job_tok = strtok(copy, ";");
        while (job_tok) {
            char *fields[3];
            int n = 0;
            char *f = strtok(job_tok, "|");
            while (f && n < 3) { fields[n++] = f; f = strtok(NULL, "|"); }
            if (n >= 2)
                cron_add_job(fields[0], fields[1], n >= 3 ? fields[2] : "");
            job_tok = strtok(NULL, ";");
        }
    }

    printf("[cron] WuBu Hermes Cron v%s\n", HERMES_VERSION);
    return cron_run_loop(60);
}
