/*
 * tool_dispatch_helpers.c — Stateless tool-dispatch utility functions.
 *
 * Port of Python agent/tool_dispatch_helpers.py.
 * All functions are stateless and thread-safe.
 */

#include "tool_dispatch_helpers.h"
#include "../libjson/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ================================================================
 *  Helper: regex-like pattern matching (subset)
 * ================================================================ */

/* Simple word-boundary check: is char a word boundary? */
static bool is_word_boundary(char c) {
    return c == '\0' || c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
           c == '|' || c == '&' || c == ';' || c == '`';
}

/* Check if cmd contains a destructive pattern at a word boundary.
 * Patterns: rm, rmdir, cp, install, mv, sed -i, truncate, dd, shred,
 * git reset/clean/checkout (with word boundary after). */
static bool matches_destructive_pattern(const char *cmd) {
    const char *patterns[] = {
        "rm ", "rmdir ", "cp ", "install ", "mv ",
        "sed -i", "truncate ", "dd ", "shred ",
        "git reset", "git clean", "git checkout",
        NULL
    };

    /* Search for each pattern preceded by a word boundary */
    for (int i = 0; patterns[i]; i++) {
        const char *p = patterns[i];
        const char *pos = cmd;
        while ((pos = strstr(pos, p)) != NULL) {
            /* Check that pattern is at start or preceded by word boundary */
            if (pos == cmd || is_word_boundary(*(pos - 1))) {
                return true;
            }
            pos++;
        }
    }
    return false;
}

/* Check for single '>' output redirect (overwrite, not append '>>').
 * Matches '>' not preceded by '>' and not followed by '>'. */
static bool matches_redirect_overwrite(const char *cmd) {
    const char *p = cmd;
    while ((p = strchr(p, '>')) != NULL) {
        /* Not '>>' */
        if (!(p > cmd && *(p - 1) == '>') && !(*(p + 1) == '>')) {
            return true;
        }
        p++;
    }
    return false;
}

/* ================================================================
 *  Public API
 * ================================================================ */

bool is_destructive_command(const char *cmd) {
    if (!cmd || !cmd[0])
        return false;
    if (matches_destructive_pattern(cmd))
        return true;
    if (matches_redirect_overwrite(cmd))
        return true;
    return false;
}

char *extract_error_preview(const char *result, size_t max_len) {
    if (!result || !result[0])
        return NULL;

    const char *text = result;
    char *parsed_text = NULL;

    /* Try to parse as JSON and extract error field */
    const char *stripped = result;
    while (*stripped && isspace((unsigned char)*stripped))
        stripped++;

    if (*stripped == '{') {
        char *jerr = NULL;
        json_t *json = json_parse(result, &jerr);
        if (json && !jerr) {
            json_t *err_node = json_obj_get(json, "error");
            if (err_node && err_node->type == JSON_STRING && err_node->str_val) {
                parsed_text = strdup(err_node->str_val);
            }
            json_free(json);
        }
        if (jerr) free(jerr);
    }

    /* If JSON parsing produced an error text, use it; otherwise use raw text */
    if (parsed_text) {
        text = parsed_text;
    }

    /* Collapse whitespace */
    char *collapsed = (char *)malloc(strlen(text) + 1);
    if (!collapsed) {
        free(parsed_text);
        return NULL;
    }

    size_t out = 0;
    bool in_space = false;
    for (const char *p = text; *p; p++) {
        if (isspace((unsigned char)*p)) {
            if (!in_space && out > 0) {
                collapsed[out++] = ' ';
                in_space = true;
            }
        } else {
            collapsed[out++] = *p;
            in_space = false;
        }
    }
    collapsed[out] = '\0';

    free(parsed_text);

    /* Truncate to max_len */
    if (out > max_len) {
        if (max_len > 1)
            collapsed[max_len - 1] = '\xe2'; /* UTF-8 ellipsis … (3 bytes) */
        if (max_len > 0)
            collapsed[max_len] = '\0';
    }

    /* Return empty string? treat as no error */
    if (collapsed[0] == '\0') {
        free(collapsed);
        return NULL;
    }

    return collapsed;
}

