#ifndef LIBCRON_H
#define LIBCRON_H

/*
 * libcron.h — Standalone cron expression parser for C.
 * Zero external deps. Replaces Python's croniter.
 *
 * MIT License — WuBu Hermes Project
 *
 * Format: 'min hour dom month dow'
 *   min:   0-59, * (wildcard), /2 (step every 2)
 *   hour:  0-23, *
 *   dom:   1-31, *
 *   month: 1-12, * (or JAN-DEC)
 *   dow:   0-6, *  (0=Sun, 1=Mon, or SUN-SAT)
 *
 * Special: '@hourly' '@daily' '@weekly' '@monthly' '@yearly'
 *
 * Usage:
 *   cron_expr_t *c = cron_parse("0 0 * * *", NULL);
 *   struct tm tm = ...;
 *   if (cron_match(c, &tm)) { ... }
 *   cron_free(c);
 */

#include <stddef.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cron_expr_t cron_expr_t;

/* Parse cron expression. Returns NULL on error. */
cron_expr_t *cron_parse(const char *expr, char **error_msg);

/* Check if a time matches. */
bool cron_match(const cron_expr_t *cron, const struct tm *tm);

/* Get next matching time. Returns false if beyond year 2099. */
bool cron_next(const cron_expr_t *cron, const struct tm *from, struct tm *out);

/* Describe expression. Caller free(). */
char *cron_describe(const cron_expr_t *cron);

/* Free expression. */
void cron_free(cron_expr_t *cron);

#ifdef __cplusplus
}
#endif

#endif /* LIBCRON_H */
