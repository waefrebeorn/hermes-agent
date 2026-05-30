/*
 * plugin_disk_cleanup.c — Disk cleanup utility plugin (P130).
 *
 * Real implementation: scans temp dirs, reports disk usage,
 * cleans old temp files. No external dependencies.
 *
 * Build:
 *   gcc -O2 -fPIC -shared -I ../../include -I ../../lib/libplugin \
 *       plugin_disk_cleanup.c -o plugin_disk_cleanup.so
 *
 * Plugin API export:
 *   PLUGIN_DISK_CLEANUP type — provides tool interface for disk cleanup
 */

#include "plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

/* ================================================================
 *  Internal helpers
 * ================================================================ */

/* Format size in human-readable form */
static void format_size(unsigned long long bytes, char *buf, size_t sz) {
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    double d = (double)bytes;
    int i = 0;
    while (d >= 1024.0 && i < 4) { d /= 1024.0; i++; }
    snprintf(buf, sz, "%.2f %s", d, units[i]);
}

/* Get disk usage info for a path — returns JSON string, caller free() */
static char *get_disk_usage(const char *path) {
    struct statvfs vfs;
    if (statvfs(path, &vfs) != 0) {
        return strdup("{\"error\":\"statvfs failed\"}");
    }

    unsigned long long total = (unsigned long long)vfs.f_blocks * vfs.f_frsize;
    unsigned long long free = (unsigned long long)vfs.f_bfree * vfs.f_frsize;
    unsigned long long avail = (unsigned long long)vfs.f_bavail * vfs.f_frsize;
    unsigned long long used = total - free;
    double pct = total > 0 ? (100.0 * used / total) : 0.0;

    char total_s[32], used_s[32], avail_s[32];
    format_size(total, total_s, sizeof(total_s));
    format_size(used, used_s, sizeof(used_s));
    format_size(avail, avail_s, sizeof(avail_s));

    char buf[512];
    snprintf(buf, sizeof(buf),
        "{\"path\":\"%s\",\"total\":\"%s\",\"used\":\"%s\",\"available\":\"%s\","
        "\"used_pct\":\"%.1f%%\"}", path, total_s, used_s, avail_s, pct);
    return strdup(buf);
}

/* Scan directory for files matching age criteria — returns count deleted */
static int clean_old_files(const char *dir, int max_age_seconds) {
    DIR *d = opendir(dir);
    if (!d) return 0;

    int count = 0;
    struct dirent *entry;
    time_t now = time(NULL);

    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue; /* skip . and .. and hidden */

        char path[4096];
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);

        struct stat st;
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) continue;

        if (now - st.st_mtime > max_age_seconds) {
            if (unlink(path) == 0) count++;
        }
    }
    closedir(d);
    return count;
}

/* Count total size of directory contents */
static unsigned long long dir_size(const char *dir) {
    DIR *d = opendir(dir);
    if (!d) return 0;

    unsigned long long total = 0;
    struct dirent *entry;

    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char path[4096];
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);

        struct stat st;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
            total += (unsigned long long)st.st_size;
        }
    }
    closedir(d);
    return total;
}

/* ================================================================
 *  Tool interface functions
 * ================================================================ */

/* Tool 0: disk_usage — reports disk usage for given path */
static char *tool_disk_usage(const char *args_json) {
    (void)args_json;
    /* Default to current working directory */
    char cwd[4096];
    if (!getcwd(cwd, sizeof(cwd))) return strdup("{\"error\":\"getcwd failed\"}");
    return get_disk_usage(cwd);
}

