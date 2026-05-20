/*
 * process.c — Background process management tool for Hermes C.
 * Manages child processes: start, poll, log, kill, wait.
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
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (g_procs[i].pid > 0 && g_procs[i].running) {
            int status;
            pid_t result = waitpid(g_procs[i].pid, &status, WNOHANG);
            if (result == g_procs[i].pid) {
                g_procs[i].running = false;
                g_procs[i].exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
                if (g_procs[i].stdin_fd >= 0) close(g_procs[i].stdin_fd);
                g_procs[i].stdin_fd = -1;
            }
        }
    }
}

static void proc_start(const char *command, json_node_t *result) {
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

    json_object_set(result, "session_id", json_new_number((double)g_procs[slot].session_id));
    json_object_set(result, "pid", json_new_number((double)pid));
    json_object_set(result, "status", json_new_string("started"));
}

char *process_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *action = json_object_get_string(args, "action", "start");
    json_node_t *result = json_new_object();

    if (strcmp(action, "start") == 0) {
        const char *command = json_object_get_string(args, "command", NULL);
        if (!command)
            json_object_set(result, "error", json_new_string("Missing command"));
        else
            proc_start(command, result);
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
            } else {
                json_object_set(result, "error", json_new_string("Unknown action"));
            }
        }
    }

    char *json_out = json_serialize(result);
    json_free(result);
    json_free(args);
    return json_out;
}

void registry_init_process(void) {
    registry_register("process",
        "Manage background processes. Actions: start (run command in bg), poll (check status), "
        "kill (terminate). Returns session_id for subsequent operations.",
        "{"
        "\"type\":\"object\","
        "\"properties\":{"
          "\"action\":{\"type\":\"string\",\"description\":\"start | poll | kill\"},"
          "\"command\":{\"type\":\"string\",\"description\":\"Command to run (required for start)\"},"
          "\"session_id\":{\"type\":\"number\",\"description\":\"Process session ID (required for poll/kill)\"}"
        "},"
        "\"required\":[\"action\"]"
        "}", process_handler);
}
