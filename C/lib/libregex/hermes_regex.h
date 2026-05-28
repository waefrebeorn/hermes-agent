/*
 * hermes_regex.h -- C regex convenience wrapper (J16: Python re port).
 * Provides simple regex match/search/sub with POSIX regex backend.
 */
#ifndef HERMES_REGEX_H
#define HERMES_REGEX_H
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define REGEX_MAX_GROUPS 16
typedef struct hregex hregex_t;
typedef struct {
    bool    matched;
    char   *groups[REGEX_MAX_GROUPS];
    int     group_count;
} regex_match_t;

hregex_t *regex_compile(const char *pattern, int flags);
regex_match_t *regex_match(hregex_t *re, const char *str);
regex_match_t *regex_search(hregex_t *re, const char *str);
char *regex_replace(hregex_t *re, const char *str, const char *replacement);
void regex_free(hregex_t *re);
void regex_match_free(regex_match_t *m);
char *regex_extract(const char *pattern, const char *str, int group);

#ifdef __cplusplus
}
#endif
#endif /* HERMES_REGEX_H */
