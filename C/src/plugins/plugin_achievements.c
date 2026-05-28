/*
 * plugin_achievements.c — Achievement tracking plugin (PLUGIN_ACHIEVEMENTS).
 *
 * Tracks session metrics and evaluates tiered achievements.
 * Uses JSON state file for persistence (~/.hermes/plugins/achievements/state.json).
 *
 * Build:
 *   gcc -O2 -fPIC -shared -I ../../include -I ../../lib/libplugin \
 *       plugin_achievements.c -o plugin_achievements.so
 */

#include "plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

/* ================================================================
 *  Achievement definitions
 * ================================================================ */

#define MAX_ACHIEVEMENTS 64
#define MAX_TIERS 5

typedef struct {
    const char *id;
    const char *name;
    const char *desc;
    const char *category;
    const char *icon;
    int is_secret;
    int tier_count;
    int tiers[MAX_TIERS];       /* thresholds */
    const char *tier_names[MAX_TIERS];
} achievement_def_t;

static const char *TIER_NAMES[] = {"Copper", "Silver", "Gold", "Diamond", "Olympian"};
static const int TIER_THRESHOLDS[] = {200, 500, 1200, 3000, 8000};

static achievement_def_t ACHIEVEMENTS[] = {
    {"let_him_cook", "Let Him Cook", "Let Hermes run a serious autonomous tool chain.",
     "Agent Autonomy", "flame", 0, 5, {200, 500, 1200, 3000, 8000}, {"Copper","Silver","Gold","Diamond","Olympian"}},
    {"terminal_goblin", "Terminal Goblin", "Spend serious time in shell-land.",
     "Tool Mastery", "terminal", 0, 5, {750, 2000, 6000, 15000, 50000}, {"Copper","Silver","Gold","Diamond","Olympian"}},
    {"autonomous_avalanche", "Autonomous Avalanche", "Accumulate a lifetime avalanche of tool calls.",
     "Agent Autonomy", "avalanche", 0, 5, {1000, 3000, 8000, 20000, 50000}, {"Copper","Silver","Gold","Diamond","Olympian"}},
    {"patch_wizard", "Patch Wizard", "Bend files to your will with targeted patches.",
     "Tool Mastery", "wand", 0, 5, {250, 750, 2000, 6000, 15000}, {"Copper","Silver","Gold","Diamond","Olympian"}},
    {"red_text_connoisseur", "Red Text Connoisseur", "Encounter enough errors to develop a palate.",
     "Debugging Chaos", "warning", 0, 5, {1500, 4000, 10000, 25000, 75000}, {"Copper","Silver","Gold","Diamond","Olympian"}},
    {"marathon_operator", "Marathon Operator", "Accumulate a serious number of sessions.",
     "Lifestyle", "marathon", 0, 5, {75, 200, 500, 1500, 5000}, {"Copper","Silver","Gold","Diamond","Olympian"}},
    {"subagent_commander", "Subagent Commander", "Coordinate delegated agent work.",
     "Agent Autonomy", "branch", 0, 5, {5, 40, 100, 1000, 5000}, {"Copper","Silver","Gold","Diamond","Olympian"}},
    {"model_hopper", "Model Hopper", "Switch or inspect providers/models enough.",
     "Model Lore", "swap", 0, 5, {10000, 30000, 80000, 200000, 500000}, {"Copper","Silver","Gold","Diamond","Olympian"}},
    {"port_3000_taken", "Port 3000 Is Taken", "Discover dev-server port conflict patterns.",
     "Debugging Chaos", "plug", 1, 5, {15, 40, 100, 300, 1000}, {"Copper","Silver","Gold","Diamond","Olympian"}},
    {"skillsmith", "Skillsmith", "Work with Hermes skills enough to leave fingerprints.",
     "Hermes Native", "hammer_scroll", 0, 5, {5000, 15000, 40000, 100000, 250000}, {"Copper","Silver","Gold","Diamond","Olympian"}},
};

static int g_achievement_count = sizeof(ACHIEVEMENTS) / sizeof(ACHIEVEMENTS[0]);

/* ================================================================
 *  State persistence
 * ================================================================ */

/* In-memory aggregate metrics */
typedef struct {
    long total_tool_calls;
    long total_terminal_calls;
    long total_patch_calls;
    long total_errors;
    long total_delegate_calls;
    long session_count;
    long skill_events;
    long model_events;
    long port_conflict_events;
    long max_tool_calls_in_session;
    /* unlocked bitmask: bit i = achievement i is unlocked at some tier */
    unsigned long unlocked_mask;
    /* tier reached for each unlocked achievement */
    int achieved_tiers[MAX_ACHIEVEMENTS];
} achievement_state_t;

