/*
 * terminal.c — Terminal execution tool for Hermes C.
 * Executes shell commands and returns output.
 * Wraps popen() with timeout and size limits.
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_tool_config.h"
#include "hermes_sandbox.h"
#include "tool_output.h"
#include "env_passthrough.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <ctype.h>
#include <termios.h>
#include <poll.h>
#include <fcntl.h>
#ifdef __linux__
#include <sys/vfs.h>
#endif
#ifdef __linux__
#include <pty.h>
#endif

#define DEFAULT_TIMEOUT 180

/* Schema for the terminal tool */
static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"command\":{\"type\":\"string\",\"description\":\"Shell command to execute\"},"
      "\"timeout\":{\"type\":\"number\",\"description\":\"Timeout in seconds\",\"default\":180},"
      "\"pty\":{\"type\":\"boolean\",\"description\":\"Use pseudo-terminal for interactive commands\",\"default\":false},"
      "\"force\":{\"type\":\"boolean\",\"description\":\"Skip dangerous command check (use after user confirms)\",\"default\":false},"
      "\"env\":{\"type\":\"string\",\"description\":\"Environment variables in KEY=VALUE KEY2=VALUE2 format\"},"
      "\"workdir\":{\"type\":\"string\",\"description\":\"Working directory for the command\"},"
      "\"backend\":{\"type\":\"string\",\"description\":\"Execution backend: local (default), docker, docker-compose, ssh, modal\"},"
      "\"docker_image\":{\"type\":\"string\",\"description\":\"Docker image override for backend=docker\"}"
    "},"
    "\"required\":[\"command\"]"
"}";

/* ================================================================
 *  Safe command preview
 * ================================================================ */

/* Return a log-safe preview of a command, truncated at limit bytes.
 * Returns pointer to a static buffer (NOT thread-safe — terminal.c
 * is single-threaded). Mirrors Python _safe_command_preview(). */
static const char *_safe_command_preview(const char *command, int limit) {
    static char buf[512];
    if (!command) return "<None>";
    if (limit <= 0) limit = 200;
    if (limit > (int)sizeof(buf) - 1) limit = (int)sizeof(buf) - 1;
    size_t len = strlen(command);
    if ((int)len <= limit) return command;
    memcpy(buf, command, (size_t)limit);
    buf[limit] = '\0';
    return buf;
}

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
        snprintf(buf, sizeof(buf), "{\"error\": \"popen failed: %s\", \"command_preview\": \"%s\"}",
                 strerror(errno), _safe_command_preview(command, 100));
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
        if (len + line_len >= (size_t)tool_output_get_max_bytes()) {
            size_t remaining = (size_t)tool_output_get_max_bytes() - len - 10;
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
    json_object_set(result, "status", json_new_string(exit_code == 0 ? "success" : "error"));
    json_object_set(result, "truncated",
                    json_new_bool(len >= (size_t)tool_output_get_max_bytes() - 25));

    char *json_out = json_serialize(result);
    json_free(result);
    free(output);
    return json_out;
}

