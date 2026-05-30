/*
 * rate_limit.c — P165: Rate limiting for Hermes C.
 *
 * Per-tool calls/minute and per-provider RPM tracking.
 * Integrates with tool dispatch in agent_loop.c.
 */

#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ================================================================
 *  Rate Limiter State
 * ================================================================ */

#define MAX_RATE_SLOTS 128
#define WINDOW_SECONDS 60

typedef struct {
    char name[64];
    int  max_calls;
    int  call_count;
    time_t window_start;
    bool  is_provider; /* false = per-tool, true = per-provider */
} rate_slot_t;

static rate_slot_t g_slots[MAX_RATE_SLOTS];
static int g_slot_count = 0;

/* ================================================================
 *  API
 * ================================================================ */

/* Initialize a rate limit slot for a tool */
bool rate_limit_init_tool(const char *tool_name, int max_per_minute) {
    if (!tool_name || g_slot_count >= MAX_RATE_SLOTS) return false;

    /* Check for existing slot */
    for (int i = 0; i < g_slot_count; i++) {
        if (!g_slots[i].is_provider && strcmp(g_slots[i].name, tool_name) == 0) {
            g_slots[i].max_calls = max_per_minute;
            return true;
        }
    }

    rate_slot_t *slot = &g_slots[g_slot_count++];
    snprintf(slot->name, sizeof(slot->name), "%s", tool_name);
    slot->max_calls = max_per_minute;
    slot->call_count = 0;
    slot->window_start = time(NULL);
    slot->is_provider = false;
    return true;
}

/* Initialize a rate limit slot for a provider */
bool rate_limit_init_provider(const char *provider_name, int max_per_minute) {
    if (!provider_name || g_slot_count >= MAX_RATE_SLOTS) return false;

    for (int i = 0; i < g_slot_count; i++) {
        if (g_slots[i].is_provider && strcmp(g_slots[i].name, provider_name) == 0) {
            g_slots[i].max_calls = max_per_minute;
            return true;
        }
    }

    rate_slot_t *slot = &g_slots[g_slot_count++];
    snprintf(slot->name, sizeof(slot->name), "%s", provider_name);
    slot->max_calls = max_per_minute;
    slot->call_count = 0;
    slot->window_start = time(NULL);
    slot->is_provider = true;
    return true;
}

/* Check if a tool is rate-limited. Returns true if allowed, false if rate-limited. */
bool rate_limit_check_tool(const char *tool_name) {
    if (!tool_name) return true;

    time_t now = time(NULL);

    for (int i = 0; i < g_slot_count; i++) {
        if (g_slots[i].is_provider) continue;
        if (strcmp(g_slots[i].name, tool_name) != 0) continue;

        rate_slot_t *slot = &g_slots[i];

        /* Reset window if expired */
        if (difftime(now, slot->window_start) >= WINDOW_SECONDS) {
            slot->call_count = 0;
            slot->window_start = now;
        }

        /* Check limit */
        if (slot->max_calls > 0 && slot->call_count >= slot->max_calls) {
            return false; /* Rate limited */
        }

        /* Increment */
        slot->call_count++;
        return true;
    }

    /* No limit configured for this tool — allow */
    return true;
}

/* Check if a provider is rate-limited. Returns true if allowed, false if rate-limited. */
bool rate_limit_check_provider(const char *provider_name) {
    if (!provider_name) return true;

    time_t now = time(NULL);

    for (int i = 0; i < g_slot_count; i++) {
        if (!g_slots[i].is_provider) continue;
        if (strcmp(g_slots[i].name, provider_name) != 0) continue;

        rate_slot_t *slot = &g_slots[i];

        if (difftime(now, slot->window_start) >= WINDOW_SECONDS) {
            slot->call_count = 0;
            slot->window_start = now;
        }

        if (slot->max_calls > 0 && slot->call_count >= slot->max_calls) {
            return false;
        }

        slot->call_count++;
        return true;
    }

    return true;
}

/* Get remaining calls in current window for a tool */
int rate_limit_remaining_tool(const char *tool_name) {
    if (!tool_name) return -1;

    time_t now = time(NULL);

    for (int i = 0; i < g_slot_count; i++) {
        if (g_slots[i].is_provider) continue;
        if (strcmp(g_slots[i].name, tool_name) != 0) continue;

        rate_slot_t *slot = &g_slots[i];

        if (difftime(now, slot->window_start) >= WINDOW_SECONDS) {
            return slot->max_calls;
        }

        return slot->max_calls - slot->call_count;
    }

    return -1; /* No limit */
}

/* Reset all rate limit counters */
void rate_limit_reset_all(void) {
    time_t now = time(NULL);
    for (int i = 0; i < g_slot_count; i++) {
        g_slots[i].call_count = 0;
        g_slots[i].window_start = now;
    }
}

/* Clear all rate limit slots */
void rate_limit_clear(void) {
    g_slot_count = 0;
}
