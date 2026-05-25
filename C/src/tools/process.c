/*
 * process.c — Background process management tool for Hermes C.
 * Manages child processes: start, poll, log, kill, wait, signal, write, submit, close.
 * F34: Send arbitrary signals
 * F35: Environment override on start
 * F36: Per-process timeout auto-kill
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#define MAX_PROCESSES 32
#define PROC_OUTPUT_BUF (64 * 1024)
#define CHECKPOINT_FILE "processes.json"

/* Checkpoint path buffer (lazily resolved) */
static char g_checkpoint_path[4096] = {0};

/* Build checkpoint file path from SLERMES_HOME or HOME */
static const char *checkpoint_path(void) {
    if (g_checkpoint_path[0] == '\0') {
        const char *home = getenv("SLERMES_HOME");
        if (!home || !home[0]) home = getenv("HOME");
        if (!home || !home[0]) home = ".";
        snprintf(g_checkpoint_path, sizeof(g_checkpoint_path),
                 "%s/%s", home, CHECKPOINT_FILE);
    }
    return g_checkpoint_path;
}

typedef struct {
    pid_t   pid;
    int     session_id;
    char    command[4096];
    char   *output;       /* accumulated output */
    size_t  output_len;
    size_t  output_cap;
    bool    running;
    int     exit_code;
    time_t  start_time;
    int     stdin_fd;    /* write end of stdin pipe */
    /* F36: Per-process timeout */
    int     timeout_sec; /* 0 = no timeout */
} managed_process_t;

static managed_process_t g_procs[MAX_PROCESSES];
static int g_next_session = 1;

static int find_free_slot(void) {
    for (int i = 0; i < MAX_PROCESSES; i++)
        if (g_procs[i].pid == 0) return i;
    return -1;
}

static int find_by_session(int session_id) {
    for (int i = 0; i < MAX_PROCESSES; i++)
        if (g_procs[i].session_id == session_id) return i;
    return -1;
}

static void reap_children(void) {
    time_t now = time(NULL);
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (g_procs[i].pid > 0 && g_procs[i].running) {
            /* F36: Check per-process timeout */
            if (g_procs[i].timeout_sec > 0 &&
                (now - g_procs[i].start_time) >= g_procs[i].timeout_sec) {
                kill(g_procs[i].pid, SIGTERM);
                usleep(100000);
                kill(g_procs[i].pid, SIGKILL);
                g_procs[i].running = false;
                g_procs[i].exit_code = -1;
                if (g_procs[i].stdin_fd >= 0) close(g_procs[i].stdin_fd);
                g_procs[i].stdin_fd = -1;
                continue;
            }

            int status;
            pid_t result = waitpid(g_procs[i].pid, &status, WNOHANG);
            if (result == g_procs[i].pid) {
                g_procs[i].running = false;
                g_procs[i].exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
                if (WIFSIGNALED(status))
                    g_procs[i].exit_code = -WTERMSIG(status);
                if (g_procs[i].stdin_fd >= 0) close(g_procs[i].stdin_fd);
                g_procs[i].stdin_fd = -1;
            }
        }
    }
}

static void proc_start(const char *command, const char *env_str, int timeout_sec,
                        json_node_t *result) {
    reap_children();
    int slot = find_free_slot();
    if (slot < 0) {
        json_object_set(result, "error", json_new_string("Max processes reached"));
        return;
    }

    int stdin_pipe[2];
    if (pipe(stdin_pipe) < 0) {
        json_object_set(result, "error", json_new_string("pipe failed"));
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(stdin_pipe[0]); close(stdin_pipe[1]);
        json_object_set(result, "error", json_new_string("fork failed"));
        return;
    }

    if (pid == 0) {
        /* Child */
        close(stdin_pipe[1]);  /* close write end */
        dup2(stdin_pipe[0], STDIN_FILENO);
        close(stdin_pipe[0]);

        /* F35: Apply environment overrides */
        if (env_str && env_str[0]) {
            /* Parse "KEY=VALUE KEY2=VALUE2" format and setenv each */
            char copy[4096];
            snprintf(copy, sizeof(copy), "%s", env_str);
            char *save = NULL;
            char *token = strtok_r(copy, " ", &save);
            while (token) {
                char *eq = strchr(token, '=');
                if (eq) {
                    *eq = '\0';
                    setenv(token, eq + 1, 1);
                }
                token = strtok_r(NULL, " ", &save);
            }
        }

        execl("/bin/sh", "sh", "-c", command, (char *)NULL);
        _exit(127);
    }

    /* Parent */
    close(stdin_pipe[0]); /* close read end */
    g_procs[slot].pid = pid;
    g_procs[slot].session_id = g_next_session++;
    snprintf(g_procs[slot].command, sizeof(g_procs[slot].command), "%s", command);
    g_procs[slot].output = NULL;
    g_procs[slot].output_len = 0;
    g_procs[slot].output_cap = 0;
    g_procs[slot].running = true;
    g_procs[slot].start_time = time(NULL);
    g_procs[slot].stdin_fd = stdin_pipe[1];
    g_procs[slot].timeout_sec = timeout_sec > 0 ? timeout_sec : 0;

    json_object_set(result, "session_id", json_new_number((double)g_procs[slot].session_id));
    json_object_set(result, "pid", json_new_number((double)pid));
    json_object_set(result, "status", json_new_string("started"));
    if (timeout_sec > 0)
        json_object_set(result, "timeout_sec", json_new_number((double)timeout_sec));
}

