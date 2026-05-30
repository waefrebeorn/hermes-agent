/*
 * agent_message_repair.c — Message sequence repair for provider compatibility.
 *
 * Port of Python agent_runtime_helpers.repair_message_sequence().
 * Providers (OpenAI, OpenRouter, Anthropic) expect strict role alternation:
 * after system message, user/tool alternates with assistant. Tool messages
 * without a matching preceding assistant tool_call are dropped. Consecutive
 * user messages are merged.
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
     * by the shallow-copied merged result. */
    for (int i = 0; i < merged_count && i < *count; i++) {
        messages[i] = merged[i];
    }
    *count = merged_count;

    free(filtered);
    free(merged);
    return repairs;
}