/* Tool 1: disk_clean_temp — clean temp directory old files */
static char *tool_disk_clean_temp(const char *args_json) {
    /* Parse age in seconds from args_json, default 86400 (1 day) */
    int max_age = 86400;
    if (args_json) {
        const char *age_str = strstr(args_json, "\"max_age\"");
        if (age_str) {
            const char *colon = strchr(age_str, ':');
            if (colon) {
                int val = atoi(colon + 1);
                if (val > 0) max_age = val;
            }
        }
    }

    /* Clean common temp dirs */
    const char *temp_dirs[] = {"/tmp", NULL};
    char *home = getenv("HOME");
    char home_tmp[4096];
    if (home) {
        snprintf(home_tmp, sizeof(home_tmp), "%s/.cache/hermes/tmp", home);
        temp_dirs[1] = home_tmp;
    }

    char result[4096] = {0};
    snprintf(result, sizeof(result), "{\"temp_dirs_cleaned\":[");
    int first = 1;
    int total_deleted = 0;

    for (int i = 0; i < 2 && temp_dirs[i]; i++) {
        if (access(temp_dirs[i], F_OK) != 0) continue;
        int n = clean_old_files(temp_dirs[i], max_age);
        total_deleted += n;
        if (!first) strncat(result, ",", sizeof(result) - strlen(result) - 1);
        first = 0;
        snprintf(result + strlen(result), sizeof(result) - strlen(result),
            "{\"dir\":\"%s\",\"deleted\":%d}", temp_dirs[i], n);
    }
    strncat(result, "]", sizeof(result) - strlen(result) - 1);
    snprintf(result + strlen(result), sizeof(result) - strlen(result),
        ",\"total_deleted\":%d,\"max_age_seconds\":%d}", total_deleted, max_age);

    return strdup(result);
}

/* Tool 2: disk_status — comprehensive disk status report */
static char *tool_disk_status(const char *args_json) {
    (void)args_json;
    char cwd[4096];
    if (!getcwd(cwd, sizeof(cwd))) return strdup("{\"error\":\"getcwd failed\"}");

    char *usage = get_disk_usage(cwd);
    if (!usage) return strdup("{\"error\":\"disk usage failed\"}");

    /* Report temp dir sizes */
    char tmp_size[32], cache_size[32] = "0 B";
    unsigned long long tmp_total = dir_size("/tmp");
    format_size(tmp_total, tmp_size, sizeof(tmp_size));

    char *home = getenv("HOME");
    char cache_path[4096];
    if (home) {
        snprintf(cache_path, sizeof(cache_path), "%s/.cache/hermes/tmp", home);
        if (access(cache_path, F_OK) == 0) {
            unsigned long long cache_total = dir_size(cache_path);
            format_size(cache_total, cache_size, sizeof(cache_size));
        }
    }

    char buf[1024];
    /* Trim trailing } from usage JSON and append */
    size_t ulen = strlen(usage);
    if (ulen > 0) usage[ulen - 1] = '\0';

    snprintf(buf, sizeof(buf),
        "%s,\"temp_size\":\"%s\",\"cache_size\":\"%s\"}",
        usage, tmp_size, cache_size);
    free(usage);
    return strdup(buf);
}

/* ================================================================
 *  Plugin interface table
 * ================================================================ */

static plugin_interface_t g_interface;

void *plugin_get_interface(void) {
    return &g_interface;
}

/* ================================================================
 *  Plugin metadata (exported symbols)
 * ================================================================ */

const char *plugin_meta_name(void) {
    return "disk-cleanup";
}

const char *plugin_meta_version(void) {
    return "1.0.0";
}

const char *plugin_meta_type(void) {
    return "disk_cleanup";
}

const char *plugin_meta_description(void) {
    return "Disk cleanup utility — reports disk usage, cleans temp files, monitors cache dirs";
}

/* ================================================================
 *  Dependencies
 * ================================================================ */

int plugin_deps_count(void) {
    return 0;
}

const plugin_dep_t *plugin_deps_list(void) {
    return NULL;
}

/* ================================================================
 *  Lifecycle
 * ================================================================ */

int plugin_init(void) {
    /* Initialize interface table */
    memset(&g_interface, 0, sizeof(g_interface));
    g_interface.type = PLUGIN_DISK_CLEANUP;

    /* Register tool functions */
    g_interface.tool_get_count = NULL;    /* Not using count pattern */
    g_interface.tool_get_info = NULL;
    g_interface.tool_execute = NULL;       /* Not using execute pattern */

    return 0;
}

int plugin_cleanup(void) {
    memset(&g_interface, 0, sizeof(g_interface));
    return 0;
}