/* F34: Send arbitrary signal to a managed process */
static void proc_signal(int slot, int signum, json_node_t *result) {
    if (!g_procs[slot].running) {
        json_object_set(result, "error", json_new_string("Process not running"));
        return;
    }
    if (kill(g_procs[slot].pid, signum) == 0) {
        json_object_set(result, "status", json_new_string("signal_sent"));
        json_object_set(result, "signal", json_new_number((double)signum));
        if (signum == SIGTERM || signum == SIGKILL || signum == SIGQUIT) {
            /* For kill signals, clean up after brief delay */
            usleep(50000);
            int status;
            pid_t result_pid = waitpid(g_procs[slot].pid, &status, WNOHANG);
            if (result_pid == g_procs[slot].pid) {
                g_procs[slot].running = false;
                g_procs[slot].exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
                if (g_procs[slot].stdin_fd >= 0) close(g_procs[slot].stdin_fd);
                g_procs[slot].stdin_fd = -1;
            }
        }
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "kill(%d, %d) failed: %s",
                 (int)g_procs[slot].pid, signum, strerror(errno));
        json_object_set(result, "error", json_new_string(buf));
    }
}

/* Map signal name string to signal number */
static int signal_name_to_number(const char *name) {
    if (!name) return -1;
    struct { const char *name; int num; } sigmap[] = {
        {"SIGTERM", SIGTERM}, {"TERM", SIGTERM}, {"15", SIGTERM},
        {"SIGKILL", SIGKILL}, {"KILL", SIGKILL}, {"9", SIGKILL},
        {"SIGINT", SIGINT},   {"INT", SIGINT},   {"2", SIGINT},
        {"SIGHUP", SIGHUP},   {"HUP", SIGHUP},   {"1", SIGHUP},
        {"SIGQUIT", SIGQUIT}, {"QUIT", SIGQUIT}, {"3", SIGQUIT},
        {"SIGSTOP", SIGSTOP}, {"STOP", SIGSTOP}, {"19", SIGSTOP},
        {"SIGCONT", SIGCONT}, {"CONT", SIGCONT}, {"18", SIGCONT},
        {"SIGUSR1", SIGUSR1}, {"USR1", SIGUSR1},
        {"SIGUSR2", SIGUSR2}, {"USR2", SIGUSR2},
        {NULL, 0}
    };
    for (int i = 0; sigmap[i].name; i++) {
        if (strcasecmp(name, sigmap[i].name) == 0)
            return sigmap[i].num;
    }
    return -1;
}

/* Save checkpoint: serialize running process state to JSON file */
static void proc_save_checkpoint(void) {
    const char *path = checkpoint_path();
    json_node_t *root = json_new_object();
    json_node_t *procs = json_new_array();
    time_t now = time(NULL);

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (g_procs[i].pid == 0) continue;
        json_node_t *p = json_new_object();
        json_object_set(p, "session_id", json_new_number((double)g_procs[i].session_id));
        json_object_set(p, "pid", json_new_number((double)g_procs[i].pid));
        json_object_set(p, "command", json_new_string(g_procs[i].command));
        json_object_set(p, "running", json_new_bool(g_procs[i].running));
        json_object_set(p, "exit_code", json_new_number((double)g_procs[i].exit_code));
        json_object_set(p, "start_time", json_new_number((double)g_procs[i].start_time));
        json_object_set(p, "timeout_sec", json_new_number((double)g_procs[i].timeout_sec));
        if (g_procs[i].output && g_procs[i].output_len > 0)
            json_object_set(p, "output_preview", json_new_string(g_procs[i].output));
        json_array_append(procs, p);
    }
    json_object_set(root, "processes", procs);
    json_object_set(root, "saved_at", json_new_number((double)now));

    char *json_str = json_serialize(root);
    if (json_str) {
        FILE *fp = fopen(path, "w");
        if (fp) {
            fputs(json_str, fp);
            fclose(fp);
        }
        free(json_str);
    }
    json_free(root);
}

