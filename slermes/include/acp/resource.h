/**
 * @file resource.h
 * @brief ACP resource link processing — convert file:// URIs, embedded blobs,
 *        and image blocks to Hermes/OpenAI-format content parts.
 *
 * ACP editors (VS Code, Zed) send attached files as resource_link or
 * embedded_resource content blocks alongside user messages. This module
 * reads local files, detects image vs text, and produces inline content
 * that the agent loop can pass to LLM providers.
 *
 * Usage:
 *   // Convert a JSON content array (from user_message params) to a text string
 *   char *text = acp_content_to_text(content_node);
 *   agent_run_conversation(&agent, text, ...);
 *   free(text);
 *
 * @see acp_adapter/server.py — _resource_link_to_parts, _content_blocks_to_openai_user_content
 */
#ifndef ACP_RESOURCE_H
#define ACP_RESOURCE_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- Constants ----------------------------------------------------------- */

/** Maximum resource size we'll inline (512 KB, matching Python). */
#define ACP_MAX_RESOURCE_BYTES (512 * 1024)

/* --- Public API ---------------------------------------------------------- */

/**
 * Convert an ACP content array (or string) into a plain text string suitable
 * for agent_run_conversation().
 *
 * Accepts three shapes:
 *   1. "string" — returned verbatim
 *   2. ["text", "resource_link", ...] — array of text/resource blocks processed
 *   3. {type, resource} — single block (uncommon but valid)
 *
 * Resource_link blocks with file:// URIs are read from disk. Image files are
 * inlined as [Attached image: ...] + data: URI header. Text files are inlined
 * with [Attached file: ...] metadata. Binary/non-file resources get a
 * descriptive placeholder.
 *
 * Returns malloc'd string. Caller must free().
 * Returns NULL on allocation failure.
 */
char *acp_content_to_text(json_node_t *content);

#ifdef __cplusplus
}
#endif

#endif /* ACP_RESOURCE_H */
