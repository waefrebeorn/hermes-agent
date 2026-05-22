/*
 * terminal.c — Terminal execution tool for Hermes C.
 * Executes shell commands and returns output.
 * Wraps popen() with timeout and size limits.
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_tool_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#ifdef __linux__
#include <pty.h>
#endif

#define MAX_OUTPUT 65536
#define DEFAULT_TIMEOUT 180

/* Schema for the terminal tool */
static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"command\":{\"type\":\"string\",\"description\":\"Shell command to execute\"},"
      "\"timeout\":{\"type\":\"number\",\"description\":\"Timeout in seconds\",\"default\":180},"
      "\"pty\":{\"type\":\"boolean\",\"description\":\"Use pseudo-terminal for interactive commands\",\"default\":false},"
      "\"env\":{\"type\":\"string\",\"description\":\"Environment variables in KEY=VALUE KEY2=VALUE2 format\"},"
      "\"workdir\":{\"type\":\"string\",\"description\":\"Working directory for the command\"}"
    "},"
    "\"required\":[\"command\"]"
"}";

/* ================================================================
 *  Execution
 * ================================================================ */

static char *run_command(const char *command, int timeout_sec) {
    if (!command) return strdup("{\"error\": \"No command provided\"}");

    /* Use popen with a simple exec */
    char full_cmd[16384];
    snprintf(full_cmd, sizeof(full_cmd),
             "timeout %d sh -c '(%s) 2>&1' 2>/dev/null || true",
             timeout_sec > 0 ? timeout_sec : DEFAULT_TIMEOUT, command);

    FILE *fp = popen(full_cmd, "r");
    if (!fp) {
        char buf[256];
        snprintf(buf, sizeof(buf), "{\"error\": \"popen failed: %s\"}", strerror(errno));
        return strdup(buf);
    }

    /* Read output */
    size_t cap = 4096, len = 0;
    char *output = (char *)malloc(cap);
    if (!output) { pclose(fp); return strdup("{\"error\": \"OOM\"}"); }
    output[0] = '\0';

    char line[4096];
    while (fgets(line, sizeof(line), fp)) {
        size_t line_len = strlen(line);
        if (len + line_len >= MAX_OUTPUT) {
            size_t remaining = MAX_OUTPUT - len - 10;
            if (remaining > 0) {
                memcpy(output + len, line, remaining);
                len += remaining;
            }
            memcpy(output + len, "\n...[truncated]", 15);
            len += 15;
            output[len] = '\0';
            break;
        }
        if (len + line_len + 1 > cap) {
            cap *= 2;
            char *new_out = (char *)realloc(output, cap);
            if (!new_out) { free(output); pclose(fp); return strdup("{\"error\": \"OOM\"}"); }
            output = new_out;
        }
        memcpy(output + len, line, line_len + 1);
        len += line_len;
    }

    int exit_code = pclose(fp);

    /* Build JSON result */
    json_node_t *result = json_new_object();
    json_object_set(result, "exit_code", json_new_number(exit_code));
    json_object_set(result, "output", json_new_string(output));
    json_object_set(result, "command", json_new_string(command));
    json_object_set(result, "truncated",
                    json_new_bool(len >= MAX_OUTPUT - 25));

    char *json_out = json_serialize(result);
    json_free(result);
    free(output);
    return json_out;
}

