/*
 * test_logger.c — Logger unit tests (M10).
 *
 * Tests init/shutdown cycle, log level filtering, output format,
 * agent.log vs errors.log routing.
 *
 * Build:
 *   gcc -O2 -g -Wall -Werror -I include -I lib/libjson -I lib/libplugin \
 *       tests/test_logger.c src/agent/logger.c \
 *       -o /tmp/hermes_test_logger -lm -Wl,--unresolved-symbols=ignore-all
 *
 * Run:
 *   SLERMES_HOME=/tmp/hermes_log_test /tmp/hermes_test_logger
 *   rm -rf /tmp/hermes_log_test
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

/* Helper: check if a file exists with content (non-zero size) */
static int file_has_content(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0 && st.st_size > 0);
}

/* Helper: check if a file has NO content (missing or zero-byte).
 * Zero-byte files happen when fopen(path, "a") creates the file
 * but nothing is written to it. */
static int file_empty_or_missing(const char *path) {
    struct stat st;
    return stat(path, &st) != 0 || st.st_size == 0;
}

/* Helper: count lines in a file */
static int count_lines(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int count = 0;
    char buf[4096];
    while (fgets(buf, sizeof(buf), f)) count++;
    fclose(f);
    return count;
}

/* Helper: read last line of a file */
static char *read_last_line(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    char *last = NULL;
    char buf[4096];
    while (fgets(buf, sizeof(buf), f)) {
        free(last);
        last = strdup(buf);
    }
    fclose(f);
    return last;
}

int main(void) {
    printf("=== Logger Tests ===\n");

    const char *log_dir = "/tmp/hermes_log_test";
    char agent_path[1024], errors_path[1024];
    snprintf(agent_path, sizeof(agent_path), "%s/logs/agent.log", log_dir);
    snprintf(errors_path, sizeof(errors_path), "%s/logs/errors.log", log_dir);

    /* Clean up any leftover state */
    unlink(agent_path);
    unlink(errors_path);
    rmdir("/tmp/hermes_log_test/logs");
    rmdir("/tmp/hermes_log_test");

    /* Create the log directory — ensure_dir in logger.c only creates last component */
    mkdir("/tmp/hermes_log_test", 0755);

    /* 1. Init creates log directory and opens files */
    hermes_log_init();
    hermes_log(LOG_INFO, "test", "test message");
    hermes_log_shutdown();
    TEST("init+info creates agent.log", file_has_content(agent_path));
    TEST("init+info does NOT create errors.log (only INFO level)",
         file_empty_or_missing(errors_path));
    unlink(agent_path);

    /* 2. WARNING level writes to both files */
    hermes_log_init();
    hermes_log(LOG_WARNING, "test", "warning message");
    hermes_log_shutdown();
    TEST("warning creates agent.log", file_has_content(agent_path));
    TEST("warning creates errors.log", file_has_content(errors_path));
    unlink(agent_path);
    unlink(errors_path);

    /* 3. ERROR level writes to both files */
    hermes_log_init();
    hermes_log(LOG_ERROR, "test", "error message");
    hermes_log_shutdown();
    TEST("error creates agent.log", file_has_content(agent_path));
    TEST("error creates errors.log", file_has_content(errors_path));
    unlink(agent_path);
    unlink(errors_path);

    /* 4. CRITICAL level writes to both files */
    hermes_log_init();
    hermes_log(LOG_CRITICAL, "test", "critical message");
    hermes_log_shutdown();
    TEST("critical creates agent.log", file_has_content(agent_path));
    TEST("critical creates errors.log", file_has_content(errors_path));
    unlink(agent_path);
    unlink(errors_path);

    /* 5. DEBUG level does NOT write (below INFO threshold) */
    hermes_log_init();
    hermes_log(LOG_DEBUG, "test", "debug message");
    hermes_log_shutdown();
    TEST("debug does NOT create agent.log (below INFO)",
         file_empty_or_missing(agent_path));

    /* 6. Log format: timestamp + level + module + message */
    hermes_log_init();
    hermes_log(LOG_INFO, "mymod", "hello %s %d", "world", 42);
    hermes_log_shutdown();
    char *line = read_last_line(agent_path);
    TEST("log line has correct format",
         line && strstr(line, "INFO") &&
         strstr(line, "mymod") &&
         strstr(line, "hello world 42"));
    /* Check timestamp format YYYY-MM-DD HH:MM:SS,mmm */
    TEST("log line has timestamp format",
         line && strlen(line) > 24 && line[4] == '-' &&
         line[7] == '-' && line[13] == ':' && line[16] == ':');
    free(line);
    unlink(agent_path);

    /* 7. Level names */
    TEST("level name: DEBUG",
         strcmp(hermes_log_level_name(LOG_DEBUG), "DEBUG") == 0);
    TEST("level name: INFO",
         strcmp(hermes_log_level_name(LOG_INFO), "INFO") == 0);
    TEST("level name: WARNING",
         strcmp(hermes_log_level_name(LOG_WARNING), "WARNING") == 0);
    TEST("level name: ERROR",
         strcmp(hermes_log_level_name(LOG_ERROR), "ERROR") == 0);
    TEST("level name: CRITICAL",
         strcmp(hermes_log_level_name(LOG_CRITICAL), "CRITICAL") == 0);
    TEST("level name: unknown",
         strcmp(hermes_log_level_name(0), "LVL?") == 0);

    /* 8. Multiple log lines accumulate */
    hermes_log_init();
    hermes_log(LOG_INFO, "test", "line1");
    hermes_log(LOG_INFO, "test", "line2");
    hermes_log(LOG_INFO, "test", "line3");
    hermes_log_shutdown();
    TEST("multiple log lines accumulate",
         count_lines(agent_path) == 3);
    unlink(agent_path);

    /* 9. Double init is safe */
    hermes_log_init();
    hermes_log_init();  /* second call should be no-op */
    hermes_log(LOG_INFO, "test", "after double init");
    hermes_log_shutdown();
    TEST("double init produces single log entry",
         count_lines(agent_path) == 1);
    unlink(agent_path);

    /* 10. Log without explicit init (lazy init) */
    hermes_log(LOG_INFO, "test", "lazy init message");
    hermes_log_shutdown();
    TEST("lazy init works",
         file_has_content(agent_path));
    unlink(agent_path);
    unlink(errors_path);

    printf("\nResults: %d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