static achievement_state_t g_state;
static char g_state_path[4096];

static void ensure_dir(const char *path) {
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s", path);
    char *p = tmp;
    while (*p) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0700);
            *p = '/';
        }
        p++;
    }
}

static void state_save(void) {
    ensure_dir(g_state_path);
    FILE *f = fopen(g_state_path, "w");
    if (!f) return;
    fprintf(f, "{\"total_tool_calls\":%ld,\"total_terminal_calls\":%ld,"
               "\"total_patch_calls\":%ld,\"total_errors\":%ld,"
               "\"total_delegate_calls\":%ld,\"session_count\":%ld,"
               "\"skill_events\":%ld,\"model_events\":%ld,"
               "\"port_conflict_events\":%ld,\"max_tool_calls_in_session\":%ld,"
               "\"unlocked_mask\":%lu",
        g_state.total_tool_calls, g_state.total_terminal_calls,
        g_state.total_patch_calls, g_state.total_errors,
        g_state.total_delegate_calls, g_state.session_count,
        g_state.skill_events, g_state.model_events,
        g_state.port_conflict_events, g_state.max_tool_calls_in_session,
        g_state.unlocked_mask);
    fprintf(f, ",\"achieved_tiers\":[");
    for (int i = 0; i < g_achievement_count; i++) {
        if (i > 0) fputc(',', f);
        fprintf(f, "%d", g_state.achieved_tiers[i]);
    }
    fprintf(f, "]}");
    fclose(f);
}

static void state_load(void) {
    memset(&g_state, 0, sizeof(g_state));
    FILE *f = fopen(g_state_path, "r");
    if (!f) return;

    /* Quick JSON parse — read whole file and sscanf fields */
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz <= 0) { fclose(f); return; }

    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return; }
    size_t nread = fread(buf, 1, (size_t)sz, f);
    buf[nread] = '\0';
    fclose(f);

    /* Parse simple fields */
    sscanf(buf, "{\"total_tool_calls\":%ld,\"total_terminal_calls\":%ld,"
                "\"total_patch_calls\":%ld,\"total_errors\":%ld,"
                "\"total_delegate_calls\":%ld,\"session_count\":%ld,"
                "\"skill_events\":%ld,\"model_events\":%ld,"
                "\"port_conflict_events\":%ld,\"max_tool_calls_in_session\":%ld,"
                "\"unlocked_mask\":%lu",
        &g_state.total_tool_calls, &g_state.total_terminal_calls,
        &g_state.total_patch_calls, &g_state.total_errors,
        &g_state.total_delegate_calls, &g_state.session_count,
        &g_state.skill_events, &g_state.model_events,
        &g_state.port_conflict_events, &g_state.max_tool_calls_in_session,
        &g_state.unlocked_mask);

    /* Parse achieved_tiers array */
    const char *arr = strstr(buf, "\"achieved_tiers\":[");
    if (arr) {
        arr += 18; /* skip key */
        for (int i = 0; i < g_achievement_count && *arr && *arr != ']'; i++) {
            if (*arr == ',') arr++;
            g_state.achieved_tiers[i] = (int)strtol(arr, (char**)&arr, 10);
        }
    }
    free(buf);
}

/* ================================================================
 *  Achievement evaluation
 * ================================================================ */

/* Get value of a metric by name */
static long get_metric(const char *name) {
    if (strcmp(name, "total_tool_calls") == 0) return g_state.total_tool_calls;
    if (strcmp(name, "total_terminal_calls") == 0) return g_state.total_terminal_calls;
    if (strcmp(name, "total_patch_calls") == 0) return g_state.total_patch_calls;
    if (strcmp(name, "total_errors") == 0) return g_state.total_errors;
    if (strcmp(name, "total_delegate_calls") == 0) return g_state.total_delegate_calls;
    if (strcmp(name, "session_count") == 0) return g_state.session_count;
    if (strcmp(name, "skill_events") == 0) return g_state.skill_events;
    if (strcmp(name, "model_events") == 0) return g_state.model_events;
    if (strcmp(name, "port_conflict_events") == 0) return g_state.port_conflict_events;
    if (strcmp(name, "max_tool_calls_in_session") == 0) return g_state.max_tool_calls_in_session;
    return 0;
}

