/*
 * llm_client.c — LLM API client for Hermes C.
 * Sends chat completions requests via HTTP, parses JSON response.
 * Supports: chat completions, tool calls, streaming (optional), reasoning.
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "provider.h"
#include "provider_metadata.h"
#include "error_classifier.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ================================================================
 *  Time helpers
 * ================================================================ */

/* P95: Monotonic time in seconds (for stream diagnostic timing) */
static double mono_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* Forward: populate upstream diagnostic headers from HTTP response */
static void populate_stream_diag_headers(llm_response_t *resp, const char *raw_headers);

/* ================================================================
 *  Internal helpers
 * ================================================================ */

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *d = (char *)malloc(n + 1);
    if (!d) return NULL;
    memcpy(d, s, n + 1);
    return d;
}

/* Build the messages array JSON from message list */
static json_node_t *build_messages_json(const message_t **msgs, size_t count) {
    json_node_t *arr = json_new_array();
    for (size_t i = 0; i < count; i++) {
        json_node_t *msg = json_new_object();
        const char *role_str;
        switch (msgs[i]->role) {
            case MSG_SYSTEM:    role_str = "system";    break;
            case MSG_USER:      role_str = "user";      break;
            case MSG_ASSISTANT: role_str = "assistant"; break;
            case MSG_TOOL:      role_str = "tool";      break;
            default:            role_str = "user";      break;
        }
        json_object_set(msg, "role", json_new_string(role_str));
        if (msgs[i]->content)
            json_object_set(msg, "content", json_new_string(msgs[i]->content));

        /* Tool call result */
        if (msgs[i]->role == MSG_TOOL && msgs[i]->tool_call_id)
            json_object_set(msg, "tool_call_id", json_new_string(msgs[i]->tool_call_id));

        /* Tool calls from assistant */
        if (msgs[i]->role == MSG_ASSISTANT && msgs[i]->tool_calls_count > 0) {
            json_node_t *tc_arr = json_new_array();
            for (int j = 0; j < msgs[i]->tool_calls_count; j++) {
                json_node_t *tc = json_new_object();
                json_object_set(tc, "id", json_new_string(msgs[i]->tool_calls[j].id));
                json_object_set(tc, "type", json_new_string("function"));
                json_node_t *fn = json_new_object();
                json_object_set(fn, "name", json_new_string(msgs[i]->tool_calls[j].name));
                json_object_set(fn, "arguments", json_new_string(msgs[i]->tool_calls[j].arguments));
                json_object_set(tc, "function", fn);
                json_array_append(tc_arr, tc);
            }
            json_object_set(msg, "tool_calls", tc_arr);
        }

        /* Reasoning content echo-back (B32: DeepSeek/Kimi/MiMo thinking mode)
         * Some providers (DeepSeek V4, Kimi/coding, Xiaomi MiMo) require
         * reasoning_content on every assistant message for multi-turn thinking.
         * Mirror of Python agent/conversation_loop.py reasoning_content copy. */
        if (msgs[i]->role == MSG_ASSISTANT && msgs[i]->reasoning)
            json_object_set(msg, "reasoning_content", json_new_string(msgs[i]->reasoning));

        json_array_append(arr, msg);
    }
    return arr;
}

/* ================================================================
 *  Token counting + context management
 * ================================================================ */

/* Count approximate tokens in a message list.
 * Stops counting after max_tokens is exceeded. */
size_t llm_count_context_tokens(const message_t **msgs, size_t count, size_t max_tokens) {
    size_t total = 0;
    for (size_t i = 0; i < count; i++) {
        if (!msgs || !msgs[i]) continue;
        /* Role tokens (~1 token each) */
        total += 1;
        /* Content tokens */
        total += llm_estimate_tokens(msgs[i]->content);
        /* Tool call overhead */
        if (msgs[i]->tool_calls_count > 0)
            total += (size_t)msgs[i]->tool_calls_count * 10;
        /* Tool result overhead */
        if (msgs[i]->tool_call_id)
            total += 2;
        if (total > max_tokens) break;
    }
    return total;
}

/* Truncate context to fit within max_tokens while keeping
 * system message (index 0) and most recent messages. */
void llm_truncate_context(agent_state_t *state, size_t max_tokens) {
    if (!state || state->message_count < 2) return;

    size_t current = llm_count_context_tokens(
        (const message_t **)state->messages, state->message_count, max_tokens);
    if (current <= max_tokens) return;

    /* Need to drop messages. Keep system + most recent. */
    size_t keep = (state->messages[0]->role == MSG_SYSTEM) ? 1 : 0;
    size_t target = keep + 4; /* Keep system + 4 most recent turns */
    if (state->message_count <= target) return;

    /* Remove from index keep to (message_count - target + keep) */
    size_t remove_count = state->message_count - target;
    for (size_t i = 0; i < remove_count; i++)
        message_free(state->messages[keep + i]);
    memmove(&state->messages[keep], &state->messages[keep + remove_count],
            (state->message_count - keep - remove_count) * sizeof(message_t *));
    state->message_count -= remove_count;

    /* Re-check after truncation */
    current = llm_count_context_tokens(
        (const message_t **)state->messages, state->message_count, max_tokens);
    if (current > max_tokens && state->message_count > keep + 2) {
        /* Still over budget — drop more aggressively */
        llm_truncate_context(state, max_tokens);
    }
}

/* ================================================================
 *  Context Compression — Tool Result Pruning
 * ================================================================ */

/* Simple FNV-1a hash for content deduplication */
static uint32_t compress_content_hash(const char *content) {
    uint32_t h = 2166136261u;
    if (!content) return 0;
    while (*content) {
        h ^= (unsigned char)*content++;
        h *= 16777619u;
    }
    return h;
}

/* Create an informative 1-line summary of a tool call + result.
 * Matches Python context_compressor._summarize_tool_result() format. */
