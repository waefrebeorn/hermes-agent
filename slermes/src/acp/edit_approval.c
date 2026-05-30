/*
 * edit_approval.c — ACP edit approval protocol for Hermes C.
 *
 * Pre-execution approval hook for file-mutation tools (write_file, patch).
 */
#include "acp/edit_approval.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ================================================================
 *  Sensitive path detection
 * ================================================================ */

static const char *SENSITIVE_BASENAMES[] = {
    ".env", ".env.local", ".env.production", "id_rsa", "id_ed25519",
    NULL
};

static const char *SENSITIVE_DIR_PARTS[] = {
    ".git", ".ssh", NULL
};

bool acp_is_sensitive_path(const char *path) {
    if (!path || !*path) return false;

    /* Check basename */
    const char *basename = strrchr(path, '/');
    basename = basename ? basename + 1 : path;

    for (int i = 0; SENSITIVE_BASENAMES[i]; i++) {
        if (strcmp(basename, SENSITIVE_BASENAMES[i]) == 0)
            return true;
    }

    /* Check path parts for sensitive directories */
    char path_copy[ACP_EDIT_PROPOSAL_PATH_MAX];
    snprintf(path_copy, sizeof(path_copy), "%s", path);
    char *lower = malloc(strlen(path_copy) + 1);
    if (!lower) return false;

    for (size_t i = 0; path_copy[i]; i++)
        lower[i] = (char)tolower((unsigned char)path_copy[i]);
    lower[strlen(path_copy)] = '\0';

    bool sensitive = false;
    for (int i = 0; SENSITIVE_DIR_PARTS[i]; i++) {
        /* Check if "/.git/" or "/.ssh/" appears in path */
        char needle[32];
        snprintf(needle, sizeof(needle), "/%s/", SENSITIVE_DIR_PARTS[i]);
        if (strstr(lower, needle)) {
            /* Verify case-insensitively by scanning original */
            if (strstr(path_copy, SENSITIVE_DIR_PARTS[i]) ||
                strstr(path_copy, SENSITIVE_DIR_PARTS[i])) {
                sensitive = true;
                break;
            }
        }
        /* Also check starts-with */
        snprintf(needle, sizeof(needle), "%s/", SENSITIVE_DIR_PARTS[i]);
        if (strncmp(lower, needle, strlen(needle)) == 0) {
            sensitive = true;
            break;
        }
    }

    free(lower);
    return sensitive;
}

/* ================================================================
 *  File content helpers
 * ================================================================ */

static char *read_file_if_exists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size < 0) { fclose(f); return NULL; }
    if (size > (long)ACP_EDIT_PROPOSAL_TEXT_MAX) {
        fclose(f);
        fprintf(stderr, "[acp-edit] File too large for proposal diff: %s (%ld bytes)\n",
                path, size);
        return NULL;
    }
    rewind(f);

    char *content = (char *)malloc((size_t)size + 1);
    if (!content) { fclose(f); return NULL; }

    size_t nread = fread(content, 1, (size_t)size, f);
    fclose(f);
    content[nread] = '\0';
    return content;
}

/* ================================================================
 *  Build edit proposals
 * ================================================================ */

static bool proposal_for_write_file(json_node_t *args, acp_edit_proposal_t *proposal, char **error) {
    const char *path = json_object_get_string(args, "path", NULL);
    if (!path || !*path) {
        *error = strdup("path required for write_file");
        return false;
    }

    json_node_t *content_node = json_object_get(args, "content");
    const char *content = NULL;
    if (content_node) {
        if (content_node->type == JSON_STRING)
            content = content_node->str_val;
    }
    if (!content) {
        *error = strdup("content required for write_file");
        return false;
    }

    snprintf(proposal->tool_name, sizeof(proposal->tool_name), "write_file");
    snprintf(proposal->path, sizeof(proposal->path), "%s", path);
    proposal->old_text = read_file_if_exists(path);
    proposal->new_text = strdup(content);

    /* Serialize arguments */
    char *ser = json_serialize(args);
    if (ser) {
        snprintf(proposal->arguments_json, sizeof(proposal->arguments_json), "%s", ser);
        free(ser);
    } else {
        proposal->arguments_json[0] = '\0';
    }

    return true;
}