/* F09: PTY mode execution using forkpty */
#ifdef __linux__
static char *run_command_pty(const char *command, int timeout_sec, const char *sudo_password) {
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

    /* Parent: if sudo_password provided, write it to PTY master for sudo -S
     * sudo reads exactly one line (the password) and passes rest of stdin
     * to the child command. This must happen before the read loop starts. */
    if (sudo_password && sudo_password[0]) {
        size_t pwlen = strlen(sudo_password);
        if (write(master_fd, sudo_password, pwlen) < 0) {
            /* PTY write failed — non-fatal, sudo will fail with password required */
            (void)0;
        }
        /* Send trailing newline for sudo -S to recognize end of password line */
        if (write(master_fd, "\n", 1) < 0)
            (void)0;
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
                if (len + n >= (size_t)tool_output_get_max_bytes()) {
                    size_t remaining = (size_t)tool_output_get_max_bytes() - len - 10;
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
    json_object_set(result, "status", json_new_string(exit_code == 0 ? "success" : "error"));
    json_object_set(result, "truncated",
                    json_new_bool(len >= (size_t)tool_output_get_max_bytes() - 25));

    char *json_out = json_serialize(result);
    json_free(result);
    free(output);
    return json_out;
}
#endif


/* D84: SSH execution backend — run command on a remote host */
static char *run_command_ssh(const char *command, int timeout_sec) {
    if (!command) return strdup("{\"error\": \"No command provided\"}");

    /* Read SSH config: tool_config → defaults */
    const char *ssh_host = tool_config_get("terminal", "ssh_host");
    const char *ssh_user = tool_config_get("terminal", "ssh_user");
    int ssh_port = tool_config_get_int("terminal", "ssh_port", 22);
    const char *ssh_key = tool_config_get("terminal", "ssh_key_path");

    if (!ssh_host || !ssh_host[0])
        return strdup("{\"error\": \"SSH host not configured\"}");
    if (!ssh_user || !ssh_user[0])
        ssh_user = getenv("USER");

    char ssh_cmd[32768];
    int n = 0;
    size_t pos = 0;
    size_t rem = sizeof(ssh_cmd);

    n = snprintf(ssh_cmd + pos, rem,
        "ssh -o ControlMaster=auto -o ControlPersist=300"
        " -o StrictHostKeyChecking=accept-new -o ConnectTimeout=10"
        " -o BatchMode=yes");
    if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }

    if (ssh_port != 22) {
        n = snprintf(ssh_cmd + pos, rem, " -p %d", ssh_port);
        if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }
    }

    if (ssh_key && ssh_key[0]) {
        n = snprintf(ssh_cmd + pos, rem, " -i \"%s\"", ssh_key);
        if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }
    }

    n = snprintf(ssh_cmd + pos, rem, " %s@%s", ssh_user ? ssh_user : "", ssh_host);
    if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }

    /* Append command, shell-quoted via bash -c */
    n = snprintf(ssh_cmd + pos, rem,
        " \"bash -c '%s'\" 2>&1", command);
    if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }

    return run_command(ssh_cmd, timeout_sec);
}

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
        char *save1 = NULL;
        char *token = strtok_r(copy, ",", &save1);
        while (token) {
            while (*token == ' ') token++;
            char *end = token + strlen(token) - 1;
            while (end > token && *end == ' ') *end-- = '\0';
            if (*token) {
                n = snprintf(docker_cmd + pos, remaining, " -e \"%s\"", token);
                if (n > 0 && (size_t)n < remaining) { pos += n; remaining -= n; }
            }
            token = strtok_r(NULL, ",", &save1);
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
        char *save2 = NULL;
        char *token = strtok_r(vcopy, ",", &save2);
        while (token) {
            while (*token == ' ') token++;
            char *end = token + strlen(token) - 1;
            while (end > token && *end == ' ') *end-- = '\0';
            if (*token) {
                n = snprintf(docker_cmd + pos, remaining, " -v \"%s\"", token);
                if (n > 0 && (size_t)n < remaining) { pos += n; remaining -= n; }
            }
            token = strtok_r(NULL, ",", &save2);
        }
    }

    /* Extra docker args (comma-sep list of raw args) */
    const char *extra_args = tool_config_get("terminal", "docker_extra_args");
    if (extra_args && extra_args[0]) {
        char acopy[1024];
        snprintf(acopy, sizeof(acopy), "%s", extra_args);
        char *save3 = NULL;
        char *token = strtok_r(acopy, ",", &save3);
        while (token) {
            while (*token == ' ') token++;
            if (*token) {
                n = snprintf(docker_cmd + pos, remaining, " %s", token);
                if (n > 0 && (size_t)n < remaining) { pos += n; remaining -= n; }
            }
            token = strtok_r(NULL, ",", &save3);
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
        if (len + line_len >= (size_t)tool_output_get_max_bytes()) {
            size_t remaining_output = (size_t)tool_output_get_max_bytes() - len - 10;
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
                    json_new_bool(len >= (size_t)tool_output_get_max_bytes() - 25));

    char *json_out = json_serialize(result);
    json_free(result);
    free(output);
    return json_out;
}

/* M35: Modal execution backend — run command via Modal CLI */
static char *run_command_modal(const char *command, int timeout_sec) {
    if (!command) return strdup("{\"error\": \"No command provided\"}");
    char tmpfile[] = "/tmp/hermes_modal_XXXXXX";
    int fd = mkstemp(tmpfile);
    if (fd < 0) return strdup("{\"error\": \"mkstemp failed\"}");

    /* Write Modal Python wrapper */
    dprintf(fd,
        "import subprocess, sys, json\n"
        "import modal\n\n"
        "app = modal.App(\"hermes-cmd\")\n\n"
        "@app.function()\n"
        "def run_cmd():\n"
        "    try:\n"
        "        r = subprocess.run(%s, shell=True, capture_output=True,\n"
        "                          text=True, timeout=%d)\n"
        "        print(json.dumps({\n"
        "            'stdout': r.stdout,\n"
        "            'stderr': r.stderr[:4000],\n"
        "            'exit_code': r.returncode\n"
        "        }))\n"
        "    except subprocess.TimeoutExpired:\n"
        "        print(json.dumps({'stdout':'','stderr':'timeout','exit_code':124}))\n\n"
        "if __name__ == '__main__':\n"
        "    modal.run(app, run_cmd)\n",
        command, timeout_sec > 0 ? timeout_sec : 30);
    close(fd);

    char cmd[66560];
    snprintf(cmd, sizeof(cmd), "modal run %s 2>/dev/null", tmpfile);
    char *result = run_command(cmd, timeout_sec + 10);
    unlink(tmpfile);
    return result;
}

/* D04: Docker Compose execution backend — run command in docker compose service */
static char *run_command_docker_compose(const char *command, int timeout_sec) {
    if (!command) return strdup("{\"error\": \"No command provided\"}");
    const char *service = tool_config_get("terminal", "compose_service");
    if (!service) service = "default";

    /* Use docker compose exec to run in an existing service container */
    char compose_cmd[32768];
    int n = snprintf(compose_cmd, sizeof(compose_cmd),
                     "timeout %d docker compose exec -T %s sh -c '%s' 2>&1 || true",
                     timeout_sec > 0 ? timeout_sec : DEFAULT_TIMEOUT,
                     service, command);
    if (n < 0 || (size_t)n >= sizeof(compose_cmd))
        return strdup("{\"error\": \"Command too long\"}");

    /* Execute via popen */
    FILE *fp = popen(compose_cmd, "r");
    if (!fp) {
        char buf[256];
        snprintf(buf, sizeof(buf), "{\"error\": \"docker compose popen failed: %s\"}", strerror(errno));
        return strdup(buf);
    }

    size_t cap = 4096, len = 0;
    char *output = (char *)malloc(cap);
    if (!output) { pclose(fp); return strdup("{\"error\": \"OOM\"}"); }
    output[0] = '\0';

    char line[4096];
    while (fgets(line, sizeof(line), fp)) {
        size_t line_len = strlen(line);
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

    json_node_t *result = json_new_object();
    json_object_set(result, "exit_code", json_new_number(exit_code));
    json_object_set(result, "output", json_new_string(output));
    json_object_set(result, "backend", json_new_string("docker-compose"));
    json_object_set(result, "compose_service", json_new_string(service));
    char *json_out = json_serialize(result);
    json_free(result);
    free(output);
    return json_out;
}

/* Build export prefix from env_passthrough registered vars.
 * Returns malloc'd string like "export KEY1='val1' KEY2='val2' && "
 * or NULL if no vars registered. Caller free()s. */
static char *build_env_passthrough_export(void) {
    char **vars = NULL;
    int count = 0;
    env_passthrough_get_all(&vars, &count);
    if (count == 0 || !vars) return NULL;

    size_t cap = 256, len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) { env_passthrough_free_list(vars, count); return NULL; }
    buf[0] = '\0';

    for (int i = 0; i < count; i++) {
        const char *val = getenv(vars[i]);
        if (!val) continue;  /* Skip unset vars */

        size_t needed = strlen("export ") + strlen(vars[i])
                      + strlen("='' ") + strlen(val) + 4;
        if (len + needed > cap) {
            cap = cap * 2 + needed;
            char *new_buf = (char *)realloc(buf, cap);
            if (!new_buf) { free(buf); env_passthrough_free_list(vars, count); return NULL; }
            buf = new_buf;
        }
        len += snprintf(buf + len, cap - len, "export %s='%s' ", vars[i], val);
    }

    env_passthrough_free_list(vars, count);

    if (len == 0) { free(buf); return NULL; }

    /* Append "&& " for safe chaining */
    size_t n = snprintf(buf + len, cap - len, "&& ");
    if (len + n + 1 > cap) {
        char *new_buf = (char *)realloc(buf, len + n + 2);
        if (!new_buf) { free(buf); return NULL; }
        buf = new_buf;
        snprintf(buf + len, cap - len, "&& ");
    }
    return buf;
}

/* ================================================================
 *  Safety checks (workdir validation, disk usage warning)
 * ================================================================ */

/* Validate workdir uses only safe filesystem characters.
 * Mirrors Python terminal_tool._validate_workdir().
 * Uses allowlist: alphanumeric plus / \ _ : - . ~ space + @ = ,
 * Returns error string if dangerous, NULL if safe. */
static const char *_validate_workdir(const char *workdir) {
    if (!workdir || !workdir[0]) return NULL;
    static const char safe[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                               "abcdefghijklmnopqrstuvwxyz"
                               "0123456789/\\_:\\-.~ +@=,";
    for (const char *p = workdir; *p; p++) {
        if (!strchr(safe, *p)) {
            static char buf[256];
            snprintf(buf, sizeof(buf),
                     "Blocked: workdir contains disallowed character '%c'. "
                     "Use a simple filesystem path without shell metacharacters.",
                     *p);
            return buf;
        }
    }
    return NULL;
}

/* Check workdir filesystem path (exists, is directory). Returns warning or NULL.
 * Mirrors Python terminal_tool._validate_workdir() for existence check. */
static const char *_check_workdir(const char *workdir) {
    if (!workdir || !workdir[0]) return NULL;
    struct stat st;
    if (stat(workdir, &st) != 0)
        return "WARNING: workdir does not exist (will be created by shell)";
    if (!S_ISDIR(st.st_mode))
        return "WARNING: workdir exists but is not a directory";
    return NULL;
}

/* Check free disk space on workdir's filesystem. Returns NULL if OK, warning if < 100MB free. */
static const char *_check_disk_usage(const char *workdir) {
#ifdef __linux__
    const char *path = workdir && workdir[0] ? workdir : "/tmp";
    struct statfs sfs;
    if (statfs(path, &sfs) != 0) return NULL;
    unsigned long long free_bytes = (unsigned long long)sfs.f_bfree * (unsigned long long)sfs.f_bsize;
    unsigned long long free_mb = free_bytes / (1024 * 1024);
    if (free_mb < 100) {
        static char buf[128];
        snprintf(buf, sizeof(buf), "WARNING: Low disk space — ~%llu MB free on %s", free_mb, path);
        return buf;
    }
#else
    (void)workdir;
#endif
    return NULL;
}

/* Inject safety warnings into result JSON. Returns malloc'd string. */
static char *_inject_warnings(const char *result_json,
                               const char *wd_warn, const char *disk_warn,
                               const char *guidance) {
    json_t *rj = json_parse(result_json, NULL);
    if (!rj) return strdup(result_json);
    if (wd_warn) json_set(rj, "workdir_warning", json_string(wd_warn));
    if (disk_warn) json_set(rj, "disk_warning", json_string(disk_warn));
    if (guidance) json_set(rj, "guidance", json_string(guidance));
    char *out = json_serialize(rj);
    json_free(rj);
    return out;
}

/* Interpret exit code into human-readable message per command semantics.
 * Mirrors Python terminal_tool._interpret_exit_code(). */
static const char *exit_code_interpret(const char *command, int exit_code) {
    if (!command || exit_code == 0) return NULL;

    /* Extract last command segment (after &&, ||, |, ;) */
    const char *last = command;
    const char *p = command;
    while (*p) {
        if ((p[0] == '&' && p[1] == '&') || (p[0] == '|' && p[1] == '|') ||
             p[0] == '|' || p[0] == ';') {
            p++;
            while (*p == ' ') p++;
            if (*p) last = p;
        } else {
            p++;
        }
    }

    /* Extract base command name (skip env VAR=val prefix) */
    const char *cmd_start = last;
    while (*cmd_start == ' ') cmd_start++;
    const char *base_cmd = cmd_start;
    while (*base_cmd && *base_cmd != ' ') base_cmd++;
    size_t cmd_len = (size_t)(base_cmd - cmd_start);

    /* Skip env assignments: VAR=val ... */
    const char *tok = cmd_start;
    while (tok < base_cmd) {
        const char *eq = memchr(tok, '=', (size_t)(base_cmd - tok));
        if (!eq || eq > base_cmd) break;
        /* Check if this looks like VAR=val — no slash, non-empty name */
        const char *name_end = eq;
        if (name_end > tok && *(name_end - 1) != ' ') {
            tok = base_cmd; /* All consumed — no real command found */
        } else {
            break;
        }
    }
    if (tok < base_cmd) cmd_start = tok;

    /* Skip path prefix: /usr/bin/grep -> grep */
    const char *slash = (const char *)memchr(cmd_start, '/', cmd_len);
    if (slash) {
        cmd_start = slash + 1;
        cmd_len -= (size_t)((slash + 1) - cmd_start);
    }

    /* Command-specific exit code semantics */
    struct { const char *name; size_t len; int exit_code; const char *msg; } sem[] = {
        { "grep",       4, 1, "No matches found (not an error)" },
        { "egrep",      5, 1, "No matches found (not an error)" },
        { "fgrep",      5, 1, "No matches found (not an error)" },
        { "rg",         2, 1, "No matches found (not an error)" },
        { "diff",       4, 1, "Files differ (expected, not an error)" },
        { "colordiff", 10, 1, "Files differ (expected, not an error)" },
        { "find",       4, 1, "Some directories were inaccessible (partial results valid)" },
        { "test",       4, 1, "Condition evaluated to false (expected, not an error)" },
        { "curl",       4, 6, "Could not resolve host" },
        { "curl",       4, 7, "Failed to connect to host" },
        { "curl",       4, 22, "HTTP response code indicated error (e.g. 404, 500)" },
        { "curl",       4, 28, "Operation timed out" },
        { "git",        3, 1, "Non-zero exit (often normal — e.g. 'git diff' returns 1 when files differ)" },
        { NULL,         0, 0, NULL }
    };

    for (int i = 0; sem[i].name; i++) {
        if (cmd_len == sem[i].len && memcmp(cmd_start, sem[i].name, cmd_len) == 0
            && exit_code == sem[i].exit_code) {
            return sem[i].msg;
        }
    }

    return NULL;
}

/* Check command output for sudo failure patterns and add a helpful tip.
 * Mirrors Python terminal_tool._handle_sudo_failure(). */
static void _inject_sudo_failure(json_t *rj, const char *command) {
    (void)command;
    if (!rj) return;
    const char *output = json_get_str(rj, "output", NULL);
    if (!output) return;

    static const char *sudo_failures[] = {
        "sudo: a password is required",
        "sudo: no tty present",
        "sudo: a terminal is required",
        NULL
    };
    for (int i = 0; sudo_failures[i]; i++) {
        if (strstr(output, sudo_failures[i])) {
            json_set(rj, "sudo_tip",
                json_string("Tip: To enable sudo over messaging, add SUDO_PASSWORD to your .env file"));
            return;
        }
    }
}

/* Check if sudo -n works without a password prompt.
 * Port of Python terminal_tool._sudo_nopasswd_works().
 * Returns true when local sudo works without prompting.
 * Only checks the local backend; Docker/SSH/etc return false.
 * No process-level cache — re-probes every call. */
bool terminal_sudo_nopasswd_works(void) {
    /* Only check for local backend */
    const char *env = getenv("TERMINAL_ENV");
    if (env && env[0] && strcasecmp(env, "local") != 0)
        return false;

    /* Run sudo -n true with 3s timeout via popen */
    FILE *fp = popen("sudo -n true 2>/dev/null", "r");
    if (!fp) return false;
    int status = pclose(fp);
    return status == 0;
}

/* Interactive sudo password prompt.
 * Port of Python terminal_tool._prompt_for_sudo_password().
 * Opens /dev/tty, disables echo, reads password with timeout via poll().
 * Returns malloc'd password string (caller must free) or NULL on timeout/skip/error.
 * Only works in interactive mode (HERMES_INTERACTIVE=1). */
char *terminal_prompt_for_sudo_password(int timeout_seconds) {
    const char *interactive = getenv("HERMES_INTERACTIVE");
    if (!interactive || strcmp(interactive, "1") != 0)
        return NULL;

    int tty_fd = open("/dev/tty", O_RDWR);
    if (tty_fd < 0) return NULL;

    struct termios old_attrs, new_attrs;
    if (tcgetattr(tty_fd, &old_attrs) < 0) { close(tty_fd); return NULL; }

    /* Disable echo + canonical mode for read-char-at-a-time */
    new_attrs = old_attrs;
    new_attrs.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL | ICANON);
    new_attrs.c_cc[VMIN] = 1;
    new_attrs.c_cc[VTIME] = 0;
    if (tcsetattr(tty_fd, TCSAFLUSH, &new_attrs) < 0) { close(tty_fd); return NULL; }

    /* Print prompt UI */
    dprintf(tty_fd,
        "\n"
        "\xe2\x94\x8c\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
        " SUDO PASSWORD REQUIRED "
        "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
        "\xe2\x94\x90\n"
        "\xe2\x94\x82  Enter password below (input is hidden), or:            \xe2\x94\x82\n"
        "\xe2\x94\x82    \xe2\x80\xa2 Press Enter to skip (command fails gracefully)     \xe2\x94\x82\n"
        "\xe2\x94\x82    \xe2\x80\xa2 Wait %ds to auto-skip                           \xe2\x94\x82\n"
        "\xe2\x94\x94\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
        "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
        "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
        "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
        "\xe2\x94\x98\n\n",
        timeout_seconds);

    dprintf(tty_fd, "  Password (hidden): ");

    /* Read password with timeout */
    size_t cap = 128, len = 0;
    char *password = (char *)malloc(cap);
    if (!password) { tcsetattr(tty_fd, TCSAFLUSH, &old_attrs); close(tty_fd); return NULL; }
    password[0] = '\0';

    struct pollfd pfd;
    pfd.fd = tty_fd;
    pfd.events = POLLIN;
    int remaining_ms = (timeout_seconds > 0 ? timeout_seconds : 45) * 1000;

    while (remaining_ms > 0) {
        int ret = poll(&pfd, 1, remaining_ms);
        if (ret <= 0) break; /* timeout or error */

        unsigned char ch;
        ssize_t n = read(tty_fd, &ch, 1);
        if (n != 1) break;

        if (ch == '\n' || ch == '\r') break;
        if (ch == 0x03) { /* Ctrl-C */
            free(password);
            password = NULL;
            goto restore;
        }
        if (ch == 0x7f || ch == 0x08) { /* Backspace */
            if (len > 0) { len--; password[len] = '\0'; }
            continue;
        }

        if (len + 2 > cap) {
            cap *= 2;
            char *tmp = (char *)realloc(password, cap);
            if (!tmp) { free(password); password = NULL; goto restore; }
            password = tmp;
        }
        password[len++] = (char)ch;
        password[len] = '\0';
    }

    /* Newline after hidden input — password[0] is the password */
    dprintf(tty_fd, "\n");
    if (password && password[0]) {
        dprintf(tty_fd, "  \xe2\x9c\x93 Password received\n\n");
    } else {
        dprintf(tty_fd, "  \xe2\x8f\xad Skipped - continuing without sudo\n\n");
        /* Return empty string so caller can distinguish skip from error */
        if (password) password[0] = '\0';
    }

restore:
    tcsetattr(tty_fd, TCSAFLUSH, &old_attrs);
    close(tty_fd);
    return password; /* malloc'd, caller frees, or NULL on cancel */
}

/* Check if token looks like a shell env assignment (KEY=VALUE).
 * Port of Python terminal_tool._looks_like_env_assignment(). */
static bool looks_like_env_assignment(const char *token) {
    if (!token || !token[0]) return false;
    const char *eq = strchr(token, '=');
    if (!eq || eq == token) return false;
    /* Check that chars before '=' are valid env var name */
    for (const char *p = token; p < eq; p++) {
        if (p == token) {
            if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || *p == '_'))
                return false;
        } else {
            if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
                  (*p >= '0' && *p <= '9') || *p == '_'))
                return false;
        }
    }
    return true;
}