static void compress_summarize_tool(const char *tool_name, const char *tool_args,
                                     const char *tool_content,
                                     char *out, size_t out_sz) {
    if (!out || out_sz == 0) return;
    out[0] = '\0';

    /* Parse args JSON if present */
    /* Count lines in content */
    int line_count = 1;
    size_t content_len = 0;
    if (tool_content) {
        content_len = strlen(tool_content);
        for (const char *p = tool_content; *p; p++) {
            if (*p == '\n') line_count++;
        }
        if (line_count < 0) line_count = 0;
    }

    if (!tool_name) tool_name = "unknown";

    if (strcmp(tool_name, "terminal") == 0) {
        /* Extract command from args */
        char cmd[256] = "";
        if (tool_args && tool_args[0]) {
            char *err = NULL;
            json_node_t *jargs = json_parse(tool_args, &err);
            free(err);
            if (jargs) {
                const char *c = json_get_str(jargs, "command", "");
                if (c[0]) {
                    size_t cl = strlen(c);
                    if (cl > 80) { memcpy(cmd, c, 77); cmd[77] = '\0'; strcat(cmd, "..."); }
                    else { memcpy(cmd, c, cl); cmd[cl] = '\0'; }
                }
                json_free(jargs);
            }
        }
        snprintf(out, out_sz, "[terminal] ran `%s` -> %d lines output",
                 cmd[0] ? cmd : "?", line_count);

    } else if (strcmp(tool_name, "read_file") == 0) {
        const char *path = "?";
        int offset = 1;
        if (tool_args && tool_args[0]) {
            char *err = NULL;
            json_node_t *jargs = json_parse(tool_args, &err);
            free(err);
            if (jargs) {
                path = json_get_str(jargs, "path", "?");
                offset = (int)json_get_num(jargs, "offset", 1);
                json_free(jargs);
            }
        }
        snprintf(out, out_sz, "[read_file] read %s from line %d (%zu chars)",
                 path, offset, content_len);

    } else if (strcmp(tool_name, "write_file") == 0) {
        const char *path = "?";
        if (tool_args && tool_args[0]) {
            char *err = NULL;
            json_node_t *jargs = json_parse(tool_args, &err);
            free(err);
            if (jargs) {
                path = json_get_str(jargs, "path", "?");
                json_free(jargs);
            }
        }
        snprintf(out, out_sz, "[write_file] wrote to %s (%d lines)",
                 path, line_count);

    } else if (strcmp(tool_name, "search_files") == 0) {
        const char *pattern = "?", *path = ".", *target = "content";
        if (tool_args && tool_args[0]) {
            char *err = NULL;
            json_node_t *jargs = json_parse(tool_args, &err);
            free(err);
            if (jargs) {
                pattern = json_get_str(jargs, "pattern", "?");
                path = json_get_str(jargs, "path", ".");
                target = json_get_str(jargs, "target", "content");
                json_free(jargs);
            }
        }
        snprintf(out, out_sz, "[search_files] %s search for '%s' in %s -> %d lines",
                 target, pattern, path, line_count);

    } else if (strcmp(tool_name, "patch") == 0) {
        const char *path = "?";
        if (tool_args && tool_args[0]) {
            char *err = NULL;
            json_node_t *jargs = json_parse(tool_args, &err);
            free(err);
            if (jargs) {
                path = json_get_str(jargs, "path", "?");
                json_free(jargs);
            }
        }
        snprintf(out, out_sz, "[patch] in %s (%zu chars result)", path, content_len);

    } else if (strcmp(tool_name, "web_search") == 0) {
        const char *query = "?";
        if (tool_args && tool_args[0]) {
            char *err = NULL;
            json_node_t *jargs = json_parse(tool_args, &err);
            free(err);
            if (jargs) {
                query = json_get_str(jargs, "query", "?");
                json_free(jargs);
            }
        }
        snprintf(out, out_sz, "[web_search] query='%s' (%zu chars result)", query, content_len);

    } else if (strcmp(tool_name, "web_extract") == 0) {
        const char *url = "?";
        if (tool_args && tool_args[0]) {
            char *err = NULL;
            json_node_t *jargs = json_parse(tool_args, &err);
            free(err);
            if (jargs) {
                json_node_t *urls = json_obj_get(jargs, "urls");
                if (urls && json_len(urls) > 0)
                    url = json_get_str(json_get(urls, 0), "", "?");
                json_free(jargs);
            }
        }
        snprintf(out, out_sz, "[web_extract] %s (%zu chars)", url, content_len);

    } else if (strcmp(tool_name, "execute_code") == 0) {
        char code_preview[80] = "";
        if (tool_args && tool_args[0]) {
            char *err = NULL;
            json_node_t *jargs = json_parse(tool_args, &err);
            free(err);
            if (jargs) {
                const char *code = json_get_str(jargs, "code", "");
                if (code[0]) {
                    size_t cl = strlen(code);
                    size_t pl = cl > 60 ? 57 : cl;
                    int newline = 0;
                    for (size_t ii = 0; ii < pl && code[ii]; ii++) {
                        if (code[ii] == '\n' && !newline) { code_preview[ii] = ' '; newline = 1; }
                        else { code_preview[ii] = code[ii]; }
                    }
                    code_preview[pl] = '\0';
                    if (cl > 60) memcpy(code_preview + pl, "...", 4);
                }
                json_free(jargs);
            }
        }
        snprintf(out, out_sz, "[execute_code] `%s` (%d lines output)",
                 code_preview[0] ? code_preview : "?", line_count);

    } else if (strcmp(tool_name, "delegate_task") == 0) {
        char goal[80] = "";
        if (tool_args && tool_args[0]) {
            char *err = NULL;
            json_node_t *jargs = json_parse(tool_args, &err);
            free(err);
            if (jargs) {
                const char *g = json_get_str(jargs, "goal", "");
                size_t gl = strlen(g);
                size_t cp = gl > 60 ? 57 : gl;
                memcpy(goal, g, cp);
                goal[cp] = '\0';
                if (gl > 60) memcpy(goal + cp, "...", 4);
                json_free(jargs);
            }
        }
        snprintf(out, out_sz, "[delegate_task] '%s' (%zu chars result)",
                 goal[0] ? goal : "?", content_len);

    } else if (strcmp(tool_name, "memory") == 0) {
        const char *action = "?", *target = "?";
        if (tool_args && tool_args[0]) {
            char *err = NULL;
            json_node_t *jargs = json_parse(tool_args, &err);
            free(err);
            if (jargs) {
                action = json_get_str(jargs, "action", "?");
                target = json_get_str(jargs, "target", "?");
                json_free(jargs);
            }
        }
        snprintf(out, out_sz, "[memory] %s on %s", action, target);

    } else if (strcmp(tool_name, "todo") == 0) {
        snprintf(out, out_sz, "[todo] updated task list");

    } else if (strcmp(tool_name, "clarify") == 0) {
        snprintf(out, out_sz, "[clarify] asked user a question");

    } else if (strcmp(tool_name, "vision_analyze") == 0) {
        char question[80] = "";
        if (tool_args && tool_args[0]) {
            char *err = NULL;
            json_node_t *jargs = json_parse(tool_args, &err);
            free(err);
            if (jargs) {
                const char *q = json_get_str(jargs, "question", "");
                size_t ql = strlen(q);
                size_t cp = ql > 50 ? 47 : ql;
                memcpy(question, q, cp);
                question[cp] = '\0';
                if (ql > 50) memcpy(question + cp, "...", 4);
                json_free(jargs);
            }
        }
        snprintf(out, out_sz, "[vision_analyze] '%s' (%zu chars)", question, content_len);

    } else {
        /* Generic fallback */
        snprintf(out, out_sz, "[%s] %s (%zu chars, %d lines)",
                 tool_name, "", content_len, line_count);
    }
}

/* Prune old tool results before LLM compression.
 * Deduplicates identical tool results, replaces old ones with 1-line summaries,
 * and truncates large tool_call arguments.
 *
 * Operates on state->messages IN PLACE (modifies content field of tool/assistant messages).
 * Does NOT add/remove messages. */
static void compress_prune_tool_results(agent_state_t *state, size_t sys_keep) {
    if (!state || state->message_count <= sys_keep + 2) return;

    /* Build tool_call_id -> (tool_name, arguments) index from assistant messages */
    #define MAX_TOOL_INDEX 256
    static struct { char id[64]; char name[128]; char args[4096]; } tool_index[MAX_TOOL_INDEX];
    int idx_count = 0;

    for (size_t i = sys_keep; i < state->message_count && idx_count < MAX_TOOL_INDEX; i++) {
        message_t *msg = state->messages[i];
        if (!msg || msg->role != MSG_ASSISTANT) continue;
        for (int j = 0; j < msg->tool_calls_count && idx_count < MAX_TOOL_INDEX; j++) {
            memcpy(tool_index[idx_count].id, msg->tool_calls[j].id, sizeof(tool_index[idx_count].id) - 1);
            tool_index[idx_count].id[sizeof(tool_index[idx_count].id) - 1] = '\0';
            memcpy(tool_index[idx_count].name, msg->tool_calls[j].name, sizeof(tool_index[idx_count].name) - 1);
            tool_index[idx_count].name[sizeof(tool_index[idx_count].name) - 1] = '\0';
            memcpy(tool_index[idx_count].args, msg->tool_calls[j].arguments, sizeof(tool_index[idx_count].args) - 1);
            tool_index[idx_count].args[sizeof(tool_index[idx_count].args) - 1] = '\0';
            idx_count++;
        }
    }

    if (idx_count == 0) return;

    /* Pass 1: Deduplicate identical tool results by content hash */
    #define MAX_HASH_MAP 256
    static struct { uint32_t hash; size_t index; } hash_map[MAX_HASH_MAP];
    int hash_count = 0;

    /* Walk backward so older duplicates get replaced */
    for (size_t i = state->message_count; i > sys_keep; i--) {
        size_t idx = i - 1;
        message_t *msg = state->messages[idx];
        if (!msg || msg->role != MSG_TOOL || !msg->content) continue;
        size_t clen = strlen(msg->content);
        if (clen < 200) continue;

        uint32_t h = compress_content_hash(msg->content);
        int found = -1;
        for (int hi = 0; hi < hash_count; hi++) {
            if (hash_map[hi].hash == h) { found = hi; break; }
        }
        if (found >= 0) {
            /* Older duplicate — replace with back-reference */
            const char *dup_text = "[Duplicate tool output — same content as a more recent call]";
            free(msg->content);
            msg->content = strdup(dup_text);
        } else if (hash_count < MAX_HASH_MAP) {
            hash_map[hash_count].hash = h;
            hash_map[hash_count].index = idx;
            hash_count++;
        }
    }

    /* Pass 2: Replace old tool results with 1-line summaries */
    /* Determine prune boundary — protect the most recent tool results within tail budget */
    size_t tail_budget_tokens = 2048; /* ~20% of 10K, similar to llm_compress_context */
    size_t tail_tokens = 0;
    int tail_count = 0;
    size_t prune_boundary = state->message_count;

    for (size_t i = state->message_count; i > sys_keep; i--) {
        size_t idx = i - 1;
        message_t *msg = state->messages[idx];
        if (!msg) continue;
        size_t mt = (size_t)context_message_tokens(msg);
        if (tail_tokens + mt > tail_budget_tokens && tail_count >= 3) {
            prune_boundary = idx;
            break;
        }
        tail_tokens += mt;
        tail_count++;
    }
    if (tail_count < 3) tail_count = 3;
    /* Ensure we don't prune past the protected tail */
    size_t actual_boundary = state->message_count;
    if (state->message_count > tail_count + sys_keep)
        actual_boundary = state->message_count - tail_count - sys_keep;
    else
        actual_boundary = sys_keep;
    if (actual_boundary > prune_boundary) actual_boundary = prune_boundary;

    for (size_t i = sys_keep; i < actual_boundary; i++) {
        message_t *msg = state->messages[i];
        if (!msg || msg->role != MSG_TOOL || !msg->content) continue;

        /* Skip already-deduplicated or short content */
        size_t clen = strlen(msg->content);
        if (clen < 200) continue;
        if (strstr(msg->content, "[Duplicate tool output")) continue;
        if (strstr(msg->content, "[old tool output truncated]")) continue;

        /* Look up tool name from index */
        const char *tool_name = "unknown";
        const char *tool_args = "";
        for (int hi = 0; hi < idx_count; hi++) {
            if (msg->tool_call_id && strcmp(tool_index[hi].id, msg->tool_call_id) == 0) {
                tool_name = tool_index[hi].name;
                tool_args = tool_index[hi].args;
                break;
            }
        }

        char summary[512];
        compress_summarize_tool(tool_name, tool_args, msg->content, summary, sizeof(summary));
        free(msg->content);
        msg->content = strdup(summary);
    }

    /* Pass 3: Truncate large tool_call arguments in assistant messages outside protected tail */
    for (size_t i = sys_keep; i < actual_boundary; i++) {
        message_t *msg = state->messages[i];
        if (!msg || msg->role != MSG_ASSISTANT || msg->tool_calls_count == 0) continue;
        for (int j = 0; j < msg->tool_calls_count; j++) {
            size_t alen = strlen(msg->tool_calls[j].arguments);
            if (alen > 500) {
                /* Truncate arguments to 200 chars + ellipsis */
                char truncated[4160]; /* 4096 max + room */
                memcpy(truncated, msg->tool_calls[j].arguments, 200);
                truncated[200] = '\0';
                strcat(truncated, "...[truncated]");
                memcpy(msg->tool_calls[j].arguments, truncated, sizeof(msg->tool_calls[j].arguments) - 1);
                msg->tool_calls[j].arguments[sizeof(msg->tool_calls[j].arguments) - 1] = '\0';
            }
        }
    }
}

