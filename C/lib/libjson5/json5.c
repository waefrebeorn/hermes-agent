/*
 * libjson5.c -- JSON5 preprocessor -> libjson delegation.
 * Preprocesses JSON5 text into standard JSON, then calls json_parse().
 *
 * JSON5 features supported:
 *   - // and block comments (not inside strings)
 *   - Trailing commas in objects and arrays
 *   - Single-quoted strings ('foo')
 *   - Unquoted object keys (key: value)
 *   - Hex (0x), octal (0o), binary (0b) numbers
 *   - Leading/trailing decimal points (.5, 1.)
 *   - Explicit + sign on numbers
 *
 * MIT License - WuBu Hermes Project
 */

#include "json5.h"
#include "../libjson/json.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <math.h>

/* ================================================================
 * Phase A: Strip comments
 * ================================================================
 * Walk input, copy to output, strip // and slash-star-star-slash comments.
 * Preserves content inside double-quoted and single-quoted strings.
 * Returns malloc'd buffer or NULL on allocation failure.
 */
static char *strip_comments(const char *input) {
    size_t len = strlen(input);
    char *out = malloc(len + 1);
    if (!out) return NULL;
    size_t j = 0;
    size_t i = 0;
    char str_char = 0; /* 0 = outside string, '\'' or '"' = inside that quote type */

    while (i < len) {
        /* String context */
        if (str_char) {
            out[j++] = input[i];
            if (input[i] == '\\' && i + 1 < len) {
                /* Escape sequence -- copy next char literally */
                i++;
                out[j++] = input[i];
                i++;
            } else if (input[i] == str_char) {
                str_char = 0; /* end of string */
                i++;
            } else {
                i++;
            }
            continue;
        }

        /* Check for string start */
        if (input[i] == '"' || input[i] == '\'') {
            str_char = input[i];
            out[j++] = input[i];
            i++;
            continue;
        }

        /* Single-line comment */
        if (input[i] == '/' && i + 1 < len && input[i + 1] == '/') {
            i += 2; /* skip // */
            while (i < len && input[i] != '\n') i++;
            continue;
        }

        /* Multi-line comment */
        if (input[i] == '/' && i + 1 < len && input[i + 1] == '*') {
            i += 2; /* skip slash-star */
            while (i + 1 < len && !(input[i] == '*' && input[i + 1] == '/')) i++;
            if (i + 1 >= len) {
                /* Unterminated comment: replace with space and continue */
                out[j++] = ' ';
                break;
            }
            i += 2; /* skip star-slash */
            continue;
        }

        /* Regular character */
        out[j++] = input[i];
        i++;
    }

    out[j] = '\0';
    return out;
}

/* ================================================================
 * Phase B: Quote unquoted object keys
 * ================================================================
 * Find patterns like {key: or ,key: and wrap key in double quotes.
 * An unquoted key is [a-zA-Z_$][a-zA-Z0-9_$]* followed by ':'.
 * Must NOT be inside a string, and must follow '{' or ',' (with optional whitespace).
 */
static int is_ident_char(char c) {
    return isalnum((unsigned char)c) || c == '_' || c == '$';
}

static char *quote_keys(const char *input) {
    size_t len = strlen(input);
    /* Worst case: every char becomes "x" -- 3x growth + NUL */
    char *out = malloc(len * 3 + 1);
    if (!out) return NULL;
    size_t j = 0;
    size_t i = 0;
    char str_char = 0;

    while (i < len) {
        if (str_char) {
            out[j++] = input[i];
            if (input[i] == '\\' && i + 1 < len) {
                i++; out[j++] = input[i]; i++;
            } else if (input[i] == str_char) {
                str_char = 0; i++;
            } else {
                i++;
            }
            continue;
        }

        if (input[i] == '"' || input[i] == '\'') {
            str_char = input[i];
            out[j++] = input[i];
            i++;
            continue;
        }

        /* After '{' or ',', look for unquoted key */
        if (input[i] == '{' || input[i] == ',') {
            out[j++] = input[i];
            i++;

            /* Save position after comma/brace for backtrack */
            size_t after_mark = j;

            /* Copy whitespace */
            while (i < len && (input[i] == ' ' || input[i] == '\t' || input[i] == '\n' || input[i] == '\r')) {
                out[j++] = input[i];
                i++;
            }

            /* Check for identifier start */
            if (i < len && is_ident_char(input[i])) {
                size_t ident_start = i;
                while (i < len && is_ident_char(input[i])) i++;
                size_t ident_end = i;

                /* Peek ahead for ':' */
                size_t peek = i;
                while (peek < len && (input[peek] == ' ' || input[peek] == '\t' || input[peek] == '\n' || input[peek] == '\r')) peek++;

                if (peek < len && input[peek] == ':') {
                    /* Yes, unquoted key -- rewind j to after_mark, then write "key": */
                    j = after_mark;
                    out[j++] = '"';
                    for (size_t k = ident_start; k < ident_end; k++) {
                        if (input[k] == '"' || input[k] == '\\') out[j++] = '\\';
                        out[j++] = input[k];
                    }
                    out[j++] = '"';
                    /* Copy whitespace between key and colon */
                    for (size_t k = ident_end; k < peek; k++) out[j++] = input[k];
                    out[j++] = ':';
                    i = peek + 1;
                    continue;
                }

                /* Not a key: copy the identifier we skipped */
                for (size_t k = ident_start; k < ident_end; k++) out[j++] = input[k];
                /* i already advanced to ident_end */
                continue;
            }
            continue;
        }

        out[j++] = input[i];
        i++;
    }

    out[j] = '\0';
    return out;
}

