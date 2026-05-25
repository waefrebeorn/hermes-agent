/* ================================================================
 * tool_guardrails.c — Per-turn tool call loop detection.
 * Port of Python agent/tool_guardrails.py (475 lines).
 *
 * MIT License — WuBu Slermes Project
 * ================================================================ */

#include "hermes_tool_guardrails.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ================================================================
 *  Default config
 * ================================================================ */

#define DEFAULT_WARN_AFTER_EXACT       2
#define DEFAULT_BLOCK_AFTER_EXACT      5
#define DEFAULT_WARN_AFTER_SAME_TOOL   3
#define DEFAULT_HALT_AFTER_SAME_TOOL   8
#define DEFAULT_WARN_AFTER_NO_PROGRESS 2
#define DEFAULT_BLOCK_AFTER_NO_PROGRESS 5

/* ================================================================
 *  Helper: fast string hash (djb2)
 * ================================================================ */

uint32_t tool_guardrail_hash(const char *str)
{
    if (!str) return 0;
    uint32_t hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + (unsigned char)c;
    return hash;
}

/* ================================================================
 *  Idempotent / mutating tool sets
 * ================================================================ */

static const char *IDEMPOTENT_TOOLS[] = {
    "read_file",
    "search_files",
    "web_search",
    "web_extract",
    "session_search",
    "browser_snapshot",
    "browser_console",
    "browser_get_images",
    "mcp_filesystem_read_file",
    "mcp_filesystem_list_directory",
    NULL
};

static const char *MUTATING_TOOLS[] = {
    "terminal",
    "execute_code",
    "write_file",
    "patch",
    "todo",
    "memory",
    "skill_manage",
    "browser_click",
    "browser_type",
    "browser_press",
    "browser_scroll",
    "browser_navigate",
    "send_message",
    "cronjob",
    "delegate_task",
    "process",
    NULL
};

static bool in_set(const char *tool_name, const char **set)
{
    if (!tool_name) return false;
    for (int i = 0; set[i]; i++) {
        if (strcmp(tool_name, set[i]) == 0)
            return true;
    }
    return false;
}

bool tool_guardrail_is_idempotent(const char *tool_name)
{
    return in_set(tool_name, IDEMPOTENT_TOOLS);
}

bool tool_guardrail_is_mutating(const char *tool_name)
{
    return in_set(tool_name, MUTATING_TOOLS);
}

/* ================================================================
 *  Initialize / reset
 * ================================================================ */

void tool_guardrail_init(tool_guardrail_controller_t *ctrl)
{
    if (!ctrl) return;
    memset(ctrl, 0, sizeof(*ctrl));
    ctrl->warnings_enabled = true;
    ctrl->hard_stop_enabled = false;
    ctrl->exact_failure_warn_after = DEFAULT_WARN_AFTER_EXACT;
    ctrl->exact_failure_block_after = DEFAULT_BLOCK_AFTER_EXACT;
    ctrl->same_tool_failure_warn_after = DEFAULT_WARN_AFTER_SAME_TOOL;
    ctrl->same_tool_failure_halt_after = DEFAULT_HALT_AFTER_SAME_TOOL;
    ctrl->no_progress_warn_after = DEFAULT_WARN_AFTER_NO_PROGRESS;
    ctrl->no_progress_block_after = DEFAULT_BLOCK_AFTER_NO_PROGRESS;
}

void tool_guardrail_reset(tool_guardrail_controller_t *ctrl)
{
    if (!ctrl) return;
    memset(ctrl->tracked, 0, sizeof(ctrl->tracked));
    ctrl->count = 0;
    ctrl->halt_decision_active = false;
    memset(&ctrl->halt_decision, 0, sizeof(ctrl->halt_decision));
}

/* ================================================================
 *  Find or allocate tracking slot
 * ================================================================ */

static tool_guardrail_tracked_t *find_slot(tool_guardrail_controller_t *ctrl,
                                            const char *tool_name,
                                            uint32_t args_hash)
{
    for (int i = 0; i < ctrl->count; i++) {
        if (ctrl->tracked[i].active &&
            strcmp(ctrl->tracked[i].tool_name, tool_name) == 0 &&
            ctrl->tracked[i].args_hash == args_hash)
            return &ctrl->tracked[i];
    }
    return NULL;
}

static tool_guardrail_tracked_t *alloc_slot(tool_guardrail_controller_t *ctrl)
{
    if (ctrl->count >= TOOL_GUARDRAIL_MAX_TRACKED)
        return NULL;
    tool_guardrail_tracked_t *slot = &ctrl->tracked[ctrl->count];
    memset(slot, 0, sizeof(*slot));
    slot->active = true;
    ctrl->count++;
    return slot;
}

/* ================================================================
 *  Build helper tools
 * ================================================================ */

