/*
 * agent_message_repair.c — Message sequence repair for provider compatibility.
 *
 * Ports from Python agent_runtime_helpers:
 *   - repair_message_sequence()
 *   - sanitize_tool_call_arguments()
 *
 * Providers (OpenAI, OpenRouter, Anthropic) expect strict role alternation
 * and valid tool call argument JSON. These functions repair common protocol
 * violations before the API call.
 */

#include "hermes.h"
#include "hermes_agent.h"
#include <string.h>
#include <stdio.h>

/* Repair message sequence in place for provider compatibility.
 *
 * Pass 1: Drops stray tool messages whose tool_call_id doesn't match
 *         any preceding assistant message's tool calls.
 * Pass 2: Merges consecutive user messages with "\n\n" separator.
 *
 * Returns number of repairs made. Modifies messages[] in place and
 * updates *count to the new filtered/merged count.
 */
int hermes_repair_message_sequence(message_t *messages, int *count) {
    if (!messages || !count || *count <= 0) return 0;

    int repairs = 0;
    int n = *count;

    /* Pass 1: Drop stray tool messages that don't match a preceding
     * assistant tool_call_id. Uses a rolling set of known IDs. */
    /* Known tool IDs buffer — stores up to 64 IDs of max 64 chars each */
    char known_ids[64][64];
    int num_known = 0;

    /* We'll build the filtered list into a separate array, then copy back */
    message_t *filtered = calloc(n, sizeof(message_t));
    if (!filtered) return 0;
    int filtered_count = 0;

    for (int i = 0; i < n; i++) {
        message_t *msg = &messages[i];
        if (msg->role == MSG_ASSISTANT) {
            /* Reset known IDs from this assistant's tool calls */
            num_known = 0;
            for (int j = 0; j < msg->tool_calls_count && j < 64; j++) {
                if (msg->tool_calls[j].id[0]) {
                    snprintf(known_ids[num_known], sizeof(known_ids[0]),
                             "%s", msg->tool_calls[j].id);
                    num_known++;
                }
            }
            /* Copy the assistant message */
            filtered[filtered_count++] = *msg;
        } else if (msg->role == MSG_TOOL) {
            /* Check if this tool_call_id matches a known ID */
            bool found = false;
            if (msg->tool_call_id && msg->tool_call_id[0]) {
                for (int j = 0; j < num_known; j++) {
                    if (strcmp(msg->tool_call_id, known_ids[j]) == 0) {
                        found = true;
                        break;
                    }
                }
            }
            if (found) {
                filtered[filtered_count++] = *msg;
            } else {
                repairs++;
                /* Free the content that won't be copied */
                free(msg->content);
                free(msg->tool_call_id);
                free(msg->tool_name);
                free(msg->reasoning);
                free(msg->encrypted_content);
                /* Zero out freed pointers to prevent double-free */
                msg->content = NULL;
                msg->tool_call_id = NULL;
                msg->tool_name = NULL;
                msg->reasoning = NULL;
                msg->encrypted_content = NULL;
            }
        } else {
            if (msg->role == MSG_USER) {
                /* A user turn closes the tool-result run */
                num_known = 0;
            }
            filtered[filtered_count++] = *msg;
        }
    }

    /* Pass 2: Merge consecutive user messages */
    message_t *merged = calloc(filtered_count, sizeof(message_t));
    if (!merged) { free(filtered); return 0; }
    int merged_count = 0;

    for (int i = 0; i < filtered_count; i++) {
        message_t *msg = &filtered[i];
        if (merged_count > 0 &&
            msg->role == MSG_USER &&
            merged[merged_count - 1].role == MSG_USER) {
            /* Merge with previous user message */
            message_t *prev = &merged[merged_count - 1];
            if (prev->content && msg->content) {
                size_t new_len = strlen(prev->content) + 3 + strlen(msg->content) + 1;
                char *new_content = malloc(new_len);
                if (new_content) {
                    snprintf(new_content, new_len, "%s\n\n%s",
                             prev->content, msg->content);
                    free(prev->content);
                    prev->content = new_content;
                }
            } else if (msg->content) {
                prev->content = strdup(msg->content);
            }
            /* Free the merged message's content */
            free(msg->content);
            free(msg->tool_call_id);
            free(msg->tool_name);
            free(msg->reasoning);
            free(msg->encrypted_content);
            msg->content = NULL;
            msg->tool_call_id = NULL;
            msg->tool_name = NULL;
            msg->reasoning = NULL;
            msg->encrypted_content = NULL;
            repairs++;
        } else {
            merged[merged_count++] = *msg;
        }
    }

    /* Copy merged results back to original array.
     * Overwrite the first merged_count entries. The original entries
     * at merged_count..*count-1 are orphans: dropped entries were
     * already freed in pass 1, kept entries' content is now owned
     * by the shallow-copied merged result. Zero them to prevent
     * stale pointer tracking. */
    int old_count = *count;
    for (int i = 0; i < merged_count && i < *count; i++) {
        messages[i] = merged[i];
    }
    for (int i = merged_count; i < old_count; i++) {
        memset(&messages[i], 0, sizeof(messages[i]));
    }
    *count = merged_count;

    free(filtered);
    free(merged);
    return repairs;
}

/* Corruption marker injected into tool results when arguments were corrupted */
#define TOOL_CALL_ARGS_CORRUPTION_MARKER \
    "[hermes-agent: tool call arguments were corrupted in this session and " \
    "have been dropped to keep the conversation alive. See issue #15236.]"