/* ================================================================
 * Phase C: Convert single-quoted strings to double-quoted
 * ================================================================
 * Within single-quoted strings:
 *   - backslash-singlequote -> single-quote (unescaped in double-quoted context)
 *   - backslash-backslash -> backslash-backslash (stays escaped)
 *   - doublequote -> backslash-doublequote (needs escaping)
 * All other escapes preserved verbatim.
 */
static char *convert_single_quoted(const char *input) {
    size_t len = strlen(input);
    char *out = malloc(len * 2 + 1);
    if (!out) return NULL;
    size_t j = 0;
    size_t i = 0;

    while (i < len) {
        if (input[i] == '"') {
            /* Double-quoted string -- copy verbatim, watch for escapes */
            out[j++] = input[i];
            i++;
            while (i < len && input[i] != '"') {
                if (input[i] == '\\' && i + 1 < len) {
                    out[j++] = input[i]; i++;
                    out[j++] = input[i]; i++;
                } else {
                    out[j++] = input[i]; i++;
                }
            }
            if (i < len) { out[j++] = input[i]; i++; }
            continue;
        }

        if (input[i] == '\'') {
            /* Single-quoted string -- convert to double-quoted */
            out[j++] = '"';
            i++;
            while (i < len && input[i] != '\'') {
                if (input[i] == '\\' && i + 1 < len) {
                    char next = input[i + 1];
                    if (next == '\'') {
                        /* backslash-singlequote -> ' (unescape, since ' not special in "..." ) */
                        out[j++] = '\'';
                        i += 2;
                    } else if (next == '"') {
                        /* backslash-doublequote -> backslash-doublequote (need to escape " in double-quoted context) */
                        out[j++] = '\\';
                        out[j++] = '"';
                        i += 2;
                    } else {
                        /* Copy escape sequence verbatim */
                        out[j++] = '\\';
                        out[j++] = next;
                        i += 2;
                    }
                } else if (input[i] == '"') {
                    /* " inside '...' -- doesn't need escaping in JSON5, but does in JSON */
                    out[j++] = '\\';
                    out[j++] = '"';
                    i++;
                } else {
                    out[j++] = input[i];
                    i++;
                }
            }
            if (i < len) i++; /* skip closing ' */
            out[j++] = '"';
            continue;
        }

        out[j++] = input[i];
        i++;
    }

    out[j] = '\0';
    return out;
}

/* ================================================================
 * Phase D: Remove trailing commas
 * ================================================================
 * Replace ",}" with "}" and ",]" with "]", including whitespace
 * between comma and closing bracket.
 */
static char *remove_trailing_commas(const char *input) {
    size_t len = strlen(input);
    char *out = malloc(len + 1);
    if (!out) return NULL;
    size_t j = 0;
    size_t i = 0;
    char str_char = 0;

    while (i < len) {
        if (str_char) {
            out[j++] = input[i];
            if (input[i] == '\\' && i + 1 < len) {
                i++; out[j++] = input[i]; i++;
            } else if (input[i] == str_char) {
                str_char = 0; i++;
            } else {
                i++;
            }
            continue;
        }

        if (input[i] == '"') {
            str_char = input[i];
            out[j++] = input[i];
            i++;
            continue;
        }

        /* Check for trailing comma pattern */
        if (input[i] == ',') {
            size_t peek = i + 1;
            while (peek < len && (input[peek] == ' ' || input[peek] == '\t' || input[peek] == '\n' || input[peek] == '\r')) peek++;
            if (peek < len && (input[peek] == '}' || input[peek] == ']')) {
                /* Skip the comma */
                i++;
                continue;
            }
        }

        out[j++] = input[i];
        i++;
    }

    out[j] = '\0';
    return out;
}

/* ================================================================
 * Phase E: Convert non-standard number formats
 * ================================================================
 * Convert: 0x/0X hex, 0o/0O octal, 0b/0B binary to decimal strings.
 * Convert leading decimal (.5) to 0.5 and trailing decimal (1.) to 1.0.
 * Strip explicit + sign from numbers.
 */
