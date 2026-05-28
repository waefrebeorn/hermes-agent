#ifndef HERMES_AUXILIARY_CLIENT_H
#define HERMES_AUXILIARY_CLIENT_H

#include "hermes.h"

/*
 * auxiliary_client.c — Resolve auxiliary task provider/model config.
 *
 * Each auxiliary task (vision, compression, web_extract, etc.) has a
 * config block with provider="auto" by default.  This module resolves
 * "auto" to the main agent's configured provider and populates an
 * llm_config_t suitable for llm_chat_completion().
 *
 * Resolution logic (matching Python auxiliary_client.py):
 *   1. If task_cfg->provider is "auto" or empty → use main provider
 *   2. If task_cfg->provider is explicit → use that provider + task settings
 *   3. If task_cfg->model/ base_url/ api_key are set → override main settings
 */

/*
 * Resolve an auxiliary task config into an llm_config_t.
 *
 * @param cfg         Global hermes config (for main provider/model/api_key).
 * @param task_cfg    The specific auxiliary task config (vision, compression, etc.).
 * @param out         [out] Populated llm_config_t.
 * @return true on success, false if resolution fails (no valid provider).
 */
bool auxiliary_resolve_llm_config(const hermes_config_t *cfg,
                                   const auxiliary_task_config_t *task_cfg,
                                   llm_config_t *out);

/*
 * Convenience: resolve auxiliary task by name string.
 * Looks up cfg->auxiliary.<name> and calls auxiliary_resolve_llm_config.
 *
 * @param cfg       Global hermes config.
 * @param task_name Task name string, e.g. "compression", "vision", "web_extract".
 * @param out       [out] Populated llm_config_t.
 * @return true on success.
 */
bool auxiliary_resolve_task(const hermes_config_t *cfg,
                             const char *task_name,
                             llm_config_t *out);

/*
 * Return a static description string for the task name (for logging, errors).
 */
const char *auxiliary_task_label(const char *task_name);

#endif /* HERMES_AUXILIARY_CLIENT_H */