/* ================================================================
 *  Smart Context Compression
 * ================================================================ */

/* Summarization prompt for context compression */
#define COMPRESS_PROMPT \
    "Summarize the following conversation segment, preserving key " \
    "information: what was discussed, what tools/files were used, " \
    "what decisions were made, and any important findings. " \
    "Be concise but complete. Output only the summary:\n\n"

/* Compress middle messages by summarizing them via LLM.
 * Returns a malloc'd summary string, or NULL on failure.
 * Does NOT modify state.
 * Matches Python context_compressor._serialize_for_summary() format. */
static char *llm_compress_messages(const message_t **msgs, size_t count,
                                    llm_config_t *llm_cfg) {
    if (!msgs || count == 0 || !llm_cfg) return NULL;

    /* Content truncation limits matching Python _serialize_for_summary */
    #define SERIALIZE_CONTENT_MAX   6000    /* max chars per message body */
    #define SERIALIZE_CONTENT_HEAD  4000    /* chars kept from start */
    #define SERIALIZE_CONTENT_TAIL  1500    /* chars kept from end */
    #define SERIALIZE_TOOL_ARGS_MAX 1500    /* tool call argument chars */
    #define SERIALIZE_TOOL_ARGS_HEAD 1200   /* kept from start of tool args */

    /* Build conversation segment text with Python-style formatting */
    size_t total = 0;
    /* Estimate size: role labels ~80 bytes per message + content + tool calls */
    for (size_t i = 0; i < count; i++) {
        if (!msgs[i]) continue;
        total += 80; /* overhead per message (label, separator, newlines) */
        if (msgs[i]->content) {
            size_t clen = strlen(msgs[i]->content);
            if (clen > SERIALIZE_CONTENT_MAX)
                total += SERIALIZE_CONTENT_HEAD + SERIALIZE_CONTENT_TAIL + 30;
            else
                total += clen;
        }
        /* Tool calls in assistant messages */
        if (msgs[i]->role == MSG_ASSISTANT && msgs[i]->tool_calls_count > 0) {
            for (int j = 0; j < msgs[i]->tool_calls_count; j++) {
                total += strlen(msgs[i]->tool_calls[j].name) + 8;
                size_t alen = strlen(msgs[i]->tool_calls[j].arguments);
                if (alen > SERIALIZE_TOOL_ARGS_MAX)
                    total += SERIALIZE_TOOL_ARGS_HEAD + 5;
                else
                    total += alen;
            }
        }
    }
    total += strlen(COMPRESS_PROMPT) + 1;
    if (total > 100000) total = 100000; /* safety cap */

    char *text = (char *)malloc(total);
    if (!text) return NULL;
    text[0] = '\0';

    /* Helper: truncate content with head/middle/tail pattern */
    #define TRUNCATE_BUF_SIZE (SERIALIZE_CONTENT_HEAD + SERIALIZE_CONTENT_TAIL + 40)
    char trunc_buf[TRUNCATE_BUF_SIZE];

    strcat(text, COMPRESS_PROMPT);
    for (size_t i = 0; i < count; i++) {
        if (!msgs[i]) continue;
        size_t cur = strlen(text);
        size_t remain = total > cur ? total - cur : 0;
        if (remain < 20) break;

        /* Redact sensitive data before serialization (matches Python redact_sensitive_text) */
        const char *raw_content = msgs[i]->content ? msgs[i]->content : "";
        char *redacted = hermes_redact(raw_content);
        const char *content = redacted ? redacted : raw_content;
        size_t clen = strlen(content);

        if (msgs[i]->role == MSG_TOOL) {
            /* Tool result: [TOOL RESULT id]: content */
            const char *tid = msgs[i]->tool_call_id ? msgs[i]->tool_call_id : "";
            if (clen > SERIALIZE_CONTENT_MAX) {
                size_t head_cp = SERIALIZE_CONTENT_HEAD < clen ? SERIALIZE_CONTENT_HEAD : clen;
                size_t tail_start = clen > SERIALIZE_CONTENT_TAIL ? clen - SERIALIZE_CONTENT_TAIL : 0;
                snprintf(trunc_buf, sizeof(trunc_buf),
                         "%.*s\n...[truncated]...\n%s",
                         (int)head_cp, content,
                         tail_start > 0 ? content + tail_start : "");
                snprintf(text + cur, remain, "[TOOL RESULT %s]: %s\n", tid, trunc_buf);
            } else {
                snprintf(text + cur, remain, "[TOOL RESULT %s]: %s\n", tid, content);
            }

        } else if (msgs[i]->role == MSG_ASSISTANT) {
            /* Assistant: include tool call names + args */
            char tc_block[4096] = "";
            if (msgs[i]->tool_calls_count > 0) {
                size_t tc_pos = 0;
                tc_pos += snprintf(tc_block + tc_pos, sizeof(tc_block) - tc_pos,
                                   "\n[Tool calls:\n");
                for (int j = 0; j < msgs[i]->tool_calls_count && tc_pos < sizeof(tc_block) - 100; j++) {
                    const char *tname = msgs[i]->tool_calls[j].name;
                    const char *targs = msgs[i]->tool_calls[j].arguments;
                    size_t alen = strlen(targs);
                    if (alen > SERIALIZE_TOOL_ARGS_MAX) {
                        char args_trunc[SERIALIZE_TOOL_ARGS_HEAD + 10];
                        size_t head_cp = SERIALIZE_TOOL_ARGS_HEAD < alen ? SERIALIZE_TOOL_ARGS_HEAD : alen;
                        memcpy(args_trunc, targs, head_cp);
                        args_trunc[head_cp] = '\0';
                        strcat(args_trunc, "...");
                        tc_pos += snprintf(tc_block + tc_pos, sizeof(tc_block) - tc_pos,
                                           "  %s(%s)\n", tname, args_trunc);
                    } else {
                        tc_pos += snprintf(tc_block + tc_pos, sizeof(tc_block) - tc_pos,
                                           "  %s(%s)\n", tname, targs);
                    }
                }
                snprintf(tc_block + tc_pos, sizeof(tc_block) - tc_pos, "]");
            }

            /* Content + tool calls block */
            if (clen > SERIALIZE_CONTENT_MAX) {
                size_t head_cp = SERIALIZE_CONTENT_HEAD < clen ? SERIALIZE_CONTENT_HEAD : clen;
                size_t tail_start = clen > SERIALIZE_CONTENT_TAIL ? clen - SERIALIZE_CONTENT_TAIL : 0;
                snprintf(trunc_buf, sizeof(trunc_buf),
                         "%.*s\n...[truncated]...\n%s",
                         (int)head_cp, content,
                         tail_start > 0 ? content + tail_start : "");
                snprintf(text + cur, remain, "[ASSISTANT]: %s%s\n",
                         trunc_buf, tc_block);
            } else {
                snprintf(text + cur, remain, "[ASSISTANT]: %s%s\n",
                         content, tc_block);
            }

        } else {
            /* User and other roles: [ROLE]: content */
            const char *role_label = "USER";
            if (msgs[i]->role == MSG_SYSTEM) role_label = "SYSTEM";
            if (clen > SERIALIZE_CONTENT_MAX) {
                size_t head_cp = SERIALIZE_CONTENT_HEAD < clen ? SERIALIZE_CONTENT_HEAD : clen;
                size_t tail_start = clen > SERIALIZE_CONTENT_TAIL ? clen - SERIALIZE_CONTENT_TAIL : 0;
                snprintf(trunc_buf, sizeof(trunc_buf),
                         "%.*s\n...[truncated]...\n%s",
                         (int)head_cp, content,
                         tail_start > 0 ? content + tail_start : "");
                snprintf(text + cur, remain, "[%s]: %s\n", role_label, trunc_buf);
            } else {
                snprintf(text + cur, remain, "[%s]: %s\n", role_label, content);
            }
        }
        /* Free redacted copy if allocated */
        if (redacted) free(redacted);
    }

    /* Create a temporary config for the summarization call */
    llm_config_t compress_cfg;
    memcpy(&compress_cfg, llm_cfg, sizeof(compress_cfg));
    compress_cfg.base_url[0] = '\0';
    compress_cfg.api_key[0] = '\0';

    /* Build summarization messages */
    message_t *sys = message_new(MSG_SYSTEM,
        "You are a summarization agent creating a context checkpoint. "
        "Treat the conversation turns below as source material for a "
        "compact record of prior work. "
        "Produce only the structured summary; do not add a greeting, "
        "preamble, or prefix. "
        "NEVER include API keys, tokens, passwords, secrets, credentials, "
        "or connection strings in the summary -- replace any that appear "
        "with [REDACTED].\n\n"
        "Use this exact structure:\n"
        "## Active Task\n"
        "[The user's most recent request verbatim -- pick up exactly here]\n\n"
        "## Completed Actions\n"
        "[Numbered list: ACTION target -- outcome]\n\n"
        "## Active State\n"
        "[Working directory, modified files, test status, running processes]\n\n"
        "## Key Decisions\n"
        "[Technical decisions and WHY they were made]\n\n"
        "## Blocked\n"
        "[Any blockers, errors, or issues not yet resolved]\n\n"
        "## Remaining Work\n"
        "[What remains -- framed as context, not instructions]");
    message_t *user = message_new(MSG_USER, text);
    const message_t *compress_msgs[2] = {sys, user};

    /* B01: Scaled summary budget — proportional to content size, capped at 5% of 128K = 6400 */
    /* Matches Python _compute_summary_budget: content_tokens * 0.20, min 2000, max 5% of context */
    size_t content_tokens = (strlen(text) + 3) / 4;  /* rough estimate */
    int scaled_budget = (int)(content_tokens * 0.20);
    int max_budget = 6400;  /* 5% of 128K context */
    if (max_budget > 12000) max_budget = 12000;  /* Python _SUMMARY_TOKENS_CEILING */
    if (scaled_budget < 2000) scaled_budget = 2000;  /* Python _MIN_SUMMARY_TOKENS */
    if (scaled_budget > max_budget) scaled_budget = max_budget;
    compress_cfg.max_tokens = scaled_budget;

    llm_response_t *resp = llm_chat_completion(&compress_cfg,
                                                compress_msgs, 2, NULL);
    char *summary = NULL;
    if (resp && resp->content) {
        summary = strdup(resp->content);
    }
    llm_response_free(resp);
    message_free(sys);
    message_free(user);
    free(text);
    return summary;
}

