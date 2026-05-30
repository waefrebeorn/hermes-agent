/* Trajectory saving utilities and static helpers.
 *
 * Python equivalent: agent/trajectory.py
 *
 * Provides:
 *   - convert_scratchpad_to_think() — tag normalization
 *   - has_incomplete_scratchpad()   — unclosed tag detection
 *   - save_trajectory()             — JSONL append
 */

#include "hermes_trajectory.h"
#include "../lib/libjson/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ------------------------------------------------------------------ */
/*  Scratchpad tag helpers                                            */
/* ------------------------------------------------------------------ */

char *hermes_convert_scratchpad_to_think(const char *content)
{
    if (!content)
        return NULL;

    /* Quick check: does it contain the tag? */
    if (!strstr(content, "<REASONING_SCRATCHPAD>"))
        return strdup(content);

    /* Allocate generous buffer (worst case: same length if no match) */
    size_t len = strlen(content);
    char *result = malloc(len + 1);
    if (!result)
        return NULL;

    /* Walk through content, replacing tags char-by-char.
     * Each replacement is exactly the same length, so result
     * length is identical to input length. */
    const char *src = content;
    char *dst = result;
    while (*src) {
        if (strncmp(src, "<REASONING_SCRATCHPAD>", 22) == 0) {
            memcpy(dst, "<think>", 7);
            dst += 7;
            src += 22;
        } else if (strncmp(src, "</REASONING_SCRATCHPAD>", 23) == 0) {
            memcpy(dst, "</think>", 8);
            dst += 8;
            src += 23;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';

    return result;
}

bool hermes_has_incomplete_scratchpad(const char *content)
{
    if (!content)
        return false;
    return strstr(content, "<REASONING_SCRATCHPAD>") != NULL
        && strstr(content, "</REASONING_SCRATCHPAD>") == NULL;
}

/* ------------------------------------------------------------------ */
/*  Trajectory saving to JSONL                                        */
/* ------------------------------------------------------------------ */

int hermes_save_trajectory(const char *trajectory_json,
                            const char *model,
                            bool completed,
                            const char *filename)
{
    /* Default filename */
    char default_fn[256];
    if (!filename) {
        const char *base = completed
            ? "trajectory_samples.jsonl"
            : "failed_trajectories.jsonl";
        snprintf(default_fn, sizeof(default_fn), "%s", base);
        filename = default_fn;
    }

    /* Build ISO timestamp */
    char ts[64];
    {
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        if (tm)
            strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", tm);
        else
            snprintf(ts, sizeof(ts), "unknown");
    }

    /* Build JSON entry: {"conversations":...,"timestamp":...,"model":...,"completed":...} */
    json_t *root = json_object();
    if (!root) return -1;

    /* conversations — parse the passed JSON array string */
    json_t *conv = json_parse(trajectory_json, NULL);
    if (!conv || conv->type != JSON_ARRAY) {
        json_free(conv);
        json_free(root);
        return -1;
    }
    json_set(root, "conversations", conv);
    /* conv is now owned by root — json_free(root) handles it */

    /* timestamp */
    json_set(root, "timestamp", json_string(ts));

    /* model */
    json_set(root, "model", json_string(model ? model : ""));

    /* completed */
    json_set(root, "completed", json_bool(completed));

    /* Serialize to JSON string */
    char *json_str = json_serialize(root);
    json_free(root);

    if (!json_str)
        return -1;

    /* Append to file */
    FILE *f = fopen(filename, "a");
    if (!f) {
        free(json_str);
        return -1;
    }
    fprintf(f, "%s\n", json_str);
    fclose(f);

    free(json_str);
    return 0;
}
