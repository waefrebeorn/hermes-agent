/**
 * rate_guard.c — Cross-session rate limit guard implementation.
 */
#include "rate_guard.h"
#include "hermes_json.h"
#include "path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

/* ── Internal helpers ───────────────────────────────────── */

static void build_state_path(const char *name, char *buf, size_t bufsz) {
    const char *home = getenv("SLERMES_HOME");
    if (!home || !*home) {
        home = getenv("HERMES_HOME");
    }
    if (!home || !*home) {
        home = getenv("HOME");
    }
    if (!home) home = ".";

    char dir[1024];
    snprintf(dir, sizeof(dir), "%s/%s", home, RATE_GUARD_SUBDIR);
    mkdir(dir, 0755);

    snprintf(buf, bufsz, "%s/%s.json", dir, name ? name : "unknown");
}

/* ── API ────────────────────────────────────────────────── */

void rate_guard_record(const char *name,
                       const rate_guard_state_t *state,
                       double cooldown) {
    char path[1152];
    build_state_path(name, path, sizeof(path));

    double now = (double)time(NULL);
    double reset_at = now + (cooldown > 0 ? cooldown : RATE_GUARD_DEFAULT_COOLDOWN);

    /* Try to extract reset time from state headers */
    if (state) {
        double best_reset = 0.0;
        const rate_guard_bucket_t *buckets[] = {
            &state->requests_hour, &state->requests_min,
            &state->tokens_hour, &state->tokens_min
        };
        for (int i = 0; i < 4; i++) {
            if (buckets[i]->valid && buckets[i]->reset_seconds > 0) {
                double candidate = now + buckets[i]->reset_seconds;
                if (candidate > best_reset) best_reset = candidate;
            }
        }
        if (best_reset > now)
            reset_at = best_reset;
    }

    /* Build JSON state */
    json_t *root = json_object();
    json_set(root, "reset_at", json_number(reset_at));
    json_set(root, "recorded_at", json_number(now));
    json_set(root, "reset_seconds", json_number(reset_at - now));

    char *serialized = json_serialize(root);
    json_free(root);
    if (!serialized) return;

    /* Atomic write: write to temp file, rename */
    char tmp_path[1280];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp.%d", path, getpid());

    FILE *f = fopen(tmp_path, "w");
    if (f) {
        fputs(serialized, f);
        fclose(f);
        rename(tmp_path, path);
    }
    free(serialized);

    /* Remove stale temp */
    unlink(tmp_path);
}

double rate_guard_remaining(const char *name) {
    char path[1152];
    build_state_path(name, path, sizeof(path));

    FILE *f = fopen(path, "r");
    if (!f) return 0.0;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0) { fclose(f); return 0.0; }

    char *content = (char *)malloc((size_t)len + 1);
    if (!content) { fclose(f); return 0.0; }
    size_t nread = fread(content, 1, (size_t)len, f);
    content[nread] = '\0';
    fclose(f);

    char *err = NULL;
    json_t *root = json_parse(content, &err);
    free(content);
    free(err);
    if (!root) return 0.0;

    double reset_at = json_get_num(root, "reset_at", 0.0);
    json_free(root);

    double now = (double)time(NULL);
    double remaining = reset_at - now;

    if (remaining > 0) return remaining;

    /* Expired — clean up */
    unlink(path);
    return 0.0;
}

void rate_guard_clear(const char *name) {
    char path[1152];
    build_state_path(name, path, sizeof(path));
    unlink(path);
}

char *rate_guard_format_remaining(double seconds) {
    if (seconds <= 0) return strdup("0s");
    int s = (int)seconds;
    char buf[64];
    if (s < 60) {
        snprintf(buf, sizeof(buf), "%ds", s);
    } else if (s < 3600) {
        int m = s / 60;
        int sec = s % 60;
        if (sec)
            snprintf(buf, sizeof(buf), "%dm %ds", m, sec);
        else
            snprintf(buf, sizeof(buf), "%dm", m);
    } else {
        int h = s / 3600;
        int m = (s % 3600) / 60;
        if (m)
            snprintf(buf, sizeof(buf), "%dh %dm", h, m);
        else
            snprintf(buf, sizeof(buf), "%dh", h);
    }
    return strdup(buf);
}

bool rate_guard_is_genuine(const rate_guard_state_t *state) {
    if (!state) return false;

    const rate_guard_bucket_t *buckets[] = {
        &state->requests_hour, &state->requests_min,
        &state->tokens_hour, &state->tokens_min
    };

    for (int i = 0; i < 4; i++) {
        if (!buckets[i]->valid) continue;
        if (buckets[i]->remaining != 0) continue;
        /* remaining == 0 */
        if (buckets[i]->reset_seconds >= RATE_GUARD_MIN_RESET_BREAKER)
            return true;
        /* Also check if reset is valid but short — transient */
        if (buckets[i]->reset_seconds < RATE_GUARD_MIN_RESET_BREAKER
            && buckets[i]->reset_seconds > 0) {
            /* Short reset: transient, NOT genuine */
            continue;
        }
    }
    return false;
}

void rate_guard_parse_headers(rate_guard_state_t *state,
                               int remaining_req, double reset_req,
                               int remaining_req1h, double reset_req1h,
                               int remaining_tok, double reset_tok,
                               int remaining_tok1h, double reset_tok1h,
                               int http_status) {
    if (!state) return;

    state->http_status = http_status;

    state->requests_min.remaining = remaining_req;
    state->requests_min.reset_seconds = reset_req;
    state->requests_min.valid = (remaining_req >= 0 && reset_req >= 0);

    state->requests_hour.remaining = remaining_req1h;
    state->requests_hour.reset_seconds = reset_req1h;
    state->requests_hour.valid = (remaining_req1h >= 0 && reset_req1h >= 0);

    state->tokens_min.remaining = remaining_tok;
    state->tokens_min.reset_seconds = reset_tok;
    state->tokens_min.valid = (remaining_tok >= 0 && reset_tok >= 0);

    state->tokens_hour.remaining = remaining_tok1h;
    state->tokens_hour.reset_seconds = reset_tok1h;
    state->tokens_hour.valid = (remaining_tok1h >= 0 && reset_tok1h >= 0);
}
