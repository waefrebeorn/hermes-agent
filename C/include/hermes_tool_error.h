#ifndef HERMES_TOOL_ERROR_H
#define HERMES_TOOL_ERROR_H

/*
 * hermes_tool_error.h — Tool error sanitization for Hermes C.
 * Port of Python model_tools._sanitize_tool_error().
 *
 * Strips structural framing tokens (XML role tags, CDATA, markdown code
 * fences) from tool error messages before they enter the conversation
 * context, preventing role-confusion framing and downstream parsing
 * confusion.
 *
 * Defense-in-depth: the JSON layer already prevents framing escape,
 * but stripping these tokens from the text the model sees is cheap
 * and worth having.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

/**
 * Maximum length of sanitized tool error message.
 * Port of Python _TOOL_ERROR_MAX_LEN = 2000.
 */
#define TOOL_ERROR_MAX_LEN 2000

/**
 * Sanitize a tool error message by stripping structural framing tokens.
 *
 * Strips:
 *   - XML role tags: <tool_call>, </function_call>, <result>, etc.
 *   - Markdown code fence opens: ```, ```json, ```xml, etc.
 *   - Markdown code fence closes: trailing ```
 *   - CDATA sections: <![CDATA[...]]>
 *
 * If the result exceeds TOOL_ERROR_MAX_LEN, it is truncated with "...".
 * Always prepends "[TOOL_ERROR] " prefix.
 *
 * @param error_msg  Input error message (may be NULL or empty).
 * @return Malloc'd sanitized string. Caller must free.
 *         Returns strdup("[TOOL_ERROR] ") on NULL/empty input.
 */
char *tool_error_sanitize(const char *error_msg);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_TOOL_ERROR_H */