static void set_decision(tool_guardrail_decision_t *d,
                          guardrail_action_t action,
                          const char *code,
                          const char *message,
                          const char *tool_name,
                          int count)
{
    memset(d, 0, sizeof(*d));
    d->action = action;
    if (code) snprintf(d->code, sizeof(d->code), "%s", code);
    if (message) snprintf(d->message, sizeof(d->message), "%s", message);
    if (tool_name) snprintf(d->tool_name, sizeof(d->tool_name), "%s", tool_name);
    d->count = count;
}

static const char *failure_recovery_hint(const char *tool_name, int count)
{
    (void)count;
    if (tool_name && strcmp(tool_name, "terminal") == 0) {
        return "For terminal failures, run a small diagnostic (pwd, ls -la), "
               "then try an absolute path, a simpler command, or a different tool.";
    }
    return "Try different arguments, a narrower query, an absolute path, "
           "or a different tool. If the blocker is external, report it after "
           "one diagnostic attempt.";
}

/* ================================================================
 *  before_call — check before executing
 * ================================================================ */

tool_guardrail_decision_t
tool_guardrail_before_call(tool_guardrail_controller_t *ctrl,
                            const char *tool_name,
                            const char *tool_args)
{
    tool_guardrail_decision_t allow = { .action = GUARDRAIL_ALLOW };
    if (!ctrl || !tool_name) return allow;
    if (ctrl->halt_decision_active) {
        snprintf(allow.message, sizeof(allow.message),
                 "Tool call skipped due to previous halt");
        return allow;
    }
    if (!ctrl->hard_stop_enabled) {
        snprintf(allow.tool_name, sizeof(allow.tool_name), "%s", tool_name);
        return allow;
    }

    uint32_t ah = tool_guardrail_hash(tool_args);
    tool_guardrail_tracked_t *slot = find_slot(ctrl, tool_name, ah);

    if (slot) {
        int exact_count = slot->failure_count;
        if (exact_count >= ctrl->exact_failure_block_after) {
            ctrl->halt_decision_active = true;
            tool_guardrail_decision_t d;
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "Blocked %s: the same tool call failed %d times "
                     "with identical arguments. Stop retrying it unchanged.",
                     tool_name, exact_count);
            set_decision(&d, GUARDRAIL_BLOCK, "repeated_exact_failure_block",
                         msg, tool_name, exact_count);
            ctrl->halt_decision = d;
            return d;
        }

        if (tool_guardrail_is_idempotent(tool_name)) {
            tool_guardrail_tracked_t *nps = NULL;
            for (int i = 0; i < ctrl->count; i++) {
                if (ctrl->tracked[i].active &&
                    strcmp(ctrl->tracked[i].tool_name, tool_name) == 0 &&
                    ctrl->tracked[i].args_hash == ah) {
                    nps = &ctrl->tracked[i];
                    break;
                }
            }
            if (nps && nps->no_progress_count >= ctrl->no_progress_block_after) {
                ctrl->halt_decision_active = true;
                tool_guardrail_decision_t d;
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "Blocked %s: this read-only call returned the same result "
                         "%d times. Use the result already provided or try a "
                         "different query.", tool_name, nps->no_progress_count);
                set_decision(&d, GUARDRAIL_BLOCK, "idempotent_no_progress_block",
                             msg, tool_name, nps->no_progress_count);
                ctrl->halt_decision = d;
                return d;
            }
        }
    }

    snprintf(allow.tool_name, sizeof(allow.tool_name), "%s", tool_name);
    return allow;
}

/* ================================================================
 *  after_call — record result and produce decision
 * ================================================================ */

