/*
 * delegate.c — Delegate task tool for Hermes C.
 * Spawns a subagent (another hermes process) to work on a subtask.
 * Uses subprocess + stdin/stdout JSON-RPC protocol.
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#define DELEGATE_MAX_OUTPUT (128 * 1024)
#define DELEGATE_TIMEOUT 300

static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"goal\":{\"type\":\"string\",\"description\":\"What the subagent should accomplish\"},"
      "\"context\":{\"type\":\"string\",\"description\":\"Background information for the subagent\"},"
      "\"timeout\":{\"type\":\"number\",\"description\":\"Max seconds for task\",\"default\":300}"
    "},"
    "\"required\":[\"goal\"]"
"}";

/* Shell-escape a string for single-quote context */
static void shell_escape(const char *in, char *out, size_t out_sz) {
    size_t pos = 0;
    for (const char *p = in; *p && pos < out_sz - 4; p++) {
        if (*p == '\'') {
            if (pos + 4 < out_sz) {
                out[pos++] = '\'';
                out[pos++] = '\\';
                out[pos++] = '\'';
                out[pos++] = '\'';
            }
        } else if (*p == '\n') {
            if (pos + 2 < out_sz) { out[pos++] = '\\'; out[pos++] = 'n'; }
        } else if (*p == '\t') {
            if (pos + 2 < out_sz) { out[pos++] = '\\'; out[pos++] = 't'; }
        } else {
            out[pos++] = *p;
        }
    }
    out[pos] = '\0';
}

char *delegate_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *goal = json_object_get_string(args, "goal", NULL);
    const char *context = json_object_get_string(args, "context", NULL);
    int timeout = (int)json_object_get_number(args, "timeout", DELEGATE_TIMEOUT);

    json_node_t *result = json_new_object();

    if (!goal) {
        json_object_set(result, "error", json_new_string("Missing goal"));
    } else {
        /* Build prompt for subagent */
        char prompt[65536];
        snprintf(prompt, sizeof(prompt), "%s",
                 goal ? goal : "");
        if (context && *context) {
            size_t plen = strlen(prompt);
            snprintf(prompt + plen, sizeof(prompt) - plen,
                     "\n\nContext:\n%s", context);
        }

        /* Escape for shell */
        char escaped[131072];
        shell_escape(prompt, escaped, sizeof(escaped));

        /* Build and run command */
        char cmd[196608];
        snprintf(cmd, sizeof(cmd),
                 "echo '%s' | timeout %d hermes 2>&1 || true",
                 escaped, timeout);

        FILE *fp = popen(cmd, "r");
        if (!fp) {
            json_object_set(result, "error", json_new_string("popen failed"));
        } else {
            /* Read output */
            size_t cap = 16384, len = 0;
            char *output = (char *)malloc(cap);
            if (!output) { pclose(fp); json_object_set(result, "error", json_new_string("OOM")); }
            else {
                output[0] = '\0';
                char line[8192];
                while (fgets(line, sizeof(line), fp)) {
                    size_t line_len = strlen(line);
                    if (len + line_len >= DELEGATE_MAX_OUTPUT) break;
                    if (len + line_len + 1 > cap) {
                        cap *= 2;
                        char *new_out = (char *)realloc(output, cap);
                        if (!new_out) break;
                        output = new_out;
                    }
                    memcpy(output + len, line, line_len + 1);
                    len += line_len;
                }

                int exit_code = pclose(fp);
                json_object_set(result, "status", json_new_string("completed"));
                json_object_set(result, "summary", json_new_string(output));
                json_object_set(result, "exit_code", json_new_number((double)exit_code));
                json_object_set(result, "output_length", json_new_number((double)len));
                free(output);
            }
        }
    }

    char *json_out = json_serialize(result);
    json_free(result);
    json_free(args);
    return json_out;
}

void registry_init_delegate(void) {
    registry_register("delegate_task",
        "Spawn a subagent to work on a subtask. The subagent runs as a separate "
        "Hermes process with the given goal and context. Returns the output.",
        SCHEMA, delegate_handler);
}
