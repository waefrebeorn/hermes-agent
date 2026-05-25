/**
 * @file curator.c
 * @brief B05: Curator — background skill maintenance orchestrator.
 *
 * Manages curator state persistence and provides status for CLI.
 * State is stored in <hermes_home>/skills/.curator_state as JSON.
 */
#include "hermes_curator.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

/* ── State file path ────────────────────────────────────────────── */

static void state_path(char *buf, size_t sz) {
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = ".";
    snprintf(buf, sz, "%s/skills/.curator_state", home);
}

/* ── Public API ─────────────────────────────────────────────────── */

void curator_init_state(curator_state_t *state) {
    if (!state) return;
    memset(state, 0, sizeof(*state));
    state->enabled = true;
    state->paused = false;
    state->run_count = 0;
    state->last_run_at = 0;
    state->last_run_duration = 0.0;
    state->last_run_summary[0] = '\0';
}

bool curator_load_state(curator_state_t *state) {
    if (!state) return false;
    curator_init_state(state);

    char path[1024];
    state_path(path, sizeof(path));

    /* Check if file exists */
    struct stat st;
    if (stat(path, &st) != 0) return false;

    json_t *root = json_parse_file(path, NULL);
    if (!root) return false;

    state->enabled = json_get_bool(root, "enabled", true);
    state->paused = json_get_bool(root, "paused", false);
    state->run_count = (int)json_get_num(root, "run_count", 0);
    state->last_run_at = (time_t)json_get_num(root, "last_run_at", 0);
    state->last_run_duration = json_get_num(root, "last_run_duration_seconds", 0.0);

    const char *summary = json_get_str(root, "last_run_summary", "");
    if (summary)
        snprintf(state->last_run_summary, sizeof(state->last_run_summary),
                 "%s", summary);

    json_free(root);
    return true;
}

void curator_save_state(const curator_state_t *state) {
    if (!state) return;

    char path[1024];
    state_path(path, sizeof(path));

    /* Ensure directory exists */
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        char mkdir_cmd[2048];
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s 2>/dev/null", dir);
        int mkrc = system(mkdir_cmd);
        (void)mkrc;
    }

    json_t *root = json_object();
    json_set(root, "enabled", json_bool(state->enabled));
    json_set(root, "paused", json_bool(state->paused));
    json_set(root, "run_count", json_number((double)state->run_count));
    json_set(root, "last_run_at", json_number((double)state->last_run_at));
    json_set(root, "last_run_duration_seconds",
             json_number(state->last_run_duration));
    json_set(root, "last_run_summary",
             json_string(state->last_run_summary[0] ?
                         state->last_run_summary : ""));

    char *out = json_serialize_pretty(root, 2);
    if (out) {
        FILE *f = fopen(path, "w");
        if (f) {
            fputs(out, f);
            fclose(f);
        }
        free(out);
    }
    json_free(root);
}

void curator_record_run(curator_state_t *state, double duration_secs,
                         const char *summary) {
    if (!state) return;
    state->last_run_at = time(NULL);
    state->last_run_duration = duration_secs;
    state->run_count++;
    if (summary)
        snprintf(state->last_run_summary, sizeof(state->last_run_summary),
                 "%s", summary);
    curator_save_state(state);
}

void curator_format_duration(double seconds, char *buf, size_t sz) {
    if (!buf || sz == 0) return;

    if (seconds < 60.0) {
        snprintf(buf, sz, "%.0fs", seconds);
    } else if (seconds < 3600.0) {
        int m = (int)(seconds / 60.0);
        int s = (int)seconds % 60;
        snprintf(buf, sz, "%dm %ds", m, s);
    } else if (seconds < 86400.0) {
        int h = (int)(seconds / 3600.0);
        int m = ((int)seconds % 3600) / 60;
        snprintf(buf, sz, "%dh %dm", h, m);
    } else {
        int d = (int)(seconds / 86400.0);
        int h = ((int)seconds % 86400) / 3600;
        snprintf(buf, sz, "%dd %dh", d, h);
    }
}

void curator_format_reltime(time_t t, char *buf, size_t sz) {
    if (!buf || sz == 0) return;
    if (t == 0) {
        snprintf(buf, sz, "never");
        return;
    }

    time_t now = time(NULL);
    double diff = difftime(now, t);
    if (diff < 0) diff = 0;

    if (diff < 60.0) {
        snprintf(buf, sz, "%.0fs ago", diff);
    } else if (diff < 3600.0) {
        snprintf(buf, sz, "%dm ago", (int)(diff / 60.0));
    } else if (diff < 86400.0) {
        snprintf(buf, sz, "%dh ago", (int)(diff / 3600.0));
    } else {
        snprintf(buf, sz, "%dd ago", (int)(diff / 86400.0));
    }
}
