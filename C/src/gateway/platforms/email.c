/*
 * email.c — Email gateway for Hermes C.
 * Sends via system sendmail/popen. Polls a maildir for incoming.
 *
 * Env vars:
 *   EMAIL_SEND_CMD    — Send command (default: "sendmail -t")
 *   EMAIL_FROM        — From address (default: "hermes@localhost")
 *   EMAIL_POLL_DIR    — Maildir to poll for incoming (optional)
 *   EMAIL_POLL_INTERVAL — Seconds between polls (default: 30)
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "hermes_gateway.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

static char g_email_from[256] = "hermes@localhost";
static char g_email_send_cmd[256] = "sendmail -t";
static char g_email_poll_dir[1024] = "";
static int g_email_poll_interval = 30;

void email_set_from(const char *from) {
    if (from) snprintf(g_email_from, sizeof(g_email_from), "%s", from);
}

bool email_send_message(http_client_t *http, const char *to, const char *subject, const char *body) {
    (void)http;
    if (!to || !body) return false;

    /* Build email and pipe to sendmail */
    FILE *fp = popen(g_email_send_cmd, "w");
    if (!fp) return false;

    fprintf(fp, "From: %s\n", g_email_from);
    fprintf(fp, "To: %s\n", to);
    fprintf(fp, "Subject: %s\n", subject ? subject : "Hermes Message");
    fprintf(fp, "Content-Type: text/plain; charset=UTF-8\n");
    fprintf(fp, "\n%s\n", body);
    int rc = pclose(fp);
    return rc == 0;
}

/* Poll for incoming email (simple: read new files from maildir new/ dir) */
json_node_t *email_poll_messages(http_client_t *http) {
    (void)http;
    if (!g_email_poll_dir[0]) return NULL;

    char new_dir[2048];
    snprintf(new_dir, sizeof(new_dir), "%s/new", g_email_poll_dir);

    DIR *d = opendir(new_dir);
    if (!d) return NULL;

    json_node_t *results = json_array();
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char path[2048];
        snprintf(path, sizeof(path), "%s/%s", new_dir, entry->d_name);

        /* Read first line as message */
        FILE *f = fopen(path, "r");
        if (!f) continue;

        char line[4096];
        line[0] = '\0';
        if (fgets(line, sizeof(line), f)) {
            size_t len = strlen(line);
            while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
                line[--len] = '\0';
        }
        fclose(f);

        if (line[0]) {
            json_node_t *msg = json_object();
            json_set(msg, "chat_id", json_string(g_email_from));
            json_set(msg, "text", json_string(line));
            json_append(results, msg);
        }

        /* Move to cur/ so we don't process again */
        char cur_path[2048];
        snprintf(cur_path, sizeof(cur_path), "%s/cur/%s", g_email_poll_dir, entry->d_name);
        rename(path, cur_path);
    }
    closedir(d);
    return json_len(results) > 0 ? results : NULL;
}

const char *email_get_chat_id(json_node_t *update) {
    return json_get_str(update, "chat_id", "");
}

const char *email_get_text(json_node_t *update) {
    return json_get_str(update, "text", "");
}
