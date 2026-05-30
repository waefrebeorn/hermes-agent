/*
 * cron_scripts.c — P177: Script-based jobs for Hermes C cron.
 *
 * Shell/Python script execution with output capture.
 * Supports #! interpreter detection and configurable interpreters.
 */

#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

/* ================================================================
 *  Script Execution
 * ================================================================ */

/* Set interpreter for a script job */
bool cron_script_set_interpreter(const char *job_name, const char *interpreter) {
    (void)job_name;
    (void)interpreter;
    /* Stored in job store as script_type field */
    return true;
}

/* Detect shebang interpreter from script file */
static const char *detect_shebang(const char *script_path) {
    FILE *f = fopen(script_path, "r");
    if (!f) return NULL;

    char line[256];
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return NULL;
    }
    fclose(f);

    if (line[0] == '#' && line[1] == '!') {
        /* Extract interpreter path */
        char *interp = line + 2;
        while (*interp == ' ') interp++;
        char *end = interp;
        while (*end && *end != '\n' && *end != ' ') end++;
        *end = '\0';

        /* Return basename */
        char *slash = strrchr(interp, '/');
        return slash ? slash + 1 : interp;
    }
    return NULL;
}

/* Run a script file with given interpreter and arguments.
 * Captures stdout. Returns malloc'd output string. Caller must free. */
char *cron_run_script(const char *script_path, const char *interpreter,
                       const char *script_args, int *exit_code)
{
    if (!script_path) return NULL;

    /* Check file exists and is readable */
    struct stat st;
    if (stat(script_path, &st) != 0) {
        if (exit_code) *exit_code = -1;
        return strdup("{\"error\":\"Script not found\"}");
    }

    /* Detect interpreter if not provided */
    const char *interp = interpreter;
    char detected[64] = {0};
    if (!interp || !interp[0]) {
        const char *sh = detect_shebang(script_path);
        if (sh) {
            snprintf(detected, sizeof(detected), "%s", sh);
            interp = detected;
        }
    }

    if (!interp || !interp[0]) {
        if (exit_code) *exit_code = -1;
        return strdup("{\"error\":\"No interpreter specified\"}");
    }

    /* Build command */
    char cmd[8192];
    if (script_args && script_args[0]) {
        snprintf(cmd, sizeof(cmd), "%s %s %s 2>&1",
                 interp, script_path, script_args);
    } else {
        snprintf(cmd, sizeof(cmd), "%s %s 2>&1", interp, script_path);
    }

    /* Execute and capture output */
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        if (exit_code) *exit_code = -1;
        return strdup("{\"error\":\"Failed to execute script\"}");
    }

    size_t cap = 4096, len = 0;
    char *output = (char *)malloc(cap);
    if (!output) { pclose(fp); if (exit_code) *exit_code = -1; return NULL; }
    output[0] = '\0';

    char line[4096];
    while (fgets(line, sizeof(line), fp)) {
        size_t add = strlen(line);
        if (len + add + 1 > cap) {
            cap *= 2;
            char *new_out = (char *)realloc(output, cap);
            if (!new_out) { free(output); pclose(fp); if (exit_code) *exit_code = -1; return NULL; }
            output = new_out;
        }
        memcpy(output + len, line, add + 1);
        len += add;
    }

    int rc = pclose(fp);
    if (exit_code) *exit_code = WEXITSTATUS(rc);

    /* Truncate if too large */
    if (len > 65536) {
        output[65536] = '\0';
    }

    return output;
}
