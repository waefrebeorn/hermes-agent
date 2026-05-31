/*
 * upstream_sync.c — Standalone C upstream drift detector for Slermes.
 *
 * Replaces the Python digest.py --upstream cron job entirely.
 * Zero token cost, no LLM agent needed (no_agent=true).
 *
 * Compile:
 *   gcc -O2 -Wall -Wextra upstream_sync.c -o upstream_sync -lm
 *
 * Usage:
 *   ./upstream_sync              # Check upstream drift, print JSON report
 *   ./upstream_sync --report     # Print human-readable report
 *   ./upstream_sync --save       # Save report to digest_output/
 *
 * Returns JSON with:
 *   { "total_changed": N, "needs_update": [...], "not_started": [...] }
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

/* ================================================================
 *  Helpers
 * ================================================================ */

static char *run_cmd(const char *cmd) {
    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;
    size_t cap = 8192, len = 0;
    char *out = malloc(cap);
    if (!out) { pclose(fp); return NULL; }
    out[0] = '\0';
    char buf[4096];
    while (fgets(buf, sizeof(buf), fp)) {
        size_t add = strlen(buf);
        if (len + add + 1 > cap) {
            cap *= 2;
            char *tmp = realloc(out, cap);
            if (!tmp) { free(out); pclose(fp); return NULL; }
            out = tmp;
        }
        memcpy(out + len, buf, add + 1);
        len += add;
    }
    pclose(fp);
    return out;
}

static int count_lines(const char *s) {
    if (!s) return 0;
    int n = 0;
    for (const char *p = s; *p; p++)
        if (*p == '\n') n++;
    return n;
}

static char *escape_json(const char *s) {
    if (!s) return strdup("");
    size_t cap = strlen(s) * 2 + 2;
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t j = 0;
    for (size_t i = 0; s[i]; i++) {
        switch (s[i]) {
            case '"':  out[j++] = '\\'; out[j++] = '"';  break;
            case '\\': out[j++] = '\\'; out[j++] = '\\'; break;
            case '\n': out[j++] = '\\'; out[j++] = 'n';  break;
            case '\r': out[j++] = '\\'; out[j++] = 'r';  break;
            case '\t': out[j++] = '\\'; out[j++] = 't';  break;
            default:   out[j++] = s[i];
        }
        if (j >= cap - 4) {
            cap *= 2;
            char *tmp = realloc(out, cap);
            if (!tmp) { free(out); return NULL; }
            out = tmp;
        }
    }
    out[j] = '\0';
    return out;
}

/* ================================================================
 *  Python → C file mapping
 * ================================================================ */

typedef struct {
    const char *py_path_prefix;
    const char *c_path;
} file_map_t;