/* Load checkpoint: restore process state from JSON file, reap dead processes */
static void proc_load_checkpoint(void) {
    /* Only load once */
    static bool loaded = false;
    if (loaded) return;
    loaded = true;

    const char *path = checkpoint_path();
    FILE *fp = fopen(path, "r");
    if (!fp) return;

    /* Read file */
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    if (fsize <= 0) { fclose(fp); return; }
    rewind(fp);
    char *content = (char *)malloc((size_t)fsize + 1);
    if (!content) { fclose(fp); return; }
    size_t nread = fread(content, 1, (size_t)fsize, fp);
    fclose(fp);
    content[nread] = '\0';

    char *err = NULL;
    json_node_t *root = json_parse(content, &err);
    free(content);
    if (!root) { free(err); return; }

    json_node_t *procs = json_obj_get(root, "processes");
    if (!procs || procs->type != JSON_ARRAY) { json_free(root); return; }

    for (size_t i = 0; i < json_len(procs); i++) {
        json_node_t *p = json_get(procs, i);
        if (!p || p->type != JSON_OBJECT) continue;

        int slot = find_free_slot();
        if (slot < 0) break;

        double sid = json_get_num(p, "session_id", 0);
        double pid = json_get_num(p, "pid", 0);

        g_procs[slot].session_id = (int)sid;
        g_procs[slot].pid = (pid_t)pid;
        g_procs[slot].running = json_get_bool(p, "running", false);
        g_procs[slot].exit_code = (int)json_get_num(p, "exit_code", -1);
        g_procs[slot].start_time = (time_t)json_get_num(p, "start_time", 0);
        g_procs[slot].timeout_sec = (int)json_get_num(p, "timeout_sec", 0);
        g_procs[slot].stdin_fd = -1; /* stdin pipe lost after restart */
        g_procs[slot].output = NULL;
        g_procs[slot].output_len = 0;
        g_procs[slot].output_cap = 0;

        const char *cmd = json_get_str(p, "command", "");
        snprintf(g_procs[slot].command, sizeof(g_procs[slot].command), "%s", cmd);

        /* Track highest session ID */
        if ((int)sid >= g_next_session)
            g_next_session = (int)sid + 1;
    }

    /* Reap any processes that have exited since checkpoint was saved */
    reap_children();
    json_free(root);
}

