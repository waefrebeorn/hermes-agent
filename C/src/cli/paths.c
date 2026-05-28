/*
 * paths.c — Path resolution for Hermes C (P21: hermes_constants port).
 * Resolves SLERMES_HOME, XDG-style subdirs, and profile-aware paths.
 */

#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* Active profile name (NULL means default/unset) */
static char active_profile[64] = "";

/* ================================================================
 *  Internal: resolve base SLERMES_HOME directory
 * ================================================================ */

static void resolve_slermes_home(char *buf, size_t sz) {
    const char *env = getenv("SLERMES_HOME");
    if (env && env[0]) {
        snprintf(buf, sz, "%s", env);
        return;
    }
    const char *home = getenv("HOME");
    if (home)
        snprintf(buf, sz, "%s/.slermes", home);
    else
        snprintf(buf, sz, "/tmp/.slermes");
}

/* ================================================================
 *  Public API
 * ================================================================ */

void hermes_get_home(char *buf, size_t sz) {
    resolve_slermes_home(buf, sz);
}

void hermes_config_dir(char *buf, size_t sz) {
    hermes_get_home(buf, sz);
    /* Already points to ~/.slermes/ which IS the config dir */
}

void hermes_data_dir(char *buf, size_t sz) {
    char home[HERMES_PATH_MAX];
    hermes_get_home(home, sizeof(home));
    snprintf(buf, sz, "%s/data", home);
}

void hermes_cache_dir(char *buf, size_t sz) {
    char home[HERMES_PATH_MAX];
    hermes_get_home(home, sizeof(home));
    snprintf(buf, sz, "%s/cache", home);
}

void hermes_log_dir(char *buf, size_t sz) {
    char home[HERMES_PATH_MAX];
    hermes_get_home(home, sizeof(home));
    snprintf(buf, sz, "%s/logs", home);
}

void hermes_resolve_path(const char *sub, char *buf, size_t sz) {
    char home[HERMES_PATH_MAX];
    hermes_get_home(home, sizeof(home));
    if (sub && sub[0])
        snprintf(buf, sz, "%s/%s", home, sub);
    else
        snprintf(buf, sz, "%s", home);
}

void hermes_display_home(void) {
    char buf[HERMES_PATH_MAX];

    hermes_get_home(buf, sizeof(buf));
    printf("SLERMES_HOME:  %s\n", buf);

    hermes_config_dir(buf, sizeof(buf));
    printf("Config dir:    %s\n", buf);

    hermes_data_dir(buf, sizeof(buf));
    printf("Data dir:      %s\n", buf);

    hermes_cache_dir(buf, sizeof(buf));
    printf("Cache dir:     %s\n", buf);

    hermes_log_dir(buf, sizeof(buf));
    printf("Log dir:       %s\n", buf);

    const char *profile = hermes_get_profile();
    if (profile && profile[0])
        printf("Profile:       %s\n", profile);
    else
        printf("Profile:       (default)\n");
}

void hermes_set_profile(const char *name) {
    if (name && name[0])
        snprintf(active_profile, sizeof(active_profile), "%s", name);
    else
        active_profile[0] = '\0';
}

const char *hermes_get_profile(void) {
    return active_profile[0] ? active_profile : NULL;
}
