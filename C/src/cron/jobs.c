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

/* Forward declarations for pause/resume — provided by cron_sqlite.c */
struct cron_sqlite_store_t;
extern struct cron_sqlite_store_t *g_cron_store;
bool cron_sqlite_update_job(struct cron_sqlite_store_t *store, const char *name,
                             const char *field, const char *value);

/* Forward declarations from cron_extras.c */
void cron_chain_store_output(const char *job_name, const char *output);
const char *cron_chain_get_context(const char *job_name);
char *cron_chain_get_output(const char *job_name);

/* List all jobs */
char *cron_list_jobs(void) {
    /* Would iterate g_jobs from scheduler.c */
    /* For minimal impl: return empty list */
    return strdup("[]");
}

/*
 * cron_run_job — Execute a cron job command, capture output, chain context.
 * If the job has a chain_from entry, the source job's output is prepended
 * as context for the command via HERMES_CHAIN_CONTEXT env var.
 * Output is stored in the chain state for downstream chained jobs.
 */
void cron_run_job(const char *name, const char *command) {
    if (!name || !command || !command[0]) return;

    /* Build the actual command with chained context if applicable */
    char full_cmd[8192];
    const char *chain_from = cron_chain_get_context(name);
    if (chain_from && chain_from[0]) {
        /* Fetch source job output for context injection */
        char *source_output = cron_chain_get_output(chain_from);
        if (source_output) {
            /* Inject via env var — agent tools/C cli can read HERMES_CHAIN_CONTEXT */
            char escaped[4096];
            /* Simple escape: replace " with \", then wrap in double-quotes */
            int ei = 0;
            for (int si = 0; source_output[si] && ei < (int)sizeof(escaped) - 3; si++) {
                if (source_output[si] == '"' || source_output[si] == '\\')
                    escaped[ei++] = '\\';
                escaped[ei++] = source_output[si];
            }
            escaped[ei] = '\0';
            snprintf(full_cmd, sizeof(full_cmd),
                     "HERMES_CHAIN_CONTEXT=\"%s\" %s", escaped, command);
            free(source_output);
        } else {
            snprintf(full_cmd, sizeof(full_cmd), "%s", command);
        }
    } else {
        snprintf(full_cmd, sizeof(full_cmd), "%s", command);
    }

    /* Execute via popen to capture output */
    char output_buf[4096] = {0};
    FILE *fp = popen(full_cmd, "r");
    if (!fp) {
        fprintf(stderr, "[cron] Failed to execute job '%s'\n", name);
        return;
    }

    size_t total = 0;
    char line[1024];
    while (fgets(line, sizeof(line), fp) && total < sizeof(output_buf) - 1) {
        size_t len = strlen(line);
        if (total + len >= sizeof(output_buf) - 1)
            len = sizeof(output_buf) - 1 - total;
        memcpy(output_buf + total, line, len);
        total += len;
    }
    output_buf[total] = '\0';

    int rc = pclose(fp);

    /* Store output for downstream chained jobs */
    if (output_buf[0])
        cron_chain_store_output(name, output_buf);

    printf("[cron] Job '%s' exit: %d, output: %zu bytes\n", name, rc, total);
}
