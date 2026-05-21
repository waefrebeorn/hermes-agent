/*
 * send_message.c — Send message tool for Slermes C.
 * Sends messages to platforms. Currently supports: local (stdout/file), telegram.
 */

#include "slermes.h"
#include "slermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"target\":{\"type\":\"string\",\"description\":\"'local' (save to file), 'stdout' (print), or 'telegram' (via configured bot)\"},"
      "\"message\":{\"type\":\"string\",\"description\":\"Message text to send\"}"
    "},"
    "\"required\":[\"message\"]"
"}";

char *send_message_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *target = json_object_get_string(args, "target", "stdout");
    const char *message = json_object_get_string(args, "message", NULL);

    json_node_t *result = json_new_object();

    if (!message) {
        json_object_set(result, "error", json_new_string("Missing message"));
    } else if (strcmp(target, "stdout") == 0) {
        printf("%s\n", message);
        json_object_set(result, "status", json_new_string("sent"));
        json_object_set(result, "target", json_new_string("stdout"));
    } else if (strcmp(target, "local") == 0) {
        /* Save to output file */
        const char *home = getenv("SLERMES_HOME");
        if (!home) home = getenv("HOME");
        if (!home) home = "/tmp";

        char dir[4096], path[4096];
        snprintf(dir, sizeof(dir), "%s/.slermes/output", home);
        mkdir(dir, 0755);

        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
        snprintf(path, sizeof(path), "%s/message_%04d%02d%02d_%02d%02d%02d.txt",
                 dir, tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
                 tm->tm_hour, tm->tm_min, tm->tm_sec);

        FILE *f = fopen(path, "w");
        if (f) {
            fputs(message, f);
            fclose(f);
            json_object_set(result, "status", json_new_string("saved"));
            json_object_set(result, "path", json_new_string(path));
        } else {
            json_object_set(result, "error", json_new_string("Cannot write file"));
        }
    } else {
        json_object_set(result, "error", json_new_string("Unsupported target"));
    }

    char *json_out = json_serialize(result);
    json_free(result);
    json_free(args);
    return json_out;
}

void registry_init_send_message(void) {
    registry_register("send_message",
        "Send a message to a platform. Supports: 'stdout' (print to console), 'local' (save to file). "
        "Use for delivering output to the user.",
        SCHEMA, send_message_handler);
}
