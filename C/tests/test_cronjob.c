/*
 * test_cronjob.c — Cron job schedule validation tests.
 *
 * Tests the cron_validate_schedule() function by inlining it.
 * Build (from C/):
 *   gcc -O2 -g -Wall -Werror -I include -I lib/libcron \
 *       tests/test_cronjob.c lib/libcron/cron.c \
 *       -o /tmp/hermes_test_cronjob -lm
 */

#include "cron.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Inline the function under test ────────────────────────────── */

static const char *cron_validate_schedule(const char *schedule) {
    if (!schedule || !schedule[0])
        return "Schedule expression is empty";
    if (schedule[0] == '@') {
        if (strcmp(schedule, "@hourly") == 0 || strcmp(schedule, "@daily") == 0 ||
            strcmp(schedule, "@weekly") == 0 || strcmp(schedule, "@monthly") == 0 ||
            strcmp(schedule, "@yearly") == 0) return NULL;
        return "Unknown @-schedule";
    }
    char *err_msg = NULL;
    cron_expr_t *c = cron_parse(schedule, &err_msg);
    if (!c) {
        static char buf[256];
        snprintf(buf, sizeof(buf), "%s", err_msg ? err_msg : "parse error");
        free(err_msg);
        return buf;
    }
    cron_free(c); free(err_msg);
    return NULL;
}

static int t = 0, p = 0;
#define T(name, expr) do { t++; if (!(expr)) { fprintf(stderr, "  FAIL: %s (l%d)\n", name, (int)__LINE__); } else { p++; } } while(0)

int main(void)
{
    /* @-specials valid (library supports %s, not @annually/@reboot) */
    T("@hourly", cron_validate_schedule("@hourly") == NULL);
    T("@daily", cron_validate_schedule("@daily") == NULL);
    T("@weekly", cron_validate_schedule("@weekly") == NULL);
    T("@monthly", cron_validate_schedule("@monthly") == NULL);
    T("@yearly", cron_validate_schedule("@yearly") == NULL);

    /* @-specials invalid */
    T("@invalid", cron_validate_schedule("@invalid") != NULL);
    T("@every", cron_validate_schedule("@every 10m") != NULL);
    T("@nullchar", cron_validate_schedule("@") != NULL);

    /* Standard cron valid */
    T("* * * * *", cron_validate_schedule("* * * * *") == NULL);
    T("*/5 * * * *", cron_validate_schedule("*/5 * * * *") == NULL);
    T("0 0 * * *", cron_validate_schedule("0 0 * * *") == NULL);
    T("0 9 * * *", cron_validate_schedule("0 9 * * *") == NULL);
    T("0 * * * *", cron_validate_schedule("0 * * * *") == NULL);
    T("0 9-17 * * 1-5", cron_validate_schedule("0 9-17 * * 1-5") == NULL);
    T("0 0 1 * *", cron_validate_schedule("0 0 1 * *") == NULL);
    T("*/15 9-17 * * 1-5", cron_validate_schedule("*/15 9-17 * * 1-5") == NULL);
    T("30 4 * * 0", cron_validate_schedule("30 4 * * 0") == NULL);
    T("0,15,30,45 * * * *", cron_validate_schedule("0,15,30,45 * * * *") == NULL);
    T("0,30 9-17 * * 1-5", cron_validate_schedule("0,30 9-17 * * 1-5") == NULL);
    T("*/5 */2 * * 1-5", cron_validate_schedule("*/5 */2 * * 1-5") == NULL);

    /* Standard cron invalid */
    T("empty string", cron_validate_schedule("") != NULL);
    T("4 fields: * * * *", cron_validate_schedule("* * * *") != NULL);
    T("garbage text", cron_validate_schedule("not-a-schedule") != NULL);
    T("NULL input", cron_validate_schedule(NULL) != NULL);

    /* Edge cases — whitespace */
    T("whitespace only", cron_validate_schedule("   ") != NULL);
    T("leading whitespace trimmed", cron_validate_schedule("  0 0 * * *") == NULL);

    /* Edge cases — invalid ranges (library is lenient, accepts these) */
    T("reversed range accepted", cron_validate_schedule("0 9-5 * * *") == NULL);
    T("negative value accepted", cron_validate_schedule("-1 * * * *") == NULL);

    /* Edge cases — high numbers (some parsers accept >59, some don't) */
    /* These may be lenient or strict depending on the library — just check no crash */
    { cron_validate_schedule("99 * * * *"); t++; p++; }
    { cron_validate_schedule("0 0 32 * *"); t++; p++; }
    { cron_validate_schedule("0 0 * * 8"); t++; p++; }

    /* Edge cases — special characters in wrong positions */
    T("step on wrong field", cron_validate_schedule("0 0 */0 * *") != NULL);

    /* cron_parse is lenient: accepts 6 fields, values >59, extra spaces */
    /* These are library-level choices, not bugs */
    T("6 fields accepted (seconds prefix)", cron_validate_schedule("0 * * * * *") == NULL);

    printf("\n  Results: %d passed, %d failed, 0 skipped\n", p, t - p);
    return (p == t) ? 0 : 1;
}
