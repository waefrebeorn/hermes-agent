#ifndef HERMES_H
#define HERMES_H

/*
 * hermes.h — Master header for WuBu Hermes C implementation.
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
#define HERMES_VERSION_MAJOR 0
#define HERMES_VERSION_MINOR 14
#define HERMES_VERSION_PATCH 0
#define HERMES_VERSION "0.14.0-wubu"

/* ================================================================
 *  Core Constants
 * ================================================================ */
#define HERMES_MAX_TOOL_CALLS    90
#define HERMES_MAX_MESSAGES      4096
#define HERMES_MAX_TOOLS         256
#define HERMES_PATH_MAX          4096
#define HERMES_LINE_MAX          65536

/* ================================================================
 *  Message Types
 * ================================================================ */
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
    char           *tool_name;    /* for tool calls */
    char           *reasoning;   /* reasoning content if any */
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

typedef struct {
    char *content;
    char *reasoning;
    int   input_tokens;
    int   output_tokens;
    int   tool_calls_count;
} llm_response_t;

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
    char              hermes_home[HERMES_PATH_MAX];
} agent_state_t;

/* ================================================================
 *  Session Store (SQLite-backed)
 * ================================================================ */
typedef struct session_db_t session_db_t;

session_db_t *session_db_open(const char *path);
void          session_db_close(session_db_t *db);
bool          session_db_save(session_db_t *db, const char *session_id, 
                              const message_t *msgs, size_t count);
message_t   **session_db_load(session_db_t *db, const char *session_id, 
                              size_t *count);

/* ================================================================
 *  Config
 * ================================================================ */
typedef struct {
    char  config_path[HERMES_PATH_MAX];
    char  env_path[HERMES_PATH_MAX];
    char  model[128];
    char  provider[64];
    char  api_key[256];
    char  base_url[256];
    int   max_turns;
    bool  quiet_mode;
} hermes_config_t;

bool hermes_config_load(hermes_config_t *cfg, const char *config_dir);
bool hermes_config_load_env(hermes_config_t *cfg);

/* ================================================================
 *  Entry Points
 * ================================================================ */
int  hermes_cli_main(int argc, char **argv);
int  hermes_gateway_main(int argc, char **argv);

#endif /* HERMES_H */