/* Forward declaration */
char *terminal_read_shell_token(const char *command, int start, int *end);

/* B07: Rewrite bare 'sudo' command words to 'sudo -S -p ""' for piped password.
 * Port of Python terminal_tool._rewrite_real_sudo_invocations().
 * Returns malloc'd transformed string. Sets *found to true if any sudo was rewritten.
 * Caller must free() the returned string. */
char *terminal_rewrite_sudo(const char *command, bool *found) {
    if (!command || !found) return NULL;
    *found = false;
    int n = (int)strlen(command);
    if (n == 0) return strdup("");

    /* Allocate output buffer: worst case is same length + " -S -p ''" for each sudo.
     * A generous overestimate: 2x input. */
    size_t out_cap = (size_t)n * 2 + 64;
    char *out = (char *)malloc(out_cap);
    if (!out) return NULL;
    int out_pos = 0;

    int i = 0;
    bool command_start = true;
    const char *sudo_replacement = "sudo -S -p ''";

    while (i < n) {
        char ch = command[i];

        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
            if ((size_t)out_pos + 1 >= out_cap) break;
            out[out_pos++] = ch;
            if (ch == '\n') command_start = true;
            i++;
            continue;
        }

        /* Comment at statement start: copy to end of line */
        if (ch == '#' && command_start) {
            while (i < n && command[i] != '\n') {
                if ((size_t)out_pos + 1 >= out_cap) break;
                out[out_pos++] = command[i++];
            }
            continue;
        }

        /* Compound operators: &&, ||, ;; */
        if (i + 1 < n) {
            if ((command[i] == '&' && command[i+1] == '&') ||
                (command[i] == '|' && command[i+1] == '|') ||
                (command[i] == ';' && command[i+1] == ';')) {
                if ((size_t)out_pos + 3 >= out_cap) break;
                out[out_pos++] = command[i++];
                out[out_pos++] = command[i++];
                command_start = true;
                continue;
            }
        }

        /* Single char operators: ; | & ( */
        if (ch == ';' || ch == '|' || ch == '&' || ch == '(') {
            if ((size_t)out_pos + 1 >= out_cap) break;
            out[out_pos++] = ch;
            i++;
            command_start = true;
            continue;
        }

        /* Closing paren */
        if (ch == ')') {
            if ((size_t)out_pos + 1 >= out_cap) break;
            out[out_pos++] = ch;
            i++;
            command_start = false;
            continue;
        }

        /* Read next shell token */
        int end = 0;
        char *token = terminal_read_shell_token(command, i, &end);
        if (!token) {
            /* Copy remaining as-is */
            while (i < n) {
                if ((size_t)out_pos + 1 >= out_cap) break;
                out[out_pos++] = command[i++];
            }
            break;
        }

        if (command_start && strcmp(token, "sudo") == 0) {
            /* Replace bare 'sudo' with 'sudo -S -p ""' */
            size_t replen = strlen(sudo_replacement);
            if ((size_t)out_pos + replen + 1 >= out_cap) {
                free(token);
                break;
            }
            memcpy(out + out_pos, sudo_replacement, replen);
            out_pos += (int)replen;
            *found = true;
        } else {
            /* Copy token verbatim */
            size_t tlen = strlen(token);
            if ((size_t)out_pos + tlen + 1 >= out_cap) {
                free(token);
                break;
            }
            memcpy(out + out_pos, token, tlen);
            out_pos += (int)tlen;
        }

        /* Determine command_start for next token */
        if (command_start && looks_like_env_assignment(token)) {
            command_start = true; /* env assignment keeps command_start */
        } else {
            command_start = false;
        }

        free(token);
        i = end;
    }

    out[out_pos] = '\0';
    return out;
}

