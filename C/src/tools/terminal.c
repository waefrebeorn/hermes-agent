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
      "\"env\":{\"type\":\"string\",\"description\":\"Environment variables in KEY=VALUE KEY2=VALUE2 format\"},"
      "\"workdir\":{\"type\":\"string\",\"description\":\"Working directory for the command\"},"
      "\"backend\":{\"type\":\"string\",\"description\":\"Execution backend: local (default), docker, docker-compose, ssh, modal\"},"
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
    json_object_set(result, "truncated",
                    json_new_bool(len >= (size_t)tool_output_get_max_bytes() - 25));

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
 *  Main terminal handler
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
        return strdup("{\"error\": \"Missing required 'command' argument\"}");

    /* O14: Check command for sandbox escape patterns before execution */
    sandbox_escape_result_t esc = sandbox_escape_check(command, -1, "terminal");
    if (esc.blocked) {
        char err[512];
        snprintf(err, sizeof(err),
                 "{\"error\": \"%s\"}", esc.reason);
        return strdup(err);
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
        return run_command_pty(full_command, timeout);
#else
        return run_command(full_command, timeout);
#endif
    }

    /* D84: Route to SSH execution backend if configured */
    if (backend && strcasecmp(backend, "ssh") == 0) {
        return run_command_ssh(command, timeout);
    }

    /* F11: Route to Docker execution backend if configured */
    if (backend && strcasecmp(backend, "docker") == 0) {
        return run_command_docker(command, timeout, docker_image);
    }

    /* D04: Route to Docker Compose backend if configured */
    if (backend && (strcasecmp(backend, "docker-compose") == 0 || strcasecmp(backend, "compose") == 0)) {
        return run_command_docker_compose(command, timeout);
    }

    /* M35: Route to Modal execution backend if configured */
    if (backend && strcasecmp(backend, "modal") == 0) {
        return run_command_modal(command, timeout);
    }

    return run_command(full_command, timeout);
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
            if (*tok) env_passthrough_register(tok);
            tok = strtok_r(NULL, ",\t ", &save);
        }
    }
    registry_register("terminal",
        "Execute a shell command. Returns stdout, stderr, and exit code. "
        "Use for running code, compiling, testing, and system operations.",
        SCHEMA, terminal_handler);
}