static char *convert_numbers(const char *input) {
    size_t len = strlen(input);
    /* Worst case expansion: hex number 0xFFFFFFFF -> 4294967295 (10 digits) */
    char *out = malloc(len * 2 + 1);
    if (!out) return NULL;
    size_t j = 0;
    size_t i = 0;
    char str_char = 0;

    while (i < len) {
        if (str_char) {
            out[j++] = input[i];
            if (input[i] == '\\' && i + 1 < len) {
                i++; out[j++] = input[i]; i++;
            } else if (input[i] == str_char) {
                str_char = 0; i++;
            } else {
                i++;
            }
            continue;
        }

        if (input[i] == '"') {
            str_char = input[i];
            out[j++] = input[i];
            i++;
            continue;
        }

        /* Detect leading decimal: starts with '.' preceded by non-digit/non-identifier */
        if (input[i] == '.') {
            /* Check if preceded by a digit or letter (would be a normal number or identifier) */
            if (i > 0 && (isdigit((unsigned char)input[i-1]) || is_ident_char(input[i-1]))) {
                /* Regular decimal point -- copy as-is */
                out[j++] = input[i];
                i++;
                continue;
            }
            /* Leading decimal: .5 -> 0.5 */
            out[j++] = '0';
            out[j++] = '.';
            i++;
            /* Copy the rest of the number */
            while (i < len && (isdigit((unsigned char)input[i]) || input[i] == 'e' || input[i] == 'E' ||
                   input[i] == '+' || input[i] == '-')) {
                out[j++] = input[i];
                i++;
            }
            continue;
        }

        /* Detect trailing decimal: digit(s) followed by '.' then non-digit (or end) */
        if (i > 0 && isdigit((unsigned char)input[i-1]) && input[i] == '.') {
            /* Check if next char after dot is non-digit */
            if (i + 1 >= len || !isdigit((unsigned char)input[i+1])) {
                if (i + 1 >= len || (input[i+1] != 'e' && input[i+1] != 'E' && !isdigit((unsigned char)input[i+1]))) {
                    out[j++] = '.';
                    out[j++] = '0';
                    i++;
                    continue;
                }
            }
        }

        /* Detect hex/octal/bin numbers: 0x, 0X, 0o, 0O, 0b, 0B */
        if (input[i] == '0' && i + 1 < len && (input[i+1] == 'x' || input[i+1] == 'X')) {
            /* Hex number */
            i += 2; /* skip 0x */
            unsigned long long val = 0;
            while (i < len) {
                char c = input[i];
                if (c >= '0' && c <= '9') { val = val * 16 + (c - '0'); i++; }
                else if (c >= 'a' && c <= 'f') { val = val * 16 + (c - 'a' + 10); i++; }
                else if (c >= 'A' && c <= 'F') { val = val * 16 + (c - 'A' + 10); i++; }
                else break;
            }
            j += sprintf(out + j, "%llu", val);
            continue;
        }

        if (input[i] == '0' && i + 1 < len && (input[i+1] == 'o' || input[i+1] == 'O')) {
            /* Octal number */
            i += 2;
            unsigned long long val = 0;
            while (i < len && input[i] >= '0' && input[i] <= '7') {
                val = val * 8 + (input[i] - '0');
                i++;
            }
            j += sprintf(out + j, "%llu", val);
            continue;
        }

        if (input[i] == '0' && i + 1 < len && (input[i+1] == 'b' || input[i+1] == 'B')) {
            /* Binary number */
            i += 2;
            unsigned long long val = 0;
            while (i < len && input[i] >= '0' && input[i] <= '1') {
                val = val * 2 + (input[i] - '0');
                i++;
            }
            j += sprintf(out + j, "%llu", val);
            continue;
        }

        /* Detect +number */
        if (input[i] == '+' && i + 1 < len && (isdigit((unsigned char)input[i+1]) || input[i+1] == '.')) {
            i++; /* skip + */
            continue;
        }

        out[j++] = input[i];
        i++;
    }

    out[j] = '\0';
    return out;
}

/* ================================================================
 * Public API
 * ================================================================
 */
json_t *json5_parse(const char *input, char **error_msg) {
    if (!input) return NULL;

    char *s1 = strip_comments(input);
    if (!s1) { if (error_msg) *error_msg = strdup("memory allocation failed"); return NULL; }

    char *s2 = convert_single_quoted(s1);
    free(s1);
    if (!s2) { if (error_msg) *error_msg = strdup("memory allocation failed"); return NULL; }

    char *s3 = quote_keys(s2);
    free(s2);
    if (!s3) { if (error_msg) *error_msg = strdup("memory allocation failed"); return NULL; }

    char *s4 = remove_trailing_commas(s3);
    free(s3);
    if (!s4) { if (error_msg) *error_msg = strdup("memory allocation failed"); return NULL; }

    char *s5 = convert_numbers(s4);
    free(s4);
    if (!s5) { if (error_msg) *error_msg = strdup("memory allocation failed"); return NULL; }

    json_t *result = json_parse(s5, error_msg);
    free(s5);
    return result;
}

void json5_free(json_t *node) {
    json_free(node);
}
