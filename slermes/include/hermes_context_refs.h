#ifndef HERMES_CONTEXT_REFS_H
#define HERMES_CONTEXT_REFS_H

/*
 * hermes_context_refs.h — @file:@folder:@git:@url reference expander.
 * Port of Python agent/context_references.py (518 lines).
 *
 * Parses @file:path, @folder:path, @diff, @staged, @git:N, @url:URL
 * references in user messages and expands them to file contents, git
 * diffs, URL content, or folder listings.
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum references per message */
#define MAX_CONTEXT_REFS 32

/* Maximum output size for expanded context (256KB) */
#define CONTEXT_REF_MAX_OUTPUT (256 * 1024)

/* Reference kinds */
typedef enum {
    CONTEXT_REF_FILE,
    CONTEXT_REF_FOLDER,
    CONTEXT_REF_DIFF,
    CONTEXT_REF_STAGED,
    CONTEXT_REF_GIT,
    CONTEXT_REF_URL,
    CONTEXT_REF_UNKNOWN,
} context_ref_kind_t;

/* Single parsed reference */
typedef struct {
    context_ref_kind_t kind;
    char raw[256];       /* original @reference text */
    char target[512];    /* resolved target (path, git count, URL) */
    int start_pos;       /* byte offset in original message */
    int end_pos;         /* byte offset after reference text */
    int line_start;      /* for @file:path:line */
    int line_end;        /* for @file:path:start-end */
} context_ref_t;

/* Expansion result */
typedef struct {
    char *result;           /* allocated: message with expansions injected, or NULL */
    char *original_message; /* copy of original message */
    context_ref_t refs[MAX_CONTEXT_REFS];
    int ref_count;
    char warnings[4096];    /* accumulated warnings */
    int injected_tokens;    /* estimated tokens injected */
    bool expanded;          /* any expansion occurred */
    bool blocked;           /* expansion blocked (exceeded limit) */
} context_ref_result_t;

/* Parse @file:@folder:@diff:@staged:@git:@url references from a message.
 * Returns number of references parsed (0 = none found, -1 = too many). */
int context_ref_parse(const char *message, context_ref_t *refs, int max_refs);

/* Expand all references in one pass.
 * Returns allocated context_ref_result_t (caller must free with context_ref_free()).
 * cwd: working directory for relative paths.
 * context_length: max context length in tokens (used for soft/hard limits).
 * allowed_root: optional root for security (NULL = use cwd). */
context_ref_result_t *context_ref_expand(const char *message,
                                          const char *cwd,
                                          int context_length,
                                          const char *allowed_root);

/* Free a result */
void context_ref_free(context_ref_result_t *result);

/* Estimate tokens (rough: ~4 chars per token) */
int context_ref_estimate_tokens(const char *text);

/* URL fetcher callback type */
typedef char *(*context_ref_url_fetcher_t)(const char *url);
void context_ref_set_url_fetcher(context_ref_url_fetcher_t fetcher);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_CONTEXT_REFS_H */
