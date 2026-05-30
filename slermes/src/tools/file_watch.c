/*
 * file_watch.c — File/directory watch tool using inotify.
 * Monitors file/directory changes via Linux inotify API.
 * Available on Linux and WSL.
 */
#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>

/* Schema */
static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"path\":{\"type\":\"string\",\"description\":\"File or directory path to watch\"},"
      "\"events\":{\"type\":\"string\",\"description\":\"Events to watch: 'modify' (default), 'create', 'delete', 'access', 'attrib', or 'all' (comma-separated)\",\"default\":\"modify\"},"
      "\"timeout\":{\"type\":\"integer\",\"description\":\"Wait timeout seconds (default: 5, max: 60)\",\"default\":5},"
      "\"recursive\":{\"type\":\"boolean\",\"description\":\"Watch directory recursively (default: false)\",\"default\":false}"
    "},"
    "\"required\":[\"path\"]"
"\"}";

/* Convert event name string to inotify mask */
static uint32_t event_name_to_mask(const char *name) {
    if (strcmp(name, "access") == 0) return IN_ACCESS;
    if (strcmp(name, "modify") == 0) return IN_MODIFY;
    if (strcmp(name, "create") == 0) return IN_CREATE;
    if (strcmp(name, "delete") == 0) return IN_DELETE;
    if (strcmp(name, "attrib") == 0) return IN_ATTRIB;
    if (strcmp(name, "close_write") == 0) return IN_CLOSE_WRITE;
    if (strcmp(name, "open") == 0) return IN_OPEN;
    if (strcmp(name, "move") == 0) return IN_MOVE;
    if (strcmp(name, "all") == 0) return IN_ALL_EVENTS;
    return IN_MODIFY; /* default */
}

/* Parse comma-separated event names into mask */
static uint32_t parse_events_mask(const char *events_str) {
    if (!events_str || !events_str[0]) return IN_MODIFY;

    /* Handle "all" shortcut */
    if (strcmp(events_str, "all") == 0) return IN_ALL_EVENTS;

    uint32_t mask = 0;
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", events_str);
    char *token = strtok(buf, ",");
    while (token) {
        /* Trim whitespace */
        while (*token == ' ') token++;
        char *end = token + strlen(token) - 1;
        while (end > token && *end == ' ') *end-- = '\0';
        mask |= event_name_to_mask(token);
        token = strtok(NULL, ",");
    }
    return mask ? mask : IN_MODIFY;
}

/* Convert inotify event mask to comma-separated event names */
static const char *mask_to_event_names(uint32_t mask, char *buf, size_t bufsz) {
    buf[0] = '\0';
    size_t pos = 0;
    #define APPEND_EVT(name, val) do { \
        if (mask & (val)) { \
            if (pos > 0 && pos < bufsz - 3) { buf[pos++] = ','; buf[pos] = '\0'; } \
            size_t n = strlen(name); \
            if (pos + n < bufsz - 1) { memcpy(buf + pos, name, n); pos += n; buf[pos] = '\0'; } \
        } \
    } while(0)
    APPEND_EVT("access", IN_ACCESS);
    APPEND_EVT("modify", IN_MODIFY);
    APPEND_EVT("create", IN_CREATE);
    APPEND_EVT("delete", IN_DELETE);
    APPEND_EVT("attrib", IN_ATTRIB);
    APPEND_EVT("close_write", IN_CLOSE_WRITE);
    APPEND_EVT("open", IN_OPEN);
    APPEND_EVT("move", IN_MOVED_FROM);
    APPEND_EVT("move", IN_MOVED_TO);
    #undef APPEND_EVT
    return buf;
}

char *file_watch_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *path = json_object_get_string(args, "path", NULL);
    const char *events_str = json_object_get_string(args, "events", "modify");
    int timeout = (int)json_object_get_number(args, "timeout", 5);
    bool recursive = json_object_get_bool(args, "recursive", false);

    if (!path) {
        json_free(args);
        return strdup("{\"error\":\"Missing path\"}");
    }

    if (timeout < 1) timeout = 1;
    if (timeout > 60) timeout = 60;

    /* Initialize inotify */
    int fd = inotify_init1(IN_NONBLOCK);
    if (fd < 0) {
        json_free(args);
        return strdup("{\"error\":\"inotify_init failed (not supported on this system)\"}");
    }

    /* Load event mask from events param */
    uint32_t watch_mask = parse_events_mask(events_str);
    /* Add IN_DELETE_SELF for file deletion detection */
    if (recursive) watch_mask |= IN_ONLYDIR;

    /* Add watch */
    int wd = inotify_add_watch(fd, path, watch_mask | IN_DELETE_SELF);
    if (wd < 0) {
        close(fd);
        json_free(args);
        char errmsg[1024];
        snprintf(errmsg, sizeof(errmsg),
            "{\"error\":\"Cannot watch '%s': %s\"}", path, strerror(errno));
        return strdup(errmsg);
    }

    /* Poll for events */
    struct pollfd pfd = { .fd = fd, .events = POLLIN };
    int poll_result = poll(&pfd, 1, timeout * 1000);

    json_node_t *result = json_new_object();
    json_object_set(result, "path", json_new_string(path));

    if (poll_result < 0) {
        json_object_set(result, "error", json_new_string("poll failed"));
    } else if (poll_result == 0) {
        json_object_set(result, "events", json_new_array());
        json_object_set(result, "timeout", json_new_bool(true));
    } else {
        /* Read events */
        char event_buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
        json_node_t *events_arr = json_new_array();

        ssize_t len = read(fd, event_buf, sizeof(event_buf));
        if (len > 0) {
            const struct inotify_event *ev;
            for (char *ptr = event_buf; ptr < event_buf + len;
                 ptr += sizeof(struct inotify_event) + ev->len) {
                ev = (const struct inotify_event *)ptr;

                json_node_t *e = json_new_object();
                json_object_set(e, "wd", json_new_number((double)ev->wd));
                json_object_set(e, "cookie", json_new_number((double)ev->cookie));
                json_object_set(e, "is_dir", json_new_bool((ev->mask & IN_ISDIR) != 0));
                char evt_names[128];
                mask_to_event_names(ev->mask, evt_names, sizeof(evt_names));
                json_object_set(e, "events", json_new_string(evt_names));
                if (ev->len > 0 && ev->name[0])
                    json_object_set(e, "name", json_new_string(ev->name));
                json_array_append(events_arr, e);
            }
        }
        json_object_set(result, "events", events_arr);
        json_object_set(result, "timeout", json_new_bool(false));
    }

    /* Cleanup */
    inotify_rm_watch(fd, wd);
    close(fd);
    json_free(args);

    char *json_out = json_serialize(result);
    json_free(result);
    return json_out;
}

void registry_init_file_watch(void) {
    registry_register("file_watch",
        "Watch a file or directory for changes using inotify. "
        "Returns events within the timeout window. "
        "Events: modify (default), create, delete, access, attrib, close_write, open, move, or 'all'. "
        "Timeout: 1-60 seconds (default: 5). "
        "Available on Linux and WSL.",
        SCHEMA, file_watch_handler);
}