char *process_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *action = json_object_get_string(args, "action", "start");
    json_node_t *result = json_new_object();

    /* Load checkpoint on first dispatch */
    proc_load_checkpoint();

    if (strcmp(action, "list") == 0) {
        /* List all managed processes */
        reap_children();
        json_node_t *procs = json_new_array();
        time_t now = time(NULL);
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (g_procs[i].pid == 0) continue;
            json_node_t *p = json_new_object();
            json_object_set(p, "session_id", json_new_number((double)g_procs[i].session_id));
            json_object_set(p, "pid", json_new_number((double)g_procs[i].pid));
            json_object_set(p, "command", json_new_string(g_procs[i].command));
            json_object_set(p, "running", json_new_bool(g_procs[i].running));
            if (!g_procs[i].running)
                json_object_set(p, "exit_code", json_new_number((double)g_procs[i].exit_code));
            /* Uptime in seconds */
            double uptime = (double)(now - g_procs[i].start_time);
            json_object_set(p, "uptime_sec", json_new_number(uptime));
            json_array_append(procs, p);
        }
        json_object_set(result, "processes", procs);
        json_object_set(result, "count", json_new_number((double)json_len(procs)));

    } else if (strcmp(action, "health") == 0) {
        /* Process subsystem health check */
        reap_children();
        int running = 0, finished = 0, stopped = 0;
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (g_procs[i].pid == 0) continue;
            if (g_procs[i].running) running++;
            else finished++;
            if (g_procs[i].stdin_fd < 0 && g_procs[i].running) stopped++;
        }
        json_object_set(result, "status", json_new_string(running > 0 ? "active" : "idle"));
        json_object_set(result, "running", json_new_number((double)running));
        json_object_set(result, "finished", json_new_number((double)finished));
        json_object_set(result, "total_slots", json_new_number((double)MAX_PROCESSES));
        json_object_set(result, "available_slots", json_new_number((double)(MAX_PROCESSES - running - finished)));
        if (stopped > 0)
            json_object_set(result, "stopped_processes", json_new_number((double)stopped));

    } else if (strcmp(action, "cleanup") == 0) {
        /* Remove finished processes older than N seconds (default 1800 = 30min) */
        reap_children();
        int max_age = (int)json_object_get_number(args, "max_age_sec", 1800);
        time_t now = time(NULL);
        int removed = 0;
        int kept_running = 0;
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (g_procs[i].pid == 0) continue;
            if (g_procs[i].running) {
                kept_running++;
                continue;
            }
            if ((now - g_procs[i].start_time) >= max_age) {
                free(g_procs[i].output);
                memset(&g_procs[i], 0, sizeof(g_procs[i]));
                removed++;
            }
        }
        json_object_set(result, "removed", json_new_number((double)removed));
        json_object_set(result, "running", json_new_number((double)kept_running));
        proc_save_checkpoint();

    } else if (strcmp(action, "start") == 0) {
        const char *command = json_object_get_string(args, "command", NULL);
        if (!command) {
            json_object_set(result, "error", json_new_string("Missing command"));
        } else {
            /* F35: Environment overrides */
            const char *env_str = json_object_get_string(args, "env", NULL);
            /* F36: Per-process timeout */
            int timeout_sec = (int)json_object_get_number(args, "timeout", 0);
            proc_start(command, env_str, timeout_sec, result);
            proc_save_checkpoint();
        }
    } else {
        int session_id = (int)json_object_get_number(args, "session_id", 0);
        int slot = find_by_session(session_id);
        if (slot < 0) {
            json_object_set(result, "error", json_new_string("Process not found"));
        } else {
            reap_children();

            if (strcmp(action, "poll") == 0) {
                json_object_set(result, "running", json_new_bool(g_procs[slot].running));
                json_object_set(result, "pid", json_new_number((double)g_procs[slot].pid));
                if (!g_procs[slot].running)
                    json_object_set(result, "exit_code", json_new_number((double)g_procs[slot].exit_code));

            } else if (strcmp(action, "kill") == 0) {
                if (g_procs[slot].running) {
                    kill(g_procs[slot].pid, SIGTERM);
                    usleep(100000);
                    kill(g_procs[slot].pid, SIGKILL);
                    waitpid(g_procs[slot].pid, NULL, WNOHANG);
                }
                g_procs[slot].running = false;
                if (g_procs[slot].stdin_fd >= 0) close(g_procs[slot].stdin_fd);
                g_procs[slot].stdin_fd = -1;
                json_object_set(result, "status", json_new_string("killed"));
                proc_save_checkpoint();

            } else if (strcmp(action, "wait") == 0) {
                /* Wait for process to finish with optional timeout */
                int wait_timeout = (int)json_object_get_number(args, "timeout", 30);
                time_t wait_start = time(NULL);
                while (g_procs[slot].running) {
                    reap_children();
                    if (!g_procs[slot].running) break;
                    if ((time(NULL) - wait_start) >= wait_timeout) {
                        json_object_set(result, "status", json_new_string("timeout"));
                        json_object_set(result, "running", json_new_bool(true));
                        goto done;
                    }
                    usleep(100000); /* 100ms */
                }
                json_object_set(result, "status", json_new_string("completed"));
                json_object_set(result, "exit_code", json_new_number((double)g_procs[slot].exit_code));

            } else if (strcmp(action, "log") == 0) {
                /* Return accumulated output */
                if (g_procs[slot].output && g_procs[slot].output_len > 0) {
                    json_object_set(result, "output", json_new_string(g_procs[slot].output));
                    json_object_set(result, "output_len", json_new_number((double)g_procs[slot].output_len));
                } else {
                    json_object_set(result, "output", json_new_string(""));
                }

            } else if (strcmp(action, "signal") == 0) {
                /* F34: Send arbitrary signal */
                const char *sig_name = json_object_get_string(args, "signal", "SIGTERM");
                int signum = signal_name_to_number(sig_name);
                if (signum < 0) {
                    char buf[128];
                    snprintf(buf, sizeof(buf), "Unknown signal: %s", sig_name);
                    json_object_set(result, "error", json_new_string(buf));
                } else {
                    proc_signal(slot, signum, result);
                    proc_save_checkpoint();
                }

            } else if (strcmp(action, "write") == 0) {
                /* Write data to process stdin */
                const char *data = json_object_get_string(args, "data", "");
                if (g_procs[slot].stdin_fd >= 0 && g_procs[slot].running) {
                    ssize_t written = write(g_procs[slot].stdin_fd, data, strlen(data));
                    if (written >= 0) {
                        json_object_set(result, "written", json_new_number((double)written));
                        json_object_set(result, "status", json_new_string("written"));
                    } else {
                        json_object_set(result, "error", json_new_string(strerror(errno)));
                    }
                } else {
                    json_object_set(result, "error", json_new_string("Process not running or no stdin"));
                }

            } else if (strcmp(action, "submit") == 0) {
                /* Write data + newline to process stdin (for interactive prompts) */
                const char *data = json_object_get_string(args, "data", "");
                if (g_procs[slot].stdin_fd >= 0 && g_procs[slot].running) {
                    /* Write data first */
                    ssize_t total = 0;
                    if (strlen(data) > 0) {
                        ssize_t written = write(g_procs[slot].stdin_fd, data, strlen(data));
                        if (written > 0) total += written;
                    }
                    /* Then write newline (Enter key) */
                    ssize_t nl = write(g_procs[slot].stdin_fd, "\n", 1);
                    if (nl > 0) total += nl;
                    if (total > 0 || (strlen(data) == 0 && nl > 0)) {
                        json_object_set(result, "written", json_new_number((double)total));
                        json_object_set(result, "status", json_new_string("submitted"));
                    } else {
                        json_object_set(result, "error", json_new_string(strerror(errno)));
                    }
                } else {
                    json_object_set(result, "error", json_new_string("Process not running or no stdin"));
                }

            } else if (strcmp(action, "close") == 0) {
                /* Close stdin (send EOF) */
                if (g_procs[slot].stdin_fd >= 0) {
                    close(g_procs[slot].stdin_fd);
                    g_procs[slot].stdin_fd = -1;
                    json_object_set(result, "status", json_new_string("stdin_closed"));
                } else {
                    json_object_set(result, "status", json_new_string("already_closed"));
                }

            } else {
                json_object_set(result, "error", json_new_string("Unknown action"));
            }
        }
    }

