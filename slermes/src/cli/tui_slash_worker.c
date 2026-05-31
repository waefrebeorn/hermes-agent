/*
 * tui_slash_worker.c — TUI Slash Command Worker (T06)
 * MIT License — WuBu Slermes Project
 *
 * Dispatch-table-based slash command handler. Replaces the flat if/else
 * chain in tui_process_input with a registered command system.
 *
 * Python equivalent: tui_gateway/slash_worker.py
 */

#include "tui_slash_worker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Internals ──────────────────────────────────────────────── */

#define MAX_COMMANDS 64

static struct {
    slash_cmd_entry_t entries[MAX_COMMANDS];
    int               count;
    void             *tui_state;    /* agent_state_t* */
    bool              initialized;
} g_slash;

/* ── Category names ─────────────────────────────────────────── */

static const char *cat_name(slash_category_t cat) {
    switch (cat) {
        case SLASH_CAT_TUI:     return "TUI Display";
        case SLASH_CAT_AGENT:   return "Agent Runtime";
        case SLASH_CAT_SESSION: return "Session";
        case SLASH_CAT_MODAL:   return "Overlays";
        case SLASH_CAT_META:    return "Meta";
        case SLASH_CAT_SKILL:   return "Skills";
    }
    return "Unknown";
}

/* ── Argument parsing ───────────────────────────────────────── */

int tui_slash_parse(const char *line, int max_args, char **argv) {
    if (!line || !argv || max_args < 1) return 0;

    /* Skip leading whitespace */
    while (*line == ' ' || *line == '\t') line++;
    if (!*line) return 0;

    int argc = 0;
    const char *p = line;
    char buf[256];

    while (*p && argc < max_args) {
        /* Skip whitespace between args */
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;

        /* Extract argument (honor double-quotes) */
        int blen = 0;
        bool in_quote = false;
        while (*p && (in_quote || (*p != ' ' && *p != '\t'))) {
            if (*p == '"') {
                in_quote = !in_quote;
                p++;
                continue;
            }
            if (blen < (int)sizeof(buf) - 1)
                buf[blen++] = *p;
            p++;
        }
        buf[blen] = '\0';
        argv[argc] = strdup(buf);
        if (argv[argc])
            argc++;
    }

    return argc;
}

void tui_slash_free_args(int argc, char **argv) {
    if (!argv) return;
    for (int i = 0; i < argc; i++)
        free(argv[i]);
}

const char *tui_slash_category_name(slash_category_t cat) {
    return cat_name(cat);
}

/* ── Registration ───────────────────────────────────────────── */

bool tui_slash_init(void *tui_state) {
    memset(&g_slash, 0, sizeof(g_slash));
    g_slash.tui_state = tui_state;
    g_slash.initialized = true;
    return true;
}

void tui_slash_shutdown(void) {
    /* Free any strdup'd command names (they're static strings, no-op) */
    for (int i = 0; i < g_slash.count; i++) {
        /* cmd, desc, args are static const — do not free */
    }
    g_slash.count = 0;
    g_slash.initialized = false;
}

bool tui_slash_register(const slash_cmd_entry_t *entry) {
    if (!entry || !entry->cmd || !entry->handler) return false;
    if (g_slash.count >= MAX_COMMANDS) return false;

    /* Check for duplicates */
    for (int i = 0; i < g_slash.count; i++) {
        if (strcmp(g_slash.entries[i].cmd, entry->cmd) == 0)
            return false; /* duplicate */
    }

    g_slash.entries[g_slash.count] = *entry;
    g_slash.count++;
    return true;
}

int tui_slash_register_batch(const slash_cmd_entry_t entries[]) {
    int registered = 0;
    for (int i = 0; entries[i].cmd != NULL; i++) {
        if (tui_slash_register(&entries[i]))
            registered++;
    }
    return registered;
}

void tui_slash_unregister(const char *cmd) {
    if (!cmd) return;
    for (int i = 0; i < g_slash.count; i++) {
        if (strcmp(g_slash.entries[i].cmd, cmd) == 0) {
            /* Shift remaining entries */
            int remaining = g_slash.count - i - 1;
            if (remaining > 0)
                memmove(&g_slash.entries[i], &g_slash.entries[i + 1],
                        remaining * sizeof(slash_cmd_entry_t));
            g_slash.count--;
            return;
        }
    }
}

/* ── Lookup and dispatch ───────────────────────────────────── */

const slash_cmd_entry_t *tui_slash_find(const char *cmd_name) {
    if (!cmd_name) return NULL;

    /* Strip leading '/' if present */
    const char *name = cmd_name;
    if (name[0] == '/') name++;

    for (int i = 0; i < g_slash.count; i++) {
        const char *ecmd = g_slash.entries[i].cmd;
        if (ecmd[0] == '/') ecmd++;
        if (strcmp(ecmd, name) == 0)
            return &g_slash.entries[i];
    }
    return NULL;
}

bool tui_slash_dispatch(const char *line) {
    if (!line || !*line) return false;
    if (line[0] != '/') return false;

    /* Parse arguments */
    char *argv[32];
    int argc = tui_slash_parse(line, 32, argv);
    if (argc < 1) return false;

    /* Find command (strip leading '/') */
    const char *cmd_name = argv[0];
    if (cmd_name[0] == '/') cmd_name++;

    const slash_cmd_entry_t *entry = NULL;
    for (int i = 0; i < g_slash.count; i++) {
        const char *ecmd = g_slash.entries[i].cmd;
        if (ecmd[0] == '/') ecmd++;
        if (strcmp(ecmd, cmd_name) == 0) {
            entry = &g_slash.entries[i];
            break;
        }
    }

    bool handled = false;
    if (entry) {
        handled = entry->handler(line, argc, argv, g_slash.tui_state);
    }

    tui_slash_free_args(argc, argv);
    return handled;
}

/* ── Query ──────────────────────────────────────────────────── */

const slash_cmd_entry_t **tui_slash_get_all(void) {
    /* Return a static array. Rebuild on each call. */
    static const slash_cmd_entry_t *result[MAX_COMMANDS + 1];
    for (int i = 0; i < g_slash.count; i++)
        result[i] = &g_slash.entries[i];
    result[g_slash.count] = NULL;
    return result;
}

int tui_slash_count(void) {
    return g_slash.count;
}

char *tui_slash_help_for_category(slash_category_t cat) {
    /* Count matching commands */
    int count = 0;
    for (int i = 0; i < g_slash.count; i++) {
        if (g_slash.entries[i].category == cat)
            count++;
    }

    /* Estimate buffer size */
    size_t bufsize = 256 + (size_t)count * 128;
    char *buf = (char *)malloc(bufsize);
    if (!buf) return NULL;

    int off = snprintf(buf, bufsize, "=== %s Commands ===\n", cat_name(cat));
    for (int i = 0; i < g_slash.count; i++) {
        if (g_slash.entries[i].category != cat) continue;
        const slash_cmd_entry_t *e = &g_slash.entries[i];
        int n = snprintf(buf + off, bufsize - off > 0 ? bufsize - off : 0,
                         "  %-16s %-32s %s\n",
                         e->cmd, e->desc,
                         e->args[0] ? e->args : "");
        if (n > 0) off += n;
    }

    return buf;
}