tool_guardrail_decision_t
tool_guardrail_after_call(tool_guardrail_controller_t *ctrl,
                           const char *tool_name,
                           const char *tool_args,
                           const char *result,
                           bool failed)
{
    tool_guardrail_decision_t allow = { .action = GUARDRAIL_ALLOW };
    if (!ctrl || !tool_name) return allow;
    snprintf(allow.tool_name, sizeof(allow.tool_name), "%s", tool_name);

    uint32_t ah = tool_guardrail_hash(tool_args);
    tool_guardrail_tracked_t *slot = find_slot(ctrl, tool_name, ah);

    if (failed) {
        /* Track failure */
        if (!slot) {
            slot = alloc_slot(ctrl);
            if (!slot) return allow;
            snprintf(slot->tool_name, sizeof(slot->tool_name), "%s", tool_name);
            slot->args_hash = ah;
        }
        slot->failure_count++;
        slot->same_tool_failures++;

        /* Also update same-tool-failure for tool-name-only tracking */
        for (int i = 0; i < ctrl->count; i++) {
            if (ctrl->tracked[i].active &&
                strcmp(ctrl->tracked[i].tool_name, tool_name) == 0 &&
                ctrl->tracked[i].args_hash != ah) {
                ctrl->tracked[i].same_tool_failures++;
            }
        }

        int exact_count = slot->failure_count;
        int same_count = slot->same_tool_failures;

        /* Hard stop checks (halt turn) */
        if (ctrl->hard_stop_enabled && same_count >= ctrl->same_tool_failure_halt_after) {
            ctrl->halt_decision_active = true;
            tool_guardrail_decision_t d;
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "Stopped %s: it failed %d times this turn. "
                     "Stop retrying and choose a different approach.",
                     tool_name, same_count);
            set_decision(&d, GUARDRAIL_HALT, "same_tool_failure_halt",
                         msg, tool_name, same_count);
            ctrl->halt_decision = d;
            return d;
        }

        /* Warning checks */
        if (ctrl->warnings_enabled && exact_count >= ctrl->exact_failure_warn_after) {
            tool_guardrail_decision_t d;
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "%s has failed %d times with identical arguments. "
                     "This looks like a loop — inspect the error and change "
                     "strategy instead of retrying unchanged.",
                     tool_name, exact_count);
            set_decision(&d, GUARDRAIL_WARN, "repeated_exact_failure_warning",
                         msg, tool_name, exact_count);
            return d;
        }

        if (ctrl->warnings_enabled && same_count >= ctrl->same_tool_failure_warn_after) {
            tool_guardrail_decision_t d;
            char msg[512];
            snprintf(msg, sizeof(msg), "%s has failed %d times this turn. %s",
                     tool_name, same_count,
                     failure_recovery_hint(tool_name, same_count));
            set_decision(&d, GUARDRAIL_WARN, "same_tool_failure_warning",
                         msg, tool_name, same_count);
            return d;
        }

        allow.count = exact_count;
        return allow;
    }

    /* Tool succeeded — clear failure tracking */
    if (slot) {
        slot->failure_count = 0;
        slot->same_tool_failures = 0;
    }

    /* No-progress tracking for idempotent tools */
    if (tool_guardrail_is_idempotent(tool_name)) {
        uint32_t rh = tool_guardrail_hash(result);
        if (!slot) {
            slot = alloc_slot(ctrl);
            if (!slot) return allow;
            snprintf(slot->tool_name, sizeof(slot->tool_name), "%s", tool_name);
            slot->args_hash = ah;
            slot->result_hash = rh;
            slot->no_progress_count = 1;
        } else {
            if (slot->result_hash == rh) {
                slot->no_progress_count++;
            } else {
                slot->result_hash = rh;
                slot->no_progress_count = 1;
            }

            if (ctrl->warnings_enabled && slot->no_progress_count >= ctrl->no_progress_warn_after) {
                tool_guardrail_decision_t d;
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "%s returned the same result %d times. "
                         "Use the result already provided or change the query.",
                         tool_name, slot->no_progress_count);
                set_decision(&d, GUARDRAIL_WARN, "idempotent_no_progress_warning",
                             msg, tool_name, slot->no_progress_count);
                return d;
            }
        }
    }

    return allow;
}

/* ================================================================
 *  Synthetic result / guidance helpers
 * ================================================================ */

char *tool_guardrail_synthetic_result(const tool_guardrail_decision_t *decision)
{
    if (!decision || !decision->tool_name[0])
        return strdup("{\"error\":\"blocked by guardrail\"}");

    /* Build JSON with tool name, code, message */
    char buf[1024];
    int n = snprintf(buf, sizeof(buf),
        "{\"error\":\"%s\",\"guardrail\":{"
        "\"action\":%d,\"code\":\"%s\",\"tool_name\":\"%s\",\"count\":%d}}",
        decision->message,
        (int)decision->action,
        decision->code,
        decision->tool_name,
        decision->count);
    if (n < 0 || n >= (int)sizeof(buf))
        return strdup("{\"error\":\"blocked by guardrail\"}");
    return strdup(buf);
}

char *tool_guardrail_append_guidance(const char *result,
                                      const tool_guardrail_decision_t *decision)
{
    if (!decision) return result ? strdup(result) : strdup("");

    if (decision->action != GUARDRAIL_WARN && decision->action != GUARDRAIL_HALT)
        return result ? strdup(result) : strdup("");

    if (!decision->message[0])
        return result ? strdup(result) : strdup("");

    const char *label = (decision->action == GUARDRAIL_HALT)
        ? "Tool loop hard stop"
        : "Tool loop warning";

    /* Calculate new length */
    size_t rlen = result ? strlen(result) : 0;
    size_t suffix_len = strlen(label) + strlen(decision->code) + 80;
    char *out = (char *)malloc(rlen + suffix_len + 1);
    if (!out) return strdup("");

    if (result)
        memcpy(out, result, rlen);

    int n = snprintf(out + rlen, suffix_len,
                     "\n\n[%s: %s; count=%d; %s]",
                     label, decision->code, decision->count, decision->message);
    if (n < 0) { free(out); return strdup(""); }

    return out;
}