/* B07: Transform sudo commands to use -S flag for piped password.
 * Port of Python terminal_tool._transform_sudo_command().
 * Returns malloc'd string: rewritten command if sudo found and password available,
 * or original command if no sudo or passwordless sudo works.
 * If out_password is non-NULL and a password was obtained, sets *out_password
 * to a malloc'd password string (caller must free).
 * Caller must free() the returned command string. */
static char *_transform_sudo(const char *command, char **out_password) {
    if (!command || !command[0]) {
        if (out_password) *out_password = NULL;
        return NULL;
    }
    if (out_password) *out_password = NULL;

    bool found = false;
    char *rewritten = terminal_rewrite_sudo(command, &found);
    if (!rewritten) return NULL;
    if (!found) {
        /* No sudo in command — return original as-is */
        return rewritten; /* already strdup'd by terminal_rewrite_sudo */
    }

    /* Passwordless sudo check: skip rewrite if sudo -n works.
     * Mirrors Python: if _sudo_nopasswd_works(): return command, None */
    if (terminal_sudo_nopasswd_works()) {
        /* Sudo works without password — return original command unchanged */
        free(rewritten);
        return strdup(command);
    }

    /* Check SUDO_PASSWORD env var first */
    const char *sudo_password = getenv("SUDO_PASSWORD");
    if (sudo_password && sudo_password[0]) {
        /* Rewritten command is correct for stdin pipe
         * Note: current popen() backends can't pipe stdin.
         * Use pty=true for forkpty-based execution which supports stdin writes. */
        if (out_password) *out_password = strdup(sudo_password);
        return rewritten;
    }

    /* Interactive mode: prompt for password */
    {
        char *prompt = terminal_prompt_for_sudo_password(45);
        if (prompt && prompt[0]) {
            /* Password obtained — use rewritten command */
            if (out_password) *out_password = prompt;
            return rewritten;
        }
        /* Skipped or timeout — return original command */
        free(prompt);
        free(rewritten);
        return strdup(command);
    }
}

