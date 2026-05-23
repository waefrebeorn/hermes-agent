/*
 * cron.c — Standalone cron expression parser for C.
 * Parses standard 5-field cron expressions. No external deps.
 * MIT License — WuBu Hermes Project
 */

#include "cron.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ================================================================
 *  Internal field representation
 * ================================================================ */

#define MAX_BITS 60

typedef struct {
    bool bits[MAX_BITS];  /* bitmask for allowed values */
    bool any;             /* wildcard */
} cron_field_t;

struct cron_expr_t {
    cron_field_t minute;   /* 0-59 */
    cron_field_t hour;     /* 0-23 */
    cron_field_t dom;      /* 1-31 */
    cron_field_t month;    /* 1-12 */
    cron_field_t dow;      /* 0-6 (0=Sun) */
    char raw[128];
};

/* ================================================================
 *  Parsing helpers
 * ================================================================ */

static int month_name_to_num(const char *s) {
    static const char *names[] = {"jan","feb","mar","apr","may","jun",
                                   "jul","aug","sep","oct","nov","dec"};
    for (int i = 0; i < 12; i++)
        if (strcasecmp(s, names[i]) == 0) return i + 1;
    return -1;
}

static int dow_name_to_num(const char *s) {
    static const char *names[] = {"sun","mon","tue","wed","thu","fri","sat"};
    for (int i = 0; i < 7; i++)
        if (strcasecmp(s, names[i]) == 0) return i;
    return -1;
}

