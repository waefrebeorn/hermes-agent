/**
 * @file hermes_api_server.h
 * @brief E01: OpenAI-compatible REST API server.
 *
 * Provides an OpenAI-compatible HTTP API endpoint on a configurable port.
 * Routes:
 *   GET  /v1/models              — list available models
 *   POST /v1/chat/completions    — chat completion (non-streaming)
 *   GET  /v1/capabilities        — machine-readable API capabilities
 *   GET  /v1/tools               — list all registered tools
 *   GET  /v1/agent/status        — current agent state
 *   GET  /v1/config              — active configuration (safe fields)
 *   GET  /v1/service/info        — service metadata (version, uptime)
 *   GET  /v1/metrics             — request/usage metrics
 *   GET  /health                 — simple health check
 *   GET  /health/detailed        — rich health + system info
 *   GET  /v1/sessions            — list sessions
 *   POST /v1/sessions            — create session
 *   GET  /v1/sessions/{id}       — get session details
 *   DELETE /v1/sessions/{id}     — delete session
 *   POST /webhook/:platform      — webhook endpoint
 *   OPTIONS /*                   — CORS preflight
 *
 * Usage:
 *   ./hermes api-server [port]
 *   api_server_start(9101, &cfg, &agent_state);
 *   // ... agent runs ...
 *   api_server_stop();
 */
#ifndef HERMES_API_SERVER_H
#define HERMES_API_SERVER_H

#include "hermes.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Start the API server on the given port in a background thread.
 * cfg and agent are used for request dispatch.
 * Returns true on success.
 */
bool api_server_start(int port, const hermes_config_t *cfg, agent_state_t *agent);

/**
 * Stop the API server and join its thread.
 */
void api_server_stop(void);

/**
 * Check if the API server is running.
 */
bool api_server_is_running(void);

/**
 * Set the port (must be called before start).
 */
void api_server_set_port(int port);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_API_SERVER_H */