/* Evaluate all achievements and update unlocks */
static void evaluate_all(void) {
    unsigned long old_mask = g_state.unlocked_mask;
    g_state.unlocked_mask = 0;

    for (int i = 0; i < g_achievement_count; i++) {
        /* Map achievement to its primary metric */
        const char *metric = NULL;
        if (strcmp(ACHIEVEMENTS[i].id, "let_him_cook") == 0) metric = "max_tool_calls_in_session";
        else if (strcmp(ACHIEVEMENTS[i].id, "terminal_goblin") == 0) metric = "total_terminal_calls";
        else if (strcmp(ACHIEVEMENTS[i].id, "autonomous_avalanche") == 0) metric = "total_tool_calls";
        else if (strcmp(ACHIEVEMENTS[i].id, "patch_wizard") == 0) metric = "total_patch_calls";
        else if (strcmp(ACHIEVEMENTS[i].id, "red_text_connoisseur") == 0) metric = "total_errors";
        else if (strcmp(ACHIEVEMENTS[i].id, "marathon_operator") == 0) metric = "session_count";
        else if (strcmp(ACHIEVEMENTS[i].id, "subagent_commander") == 0) metric = "total_delegate_calls";
        else if (strcmp(ACHIEVEMENTS[i].id, "model_hopper") == 0) metric = "model_events";
        else if (strcmp(ACHIEVEMENTS[i].id, "port_3000_taken") == 0) metric = "port_conflict_events";
        else if (strcmp(ACHIEVEMENTS[i].id, "skillsmith") == 0) metric = "skill_events";
        else continue;

        long value = get_metric(metric);
        int tier = -1;
        for (int t = 0; t < ACHIEVEMENTS[i].tier_count; t++) {
            if (value >= ACHIEVEMENTS[i].tiers[t]) tier = t;
        }
        if (tier >= 0) {
            g_state.unlocked_mask |= (1UL << i);
            g_state.achieved_tiers[i] = tier;
        }
    }

    /* Save if changed */
    if (g_state.unlocked_mask != old_mask) state_save();
}

/* ================================================================
 *  Tool interface functions
 * ================================================================ */

/* Build metrics JSON — returns malloc'd string */
static char *metrics_to_json(void) {
    char buf[4096];
    int pos = snprintf(buf, sizeof(buf),
        "{\"total_tool_calls\":%ld,\"total_terminal_calls\":%ld,"
        "\"total_patch_calls\":%ld,\"total_errors\":%ld,"
        "\"total_delegate_calls\":%ld,\"session_count\":%ld,"
        "\"skill_events\":%ld,\"model_events\":%ld,"
        "\"port_conflict_events\":%ld,\"max_tool_calls_in_session\":%ld}",
        g_state.total_tool_calls, g_state.total_terminal_calls,
        g_state.total_patch_calls, g_state.total_errors,
        g_state.total_delegate_calls, g_state.session_count,
        g_state.skill_events, g_state.model_events,
        g_state.port_conflict_events, g_state.max_tool_calls_in_session);

    /* Build achievements array inline */
    pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, ",\"achievements\":[");
    for (int i = 0; i < g_achievement_count; i++) {
        if (i > 0) {
            int r = snprintf(buf + pos, sizeof(buf) - (size_t)pos, ",");
            if (r < 0 || (size_t)r >= sizeof(buf) - (size_t)pos) break;
            pos += r;
        }
        int unlocked = (g_state.unlocked_mask >> i) & 1;
        int r = snprintf(buf + pos, sizeof(buf) - (size_t)pos,
            "{\"id\":\"%s\",\"name\":\"%s\",\"category\":\"%s\",\"icon\":\"%s\","
            "\"unlocked\":%d,\"tier\":%d}",
            ACHIEVEMENTS[i].id, ACHIEVEMENTS[i].name,
            ACHIEVEMENTS[i].category, ACHIEVEMENTS[i].icon,
            unlocked, unlocked ? g_state.achieved_tiers[i] : -1);
        if (r < 0 || (size_t)r >= sizeof(buf) - (size_t)pos) break;
        pos += r;
    }
    snprintf(buf + pos, sizeof(buf) - (size_t)pos, "]}");
    return strdup(buf);
}

/* Tool 0: get_metrics — return all metrics + achievement status */
static char *tool_get_metrics(const char *args_json) {
    (void)args_json;
    evaluate_all();
    return metrics_to_json();
}