char **extract_file_mutation_targets(const char *tool_name,
                                      const char *args_json,
                                      size_t *count_out) {
    *count_out = 0;
    if (!tool_name || !args_json)
        return NULL;

    char **result = NULL;
    size_t count = 0;

    /* Parse args JSON */
    char *jerr = NULL;
    json_t *args = json_parse(args_json, &jerr);
    if (!args || jerr) {
        if (jerr) free(jerr);
        if (args) json_free(args);
        return NULL;
    }

    if (strcmp(tool_name, "write_file") == 0) {
        json_t *path_node = json_obj_get(args, "path");
        if (path_node && path_node->type == JSON_STRING && path_node->str_val) {
            result = (char **)malloc(sizeof(char *));
            if (result) {
                result[0] = strdup(path_node->str_val);
                count = 1;
            }
        }
    } else if (strcmp(tool_name, "patch") == 0) {
        json_t *mode_node = json_obj_get(args, "mode");
        const char *mode = NULL;
        if (mode_node && mode_node->type == JSON_STRING)
            mode = mode_node->str_val;

        /* Default mode is "replace" */
        if (!mode || strcmp(mode, "replace") == 0) {
            json_t *path_node = json_obj_get(args, "path");
            if (path_node && path_node->type == JSON_STRING && path_node->str_val) {
                result = (char **)malloc(sizeof(char *));
                if (result) {
                    result[0] = strdup(path_node->str_val);
                    count = 1;
                }
            }
        } else if (strcmp(mode, "patch") == 0) {
            json_t *patch_node = json_obj_get(args, "patch");
            if (patch_node && patch_node->type == JSON_STRING && patch_node->str_val) {
                /* Parse V4A patch headers: *** Update/Add/Delete File: <path> */
                const char *body = patch_node->str_val;
                /* Count first */
                size_t cap = 0;
                const char *p = body;
                while (*p) {
                    /* Look for "***" at start of line */
                    if ((p == body || *(p-1) == '\n') &&
                        p[0] == '*' && p[1] == '*' && p[2] == '*') {
                        p += 3;
                        while (*p && isspace((unsigned char)*p)) p++;
                        if (strncmp(p, "Update File:", 12) == 0 ||
                            strncmp(p, "Add File:", 9) == 0 ||
                            strncmp(p, "Delete File:", 12) == 0) {
                            /* Skip past "XXX File:" */
                            while (*p && *p != ':') p++;
                            if (*p == ':') p++;
                            while (*p && isspace((unsigned char)*p)) p++;
                            /* Extract path until end of line */
                            const char *start = p;
                            while (*p && *p != '\n' && *p != '\r') p++;
                            if (p > start) {
                                cap++;
                                result = (char **)realloc(result, cap * sizeof(char *));
                                if (result) {
                                    size_t plen = (size_t)(p - start);
                                    char *path = (char *)malloc(plen + 1);
                                    if (path) {
                                        memcpy(path, start, plen);
                                        path[plen] = '\0';
                                        result[cap - 1] = path;
                                        count = cap;
                                    }
                                }
                            }
                        }
                    }
                    p++;
                }
            }
        }
    }

    json_free(args);
    *count_out = count;
    return result;
}

void free_mutation_targets(char **targets, size_t count) {
    if (!targets) return;
    for (size_t i = 0; i < count; i++)
        free(targets[i]);
    free(targets);
}

bool is_multimodal_tool_result(const char *result_json) {
    if (!result_json || !result_json[0])
        return false;

    char *jerr = NULL;
    json_t *json = json_parse(result_json, &jerr);
    if (!json || jerr) {
        if (jerr) free(jerr);
        if (json) json_free(json);
        return false;
    }

    /* Check _multimodal == true and content is an array */
    json_t *mm = json_obj_get(json, "_multimodal");
    json_t *content = json_obj_get(json, "content");

    bool is_mm = (mm && mm->type == JSON_BOOL && mm->bool_val &&
                  content && content->type == JSON_ARRAY);

    json_free(json);
    return is_mm;
}

bool paths_overlap(const char *left, const char *right) {
    if (!left || !right || !left[0] || !right[0])
        return false;

    /* Simple check: does one path start with the other? */
    size_t llen = strlen(left);
    size_t rlen = strlen(right);
    size_t min_len = llen < rlen ? llen : rlen;

    /* Root path "/" overlaps with any absolute path */
    if (llen == 1 && left[0] == '/' && rlen > 0 && right[0] == '/')
        return true;
    if (rlen == 1 && right[0] == '/' && llen > 0 && left[0] == '/')
        return true;

    /* Check if they match up to the common prefix */
    if (strncmp(left, right, min_len) != 0)
        return false;

    /* If one is a prefix of the other AND the next char is '/' or '\0',
     * they refer to the same subtree */
    if (llen == rlen)
        return true;
    if (llen < rlen && (right[llen] == '/' || right[llen] == '\0'))
        return true;
    if (rlen < llen && (left[rlen] == '/' || left[rlen] == '\0'))
        return true;

    return false;
}