/* B07: Rewrite A && B & (or A || B &) to A && { B & } at depth 0.
 * Bash parses A && B & as a subshell fork for the whole compound, making
 * the subshell wait for B to finish. Wrapping in braces runs B as a simple
 * backgrounded child that exits immediately. Port of Python
 * terminal_tool._rewrite_compound_background().
 * Returns malloc'd string; caller must free(). */
char *terminal_rewrite_compound_background(const char *command) {
    if (!command || !command[0]) return strdup(command ? command : "");

    /* First pass: walk the command to find rewrites.
     * Max rewrites: one per background operator. */
    typedef struct { int chain_end; int amp_pos; } rewrite_t;
    rewrite_t rewrites[64];
    int rewrite_count = 0;
    int n = (int)strlen(command);
    int i = 0, paren_depth = 0, brace_depth = 0, last_chain_op_end = -1;

    while (i < n) {
        char ch = command[i];
        if (ch == '\n' && paren_depth == 0 && brace_depth == 0) { last_chain_op_end = -1; i++; continue; }
        if (ch == ' ' || ch == '\t' || ch == '\r') { i++; continue; }
        if (ch == '#') { while (i < n && command[i] != '\n') i++; continue; }
        if (ch == '\\' && i + 1 < n) { i += 2; continue; }
        if (ch == '\'' || ch == '"') { int e = 0; free(terminal_read_shell_token(command, i, &e)); i = (e > i) ? e : i + 1; continue; }
        if (ch == '(') { paren_depth++; i++; continue; }
        if (ch == ')') { paren_depth = (paren_depth > 0) ? paren_depth - 1 : 0; i++; continue; }
        if (ch == '{' && i + 1 < n && (command[i+1] == ' ' || command[i+1] == '\t' || command[i+1] == '\n' || command[i+1] == '\r')) { brace_depth++; i++; continue; }
        if (ch == '}' && brace_depth > 0) { brace_depth--; last_chain_op_end = -1; i++; continue; }
        if (paren_depth > 0 || brace_depth > 0) { i++; continue; }

        if ((ch == '&' && i + 1 < n && command[i+1] == '&') || (ch == '|' && i + 1 < n && command[i+1] == '|')) { last_chain_op_end = i + 2; i += 2; continue; }
        if (ch == ';' || ch == '|') { last_chain_op_end = -1; i++; continue; }

        if (ch == '&') {
            if (i + 1 < n && command[i+1] == '>') { i += 2; continue; }
            int j = i - 1; while (j >= 0 && (command[j] == ' ' || command[j] == '\t')) j--;
            if (j >= 0 && (command[j] == '>' || command[j] == '<')) { i++; continue; }
            if (last_chain_op_end >= 0 && rewrite_count < 64) {
                rewrites[rewrite_count].chain_end = last_chain_op_end;
                rewrites[rewrite_count].amp_pos = i;
                rewrite_count++;
            }
            last_chain_op_end = -1; i++; continue;
        }

        int e = 0; free(terminal_read_shell_token(command, i, &e)); i = (e > i) ? e : i + 1;
    }

    if (rewrite_count == 0) return strdup(command);

    /* Apply rewrites right-to-left. Each rewrite:
     *   result = result[:insert_pos] + "{ " + result[insert_pos:amp_pos] + " & }" + result[amp_pos+1:]
     * where insert_pos = chain_end with trailing whitespace skipped.
     * Right-to-left order ensures outer rewrite indices are valid (they are left of inner's changes). */
    char *result = strdup(command);
    if (!result) return NULL;

    for (int r = rewrite_count - 1; r >= 0; r--) {
        int chain_end = rewrites[r].chain_end;
        int amp_pos = rewrites[r].amp_pos;

        int insert_pos = chain_end;
        while (insert_pos < amp_pos && (result[insert_pos] == ' ' || result[insert_pos] == '\t'))
            insert_pos++;

        /* Build: result[:insert_pos] + "{ " + result[insert_pos:amp_pos] + " & }" + result[amp_pos+1:] */
        int result_len = (int)strlen(result);
        int prefix_len = insert_pos;
        int mid_len = amp_pos - insert_pos;
        int suffix_len = result_len - (amp_pos + 1);
        if (suffix_len < 0) suffix_len = 0;

        /* New length = prefix + 2 + mid + 4 + suffix */
        int new_len = prefix_len + 2 + mid_len + 4 + suffix_len;
        char *new_result = (char *)malloc((size_t)new_len + 1);
        if (!new_result) { free(result); return strdup(command); }

        int pos = 0;
        memcpy(new_result + pos, result, (size_t)prefix_len); pos += prefix_len;
        memcpy(new_result + pos, "{ ", 2); pos += 2;
        if (mid_len > 0) { memcpy(new_result + pos, result + insert_pos, (size_t)mid_len); pos += mid_len; }
        memcpy(new_result + pos, " & }", 4); pos += 4;
        if (suffix_len > 0) { memcpy(new_result + pos, result + amp_pos + 1, (size_t)suffix_len); pos += suffix_len; }
        new_result[pos] = '\0';

        free(result);
        result = new_result;
    }

    return result;
}