/* Smart context compression: before dropping old messages, summarize them.
 * Returns summary string on success (caller must insert), NULL if compression
 * is disabled or fails (caller should fall back to dropping). */
char *llm_compress_context(agent_state_t *state, size_t max_tokens,
                            bool enabled) {
    if (!state || !enabled || state->message_count < 3) return NULL;

    /* Phase 1: Prune old tool results before counting tokens.
     * This deduplicates identical outputs, replaces large tool results with
     * 1-line summaries, and truncates oversized tool_call arguments.
     * Matches Python context_compressor._prune_old_tool_results(). */
    size_t keep = (state->messages[0]->role == MSG_SYSTEM) ? 1 : 0;
    if (keep >= state->message_count) return NULL;
    compress_prune_tool_results(state, keep);

    size_t current = llm_count_context_tokens(
        (const message_t **)state->messages, state->message_count, max_tokens);
    if (current <= max_tokens) return NULL;

    /* B02: Token-budget tail — walk backwards to determine how many
     * messages to preserve. Budget = 20% of max_tokens (summary_target_ratio). */
    size_t tail_budget = max_tokens / 5; /* 20% */
    if (tail_budget < 1024) tail_budget = 1024; /* floor: at least 1K tokens */
    size_t tail_tokens = 0;
    int tail_count = 0;
    for (int i = (int)state->message_count - 1; i >= (int)keep; i--) {
        if (!state->messages[i]) continue;
        size_t mt = (size_t)context_message_tokens(state->messages[i]);
        if (tail_tokens + mt > tail_budget && tail_count >= 2) break;
        tail_tokens += mt;
        tail_count++;
    }
    if (tail_count < 2) tail_count = 2;

    size_t compress_count = (tail_count < (int)(state->message_count - keep))
        ? state->message_count - keep - tail_count : 0;
    if (compress_count < 2 || compress_count > 20) return NULL;

    /* Boundary alignment: don't split tool_call/tool_result pairs.
     * If the first tail message is a tool result, exclude it from the tail
     * and include it in the compress region instead. This prevents the
     * compressed summary from referencing a tool_call whose result is
     * still in the tail, or a tool result with no preceding call. */
    size_t compress_end = keep + compress_count;
    if (compress_end < state->message_count &&
        state->messages[compress_end] &&
        state->messages[compress_end]->role == MSG_TOOL) {
        /* Move the orphan tool result into the compress region */
        compress_count++;
        if (compress_count > 20) compress_count = 20;
    } else if (compress_end > keep && compress_end - 1 < state->message_count &&
               compress_end - 1 >= keep &&
               state->messages[compress_end - 1] &&
               state->messages[compress_end - 1]->role == MSG_ASSISTANT &&
               state->messages[compress_end - 1]->tool_calls_count > 0) {
        /* The last compress message is an assistant with pending tool calls.
         * Move it into the tail so the tool result has its call visible. */
        if (tail_count > 0) {
            compress_count--;
        }
    }
    if (compress_count < 2) return NULL;

    return llm_compress_messages(
        (const message_t **)&state->messages[keep],
        compress_count, &state->llm);
}

/* ================================================================
 *  LLM API call
 * ================================================================ */

