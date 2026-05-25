/*
 * nous_rate_guard.c — Cross-session rate limit guard for Nous Portal.
 * Port of Python agent/nous_rate_guard.py (325 lines).
 *
 * Writes/reads a shared JSON file (~/.hermes/rate_limits/nous.json)
 * to track cross-session rate limit state.
 */
#include "nous_rate_guard.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

/* --- Constants --- */

#define STATE_SUBDIR    "rate_limits"
#define STATE_FILENAME  "nous.json"
#define DEFAULT_COOLDOWN 300.0  /* 5 minutes */

/* --- Internal helpers --- */

/* Build path to the state file: <hermes_home>/rate_limits/nous.json */
static int state_file_path(const char *hermes_home, char *out, size_t sz) {
    if (!hermes_home || !hermes_home[0] || !out || sz == 0)
        return -1;
    int n = snprintf(out, sz, "%s/%s/%s", hermes_home, STATE_SUBDIR, STATE_FILENAME);
    if (n < 0 || (size_t)n >= sz)
        return -1;
    return 0;
}

/* Ensure the rate_limits directory exists */
static int ensure_state_dir(const char *hermes_home) {
    char dir[4096];
    int n = snprintf(dir, sizeof(dir), "%s/%s", hermes_home, STATE_SUBDIR);
    if (n < 0 || (size_t)n >= sizeof(dir))
        return -1;
    struct stat st;
    if (stat(dir, &st) == 0) {
        if (S_ISDIR(st.st_mode))
            return 0;
        errno = ENOTDIR;
        return -1;
    }
    return mkdir(dir, 0755);
}

/* Atomic write: write to temp file, then rename (POSIX atomic for same-fs) */
static int atomic_write(const char *path, const char *content) {
    /* Create temp file in the same directory (same filesystem) */
    size_t plen = strlen(path);
    char *tmp_path = (char *)malloc(plen + 16);
    if (!tmp_path) return -1;
    memcpy(tmp_path, path, plen);
    memcpy(tmp_path + plen, ".tmp_XXXXXX", 12);  /* mkstemp needs exactly 6 X's at end */
    tmp_path[plen + 12] = '\0';
    /* Use mkstemp for safe temp file creation */
    int fd = mkstemp(tmp_path);
    if (fd < 0) {
        free(tmp_path);
        return -1;
    }
    /* Write content */
    size_t clen = strlen(content);
    ssize_t written = write(fd, content, clen);
    int write_err = (written < 0 || (size_t)written != clen) ? errno : 0;
    close(fd);
    if (write_err) {
        unlink(tmp_path);
        free(tmp_path);
        errno = write_err;
        return -1;
    }
    /* Atomic rename */
    if (rename(tmp_path, path) != 0) {
        int rename_err = errno;
        unlink(tmp_path);
        free(tmp_path);
        errno = rename_err;
        return -1;
    }
    free(tmp_path);
    return 0;
}

/* --- Public API --- */

void nous_rate_guard_record(const char *hermes_home,
                             double reset_at,
                             double default_cooldown_secs) {
    if (!hermes_home || !hermes_home[0]) return;

    if (ensure_state_dir(hermes_home) != 0)
        return;

    double now = (double)time(NULL);
    double effective_reset = reset_at;

    if (effective_reset <= now)
        effective_reset = now + (default_cooldown_secs > 0 ? default_cooldown_secs : DEFAULT_COOLDOWN);

    /* Build state JSON */
    json_t *state = json_new_object();
    json_set(state, "reset_at", json_number(effective_reset));
    json_set(state, "recorded_at", json_number(now));
    json_set(state, "reset_seconds", json_number(effective_reset - now));

    char *json_str = json_serialize(state);
    json_free(state);
    if (!json_str) return;

    char path[4096];
    if (state_file_path(hermes_home, path, sizeof(path)) != 0) {
        free(json_str);
        return;
    }

    atomic_write(path, json_str);
    free(json_str);
}

double nous_rate_guard_remaining(const char *hermes_home) {
    if (!hermes_home || !hermes_home[0]) return -1.0;

    char path[4096];
    if (state_file_path(hermes_home, path, sizeof(path)) != 0)
        return -1.0;

    /* Read the file */
    FILE *f = fopen(path, "r");
    if (!f) return -1.0;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    if (fsize <= 0) { fclose(f); return -1.0; }
    fseek(f, 0, SEEK_SET);

    char *content = (char *)malloc((size_t)fsize + 1);
    if (!content) { fclose(f); return -1.0; }
    size_t nread = fread(content, 1, (size_t)fsize, f);
    fclose(f);
    content[nread] = '\0';

    /* Parse JSON */
    char *err = NULL;
    json_t *state = json_parse(content, &err);
    free(err);
    free(content);
    if (!state) return -1.0;

    double reset_at = json_get_num(state, "reset_at", 0.0);
    json_free(state);

    double now = (double)time(NULL);
    double remaining = reset_at - now;
    if (remaining > 0)
        return remaining;

    /* Expired — clean up the file */
    unlink(path);
    return -1.0;
}

void nous_rate_guard_clear(const char *hermes_home) {
    if (!hermes_home || !hermes_home[0]) return;
    char path[4096];
    if (state_file_path(hermes_home, path, sizeof(path)) != 0)
        return;
    unlink(path);
}

const char *nous_rate_guard_format_remaining(double seconds, char *buf, size_t sz) {
    if (!buf || sz == 0) return buf;
    if (seconds < 0) {
        snprintf(buf, sz, "expired");
        return buf;
    }
    int s = seconds > 0 ? (int)seconds : 0;
    if (s < 60) {
        snprintf(buf, sz, "%ds", s);
    } else if (s < 3600) {
        int m = s / 60, sec = s % 60;
        if (sec)
            snprintf(buf, sz, "%dm %ds", m, sec);
        else
            snprintf(buf, sz, "%dm", m);
    } else {
        int h = s / 3600, m = (s % 3600) / 60;
        if (m)
            snprintf(buf, sz, "%dh %dm", h, m);
        else
            snprintf(buf, sz, "%dh", h);
    }
    return buf;
}