/* Inject exit_code_interpretation field into result JSON */
static char *_inject_interpretation(const char *result_json, const char *command) {
    if (!result_json) return NULL;
    json_t *rj = json_parse(result_json, NULL);
    if (!rj) return strdup(result_json);

    int exit_code = (int)json_get_num(rj, "exit_code", 0);
    const char *interpretation = exit_code_interpret(command, exit_code);
    if (interpretation) {
        json_set(rj, "exit_code_interpretation", json_string(interpretation));
    }
    _inject_sudo_failure(rj, command);
    char *out = json_serialize(rj);
    json_free(rj);
    return out;
}

/* Strip quoted content from command to prevent false-positive pattern matches.
 * Mirrors Python terminal_tool._strip_quotes().
 * Removes content inside single quotes, double quotes (with escape handling),
 * and backtick-quoted strings. Replaces with empty quotes to preserve structure. */
static char *_strip_quotes(const char *command) {
    if (!command) return NULL;
    size_t len = strlen(command);
    if (len == 0) return strdup("");
    char *result = strdup(command);
    if (!result) return NULL;

    size_t dst = 0;
    for (size_t i = 0; i < len; i++) {
        if (result[i] == '\'' || result[i] == '`') {
            char quote = result[i];
            result[dst++] = quote;
            result[dst++] = quote;  /* '' or `` */
            /* Skip until matching close quote */
            i++;
            while (i < len && result[i] != quote) i++;
            if (result[i] == quote) {
                /* Skip the closing quote — already emitted the pair above */
            }
        } else if (result[i] == '"') {
            /* Double-quoted string with possible backslash escapes */
            result[dst++] = '"';
            result[dst++] = '"';  /* "" */
            i++;
            while (i < len) {
                if (result[i] == '\\' && i + 1 < len) {
                    i += 2;  /* skip escaped char */
                    continue;
                }
                if (result[i] == '"') break;
                i++;
            }
            if (result[i] == '"') {
                /* skip closing quote — already emitted the pair */
            }
        } else {
            result[dst++] = result[i];
        }
    }
    result[dst] = '\0';
    return result;
}

/* Read one shell token from a command string, preserving quotes/escapes.
 * Port of Python terminal_tool._read_shell_token().
 * Reads token starting at *start. On success, sets *end to position after token
 * and returns a malloc'd copy of the token. Caller must free().
 * Returns NULL on error. */
char *terminal_read_shell_token(const char *command, int start, int *end) {
    if (!command || start < 0 || !end)
        return NULL;

    int n = (int)strlen(command);
    int i = start;
    *end = start;

    /* Empty input */
    if (i >= n) return NULL;

    /* Count maximum token length for allocation */
    int token_start = i;

    while (i < n) {
        char ch = command[i];
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' ||
            ch == ';' || ch == '|' || ch == '&' || ch == '(' || ch == ')')
            break;

        if (ch == '\'') {
            i++;
            while (i < n && command[i] != '\'') i++;
            if (i < n) i++; /* skip closing quote */
            continue;
        }

        if (ch == '"') {
            i++;
            while (i < n) {
                char inner = command[i];
                if (inner == '\\' && i + 1 < n) {
                    i += 2;
                    continue;
                }
                if (inner == '"') {
                    i++;
                    break;
                }
                i++;
            }
            continue;
        }

        if (ch == '\\' && i + 1 < n) {
            i += 2;
            continue;
        }

        i++;
    }

    int token_len = i - token_start;
    if (token_len <= 0) return NULL;

    char *token = (char *)malloc((size_t)token_len + 1);
    if (!token) return NULL;
    memcpy(token, command + token_start, (size_t)token_len);
    token[token_len] = '\0';

    *end = i;
    return token;
}

/* Check command for shell-level background wrappers and suggest background=true.
 * Mirrors Python terminal_tool._foreground_background_guidance(). */
