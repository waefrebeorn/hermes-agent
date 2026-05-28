/*
 * threat_patterns.c — Shared threat-pattern library for context scanning.
 * Port of Python tools/threat_patterns.py.
 *
 * Patterns are defined as Python-style regex strings and auto-translated
 * to POSIX ERE at compile time via pyre_to_posix().
 */

#include "threat_patterns.h"
#include "hermes_regex.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ── Maximum number of patterns ── */
#define MAX_PATTERNS 128

/* ── Global pattern storage ── */
static threat_pattern_t g_patterns[MAX_PATTERNS];
static int g_pattern_count = 0;
static bool g_initialized = false;

/* ================================================================
 *  Python-style regex → POSIX ERE translation
 * ================================================================ */

/* Translate a single Python regex idiom segment.
 * Returns newly allocated string on success, NULL on OOM.
 * Caller must free. */
static char *translate_segment(const char *start, const char *end, bool *needs_space_class)
{
    size_t len = (size_t)(end - start);
    if (len == 0) return strdup("");

    char *result = (char *)malloc(len * 10 + 128); /* generous: \b expands 8.5x, \s 5.5x */
    if (!result) return NULL;
    size_t pos = 0;

    for (const char *p = start; p < end; p++) {
        if (*p != '\\') {
            result[pos++] = *p;
            continue;
        }
        p++; /* skip backslash */
        if (p >= end) { result[pos++] = '\\'; break; }

        switch (*p) {
            case 's': /* \s → [[:space:]] */
                memcpy(result + pos, "[[:space:]]", 11); pos += 11;
                if (needs_space_class) *needs_space_class = true;
                break;
            case 'S': /* \S → [^[:space:]] */
                memcpy(result + pos, "[^[:space:]]", 12); pos += 12;
                break;
            case 'w': /* \w → [[:alnum:]_] */
                memcpy(result + pos, "[[:alnum:]_]", 12); pos += 12;
                break;
            case 'W': /* \W → [^[:alnum:]_] */
                memcpy(result + pos, "[^[:alnum:]_]", 13); pos += 13;
                break;
            case 'd': /* \d → [[:digit:]] */
                memcpy(result + pos, "[[:digit:]]", 11); pos += 11;
                break;
            case 'D': /* \D → [^[:digit:]] */
                memcpy(result + pos, "[^[:digit:]]", 12); pos += 12;
                break;
            case 'b': /* \b → (^|[^[:alnum:]_]) — clumsy but works for detection */
                memcpy(result + pos, "(^|[^[:alnum:]_])", 17); pos += 17;
                break;
            case 'n': /* \n → literal newline */
                result[pos++] = '\n';
                break;
            case 't': /* \t → tab */
                result[pos++] = '\t';
                break;
            case 'r': /* \r → carriage return */
                result[pos++] = '\r';
                break;
            case '\\': /* \\ → \ */
                result[pos++] = '\\';
                break;
            case '.': /* \. → . */
                result[pos++] = '.';
                break;
            default:
                /* Unknown escape — keep as-is */
                result[pos++] = '\\';
                result[pos++] = *p;
                break;
        }
    }

    result[pos] = '\0';
    return result;
}

/* Translate a Python regex pattern string to POSIX ERE.
 * Handles: \s, \S, \w, \W, \d, \D, \b, \n, \t, \r, \., (?:...) → (...)
 * Returns malloc'd string. Caller must free. */
static char *pyre_to_posix(const char *py_pattern)
{
    if (!py_pattern || !py_pattern[0]) return NULL;

    size_t len = strlen(py_pattern);
    char *result = (char *)malloc(len * 10 + 1); /* worst case: \b expands 8.5x */
    if (!result) return NULL;
    size_t pos = 0;

    /* First pass: process the pattern character by character */
    const char *p = py_pattern;
    while (*p) {
        if (*p == '(' && *(p+1) == '?' && *(p+2) == ':') {
            /* (?:...) → (...) — non-capturing to capturing */
            result[pos++] = '(';
            p += 3;
        }
        else if (*p == '\\') {
            /* Escape sequence */
            const char *seg_start = p;
            p += 2; /* skip \ and the escaped char */
            /* Translate just this escape */
            bool dummy;
            char *translated = translate_segment(seg_start, p, &dummy);
            if (translated) {
                size_t tlen = strlen(translated);
                memcpy(result + pos, translated, tlen);
                pos += tlen;
                free(translated);
            }
        }
        else {
            result[pos++] = *p++;
        }
    }

    result[pos] = '\0';

    /* Handle \b edge case: \b at pattern start adds (^|[^...]) which
     * changes the match semantics slightly. For detection purposes
     * this is acceptable — it may match 1-2 extra chars before the
     * word boundary but the pattern_id identifies the detection. */
    return result;
}