llm_response_t *llm_chat_completion(llm_config_t *cfg,
                                     const message_t **messages,
                                     size_t message_count,
                                     json_node_t *tools_json)
{
    llm_response_t *resp = (llm_response_t *)calloc(1, sizeof(llm_response_t));
    if (!resp) return NULL;
    resp->content = NULL;
    resp->reasoning = NULL;
    resp->input_tokens = 0;
    resp->output_tokens = 0;
    resp->tool_calls_count = 0;

    /* Create provider instance from config */
    provider_t *prov = provider_create(
        cfg->provider[0] ? cfg->provider : "openai",
        cfg->model, cfg->api_key, cfg->base_url);

    /* P91: Pass system cache state to provider */
    if (prov) prov->system_cached = cfg->system_cached;

    /* Copy LLM request params from config to provider */
    if (prov) {
        prov->config.max_tokens = cfg->max_tokens;
        prov->config.temperature = cfg->temperature;
        prov->config.top_p = cfg->top_p;
        prov->config.stop_count = cfg->stop_count;
        memcpy(prov->config.stop_sequences, cfg->stop_sequences,
               sizeof(prov->config.stop_sequences));
        prov->config.presence_penalty = cfg->presence_penalty;
        prov->config.frequency_penalty = cfg->frequency_penalty;
        prov->config.seed = cfg->seed;
        prov->config.logprobs = cfg->logprobs;
        prov->config.top_logprobs = cfg->top_logprobs;
        memcpy(prov->config.user, cfg->user, sizeof(prov->config.user));
        memcpy(prov->config.service_tier, cfg->service_tier,
               sizeof(prov->config.service_tier));
        memcpy(prov->config.reasoning_effort, cfg->reasoning_effort,
               sizeof(prov->config.reasoning_effort));
        memcpy(prov->config.response_format, cfg->response_format,
               sizeof(prov->config.response_format));
        memcpy(prov->config.metadata, cfg->metadata,
               sizeof(prov->config.metadata));
        memcpy(prov->config.tool_choice, cfg->tool_choice,
               sizeof(prov->config.tool_choice));
        prov->config.parallel_tool_calls = cfg->parallel_tool_calls;
        prov->config.top_k = cfg->top_k;
        prov->config.candidate_count = cfg->candidate_count;
        prov->config.json_mode = cfg->json_mode;
        prov->config.response_format_strict = cfg->response_format_strict;
        memcpy(prov->config.safety_settings, cfg->safety_settings,
               sizeof(prov->config.safety_settings));
        prov->config.max_tool_calls = cfg->max_tool_calls;
        prov->config.n = cfg->n;
    }

    if (prov && prov->ops) {
        const provider_ops_t *ops = prov->ops;
        char *url = ops->build_url(prov, cfg->base_url);
        /* P158: Verify URL is trusted for this provider's API key */
        const char *effective_key = cfg->api_key;
        if (effective_key[0] && !provider_url_is_trusted(cfg->provider, url)) {
            effective_key = "";
        }
        char *headers = ops->build_headers(prov, effective_key);
        char *body = ops->build_request_body(prov, messages, message_count, tools_json, false);
        if (!url || !headers || !body) {
            free(url); free(headers); free(body);
            provider_free(prov); free(resp); return NULL;
        }
        http_client_t *client = http_client_new_with_retry(30, 3, 1000);
        http_response_t *http_resp = http_request(client, HTTP_POST, url,
                                                   headers, body, strlen(body));
        free(url); free(headers); free(body);

        /* P95: Capture upstream diagnostic headers from response */
        if (http_resp)
            populate_stream_diag_headers(resp, http_resp->headers);

        /* Classify HTTP error responses for logging and retry decisions */
        if (http_resp && http_resp->status >= 400) {
            classified_error_t err;
            error_classify(http_resp->status, http_resp->body,
                           cfg->provider, cfg->model,
                           0, 0, &err);
            char err_buf[256];
            error_format(&err, err_buf, sizeof(err_buf));
            if (resp->diag.upstream_headers[0])
                fprintf(stderr, "[llm] %s [upstream: %s]\n",
                        err_buf, resp->diag.upstream_headers);
            else
                fprintf(stderr, "[llm] %s\n", err_buf);
            if (err.should_compress)
                resp->compress_hint = true;
            if (err.should_rotate_credential)
                resp->credential_expired = true;
        }

        if (!http_resp || http_resp->status < 0) {
            resp->content = strdup("HTTP request failed");
            if (http_resp) http_response_free(http_resp);
            http_client_free(client); provider_free(prov);
            return resp;
        }
        provider_response_t *presp = ops->parse_response(prov, http_resp->body);
        http_response_free(http_resp); http_client_free(client); provider_free(prov);
        if (presp) {
            resp->content = presp->content; presp->content = NULL;
            resp->reasoning = presp->reasoning; presp->reasoning = NULL;
            resp->input_tokens = presp->input_tokens;
            resp->output_tokens = presp->output_tokens;
            resp->tool_calls_count = presp->tool_calls_count;
            for (int i = 0; i < presp->tool_calls_count && i < 64; i++)
                resp->tool_calls[i] = presp->tool_calls[i];
            ops->free_response(presp);
        }
        return resp;
    }
    provider_free(prov);

    /* --- legacy fallback --- */
    json_node_t *req = json_new_object();
    json_object_set(req, "model", json_new_string(cfg->model));

    /* Messages */
    json_node_t *msgs = build_messages_json(messages, message_count);
    json_object_set(req, "messages", msgs);

    /* Add tools if provided */
    if (tools_json && json_array_count(tools_json) > 0)
        json_object_set(req, "tools", json_copy(tools_json));

    /* Serialize */
    char *body = json_serialize(req);

    /* Determine URL */
    char url[512];
    const char *base = cfg->base_url;
    if (base && strlen(base) > 0) {
        /* Check if base already ends with /chat/completions */
        if (strstr(base, "/chat/completions"))
            snprintf(url, sizeof(url), "%s", base);
        else
            snprintf(url, sizeof(url), "%s/chat/completions", base);
    } else {
        snprintf(url, sizeof(url), "https://api.openai.com/v1/chat/completions");
    }

    /* Build auth header */
    char auth_header[512];
    const char *effective_key = cfg->api_key;
    if (effective_key[0] && !provider_url_is_trusted(cfg->provider, url)) {
        effective_key = "";
    }
    if (effective_key[0]) {
        snprintf(auth_header, sizeof(auth_header),
                "Authorization: Bearer %s\r\nContent-Type: application/json",
                 effective_key);
    } else {
        snprintf(auth_header, sizeof(auth_header),
                 "Content-Type: application/json");
    }

    /* Make HTTP request */
    http_client_t *client = http_client_new_with_retry(30, 3, 1000);
    http_response_t *http_resp = http_request(client, HTTP_POST, url,
                                               auth_header, body, strlen(body));

    json_free(req);
    free(body);

    if (!http_resp ||        http_resp->status < 0) {
        resp->content = xstrdup("HTTP request failed");
        if (http_resp) http_response_free(http_resp);
        http_client_free(client);
        return resp;
    }

    /* Parse JSON response */
    char *json_err = NULL;
    json_node_t *root = json_parse(http_resp->body, &json_err);
    http_response_free(http_resp);

    if (!root) {
        resp->content = (char *)malloc(256);
        snprintf(resp->content, 256, "JSON parse error: %s", json_err ? json_err : "unknown");
        free(json_err);
        http_client_free(client);
        return resp;
    }

    /* Extract usage if present */
    json_node_t *usage = json_object_get(root, "usage");
    if (usage) {
        resp->input_tokens = (int)json_object_get_number(usage, "prompt_tokens", 0);
        resp->output_tokens = (int)json_object_get_number(usage, "completion_tokens", 0);
    }

    /* Extract choices */
    json_node_t *choices = json_object_get(root, "choices");
    if (choices && json_array_count(choices) > 0) {
        json_node_t *choice = json_array_get(choices, 0);
        json_node_t *message = json_object_get(choice, "message");

        if (message) {
            resp->content = xstrdup(json_object_get_string(message, "content", ""));

            /* Check for reasoning */
            json_node_t *reasoning = json_object_get(message, "reasoning");
            if (reasoning && reasoning->type == JSON_STRING)
                resp->reasoning = xstrdup(reasoning->str_val);

            /* Check for tool calls */
            json_node_t *tool_calls = json_object_get(message, "tool_calls");
            if (tool_calls && json_array_count(tool_calls) > 0) {
                resp->tool_calls_count = (int)json_array_count(tool_calls);
                if (resp->tool_calls_count > 64)
                    resp->tool_calls_count = 64;
                for (int i = 0; i < resp->tool_calls_count; i++) {
                    json_node_t *tc = json_array_get(tool_calls, (size_t)i);
                    const char *id = json_object_get_string(tc, "id", "");
                    snprintf(resp->tool_calls[i].id, sizeof(resp->tool_calls[i].id), "%s", id);
                    json_node_t *fn = json_object_get(tc, "function");
                    if (fn) {
                        const char *name = json_object_get_string(fn, "name", "");
                        const char *args = json_object_get_string(fn, "arguments", "{}");
                        snprintf(resp->tool_calls[i].name, sizeof(resp->tool_calls[i].name), "%s", name);
                        snprintf(resp->tool_calls[i].arguments, sizeof(resp->tool_calls[i].arguments), "%s", args);
                    }
                }
            }

            /* Reasoning content field (some providers use this instead of "reasoning") */
            json_node_t *reasoning_content = json_object_get(message, "reasoning_content");
            if (!resp->reasoning && reasoning_content && reasoning_content->type == JSON_STRING)
                resp->reasoning = xstrdup(reasoning_content->str_val);

            /* L07: xAI encrypted reasoning content — serialize the encrypted_content array */
            json_node_t *enc_content = json_object_get(message, "encrypted_content");
            if (enc_content && json_array_count(enc_content) > 0) {
                char *ser = json_serialize(enc_content);
                if (ser) { resp->encrypted_content = ser; }
            }
        }
    }

    json_free(root);
    http_client_free(client);
    return resp;
}

