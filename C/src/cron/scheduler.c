/*
 * scheduler.c — Minimal cron scheduler for Hermes C.
 * Parses crontab-style schedules, manages job ticks.
 * Phase 5: basic schedule parsing + execution loop.
 */

#include "hermes.h"
#include "hermes_json.h"
#include "../cron/scheduler.h"
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
    cron_schedule_t  schedule;
    time_t           last_run;
    bool             active;
    struct cron_job_t *next;
} cron_job_t;

static cron_job_t *g_jobs = NULL;

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
    job->active = true;
    job->last_run = 0;

    if (!parse_schedule(schedule_expr, &job->schedule)) {
        free(job);
        return false;
    }

    /* Add to list */
    job->next = g_jobs;
    g_jobs = job;
    return true;
}

void cron_remove_job(const char *name) {
    cron_job_t **pp = &g_jobs;
    while (*pp) {
        cron_job_t *job = *pp;
        if (strcmp(job->name, name) == 0) {
            *pp = job->next;
            free(job);
            return;
        }
        pp = &job->next;
    }
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
                    if (rc == 0)
                        cron_send_notification(job->name, "completed", NULL);
                    else {
                        char msg[64];
                        snprintf(msg, sizeof(msg), "exit code %d", rc);
                        cron_send_notification(job->name, "failed", msg);
                    }
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

    /* Set notification channel from env var (standalone mode) */
    {
        const char *cron_chan = getenv("HERMES_CRON_NOTIFY_CHANNEL");
        if (cron_chan && cron_chan[0])
            cron_notify_set_channel(cron_chan);
    }

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

    /* I08: Exit early with clear message if no jobs configured */
    if (!g_jobs) {
        printf("[cron] No jobs configured. Set HERMES_CRON_JOBS or use /cron add.\n");
        printf("[cron] Example: HERMES_CRON_JOBS=\"ping|*/5 * * * *|curl example.com\"\n");
        return 0;
    }

    printf("[cron] WuBu Slermes Cron v%s\n", HERMES_VERSION);
    return cron_run_loop(60);
}
