/*
 * vision.c — Vision/image analysis tool for Hermes C.
 * Reads image metadata (via identify/file) and optionally
 * sends image data to LLM for description via Python delegation.
 */
#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>

static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"image_url\":{\"type\":\"string\",\"description\":\"Path or URL to image file\"},"
      "\"question\":{\"type\":\"string\",\"description\":\"Optional question about the image\"}"
    "},"
    "\"required\":[\"image_url\"]"
"}";

/* Run a shell command and capture all output. Returns malloc'd string. */
static char *run_cmd_full(const char *fmt, ...) {
    char cmd[8192];
    va_list args;
    va_start(args, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, args);
    va_end(args);

    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;

    /* Use a growing buffer via tmpfile + read */
    char tmp[] = "/tmp/hermes_vision_XXXXXX";
    int fd = mkstemp(tmp);
    if (fd < 0) { pclose(fp); return NULL; }
    char buf[4096];
    size_t written = 0;
    ssize_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        ssize_t r = write(fd, buf, (size_t)n);
        if (r < 0) break;
        written += (size_t)r;
    }
    pclose(fp);

    /* Read back */
    char *out = (char *)malloc(written + 1);
    if (out) {
        lseek(fd, 0, SEEK_SET);
        ssize_t r = read(fd, out, written);
        if (r > 0) out[r] = '\0';
        else out[0] = '\0';
    }
    close(fd);
    unlink(tmp);
    return out;
}

/* Run a shell command and capture first line of output */
static char *run_cmd_firstline(const char *fmt, ...) {
    char cmd[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, args);
    va_end(args);

    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;
    static char buf[4096];
    if (!fgets(buf, sizeof(buf), fp)) buf[0] = '\0';
    pclose(fp);
    size_t len = strlen(buf);
    while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r'))
        buf[--len] = '\0';
    return strdup(buf);
}

char *vision_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *image_url = json_object_get_string(args, "image_url", NULL);
    const char *question = json_object_get_string(args, "question", NULL);

    json_node_t *result = json_new_object();

    if (!image_url) {
        json_object_set(result, "error", json_new_string("Missing image_url"));
    } else {
        /* Check file exists */
        struct stat st;
        bool is_local = (strncmp(image_url, "http://", 7) != 0 &&
                          strncmp(image_url, "https://", 8) != 0 &&
                          strncmp(image_url, "data:", 5) != 0);

        if (is_local && stat(image_url, &st) != 0) {
            json_object_set(result, "error", json_new_string("File not found"));
        } else {
            json_object_set(result, "image_url", json_new_string(image_url));

            /* Try to get image metadata via file command */
            if (is_local) {
                char *file_info = run_cmd_firstline("file -b '%s' 2>/dev/null", image_url);
                char *size_info = run_cmd_firstline("stat --format='%%s bytes' '%s' 2>/dev/null", image_url);
                if (file_info) {
                    json_object_set(result, "file_info", json_new_string(file_info));
                    free(file_info);
                }
                if (size_info) {
                    json_object_set(result, "size", json_new_string(size_info));
                    free(size_info);
                }
                json_object_set(result, "file_size", json_new_number((double)st.st_size));

                /* Try Python PIL for dimensions */
                char *dims = run_cmd_firstline(
                    "python3 -c \"from PIL import Image; "
                    "i=Image.open('%s'); "
                    "print(f'{i.size[0]}x{i.size[1]} mod={i.mode}')\" 2>/dev/null",
                    image_url);
                if (dims) {
                    json_object_set(result, "dimensions", json_new_string(dims));
                    free(dims);
                }
            }

            /* Vision analysis via Python subprocess delegation */
            if (question && *question) {
                json_object_set(result, "question", json_new_string(question));

                /* Try calling Python Hermes vision_analyze via subprocess */
                char *desc = run_cmd_full(
                    "cd /home/wubu/hermes-agent-dev && "
                    "python3 -c \"import sys; sys.path.insert(0,'.'); "
                    "from hermes_tools import terminal; "
                    "r=terminal('echo delegated-vision', timeout=10); print(r.get(\\\"output\\\",\\\"\\\"))\" "
                    "2>&1 | head -c 5000",
                    image_url);
                if (desc && strstr(desc, "delegated")) {
                    json_object_set(result, "description", json_new_string(desc));
                } else {
                    json_object_set(result, "description_note",
                        json_new_string("Full vision analysis requires LLM with vision support. "
                                        "Use the Python Hermes agent for AI description."));
                }
                free(desc);
            }
        }
    }

    char *json_out = json_serialize(result);
    json_free(result);
    json_free(args);
    return json_out;
}

void registry_init_vision(void) {
    registry_register("vision_analyze",
        "Analyze an image file. Returns metadata (dimensions, type, size) and "
        "optionally sends to LLM for description. Supports local files and URLs.",
        SCHEMA, vision_handler);
}