done:
    ;
    char *json_out = json_serialize(result);
    json_free(result);
    json_free(args);
    return json_out;
}

void registry_init_process(void) {
    registry_register("process",
        "Manage background processes. Actions: start (run command in bg), "
        "list (list all processes), health (subsystem health check), poll (check status), kill (terminate), "
        "wait (block until done), log (get output buffer), signal (send arbitrary signal), "
        "write (write to stdin), submit (write + newline), close (close stdin/EOF), "
        "cleanup (remove finished processes). "
        "Supports env overrides, per-process timeout, and signal by name/number.",
        "{"
        "\"type\":\"object\","
        "\"properties\":{"
          "\"action\":{\"type\":\"string\",\"description\":\"start | list | health | cleanup | poll | kill | wait | log | signal | write | submit | close\"},"
          "\"command\":{\"type\":\"string\",\"description\":\"Command to run (required for start)\"},"
          "\"session_id\":{\"type\":\"number\",\"description\":\"Process session ID (required for poll/kill/wait/log/signal/write/close)\"},"
          "\"env\":{\"type\":\"string\",\"description\":\"Environment overrides as 'KEY=VALUE KEY2=VALUE2' (for start)\"},"
          "\"timeout\":{\"type\":\"number\",\"description\":\"Auto-kill timeout in seconds (for start/wait)\"},"
          "\"signal\":{\"type\":\"string\",\"description\":\"Signal name/number: SIGTERM, SIGKILL, SIGINT, 9, etc. (for signal action)\"},"
          "\"data\":{\"type\":\"string\",\"description\":\"Data to write to stdin (for write action)\"},"
          "\"max_age_sec\":{\"type\":\"number\",\"description\":\"Max age in seconds for cleanup (default 1800)\"}"
        "},"
        "\"required\":[\"action\"]"
        "}", process_handler);
}