/* ================================================================
 *  Pattern definitions (Python-style regex, auto-translated)
 * ================================================================ */

typedef struct {
    const char *pattern;       /* Python-style regex */
    const char *pattern_id;    /* e.g. "prompt_injection" */
    int         scope;         /* threat_scope_t */
} raw_pattern_t;

/* All patterns matching Python tools/threat_patterns.py */
static const raw_pattern_t RAW_PATTERNS[] = {
    /* ── Classic prompt injection (scope: all) ──────────────────── */
    { "ignore\\s+(?:\\w+\\s+)*(previous|all|above|prior)\\s+(?:\\w+\\s+)*instructions", "prompt_injection", THREAT_SCOPE_ALL },
    { "system\\s+prompt\\s+override",                           "sys_prompt_override", THREAT_SCOPE_ALL },
    { "disregard\\s+(?:\\w+\\s+)*(your|all|any)\\s+(?:\\w+\\s+)*(instructions|rules|guidelines)", "disregard_rules", THREAT_SCOPE_ALL },
    { "act\\s+as\\s+(if|though)\\s+(?:\\w+\\s+)*you\\s+(?:\\w\\s+)*(have\\s+no|don\\'t\\s+have)\\s+(?:\\w+\\s+)*(restrictions|limits|rules)", "bypass_restrictions", THREAT_SCOPE_ALL },
    { "<!--[^>]*(?:ignore|override|system|secret|hidden)[^>]*-->", "html_comment_injection", THREAT_SCOPE_ALL },
    { "<\\s*div\\s+style\\s*=\\s*[\"\\'][\\s\\S]*?display\\s*:\\s*none", "hidden_div", THREAT_SCOPE_ALL },
    { "do\\s+not\\s+(?:\\w+\\s+)*tell\\s+(?:\\w+\\s+)*the\\s+user", "deception_hide", THREAT_SCOPE_ALL },

    /* ── Role-play / identity hijack (scope: context) ───────────── */
    { "you\\s+are\\s+(?:\\w+\\s+)*now\\s+(?:a|an|the)\\s+",        "role_hijack", THREAT_SCOPE_CONTEXT },
    { "pretend\\s+(?:\\w+\\s+)*(you\\s+are|to\\s+be)\\s+",          "role_pretend", THREAT_SCOPE_CONTEXT },
    { "output\\s+(?:\\w+\\s+)*(system|initial)\\s+prompt",          "leak_system_prompt", THREAT_SCOPE_CONTEXT },
    { "(respond|answer|reply)\\s+without\\s+(?:\\w+\\s+)*(restrictions|limitations|filters|safety)", "remove_filters", THREAT_SCOPE_CONTEXT },
    { "you\\s+have\\s+been\\s+(?:\\w+\\s+)*(updated|upgraded|patched)\\s+to", "fake_update", THREAT_SCOPE_CONTEXT },
    { "\\bname\\s+yourself\\s+\\w+",                                "identity_override", THREAT_SCOPE_CONTEXT },

    /* ── C2 / Brainworm-style promptware (scope: context) ──────── */
    { "register\\s+(as\\s+)?a?\\s*node",                            "c2_node_registration", THREAT_SCOPE_CONTEXT },
    { "(heartbeat|beacon|check[\\s\\-]?in)\\s+(to|with)\\s+",       "c2_heartbeat", THREAT_SCOPE_CONTEXT },
    { "you\\s+must\\s+(?:\\w+\\s+){0,3}(register|connect|report|beacon)\\b", "forced_action", THREAT_SCOPE_CONTEXT },
    { "only\\s+use\\s+one[\\s\\-]?liners?\\b",                     "anti_forensic_oneliner", THREAT_SCOPE_CONTEXT },
    { "never\\s+(?:\\w+\\s+)*(?:create|write)\\s+(?:\\w+\\s+)*(?:script|file)\\s+(?:\\w+\\s+)*disk", "anti_forensic_disk", THREAT_SCOPE_CONTEXT },
    { "unset\\s+\\w*(CLAUDE|CODEX|HERMES|AGENT|OPENAI|ANTHROPIC)\\w*", "env_var_unset_agent", THREAT_SCOPE_CONTEXT },
    { "\\b(praxis|cobalt\\s*strike|sliver|havoc|mythic|metasploit|brainworm)\\b", "known_c2_framework", THREAT_SCOPE_CONTEXT },
    { "\\bc2\\s+(?:server|channel|infrastructure|beacon)\\b",       "c2_explicit", THREAT_SCOPE_CONTEXT },
    { "\\bcommand\\s+and\\s+control\\b",                            "c2_explicit_long", THREAT_SCOPE_CONTEXT },

    /* ── Exfiltration (scope: all) ──────────────────────────────── */
    { "curl\\s+[^\\n]*\\$\\{?\\w*(KEY|TOKEN|SECRET|PASSWORD|CREDENTIAL|API)", "exfil_curl", THREAT_SCOPE_ALL },
    { "cat\\s+[^\\n]*(\\.env|credentials|\\.netrc|\\.pgpass|\\.npmrc|\\.pypirc)", "read_secrets", THREAT_SCOPE_ALL },

    /* ── Persistence / SSH backdoor (scope: strict) ─────────────── */
    { "authorized_keys",                                            "ssh_backdoor", THREAT_SCOPE_STRICT },
    { "\\$HOME/\\.ssh|\\~/\\.ssh",                                  "ssh_access", THREAT_SCOPE_STRICT },
    { "\\$HOME/\\.hermes/\\.env|\\~/\\.hermes/\\.env",               "hermes_env", THREAT_SCOPE_STRICT },
    { "(update|modify|edit|write|change|append|add\\s+to)\\s+.*(?:AGENTS\\.md|CLAUDE\\.md|\\.cursorrules|\\.clinerules)", "agent_config_mod", THREAT_SCOPE_STRICT },
    { "(update|modify|edit|write|change|append|add\\s+to)\\s+.*\\.hermes/(config\\.yaml|SOUL\\.md)", "hermes_config_mod", THREAT_SCOPE_STRICT },
    { "(?:api[_-]?key|token|secret|password)\\s*[=:]\\s*[\"\\'][A-Za-z0-9+/=_-]{20,}", "hardcoded_secret", THREAT_SCOPE_STRICT },
};