static const file_map_t FILE_MAP[] = {
    {"agent/anthropic_adapter",      "agent/provider_anthropic"},
    {"agent/bedrock_adapter",        "agent/provider_bedrock"},
    {"agent/provider_openai",        "agent/provider_openai"},
    {"agent/provider_openrouter",    "agent/provider_openrouter"},
    {"agent/provider_deepseek",      "agent/provider_deepseek"},
    {"agent/provider_xai",           "agent/provider_xai"},
    {"agent/provider_custom",        "agent/provider_custom"},
    {"agent/provider_azure",         "agent/provider_azure"},
    {"agent/provider_google",        "agent/provider_google"},
    {"agent/conversation_loop",      "agent/agent_loop"},
    {"agent/chat_completion_helpers","agent/llm_client"},
    {"agent/agent_init",             "agent/context"},
    {"agent/agent_runtime_helpers",  "agent/agent_message_repair"},
    {"agent/credential_pool",        "agent/credential_pool"},
    {"agent/context_compressor",     "agent/context_engine"},
    {"agent/prompt_builder",         "agent/system_prompt"},
    {"agent/model_metadata",         "agent/provider_metadata"},
    {"agent/budget_tracker",         "agent/budget_tracker"},
    {"agent/tool_executor",          "tools/tool_init"},
    {"tools/registry",               "tools/registry"},
    {"tools/terminal_tool",          "tools/terminal"},
    {"tools/file_tools",             "tools/file"},
    {"tools/web_tools",              "tools/web"},
    {"tools/browser_tool",           "tools/browser"},
    {"tools/send_message_tool",      "tools/send_message"},
    {"tools/session_search",         "tools/session_search"},
    {"tools/mcp_tool",               "tools/mcp_tool"},
    {"tools/vision_tools",           "tools/vision"},
    {"tools/image_gen",              "tools/image_gen"},
    {"tools/voice_mode_tool",        "tools/voice_mode"},
    {"tools/tts_tool",               "tools/tts"},
    {"tools/transcribe_tool",        "tools/transcribe"},
    {"tools/cronjob_tool",           "tools/cronjob"},
    {"tools/delegate_tool",          "tools/delegate"},
    {"tools/clarify_tool",           "tools/clarify"},
    {"tools/memory_tool",            "tools/memory"},
    {"tools/approval_tool",          "tools/approval"},
    {"tools/process_tool",           "tools/process"},
    {"tools/exec_code_tool",         "tools/exec_code"},
    {"tools/todo_tool",              "tools/todo"},
    {"tools/x_search_tool",          "tools/x_search"},
    {"tools/tirith_tool",            "tools/tirith"},
    {"tools/patch_tool",             "tools/patch"},
    {"tools/kanban_tool",            "tools/kanban"},
    {"tools/skills_tool",            "tools/skills"},
    {"cli.py",                       "cli/cli"},
    {"config.py",                    "cli/config"},
    {"commands.py",                  "cli/commands"},
    {"gateway/server.py",            "gateway/server"},
    {"gateway/telegram",             "gateway/platforms/telegram"},
    {"gateway/discord",              "gateway/platforms/discord"},
    {"gateway/platforms/webhook",    "gateway/platforms/webhook"},
    {"gateway/platforms/slack",      "gateway/platforms/slack"},
    {"gateway/platforms/matrix",     "gateway/platforms/matrix"},
    {"gateway/platforms/email",      "gateway/platforms/email"},
    {"gateway/platforms/signal",     "gateway/platforms/signal"},
    {"gateway/platforms/feishu",     "gateway/platforms/feishu"},
    {"gateway/platforms/wecom",      "gateway/platforms/wecom"},
    {"cron/jobs.py",                 "cron/jobs"},
    {"cron/scheduler.py",            "cron/scheduler"},
    {"acp_adapter/server.py",        "acp/server"},
    {NULL, NULL}
};

static const char *map_py_to_c(const char *py_path) {
    for (int i = 0; FILE_MAP[i].py_path_prefix; i++) {
        if (strstr(py_path, FILE_MAP[i].py_path_prefix))
            return FILE_MAP[i].c_path;
    }
    return NULL;
}

/* ================================================================
 *  Main
 * ================================================================ */