/* F09: PTY mode execution using forkpty */
#ifdef __linux__
static char *run_command_pty(const char *command, int timeout_sec) {
    if (!command) return strdup("{\"error\": \"No command provided\"}");

    int master_fd;
    pid_t pid = forkpty(&master_fd, NULL, NULL, NULL);
    if (pid == -1) {
        char buf[256];
        snprintf(buf, sizeof(buf), "{\"error\": \"forkpty failed: %s\"}", strerror(errno));
        return strdup(buf);
    }

    if (pid == 0) {
        /* Child: execute command */
        execl("/bin/sh", "sh", "-c", command, (char *)NULL);
        _exit(127);
    }

    /* Parent: read output from PTY with timeout */
    size_t cap = 4096, len = 0;
    char *output = (char *)malloc(cap);
    if (!output) { close(master_fd); return strdup("{\"error\": \"OOM\"}"); }
    output[0] = '\0';

    struct timeval tv;
    fd_set fds;
    int exit_code = -1;
    time_t start = time(NULL);
    int timeout = timeout_sec > 0 ? timeout_sec : DEFAULT_TIMEOUT;

    while (1) {
        /* Check timeout */
        if (timeout > 0 && (time(NULL) - start) >= timeout) {
            kill(pid, SIGTERM);
            usleep(100000);
            kill(pid, SIGKILL);
            break;
        }

        FD_ZERO(&fds);
        FD_SET(master_fd, &fds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int ret = select(master_fd + 1, &fds, NULL, NULL, &tv);
        if (ret > 0 && FD_ISSET(master_fd, &fds)) {
            char buf[4096];
            ssize_t n = read(master_fd, buf, sizeof(buf) - 1);
            if (n > 0) {
                buf[n] = '\0';
                if (len + n >= MAX_OUTPUT) {
                    size_t remaining = MAX_OUTPUT - len - 10;
                    if (remaining > 0) {
                        memcpy(output + len, buf, remaining);
                        len += remaining;
                    }
                    memcpy(output + len, "\n...[truncated]", 15);
                    len += 15;
                    output[len] = '\0';
                    kill(pid, SIGKILL);
                    break;
                }
                if (len + n + 1 > cap) {
                    cap = cap * 2 > len + n + 1 ? cap * 2 : len + n + 1;
                    char *new_out = (char *)realloc(output, cap);
                    if (!new_out) { free(output); close(master_fd); return strdup("{\"error\": \"OOM\"}"); }
                    output = new_out;
                }
                memcpy(output + len, buf, n);
                len += n;
                output[len] = '\0';
            } else if (n == 0) {
                /* EOF — process exited */
                break;
            }
        } else if (ret == 0) {
            /* Timeout on select — process still running */
            int status;
            pid_t result = waitpid(pid, &status, WNOHANG);
            if (result == pid) {
                if (WIFEXITED(status)) exit_code = WEXITSTATUS(status);
                break;
            }
        } else {
            /* select error */
            break;
        }
    }

    /* Final wait for process */
    if (exit_code < 0) {
        int status;
        if (waitpid(pid, &status, WNOHANG) == pid) {
            if (WIFEXITED(status)) exit_code = WEXITSTATUS(status);
        }
    }
    close(master_fd);

    json_node_t *result = json_new_object();
    json_object_set(result, "exit_code", json_new_number(exit_code));
    json_object_set(result, "output", json_new_string(output));
    json_object_set(result, "command", json_new_string(command));
    json_object_set(result, "pty", json_new_bool(true));
    json_object_set(result, "truncated",
                    json_new_bool(len >= MAX_OUTPUT - 25));

    char *json_out = json_serialize(result);
    json_free(result);
    free(output);
    return json_out;
}
#endif

/* ================================================================
 *  Handler
 * ================================================================ */

char *terminal_handler(const char *args_json, const char *task_id) {
    (void)task_id;

    if (!args_json)
        return strdup("{\"error\": \"No arguments provided\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) {
        char buf[512];
        snprintf(buf, sizeof(buf), "{\"error\": \"JSON parse error: %s\"}", err ? err : "unknown");
        free(err);
        return strdup(buf);
    }

    const char *command = json_object_get_string(args, "command", NULL);

    /* F12: Timeout propagation — read from args, fallback to tool config, then default */
    int timeout = (int)json_object_get_number(args, "timeout", 0);
    if (timeout <= 0) {
        timeout = tool_config_get_int("terminal", "timeout", DEFAULT_TIMEOUT);
    }

    /* F09: PTY mode */
    bool use_pty = json_object_get_bool(args, "pty", false);

    /* F10: Environment isolation — read optional env string (KEY=VALUE KEY2=VALUE2) */
    const char *env_str = json_object_get_string(args, "env", NULL);

    /* Workdir */
    const char *workdir = json_object_get_string(args, "workdir", NULL);

    json_free(args);

    if (!command)
        return strdup("{\"error\": \"Missing required 'command' argument\"}");

    /* Build workdir prefix */
    char workdir_prefix[4096] = "";
    if (workdir && workdir[0]) {
        snprintf(workdir_prefix, sizeof(workdir_prefix), "cd %s && ", workdir);
    }

    /* Wrap command with env and workdir */
    char full_command[16384];
    if (env_str && env_str[0]) {
        snprintf(full_command, sizeof(full_command), "%s %s%s%s",
                env_str, workdir_prefix, use_pty ? "" : "", command);
    } else {
        snprintf(full_command, sizeof(full_command), "%s%s",
                workdir_prefix, command);
    }

    if (use_pty) {
#ifdef __linux__
        return run_command_pty(full_command, timeout);
#else
        return run_command(full_command, timeout);
#endif
    }
    return run_command(full_command, timeout);
}

/* Auto-register */
void registry_init_terminal(void) {
    registry_register("terminal",
        "Execute a shell command. Returns stdout, stderr, and exit code. "
        "Use for running code, compiling, testing, and system operations.",
        SCHEMA, terminal_handler);
}
