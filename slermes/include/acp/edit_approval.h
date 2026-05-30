#ifndef HERMES_ACP_EDIT_APPROVAL_H
#define HERMES_ACP_EDIT_APPROVAL_H

/*
 * acp/edit_approval.h — ACP edit approval protocol for Hermes C.
 *
 * Pre-execution approval hook for file-mutation tools (write_file, patch).
 * Mirrors Python's acp_adapter/edit_approval.py with a simpler C dispatch
 * model: the ACP server sends an edit proposal notification and waits for
 * the client to respond via edit_approval_response method.
 *
 * Auto-approval policies (ask, workspace_session, session) skip the prompt
 * for non-sensitive paths. Sensitive paths (.git/, .ssh/, .env, id_rsa, ...)
 * always require approval regardless of policy.
 */

#include "hermes_json.h"
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Edit Proposal
 * ================================================================ */

#define ACP_EDIT_PROPOSAL_PATH_MAX 1024
#define ACP_EDIT_PROPOSAL_TEXT_MAX (256 * 1024)  /* 256KB max diff text */

typedef struct {
    char   tool_name[32];                         /* "write_file" | "patch" */
    char   path[ACP_EDIT_PROPOSAL_PATH_MAX];       /* target file path */
    char  *old_text;                               /* previous content (NULL if new file) */
    char  *new_text;                               /* proposed new content */
    char   arguments_json[16384];                  /* serialized original arguments */
} acp_edit_proposal_t;

/* Build an edit proposal from tool name and JSON arguments.
 * Returns true if proposal was built (tool is file-mutating), false if tool
 * does not need approval. On error (missing fields, read failure), sets
 * error_msg to a heap-allocated string the caller must free. */
bool acp_build_edit_proposal(
    const char *tool_name,
    json_node_t *args_node,
    acp_edit_proposal_t *proposal,
    char **error_msg
);

/* Free internal allocations of a proposal (old_text, new_text, arguments_json) */
void acp_edit_proposal_free(acp_edit_proposal_t *proposal);

/* ================================================================
 *  Auto-approval policy
 * ================================================================ */

/* Auto-approve a proposal under the given policy.
 * Policy values: "ask" (always ask), "workspace_session" (auto-approve inside
 * cwd/tmp), "session" (auto-approve everything non-sensitive).
 * Returns true if the proposal may be auto-approved. */
bool acp_should_auto_approve_edit(
    const acp_edit_proposal_t *proposal,
    const char *policy,
    const char *cwd
);

/* ================================================================
 *  Approval dispatch hook
 * ================================================================ */

/* Result of an edit approval request */
typedef enum {
    ACP_APPROVAL_PENDING,     /* waiting for client response */
    ACP_APPROVAL_GRANTED,     /* client approved */
    ACP_APPROVAL_DENIED       /* client denied */
} acp_approval_status_t;

/* Approval request state — tracks one pending approval per ACP server */
typedef struct {
    acp_approval_status_t status;
    time_t                started_at;
    double                timeout_sec;
    char                  proposal_id[64];  /* unique id for correlation */
    acp_edit_proposal_t   proposal;
} acp_approval_request_t;

/* Initialize an approval request tracker */
void acp_approval_request_init(acp_approval_request_t *req);

/* Begin a new approval request. Copies proposal data. */
void acp_approval_request_begin(
    acp_approval_request_t *req,
    const acp_edit_proposal_t *proposal,
    double timeout_sec
);

/* Resolve the request: called when client sends edit_approval_response. */
void acp_approval_request_resolve(acp_approval_request_t *req, bool granted);

/* Check if a pending request has timed out. Returns true if timed out. */
bool acp_approval_request_timed_out(const acp_approval_request_t *req);

/* ================================================================
 *  Sensitive path detection
 * ================================================================ */

/* Returns true if path is in a sensitive directory (.git/, .ssh/) or
 * has a sensitive basename (.env, id_rsa, id_ed25519). */
bool acp_is_sensitive_path(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_ACP_EDIT_APPROVAL_H */