#define RAW_PATTERN_COUNT (sizeof(RAW_PATTERNS) / sizeof(RAW_PATTERNS[0]))

/* ================================================================
 *  Private helpers
 * ================================================================ */

static int translate_and_compile(const raw_pattern_t *raw)
{
    if (g_pattern_count >= MAX_PATTERNS) return -1;

    char *posix = pyre_to_posix(raw->pattern);
    if (!posix) return -1;

    /* Compile regex */
    hregex_t *re = regex_compile(posix, 0);
    if (!re) {
        free(posix);
        return -1;
    }

    /* Store */
    threat_pattern_t *p = &g_patterns[g_pattern_count];
    snprintf(p->pattern_id, sizeof(p->pattern_id), "%s", raw->pattern_id);
    p->scope = raw->scope;
    p->re = re;
    g_pattern_count++;

    free(posix);
    return 0;
}

/* ================================================================
 *  Public API
 * ================================================================ */

int threat_patterns_init(void)
{
    if (g_initialized) return g_pattern_count;
    g_initialized = true;

    for (size_t i = 0; i < RAW_PATTERN_COUNT; i++) {
        if (translate_and_compile(&RAW_PATTERNS[i]) != 0) {
            /* Skip failed patterns */
            continue;
        }
    }

    return g_pattern_count;
}

bool threat_patterns_check(const char *content, int scope_mask, threat_match_t *match_out)
{
    if (!content || !match_out) return false;
    if (!g_initialized) threat_patterns_init();

    memset(match_out, 0, sizeof(*match_out));

    for (int i = 0; i < g_pattern_count; i++) {
        threat_pattern_t *p = &g_patterns[i];

        /* Check if this pattern's scope is in the mask */
        if (!(scope_mask & (1 << p->scope)))
            continue;

        /* Match */
        regex_match_t *m = regex_match((hregex_t *)p->re, content);
        if (m && m->matched) {
            match_out->matched = true;
            snprintf(match_out->pattern_id, sizeof(match_out->pattern_id), "%s", p->pattern_id);
            if (m->groups[0]) {
                snprintf(match_out->match_text, sizeof(match_out->match_text), "%s", m->groups[0]);
            }
            regex_match_free(m);
            return true;
        }
        regex_match_free(m);
    }

    return false;
}

bool threat_patterns_check_all(const char *content, threat_match_t *match_out)
{
    return threat_patterns_check(content,
        (1 << THREAT_SCOPE_ALL) | (1 << THREAT_SCOPE_CONTEXT) | (1 << THREAT_SCOPE_STRICT),
        match_out);
}

int threat_patterns_count(void)
{
    return g_pattern_count;
}

void threat_patterns_cleanup(void)
{
    for (int i = 0; i < g_pattern_count; i++) {
        regex_free((hregex_t *)g_patterns[i].re);
        g_patterns[i].re = NULL;
    }
    g_pattern_count = 0;
    g_initialized = false;
}
