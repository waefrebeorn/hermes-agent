#ifndef HERMES_TOOL_GUARDRAILS_H
#define HERMES_TOOL_GUARDRAILS_H

/*
 * hermes_tool_guardrails.h — Per-turn tool call loop detection.
 * Port of Python agent/tool_guardrails.py.
 *
 * Tracks repeated failed tool calls within a single turn and produces
 * decisions (allow/warn/block/halt) to prevent infinite tool-calling loops.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Constants ────────────────────────────────────────────────── */

#define TOOL_GUARDRAIL_MAX_TRACKED  64   /* max tool calls tracked per turn */
#define TOOL_GUARDRAIL_NAME_LEN     64   /* max tool name length */
#define TOOL_GUARDRAIL_CODE_LEN     48   /* max decision code length */
#define TOOL_GUARDRAIL_MSG_LEN      512  /* max decision message length */

/* ── Enums ────────────────────────────────────────────────────── */

typedef enum {
    GUARDRAIL_ALLOW = 0,
    GUARDRAIL_WARN,    /* Warning injected into tool result */
    GUARDRAIL_BLOCK,   /* Blocked — tool not executed */
    GUARDRAIL_HALT,    /* Halted — turn should be aborted */
} guardrail_action_t;

/* ── Structs ──────────────────────────────────────────────────── */

/* Decision produced by the guardrail controller */
typedef struct {
    guardrail_action_t action;
    char               code[TOOL_GUARDRAIL_CODE_LEN];
    char               message[TOOL_GUARDRAIL_MSG_LEN];
    char               tool_name[TOOL_GUARDRAIL_NAME_LEN];
    int                count;
} tool_guardrail_decision_t;

/* Per-call tracking slot */
typedef struct {
    char     tool_name[TOOL_GUARDRAIL_NAME_LEN];
    uint32_t args_hash;          /* hash of canonicalized arg string */
    uint32_t result_hash;        /* hash of result string (no-progress) */
    int      failure_count;      /* consecutive failures for exact call */
    int      same_tool_failures; /* failures for this tool in this turn */
    int      no_progress_count;  /* consecutive same-result for idempotent */
    bool     active;
} tool_guardrail_tracked_t;

/* Controller — one per turn */
typedef struct {
    tool_guardrail_tracked_t tracked[TOOL_GUARDRAIL_MAX_TRACKED];
    int                      count;
    bool                     halt_decision_active;
    tool_guardrail_decision_t halt_decision;
    /* Config (set before turn) */
    bool warnings_enabled;
    bool hard_stop_enabled;
    int  exact_failure_warn_after;
    int  exact_failure_block_after;
    int  same_tool_failure_warn_after;
    int  same_tool_failure_halt_after;
    int  no_progress_warn_after;
    int  no_progress_block_after;
} tool_guardrail_controller_t;

/* ── Public API ───────────────────────────────────────────────── */

/* Initialize controller with default config */
void tool_guardrail_init(tool_guardrail_controller_t *ctrl);

/* Reset per-turn state (call at start of each turn) */
void tool_guardrail_reset(tool_guardrail_controller_t *ctrl);

/* Check before executing a tool call. Returns decision. */
tool_guardrail_decision_t
tool_guardrail_before_call(tool_guardrail_controller_t *ctrl,
                           const char *tool_name,
                           const char *tool_args);

/* Record tool result after execution. Returns decision. */
tool_guardrail_decision_t
tool_guardrail_after_call(tool_guardrail_controller_t *ctrl,
                          const char *tool_name,
                          const char *tool_args,
                          const char *result,
                          bool failed);

/* Check if tool is idempotent (read-only) */
bool tool_guardrail_is_idempotent(const char *tool_name);

/* Check if tool is mutating (writes state) */
bool tool_guardrail_is_mutating(const char *tool_name);

/* Build synthetic JSON result for a blocked tool call */
char *tool_guardrail_synthetic_result(const tool_guardrail_decision_t *decision);

/* Append guidance to existing tool result (allocates new string) */
char *tool_guardrail_append_guidance(const char *result,
                                     const tool_guardrail_decision_t *decision);

/* Hash a string to uint32 (djb2 variant — fast, not cryptographic) */
uint32_t tool_guardrail_hash(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_TOOL_GUARDRAILS_H */
