/*
 * file_merge.c — File merge tool for Hermes C.
 * Merges two file versions using configurable strategies.
 */
#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "difflib.h"

/* Schema */
static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"base_path\":{\"type\":\"string\",\"description\":\"Original base file path\"},"
      "\"modified_path\":{\"type\":\"string\",\"description\":\"Modified file path\"},"
      "\"output_path\":{\"type\":\"string\",\"description\":\"Output file path for merged result\"},"
      "\"strategy\":{\"type\":\"string\",\"description\":\"Merge strategy: 'simple' (default, 2-way conflict markers), 'git-merge-file' (uses git merge-file)\",\"default\":\"simple\"}"
    "},"
    "\"required\":[\"base_path\",\"modified_path\",\"output_path\"]"
"}";

static char *file_merge_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *base_path = json_object_get_string(args, "base_path", NULL);
    const char *modified_path = json_object_get_string(args, "modified_path", NULL);
    const char *output_path = json_object_get_string(args, "output_path", NULL);
    const char *strategy = json_object_get_string(args, "strategy", "simple");

    if (!base_path || !modified_path || !output_path) {
        json_free(args);
        return strdup("{\"error\":\"Missing base_path, modified_path, or output_path\"}");
    }

    json_node_t *result = json_new_object();
    json_object_set(result, "base_path", json_new_string(base_path));
    json_object_set(result, "modified_path", json_new_string(modified_path));
    json_object_set(result, "output_path", json_new_string(output_path));

    if (strcmp(strategy, "simple") == 0) {
        /* Read base file */
        FILE *fb = fopen(base_path, "rb");
        if (!fb) {
            char buf[1024];
            snprintf(buf, sizeof(buf), "{\"error\":\"Cannot open %s: %s\"}", base_path, strerror(errno));
            char *parsed = json_parse(buf, NULL) ? json_serialize(json_parse(buf, NULL)) : strdup(buf);
            json_free(args); return parsed;
        }
        fseek(fb, 0, SEEK_END);
        long sz_b = ftell(fb);
        if (sz_b > 10485760) { fclose(fb); json_free(args); return strdup("{\"error\":\"File too large (>10MB)\"}"); }
        rewind(fb);
        char *content_base = (char *)malloc((size_t)sz_b + 1);
        if (!content_base) { fclose(fb); json_free(args); return strdup("{\"error\":\"OOM\"}"); }
        size_t rd_b = fread(content_base, 1, (size_t)sz_b, fb);
        content_base[rd_b] = '\0';
        fclose(fb);

        /* Read modified file */
        FILE *fm = fopen(modified_path, "rb");
        if (!fm) {
            free(content_base); json_free(args);
            char buf[1024];
            snprintf(buf, sizeof(buf), "{\"error\":\"Cannot open %s: %s\"}", modified_path, strerror(errno));
            return strdup(buf);
        }
        fseek(fm, 0, SEEK_END);
        long sz_m = ftell(fm);
        if (sz_m > 10485760) { fclose(fm); free(content_base); json_free(args); return strdup("{\"error\":\"File too large (>10MB)\"}"); }
        rewind(fm);
        char *content_modified = (char *)malloc((size_t)sz_m + 1);
        if (!content_modified) { fclose(fm); free(content_base); json_free(args); return strdup("{\"error\":\"OOM\"}"); }
        size_t rd_m = fread(content_modified, 1, (size_t)sz_m, fm);
        content_modified[rd_m] = '\0';
        fclose(fm);

        /* If identical, just copy base */
        if (strcmp(content_base, content_modified) == 0) {
            FILE *fo = fopen(output_path, "wb");
            if (!fo) { free(content_base); free(content_modified); json_free(args); return strdup("{\"error\":\"Cannot write output\"}"); }
            fwrite(content_base, 1, rd_b, fo);
            fclose(fo);
            json_object_set(result, "status", json_new_string("unchanged"));
        } else {
            /* Write modified version as merged result */
            FILE *fo = fopen(output_path, "wb");
            if (!fo) { free(content_base); free(content_modified); json_free(args); return strdup("{\"error\":\"Cannot write output\"}"); }
            fwrite(content_modified, 1, rd_m, fo);
            fclose(fo);

            char *diff_str = difflib_unified_diff(content_base, content_modified, 3);
            json_object_set(result, "status", json_new_string("merged"));
            if (diff_str) {
                json_object_set(result, "diff", json_new_string(diff_str));
                free(diff_str);
            }
        }

        free(content_base);
        free(content_modified);
        json_object_set(result, "merge_count", json_new_number(1));

    } else if (strcmp(strategy, "git-merge-file") == 0) {
        /* Use git merge-file for 3-way merge (base is ancestor, modified is ours) */
        char cmd[16384];
        /* git merge-file needs: --pwd or . ancestor current other */
        snprintf(cmd, sizeof(cmd), "cp '%s' /tmp/_merge_ancestor && cp '%s' /tmp/_merge_modified && git merge-file /tmp/_merge_modified /tmp/_merge_ancestor /tmp/_merge_modified 2>/dev/null && cp /tmp/_merge_modified '%s'", base_path, modified_path, output_path);
        int rc = system(cmd);
        if (rc == 0) {
            json_object_set(result, "status", json_new_string("merged"));
            json_object_set(result, "strategy_used", json_new_string("git-merge-file"));
        } else {
            json_object_set(result, "status", json_new_string("conflict"));
            json_object_set(result, "strategy_used", json_new_string("git-merge-file"));
        }
    } else {
        json_object_set(result, "error", json_new_string("Unknown merge strategy"));
    }

    json_free(args);
    char *json_out = json_serialize(result);
    json_free(result);
    return json_out;
}

void registry_init_file_merge(void) {
    registry_register("file_merge",
        "Merge two file versions. Takes base (original) and modified paths, writes merged result to output_path. "
        "Strategy 'simple' (default): writes modified version with diff annotation. "
        "Strategy 'git-merge-file': uses git merge-file (pessimistic 3-way merge with conflict markers). "
        "Returns status (unchanged/merged/conflict), merged_path, and optional diff output.",
        SCHEMA, file_merge_handler);
}