int main(int argc, char **argv) {
    int save_mode = 0, report_mode = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--save") == 0) save_mode = 1;
        if (strcmp(argv[i], "--report") == 0) report_mode = 1;
    }

    /* 1. Fetch upstream */
    printf("[upstream_sync] Fetching upstream/main (NousResearch)...\n");
    char *fetch = run_cmd("cd /home/wubu/hermes-agent-dev/slermes && git fetch upstream main 2>&1");
    if (!fetch) { fprintf(stderr, "ERROR: git fetch failed\n"); return 1; }
    printf("  %s", fetch);
    free(fetch);

    /* 2. Get current HEAD */
    char *head = run_cmd("cd /home/wubu/hermes-agent-dev/slermes && git rev-parse --short HEAD 2>/dev/null");
    if (!head) { fprintf(stderr, "ERROR: git rev-parse failed\n"); return 1; }
    if (head[0]) head[strcspn(head, "\n")] = '\0';

    /* 3. Get changed Python files vs upstream main */
    char *changed = run_cmd(
        "cd /home/wubu/hermes-agent-dev/slermes && "
        "git diff --name-only HEAD..upstream/main -- '*.py' 2>/dev/null");
    if (!changed) { fprintf(stderr, "ERROR: git diff failed\n"); return 1; }

    int total_changed = count_lines(changed);
    printf("[upstream_sync] %d Python files changed since last sync\n", total_changed);

    /* 4. Map each changed file to C work items */
    /* Count first for allocation */
    int needs_update_count = 0;
    int not_started_count = 0;
    char **needs_update_list = NULL;
    char **not_started_list = NULL;
    size_t nu_cap = 0, ns_cap = 0;

    char *line = changed;
    char *next;
    while (line && *line) {
        next = strchr(line, '\n');
        if (next) *next = '\0';

        /* Strip leading paths like "agent/..." */
        const char *py = line;
        while (*py == ' ' || *py == '\t') py++;
        if (!*py) { line = next ? next + 1 : NULL; continue; }

        const char *c_path = map_py_to_c(py);
        if (c_path) {
            /* Needs update */
            if (needs_update_count >= (int)nu_cap) {
                nu_cap = nu_cap ? nu_cap * 2 : 64;
                needs_update_list = realloc(needs_update_list, nu_cap * sizeof(char*));
            }
            char buf[512];
            snprintf(buf, sizeof(buf), "src/%s.c", c_path);
            needs_update_list[needs_update_count++] = strdup(buf);
        } else {
            /* Not started — only flag files under agent/, tools/, gateway/, cli/ */
            if (strstr(py, "agent/") || strstr(py, "tools/") ||
                strstr(py, "gateway/") || strstr(py, "cli/") ||
                strstr(py, "cron/") || strstr(py, "acp_")) {
                if (not_started_count >= (int)ns_cap) {
                    ns_cap = ns_cap ? ns_cap * 2 : 64;
                    not_started_list = realloc(not_started_list, ns_cap * sizeof(char*));
                }
                not_started_list[not_started_count++] = strdup(py);
            }
        }
        line = next ? next + 1 : NULL;
    }
    free(changed);

    /* 5. Build JSON report */
    char *json_head = escape_json(head);
    char result[65536];
    int pos = 0;
    pos += snprintf(result + pos, sizeof(result) - pos,
        "{\n");
    pos += snprintf(result + pos, sizeof(result) - pos,
        "  \"timestamp\": %ld,\n", (long)time(NULL));
    pos += snprintf(result + pos, sizeof(result) - pos,
        "  \"head\": \"%s\",\n", json_head ? json_head : "?");
    pos += snprintf(result + pos, sizeof(result) - pos,
        "  \"total_changed\": %d,\n", total_changed);
    pos += snprintf(result + pos, sizeof(result) - pos,
        "  \"needs_update_count\": %d,\n", needs_update_count);
    pos += snprintf(result + pos, sizeof(result) - pos,
        "  \"not_started_count\": %d,\n", not_started_count);
    pos += snprintf(result + pos, sizeof(result) - pos,
        "  \"needs_update\": [\n");
    for (int i = 0; i < needs_update_count; i++) {
        char *esc = escape_json(needs_update_list[i]);
        pos += snprintf(result + pos, sizeof(result) - pos,
            "    \"%s\"%s\n", esc ? esc : "?", i < needs_update_count - 1 ? "," : "");
        free(esc);
        free(needs_update_list[i]);
    }
    pos += snprintf(result + pos, sizeof(result) - pos,
        "  ],\n  \"not_started\": [\n");
    for (int i = 0; i < not_started_count; i++) {
        char *esc = escape_json(not_started_list[i]);
        pos += snprintf(result + pos, sizeof(result) - pos,
            "    \"%s\"%s\n", esc ? esc : "?", i < not_started_count - 1 ? "," : "");
        free(esc);
        free(not_started_list[i]);
    }
    pos += snprintf(result + pos, sizeof(result) - pos,
        "  ]\n}");
    free(needs_update_list);
    free(not_started_list);
    free(json_head);

    /* 6. Output */
    if (report_mode) {
        printf("\n=== UPSTREAM SYNC REPORT ===\n");
        printf("HEAD: %s\n", head);
        printf("Total changed Python files: %d\n", total_changed);
        printf("C files needing update: %d\n", needs_update_count);
        printf("New C files needed: %d\n", not_started_count);
        printf("\nTo see details, run without --report\n");
    } else if (save_mode) {
        mkdir("digest_output", 0755);
        FILE *f = fopen("digest_output/upstream_report.json", "w");
        if (f) {
            fputs(result, f);
            fclose(f);
            printf("[upstream_sync] Report saved to digest_output/upstream_report.json\n");
        } else {
            printf("[upstream_sync] ERROR: could not write report\n");
        }
    } else {
        printf("%s\n", result);
    }

    free(head);
    return 0;
}
