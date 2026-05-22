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
#include <strings.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
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
      "\"workdir\":{\"type\":\"string\",\"description\":\"Working directory for the command\"},"
      "\"backend\":{\"type\":\"string\",\"description\":\"Execution backend: local (default), docker\"},"
      "\"docker_image\":{\"type\":\"string\",\"description\":\"Docker image override for backend=docker\"}"
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

/* F11: Docker execution backend — run command in a Docker container */
static char *run_command_docker(const char *command, int timeout_sec,
                                 const char *image_override) {
    if (!command) return strdup("{\"error\": \"No command provided\"}");

    /* Resolve docker image: arg override → config → default */
    const char *image = image_override;
    if (!image || !image[0])
        image = tool_config_get("terminal", "docker_image");
    if (!image || !image[0])
        image = "ubuntu:22.04";

    /* Write command to temp script to avoid shell quoting issues */
    char tmpfile[64];
    snprintf(tmpfile, sizeof(tmpfile), "/tmp/hermes_docker_XXXXXX");
    int fd = mkstemp(tmpfile);
    if (fd < 0) {
        char buf[256];
        snprintf(buf, sizeof(buf), "{\"error\": \"mkstemp failed: %s\"}", strerror(errno));
        return strdup(buf);
    }
    /* Write command with shell header */
    dprintf(fd, "#!/bin/sh\n(%s) 2>&1\n", command ? command : "");
    close(fd);
    chmod(tmpfile, 0755);

    /* Resolve CWD for volume mount */
    char cwd[4096] = "";
    bool mount_cwd = tool_config_get_bool("terminal", "docker_mount_cwd", true);
    if (mount_cwd) {
        if (!getcwd(cwd, sizeof(cwd))) cwd[0] = '\0';
    }

    /* Build docker run command (max 32KB should be plenty) */
    char docker_cmd[32768];
    int n;
    size_t pos = 0;
    size_t remaining = sizeof(docker_cmd);

    n = snprintf(docker_cmd + pos, remaining,
                 "timeout %d docker run --rm -i",
                 timeout_sec > 0 ? timeout_sec : DEFAULT_TIMEOUT);
    if (n > 0 && (size_t)n < remaining) { pos += n; remaining -= n; }

    /* Mount CWD as /workspace */
    if (cwd[0]) {
        n = snprintf(docker_cmd + pos, remaining,
                     " -v \"%s:/workspace\" -w /workspace", cwd);
        if (n > 0 && (size_t)n < remaining) { pos += n; remaining -= n; }
    }

    /* Mount temp script as /hermes_cmd.sh */
    n = snprintf(docker_cmd + pos, remaining,
                 " -v \"%s:/hermes_cmd.sh:ro\"", tmpfile);
    if (n > 0 && (size_t)n < remaining) { pos += n; remaining -= n; }

    /* Forward specified env vars from host */
    const char *forward_env = tool_config_get("terminal", "docker_forward_env");
    if (forward_env && forward_env[0]) {
        char copy[1024];
        snprintf(copy, sizeof(copy), "%s", forward_env);
        char *token = strtok_r(copy, ",", &copy);
        while (token) {
            while (*token == ' ') token++;
            char *end = token + strlen(token) - 1;
            while (end > token && *end == ' ') *end-- = '\0';
            if (*token) {
                n = snprintf(docker_cmd + pos, remaining, " -e \"%s\"", token);
                if (n > 0 && (size_t)n < remaining) { pos += n; remaining -= n; }
            }
            token = strtok_r(NULL, ",", &copy);
        }
    }

    /* Set explicit env vars for the container (comma/space-sep KEY=VAL pairs) */
    const char *docker_env = tool_config_get("terminal", "docker_env");
    if (docker_env && docker_env[0]) {
        char copy[1024];
        snprintf(copy, sizeof(copy), "%s", docker_env);
        char *save = NULL;
        char *token = strtok_r(copy, " ,", &save);
        while (token) {
            n = snprintf(docker_cmd + pos, remaining, " -e \"%s\"", token);
            if (n > 0 && (size_t)n < remaining) { pos += n; remaining -= n; }
            token = strtok_r(NULL, " ,", &save);
        }
    }

    /* Mount additional volumes (comma-sep list of host:container specs) */
    const char *volumes = tool_config_get("terminal", "docker_volumes");
    if (volumes && volumes[0]) {
        char vcopy[1024];
        snprintf(vcopy, sizeof(vcopy), "%s", volumes);
        char *token = strtok_r(vcopy, ",", &vcopy);
        while (token) {
            while (*token == ' ') token++;
            char *end = token + strlen(token) - 1;
            while (end > token && *end == ' ') *end-- = '\0';
            if (*token) {
                n = snprintf(docker_cmd + pos, remaining, " -v \"%s\"", token);
                if (n > 0 && (size_t)n < remaining) { pos += n; remaining -= n; }
            }
            token = strtok_r(NULL, ",", &vcopy);
        }
    }

    /* Extra docker args (comma-sep list of raw args) */
    const char *extra_args = tool_config_get("terminal", "docker_extra_args");
    if (extra_args && extra_args[0]) {
        char acopy[1024];
        snprintf(acopy, sizeof(acopy), "%s", extra_args);
        char *token = strtok_r(acopy, ",", &acopy);
        while (token) {
            while (*token == ' ') token++;
            if (*token) {
                n = snprintf(docker_cmd + pos, remaining, " %s", token);
                if (n > 0 && (size_t)n < remaining) { pos += n; remaining -= n; }
            }
            token = strtok_r(NULL, ",", &acopy);
        }
    }

    /* Run as host user UID:GID */
    bool run_as_host = tool_config_get_bool("terminal", "docker_run_as_host_user", false);
    if (run_as_host) {
        n = snprintf(docker_cmd + pos, remaining,
                     " -u \"$(id -u):$(id -g)\"");
        if (n > 0 && (size_t)n < remaining) { pos += n; remaining -= n; }
    }

    /* Image + script */
    n = snprintf(docker_cmd + pos, remaining, " %s sh /hermes_cmd.sh", image);
    if (n > 0 && (size_t)n < remaining) { pos += n; remaining -= n; }

    /* Allow stderr debug in case docker itself fails */
    n = snprintf(docker_cmd + pos, remaining, " 2>&1 || true");
    if (n > 0 && (size_t)n < remaining) { pos += n; remaining -= n; }

    /* Execute via popen */
    FILE *fp = popen(docker_cmd, "r");
    if (!fp) {
        unlink(tmpfile);
        char buf[256];
        snprintf(buf, sizeof(buf), "{\"error\": \"docker popen failed: %s\"}", strerror(errno));
        return strdup(buf);
    }

    /* Read output */
    size_t cap = 4096, len = 0;
    char *output = (char *)malloc(cap);
    if (!output) { pclose(fp); unlink(tmpfile); return strdup("{\"error\": \"OOM\"}"); }
    output[0] = '\0';

    char line[4096];
    while (fgets(line, sizeof(line), fp)) {
        size_t line_len = strlen(line);
        if (len + line_len >= MAX_OUTPUT) {
            size_t remaining_output = MAX_OUTPUT - len - 10;
            if (remaining_output > 0) {
                memcpy(output + len, line, remaining_output);
                len += remaining_output;
            }
            memcpy(output + len, "\n...[truncated]", 15);
            len += 15;
            output[len] = '\0';
            break;
        }
        if (len + line_len + 1 > cap) {
            cap *= 2;
            char *new_out = (char *)realloc(output, cap);
            if (!new_out) { free(output); pclose(fp); unlink(tmpfile);
                return strdup("{\"error\": \"OOM\"}"); }
            output = new_out;
        }
        memcpy(output + len, line, line_len + 1);
        len += line_len;
    }

    int exit_code = pclose(fp);
    unlink(tmpfile);

    /* Build JSON result */
    json_node_t *result = json_new_object();
    json_object_set(result, "exit_code", json_new_number(exit_code));
    json_object_set(result, "output", json_new_string(output));
    json_object_set(result, "command", json_new_string(command));
    json_object_set(result, "backend", json_new_string("docker"));
    json_object_set(result, "docker_image", json_new_string(image));
    json_object_set(result, "truncated",
                    json_new_bool(len >= MAX_OUTPUT - 25));

    char *json_out = json_serialize(result);
    json_free(result);
    free(output);
    return json_out;
}

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

    /* F11: Docker backend — read backend arg, fallback to config */
    const char *backend_arg = json_object_get_string(args, "backend", NULL);
    const char *backend = backend_arg;
    if (!backend || !backend[0])
        backend = tool_config_get("terminal", "backend");
    
    /* F11: Docker image override */
    const char *docker_image = json_object_get_string(args, "docker_image", NULL);

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

    /* F11: Route to Docker execution backend if configured */
    if (backend && strcasecmp(backend, "docker") == 0) {
        return run_command_docker(command, timeout, docker_image);
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