/* Parse a single field (e.g., "step /5 in *", "1,3,5", "MON-FRI", "*") */
static bool parse_field(const char *str, cron_field_t *f, int min, int max) {
    memset(f, 0, sizeof(*f));

    if (!str || *str == '\0') return false;

    /* Check for wildcard */
    if (str[0] == '*' && str[1] == '\0') {
        f->any = true;
        return true;
    }

    /* Check for step: */
    if (str[0] == '*' && str[1] == '/') {
        int step = atoi(str + 2);
        if (step <= 0) return false;
        for (int i = min; i <= max; i += step) f->bits[i - min] = true;
        return true;
    }

    /* Parse comma-separated list */
    char copy[64];
    strncpy(copy, str, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';

    char *saveptr;
    char *tok = strtok_r(copy, ",", &saveptr);
    while (tok) {
        /* Check for range: 1-5 or MON-FRI */
        char *dash = strchr(tok, '-');
        if (dash) {
            *dash = '\0';
            const char *start_str = tok;
            const char *end_str = dash + 1;

            int start_val, end_val;

            /* Try month/dow names */
            if (isalpha((unsigned char)*start_str)) {
                start_val = month_name_to_num(start_str);
                if (start_val < 0) start_val = dow_name_to_num(start_str);
                if (start_val < 0) return false;
                end_val = month_name_to_num(end_str);
                if (end_val < 0) end_val = dow_name_to_num(end_str);
                if (end_val < 0) return false;
            } else {
                start_val = atoi(start_str);
                end_val = atoi(end_str);
            }

            if (start_val > end_val) { int t = start_val; start_val = end_val; end_val = t; }
            for (int i = start_val; i <= end_val; i++) {
                if (i >= min && i <= max) f->bits[i - min] = true;
            }
        } else {
            /* Single value */
            int val;
            if (isalpha((unsigned char)*tok)) {
                val = month_name_to_num(tok);
                if (val < 0) val = dow_name_to_num(tok);
                if (val < 0) return false;
            } else {
                val = atoi(tok);
            }
            if (val >= min && val <= max) f->bits[val - min] = true;
        }

        tok = strtok_r(NULL, ",", &saveptr);
    }

    return true;
}

/* ================================================================
 *  Main parse
 * ================================================================ */

static bool is_special(const char *expr, const char **result) {
    struct { const char *name; const char *cron; } specials[] = {
        {"@yearly",  "0 0 1 1 *"},
        {"@annually","0 0 1 1 *"},
        {"@monthly", "0 0 1 * *"},
        {"@weekly",  "0 0 * * 0"},
        {"@daily",   "0 0 * * *"},
        {"@hourly",  "0 * * * *"},
        {NULL, NULL}
    };
    for (int i = 0; specials[i].name; i++) {
        if (strcmp(expr, specials[i].name) == 0) {
            *result = specials[i].cron;
            return true;
        }
    }
    return false;
}

cron_expr_t *cron_parse(const char *expr, char **error_msg) {
    if (!expr) {
        if (error_msg) *error_msg = strdup("NULL expression");
        return NULL;
    }

    const char *actual = expr;
    is_special(expr, &actual);

    cron_expr_t *c = (cron_expr_t *)calloc(1, sizeof(cron_expr_t));
    if (!c) { if (error_msg) *error_msg = strdup("OOM"); return NULL; }
    strncpy(c->raw, expr, sizeof(c->raw) - 1);

    /* Split into 5 fields */
    char copy[128];
    strncpy(copy, actual, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';

    char *fields[5] = {NULL};
    char *saveptr;
    char *tok = strtok_r(copy, " \t", &saveptr);
    for (int i = 0; i < 5 && tok; i++) {
        fields[i] = tok;
        tok = strtok_r(NULL, " \t", &saveptr);
    }

    if (!fields[0] || !fields[1] || !fields[2] || !fields[3] || !fields[4]) {
        if (error_msg) *error_msg = strdup("Need 5 fields (min hour dom month dow)");
        cron_free(c);
        return NULL;
    }

    bool ok = true;
    ok = ok && parse_field(fields[0], &c->minute, 0, 59);
    ok = ok && parse_field(fields[1], &c->hour, 0, 23);
    ok = ok && parse_field(fields[2], &c->dom, 1, 31);
    ok = ok && parse_field(fields[3], &c->month, 1, 12);
    ok = ok && parse_field(fields[4], &c->dow, 0, 6);

    if (!ok) {
        if (error_msg) *error_msg = strdup("Invalid field value");
        cron_free(c);
        return NULL;
    }

    return c;
}

/* ================================================================
 *  Matching
 * ================================================================ */

static bool field_match(const cron_field_t *f, int val, int min) {
    if (f->any) return true;
    int idx = val - min;
    if (idx < 0 || idx >= MAX_BITS) return false;
    return f->bits[idx];
}

bool cron_match(const cron_expr_t *cron, const struct tm *tm) {
    if (!cron || !tm) return false;

    /* If both dom and dow are non-wildcard, match if EITHER matches */
    bool dom_match = field_match(&cron->dom, tm->tm_mday, 1);
    bool dow_match = field_match(&cron->dow, tm->tm_wday, 0);

    if (!cron->dom.any && !cron->dow.any) {
        /* Both are set: match if either matches */
        if (!dom_match && !dow_match) return false;
    } else if (!cron->dom.any) {
        /* Only dom set */
        if (!dom_match) return false;
    } else if (!cron->dow.any) {
        /* Only dow set */
        if (!dow_match) return false;
    }

    return field_match(&cron->minute, tm->tm_min, 0)
        && field_match(&cron->hour, tm->tm_hour, 0)
        && field_match(&cron->month, tm->tm_mon + 1, 1);
}

/* ================================================================
 *  Next matching time
 * ================================================================ */

#define TM_CMP(a,b,f) ((a)->tm_##f != (b)->tm_##f)

bool cron_next(const cron_expr_t *cron, const struct tm *from, struct tm *out) {
    if (!cron || !from || !out) return false;

    memcpy(out, from, sizeof(struct tm));
    out->tm_sec = 0;
    out->tm_isdst = -1;

    for (int year = from->tm_year; year <= 199; year++) {
        out->tm_year = year;
        for (int mon = 1; mon <= 12; mon++) {
            out->tm_mon = mon - 1;
            if (!field_match(&cron->month, mon, 1)) continue;

            /* Skip past dates before 'from' */
            if (year == from->tm_year && mon - 1 < from->tm_mon) continue;

            int max_dom = 31;
            /* Approximate — not handling Feb leap year precisely, close enough for cron */
            if (mon == 2) max_dom = 28;
            else if (mon == 4 || mon == 6 || mon == 9 || mon == 11) max_dom = 30;

            for (int dom = 1; dom <= max_dom; dom++) {
                out->tm_mday = dom;

                /* Skip past dates before 'from' */
                if (year == from->tm_year && mon - 1 == from->tm_mon && dom < from->tm_mday) continue;

                /* Check dom/dow */
                bool dom_ok = field_match(&cron->dom, dom, 1);
                bool dow_ok = field_match(&cron->dow, out->tm_wday, 0);

                if (!cron->dom.any && !cron->dow.any) {
                    if (!dom_ok && !dow_ok) continue;
                } else if (!cron->dom.any) {
                    if (!dom_ok) continue;
                } else if (!cron->dow.any) {
                    if (!dow_ok) continue;
                }

                for (int hour = 0; hour < 24; hour++) {
                    out->tm_hour = hour;
                    if (!field_match(&cron->hour, hour, 0)) continue;
                    if (year == from->tm_year && mon - 1 == from->tm_mon && dom == from->tm_mday && hour < from->tm_hour) continue;

                    for (int min = 0; min < 60; min++) {
                        out->tm_min = min;
                        if (!field_match(&cron->minute, min, 0)) continue;
                        if (year == from->tm_year && mon - 1 == from->tm_mon && dom == from->tm_mday && hour == from->tm_hour && min <= from->tm_min) continue;

                        /* Normalize (recalculate wday/yday) */
                        time_t t = mktime(out);
                        if (t == -1) continue;
                        localtime_r(&t, out);
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

/* ================================================================
 *  Describe
 * ================================================================ */

char *cron_describe(const cron_expr_t *cron) {
    if (!cron) return strdup("invalid");
    char buf[256];

    if (cron->minute.any && cron->hour.any && cron->dom.any && cron->month.any && cron->dow.any)
        snprintf(buf, sizeof(buf), "Every minute");
    else if (cron->minute.any && !cron->hour.any && cron->dom.any && cron->month.any && cron->dow.any)
        snprintf(buf, sizeof(buf), "Every minute past hour %d", (int)cron->hour.bits[0]);
    else if (!cron->minute.any && !cron->hour.any && cron->dom.any && cron->month.any && cron->dow.any)
        snprintf(buf, sizeof(buf), "At %02d:%02d every day", (int)cron->hour.bits[0], (int)cron->minute.bits[0]);
    else
        snprintf(buf, sizeof(buf), "Cron: %s", cron->raw);

    return strdup(buf);
}

void cron_free(cron_expr_t *cron) { free(cron); }