static const char *_check_foreground_guidance(const char *command) {
    if (!command) return NULL;

    /* Skip leading whitespace to find first token */
    const char *first = command;
    while (*first == ' ' || *first == '\t') first++;

    /* Check if the first token looks like a shell environment assignment
     * (e.g. PATH=/usr/bin command). Mirrors Python _looks_like_env_assignment(). */
    if (*first) {
        const char *eq = strchr(first, '=');
        if (eq && eq != first) {
            /* Check that only '=' and alphanumeric/underscore chars precede it */
            bool valid = true;
            for (const char *p = first; p < eq; p++) {
                if (p == first) {
                    if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || *p == '_'))
                        valid = false;
                } else {
                    if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
                          (*p >= '0' && *p <= '9') || *p == '_'))
                        valid = false;
                }
            }
            /* Verify the character after '=' is not whitespace (would be empty value, unlikely env var) */
            if (valid) {
                /* Env assignment — likely a non-interactive command prefix.
                 * Skip guidance (matches Python behaviour: env assignments are setup, not commands). */
                return NULL;
            }
        }
    }

    /* Check if this is a help/version command — return no guidance needed.
     * Mirrors Python terminal_tool._looks_like_help_or_version_command(). */
    {
        char cmd_lower[512];
        size_t clen = strlen(command);
        if (clen < sizeof(cmd_lower)) {
            for (size_t i = 0; i < clen; i++)
                cmd_lower[i] = (char)tolower((unsigned char)command[i]);
            cmd_lower[clen] = '\0';
            /* --help or --version anywhere in command */
            if (strstr(cmd_lower, " --help") != NULL ||
                strstr(cmd_lower, " --version") != NULL)
                return NULL;
            /* Ends with -h or -v (preceded by space) */
            int len = (int)strlen(cmd_lower);
            if ((len >= 3 && strcmp(cmd_lower + len - 3, " -h") == 0) ||
                (len >= 3 && strcmp(cmd_lower + len - 3, " -v") == 0))
                return NULL;
        }
    }

    /* Strip quotes to avoid false positives on echoed text */
    char *stripped = _strip_quotes(command);
    if (!stripped) return NULL;
    const char *check = stripped;

    /* Check for shell-level background wrappers */
    const char *patterns[] = {"nohup ", " disown", " setsid", NULL};
    for (int i = 0; patterns[i]; i++) {
        if (strstr(check, patterns[i])) {
            free(stripped);
            return
                "Foreground command uses shell-level background wrappers (nohup/disown/setsid). "
                "Use terminal(background=true) so Hermes can track lifecycle, "
                "then run readiness checks and tests in separate commands.";
        }
    }
    /* Check for trailing & backgrounding */
    size_t len = strlen(check);
    if (len > 0 && check[len-1] == '&') {
        free(stripped);
        return
            "Foreground command uses '&' backgrounding. "
            "Use terminal(background=true) for long-lived processes, "
            "then run health checks and tests in follow-up calls.";
    }
    /* Check for inline & (cmd1 & cmd2) */
    if (strstr(check, " & ") || strstr(check, " &;")) {
        free(stripped);
        return
            "Foreground command uses inline '&' backgrounding. "
            "Use terminal(background=true) for long-lived processes, "
            "then run health checks and tests in follow-up calls.";
    }
    /* Check for known long-lived foreground processes (servers, watchers).
     * Mirrors Python terminal_tool._LONG_LIVED_FOREGROUND_PATTERNS. */
    {
        /* Build lower-case copy for case-insensitive matching */
        char lc[512];
        size_t clen = strlen(check);
        if (clen >= sizeof(lc)) clen = sizeof(lc) - 1;
        for (size_t i = 0; i < clen; i++)
            lc[i] = (char)tolower((unsigned char)check[i]);
        lc[clen] = '\0';

        bool is_long_lived = false;
        /* npm/pnpm/yarn/bun with run: "npm run dev|start|serve|watch" */
        if (strstr(lc, "npm run") || strstr(lc, "pnpm run") ||
            strstr(lc, "yarn run") || strstr(lc, "bun run"))
            is_long_lived = true;
        /* npm/pnpm/yarn/bun dev|start|serve|watch (without "run") */
        else if (strstr(lc, "npm dev") || strstr(lc, "pnpm dev") ||
                 strstr(lc, "yarn dev") || strstr(lc, "bun dev"))
            is_long_lived = true;
        else if (strstr(lc, "npm start") || strstr(lc, "pnpm start") ||
                 strstr(lc, "yarn start") || strstr(lc, "bun start"))
            is_long_lived = true;
        else if (strstr(lc, "npm serve") || strstr(lc, "pnpm serve") ||
                 strstr(lc, "yarn serve") || strstr(lc, "bun serve"))
            is_long_lived = true;
        else if (strstr(lc, "npm watch") || strstr(lc, "pnpm watch") ||
                 strstr(lc, "yarn watch") || strstr(lc, "bun watch"))
            is_long_lived = true;
        /* docker compose up */
        else if (strstr(lc, "docker compose up") || strstr(lc, "docker-compose up"))
            is_long_lived = true;
        /* next dev */
        else if (strstr(lc, "next dev"))
            is_long_lived = true;
        /* vite */
        else if (strstr(lc, "vite ") || strcmp(lc, "vite") == 0)
            is_long_lived = true;
        /* nodemon */
        else if (strstr(lc, "nodemon"))
            is_long_lived = true;
        /* uvicorn */
        else if (strstr(lc, "uvicorn"))
            is_long_lived = true;
        /* gunicorn */
        else if (strstr(lc, "gunicorn"))
            is_long_lived = true;
        /* python -m http.server */
        else if (strstr(lc, "python -m http.server") ||
                 strstr(lc, "python3 -m http.server"))
            is_long_lived = true;
        /* py (Windows python launcher) -m http.server */
        else if (strstr(lc, "py -m http.server"))
            is_long_lived = true;

        if (is_long_lived) {
            free(stripped);
            return
                "This foreground command appears to start a long-lived server/watch process. "
                "Run it with background=true, verify readiness (health endpoint/log signal), "
                "then execute tests in a separate command.";
        }
    }
    free(stripped);
    return NULL;
}

/* ================================================================
 *  Main terminal handler
 * ================================================================ */

