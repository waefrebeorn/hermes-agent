/*
 * jobs.c — Cron job management for Hermes C.
 * Thin wrapper around scheduler.c for CLI integration.
 */

#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations from scheduler.c */
bool cron_add_job(const char *name, const char *schedule, const char *command);
void cron_remove_job(const char *name);
char *cron_get_jobs_json(void);

/* List all jobs as JSON array */
char *cron_list_jobs(void) {
    return cron_get_jobs_json();
}
