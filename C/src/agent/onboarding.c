/*
 * hermes_onboarding.c — Contextual first-touch onboarding hints.
 * Port of Python agent/onboarding.py.
 *
 * Shows one-time hints the first time a user hits a behavior fork.
 * State tracked in HERMES_HOME/onboarding.json (simple flat JSON).
 *
 * MIT License — WuBu Slermes Project
 */

#include "hermes_onboarding.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

/* ================================================================
 *  Hint text generators
 * ================================================================ */

const char *onboarding_busy_input_hint_gateway(const char *mode) {
    if (mode && strcmp(mode, "queue") == 0) {
        return "💡 First-time tip — I queued your message instead of interrupting. "
               "Send /busy interrupt to make new messages stop the current task "
               "immediately, or /busy status to check. This notice won't appear again.";
    }
    if (mode && strcmp(mode, "steer") == 0) {
        return "💡 First-time tip — I steered your message into the current run; "
               "it will arrive after the next tool call instead of interrupting. "
               "Send /busy interrupt or /busy queue to change this, or "
               "/busy status to check. This notice won't appear again.";
    }
    /* Default: interrupt mode */
    return "💡 First-time tip — I just interrupted my current task to answer you. "
           "Send /busy queue to queue follow-ups for after the current task instead, "
           "/busy steer to inject them mid-run without interrupting, or "
           "/busy status to check. This notice won't appear again.";
}

const char *onboarding_busy_input_hint_cli(const char *mode) {
    if (mode && strcmp(mode, "queue") == 0) {
        return "(tip) Your message was queued for the next turn. "
               "Use /busy interrupt to make Enter stop the current run instead, "
               "or /busy steer to inject mid-run. This tip only shows once.";
    }
    if (mode && strcmp(mode, "steer") == 0) {
        return "(tip) Your message was steered into the current run; it arrives "
               "after the next tool call. Use /busy interrupt or /busy queue to "
               "change this. This tip only shows once.";
    }
    /* Default: interrupt mode */
    return "(tip) Your message interrupted the current run. "
           "Use /busy queue to queue messages for the next turn instead, "
           "or /busy steer to inject mid-run. This tip only shows once.";
}

const char *onboarding_tool_progress_hint_gateway(void) {
    return "💡 First-time tip — that tool took a while and I'm streaming every step. "
           "If the progress messages feel noisy, send /verbose to cycle modes "
           "(all \u2192 new \u2192 off). This notice won't appear again.";
}

const char *onboarding_tool_progress_hint_cli(void) {
    return "(tip) That tool ran for a while. Use /verbose to cycle tool-progress "
           "display modes (all -> new -> off -> verbose). This tip only shows once.";
}

const char *onboarding_openclaw_residue_hint_cli(void) {
    return "A legacy OpenClaw directory was detected at ~/.openclaw/.\n"
           "To port your config, memory, and skills over to Hermes, run "
           "`hermes claw migrate`.\n"
           "If you've already migrated and want to archive the old directory, "
           "run `hermes claw cleanup` (renames it to ~/.openclaw.pre-migration \u2014 "
           "OpenClaw will stop working after this).\n"
           "This tip only shows once.";
}

/* ================================================================
 *  Path helpers
 * ================================================================ */

char *onboarding_default_path(void) {
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) return NULL;

    char *path = (char *)malloc(strlen(home) + 32);
    if (!path) return NULL;

    if (getenv("HERMES_HOME")) {
        sprintf(path, "%s/onboarding.json", home);
    } else {
        sprintf(path, "%s/.hermes/onboarding.json", home);
    }
    return path;
}

/* ================================================================
 *  State: read/write onboarding.json (flat JSON: {"flag": true})
 * ================================================================ */

bool onboarding_is_seen(const char *onboarding_path, const char *flag) {
    if (!flag) return false;

    const char *path = onboarding_path;
    char *auto_path = NULL;
    if (!path) {
        auto_path = onboarding_default_path();
        path = auto_path;
    }
    if (!path) return false;

    FILE *f = fopen(path, "rb");
    if (!f) {
        free(auto_path);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 2) { fclose(f); free(auto_path); return false; }

    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf) { fclose(f); free(auto_path); return false; }

    size_t n = fread(buf, 1, (size_t)len, f);
    fclose(f);
    buf[n] = '\0';

    json_node_t *root = json_parse(buf, NULL);
    free(buf);

    if (!root) { free(auto_path); return false; }

    bool seen = json_object_get_bool(root, flag, false);
    json_free(root);
    free(auto_path);
    return seen;
}

bool onboarding_mark_seen(const char *onboarding_path, const char *flag) {
    if (!flag) return false;

    const char *path = onboarding_path;
    char *auto_path = NULL;
    if (!path) {
        auto_path = onboarding_default_path();
        path = auto_path;
    }
    if (!path) return false;

    /* Read existing state */
    json_node_t *root = NULL;
    FILE *f = fopen(path, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (len > 2) {
            char *buf = (char *)malloc((size_t)len + 1);
            if (buf) {
                size_t n = fread(buf, 1, (size_t)len, f);
                buf[n] = '\0';
                root = json_parse(buf, NULL);
                free(buf);
            }
        }
        fclose(f);
    }

    if (!root) root = json_new_object();

    /* Set flag */
    json_object_set(root, flag, json_new_bool(true));

    /* Serialize */
    char *json_str = json_serialize(root);
    json_free(root);

    if (!json_str) { free(auto_path); return false; }

    /* Atomic write: write to temp file, then rename */
    char tmp_path[4096];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp.%ld", path, (long)time(NULL));

    f = fopen(tmp_path, "wb");
    if (!f) { free(json_str); free(auto_path); return false; }

    fwrite(json_str, 1, strlen(json_str), f);
    fclose(f);

    if (rename(tmp_path, path) != 0) {
        remove(tmp_path);
        free(json_str);
        free(auto_path);
        return false;
    }

    free(json_str);
    free(auto_path);
    return true;
}

/* ================================================================
 *  OpenClaw residue detection
 * ================================================================ */

bool onboarding_detect_openclaw_residue(void) {
    const char *home = getenv("HOME");
    if (!home) return false;

    char path[4096];
    snprintf(path, sizeof(path), "%s/.openclaw", home);

    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}
