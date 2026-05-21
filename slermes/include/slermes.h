#ifndef SLERMES_H
#define SLERMES_H

/*
 * slermes.h — Master header for WuBu Slermes C implementation.
 * Defines core types, config, and forward declarations.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 *  Version
 * ================================================================ */
#define SLERMES_VERSION_MAJOR 0
#define SLERMES_VERSION_MINOR 14
#define SLERMES_VERSION_PATCH 0
#define SLERMES_VERSION "0.14.0-wubu"

/* ================================================================
 *  Core Constants
 * ================================================================ */
#define SLERMES_MAX_TOOL_CALLS    90
#define SLERMES_MAX_MESSAGES      4096
#define SLERMES_MAX_TOOLS         256
#define SLERMES_PATH_MAX          4096
#define SLERMES_LINE_MAX          65536
#define SLERMES_MAX_CTX_TOKENS    131072

/* ================================================================
 *  Message Types
 * ================================================================ */

/* Tool call metadata (must be before message_t which uses it) */
typedef struct {
    char  id[64];
    char  name[128];
    char  arguments[4096]; /* JSON args string */
} tool_call_t;

typedef enum {
    MSG_SYSTEM,
    MSG_USER,
    MSG_ASSISTANT,
    MSG_TOOL,
} message_role_t;

typedef struct {
    message_role_t  role;
    char           *content;     /* text content */
    char           *tool_call_id; /* for tool results */
    char           *tool_name;    /* for tool calls (single) */
    char           *reasoning;   /* reasoning content if any */
    int             tool_calls_count;
    tool_call_t     tool_calls[64];
} message_t;

/* ================================================================
 *  Tool System
 * ================================================================ */
typedef struct {
    char   name[64];
    char   description[512];
    char   schema_json[4096];
    char *(*handler)(const char *args_json, const char *task_id);
    bool  available;
} tool_t;

typedef struct {
    tool_t *tools;
    size_t  count;
    size_t  capacity;
} tool_registry_t;

/* ================================================================
 *  LLM Provider
 * ================================================================ */
typedef struct {
    char  base_url[256];
    char  api_key[256];
    char  model[128];
    char  provider[64];
} llm_config_t;

/* LLM response */
typedef struct {
    char         *content;
    char         *reasoning;
    int           input_tokens;
    int           output_tokens;
    int           tool_calls_count;
    tool_call_t   tool_calls[64]; /* Max 64 tool calls per turn */
} llm_response_t;

/* Forward declaration for session database (defined in slermes_db.h) */
typedef struct db_t db_t;

/* Streaming output callback. Called with each token content during LLM response.
 * Return non-zero to abort. */
typedef int (*llm_token_cb_t)(const char *token, void *userdata);

/* ================================================================
 *  Agent State
 * ================================================================ */
typedef struct {
    message_t       **messages;
    size_t            message_count;
    size_t            message_capacity;
    llm_config_t      llm;
    tool_registry_t   tools;
    int               max_iterations;
    int               iteration_count;
    bool              interrupted;
    char              session_id[64];
    char              slermes_home[SLERMES_PATH_MAX];
    db_t             *db;           /* session database (optional) */
    llm_token_cb_t    stream_cb;   /* streaming token callback (optional) */
    void             *stream_data; /* userdata for stream callback */
} agent_state_t;

/* ================================================================
 *  Config
 * ================================================================ */
typedef struct {
    char  config_path[SLERMES_PATH_MAX];
    char  env_path[SLERMES_PATH_MAX];
    char  model[128];
    char  provider[64];
    char  api_key[256];
    char  base_url[256];
    char  skin_path[SLERMES_PATH_MAX];
    int   max_turns;
    bool  quiet_mode;
} slermes_config_t;

bool slermes_config_load(slermes_config_t *cfg, const char *config_dir);
bool slermes_config_load_env(slermes_config_t *cfg);

/* ================================================================
 *  Include sub-headers for dependency wrappers
 * ================================================================ */
#include "slermes_json.h"
#include "slermes_yaml.h"
#include "slermes_http.h"
#include "slermes_crypto.h"
#include "slermes_db.h"
#include "slermes_display.h"
#include "slermes_skin.h"
#include "slermes_agent.h"

/* ================================================================
 *  Entry Points
 * ================================================================ */
int  slermes_cli_main(int argc, char **argv);
int  slermes_gateway_main(int argc, char **argv);

/* Command system */
typedef struct command_def_t command_def_t;
bool commands_dispatch(const char *input, agent_state_t *state);
const command_def_t *commands_resolve(const char *input);
const command_def_t *commands_get_all(void);

/* Security approval */
int approval_check(const char *tool_name, const char *args_json);
void approval_reset_session(void);

#endif /* SLERMES_H */