/* Tool 1: update_metrics — update a metric value */
static char *tool_update_metrics(const char *args_json) {
    if (!args_json) return strdup("{\"error\":\"no args\"}");

    long val;
    if (sscanf(args_json, "{\"terminal_calls\":%ld", &val) == 1) {
        g_state.total_terminal_calls += val;
    }
    if (sscanf(args_json, "{\"patch_calls\":%ld", &val) == 1) {
        g_state.total_patch_calls += val;
    }
    if (strstr(args_json, "\"tool_call\"")) g_state.total_tool_calls++;
    if (strstr(args_json, "\"error\"")) g_state.total_errors++;
    if (strstr(args_json, "\"delegate\"")) g_state.total_delegate_calls++;
    if (strstr(args_json, "\"skill\"")) g_state.skill_events++;
    if (strstr(args_json, "\"model\"")) g_state.model_events++;
    if (strstr(args_json, "\"port_conflict\"")) g_state.port_conflict_events++;

    /* Track max tool calls in session */
    if (strstr(args_json, "\"tool_call\"")) {
        /* Simple: count tool_call occurrences in this batch */
        long count = 0;
        const char *p = args_json;
        while ((p = strstr(p, "\"tool_call\"")) != NULL) {
            count++;
            p++;
        }
        if (count > g_state.max_tool_calls_in_session)
            g_state.max_tool_calls_in_session = count;
    }

    state_save();
    evaluate_all();
    return strdup("{\"status\":\"updated\"}");
}

/* Tool 2: list_achievements — list all defined achievements */
static char *tool_list_achievements(const char *args_json) {
    (void)args_json;
    evaluate_all();
    char buf[8192];
    int pos = snprintf(buf, sizeof(buf), "{\"count\":%d,\"items\":[", g_achievement_count);
    for (int i = 0; i < g_achievement_count; i++) {
        if (i > 0) {
            int r = snprintf(buf + pos, sizeof(buf) - (size_t)pos, ",");
            if (r < 0 || (size_t)r >= sizeof(buf) - (size_t)pos) break;
            pos += r;
        }
        int unlocked = (g_state.unlocked_mask >> i) & 1;
        int tier = unlocked ? g_state.achieved_tiers[i] : -1;
        const char *tier_name = (tier >= 0 && tier < MAX_TIERS) ? TIER_NAMES[tier] : "None";
        const char *display_name = (ACHIEVEMENTS[i].is_secret && !unlocked)
                                   ? "???" : ACHIEVEMENTS[i].name;
        const char *display_desc = (ACHIEVEMENTS[i].is_secret && !unlocked)
                                   ? "???" : ACHIEVEMENTS[i].desc;

        int r = snprintf(buf + pos, sizeof(buf) - (size_t)pos,
            "{\"id\":\"%s\",\"name\":\"%s\",\"desc\":\"%s\",\"category\":\"%s\","
            "\"icon\":\"%s\",\"unlocked\":%d,\"tier_name\":\"%s\",\"tier_index\":%d}",
            ACHIEVEMENTS[i].id, display_name, display_desc,
            ACHIEVEMENTS[i].category, ACHIEVEMENTS[i].icon,
            unlocked, tier_name, tier);
        if (r < 0 || (size_t)r >= sizeof(buf) - (size_t)pos) break;
        pos += r;
    }
    snprintf(buf + pos, sizeof(buf) - (size_t)pos, "]}");
    return strdup(buf);
}

/* ================================================================
 *  Plugin interface table
 * ================================================================ */

static plugin_interface_t g_interface;

void *plugin_get_interface(void) {
    return &g_interface;
}

/* ================================================================
 *  Plugin metadata
 * ================================================================ */

const char *plugin_meta_name(void) {
    return "achievements";
}

const char *plugin_meta_version(void) {
    return "1.0.0";
}

const char *plugin_meta_type(void) {
    return "achievements";
}

const char *plugin_meta_description(void) {
    return "Achievement tracking — tiered achievements for session milestones, tool usage, and debugging feats";
}

/* ================================================================
 *  Dependencies
 * ================================================================ */

int plugin_deps_count(void) {
    return 0;
}

const plugin_dep_t *plugin_deps_list(void) {
    return NULL;
}

/* ================================================================
 *  Lifecycle
 * ================================================================ */

int plugin_init(void) {
    memset(&g_interface, 0, sizeof(g_interface));
    g_interface.type = PLUGIN_ACHIEVEMENTS;

    /* Set tool interface for achievements tools */
    g_interface.tool_get_count = NULL;   /* Using named dispatch */
    g_interface.tool_get_info = NULL;
    g_interface.tool_execute = NULL;

    /* Determine state file path */
    const char *home = getenv("SLERMES_HOME");
    if (!home) home = getenv("HOME");
    if (home) {
        snprintf(g_state_path, sizeof(g_state_path),
                 "%s/.hermes/plugins/achievements/state.json", home);
    } else {
        snprintf(g_state_path, sizeof(g_state_path),
                 "/tmp/.hermes/plugins/achievements/state.json");
    }

    state_load();
    evaluate_all();
    return 0;
}

int plugin_cleanup(void) {
    state_save();
    memset(&g_interface, 0, sizeof(g_interface));
    return 0;
}
