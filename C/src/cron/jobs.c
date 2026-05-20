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

/* List all jobs */
char *cron_list_jobs(void) {
    /* Would iterate g_jobs from scheduler.c */
    /* For minimal impl: return empty list */
    return strdup("[]");
}