/* ================================================================
 *  Streaming: per-chunk callback context
 * ================================================================ */

typedef struct {
    llm_response_t  *resp;      /* accumulated response */
    llm_token_cb_t   token_cb;  /* token callback */
    void            *userdata;  /* callback userdata */
    const provider_ops_t *prov_ops;  /* provider ops for per-chunk parsing (optional) */
    provider_t      *prov;      /* provider instance (optional) */
    /* Track tool_calls across chunks (streaming builds them incrementally) */
    int              max_tc_idx; /* highest tool_call index seen */
    char             tc_id[64][64];  /* id per index */
    char             tc_name[64][128]; /* name per index */
    char             tc_args[64][4096]; /* args buffer per index */
    bool             streaming_done; /* set when streaming is complete */
    /* P95: Stream diagnostic timing */
    double           req_start_time;  /* monotonic time of request send */
    size_t           token_count;     /* running token count */
    bool             first_token_flag; /* first content token tracked */
} stream_ctx_t;

/* Provider-aware stream chunk handler.
 * Uses provider's parse_stream_chunk for text deltas, falls back
 * to OpenAI-style delta parsing for tool calls. */
static int on_provider_stream_chunk(const char *data, size_t len, void *userdata) {
    stream_ctx_t *ctx = (stream_ctx_t *)userdata;
    (void)len;

    if (!ctx->prov_ops || !ctx->prov) {
        /* No provider — skip */
        return 0;
    }

    /* Try provider's parse_stream_chunk first */
    provider_response_t *delta = ctx->prov_ops->parse_stream_chunk(ctx->prov, data);
    if (delta) {
        /* Text content from provider */
        if (delta->content && delta->content[0]) {
            /* P95: Track first token timing */
            if (!ctx->first_token_flag) {
                ctx->first_token_flag = true;
                ctx->resp->diag.first_token_time = mono_time();
                ctx->resp->diag.first_token_received = true;
                ctx->resp->diag.time_to_first_token =
                    ctx->resp->diag.first_token_time - ctx->req_start_time;
            }
            ctx->token_count++;
            size_t cur = ctx->resp->content ? strlen(ctx->resp->content) : 0;
            size_t add = strlen(delta->content);
            char *newc = (char *)realloc(ctx->resp->content, cur + add + 1);
            if (newc) {
                ctx->resp->content = newc;
                memcpy(ctx->resp->content + cur, delta->content, add + 1);
            }
            if (ctx->token_cb)
                ctx->token_cb(delta->content, ctx->userdata);
        }

        /* B22: finish_reason from provider delta */
        if (delta->finish_reason[0])
            snprintf(ctx->resp->finish_reason, sizeof(ctx->resp->finish_reason),
                     "%s", delta->finish_reason);

        /* Token counts from delta (Anthropic sends in message_delta) */
        if (delta->input_tokens > 0)
            ctx->resp->input_tokens = delta->input_tokens;
        if (delta->output_tokens > 0)
            ctx->resp->output_tokens = delta->output_tokens;

        /* Tool calls from provider (Anthropic sends complete tool_use blocks,
         * OpenAI sends deltas — handled below) */
        if (delta->tool_calls_count > 0) {
            for (int i = 0; i < delta->tool_calls_count && i < 64; i++) {
                int idx = ctx->max_tc_idx;
                if (idx < 64) {
                    snprintf(ctx->tc_id[idx], sizeof(ctx->tc_id[idx]), "%s",
                             delta->tool_calls[i].id);
                    snprintf(ctx->tc_name[idx], sizeof(ctx->tc_name[idx]), "%s",
                             delta->tool_calls[i].name);
                    snprintf(ctx->tc_args[idx], sizeof(ctx->tc_args[idx]), "%s",
                             delta->tool_calls[i].arguments);
                    ctx->max_tc_idx = idx + 1;
                }
            }
        }

        ctx->prov_ops->free_response(delta);
    }

    /* Fallback: also try OpenAI-style delta parsing for tool calls.
     * This handles the case where parse_stream_chunk returns empty
     * but the chunk has tool_call deltas (OpenAI format).
     * HTTP layer already strips "data: " prefix — handle both cases. */
    const char *json_str = data;
    if (strncmp(data, "data: ", 6) == 0)
        json_str = data + 6;
    if (strncmp(json_str, "[DONE]", 6) == 0) {
            ctx->streaming_done = true;
            return 0;
        }

    char *err = NULL;
    json_node_t *root = json_parse(json_str, &err);
    if (!root) { free(err); return 0; }

    json_node_t *choices = json_object_get(root, "choices");
    if (choices && json_array_count(choices) > 0) {
        json_node_t *choice = json_array_get(choices, 0);
        json_node_t *delta = json_object_get(choice, "delta");

        /* OpenAI tool call deltas */
        if (delta) {
            json_node_t *tc_delta = json_object_get(delta, "tool_calls");
            if (tc_delta && json_array_count(tc_delta) > 0) {
                for (size_t i = 0; i < json_array_count(tc_delta); i++) {
                    json_node_t *tc = json_array_get(tc_delta, i);
                    int idx = (int)json_object_get_number(tc, "index", 0);
                    if (idx >= 64) continue;
                    if (idx >= ctx->max_tc_idx) ctx->max_tc_idx = idx + 1;
                    const char *id = json_object_get_string(tc, "id", NULL);
                    if (id && id[0])
                        snprintf(ctx->tc_id[idx], sizeof(ctx->tc_id[idx]), "%s", id);
                    json_node_t *fn = json_object_get(tc, "function");
                    if (fn) {
                        const char *name = json_object_get_string(fn, "name", NULL);
                        if (name && name[0])
                            snprintf(ctx->tc_name[idx], sizeof(ctx->tc_name[idx]), "%s", name);
                        const char *args_chunk = json_object_get_string(fn, "arguments", NULL);
                        if (args_chunk && args_chunk[0]) {
                            size_t cur = strlen(ctx->tc_args[idx]);
                            size_t add = strlen(args_chunk);
                            if (cur + add < sizeof(ctx->tc_args[idx]) - 1) {
                                memcpy(ctx->tc_args[idx] + cur, args_chunk, add);
                                ctx->tc_args[idx][cur + add] = '\0';
                            }
                        }
                    }
                }
            }

            /* OpenAI finish reason */
            const char *finish = json_object_get_string(choice, "finish_reason", NULL);
            if (finish && finish[0]) {
                ctx->streaming_done = true;
                snprintf(ctx->resp->finish_reason, sizeof(ctx->resp->finish_reason), "%s", finish);
                json_node_t *usage = json_object_get(root, "usage");
                if (usage) {
                    ctx->resp->input_tokens = (int)json_object_get_number(usage, "prompt_tokens", 0);
                    ctx->resp->output_tokens = (int)json_object_get_number(usage, "completion_tokens", 0);
                }
            }
        }
    }

    json_free(root);
    return 0;
}