static bool proposal_for_patch_replace(json_node_t *args, acp_edit_proposal_t *proposal, char **error) {
    const char *path = json_object_get_string(args, "path", NULL);
    if (!path || !*path) {
        *error = strdup("path required for patch");
        return false;
    }

    const char *old_string = json_object_get_string(args, "old_string", NULL);
    const char *new_string = json_object_get_string(args, "new_string", NULL);
    if (!old_string || !new_string) {
        *error = strdup("old_string and new_string required for patch");
        return false;
    }

    char *old_text = read_file_if_exists(path);
    if (!old_text) {
        *error = strdup("Could not read file for patch diff");
        return false;
    }

    snprintf(proposal->tool_name, sizeof(proposal->tool_name), "patch");
    snprintf(proposal->path, sizeof(proposal->path), "%s", path);
    proposal->old_text = old_text;
    proposal->new_text = strdup(new_string);

    char *ser = json_serialize(args);
    if (ser) {
        snprintf(proposal->arguments_json, sizeof(proposal->arguments_json), "%s", ser);
        free(ser);
    } else {
        proposal->arguments_json[0] = '\0';
    }

    return true;
}

/* ================================================================
 *  Public API
 * ================================================================ */

bool acp_build_edit_proposal(
    const char *tool_name,
    json_node_t *args_node,
    acp_edit_proposal_t *proposal,
    char **error_msg
) {
    if (!tool_name || !args_node || !proposal) {
        if (error_msg) *error_msg = strdup("Invalid arguments");
        return false;
    }

    memset(proposal, 0, sizeof(*proposal));

    if (strcmp(tool_name, "write_file") == 0) {
        return proposal_for_write_file(args_node, proposal, error_msg);
    }
    if (strcmp(tool_name, "patch") == 0) {
        const char *mode = json_object_get_string(args_node, "mode", "replace");
        if (strcmp(mode, "replace") == 0) {
            return proposal_for_patch_replace(args_node, proposal, error_msg);
        }
    }

    /* Not a file-mutation tool — no proposal needed */
    return false;
}

void acp_edit_proposal_free(acp_edit_proposal_t *proposal) {
    if (!proposal) return;
    free(proposal->old_text);
    free(proposal->new_text);
    proposal->old_text = NULL;
    proposal->new_text = NULL;
}

bool acp_should_auto_approve_edit(
    const acp_edit_proposal_t *proposal,
    const char *policy,
    const char *cwd
) {
    if (!proposal || !policy)
        return false;

    /* Sensitive paths always require approval */
    if (acp_is_sensitive_path(proposal->path))
        return false;

    if (strcmp(policy, "ask") == 0)
        return false;

    if (strcmp(policy, "session") == 0)
        return true;

    if (strcmp(policy, "workspace_session") == 0) {
        /* Auto-approve if in /tmp or under cwd */
        if (strncmp(proposal->path, "/tmp/", 5) == 0)
            return true;
        if (strncmp(proposal->path, "/private/tmp/", 13) == 0)
            return true;
        if (cwd && *cwd) {
            size_t cwd_len = strlen(cwd);
            if (strncmp(proposal->path, cwd, cwd_len) == 0 &&
                (proposal->path[cwd_len] == '/' || proposal->path[cwd_len] == '\0'))
                return true;
        }
    }

    return false;
}

/* ================================================================
 *  Approval request state
 * ================================================================ */

void acp_approval_request_init(acp_approval_request_t *req) {
    if (!req) return;
    memset(req, 0, sizeof(*req));
    req->status = ACP_APPROVAL_PENDING;
}

void acp_approval_request_begin(
    acp_approval_request_t *req,
    const acp_edit_proposal_t *proposal,
    double timeout_sec
) {
    if (!req || !proposal) return;

    /* Generate unique proposal ID */
    static int counter = 0;
    counter++;
    snprintf(req->proposal_id, sizeof(req->proposal_id),
             "edit-approval-%d", counter);

    req->status = ACP_APPROVAL_PENDING;
    req->started_at = time(NULL);
    req->timeout_sec = timeout_sec;

    /* Copy proposal data */
    acp_edit_proposal_free(&req->proposal);
    memcpy(&req->proposal, proposal, sizeof(req->proposal));
    req->proposal.old_text = proposal->old_text ? strdup(proposal->old_text) : NULL;
    req->proposal.new_text = proposal->new_text ? strdup(proposal->new_text) : NULL;
}

void acp_approval_request_resolve(acp_approval_request_t *req, bool granted) {
    if (!req) return;
    req->status = granted ? ACP_APPROVAL_GRANTED : ACP_APPROVAL_DENIED;
}

bool acp_approval_request_timed_out(const acp_approval_request_t *req) {
    if (!req || req->status != ACP_APPROVAL_PENDING)
        return false;
    if (req->timeout_sec <= 0)
        return false;
    double elapsed = (double)(time(NULL) - req->started_at);
    return elapsed >= req->timeout_sec;
}
