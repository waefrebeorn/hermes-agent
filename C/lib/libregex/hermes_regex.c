/*
 * hermes_regex.c -- C regex convenience wrapper (J16: Python re port).
 * Wraps POSIX regex with simple match/search/replace API.
 */
#include "hermes_regex.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

struct hregex {
    regex_t native;
    int    flags;
};

hregex_t *regex_compile(const char *pattern, int flags) {
    if (!pattern || !pattern[0]) return NULL;
    hregex_t *re = (hregex_t *)calloc(1, sizeof(hregex_t));
    if (!re) return NULL;
    int cflags = REG_EXTENDED;
    if (flags & 1) cflags |= REG_ICASE;
    int err = regcomp(&re->native, pattern, cflags);
    if (err != 0) { free(re); return NULL; }
    re->flags = flags;
    return re;
}

regex_match_t *regex_match(hregex_t *re, const char *str) {
    if (!re || !str) return NULL;
    regex_match_t *m = (regex_match_t *)calloc(1, sizeof(regex_match_t));
    if (!m) return NULL;
    regmatch_t pmatch[REGEX_MAX_GROUPS];
    int err = regexec(&re->native, str, REGEX_MAX_GROUPS, pmatch, 0);
    if (err != 0) { m->matched = false; return m; }
    m->matched = true;
    for (int i = 0; i < REGEX_MAX_GROUPS; i++) {
        if (pmatch[i].rm_so >= 0) {
            size_t len = (size_t)(pmatch[i].rm_eo - pmatch[i].rm_so);
            m->groups[i] = (char *)malloc(len + 1);
            if (m->groups[i]) {
                memcpy(m->groups[i], str + pmatch[i].rm_so, len);
                m->groups[i][len] = '\0';
            }
        }
    }
    return m;
}

regex_match_t *regex_search(hregex_t *re, const char *str) {
    return regex_match(re, str);
}

char *regex_replace(hregex_t *re, const char *str, const char *replacement) {
    if (!re || !str) return NULL;
    if (!replacement) return strdup(str);
    regex_match_t *m = regex_match(re, str);
    if (!m || !m->matched) { regex_match_free(m); return strdup(str); }

    size_t str_len = strlen(str);
    size_t repl_len = replacement ? strlen(replacement) : 0;
    char expanded[1024]; expanded[0] = '\0'; size_t exp_pos = 0;

    for (size_t i = 0; i < repl_len && exp_pos < sizeof(expanded) - 1; i++) {
        if (replacement[i] == '$' && i + 1 < repl_len && replacement[i+1] >= '1' && replacement[i+1] <= '9') {
            int g = replacement[i+1] - '0';
            if (g < REGEX_MAX_GROUPS && m->groups[g]) {
                size_t glen = strlen(m->groups[g]);
                size_t copy = (sizeof(expanded) - 1 - exp_pos < glen) ? sizeof(expanded) - 1 - exp_pos : glen;
                memcpy(expanded + exp_pos, m->groups[g], copy);
                exp_pos += copy;
            }
            i++;
        } else {
            expanded[exp_pos++] = replacement[i];
        }
    }
    expanded[exp_pos] = '\0';

    size_t match_start = 0, match_end = str_len;
    if (m->groups[0]) {
        char *pos = strstr(str, m->groups[0]);
        if (pos) { match_start = (size_t)(pos - str); match_end = match_start + strlen(m->groups[0]); }
    }

    size_t total = match_start + strlen(expanded) + (str_len - match_end) + 1;
    char *result = (char *)malloc(total);
    if (!result) { regex_match_free(m); return NULL; }
    memcpy(result, str, match_start);
    memcpy(result + match_start, expanded, strlen(expanded));
    memcpy(result + match_start + strlen(expanded), str + match_end, str_len - match_end + 1);
    regex_match_free(m);
    return result;
}

void regex_free(hregex_t *re) {
    if (!re) return;
    regfree(&re->native);
    free(re);
}

void regex_match_free(regex_match_t *m) {
    if (!m) return;
    for (int i = 0; i < REGEX_MAX_GROUPS; i++) free(m->groups[i]);
    free(m);
}

char *regex_extract(const char *pattern, const char *str, int group) {
    if (!pattern || !str) return NULL;
    hregex_t *re = regex_compile(pattern, 0);
    if (!re) return NULL;
    regex_match_t *m = regex_search(re, str);
    char *result = NULL;
    if (m && m->matched && group < REGEX_MAX_GROUPS && m->groups[group])
        result = strdup(m->groups[group]);
    regex_match_free(m);
    regex_free(re);
    return result;
}