static int on_stream_chunk(const char *data, size_t len, void *userdata) {
    stream_ctx_t *ctx = (stream_ctx_t *)userdata;
    (void)len;

    /* Parse JSON */
    char *err = NULL;
    json_node_t *root = json_parse(data, &err);
    if (!root) { free(err); return 0; } /* Skip non-JSON events */

    json_node_t *choices = json_object_get(root, "choices");
    if (!choices || json_array_count(choices) == 0) { json_free(root); return 0; }

    json_node_t *choice = json_array_get(choices, 0);
    json_node_t *delta = json_object_get(choice, "delta");
    if (!delta) { json_free(root); return 0; }

    /* Extract finish_reason */
    const char *finish = json_object_get_string(choice, "finish_reason", NULL);
    if (finish && finish[0])
        snprintf(ctx->resp->finish_reason, sizeof(ctx->resp->finish_reason), "%s", finish);

    /* Extract delta content */
    const char *content = json_object_get_string(delta, "content", NULL);
    if (content && content[0]) {
        /* P95: Track first token timing */
        if (!ctx->first_token_flag) {
            ctx->first_token_flag = true;
            ctx->resp->diag.first_token_time = mono_time();
            ctx->resp->diag.first_token_received = true;
            ctx->resp->diag.time_to_first_token =
                ctx->resp->diag.first_token_time - ctx->req_start_time;
        }
        ctx->token_count++;
        /* Append to accumulated content */
        size_t cur = ctx->resp->content ? strlen(ctx->resp->content) : 0;
        size_t add = strlen(content);
        char *newc = (char *)realloc(ctx->resp->content, cur + add + 1);
        if (newc) {
            ctx->resp->content = newc;
            memcpy(ctx->resp->content + cur, content, add + 1);
        }
        /* Call token callback */
        if (ctx->token_cb)
            ctx->token_cb(content, ctx->userdata);
    }

    /* Extract delta tool_calls (streaming builds them across chunks) */
    json_node_t *tc_delta = json_object_get(delta, "tool_calls");
    if (tc_delta && json_array_count(tc_delta) > 0) {
        for (size_t i = 0; i < json_array_count(tc_delta); i++) {
            json_node_t *tc = json_array_get(tc_delta, i);
            int idx = (int)json_object_get_number(tc, "index", 0);
            if (idx >= 64) continue;

            /* Track highest index */
            if (idx >= ctx->max_tc_idx) ctx->max_tc_idx = idx + 1;

            /* id (only present in first chunk for this tool_call) */
            const char *id = json_object_get_string(tc, "id", NULL);
            if (id && id[0]) snprintf(ctx->tc_id[idx], sizeof(ctx->tc_id[idx]), "%s", id);

            /* function delta */
            json_node_t *fn = json_object_get(tc, "function");
            if (fn) {
                const char *name = json_object_get_string(fn, "name", NULL);
                if (name && name[0]) snprintf(ctx->tc_name[idx], sizeof(ctx->tc_name[idx]), "%s", name);

                const char *args_chunk = json_object_get_string(fn, "arguments", NULL);
                if (args_chunk && args_chunk[0]) {
                    size_t cur = strlen(ctx->tc_args[idx]);
                    size_t add = strlen(args_chunk);
                    if (cur + add < sizeof(ctx->tc_args[idx]) - 1) {
                        memcpy(ctx->tc_args[idx] + cur, args_chunk, add);
                        ctx->tc_args[idx][cur + add] = '\0';
                    }
                }
            }
        }
    }

    /* Extract reasoning (some providers send it in chunks too) */
    const char *reasoning = json_object_get_string(delta, "reasoning", NULL);
    if (!reasoning) reasoning = json_object_get_string(delta, "reasoning_content", NULL);
    if (reasoning && reasoning[0]) {
        size_t cur = ctx->resp->reasoning ? strlen(ctx->resp->reasoning) : 0;
        size_t add = strlen(reasoning);
        char *newr = (char *)realloc(ctx->resp->reasoning, cur + add + 1);
        if (newr) {
            ctx->resp->reasoning = newr;
            memcpy(ctx->resp->reasoning + cur, reasoning, add + 1);
        }
    }

    /* L07: xAI encrypted reasoning content in streaming delta */
    if (!ctx->resp->encrypted_content) {
        json_node_t *enc_content = json_object_get(delta, "encrypted_content");
        if (enc_content && json_array_count(enc_content) > 0) {
            char *ser = json_serialize(enc_content);
            if (ser) ctx->resp->encrypted_content = ser;
        }
    }

    /* Extract usage if present (final chunk with finish_reason) */
    if (finish && finish[0]) {
        json_node_t *usage = json_object_get(root, "usage");
        if (usage) {
            ctx->resp->input_tokens = (int)json_object_get_number(usage, "prompt_tokens", 0);
            ctx->resp->output_tokens = (int)json_object_get_number(usage, "completion_tokens", 0);
        }
    }

    json_free(root);
    return 0;
}

/* Build tool_calls array from accumulated streaming data */
static void finalize_stream_toolcalls(stream_ctx_t *ctx) {
    ctx->resp->tool_calls_count = ctx->max_tc_idx;
    for (int i = 0; i < ctx->max_tc_idx; i++) {
        snprintf(ctx->resp->tool_calls[i].id, sizeof(ctx->resp->tool_calls[i].id), "%s", ctx->tc_id[i]);
        snprintf(ctx->resp->tool_calls[i].name, sizeof(ctx->resp->tool_calls[i].name), "%s", ctx->tc_name[i]);
        snprintf(ctx->resp->tool_calls[i].arguments, sizeof(ctx->resp->tool_calls[i].arguments), "%s", ctx->tc_args[i]);
    }
}

/* P95: Populate upstream diagnostic headers from HTTP response.
 * Extracts key headers (cf-ray, x-openrouter-*, x-request-id, server)
 * from the raw HTTP response headers string into stream_diag_t. */
static void populate_stream_diag_headers(llm_response_t *resp, const char *raw_headers) {
    if (!resp || !raw_headers) return;
    char buf[384] = {0};
    size_t pos = 0;

    /* Header names to capture (lowercase for case-insensitive search) */
    const char *headers_to_capture[] = {
        "cf-ray",
        "x-openrouter-provider",
        "x-openrouter-model",
        "x-openrouter-id",
        "x-request-id",
        "x-vercel-id",
        "server",
        NULL
    };

    for (int i = 0; headers_to_capture[i] && pos < sizeof(buf) - 64; i++) {
        const char *name = headers_to_capture[i];
        size_t nlen = strlen(name);

        /* Case-insensitive search in raw headers */
        const char *h = raw_headers;
        while ((h = strcasestr(h, name)) != NULL) {
            const char *val = h + nlen;
            while (*val == ':' || *val == ' ') val++;
            const char *end = val;
            while (*end && *end != '\r' && *end != '\n') end++;
            size_t vlen = (size_t)(end - val);
            if (vlen > 120) vlen = 120;
            if (vlen > 0) {
                size_t remaining = sizeof(buf) - pos;
                int written = snprintf(buf + pos, remaining, "%s=%.*s ",
                                       name, (int)vlen, val);
                if (written > 0) pos += (size_t)written;
            }
            h = end; /* Continue after this match */
            break;    /* Only first occurrence */
        }
    }

    if (pos > 0) {
        /* Remove trailing space */
        if (pos > 0 && buf[pos-1] == ' ') buf[pos-1] = '\0';
        strncpy(resp->diag.upstream_headers, buf, sizeof(resp->diag.upstream_headers) - 1);
        resp->diag.upstream_headers[sizeof(resp->diag.upstream_headers) - 1] = '\0';
    }
}

/* P95: Finalize stream diagnostic info */
static void finalize_stream_diag(stream_ctx_t *ctx) {
    if (!ctx->resp) return;
    stream_diag_t *d = &ctx->resp->diag;
    d->stream_end_time = mono_time();
    d->total_tokens = ctx->token_count;
    if (d->first_token_received) {
        d->total_stream_time = d->stream_end_time - d->first_token_time;
        if (d->total_stream_time > 0.001)
            d->tokens_per_second = (double)d->total_tokens / d->total_stream_time;
    }
}

