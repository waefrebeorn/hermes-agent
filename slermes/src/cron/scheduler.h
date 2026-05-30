/*
 * scheduler.h — Internal header for Hermes C cron scheduler.
 * Exposes scheduler structures for cron_cli.c, cron_extras.c, etc.
 */

#ifndef CRON_SCHEDULER_H
#define CRON_SCHEDULER_H

#include "hermes.h"
#include <time.h>

/* ================================================================
 *  Cron expression parsing (libcron-based)
 * ================================================================ */

#include "cron.h"  /* lib/libcron/cron.h */

/* ================================================================
 *  Internal schedule types
 * ================================================================ */

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

/* ================================================================
 *  Job entry structure (matches cron_sqlite.c)
 * ================================================================ */

#define MAX_JOBS_STORED 256

typedef struct {
    char name[128];
    char schedule[128];
    char command[2048];
    bool active;
    int  retry_count;
    int  max_retries;
    int  backoff_sec;
    char chain_from[128];
    char template_name[128];
    char script_type[32];
    char interpreter[64];
    char notify_channel[128];
    bool notify_on_complete;
    bool notify_on_failure;
    time_t last_run;
    time_t created_at;
} cron_job_entry_t;

/* ================================================================
 *  Global state (defined in scheduler.c)
 * ================================================================ */

extern cron_sqlite_store_t *g_cron_store;

/* ================================================================
 *  Internal functions
 * ================================================================ */

/* Schedule parsing (exposed for testing) */
bool parse_cron_field(const char *str, int *value, int *interval);
bool parse_schedule(const char *expr, cron_schedule_t *sched);
bool should_run(cron_schedule_t *sched, time_t now, time_t last_run);

/* Run a single job inline */
void cron_run_job(const char *name, const char *command);

/* Check if job should run now */
bool cron_should_run(const char *schedule_expr, time_t now, time_t last_run);

/* Get store functions */
cron_sqlite_store_t *cron_sqlite_open(const char *path);
void cron_sqlite_close(cron_sqlite_store_t *store);
bool cron_sqlite_save_job(cron_sqlite_store_t *store, const char *name,
                           const char *schedule, const char *command,
                           bool active, int retry_count, int max_retries,
                           const char *chain_from, const char *template_name,
                           const char *script_type);
bool cron_sqlite_load_jobs(cron_sqlite_store_t *store);
bool cron_sqlite_delete_job(cron_sqlite_store_t *store, const char *name);
bool cron_sqlite_update_job(cron_sqlite_store_t *store, const char *name,
                             const char *field, const char *value);
int cron_sqlite_count(cron_sqlite_store_t *store);
cron_job_entry_t *cron_sqlite_get(cron_sqlite_store_t *store, int index);
cron_job_entry_t *cron_sqlite_find(cron_sqlite_store_t *store, const char *name);

/* Retry */
bool cron_job_increment_retry(const char *job_name);
void cron_job_reset_retry(const char *job_name);
bool cron_send_notification(const char *job_name, const char *status,
                             const char *message);

/* P177: Script-based jobs */
char *cron_run_script(const char *script_path, const char *interpreter,
                       const char *script_args, int *exit_code);

#endif /* CRON_SCHEDULER_H */