/* Sanitize tool call arguments in place.
 *
 * Two-pass approach:
 *   Pass 1: Walk messages. For each assistant with tool_calls, check each
 *           tool_call's arguments. NULL/empty/corrupted → replace with "{}".
 *           Record corrupted tool_call_ids for marker prepending.
 *   Pass 2: Walk messages again. For each tool result with a recorded
 *           tool_call_id, prepend the corruption marker to its content.
 *           If no matching tool result exists, insert a new one.
 *
 * Returns number of repairs made. May insert new messages (updates *count).
 */
int hermes_sanitize_tool_call_arguments(message_t *messages, int *count) {
    if (!messages || !count || *count <= 0) return 0;

    int repairs = 0;
    int n = *count;
    int max_msgs = HERMES_MAX_MESSAGES;
    if (max_msgs > 8192) max_msgs = 8192;

    /* Track tool_call_ids that need corruption markers (up to 64) */
    char needs_marker[64][64];
    int num_needs_marker = 0;

    /* Also track which tool_call_ids already have existing results */
    bool has_existing_result[64];
    for (int i = 0; i < 64; i++) has_existing_result[i] = false;

    /* === Pass 1: Find corruptions and fix arguments === */
    for (int i = 0; i < n; i++) {
        message_t *msg = &messages[i];
        if (msg->role == MSG_ASSISTANT && msg->tool_calls_count > 0) {
            int tc_count = msg->tool_calls_count;
            if (tc_count > 64) tc_count = 64;

            for (int j = 0; j < tc_count; j++) {
                tool_call_t *tc = &msg->tool_calls[j];
                bool corrupted = false;

                if (!tc->arguments[0]) {
                    snprintf(tc->arguments, sizeof(tc->arguments), "{}");
                    corrupted = true;
                } else {
                    const char *a = tc->arguments;
                    bool valid_json = false;
                    while (*a == ' ' || *a == '\t' || *a == '\n' || *a == '\r') a++;
                    if (*a == '{' || *a == '[') valid_json = true;
                    if (!valid_json) {
                        snprintf(tc->arguments, sizeof(tc->arguments), "{}");
                        corrupted = true;
                    }
                }

                if (corrupted) {
                    repairs++;
                    if (num_needs_marker < 64) {
                        snprintf(needs_marker[num_needs_marker],
                                 sizeof(needs_marker[0]), "%s", tc->id);
                        num_needs_marker++;

                        /* Scan ahead to check if tool result exists */
                        for (int k = i + 1; k < n; k++) {
                            if (messages[k].role == MSG_TOOL &&
                                messages[k].tool_call_id &&
                                messages[k].tool_call_id[0] &&
                                strcmp(messages[k].tool_call_id, tc->id) == 0) {
                                has_existing_result[num_needs_marker - 1] = true;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    if (repairs == 0) return 0;

    /* === Pass 2: Build filtered list with markers and insertions === */
    message_t *filtered = calloc(n + num_needs_marker + 16, sizeof(message_t));
    if (!filtered) return 0;
    int filtered_count = 0;

    for (int i = 0; i < n; i++) {
        message_t *msg = &messages[i];

        if (msg->role == MSG_TOOL && msg->tool_call_id && msg->tool_call_id[0]) {
            /* Check if this tool result needs a corruption marker */
            int marker_idx = -1;
            for (int j = 0; j < num_needs_marker; j++) {
                if (strcmp(msg->tool_call_id, needs_marker[j]) == 0) {
                    marker_idx = j;
                    break;
                }
            }
            if (marker_idx >= 0) {
                /* Prepend marker to content */
                size_t marker_len = strlen(TOOL_CALL_ARGS_CORRUPTION_MARKER);
                size_t content_len = msg->content ? strlen(msg->content) : 0;
                char *new_content = malloc(marker_len + content_len + 3);
                if (new_content) {
                    if (msg->content) {
                        snprintf(new_content, marker_len + content_len + 3,
                                 "%s\n%s", TOOL_CALL_ARGS_CORRUPTION_MARKER, msg->content);
                    } else {
                        snprintf(new_content, marker_len + 1, "%s",
                                 TOOL_CALL_ARGS_CORRUPTION_MARKER);
                    }
                    free(msg->content);
                    msg->content = new_content;
                }
                filtered[filtered_count++] = *msg;
                memset(msg, 0, sizeof(*msg));
                /* Mark as handled so we don't insert a duplicate */
                has_existing_result[marker_idx] = true;
            } else {
                filtered[filtered_count++] = *msg;
                memset(msg, 0, sizeof(*msg));
            }
        } else if (msg->role == MSG_ASSISTANT && msg->tool_calls_count > 0) {
            /* Assistant with tool_calls — copy normally (args already fixed in pass 1) */
            filtered[filtered_count++] = *msg;
            memset(msg, 0, sizeof(*msg));
        } else {
            filtered[filtered_count++] = *msg;
            memset(msg, 0, sizeof(*msg));
        }
    }

    /* Insert new tool results for any corrupted calls that had no existing result */
    for (int j = 0; j < num_needs_marker; j++) {
        if (!has_existing_result[j] && filtered_count < max_msgs) {
            message_t *tr = &filtered[filtered_count++];
            memset(tr, 0, sizeof(*tr));
            tr->role = MSG_TOOL;
            tr->content = strdup(TOOL_CALL_ARGS_CORRUPTION_MARKER);
            tr->tool_call_id = strdup(needs_marker[j]);
        }
    }

    /* Copy filtered results back, zeroing beyond new count.
     * Write ALL filtered entries — caller must provide enough capacity. */
    int write_count = filtered_count;
    int old_count = *count;
    for (int i = 0; i < write_count; i++) {
        messages[i] = filtered[i];
    }
    for (int i = write_count; i < old_count; i++) {
        memset(&messages[i], 0, sizeof(messages[i]));
    }
    *count = write_count;

    free(filtered);
    return repairs;
}