llm_response_t *llm_chat_completion_stream(llm_config_t *cfg,
                                            const message_t **messages,
                                            size_t message_count,
                                            json_node_t *tools_json,
                                            llm_token_cb_t token_cb,
                                            void *userdata) {
    llm_response_t *resp = (llm_response_t *)calloc(1, sizeof(llm_response_t));
    if (!resp) return NULL;
    resp->content = NULL;
    resp->reasoning = NULL;
    resp->input_tokens = 0;
    resp->output_tokens = 0;
    resp->tool_calls_count = 0;

    /* Create provider instance from config */
    provider_t *prov = provider_create(
        cfg->provider[0] ? cfg->provider : "openai",
        cfg->model, cfg->api_key, cfg->base_url);

    /* P91: Pass system cache state to provider */
    if (prov) prov->system_cached = cfg->system_cached;

    /* Copy LLM request params from config to provider (streaming path) */
    if (prov) {
        prov->config.max_tokens = cfg->max_tokens;
        prov->config.temperature = cfg->temperature;
        prov->config.top_p = cfg->top_p;
        prov->config.stop_count = cfg->stop_count;
        memcpy(prov->config.stop_sequences, cfg->stop_sequences,
               sizeof(prov->config.stop_sequences));
        prov->config.presence_penalty = cfg->presence_penalty;
        prov->config.frequency_penalty = cfg->frequency_penalty;
        prov->config.seed = cfg->seed;
        prov->config.logprobs = cfg->logprobs;
        prov->config.top_logprobs = cfg->top_logprobs;
        memcpy(prov->config.user, cfg->user, sizeof(prov->config.user));
        memcpy(prov->config.service_tier, cfg->service_tier,
               sizeof(prov->config.service_tier));
        memcpy(prov->config.reasoning_effort, cfg->reasoning_effort,
               sizeof(prov->config.reasoning_effort));
        memcpy(prov->config.response_format, cfg->response_format,
               sizeof(prov->config.response_format));
        memcpy(prov->config.metadata, cfg->metadata,
               sizeof(prov->config.metadata));
        memcpy(prov->config.tool_choice, cfg->tool_choice,
               sizeof(prov->config.tool_choice));
        prov->config.parallel_tool_calls = cfg->parallel_tool_calls;
        prov->config.top_k = cfg->top_k;
        prov->config.candidate_count = cfg->candidate_count;
        prov->config.json_mode = cfg->json_mode;
        prov->config.response_format_strict = cfg->response_format_strict;
        memcpy(prov->config.safety_settings, cfg->safety_settings,
               sizeof(prov->config.safety_settings));
        prov->config.max_tool_calls = cfg->max_tool_calls;
        prov->config.n = cfg->n;
    }

    if (prov && prov->ops) {
        const provider_ops_t *ops = prov->ops;
        char *url = ops->build_url(prov, cfg->base_url);
        /* P158: Verify URL is trusted for this provider's API key */
        const char *effective_key = cfg->api_key;
        if (effective_key[0] && !provider_url_is_trusted(cfg->provider, url)) {
            effective_key = "";
        }
        char *headers = ops->build_headers(prov, effective_key);
        char *body = ops->build_request_body(prov, messages, message_count, tools_json, true);
        if (!url || !headers || !body) {
            free(url); free(headers); free(body);
            provider_free(prov); free(resp); return NULL;
        }
        /* Use true streaming via http_stream_request with provider-aware callback */
        stream_ctx_t ctx;
        memset(&ctx, 0, sizeof(ctx));
        ctx.resp = resp;
        ctx.token_cb = token_cb;
        ctx.userdata = userdata;
        ctx.prov_ops = ops;
        ctx.prov = prov;
        ctx.req_start_time = mono_time(); /* P95: start timing */

        http_t *h = http_new(60);
        int r = http_stream_request(h, HTTP_POST, url,
                                    headers, body, strlen(body),
                                    on_provider_stream_chunk, &ctx);
        free(url); free(headers); free(body);
        http_free(h);

        /* P95: Finalize stream diagnostics */
        finalize_stream_diag(&ctx);

        if (r != 0 && r != -2) {
            /* Log structured stream error */
            const char *upstream = resp->diag.upstream_headers;
            if (upstream && upstream[0])
                fprintf(stderr, "[llm] Stream failed for %s/%s [upstream: %s]\n",
                        cfg->provider, cfg->model, upstream);
            else
                fprintf(stderr, "[llm] Stream failed for %s/%s\n",
                        cfg->provider, cfg->model);
            if (!resp->content)
                resp->content = strdup("HTTP stream request failed");
            provider_free(prov);
            return resp;
        }

        /* Finalize tool calls from accumulated chunks */
        finalize_stream_toolcalls(&ctx);
        provider_free(prov);
        return resp;
    }
    provider_free(prov);

    /* --- legacy fallback --- */
    json_node_t *req = json_new_object();
    json_object_set(req, "model", json_new_string(cfg->model));
    json_object_set(req, "stream", json_new_bool(true));

    /* Stream options: include usage in final chunk */
    json_node_t *stream_opts = json_new_object();
    json_object_set(stream_opts, "include_usage", json_new_bool(true));
    json_object_set(req, "stream_options", stream_opts);

    /* Messages */
    json_node_t *msgs = build_messages_json(messages, message_count);
    json_object_set(req, "messages", msgs);

    /* Add tools if provided */
    if (tools_json && json_array_count(tools_json) > 0)
        json_object_set(req, "tools", json_copy(tools_json));

    /* Serialize */
    char *body = json_serialize(req);

    /* Determine URL */
    char url[512];
    const char *base = cfg->base_url;
    if (base && strlen(base) > 0) {
        if (strstr(base, "/chat/completions"))
            snprintf(url, sizeof(url), "%s", base);
        else
            snprintf(url, sizeof(url), "%s/chat/completions", base);
    } else {
        snprintf(url, sizeof(url), "https://api.openai.com/v1/chat/completions");
    }

    /* Build auth header */
    char auth_header[512];
    const char *effective_key = cfg->api_key;
    if (effective_key[0] && !provider_url_is_trusted(cfg->provider, url)) {
        effective_key = "";
    }
    if (effective_key[0]) {
        snprintf(auth_header, sizeof(auth_header),
                "Authorization: Bearer %s\r\nContent-Type: application/json",
                 effective_key);
    } else {
        snprintf(auth_header, sizeof(auth_header),
                 "Content-Type: application/json");
    }

    /* Make streaming request */
    stream_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.resp = resp;
    ctx.token_cb = token_cb;
    ctx.userdata = userdata;
    ctx.req_start_time = mono_time(); /* P95: start timing */

    /* Make streaming request */
    http_t *h = http_new(60);
    int r = http_stream_request(h, HTTP_POST, url,
                                auth_header, body, strlen(body),
                                on_stream_chunk, &ctx);
    json_free(req);
    free(body);
    http_free(h);

    /* P95: Finalize stream diagnostics */
    finalize_stream_diag(&ctx);

    if (r != 0 && r != -2 /* aborted by callback OK */) {
        resp->content = strdup("HTTP stream request failed");
        return resp;
    }

    /* Finalize tool calls from accumulated chunks */
    finalize_stream_toolcalls(&ctx);

    return resp;
}

void llm_response_free(llm_response_t *resp) {
    if (!resp) return;
    free(resp->content);
    free(resp->reasoning);
    free(resp->encrypted_content);
    free(resp);
}

/* ================================================================
 *  P100: Background review — AI review of tool results
 * ================================================================ */

/* Review prompt template */
#define REVIEW_PROMPT_P100 \
    "Review the following tool execution results. Identify any potential issues, " \
    "suggest improvements, and note any security concerns. " \
    "Be concise. Output only your review:\n\n"

/* Perform background review of a tool result — available for future use.
 * Returns malloc'd review text, or NULL on failure.
 * Currently synchronous (runs inline). To use, call from a detached thread
 * and deliver the result via callback to avoid blocking the agent loop.
 * NOT YET WIRED: no caller in C agent_loop. */
char *llm_background_review(llm_config_t *cfg,
                             const char *tool_name,
                             const char *tool_args,
                             const char *tool_result) {
    if (!cfg || !tool_name || !tool_result) return NULL;

    /* Build review text */
    size_t total = strlen(REVIEW_PROMPT_P100) + 128 +
                   strlen(tool_name) + 4 +
                   (tool_args ? strlen(tool_args) : 0) + 1 +
                   strlen(tool_result) + 1;

    char *text = (char *)malloc(total);
    if (!text) return NULL;
    text[0] = '\0';

    strcat(text, REVIEW_PROMPT_P100);
    size_t cur = strlen(text);
    snprintf(text + cur, total - cur,
             "Tool: %s\n"
             "Arguments: %s\n"
             "Result:\n%s\n",
             tool_name, tool_args ? tool_args : "{}", tool_result);

    /* Create review messages */
    message_t *sys = message_new(MSG_SYSTEM,
        "You are a code review expert. Review tool execution results "
        "and identify issues, improvements, and security concerns.");
    message_t *user = message_new(MSG_USER, text);
    const message_t *review_msgs[2] = {sys, user};

    /* Use a fresh llm_config copy with no streaming */
    llm_config_t review_cfg;
    memcpy(&review_cfg, cfg, sizeof(review_cfg));
    review_cfg.base_url[0] = '\0';  /* will use default URL */

    llm_response_t *resp = llm_chat_completion(&review_cfg, review_msgs, 2, NULL);
    char *review = NULL;
    if (resp && resp->content) {
        review = strdup(resp->content);
    }
    llm_response_free(resp);
    message_free(sys);
    message_free(user);
    free(text);
    return review;
}
