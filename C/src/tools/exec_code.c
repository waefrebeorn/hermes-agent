/*
 * exec_code.c — Execute code tool for Hermes C.
 * Runs Python code via subprocess and returns stdout/stderr.
 * Replaces Python's execute_code tool.
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

#define EXEC_MAX_OUTPUT 65536
#define EXEC_DEFAULT_TIMEOUT 60

static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"code\":{\"type\":\"string\",\"description\":\"Python code to execute\"},"
      "\"timeout\":{\"type\":\"number\",\"description\":\"Timeout seconds\",\"default\":60}"
    "},"
    "\"required\":[\"code\"]"
"}";

static char *run_python(const char *code, int timeout_sec) {
    if (!code) return strdup("{\"error\":\"No code provided\"}");

    /* Write code to temp file */
    char tmp_path[] = "/tmp/hermes_exec_XXXXXX.py";
    int fd = mkstemps(tmp_path, 3);
    if (fd < 0) return strdup("{\"error\":\"Cannot create temp file\"}");
    FILE *f = fdopen(fd, "w");
    if (!f) { close(fd); return strdup("{\"error\":\"Cannot write temp file\"}"); }
    fputs(code, f);
    fclose(f);

    /* Build command with timeout */
    char cmd[32768];
    snprintf(cmd, sizeof(cmd),
             "timeout %d python3 %s 2>&1 || true",
             timeout_sec > 0 ? timeout_sec : EXEC_DEFAULT_TIMEOUT, tmp_path);

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        unlink(tmp_path);
        return strdup("{\"error\":\"popen failed\"}");
    }

    /* Read output */
    size_t cap = 4096, len = 0;
    char *output = (char *)malloc(cap);
    if (!output) { pclose(fp); unlink(tmp_path); return strdup("{\"error\":\"OOM\"}"); }
    output[0] = '\0';

    char line[4096];
    while (fgets(line, sizeof(line), fp)) {
        size_t line_len = strlen(line);
        if (len + line_len >= EXEC_MAX_OUTPUT) {
            memcpy(output + len, "\n...[truncated]", 15);
            len += 15;
            output[len] = '\0';
            break;
        }
        if (len + line_len + 1 > cap) {
            cap *= 2;
            char *new_out = (char *)realloc(output, cap);
            if (!new_out) { free(output); pclose(fp); unlink(tmp_path); return strdup("{\"error\":\"OOM\"}"); }
            output = new_out;
        }
        memcpy(output + len, line, line_len + 1);
        len += line_len;
    }

    int exit_code = pclose(fp);
    unlink(tmp_path);

    json_node_t *result = json_new_object();
    json_object_set(result, "exit_code", json_new_number((double)exit_code));
    json_object_set(result, "output", json_new_string(output));
    json_object_set(result, "truncated", json_new_bool(len >= EXEC_MAX_OUTPUT - 20));

    char *json_out = json_serialize(result);
    json_free(result);
    free(output);
    return json_out;
}

char *exec_code_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *code = json_object_get_string(args, "code", NULL);
    int timeout = (int)json_object_get_number(args, "timeout", EXEC_DEFAULT_TIMEOUT);
    json_free(args);

    if (!code) return strdup("{\"error\":\"Missing required 'code'\"}");
    return run_python(code, timeout);
}

void registry_init_exec_code(void) {
    registry_register("execute_code",
        "Execute Python code in a subprocess. Returns stdout, stderr, and exit code. "
        "Use for running calculations, data processing, and automation.",
        SCHEMA, exec_code_handler);
}