char *terminal_handler(const char *args_json, const char *task_id) {
    (void)task_id;

    if (!args_json)
        return strdup("{\"error\": \"No arguments provided\", \"status\": \"error\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) {
        char buf[512];
        snprintf(buf, sizeof(buf), "{\"error\": \"JSON parse error: %s\", \"status\": \"error\"}", err ? err : "unknown");
        free(err);
        return strdup(buf);
    }

    /* Copy strings from JSON tree before freeing */
    const char *command_raw = json_object_get_string(args, "command", NULL);
    char command_buf[16384] = "";
    if (command_raw) snprintf(command_buf, sizeof(command_buf), "%s", command_raw);

    /* F12: Timeout propagation — read from args, fallback to tool config, then default */
    int timeout = (int)json_object_get_number(args, "timeout", 0);
    if (timeout <= 0) {
        timeout = tool_config_get_int("terminal", "timeout", DEFAULT_TIMEOUT);
    }

    /* F09: PTY mode */
    bool use_pty = json_object_get_bool(args, "pty", false);

    /* Check if command requires piped stdin instead of PTY.
     * Some CLIs (gh auth login --with-token) change behavior when stdin
     * is a TTY and hang forever waiting for piped input.
     * Port of Python terminal_tool._command_requires_pipe_stdin(). */
    if (use_pty && command_buf[0]) {
        char cmd_lower[512];
        size_t clen = strlen(command_buf);
        if (clen < sizeof(cmd_lower)) {
            for (size_t i = 0; i < clen; i++)
                cmd_lower[i] = (char)tolower((unsigned char)command_buf[i]);
            cmd_lower[clen] = '\0';
            /* Check: gh auth login --with-token requires piped stdin, not PTY */
            if (strstr(cmd_lower, "gh auth login") != NULL &&
                strstr(cmd_lower, "--with-token") != NULL) {
                use_pty = false;
            }
        }
    }

    /* F13: Force — skip sandbox escape check (use after user confirms) */
    bool force = json_object_get_bool(args, "force", false);

    /* Foreground timeout guard: reject foreground commands over 600s
     * Only when timeout was explicitly provided (not default fallback).
     * Test binaries with unresolved tool_config_get_int will get garbage
     * defaults that trigger false positives. */
    bool timeout_explicit = json_object_get(args, "timeout") != NULL;
    if (!force && timeout_explicit && timeout > 600) {
        char err[256];
        snprintf(err, sizeof(err),
                 "{\"error\":\"Foreground timeout %d exceeds maximum of 600s. "
                 "Use background=true with notify_on_complete=true for long-running commands.\","
                 "\"status\":\"error\"}", timeout);
        json_free(args);
        return strdup(err);
    }

    /* Approval security check: block dangerous commands unless force=true */
    if (!force && command_buf[0]) {
        char *normalized = approval_normalize_command(command_buf);
        if (normalized) {
            bool dangerous = approval_is_terminal_dangerous(normalized);
            free(normalized);
            if (dangerous) {
                char err[1024];
                snprintf(err, sizeof(err),
                         "{\"error\":\"BLOCKED: command matches dangerous pattern. "
                         "Set force=true to bypass (requires user confirmation).\","
                         "\"status\":\"error\",\"blocked\":true}");
                json_free(args);
                return strdup(err);
            }
        }
    }

    /* F10: Environment isolation — read optional env string (KEY=VALUE KEY2=VALUE2) */
    const char *env_raw = json_object_get_string(args, "env", NULL);
    char env_buf[4096] = "";
    if (env_raw) snprintf(env_buf, sizeof(env_buf), "%s", env_raw);

    /* Workdir */
    const char *workdir_raw = json_object_get_string(args, "workdir", NULL);
    char workdir_buf[4096] = "";
    if (workdir_raw) snprintf(workdir_buf, sizeof(workdir_buf), "%s", workdir_raw);

    /* F11: Docker backend — read backend arg, fallback to config */
    const char *backend_arg = json_object_get_string(args, "backend", NULL);
    const char *backend = backend_arg;
    if (!backend || !backend[0])
        backend = tool_config_get("terminal", "backend");

    /* F11: Docker image override */
    const char *docker_raw = json_object_get_string(args, "docker_image", NULL);
    char docker_buf[1024] = "";
    if (docker_raw) snprintf(docker_buf, sizeof(docker_buf), "%s", docker_raw);

    json_free(args);

    const char *command = command_buf[0] ? command_buf : NULL;
    const char *env_str = env_buf[0] ? env_buf : NULL;
    const char *workdir = workdir_buf[0] ? workdir_buf : NULL;
    const char *docker_image = docker_buf[0] ? docker_buf : NULL;

    if (!command)
        return strdup("{\"error\": \"Missing required 'command' argument\", \"status\": \"error\"}");

    /* Safety checks: workdir validation + disk usage warning */
    const char *wd_warn = _check_workdir(workdir);
    const char *disk_warn = _check_disk_usage(workdir);
    const char *guidance = _check_foreground_guidance(command);

    /* F14: Validate workdir uses only safe filesystem characters.
     * Blocking check (rejects command) — not a warning. */
    const char *wd_valid = _validate_workdir(workdir);
    if (wd_valid) {
        char err[512];
        snprintf(err, sizeof(err),
                 "{\"error\": \"%s\", \"status\": \"error\"}", wd_valid);
        return strdup(err);
    }

    /* O14: Check command for sandbox escape patterns before execution */
    sandbox_escape_result_t esc = sandbox_escape_check(command, -1, "terminal");
    if (esc.blocked) {
        if (force) {
            /* Force flag skips sandbox escape check (user confirmed) */
        } else {
            char err[512];
            snprintf(err, sizeof(err),
                     "{\"error\": \"%s\", \"status\": \"error\"}", esc.reason);
            return strdup(err);
        }
    }

    /* B07: Transform sudo commands for piped password support.
     * Calls _transform_sudo() which handles SUDO_PASSWORD env var,
     * passwordless sudo detection, interactive prompt, and command rewrite.
     * The result replaces the command pointer for downstream use.
     * sudo_password persists for PTY stdin piping below. */
    char *sudo_password = NULL;
    {
        char *transformed = _transform_sudo(command, &sudo_password);
        if (transformed) {
            if (strcmp(transformed, command) != 0) {
                /* Transformation changed the command — copy into command_buf */
                size_t tlen = strlen(transformed);
                if (tlen < sizeof(command_buf)) {
                    memcpy(command_buf, transformed, tlen + 1);
                    command = command_buf;
                }
            }
            free(transformed);
        }
    }

    /* Build workdir prefix */
    char workdir_prefix[4096] = "";
    if (workdir && workdir[0]) {
        snprintf(workdir_prefix, sizeof(workdir_prefix), "cd %s && ", workdir);
    }

    /* Wrap command with env and workdir */
    char full_command[16384];
    char *passthrough_export = build_env_passthrough_export();
    if (passthrough_export) {
        if (env_str && env_str[0]) {
            snprintf(full_command, sizeof(full_command), "%s %s %s%s%s",
                    passthrough_export, env_str, workdir_prefix, use_pty ? "" : "", command);
        } else {
            snprintf(full_command, sizeof(full_command), "%s %s%s%s",
                    passthrough_export, workdir_prefix, use_pty ? "" : "", command);
        }
        free(passthrough_export);
    } else if (env_str && env_str[0]) {
        snprintf(full_command, sizeof(full_command), "%s %s%s%s",
                env_str, workdir_prefix, use_pty ? "" : "", command);
    } else {
        snprintf(full_command, sizeof(full_command), "%s%s",
                workdir_prefix, command);
    }

    if (use_pty) {
#ifdef __linux__
        char *res1 = run_command_pty(full_command, timeout, sudo_password);
        char *w1 = _inject_warnings(res1, wd_warn, disk_warn, guidance);
        free(res1);
        free(sudo_password);
        return _inject_interpretation(w1, command);
#else
        char *res1b = run_command(full_command, timeout);
        char *w1b = _inject_warnings(res1b, wd_warn, disk_warn, guidance);
        free(res1b);
        free(sudo_password);
        return _inject_interpretation(w1b, command);
#endif
    }

    /* D84: Route to SSH execution backend if configured */
    if (backend && strcasecmp(backend, "ssh") == 0) {
        char *res2 = run_command_ssh(command, timeout);
        char *w2 = _inject_warnings(res2, wd_warn, disk_warn, guidance);
        free(res2);
        return _inject_interpretation(w2, command);
    }

    /* F11: Route to Docker execution backend if configured */
    if (backend && strcasecmp(backend, "docker") == 0) {
        char *res3 = run_command_docker(command, timeout, docker_image);
        char *w3 = _inject_warnings(res3, wd_warn, disk_warn, guidance);
        free(res3);
        return _inject_interpretation(w3, command);
    }

    /* D04: Route to Docker Compose backend if configured */
    if (backend && (strcasecmp(backend, "docker-compose") == 0 || strcasecmp(backend, "compose") == 0)) {
        char *res4 = run_command_docker_compose(command, timeout);
        return _inject_interpretation(res4, command);
    }

    /* M35: Route to Modal execution backend if configured */
    if (backend && strcasecmp(backend, "modal") == 0) {
        char *res5 = run_command_modal(command, timeout);
        return _inject_interpretation(res5, command);
    }

    char *res_default = run_command(full_command, timeout);
    char *w_default = _inject_warnings(res_default, wd_warn, disk_warn, guidance);
    free(res_default);
    return _inject_interpretation(w_default, command);
}

/* Auto-register */
void registry_init_terminal(void) {
    /* Wire env_passthrough config — register comma-separated vars from terminal.env_passthrough */
    const char *passthrough = tool_config_get("terminal", "env_passthrough");
    if (passthrough && passthrough[0]) {
        char copy[1024];
        snprintf(copy, sizeof(copy), "%s", passthrough);
        char *save = NULL;
        char *tok = strtok_r(copy, ",\t ", &save);
        while (tok) {
            while (*tok == ' ' || *tok == '\t') tok++;
            if (*tok) {
                if (!env_passthrough_register(tok)) {
                    /* GHSA-rhgp-j443-p4rf: warn when blocked credential attempted */
                    fprintf(stderr, "[terminal] WARNING: env_passthrough '%s' blocked — "
                            "provider credential cannot be forwarded to sandboxed execution.\n", tok);
                }
            }
            tok = strtok_r(NULL, ",\t ", &save);
        }
    }
    registry_register("terminal",
        "Execute a shell command. Returns stdout, stderr, and exit code. "
        "Use for running code, compiling, testing, and system operations.",
        SCHEMA, terminal_handler);
}
