/*
 * config.c — Config loading for Hermes C.
 * Reads ~/.hermes/config.yaml + ~/.hermes/.env
 * Merges env vars over YAML.
 */

#include "hermes.h"
#include "hermes_yaml.h"
#include "hermes_json.h"
#include "provider_metadata.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>

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

/* A03: Expand ${VAR:-default} env var references in YAML string values.
 * Scans for ${...} patterns, looks up the env var, substitutes its value
 * or the :-default if the var is unset. Returns malloc'd result.
 * Caller must free(). */
static char *config_expand_env_vars(const char *input) {
    if (!input) return NULL;
    size_t cap = strlen(input) + 1;
    char *result = (char *)malloc(cap);
    if (!result) return NULL;
    const char *p = input;
    size_t pos = 0;
    while (*p) {
        if (*p == '$' && *(p + 1) == '{') {
            const char *start = p + 2;
            const char *end = strchr(start, '}');
            if (!end) {
                /* No closing brace — copy as literal */
                result[pos++] = *p++;
                continue;
            }
            /* Find ':-' separator for default value */
            const char *colon_dash = NULL;
            const char *scan = start;
            while (scan < end) {
                if (*scan == ':' && (scan + 1 < end) && *(scan + 1) == '-') {
                    colon_dash = scan;
                    break;
                }
                scan++;
            }
            const char *var_name = start;
            size_t var_len;
            const char *default_val = NULL;
            size_t default_len = 0;
            if (colon_dash) {
                var_len = (size_t)(colon_dash - start);
                default_val = colon_dash + 2;
                default_len = (size_t)(end - default_val);
            } else {
                var_len = (size_t)(end - start);
            }
            /* Look up env var */
            char var_buf[256];
            size_t vn = var_len < sizeof(var_buf) - 1 ? var_len : sizeof(var_buf) - 1;
            memcpy(var_buf, var_name, vn);
            var_buf[vn] = '\0';
            const char *env_val = getenv(var_buf);
            const char *subst;
            size_t subst_len;
            if (env_val && env_val[0]) {
                subst = env_val;
                subst_len = strlen(env_val);
            } else if (default_val) {
                subst = default_val;
                subst_len = default_len;
            } else {
                subst = NULL;
                subst_len = 0;
            }
            if (subst) {
                /* Grow buffer if needed */
                size_t needed = pos + subst_len + 1;
                if (needed > cap) {
                    cap = needed + 256;
                    char *new_r = (char *)realloc(result, cap);
                    if (!new_r) { free(result); return NULL; }
                    result = new_r;
                }
                memcpy(result + pos, subst, subst_len);
                pos += subst_len;
            }
            p = end + 1; /* Skip past } */
        } else {
            result[pos++] = *p++;
        }
    }
    result[pos] = '\0';
    return result;
}

/* A04: Preprocess YAML file to resolve !include directives.
 * Scans file lines, replaces !include <path> with the content
 * of the referenced file (relative to the including file's dir).
 * Returns malloc'd preprocessed YAML, or strdup of original on error. */
static char *config_resolve_includes(const char *filepath) {
    FILE *f = fopen(filepath, "r");
    if (!f) return NULL;

    /* Read entire file */
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize <= 0) { fclose(f); return NULL; }

    char *content = (char *)malloc((size_t)fsize + 16384);
    if (!content) { fclose(f); return NULL; }
    size_t total = fread(content, 1, (size_t)fsize, f);
    fclose(f);
    content[total] = '\0';

    /* Directory of the including file */
    char dir[4096];
    const char *last_slash = strrchr(filepath, '/');
    if (last_slash) {
        size_t dlen = (size_t)(last_slash - filepath);
        memcpy(dir, filepath, dlen);
        dir[dlen] = '\0';
    } else {
        snprintf(dir, sizeof(dir), ".");
    }

    /* Process line by line */
    char *result = (char *)malloc((size_t)fsize + 32768);
    if (!result) { free(content); return NULL; }
    result[0] = '\0';
    size_t pos = 0;

    char *line_start = content;
    while (line_start && *line_start) {
        char *newline = strchr(line_start, '\n');
        size_t line_len = newline ? (size_t)(newline - line_start) : strlen(line_start);

        /* Trim leading whitespace */
        const char *trimmed = line_start;
        size_t indent = 0;
        while (*trimmed == ' ' || *trimmed == '\t') { trimmed++; indent++; }

        /* Check for !include directive */
        if (strncmp(trimmed, "!include ", 9) == 0) {
            const char *include_path = trimmed + 9;
            /* Trim trailing whitespace */
            char inc_path[4096];
            size_t ip = 0;
            while (*include_path && *include_path != '\n' && *include_path != '\r'
                   && ip < sizeof(inc_path) - 1) {
                inc_path[ip++] = *include_path++;
            }
            inc_path[ip] = '\0';

            /* Resolve relative path */
            char full_path[4096];
            if (inc_path[0] == '/') {
                snprintf(full_path, sizeof(full_path), "%s", inc_path);
            } else {
                snprintf(full_path, sizeof(full_path), "%s/%s", dir, inc_path);
            }

            /* Read included file content */
            FILE *inc_f = fopen(full_path, "r");
            if (!inc_f) {
                /* File not found — output a comment instead of include */
                size_t needed = pos + 60 + line_len;
                if (needed > (size_t)fsize + 32768) break;
                pos += snprintf(result + pos, (size_t)fsize + 32768 - pos,
                                "# INCLUDE NOT FOUND: %s\n", inc_path);
            } else {
                fseek(inc_f, 0, SEEK_END);
                long inc_size = ftell(inc_f);
                fseek(inc_f, 0, SEEK_SET);

                if (inc_size > 0) {
                    char *inc_content = (char *)malloc((size_t)inc_size + 1);
                    if (inc_content) {
                        size_t nread = fread(inc_content, 1, (size_t)inc_size, inc_f);
                        inc_content[nread] = '\0';

                        /* Indent each line of included content */
                        char *inc_line = inc_content;
                        while (inc_line && *inc_line) {
                            char *inc_nl = strchr(inc_line, '\n');
                            size_t inc_ll = inc_nl ? (size_t)(inc_nl - inc_line) : strlen(inc_line);
                            size_t needed = pos + indent + inc_ll + 2;
                            if (needed > (size_t)fsize + 32768) break;
                            /* Add indentation */
                            for (size_t si = 0; si < indent && pos < (size_t)fsize + 32768 - 1; si++)
                                result[pos++] = ' ';
                            /* Copy line content */
                            if (inc_ll > 0 && pos < (size_t)fsize + 32768 - inc_ll) {
                                memcpy(result + pos, inc_line, inc_ll);
                                pos += inc_ll;
                            }
                            /* Add newline */
                            if (pos < (size_t)fsize + 32768 - 1)
                                result[pos++] = '\n';
                            inc_line = inc_nl ? inc_nl + 1 : NULL;
                        }
                        free(inc_content);
                    }
                }
                fclose(inc_f);
            }
        } else {
            /* Copy line as-is */
            if (pos + line_len + 2 <= (size_t)fsize + 32768) {
                memcpy(result + pos, line_start, line_len);
                pos += line_len;
                result[pos++] = '\n';
            }
        }

        line_start = newline ? newline + 1 : NULL;
    }

    result[pos] = '\0';
    free(content);
    return result;
}

/* Resolve SLERMES_HOME. Default: ~/.slermes — delegates to hermes_get_home() */
static void get_slermes_home(char *buf, size_t sz) {
    hermes_get_home(buf, sz);
}

/* ================================================================
 *  .env file parser (KEY=VALUE, one per line, # comments)
 * ================================================================ */

static void parse_env_file(const char *path, hermes_config_t *cfg) {
    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        /* Strip trailing newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';

        /* Skip empty/comment lines */
        const char *p = line;
        while (*p == ' ') p++;
        if (*p == '#' || *p == '\0') continue;

        /* Find '=' */
        const char *eq = strchr(p, '=');
        if (!eq) continue;

        size_t key_len = (size_t)(eq - p);
        if (key_len == 0) continue;

        const char *val = eq + 1;

        /* Match known keys and update config */
        if (strncmp(p, "HERMES_API_KEY", key_len) == 0 && key_len == 14) {
            snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", val);
            snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", val);
        }
        else if (strncmp(p, "OPENAI_API_KEY", key_len) == 0 && key_len == 14) {
            if (cfg->api_key[0] == '\0') {
                snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", val);
                snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", val);
            }
        }
        else if (strncmp(p, "ANTHROPIC_API_KEY", key_len) == 0 && key_len == 17) {
            /* Store for anthropic provider; also keep as generic fallback */
            if (cfg->api_key[0] == '\0') {
                snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", val);
                snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", val);
            }
        }
        else if (strncmp(p, "GOOGLE_API_KEY", key_len) == 0 && key_len == 14) {
            if (cfg->api_key[0] == '\0') {
                snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", val);
                snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", val);
            }
        }
        else if (strncmp(p, "DEEPSEEK_API_KEY", key_len) == 0 && key_len == 16) {
            if (cfg->api_key[0] == '\0') {
                snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", val);
                snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", val);
            }
            snprintf(cfg->provider_cfg.deepseek_api_key, sizeof(cfg->provider_cfg.deepseek_api_key), "%s", val);
        }
        else if (strncmp(p, "SLERMES_API_KEY", key_len) == 0 && key_len == 15) {
            if (cfg->api_key[0] == '\0') {
                snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", val);
                snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", val);
            }
        }
        else if (strncmp(p, "HERMES_MODEL", key_len) == 0 && key_len == 12) {
            snprintf(cfg->model, sizeof(cfg->model), "%s", val);
            snprintf(cfg->provider_cfg.model, sizeof(cfg->provider_cfg.model), "%s", val);
        }
        else if (strncmp(p, "HERMES_PROVIDER", key_len) == 0 && key_len == 15) {
            snprintf(cfg->provider, sizeof(cfg->provider), "%s", val);
            snprintf(cfg->provider_cfg.provider, sizeof(cfg->provider_cfg.provider), "%s", val);
        }
        else if (strncmp(p, "HERMES_BASE_URL", key_len) == 0 && key_len == 15) {
            snprintf(cfg->base_url, sizeof(cfg->base_url), "%s", val);
            snprintf(cfg->provider_cfg.base_url, sizeof(cfg->provider_cfg.base_url), "%s", val);
        }
        else if (strncmp(p, "SLERMES_HOME", key_len) == 0 && key_len == 12) {
            /* Handled by get_slermes_home() */
        }
    }
    fclose(f);
}

/* ================================================================
 *  Config file loading
 * ================================================================ */

bool hermes_config_load(hermes_config_t *cfg, const char *config_dir) {
    /* Set defaults */
    memset(cfg, 0, sizeof(*cfg));
    cfg->max_turns = 90;
    cfg->quiet_mode = false;
    cfg->verbose = 0;
    cfg->yolo_mode = false;
    cfg->fast_mode = false;
    cfg->compress_enabled = false;

    /* Provider config defaults */
    cfg->provider_cfg.max_tokens = 4096;
    cfg->provider_cfg.temperature = 1.0f;
    cfg->provider_cfg.top_p = 1.0f;
    cfg->provider_cfg.stop_count = 0;
    cfg->provider_cfg.presence_penalty = 0.0f;
    cfg->provider_cfg.frequency_penalty = 0.0f;
    cfg->provider_cfg.seed = -1;
    cfg->provider_cfg.logprobs = false;
    cfg->provider_cfg.top_logprobs = 0;
    cfg->provider_cfg.user[0] = '\0';
    snprintf(cfg->provider_cfg.api_mode, sizeof(cfg->provider_cfg.api_mode), "chat_completions");
    snprintf(cfg->provider_cfg.service_tier, sizeof(cfg->provider_cfg.service_tier), "default");
    snprintf(cfg->provider_cfg.reasoning_effort, sizeof(cfg->provider_cfg.reasoning_effort), "medium");
    cfg->provider_cfg.response_format[0] = '\0';
    cfg->provider_cfg.metadata[0] = '\0';
    cfg->provider_cfg.tool_choice[0] = '\0';
    cfg->provider_cfg.parallel_tool_calls = true;
    cfg->provider_cfg.json_mode = false;
    cfg->provider_cfg.response_format_strict = false;
    cfg->provider_cfg.safety_settings[0] = '\0';
    cfg->provider_cfg.max_tool_calls = 0;
    cfg->provider_cfg.n = 1;
    cfg->provider_cfg.top_k = 0;
    cfg->provider_cfg.candidate_count = 0;
    cfg->provider_cfg.azure_deployment_id[0] = '\0';
    cfg->provider_cfg.azure_api_version[0] = '\0';
    cfg->provider_cfg.openrouter_provider[0] = '\0';
    cfg->provider_cfg.bedrock_inference_profile[0] = '\0';
    cfg->provider_cfg.bedrock_guardrail_config[0] = '\0';
    cfg->provider_cfg.bedrock_trace_enabled = false;
    cfg->provider_cfg.deepseek_cache_ttl = 0;  /* 0 = default 300s */

    /* Agent config defaults */
    cfg->agent.max_iterations = 90;
    cfg->agent.max_tool_calls_round = 0;  /* 0 = unlimited */
    cfg->agent.max_output_tokens = 4096;
    cfg->agent.verbose_level = 0;
    cfg->agent.fast_mode = false;
    cfg->agent.quiet_mode = false;
    cfg->agent.compress_threshold = 0.38f;
    cfg->agent.api_max_retries = 3;
    cfg->agent.clarify_timeout = 300;
    snprintf(cfg->agent.reasoning_effort, sizeof(cfg->agent.reasoning_effort), "medium");

    /* Tools/terminal config defaults */
    cfg->tools.allow_background = true;
    cfg->tools.local_process_cleanup = true;
    snprintf(cfg->tools.approval_mode, sizeof(cfg->tools.approval_mode), "manual");
    cfg->tools.approval_timeout = 600;
    cfg->tools.max_result_size = 50000;
    cfg->tools.terminal_timeout = 1800;
    cfg->tools.vision_timeout = 300;
    cfg->tools.persistent_shell = true;
    cfg->tools.web_search_timeout = 30;

    /* P5-P14 config defaults */
    cfg->delegation.max_concurrent_children = 3;
    cfg->delegation.max_spawn_depth = 1;
    cfg->delegation.child_timeout = 600;
    cfg->delegation.child_max_turns = 50;

    cfg->browser_cfg.headless = true;
    cfg->browser_cfg.viewport_width = 1280;
    cfg->browser_cfg.viewport_height = 720;
    cfg->browser_cfg.timeout = 30;
    cfg->browser_cfg.enable_javascript = true;

    cfg->memory.char_limit = 2200;
    cfg->memory.user_char_limit = 1375;
    cfg->memory.ttl_days = 30;
    cfg->memory.auto_save = true;

    cfg->compression.target_ratio = 0.2f;
    cfg->compression.min_messages = 10;
    snprintf(cfg->compression.strategy, sizeof(cfg->compression.strategy), "smart");
    cfg->compression.preserve_system = true;
    cfg->compression.protect_last_n = 20;
    cfg->compression.protect_first_n = 3;
    cfg->compression.hygiene_hard_message_limit = 400;
    cfg->compression.abort_on_summary_failure = false;

    cfg->cron.max_concurrent_jobs = 5;
    cfg->cron.job_timeout = 3600;
    cfg->cron.retention_days = 30;
    cfg->cron.notify_on_failure = true;

    cfg->notification.on_complete = true;
    cfg->notification.on_error = true;
    cfg->notification.on_approval = false;

    cfg->security.tirith_timeout = 5;
    cfg->security.tirith_enabled = true;
    cfg->security.allow_private_urls = false;
    cfg->security.website_blocklist_enabled = false;

    cfg->session.retention_days = 90;
    cfg->session.auto_save_interval = 10;
    cfg->session.compress = false;
    cfg->session.store_trajectories = false;

    cfg->mcp.timeout = 120;
    cfg->mcp.max_tools = 50;
    cfg->mcp.auth_enabled = false;

    /* Terminal config defaults */
    snprintf(cfg->terminal.backend, sizeof(cfg->terminal.backend), "local");
    cfg->terminal.timeout = 180;
    cfg->terminal.persistent_shell = true;
    snprintf(cfg->terminal.cwd, sizeof(cfg->terminal.cwd), ".");
    cfg->terminal.auto_source_bashrc = true;
    cfg->terminal.container_cpu = 1;
    cfg->terminal.container_memory = 5120;
    cfg->terminal.container_disk = 51200;
    cfg->terminal.container_persistent = true;
    cfg->terminal.docker_mount_cwd = false;
    cfg->terminal.docker_run_as_host_user = false;
    snprintf(cfg->terminal.docker_image, sizeof(cfg->terminal.docker_image),
             "nikolaik/python-nodejs:python3.11-nodejs20");
    snprintf(cfg->terminal.singularity_image, sizeof(cfg->terminal.singularity_image),
             "docker://nikolaik/python-nodejs:python3.11-nodejs20");
    snprintf(cfg->terminal.modal_image, sizeof(cfg->terminal.modal_image),
             "nikolaik/python-nodejs:python3.11-nodejs20");
    snprintf(cfg->terminal.daytona_image, sizeof(cfg->terminal.daytona_image),
             "nikolaik/python-nodejs:python3.11-nodejs20");
    snprintf(cfg->terminal.vercel_runtime, sizeof(cfg->terminal.vercel_runtime), "node24");

    /* Logging config defaults */
    snprintf(cfg->logging.level, sizeof(cfg->logging.level), "info");
    snprintf(cfg->logging.format, sizeof(cfg->logging.format), "text");
    cfg->logging.max_files = 10;
    cfg->logging.max_size_mb = 50;

    /* Skills config defaults */
    cfg->skills.auto_discover = true;
    cfg->skills.bundle_size_limit = 1024;
    cfg->skills.validate_on_load = 1;

    /* Checkpoints config defaults */
    cfg->checkpoints.enabled = true;
    cfg->checkpoints.interval = 10;
    cfg->checkpoints.max_checkpoints = 5;
    cfg->checkpoints.auto_rollback = true;
    cfg->checkpoints.save_on_interrupt = true;
    cfg->checkpoints.compression_level = 1;
    cfg->checkpoints.include_tool_results = false;

    /* Secrets config defaults (L01: Bitwarden Secrets Manager) */
    cfg->secrets.enabled = false;
    cfg->secrets.access_token[0] = '\0';
    cfg->secrets.organization_id[0] = '\0';
    cfg->secrets.bws_path[0] = '\0';
    cfg->secrets.install_timeout = 30;
    snprintf(cfg->auxiliary.vision.provider, sizeof(cfg->auxiliary.vision.provider), "auto");
    cfg->auxiliary.vision.timeout = 120;
    cfg->auxiliary.vision_download_timeout = 30;
    snprintf(cfg->auxiliary.web_extract.provider, sizeof(cfg->auxiliary.web_extract.provider), "auto");
    cfg->auxiliary.web_extract.timeout = 360;
    snprintf(cfg->auxiliary.compression.provider, sizeof(cfg->auxiliary.compression.provider), "auto");
    cfg->auxiliary.compression.timeout = 120;
    snprintf(cfg->auxiliary.skills_hub.provider, sizeof(cfg->auxiliary.skills_hub.provider), "auto");
    snprintf(cfg->auxiliary.approval.provider, sizeof(cfg->auxiliary.approval.provider), "auto");
    snprintf(cfg->auxiliary.mcp.provider, sizeof(cfg->auxiliary.mcp.provider), "auto");
    snprintf(cfg->auxiliary.title_generation.provider, sizeof(cfg->auxiliary.title_generation.provider), "auto");
    snprintf(cfg->auxiliary.triage_specifier.provider, sizeof(cfg->auxiliary.triage_specifier.provider), "auto");
    cfg->auxiliary.triage_specifier.timeout = 120;
    snprintf(cfg->auxiliary.kanban_decomposer.provider, sizeof(cfg->auxiliary.kanban_decomposer.provider), "auto");
    cfg->auxiliary.kanban_decomposer.timeout = 180;
    snprintf(cfg->auxiliary.profile_describer.provider, sizeof(cfg->auxiliary.profile_describer.provider), "auto");
    cfg->auxiliary.profile_describer.timeout = 60;
    snprintf(cfg->auxiliary.curator.provider, sizeof(cfg->auxiliary.curator.provider), "auto");
    cfg->auxiliary.curator.timeout = 600;

    /* TTS config defaults */
    snprintf(cfg->tts.provider, sizeof(cfg->tts.provider), "edge");
    snprintf(cfg->tts.edge_voice, sizeof(cfg->tts.edge_voice), "en-US-AriaNeural");
    snprintf(cfg->tts.elevenlabs_voice_id, sizeof(cfg->tts.elevenlabs_voice_id), "pNInz6obpgDQGcFmaJgB");
    snprintf(cfg->tts.elevenlabs_model_id, sizeof(cfg->tts.elevenlabs_model_id), "eleven_multilingual_v2");
    snprintf(cfg->tts.openai_model, sizeof(cfg->tts.openai_model), "gpt-4o-mini-tts");
    snprintf(cfg->tts.openai_voice, sizeof(cfg->tts.openai_voice), "alloy");
    snprintf(cfg->tts.xai_voice_id, sizeof(cfg->tts.xai_voice_id), "eve");
    snprintf(cfg->tts.xai_language, sizeof(cfg->tts.xai_language), "en");
    cfg->tts.xai_sample_rate = 24000;
    cfg->tts.xai_bit_rate = 128000;
    snprintf(cfg->tts.mistral_model, sizeof(cfg->tts.mistral_model), "voxtral-mini-tts-2603");
    snprintf(cfg->tts.mistral_voice_id, sizeof(cfg->tts.mistral_voice_id), "c69964a6-ab8b-4f8a-9465-ec0925096ec8");
    snprintf(cfg->tts.neutts_model, sizeof(cfg->tts.neutts_model), "neuphonic/neutts-air-q4-gguf");
    snprintf(cfg->tts.neutts_device, sizeof(cfg->tts.neutts_device), "cpu");
    snprintf(cfg->tts.piper_voice, sizeof(cfg->tts.piper_voice), "en_US-lessac-medium");

    /* STT config defaults */
    cfg->stt.enabled = true;
    snprintf(cfg->stt.provider, sizeof(cfg->stt.provider), "local");
    snprintf(cfg->stt.local_model, sizeof(cfg->stt.local_model), "base");
    snprintf(cfg->stt.openai_model, sizeof(cfg->stt.openai_model), "whisper-1");
    snprintf(cfg->stt.mistral_model, sizeof(cfg->stt.mistral_model), "voxtral-mini-latest");

    /* Voice config defaults */
    snprintf(cfg->voice.record_key, sizeof(cfg->voice.record_key), "ctrl+b");
    cfg->voice.max_recording_seconds = 120;
    cfg->voice.auto_tts = false;
    cfg->voice.beep_enabled = true;
    cfg->voice.silence_threshold = 200;
    cfg->voice.silence_duration = 3.0f;

    /* Discord config defaults */
    cfg->discord.max_message_length = 2000;
    cfg->discord.sync_permissions = true;
    cfg->discord.slash_commands_enabled = true;
    cfg->discord.thread_auto_archive = true;
    snprintf(cfg->discord.command_prefix, sizeof(cfg->discord.command_prefix), "/");
    snprintf(cfg->discord.status, sizeof(cfg->discord.status), "online");
    snprintf(cfg->discord.activity_type, sizeof(cfg->discord.activity_type), "playing");

    /* Kanban config defaults */
    cfg->kanban.max_wip = 5;
    cfg->kanban.default_sprint_days = 14;
    cfg->kanban.auto_archive_days = 90;
    cfg->kanban.auto_sync = true;

    /* Guardrails config defaults */
    cfg->guardrails.max_consecutive_failures = 3;
    cfg->guardrails.max_tool_calls_per_turn = 10;
    cfg->guardrails.max_result_bytes = 50000;
    cfg->guardrails.abort_on_safety_violation = true;
    cfg->guardrails.rate_limit_per_minute = 60;
    cfg->guardrails.cooldown_seconds = 30;

    /* Approvals config defaults */
    snprintf(cfg->approvals.mode, sizeof(cfg->approvals.mode), "manual");
    cfg->approvals.timeout = 600;
    cfg->approvals.require_reason = false;
    cfg->approvals.notify_on_pending = true;

    /* Small platform config defaults */
    snprintf(cfg->x_search.engine, sizeof(cfg->x_search.engine), "twitter");

    cfg->model_catalog.auto_update = true;

    cfg->openrouter.api_key[0] = '\0';

    cfg->human_delay.min_ms = 0;
    cfg->human_delay.max_ms = 3000;
    cfg->human_delay.enabled = false;

    cfg->updates.check_interval = 24;
    snprintf(cfg->updates.channel, sizeof(cfg->updates.channel), "release");

    cfg->dashboard.port = 8081;
    snprintf(cfg->dashboard.theme, sizeof(cfg->dashboard.theme), "light");

    /* Display config defaults */
    snprintf(cfg->display.skin, sizeof(cfg->display.skin), "default");
    cfg->display.show_reasoning = true;
    cfg->display.compact = false;
    cfg->display.statusbar = true;
    cfg->display.show_cost = false;
    cfg->display.timestamps = false;
    snprintf(cfg->display.language, sizeof(cfg->display.language), "en");

    char hermes_home[HERMES_PATH_MAX];
    if (config_dir && config_dir[0])
        snprintf(hermes_home, sizeof(hermes_home), "%s", config_dir);
    else
        get_slermes_home(hermes_home, sizeof(hermes_home));

    snprintf(cfg->config_path, sizeof(cfg->config_path), "%s/config.yaml", hermes_home);
    snprintf(cfg->env_path, sizeof(cfg->env_path), "%s/.env", hermes_home);

    /* N02: Secure parent dir — chmod 0700 on config directory.
     * Skips if running as root (root can read anything anyway).
     * Creates directory if it doesn't exist. */
    {
        struct stat st;
        if (stat(hermes_home, &st) != 0) {
            /* Directory doesn't exist — create it with secure permissions */
            mkdir(hermes_home, 0700);
        } else if (S_ISDIR(st.st_mode)) {
            /* Directory exists — harden permissions unless root */
            if (geteuid() != 0) {
                chmod(hermes_home, 0700);
            }
        }
    }

    /* Parse config.yaml — with A04: !include directive support */
    char *err = NULL;
    yaml_doc_t *doc = NULL;

    /* Resolve !include directives before YAML parsing */
    char *preprocessed = config_resolve_includes(cfg->config_path);
    if (preprocessed) {
        doc = yaml_parse(preprocessed, &err);
        free(preprocessed);
    }
    if (!doc)
        doc = yaml_parse_file(cfg->config_path, &err);

    if (!doc) {
        /* No config file or parse error — use defaults */
        if (err) { free(err); }
        /* Try to set model from environment as fallback */
        const char *model_env = getenv("HERMES_MODEL");
        if (model_env) {
            snprintf(cfg->model, sizeof(cfg->model), "%s", model_env);
            snprintf(cfg->provider_cfg.model, sizeof(cfg->provider_cfg.model), "%s", model_env);
        }
        const char *prov_env = getenv("HERMES_PROVIDER");
        if (prov_env) {
            snprintf(cfg->provider, sizeof(cfg->provider), "%s", prov_env);
            snprintf(cfg->provider_cfg.provider, sizeof(cfg->provider_cfg.provider), "%s", prov_env);
        }
        const char *url_env = getenv("HERMES_BASE_URL");
        if (url_env) {
            snprintf(cfg->base_url, sizeof(cfg->base_url), "%s", url_env);
            snprintf(cfg->provider_cfg.base_url, sizeof(cfg->provider_cfg.base_url), "%s", url_env);
        }
        const char *key_env = getenv("HERMES_API_KEY");
        if (key_env) {
            snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", key_env);
            snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", key_env);
        }
        /* P158: Derive <VENDOR>_API_KEY in env-only path */
        if (cfg->api_key[0] == '\0' && cfg->provider_cfg.base_url[0]) {
            char *derived_name = provider_derive_api_key_name(
                cfg->provider_cfg.provider, cfg->provider_cfg.base_url);
            if (derived_name) {
                const char *env_val = getenv(derived_name);
                if (env_val && env_val[0]) {
                    snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", env_val);
                    snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", env_val);
                }
            free(derived_name);
        }
    }
    /* B33: DeepSeek cache TTL from env */
    {
        const char *ttl_env = getenv("HERMES_DEEPSEEK_CACHE_TTL");
        if (ttl_env && ttl_env[0]) {
            int ttl = atoi(ttl_env);
            if (ttl > 0) cfg->provider_cfg.deepseek_cache_ttl = ttl;
            else if (strcmp(ttl_env, "-1") == 0) cfg->provider_cfg.deepseek_cache_ttl = -1;
            else if (strcmp(ttl_env, "0") == 0) cfg->provider_cfg.deepseek_cache_ttl = 0;
        }
    }
    /* L06: supports_vision from env in env-only path */
    {
        const char *sv_env = getenv("HERMES_SUPPORTS_VISION");
        if (sv_env) {
            cfg->provider_cfg.supports_vision = (strcmp(sv_env, "1") == 0 ||
                                                  strcasecmp(sv_env, "true") == 0 ||
                                                  strcasecmp(sv_env, "yes") == 0);
        }
    }
    return true;
    }

    /* P1: Extended provider config — parse directly into provider_cfg, then sync to flat fields */

    /* Read config_version for migration tracking */
    int cfg_ver = yaml_get_int(doc, HERMES_CONFIG_VERSION_KEY, 0);
    cfg->config_version = cfg_ver;
    if (cfg_ver < HERMES_CONFIG_VERSION && cfg_ver > 0) {
        fprintf(stderr, "Config version v%d < current v%d. Run '/config migrate' to upgrade.\n",
                cfg_ver, HERMES_CONFIG_VERSION);
    }

    /* model section */
    const char *model_name = yaml_get_string(doc, "model.default");
    if (model_name) {
        char *exp = config_expand_env_vars(model_name);
        snprintf(cfg->provider_cfg.model, sizeof(cfg->provider_cfg.model), "%s", exp ? exp : model_name);
        free(exp);
    }

    const char *provider_str = yaml_get_string(doc, "model.provider");
    if (provider_str) {
        char *exp = config_expand_env_vars(provider_str);
        snprintf(cfg->provider_cfg.provider, sizeof(cfg->provider_cfg.provider), "%s", exp ? exp : provider_str);
        free(exp);
    }

    const char *base_url = yaml_get_string(doc, "model.base_url");
    if (base_url) {
        char *exp = config_expand_env_vars(base_url);
        snprintf(cfg->provider_cfg.base_url, sizeof(cfg->provider_cfg.base_url), "%s", exp ? exp : base_url);
        free(exp);
    }

    /* N05: Local provider trust — if base_url is localhost/127.0.0.1,
     * skip API key requirement. The key may still be provided for
     * providers that need it, but we don't error if missing. */
    {
        const char *burl = cfg->provider_cfg.base_url;
        if (burl[0] && (strstr(burl, "localhost") || strstr(burl, "127.0.0.1") ||
                        strstr(burl, "::1") || strstr(burl, "0.0.0.0"))) {
            cfg->provider_cfg.local_provider = true;
        }
    }

    const char *api_key = yaml_get_string(doc, "model.api_key");
    if (api_key) {
        char *exp = config_expand_env_vars(api_key);
        snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", exp ? exp : api_key);
        free(exp);
    }

    const char *api_mode = yaml_get_string(doc, "model.api_mode");
    if (api_mode) snprintf(cfg->provider_cfg.api_mode, sizeof(cfg->provider_cfg.api_mode), "%s", api_mode);

    int max_tokens = yaml_get_int(doc, "model.max_tokens", -1);
    if (max_tokens > 0) cfg->provider_cfg.max_tokens = max_tokens;

    float temp = (float)yaml_get_int(doc, "model.temperature", -1000) / 100.0f;
    if (temp < -999.0f) temp = (float)yaml_get_int(doc, "model.temperature", -1000);
    /* Try float via string */
    {
        const char *temp_str = yaml_get_string(doc, "model.temperature");
        if (temp_str) {
            float f = (float)atof(temp_str);
            if (f >= 0.0f && f <= 2.0f) cfg->provider_cfg.temperature = f;
        }
    }

    {
        const char *top_p_str = yaml_get_string(doc, "model.top_p");
        if (top_p_str) {
            float f = (float)atof(top_p_str);
            if (f >= 0.0f && f <= 1.0f) cfg->provider_cfg.top_p = f;
        }
    }

    /* stop_sequences from model.stop list */
    size_t stop_count = yaml_list_count(doc, "model.stop");
    if (stop_count > 0) {
        if (stop_count > HERMES_STOP_SEQUENCES_MAX)
            stop_count = HERMES_STOP_SEQUENCES_MAX;
        cfg->provider_cfg.stop_count = (int)stop_count;
        for (size_t i = 0; i < stop_count; i++) {
            const char *s = yaml_list_get(doc, "model.stop", i);
            if (s)
                snprintf(cfg->provider_cfg.stop_sequences[i],
                         sizeof(cfg->provider_cfg.stop_sequences[0]), "%s", s);
        }
    }

    /* presence_penalty, frequency_penalty, seed, logprobs, user */
    {
        const char *s = yaml_get_string(doc, "model.presence_penalty");
        if (s) { float f = (float)atof(s); if (f >= -2.0f && f <= 2.0f) cfg->provider_cfg.presence_penalty = f; }
    }
    {
        const char *s = yaml_get_string(doc, "model.frequency_penalty");
        if (s) { float f = (float)atof(s); if (f >= -2.0f && f <= 2.0f) cfg->provider_cfg.frequency_penalty = f; }
    }
    int seed = yaml_get_int(doc, "model.seed", -2);
    if (seed >= 0) cfg->provider_cfg.seed = seed;
    {
        const char *s = yaml_get_string(doc, "model.logprobs");
        if (s) cfg->provider_cfg.logprobs = (strcmp(s, "true") == 0 || strcmp(s, "1") == 0);
    }
    int tl = yaml_get_int(doc, "model.top_logprobs", -1);
    if (tl >= 0) cfg->provider_cfg.top_logprobs = tl;
    {
        const char *s = yaml_get_string(doc, "model.user");
        if (s) snprintf(cfg->provider_cfg.user, sizeof(cfg->provider_cfg.user), "%s", s);
    }

    /* Service tier from config */
    const char *fallback_model = yaml_get_string(doc, "model.fallback");
    if (!fallback_model) fallback_model = yaml_get_string(doc, "model.fallback_model");
    if (fallback_model) snprintf(cfg->provider_cfg.fallback_model,
                                  sizeof(cfg->provider_cfg.fallback_model), "%s", fallback_model);

    const char *fallback_providers = yaml_get_string(doc, "model.fallback_providers");
    if (!fallback_providers) fallback_providers = yaml_get_string(doc, "provider.fallback_providers");
    if (fallback_providers) snprintf(cfg->provider_cfg.fallback_providers,
                                      sizeof(cfg->provider_cfg.fallback_providers), "%s", fallback_providers);

    const char *service_tier = yaml_get_string(doc, "agent.service_tier");
    if (service_tier) snprintf(cfg->provider_cfg.service_tier,
                                sizeof(cfg->provider_cfg.service_tier), "%s", service_tier);

    const char *reasoning_effort = yaml_get_string(doc, "agent.reasoning_effort");
    if (reasoning_effort) snprintf(cfg->provider_cfg.reasoning_effort,
                                    sizeof(cfg->provider_cfg.reasoning_effort), "%s", reasoning_effort);

    const char *response_format = yaml_get_string(doc, "agent.response_format");
    if (response_format) snprintf(cfg->provider_cfg.response_format,
                                   sizeof(cfg->provider_cfg.response_format), "%s", response_format);

    const char *metadata = yaml_get_string(doc, "agent.metadata");
    if (metadata) snprintf(cfg->provider_cfg.metadata,
                            sizeof(cfg->provider_cfg.metadata), "%s", metadata);

    const char *tool_choice = yaml_get_string(doc, "agent.tool_choice");
    if (tool_choice) snprintf(cfg->provider_cfg.tool_choice,
                               sizeof(cfg->provider_cfg.tool_choice), "%s", tool_choice);
    /* parallel_tool_calls: default true */
    cfg->provider_cfg.parallel_tool_calls =
        yaml_get_bool(doc, "agent.parallel_tool_calls", true);
    /* max_tool_calls: 0 = unlimited */
    int max_tc = yaml_get_int(doc, "agent.max_tool_calls", -1);
    if (max_tc >= 0) cfg->provider_cfg.max_tool_calls = max_tc;
    /* n: number of choices (default 1) */
    int n_val = yaml_get_int(doc, "agent.n", 0);
    if (n_val > 0) cfg->provider_cfg.n = n_val;
    /* B30: top_k + candidate_count (Google generation_config depth) */
    int tk = yaml_get_int(doc, "agent.top_k", 0);
    if (tk > 0) cfg->provider_cfg.top_k = tk;
    int cc = yaml_get_int(doc, "agent.candidate_count", 0);
    if (cc > 0) cfg->provider_cfg.candidate_count = cc;
    /* B23: json_mode — auto-set response_format to json_object */
    cfg->provider_cfg.json_mode = yaml_get_bool(doc, "agent.json_mode", false);
    /* B24: response_format_strict — strict JSON schema enforcement */
    cfg->provider_cfg.response_format_strict = yaml_get_bool(doc, "agent.response_format_strict", false);

    /* B29: safety_settings JSON array */
    const char *ss = yaml_get_string(doc, "agent.safety_settings");
    if (ss) snprintf(cfg->provider_cfg.safety_settings, sizeof(cfg->provider_cfg.safety_settings), "%s", ss);

    /* L05: extra_body — arbitrary JSON to merge into request body */
    const char *extra = yaml_get_string(doc, "agent.extra_body");
    if (extra) snprintf(cfg->provider_cfg.extra_body, sizeof(cfg->provider_cfg.extra_body), "%s", extra);

    /* B37-B38: Azure provider depth — deployment_id + api_version */
    const char *az_deploy = yaml_get_string(doc, "azure.deployment_id");
    if (az_deploy) snprintf(cfg->provider_cfg.azure_deployment_id,
                             sizeof(cfg->provider_cfg.azure_deployment_id), "%s", az_deploy);
    const char *az_ver = yaml_get_string(doc, "azure.api_version");
    if (az_ver) snprintf(cfg->provider_cfg.azure_api_version,
                          sizeof(cfg->provider_cfg.azure_api_version), "%s", az_ver);

    /* B43-B46: OpenRouter provider preferences JSON */
    const char *or_prov = yaml_get_string(doc, "openrouter.provider");
    if (or_prov) snprintf(cfg->provider_cfg.openrouter_provider,
                           sizeof(cfg->provider_cfg.openrouter_provider), "%s", or_prov);

    /* B39-B41: Bedrock provider depth */
    const char *br_ip = yaml_get_string(doc, "bedrock.inference_profile");
    if (br_ip) snprintf(cfg->provider_cfg.bedrock_inference_profile,
                         sizeof(cfg->provider_cfg.bedrock_inference_profile), "%s", br_ip);
    const char *br_gc = yaml_get_string(doc, "bedrock.guardrail_config");
    if (br_gc) snprintf(cfg->provider_cfg.bedrock_guardrail_config,
                         sizeof(cfg->provider_cfg.bedrock_guardrail_config), "%s", br_gc);
    cfg->provider_cfg.bedrock_trace_enabled = yaml_get_bool(doc, "bedrock.trace_enabled", false);

    /* Sync provider_cfg back to flat fields */
    snprintf(cfg->model, sizeof(cfg->model), "%s", cfg->provider_cfg.model);
    snprintf(cfg->provider, sizeof(cfg->provider), "%s", cfg->provider_cfg.provider);
    snprintf(cfg->base_url, sizeof(cfg->base_url), "%s", cfg->provider_cfg.base_url);
    snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", cfg->provider_cfg.api_key);

    /* Also try flat top-level keys (backward compat with v0 format) */
    if (cfg->model[0] == '\0') {
        const char *flat_model = yaml_get_string(doc, "model");
        if (flat_model) snprintf(cfg->model, sizeof(cfg->model), "%s", flat_model);
    }
    if (cfg->provider[0] == '\0') {
        const char *flat_prov = yaml_get_string(doc, "provider");
        if (flat_prov) snprintf(cfg->provider, sizeof(cfg->provider), "%s", flat_prov);
    }
    if (cfg->base_url[0] == '\0') {
        const char *flat_url = yaml_get_string(doc, "base_url");
        if (flat_url) snprintf(cfg->base_url, sizeof(cfg->base_url), "%s", flat_url);
    }

    /* Env var overrides — always checked, even when config file exists */
    const char *model_env = getenv("HERMES_MODEL");
    if (model_env && model_env[0]) {
        snprintf(cfg->model, sizeof(cfg->model), "%s", model_env);
        snprintf(cfg->provider_cfg.model, sizeof(cfg->provider_cfg.model), "%s", model_env);
    }
    const char *prov_env = getenv("HERMES_PROVIDER");
    if (prov_env && prov_env[0]) {
        snprintf(cfg->provider, sizeof(cfg->provider), "%s", prov_env);
        snprintf(cfg->provider_cfg.provider, sizeof(cfg->provider_cfg.provider), "%s", prov_env);
    }
    const char *url_env = getenv("HERMES_BASE_URL");
    if (url_env && url_env[0]) {
        snprintf(cfg->base_url, sizeof(cfg->base_url), "%s", url_env);
        snprintf(cfg->provider_cfg.base_url, sizeof(cfg->provider_cfg.base_url), "%s", url_env);
    }
    const char *key_env = getenv("HERMES_API_KEY");
    if (key_env && key_env[0]) {
        snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", key_env);
        snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", key_env);
    }

    /* P158: Derive <VENDOR>_API_KEY from base_url/provider when no explicit key set */
    if (cfg->api_key[0] == '\0' && cfg->provider_cfg.base_url[0]) {
        char *derived_name = provider_derive_api_key_name(
            cfg->provider_cfg.provider, cfg->provider_cfg.base_url);
        if (derived_name) {
            const char *env_val = getenv(derived_name);
            if (env_val && env_val[0]) {
                snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", env_val);
                snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", env_val);
                fprintf(stderr, "[config] Derived API key from %s\n", derived_name);
            }
            free(derived_name);
        }
    }

    /* L06: supports_vision config override */
    cfg->provider_cfg.supports_vision = yaml_get_bool(doc, "model.supports_vision", false);

    /* Env override for supports_vision */
    {
        const char *sv_env = getenv("HERMES_SUPPORTS_VISION");
        if (sv_env) {
            cfg->provider_cfg.supports_vision = (strcmp(sv_env, "1") == 0 ||
                                                  strcasecmp(sv_env, "true") == 0 ||
                                                  strcasecmp(sv_env, "yes") == 0);
        }
    }

    /* Agent section */
    int max_turns = yaml_get_int(doc, "agent.max_turns", 90);
    cfg->max_turns = max_turns > 0 ? max_turns : 90;
    cfg->agent.max_iterations = cfg->max_turns;

    int max_tool_calls = yaml_get_int(doc, "agent.max_tool_calls_round", 0);
    if (max_tool_calls > 0) cfg->agent.max_tool_calls_round = max_tool_calls;

    int max_output = yaml_get_int(doc, "agent.max_output_tokens", 0);
    if (max_output > 0) cfg->agent.max_output_tokens = max_output;

    const char *sys_prompt = yaml_get_string(doc, "agent.system_prompt");
    if (sys_prompt) snprintf(cfg->agent.system_prompt, sizeof(cfg->agent.system_prompt), "%s", sys_prompt);

    const char *profile = yaml_get_string(doc, "agent.profile");
    if (profile) snprintf(cfg->agent.profile, sizeof(cfg->agent.profile), "%s", profile);

    /* Tools: enabled/disabled toolsets */
    const char *enabled_ts = yaml_get_string(doc, "tools.enabled_toolsets");
    if (enabled_ts) snprintf(cfg->tools.enabled_toolsets, sizeof(cfg->tools.enabled_toolsets), "%s", enabled_ts);
    const char *disabled_ts = yaml_get_string(doc, "tools.disabled_toolsets");
    if (disabled_ts) snprintf(cfg->tools.disabled_toolsets, sizeof(cfg->tools.disabled_toolsets), "%s", disabled_ts);

    /* Compression section — threshold */
    int c_thresh_int = yaml_get_int(doc, "compression.threshold", -1);
    if (c_thresh_int >= 0 && c_thresh_int <= 100) {
        cfg->agent.compress_threshold = (float)c_thresh_int / 100.0f;
    } else {
        const char *c_thresh_str = yaml_get_string(doc, "compression.threshold");
        if (c_thresh_str) {
            float f = (float)atof(c_thresh_str);
            if (f > 0.0f && f <= 1.0f) cfg->agent.compress_threshold = f;
        }
    }

    int api_retries = yaml_get_int(doc, "agent.api_max_retries", 0);
    if (api_retries > 0) cfg->agent.api_max_retries = api_retries;

    int clarify_to = yaml_get_int(doc, "agent.clarify_timeout", 0);
    if (clarify_to > 0) cfg->agent.clarify_timeout = clarify_to;

    /* Display section */
    const char *skin = yaml_get_string(doc, "display.skin");
    if (skin && strcmp(skin, "default") != 0 && skin[0] != '\0')
        snprintf(cfg->skin_path, sizeof(cfg->skin_path), "%s", skin);
    else
        cfg->skin_path[0] = '\0';
    /* Sync to display sub-struct */
    if (skin) snprintf(cfg->display.skin, sizeof(cfg->display.skin), "%s", skin);

    const char *banner = yaml_get_string(doc, "display.banner");
    if (banner) snprintf(cfg->display.banner, sizeof(cfg->display.banner), "%s", banner);

    const char *spinner = yaml_get_string(doc, "display.spinner");
    if (spinner) snprintf(cfg->display.spinner_style, sizeof(cfg->display.spinner_style), "%s", spinner);

    cfg->display.stream      = yaml_get_bool(doc, "display.streaming", false);
    cfg->display.show_reasoning = yaml_get_bool(doc, "display.show_reasoning", true);
    cfg->display.compact     = yaml_get_bool(doc, "display.compact", false);
    cfg->display.statusbar   = yaml_get_bool(doc, "display.statusbar", true);
    cfg->display.show_cost   = yaml_get_bool(doc, "display.show_cost", false);
    cfg->display.timestamps  = yaml_get_bool(doc, "display.timestamps", false);

    const char *indicator = yaml_get_string(doc, "display.indicator");
    if (!indicator) indicator = yaml_get_string(doc, "display.tui_status_indicator");
    if (indicator) snprintf(cfg->display.indicator, sizeof(cfg->display.indicator), "%s", indicator);

    const char *footer = yaml_get_string(doc, "display.footer");
    if (footer) snprintf(cfg->display.footer, sizeof(cfg->display.footer), "%s", footer);

    const char *lang = yaml_get_string(doc, "display.language");
    if (lang) snprintf(cfg->display.language, sizeof(cfg->display.language), "%s", lang);

    /* Gateway section */
    const char *gw_platforms = yaml_get_string(doc, "gateway.platforms");
    if (gw_platforms)
        snprintf(cfg->gateway_platforms, sizeof(cfg->gateway_platforms), "%s", gw_platforms);

    /* Display section — personality */
    const char *personality = yaml_get_string(doc, "display.personality");
    if (personality && personality[0])
        snprintf(cfg->personality, sizeof(cfg->personality), "%s", personality);

    /* Browser section */
    const char *cdp_url = yaml_get_string(doc, "browser.cdp_url");
    if (cdp_url) {
        snprintf(cfg->cdp_url, sizeof(cfg->cdp_url), "%s", cdp_url);
        snprintf(cfg->browser_cfg.cdp_url, sizeof(cfg->browser_cfg.cdp_url), "%s", cdp_url);
    }

    const char *browser_type = yaml_get_string(doc, "browser.engine");
    if (browser_type) snprintf(cfg->browser_cfg.browser_type, sizeof(cfg->browser_cfg.browser_type), "%s", browser_type);

    cfg->browser_cfg.headless = yaml_get_bool(doc, "browser.headless", true);
    cfg->browser_cfg.enable_javascript = yaml_get_bool(doc, "browser.javascript", true);

    int bw = yaml_get_int(doc, "browser.viewport_width", 0);
    if (bw > 0) cfg->browser_cfg.viewport_width = bw;
    int bh = yaml_get_int(doc, "browser.viewport_height", 0);
    if (bh > 0) cfg->browser_cfg.viewport_height = bh;

    int bt = yaml_get_int(doc, "browser.command_timeout", 0);
    if (bt > 0) cfg->browser_cfg.timeout = bt;

    /* P5: Delegation section */
    int dcc = yaml_get_int(doc, "delegation.max_concurrent_children", 0);
    if (dcc > 0) cfg->delegation.max_concurrent_children = dcc;
    int dsd = yaml_get_int(doc, "delegation.max_spawn_depth", 0);
    if (dsd > 0) cfg->delegation.max_spawn_depth = dsd;
    int dct = yaml_get_int(doc, "delegation.child_timeout_seconds", 0);
    if (dct > 0) cfg->delegation.child_timeout = dct;
    const char *dcm = yaml_get_string(doc, "delegation.model");
    if (dcm) snprintf(cfg->delegation.child_model, sizeof(cfg->delegation.child_model), "%s", dcm);
    const char *dcp = yaml_get_string(doc, "delegation.provider");
    if (dcp) snprintf(cfg->delegation.child_provider, sizeof(cfg->delegation.child_provider), "%s", dcp);
    int dmt = yaml_get_int(doc, "delegation.max_iterations", 0);
    if (dmt > 0) cfg->delegation.child_max_turns = dmt;

    /* P7: Memory section */
    const char *mp = yaml_get_string(doc, "memory.provider");
    if (mp) snprintf(cfg->memory.provider, sizeof(cfg->memory.provider), "%s", mp);
    int mcl = yaml_get_int(doc, "memory.memory_char_limit", 0);
    if (mcl > 0) cfg->memory.char_limit = mcl;
    int ucl = yaml_get_int(doc, "memory.user_char_limit", 0);
    if (ucl > 0) cfg->memory.user_char_limit = ucl;
    int mtd = yaml_get_int(doc, "memory.ttl_days", 0);
    if (mtd > 0) cfg->memory.ttl_days = mtd;
    cfg->memory.auto_save = yaml_get_bool(doc, "memory.auto_save", true);
    cfg->memory.compression_enabled = yaml_get_bool(doc, "memory.compression_enabled", false);
    int msl = yaml_get_int(doc, "memory.search_limit", 0);
    if (msl > 0) cfg->memory.search_limit = msl;
    int masi = yaml_get_int(doc, "memory.auto_save_interval", 0);
    if (masi > 0) cfg->memory.auto_save_interval = masi;
    cfg->memory.dedup_enabled = yaml_get_bool(doc, "memory.dedup", true);
    int mst = yaml_get_int(doc, "memory.storage_type", 0);
    if (mst >= 0 && mst <= 3) cfg->memory.storage_type = mst;
    const char *msp = yaml_get_string(doc, "memory.storage_path");
    if (msp) snprintf(cfg->memory.storage_path, sizeof(cfg->memory.storage_path), "%s", msp);

    /* P8: Compression section */
    const char *cm = yaml_get_string(doc, "compression.model");
    if (cm) snprintf(cfg->compression.model, sizeof(cfg->compression.model), "%s", cm);
    const char *cs = yaml_get_string(doc, "compression.strategy");
    if (cs) snprintf(cfg->compression.strategy, sizeof(cfg->compression.strategy), "%s", cs);
    {
        const char *tr_s = yaml_get_string(doc, "compression.target_ratio");
        if (tr_s) { float f = (float)atof(tr_s); if (f > 0.0f && f <= 1.0f) cfg->compression.target_ratio = f; }
    }
    int cmm = yaml_get_int(doc, "compression.min_messages", 0);
    if (cmm > 0) cfg->compression.min_messages = cmm;
    cfg->compression.preserve_system = yaml_get_bool(doc, "compression.preserve_system", true);

    /* P9: Cron section */
    const char *cd = yaml_get_string(doc, "cron.cron_dir");
    if (cd) snprintf(cfg->cron.dir, sizeof(cfg->cron.dir), "%s", cd);
    int mcj = yaml_get_int(doc, "cron.max_concurrent_jobs", 0);
    if (mcj > 0) cfg->cron.max_concurrent_jobs = mcj;
    int jto = yaml_get_int(doc, "cron.job_timeout", 0);
    if (jto > 0) cfg->cron.job_timeout = jto;
    int crd = yaml_get_int(doc, "cron.retention_days", 0);
    if (crd > 0) cfg->cron.retention_days = crd;
    cfg->cron.notify_on_failure = yaml_get_bool(doc, "cron.notify_on_failure", true);

    /* P10: Notification section */
    const char *np = yaml_get_string(doc, "notification.provider");
    if (np) snprintf(cfg->notification.provider, sizeof(cfg->notification.provider), "%s", np);
    const char *ns = yaml_get_string(doc, "notification.sound");
    if (ns) snprintf(cfg->notification.sound, sizeof(cfg->notification.sound), "%s", ns);
    cfg->notification.on_complete = yaml_get_bool(doc, "notification.on_complete", true);
    cfg->notification.on_error = yaml_get_bool(doc, "notification.on_error", true);
    cfg->notification.on_approval = yaml_get_bool(doc, "notification.on_approval", false);

    /* P11: Security section */
    const char *rp = yaml_get_string(doc, "security.redact_secrets");
    if (rp) snprintf(cfg->security.redact_patterns, sizeof(cfg->security.redact_patterns), "%s", rp);
    const char *tp = yaml_get_string(doc, "security.tirith_path");
    if (tp) snprintf(cfg->security.tirith_path, sizeof(cfg->security.tirith_path), "%s", tp);
    int tto = yaml_get_int(doc, "security.tirith_timeout", 0);
    if (tto > 0) cfg->security.tirith_timeout = tto;
    cfg->security.tirith_enabled = yaml_get_bool(doc, "security.tirith_enabled", true);
    cfg->security.allow_private_urls = yaml_get_bool(doc, "security.allow_private_urls", false);
    bool wbe = yaml_get_bool(doc, "security.website_blocklist.enabled", false);
    cfg->security.website_blocklist_enabled = wbe;

    /* O13: Load TIRITH policy text (YAML list of policy rules) */
    cfg->security.tirith_policy_text[0] = '\0';
    const char *tpt = yaml_get_string(doc, "security.tirith_policy_text");
    if (tpt) {
        snprintf(cfg->security.tirith_policy_text, sizeof(cfg->security.tirith_policy_text), "%s", tpt);
    }

    /* P12: Session section */
    const char *sdb = yaml_get_string(doc, "sessions.retention_days");
    /* sessions.retention_days already handled as int above via different path */
    int srd = yaml_get_int(doc, "sessions.retention_days", 0);
    if (srd > 0) cfg->session.retention_days = srd;
    int sai = yaml_get_int(doc, "sessions.auto_save_interval", 0);
    if (sai > 0) cfg->session.auto_save_interval = sai;
    cfg->session.compress = yaml_get_bool(doc, "sessions.compress", false);
    cfg->session.store_trajectories = yaml_get_bool(doc, "sessions.store_trajectories", false);

    /* P13: Plugin section */
    const char *pdirs = yaml_get_string(doc, "plugin.dirs");
    if (pdirs) snprintf(cfg->plugin.dirs, sizeof(cfg->plugin.dirs), "%s", pdirs);
    const char *pen = yaml_get_string(doc, "plugin.enabled");
    if (pen) snprintf(cfg->plugin.enabled, sizeof(cfg->plugin.enabled), "%s", pen);

    /* P14: MCP section */
    int mto = yaml_get_int(doc, "mcp_servers.timeout", 0);
    if (mto > 0) cfg->mcp.timeout = mto;
    cfg->mcp.auth_enabled = yaml_get_bool(doc, "mcp.auth_enabled", false);
    int mmt = yaml_get_int(doc, "mcp.max_tools", 0);
    if (mmt > 0) cfg->mcp.max_tools = mmt;
    const char *mcs = yaml_get_string(doc, "mcp.credential_store");
    if (mcs) snprintf(cfg->mcp.credential_store, sizeof(cfg->mcp.credential_store), "%s", mcs);

    /* Agent section */
    cfg->verbose = yaml_get_int(doc, "agent.verbose", 0);
    if (cfg->verbose < 0) cfg->verbose = 0;
    if (cfg->verbose > 2) cfg->verbose = 2;
    cfg->agent.verbose_level = cfg->verbose;

    /* Approvals section — yolo mode from approvals.mode=off */
    const char *approval_mode = yaml_get_string(doc, "approvals.mode");
    if (approval_mode) {
        snprintf(cfg->tools.approval_mode, sizeof(cfg->tools.approval_mode), "%s", approval_mode);
        if (strcmp(approval_mode, "off") == 0)
            cfg->yolo_mode = true;
    }

    int approval_timeout = yaml_get_int(doc, "approvals.timeout", 0);
    if (approval_timeout > 0) cfg->tools.approval_timeout = approval_timeout;

    /* Terminal section */
    int term_timeout = yaml_get_int(doc, "terminal.timeout", 0);
    if (term_timeout > 0) cfg->tools.terminal_timeout = term_timeout;

    /* Tool output section */
    int max_result = yaml_get_int(doc, "tool_output.max_bytes", 0);
    if (max_result > 0) cfg->tools.max_result_size = max_result;

    /* Auxiliary config section — 11 sub-tasks, each with provider/model/base_url/api_key/timeout/extra_body */
    /* Vision */
    { const char *v = yaml_get_string(doc, "auxiliary.vision.provider"); if (v) snprintf(cfg->auxiliary.vision.provider, sizeof(cfg->auxiliary.vision.provider), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.vision.model"); if (v) snprintf(cfg->auxiliary.vision.model, sizeof(cfg->auxiliary.vision.model), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.vision.base_url"); if (v) snprintf(cfg->auxiliary.vision.base_url, sizeof(cfg->auxiliary.vision.base_url), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.vision.api_key"); if (v) snprintf(cfg->auxiliary.vision.api_key, sizeof(cfg->auxiliary.vision.api_key), "%s", v); }
    { int v = yaml_get_int(doc, "auxiliary.vision.timeout", 0); if (v > 0) cfg->auxiliary.vision.timeout = v; }
    { const char *v = yaml_get_string(doc, "auxiliary.vision.extra_body"); if (v) snprintf(cfg->auxiliary.vision.extra_body, sizeof(cfg->auxiliary.vision.extra_body), "%s", v); }
    { int v = yaml_get_int(doc, "auxiliary.vision.download_timeout", 0); if (v > 0) cfg->auxiliary.vision_download_timeout = v; }
    /* Backward compat: sync to tools.vision_model/timeout */
    if (cfg->auxiliary.vision.model[0]) snprintf(cfg->tools.vision_model, sizeof(cfg->tools.vision_model), "%s", cfg->auxiliary.vision.model);
    if (cfg->auxiliary.vision.timeout > 0) cfg->tools.vision_timeout = cfg->auxiliary.vision.timeout;

    /* Web Extract */
    { const char *v = yaml_get_string(doc, "auxiliary.web_extract.provider"); if (v) snprintf(cfg->auxiliary.web_extract.provider, sizeof(cfg->auxiliary.web_extract.provider), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.web_extract.model"); if (v) snprintf(cfg->auxiliary.web_extract.model, sizeof(cfg->auxiliary.web_extract.model), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.web_extract.base_url"); if (v) snprintf(cfg->auxiliary.web_extract.base_url, sizeof(cfg->auxiliary.web_extract.base_url), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.web_extract.api_key"); if (v) snprintf(cfg->auxiliary.web_extract.api_key, sizeof(cfg->auxiliary.web_extract.api_key), "%s", v); }
    { int v = yaml_get_int(doc, "auxiliary.web_extract.timeout", 0); if (v > 0) cfg->auxiliary.web_extract.timeout = v; }
    { const char *v = yaml_get_string(doc, "auxiliary.web_extract.extra_body"); if (v) snprintf(cfg->auxiliary.web_extract.extra_body, sizeof(cfg->auxiliary.web_extract.extra_body), "%s", v); }

    /* Compression */
    { const char *v = yaml_get_string(doc, "auxiliary.compression.provider"); if (v) snprintf(cfg->auxiliary.compression.provider, sizeof(cfg->auxiliary.compression.provider), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.compression.model"); if (v) snprintf(cfg->auxiliary.compression.model, sizeof(cfg->auxiliary.compression.model), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.compression.base_url"); if (v) snprintf(cfg->auxiliary.compression.base_url, sizeof(cfg->auxiliary.compression.base_url), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.compression.api_key"); if (v) snprintf(cfg->auxiliary.compression.api_key, sizeof(cfg->auxiliary.compression.api_key), "%s", v); }
    { int v = yaml_get_int(doc, "auxiliary.compression.timeout", 0); if (v > 0) cfg->auxiliary.compression.timeout = v; }
    { const char *v = yaml_get_string(doc, "auxiliary.compression.extra_body"); if (v) snprintf(cfg->auxiliary.compression.extra_body, sizeof(cfg->auxiliary.compression.extra_body), "%s", v); }

    /* Skills Hub */
    { const char *v = yaml_get_string(doc, "auxiliary.skills_hub.provider"); if (v) snprintf(cfg->auxiliary.skills_hub.provider, sizeof(cfg->auxiliary.skills_hub.provider), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.skills_hub.model"); if (v) snprintf(cfg->auxiliary.skills_hub.model, sizeof(cfg->auxiliary.skills_hub.model), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.skills_hub.base_url"); if (v) snprintf(cfg->auxiliary.skills_hub.base_url, sizeof(cfg->auxiliary.skills_hub.base_url), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.skills_hub.api_key"); if (v) snprintf(cfg->auxiliary.skills_hub.api_key, sizeof(cfg->auxiliary.skills_hub.api_key), "%s", v); }
    { int v = yaml_get_int(doc, "auxiliary.skills_hub.timeout", 0); if (v > 0) cfg->auxiliary.skills_hub.timeout = v; }
    { const char *v = yaml_get_string(doc, "auxiliary.skills_hub.extra_body"); if (v) snprintf(cfg->auxiliary.skills_hub.extra_body, sizeof(cfg->auxiliary.skills_hub.extra_body), "%s", v); }

    /* Approval */
    { const char *v = yaml_get_string(doc, "auxiliary.approval.provider"); if (v) snprintf(cfg->auxiliary.approval.provider, sizeof(cfg->auxiliary.approval.provider), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.approval.model"); if (v) snprintf(cfg->auxiliary.approval.model, sizeof(cfg->auxiliary.approval.model), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.approval.base_url"); if (v) snprintf(cfg->auxiliary.approval.base_url, sizeof(cfg->auxiliary.approval.base_url), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.approval.api_key"); if (v) snprintf(cfg->auxiliary.approval.api_key, sizeof(cfg->auxiliary.approval.api_key), "%s", v); }
    { int v = yaml_get_int(doc, "auxiliary.approval.timeout", 0); if (v > 0) cfg->auxiliary.approval.timeout = v; }
    { const char *v = yaml_get_string(doc, "auxiliary.approval.extra_body"); if (v) snprintf(cfg->auxiliary.approval.extra_body, sizeof(cfg->auxiliary.approval.extra_body), "%s", v); }

    /* MCP */
    { const char *v = yaml_get_string(doc, "auxiliary.mcp.provider"); if (v) snprintf(cfg->auxiliary.mcp.provider, sizeof(cfg->auxiliary.mcp.provider), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.mcp.model"); if (v) snprintf(cfg->auxiliary.mcp.model, sizeof(cfg->auxiliary.mcp.model), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.mcp.base_url"); if (v) snprintf(cfg->auxiliary.mcp.base_url, sizeof(cfg->auxiliary.mcp.base_url), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.mcp.api_key"); if (v) snprintf(cfg->auxiliary.mcp.api_key, sizeof(cfg->auxiliary.mcp.api_key), "%s", v); }
    { int v = yaml_get_int(doc, "auxiliary.mcp.timeout", 0); if (v > 0) cfg->auxiliary.mcp.timeout = v; }
    { const char *v = yaml_get_string(doc, "auxiliary.mcp.extra_body"); if (v) snprintf(cfg->auxiliary.mcp.extra_body, sizeof(cfg->auxiliary.mcp.extra_body), "%s", v); }

    /* Title Generation */
    { const char *v = yaml_get_string(doc, "auxiliary.title_generation.provider"); if (v) snprintf(cfg->auxiliary.title_generation.provider, sizeof(cfg->auxiliary.title_generation.provider), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.title_generation.model"); if (v) snprintf(cfg->auxiliary.title_generation.model, sizeof(cfg->auxiliary.title_generation.model), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.title_generation.base_url"); if (v) snprintf(cfg->auxiliary.title_generation.base_url, sizeof(cfg->auxiliary.title_generation.base_url), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.title_generation.api_key"); if (v) snprintf(cfg->auxiliary.title_generation.api_key, sizeof(cfg->auxiliary.title_generation.api_key), "%s", v); }
    { int v = yaml_get_int(doc, "auxiliary.title_generation.timeout", 0); if (v > 0) cfg->auxiliary.title_generation.timeout = v; }
    { const char *v = yaml_get_string(doc, "auxiliary.title_generation.extra_body"); if (v) snprintf(cfg->auxiliary.title_generation.extra_body, sizeof(cfg->auxiliary.title_generation.extra_body), "%s", v); }

    /* Triage Specifier */
    { const char *v = yaml_get_string(doc, "auxiliary.triage_specifier.provider"); if (v) snprintf(cfg->auxiliary.triage_specifier.provider, sizeof(cfg->auxiliary.triage_specifier.provider), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.triage_specifier.model"); if (v) snprintf(cfg->auxiliary.triage_specifier.model, sizeof(cfg->auxiliary.triage_specifier.model), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.triage_specifier.base_url"); if (v) snprintf(cfg->auxiliary.triage_specifier.base_url, sizeof(cfg->auxiliary.triage_specifier.base_url), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.triage_specifier.api_key"); if (v) snprintf(cfg->auxiliary.triage_specifier.api_key, sizeof(cfg->auxiliary.triage_specifier.api_key), "%s", v); }
    { int v = yaml_get_int(doc, "auxiliary.triage_specifier.timeout", 0); if (v > 0) cfg->auxiliary.triage_specifier.timeout = v; }
    { const char *v = yaml_get_string(doc, "auxiliary.triage_specifier.extra_body"); if (v) snprintf(cfg->auxiliary.triage_specifier.extra_body, sizeof(cfg->auxiliary.triage_specifier.extra_body), "%s", v); }

    /* Kanban Decomposer */
    { const char *v = yaml_get_string(doc, "auxiliary.kanban_decomposer.provider"); if (v) snprintf(cfg->auxiliary.kanban_decomposer.provider, sizeof(cfg->auxiliary.kanban_decomposer.provider), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.kanban_decomposer.model"); if (v) snprintf(cfg->auxiliary.kanban_decomposer.model, sizeof(cfg->auxiliary.kanban_decomposer.model), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.kanban_decomposer.base_url"); if (v) snprintf(cfg->auxiliary.kanban_decomposer.base_url, sizeof(cfg->auxiliary.kanban_decomposer.base_url), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.kanban_decomposer.api_key"); if (v) snprintf(cfg->auxiliary.kanban_decomposer.api_key, sizeof(cfg->auxiliary.kanban_decomposer.api_key), "%s", v); }
    { int v = yaml_get_int(doc, "auxiliary.kanban_decomposer.timeout", 0); if (v > 0) cfg->auxiliary.kanban_decomposer.timeout = v; }
    { const char *v = yaml_get_string(doc, "auxiliary.kanban_decomposer.extra_body"); if (v) snprintf(cfg->auxiliary.kanban_decomposer.extra_body, sizeof(cfg->auxiliary.kanban_decomposer.extra_body), "%s", v); }

    /* Profile Describer */
    { const char *v = yaml_get_string(doc, "auxiliary.profile_describer.provider"); if (v) snprintf(cfg->auxiliary.profile_describer.provider, sizeof(cfg->auxiliary.profile_describer.provider), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.profile_describer.model"); if (v) snprintf(cfg->auxiliary.profile_describer.model, sizeof(cfg->auxiliary.profile_describer.model), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.profile_describer.base_url"); if (v) snprintf(cfg->auxiliary.profile_describer.base_url, sizeof(cfg->auxiliary.profile_describer.base_url), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.profile_describer.api_key"); if (v) snprintf(cfg->auxiliary.profile_describer.api_key, sizeof(cfg->auxiliary.profile_describer.api_key), "%s", v); }
    { int v = yaml_get_int(doc, "auxiliary.profile_describer.timeout", 0); if (v > 0) cfg->auxiliary.profile_describer.timeout = v; }
    { const char *v = yaml_get_string(doc, "auxiliary.profile_describer.extra_body"); if (v) snprintf(cfg->auxiliary.profile_describer.extra_body, sizeof(cfg->auxiliary.profile_describer.extra_body), "%s", v); }

    /* Curator */
    { const char *v = yaml_get_string(doc, "auxiliary.curator.provider"); if (v) snprintf(cfg->auxiliary.curator.provider, sizeof(cfg->auxiliary.curator.provider), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.curator.model"); if (v) snprintf(cfg->auxiliary.curator.model, sizeof(cfg->auxiliary.curator.model), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.curator.base_url"); if (v) snprintf(cfg->auxiliary.curator.base_url, sizeof(cfg->auxiliary.curator.base_url), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.curator.api_key"); if (v) snprintf(cfg->auxiliary.curator.api_key, sizeof(cfg->auxiliary.curator.api_key), "%s", v); }
    { int v = yaml_get_int(doc, "auxiliary.curator.timeout", 0); if (v > 0) cfg->auxiliary.curator.timeout = v; }
    { const char *v = yaml_get_string(doc, "auxiliary.curator.extra_body"); if (v) snprintf(cfg->auxiliary.curator.extra_body, sizeof(cfg->auxiliary.curator.extra_body), "%s", v); }

    /* TTS config */
    { const char *v = yaml_get_string(doc, "tts.provider"); if (v) snprintf(cfg->tts.provider, sizeof(cfg->tts.provider), "%s", v); }
    { const char *v = yaml_get_string(doc, "tts.edge.voice"); if (v) snprintf(cfg->tts.edge_voice, sizeof(cfg->tts.edge_voice), "%s", v); }
    { const char *v = yaml_get_string(doc, "tts.elevenlabs.voice_id"); if (v) snprintf(cfg->tts.elevenlabs_voice_id, sizeof(cfg->tts.elevenlabs_voice_id), "%s", v); }
    { const char *v = yaml_get_string(doc, "tts.elevenlabs.model_id"); if (v) snprintf(cfg->tts.elevenlabs_model_id, sizeof(cfg->tts.elevenlabs_model_id), "%s", v); }
    { const char *v = yaml_get_string(doc, "tts.openai.model"); if (v) snprintf(cfg->tts.openai_model, sizeof(cfg->tts.openai_model), "%s", v); }
    { const char *v = yaml_get_string(doc, "tts.openai.voice"); if (v) snprintf(cfg->tts.openai_voice, sizeof(cfg->tts.openai_voice), "%s", v); }
    { const char *v = yaml_get_string(doc, "tts.xai.voice_id"); if (v) snprintf(cfg->tts.xai_voice_id, sizeof(cfg->tts.xai_voice_id), "%s", v); }
    { const char *v = yaml_get_string(doc, "tts.xai.language"); if (v) snprintf(cfg->tts.xai_language, sizeof(cfg->tts.xai_language), "%s", v); }
    { int v = yaml_get_int(doc, "tts.xai.sample_rate", 0); if (v > 0) cfg->tts.xai_sample_rate = v; }
    { int v = yaml_get_int(doc, "tts.xai.bit_rate", 0); if (v > 0) cfg->tts.xai_bit_rate = v; }
    { const char *v = yaml_get_string(doc, "tts.mistral.model"); if (v) snprintf(cfg->tts.mistral_model, sizeof(cfg->tts.mistral_model), "%s", v); }
    { const char *v = yaml_get_string(doc, "tts.mistral.voice_id"); if (v) snprintf(cfg->tts.mistral_voice_id, sizeof(cfg->tts.mistral_voice_id), "%s", v); }
    { const char *v = yaml_get_string(doc, "tts.neutts.ref_audio"); if (v) snprintf(cfg->tts.neutts_ref_audio, sizeof(cfg->tts.neutts_ref_audio), "%s", v); }
    { const char *v = yaml_get_string(doc, "tts.neutts.ref_text"); if (v) snprintf(cfg->tts.neutts_ref_text, sizeof(cfg->tts.neutts_ref_text), "%s", v); }
    { const char *v = yaml_get_string(doc, "tts.neutts.model"); if (v) snprintf(cfg->tts.neutts_model, sizeof(cfg->tts.neutts_model), "%s", v); }
    { const char *v = yaml_get_string(doc, "tts.neutts.device"); if (v) snprintf(cfg->tts.neutts_device, sizeof(cfg->tts.neutts_device), "%s", v); }
    { const char *v = yaml_get_string(doc, "tts.piper.voice"); if (v) snprintf(cfg->tts.piper_voice, sizeof(cfg->tts.piper_voice), "%s", v); }

    /* STT config */
    cfg->stt.enabled = yaml_get_bool(doc, "stt.enabled", true);
    { const char *v = yaml_get_string(doc, "stt.provider"); if (v) snprintf(cfg->stt.provider, sizeof(cfg->stt.provider), "%s", v); }
    { const char *v = yaml_get_string(doc, "stt.local.model"); if (v) snprintf(cfg->stt.local_model, sizeof(cfg->stt.local_model), "%s", v); }
    { const char *v = yaml_get_string(doc, "stt.local.language"); if (v) snprintf(cfg->stt.local_language, sizeof(cfg->stt.local_language), "%s", v); }
    { const char *v = yaml_get_string(doc, "stt.openai.model"); if (v) snprintf(cfg->stt.openai_model, sizeof(cfg->stt.openai_model), "%s", v); }
    { const char *v = yaml_get_string(doc, "stt.mistral.model"); if (v) snprintf(cfg->stt.mistral_model, sizeof(cfg->stt.mistral_model), "%s", v); }

    /* Voice config */
    { const char *v = yaml_get_string(doc, "voice.record_key"); if (v) snprintf(cfg->voice.record_key, sizeof(cfg->voice.record_key), "%s", v); }
    { int v = yaml_get_int(doc, "voice.max_recording_seconds", 0); if (v > 0) cfg->voice.max_recording_seconds = v; }
    cfg->voice.auto_tts = yaml_get_bool(doc, "voice.auto_tts", false);
    cfg->voice.beep_enabled = yaml_get_bool(doc, "voice.beep_enabled", true);
    { int v = yaml_get_int(doc, "voice.silence_threshold", 0); if (v > 0) cfg->voice.silence_threshold = v; }
    { const char *v = yaml_get_string(doc, "voice.silence_duration"); if (v) cfg->voice.silence_duration = (float)atof(v); }

    /* Terminal backend */
    const char *term_backend = yaml_get_string(doc, "terminal.backend");
    if (term_backend) snprintf(cfg->tools.terminal_backend, sizeof(cfg->tools.terminal_backend), "%s", term_backend);
    cfg->tools.persistent_shell = yaml_get_bool(doc, "terminal.persistent_shell", cfg->tools.persistent_shell);

    /* Web config */
    const char *web_backend = yaml_get_string(doc, "web.backend");
    if (web_backend) snprintf(cfg->tools.web_backend, sizeof(cfg->tools.web_backend), "%s", web_backend);
    const char *web_search = yaml_get_string(doc, "web.search_backend");
    if (web_search) snprintf(cfg->tools.web_search_backend, sizeof(cfg->tools.web_search_backend), "%s", web_search);
    const char *web_extract = yaml_get_string(doc, "web.extract_backend");
    if (web_extract) snprintf(cfg->tools.web_extract_backend, sizeof(cfg->tools.web_extract_backend), "%s", web_extract);
    int web_search_to = yaml_get_int(doc, "web.search_timeout", 0);
    if (web_search_to > 0) cfg->tools.web_search_timeout = web_search_to;

    /* Fast mode */
    cfg->fast_mode = yaml_get_bool(doc, "agent.fast", false);
    cfg->agent.fast_mode = cfg->fast_mode;

    /* Compression section */
    cfg->compress_enabled = yaml_get_bool(doc, "compression.enabled", false);
    cfg->compression.protect_last_n = yaml_get_int(doc, "compression.protect_last_n", cfg->compression.protect_last_n);
    cfg->compression.protect_first_n = yaml_get_int(doc, "compression.protect_first_n", cfg->compression.protect_first_n);
    cfg->compression.hygiene_hard_message_limit = yaml_get_int(doc, "compression.hygiene_hard_message_limit", cfg->compression.hygiene_hard_message_limit);
    cfg->compression.abort_on_summary_failure = yaml_get_bool(doc, "compression.abort_on_summary_failure", cfg->compression.abort_on_summary_failure);

    /* Terminal config (expanded) */
    {
        const char *tb = yaml_get_string(doc, "terminal.backend");
        if (tb) {
            snprintf(cfg->terminal.backend, sizeof(cfg->terminal.backend), "%s", tb);
            snprintf(cfg->tools.terminal_backend, sizeof(cfg->tools.terminal_backend), "%s", tb);
        }
        int tto = yaml_get_int(doc, "terminal.timeout", 0);
        if (tto > 0) {
            cfg->terminal.timeout = tto;
            cfg->tools.terminal_timeout = tto;
        }
        cfg->terminal.persistent_shell = yaml_get_bool(doc, "terminal.persistent_shell", cfg->terminal.persistent_shell);
        cfg->tools.persistent_shell = cfg->terminal.persistent_shell;
    }
    {
        const char *tcwd = yaml_get_string(doc, "terminal.cwd");
        if (tcwd) snprintf(cfg->terminal.cwd, sizeof(cfg->terminal.cwd), "%s", tcwd);
        const char *tpassthru = yaml_get_string(doc, "terminal.env_passthrough");
        if (tpassthru) snprintf(cfg->terminal.env_passthrough, sizeof(cfg->terminal.env_passthrough), "%s", tpassthru);
        const char *tsif = yaml_get_string(doc, "terminal.shell_init_files");
        if (tsif) snprintf(cfg->terminal.shell_init_files, sizeof(cfg->terminal.shell_init_files), "%s", tsif);
        cfg->terminal.auto_source_bashrc = yaml_get_bool(doc, "terminal.auto_source_bashrc", cfg->terminal.auto_source_bashrc);
        const char *tdi = yaml_get_string(doc, "terminal.docker_image");
        if (tdi) snprintf(cfg->terminal.docker_image, sizeof(cfg->terminal.docker_image), "%s", tdi);
        const char *tdfe = yaml_get_string(doc, "terminal.docker_forward_env");
        if (tdfe) snprintf(cfg->terminal.docker_forward_env, sizeof(cfg->terminal.docker_forward_env), "%s", tdfe);
        const char *tde = yaml_get_string(doc, "terminal.docker_env");
        if (tde) snprintf(cfg->terminal.docker_env, sizeof(cfg->terminal.docker_env), "%s", tde);
        const char *tsi = yaml_get_string(doc, "terminal.singularity_image");
        if (tsi) snprintf(cfg->terminal.singularity_image, sizeof(cfg->terminal.singularity_image), "%s", tsi);
        const char *tmi = yaml_get_string(doc, "terminal.modal_image");
        if (tmi) snprintf(cfg->terminal.modal_image, sizeof(cfg->terminal.modal_image), "%s", tmi);
        const char *tdai = yaml_get_string(doc, "terminal.daytona_image");
        if (tdai) snprintf(cfg->terminal.daytona_image, sizeof(cfg->terminal.daytona_image), "%s", tdai);
        const char *tvr = yaml_get_string(doc, "terminal.vercel_runtime");
        if (tvr) snprintf(cfg->terminal.vercel_runtime, sizeof(cfg->terminal.vercel_runtime), "%s", tvr);
        int tcc = yaml_get_int(doc, "terminal.container_cpu", 0);
        if (tcc > 0) cfg->terminal.container_cpu = tcc;
        int tcm = yaml_get_int(doc, "terminal.container_memory", 0);
        if (tcm > 0) cfg->terminal.container_memory = tcm;
        int tcd = yaml_get_int(doc, "terminal.container_disk", 0);
        if (tcd > 0) cfg->terminal.container_disk = tcd;
        cfg->terminal.container_persistent = yaml_get_bool(doc, "terminal.container_persistent", cfg->terminal.container_persistent);
        const char *tdv = yaml_get_string(doc, "terminal.docker_volumes");
        if (tdv) snprintf(cfg->terminal.docker_volumes, sizeof(cfg->terminal.docker_volumes), "%s", tdv);
        cfg->terminal.docker_mount_cwd = yaml_get_bool(doc, "terminal.docker_mount_cwd_to_workspace", cfg->terminal.docker_mount_cwd);
        const char *tdea = yaml_get_string(doc, "terminal.docker_extra_args");
        if (tdea) snprintf(cfg->terminal.docker_extra_args, sizeof(cfg->terminal.docker_extra_args), "%s", tdea);
        cfg->terminal.docker_run_as_host_user = yaml_get_bool(doc, "terminal.docker_run_as_host_user", cfg->terminal.docker_run_as_host_user);
    }

    /* Logging config */
    {
        const char *ll = yaml_get_string(doc, "logging.level");
        if (ll) snprintf(cfg->logging.level, sizeof(cfg->logging.level), "%s", ll);
        const char *lf = yaml_get_string(doc, "logging.format");
        if (lf) snprintf(cfg->logging.format, sizeof(cfg->logging.format), "%s", lf);
        const char *ld = yaml_get_string(doc, "logging.dir");
        if (ld) snprintf(cfg->logging.dir, sizeof(cfg->logging.dir), "%s", ld);
        int lmf = yaml_get_int(doc, "logging.max_files", 0);
        if (lmf > 0) cfg->logging.max_files = lmf;
        int lms = yaml_get_int(doc, "logging.max_size_mb", 0);
        if (lms > 0) cfg->logging.max_size_mb = lms;
    }

    /* Skills config */
    {
        const char *sd = yaml_get_string(doc, "skills.dir");
        if (sd) snprintf(cfg->skills.dir, sizeof(cfg->skills.dir), "%s", sd);
        const char *se = yaml_get_string(doc, "skills.enabled");
        if (se) snprintf(cfg->skills.enabled, sizeof(cfg->skills.enabled), "%s", se);
        cfg->skills.auto_discover = yaml_get_bool(doc, "skills.auto_discover", cfg->skills.auto_discover);
        int sbs = yaml_get_int(doc, "skills.bundle_size_limit", 0);
        if (sbs > 0) cfg->skills.bundle_size_limit = sbs;
        int sv = yaml_get_int(doc, "skills.validate", 1);
        if (sv >= 0 && sv <= 2) cfg->skills.validate_on_load = sv;
    }

    /* Checkpoints config */
    {
        cfg->checkpoints.enabled = yaml_get_bool(doc, "checkpoints.enabled", cfg->checkpoints.enabled);
        int ci = yaml_get_int(doc, "checkpoints.interval", 0);
        if (ci > 0) cfg->checkpoints.interval = ci;
        int cm = yaml_get_int(doc, "checkpoints.max", 0);
        if (cm > 0) cfg->checkpoints.max_checkpoints = cm;
        const char *cd = yaml_get_string(doc, "checkpoints.dir");
        if (cd) snprintf(cfg->checkpoints.dir, sizeof(cfg->checkpoints.dir), "%s", cd);
        cfg->checkpoints.auto_rollback = yaml_get_bool(doc, "checkpoints.auto_rollback", cfg->checkpoints.auto_rollback);
        cfg->checkpoints.save_on_interrupt = yaml_get_bool(doc, "checkpoints.save_on_interrupt", cfg->checkpoints.save_on_interrupt);
        int ccl = yaml_get_int(doc, "checkpoints.compression", 1);
        if (ccl >= 0 && ccl <= 9) cfg->checkpoints.compression_level = ccl;
        cfg->checkpoints.include_tool_results = yaml_get_bool(doc, "checkpoints.include_tool_results", cfg->checkpoints.include_tool_results);
    }

    /* Secrets config (L01: Bitwarden Secrets Manager) */
    {
        cfg->secrets.enabled = yaml_get_bool(doc, "secrets.bitwarden.enabled", cfg->secrets.enabled);
        const char *sat = yaml_get_string(doc, "secrets.bitwarden.access_token");
        if (sat) snprintf(cfg->secrets.access_token, sizeof(cfg->secrets.access_token), "%s", sat);
        const char *soi = yaml_get_string(doc, "secrets.bitwarden.organization_id");
        if (soi) snprintf(cfg->secrets.organization_id, sizeof(cfg->secrets.organization_id), "%s", soi);
        const char *sbp = yaml_get_string(doc, "secrets.bitwarden.bws_path");
        if (sbp) snprintf(cfg->secrets.bws_path, sizeof(cfg->secrets.bws_path), "%s", sbp);
        int sit = yaml_get_int(doc, "secrets.bitwarden.install_timeout", 0);
        if (sit > 0) cfg->secrets.install_timeout = sit;
    }

    /* Quiet mode */
    cfg->agent.quiet_mode = cfg->quiet_mode;

    yaml_free(doc);

    /* Parse .env (overrides config.yaml) */
    parse_env_file(cfg->env_path, cfg);

    /* Auto-load profile if configured */
    if (cfg->agent.profile[0]) {
        if (!hermes_config_load_profile(cfg, cfg->agent.profile, config_dir)) {
            fprintf(stderr, "Warning: profile '%s' not found in profiles/\\n", cfg->agent.profile);
        }
        hermes_set_profile(cfg->agent.profile);
    }

    return true;
}

bool hermes_config_load_env(hermes_config_t *cfg) {
    /* Environment overrides for all config fields */
    const char *v;

    v = getenv("HERMES_MODEL");
    if (v) {
        snprintf(cfg->model, sizeof(cfg->model), "%s", v);
        snprintf(cfg->provider_cfg.model, sizeof(cfg->provider_cfg.model), "%s", v);
    }

    v = getenv("HERMES_PROVIDER");
    if (v) {
        snprintf(cfg->provider, sizeof(cfg->provider), "%s", v);
        snprintf(cfg->provider_cfg.provider, sizeof(cfg->provider_cfg.provider), "%s", v);
    }

    v = getenv("HERMES_BASE_URL");
    if (v) {
        snprintf(cfg->base_url, sizeof(cfg->base_url), "%s", v);
        snprintf(cfg->provider_cfg.base_url, sizeof(cfg->provider_cfg.base_url), "%s", v);
    }

    v = getenv("HERMES_API_KEY");
    if (v) {
        snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", v);
        snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", v);
    }

    v = getenv("HERMES_MAX_TURNS");
    if (v) { int t = atoi(v); if (t > 0) {
        cfg->max_turns = t;
        cfg->agent.max_iterations = t;
    }}

    v = getenv("HERMES_SKIN");
    if (v) {
        snprintf(cfg->skin_path, sizeof(cfg->skin_path), "%s", v);
        snprintf(cfg->display.skin, sizeof(cfg->display.skin), "%s", v);
    }

    v = getenv("HERMES_PERSONALITY");
    if (v) {
        snprintf(cfg->personality, sizeof(cfg->personality), "%s", v);
        snprintf(cfg->display.personality, sizeof(cfg->display.personality), "%s", v);
    }

    v = getenv("HERMES_VERBOSE");
    if (v) { int t = atoi(v); if (t >= 0 && t <= 2) {
        cfg->verbose = t;
        cfg->agent.verbose_level = t;
    }}

    v = getenv("HERMES_YOLO");
    if (v && (strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0))
        cfg->yolo_mode = true;

    v = getenv("HERMES_FAST");
    if (v && (strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0)) {
        cfg->fast_mode = true;
        cfg->agent.fast_mode = true;
    }

    v = getenv("HERMES_COMPRESS");
    if (v && (strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0))
        cfg->compress_enabled = true;

    /* P1 env overrides */
    v = getenv("HERMES_API_MODE");
    if (v) snprintf(cfg->provider_cfg.api_mode, sizeof(cfg->provider_cfg.api_mode), "%s", v);

    v = getenv("HERMES_MAX_TOKENS");
    if (v) { int t = atoi(v); if (t > 0) cfg->provider_cfg.max_tokens = t; }

    v = getenv("HERMES_TEMPERATURE");
    if (v) { float f = (float)atof(v); if (f >= 0.0f && f <= 2.0f) cfg->provider_cfg.temperature = f; }

    v = getenv("HERMES_PRESENCE_PENALTY");
    if (v) { float f = (float)atof(v); if (f >= -2.0f && f <= 2.0f) cfg->provider_cfg.presence_penalty = f; }

    v = getenv("HERMES_FREQUENCY_PENALTY");
    if (v) { float f = (float)atof(v); if (f >= -2.0f && f <= 2.0f) cfg->provider_cfg.frequency_penalty = f; }

    v = getenv("HERMES_SEED");
    if (v) { int t = atoi(v); if (t >= 0) cfg->provider_cfg.seed = t; }

    v = getenv("HERMES_LOGPROBS");
    if (v) cfg->provider_cfg.logprobs = (strcmp(v, "true") == 0 || strcmp(v, "1") == 0);

    v = getenv("HERMES_TOP_LOGPROBS");
    if (v) { int t = atoi(v); if (t >= 0) cfg->provider_cfg.top_logprobs = t; }

    v = getenv("HERMES_USER");
    if (v) snprintf(cfg->provider_cfg.user, sizeof(cfg->provider_cfg.user), "%s", v);

    v = getenv("HERMES_TOP_P");
    if (v) { float f = (float)atof(v); if (f >= 0.0f && f <= 1.0f) cfg->provider_cfg.top_p = f; }

    v = getenv("HERMES_FALLBACK_MODEL");
    if (v) snprintf(cfg->provider_cfg.fallback_model, sizeof(cfg->provider_cfg.fallback_model), "%s", v);

    v = getenv("HERMES_SERVICE_TIER");
    if (v) snprintf(cfg->provider_cfg.service_tier, sizeof(cfg->provider_cfg.service_tier), "%s", v);

    v = getenv("HERMES_REASONING_EFFORT");
    if (v) snprintf(cfg->provider_cfg.reasoning_effort, sizeof(cfg->provider_cfg.reasoning_effort), "%s", v);

    v = getenv("HERMES_RESPONSE_FORMAT");
    if (v) snprintf(cfg->provider_cfg.response_format, sizeof(cfg->provider_cfg.response_format), "%s", v);

    v = getenv("HERMES_METADATA");
    if (v) snprintf(cfg->provider_cfg.metadata, sizeof(cfg->provider_cfg.metadata), "%s", v);

    v = getenv("HERMES_TOOL_CHOICE");
    if (v) snprintf(cfg->provider_cfg.tool_choice, sizeof(cfg->provider_cfg.tool_choice), "%s", v);

    v = getenv("HERMES_PARALLEL_TOOL_CALLS");
    if (v) cfg->provider_cfg.parallel_tool_calls = (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0) ? false : true;

    v = getenv("HERMES_MAX_TOOL_CALLS");
    if (v) { int t = atoi(v); if (t >= 0) cfg->provider_cfg.max_tool_calls = t; }

    v = getenv("HERMES_N");
    if (v) { int t = atoi(v); if (t > 0) cfg->provider_cfg.n = t; }

    /* B30: top_k + candidate_count env vars */
    v = getenv("HERMES_TOP_K");
    if (v) { int t = atoi(v); if (t > 0) cfg->provider_cfg.top_k = t; }
    v = getenv("HERMES_CANDIDATE_COUNT");
    if (v) { int t = atoi(v); if (t > 0) cfg->provider_cfg.candidate_count = t; }

    /* B23: json_mode env var */
    v = getenv("HERMES_JSON_MODE");
    if (v) cfg->provider_cfg.json_mode = (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0) ? false : true;

    /* B24: response_format_strict env var */
    v = getenv("HERMES_RESPONSE_FORMAT_STRICT");
    if (v) cfg->provider_cfg.response_format_strict = (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0) ? false : true;

    /* B29: safety_settings env var */
    v = getenv("HERMES_SAFETY_SETTINGS");
    if (v) snprintf(cfg->provider_cfg.safety_settings, sizeof(cfg->provider_cfg.safety_settings), "%s", v);

    v = getenv("HERMES_EXTRA_BODY");
    if (v) snprintf(cfg->provider_cfg.extra_body, sizeof(cfg->provider_cfg.extra_body), "%s", v);

    /* B37: Azure deployment_id env var */
    v = getenv("HERMES_AZURE_DEPLOYMENT_ID");
    if (v) snprintf(cfg->provider_cfg.azure_deployment_id, sizeof(cfg->provider_cfg.azure_deployment_id), "%s", v);
    v = getenv("HERMES_AZURE_API_VERSION");
    if (v) snprintf(cfg->provider_cfg.azure_api_version, sizeof(cfg->provider_cfg.azure_api_version), "%s", v);

    /* B43-B46: OpenRouter provider env var */
    v = getenv("HERMES_OPENROUTER_PROVIDER");
    if (v) snprintf(cfg->provider_cfg.openrouter_provider, sizeof(cfg->provider_cfg.openrouter_provider), "%s", v);

    /* B39-B41: Bedrock provider env vars */
    v = getenv("HERMES_BEDROCK_INFERENCE_PROFILE");
    if (v) snprintf(cfg->provider_cfg.bedrock_inference_profile, sizeof(cfg->provider_cfg.bedrock_inference_profile), "%s", v);
    v = getenv("HERMES_BEDROCK_GUARDRAIL_CONFIG");
    if (v) snprintf(cfg->provider_cfg.bedrock_guardrail_config, sizeof(cfg->provider_cfg.bedrock_guardrail_config), "%s", v);
    v = getenv("HERMES_BEDROCK_TRACE_ENABLED");
    if (v) cfg->provider_cfg.bedrock_trace_enabled = (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0) ? false : true;

    /* P2 env overrides (display) */
    v = getenv("HERMES_SKIN");
    if (v) {
        snprintf(cfg->skin_path, sizeof(cfg->skin_path), "%s", v);
        snprintf(cfg->display.skin, sizeof(cfg->display.skin), "%s", v);
    }

    v = getenv("HERMES_PERSONALITY");
    if (v) {
        snprintf(cfg->personality, sizeof(cfg->personality), "%s", v);
        snprintf(cfg->display.personality, sizeof(cfg->display.personality), "%s", v);
    }

    v = getenv("HERMES_BANNER");
    if (v) snprintf(cfg->display.banner, sizeof(cfg->display.banner), "%s", v);

    v = getenv("HERMES_SPINNER");
    if (v) snprintf(cfg->display.spinner_style, sizeof(cfg->display.spinner_style), "%s", v);

    v = getenv("HERMES_STREAM");
    if (v && (strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0))
        cfg->display.stream = true;

    v = getenv("HERMES_SHOW_REASONING");
    if (v && (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0))
        cfg->display.show_reasoning = false;

    v = getenv("HERMES_INDICATOR");
    if (v) snprintf(cfg->display.indicator, sizeof(cfg->display.indicator), "%s", v);

    v = getenv("HERMES_COMPACT");
    if (v && (strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0))
        cfg->display.compact = true;

    v = getenv("HERMES_DISPLAY_LANGUAGE");
    if (v) snprintf(cfg->display.language, sizeof(cfg->display.language), "%s", v);

    v = getenv("HERMES_SHOW_COST");
    if (v && (strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0))
        cfg->display.show_cost = true;

    v = getenv("HERMES_TIMESTAMPS");
    if (v && (strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0))
        cfg->display.timestamps = true;

    /* P3 env overrides (agent) */
    v = getenv("HERMES_MAX_TOOL_CALLS_ROUND");
    if (v) { int t = atoi(v); if (t > 0) cfg->agent.max_tool_calls_round = t; }

    v = getenv("HERMES_MAX_OUTPUT_TOKENS");
    if (v) { int t = atoi(v); if (t > 0) cfg->agent.max_output_tokens = t; }

    v = getenv("HERMES_SYSTEM_PROMPT");
    if (v) snprintf(cfg->agent.system_prompt, sizeof(cfg->agent.system_prompt), "%s", v);

    v = getenv("HERMES_PROFILE");
    if (v) snprintf(cfg->agent.profile, sizeof(cfg->agent.profile), "%s", v);

    v = getenv("HERMES_COMPRESS_THRESHOLD");
    if (v) { float f = (float)atof(v); if (f > 0.0f && f <= 1.0f) cfg->agent.compress_threshold = f; }

    v = getenv("HERMES_AGENT_REASONING_EFFORT");
    if (v) snprintf(cfg->agent.reasoning_effort, sizeof(cfg->agent.reasoning_effort), "%s", v);

    v = getenv("HERMES_API_MAX_RETRIES");
    if (v) { int t = atoi(v); if (t > 0) cfg->agent.api_max_retries = t; }

    v = getenv("HERMES_CLARIFY_TIMEOUT");
    if (v) { int t = atoi(v); if (t > 0) cfg->agent.clarify_timeout = t; }

    /* P4 env overrides (tools) */
    v = getenv("HERMES_ALLOW_BACKGROUND");
    if (v && (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0))
        cfg->tools.allow_background = false;

    v = getenv("HERMES_APPROVAL_MODE");
    if (v) snprintf(cfg->tools.approval_mode, sizeof(cfg->tools.approval_mode), "%s", v);

    v = getenv("HERMES_APPROVAL_TIMEOUT");
    if (v) { int t = atoi(v); if (t > 0) cfg->tools.approval_timeout = t; }

    v = getenv("HERMES_MAX_RESULT_SIZE");
    if (v) { int t = atoi(v); if (t > 0) cfg->tools.max_result_size = t; }

    v = getenv("HERMES_TERMINAL_TIMEOUT");
    if (v) { int t = atoi(v); if (t > 0) cfg->tools.terminal_timeout = t; }

    v = getenv("HERMES_VISION_MODEL");
    if (v) {
        snprintf(cfg->tools.vision_model, sizeof(cfg->tools.vision_model), "%s", v);
        snprintf(cfg->auxiliary.vision.model, sizeof(cfg->auxiliary.vision.model), "%s", v);
    }

    v = getenv("HERMES_VISION_TIMEOUT");
    if (v) {
        int t = atoi(v); if (t > 0) cfg->tools.vision_timeout = t;
        if (t > 0) cfg->auxiliary.vision.timeout = t;
    }

    /* Auxiliary env overrides — HERMES_AUX_<TASK>_<FIELD> */
    #define LOAD_AUX_ENV_STR(task, field) do { \
        char en[128]; snprintf(en, sizeof(en), "HERMES_AUX_" #task "_" #field); \
        const char *e = getenv(en); if (e) snprintf(cfg->auxiliary.task.field, sizeof(cfg->auxiliary.task.field), "%s", e); \
    } while(0)
    #define LOAD_AUX_ENV_INT(task, field) do { \
        char en[128]; snprintf(en, sizeof(en), "HERMES_AUX_" #task "_" #field); \
        const char *e = getenv(en); if (e) { int tv = atoi(e); if (tv > 0) cfg->auxiliary.task.field = tv; } \
    } while(0)

    LOAD_AUX_ENV_STR(vision, provider);
    LOAD_AUX_ENV_STR(vision, base_url);
    LOAD_AUX_ENV_STR(vision, api_key);
    {
        char en[128]; snprintf(en, sizeof(en), "HERMES_AUX_VISION_DOWNLOAD_TIMEOUT");
        const char *e = getenv(en); if (e) { int tv = atoi(e); if (tv > 0) cfg->auxiliary.vision_download_timeout = tv; }
    }
    LOAD_AUX_ENV_STR(vision, extra_body);

    LOAD_AUX_ENV_STR(web_extract, provider);
    LOAD_AUX_ENV_STR(web_extract, model);
    LOAD_AUX_ENV_STR(web_extract, base_url);
    LOAD_AUX_ENV_STR(web_extract, api_key);
    LOAD_AUX_ENV_INT(web_extract, timeout);
    LOAD_AUX_ENV_STR(web_extract, extra_body);

    LOAD_AUX_ENV_STR(compression, provider);
    LOAD_AUX_ENV_STR(compression, model);
    LOAD_AUX_ENV_STR(compression, base_url);
    LOAD_AUX_ENV_STR(compression, api_key);
    LOAD_AUX_ENV_INT(compression, timeout);
    LOAD_AUX_ENV_STR(compression, extra_body);

    LOAD_AUX_ENV_STR(skills_hub, provider);
    LOAD_AUX_ENV_STR(skills_hub, model);
    LOAD_AUX_ENV_STR(skills_hub, base_url);
    LOAD_AUX_ENV_STR(skills_hub, api_key);
    LOAD_AUX_ENV_INT(skills_hub, timeout);
    LOAD_AUX_ENV_STR(skills_hub, extra_body);

    LOAD_AUX_ENV_STR(approval, provider);
    LOAD_AUX_ENV_STR(approval, model);
    LOAD_AUX_ENV_STR(approval, base_url);
    LOAD_AUX_ENV_STR(approval, api_key);
    LOAD_AUX_ENV_INT(approval, timeout);
    LOAD_AUX_ENV_STR(approval, extra_body);

    LOAD_AUX_ENV_STR(mcp, provider);
    LOAD_AUX_ENV_STR(mcp, model);
    LOAD_AUX_ENV_STR(mcp, base_url);
    LOAD_AUX_ENV_STR(mcp, api_key);
    LOAD_AUX_ENV_INT(mcp, timeout);
    LOAD_AUX_ENV_STR(mcp, extra_body);

    LOAD_AUX_ENV_STR(title_generation, provider);
    LOAD_AUX_ENV_STR(title_generation, model);
    LOAD_AUX_ENV_STR(title_generation, base_url);
    LOAD_AUX_ENV_STR(title_generation, api_key);
    LOAD_AUX_ENV_INT(title_generation, timeout);
    LOAD_AUX_ENV_STR(title_generation, extra_body);

    LOAD_AUX_ENV_STR(triage_specifier, provider);
    LOAD_AUX_ENV_STR(triage_specifier, model);
    LOAD_AUX_ENV_STR(triage_specifier, base_url);
    LOAD_AUX_ENV_STR(triage_specifier, api_key);
    LOAD_AUX_ENV_INT(triage_specifier, timeout);
    LOAD_AUX_ENV_STR(triage_specifier, extra_body);

    LOAD_AUX_ENV_STR(kanban_decomposer, provider);
    LOAD_AUX_ENV_STR(kanban_decomposer, model);
    LOAD_AUX_ENV_STR(kanban_decomposer, base_url);
    LOAD_AUX_ENV_STR(kanban_decomposer, api_key);
    LOAD_AUX_ENV_INT(kanban_decomposer, timeout);
    LOAD_AUX_ENV_STR(kanban_decomposer, extra_body);

    LOAD_AUX_ENV_STR(profile_describer, provider);
    LOAD_AUX_ENV_STR(profile_describer, model);
    LOAD_AUX_ENV_STR(profile_describer, base_url);
    LOAD_AUX_ENV_STR(profile_describer, api_key);
    LOAD_AUX_ENV_INT(profile_describer, timeout);
    LOAD_AUX_ENV_STR(profile_describer, extra_body);

    LOAD_AUX_ENV_STR(curator, provider);
    LOAD_AUX_ENV_STR(curator, model);
    LOAD_AUX_ENV_STR(curator, base_url);
    LOAD_AUX_ENV_STR(curator, api_key);
    LOAD_AUX_ENV_INT(curator, timeout);
    LOAD_AUX_ENV_STR(curator, extra_body);

    #undef LOAD_AUX_ENV_STR
    #undef LOAD_AUX_ENV_INT

    /* TTS env overrides */
    v = getenv("HERMES_TTS_PROVIDER");
    if (v) snprintf(cfg->tts.provider, sizeof(cfg->tts.provider), "%s", v);
    v = getenv("HERMES_TTS_EDGE_VOICE");
    if (v) snprintf(cfg->tts.edge_voice, sizeof(cfg->tts.edge_voice), "%s", v);
    v = getenv("HERMES_TTS_ELEVENLABS_VOICE_ID");
    if (v) snprintf(cfg->tts.elevenlabs_voice_id, sizeof(cfg->tts.elevenlabs_voice_id), "%s", v);
    v = getenv("HERMES_TTS_OPENAI_VOICE");
    if (v) snprintf(cfg->tts.openai_voice, sizeof(cfg->tts.openai_voice), "%s", v);
    v = getenv("HERMES_TTS_XAI_VOICE_ID");
    if (v) snprintf(cfg->tts.xai_voice_id, sizeof(cfg->tts.xai_voice_id), "%s", v);
    v = getenv("HERMES_TTS_PIPER_VOICE");
    if (v) snprintf(cfg->tts.piper_voice, sizeof(cfg->tts.piper_voice), "%s", v);

    /* STT env overrides */
    v = getenv("HERMES_STT_PROVIDER");
    if (v) snprintf(cfg->stt.provider, sizeof(cfg->stt.provider), "%s", v);
    v = getenv("HERMES_STT_LOCAL_MODEL");
    if (v) snprintf(cfg->stt.local_model, sizeof(cfg->stt.local_model), "%s", v);

    /* Voice env overrides */
    v = getenv("HERMES_VOICE_RECORD_KEY");
    if (v) snprintf(cfg->voice.record_key, sizeof(cfg->voice.record_key), "%s", v);
    v = getenv("HERMES_VOICE_AUTO_TTS");
    if (v && (strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0)) cfg->voice.auto_tts = true;

    v = getenv("HERMES_TERMINAL_BACKEND");
    if (v) snprintf(cfg->tools.terminal_backend, sizeof(cfg->tools.terminal_backend), "%s", v);

    v = getenv("HERMES_PERSISTENT_SHELL");
    if (v && (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0))
        cfg->tools.persistent_shell = false;
    else if (v)
        cfg->tools.persistent_shell = true;

    v = getenv("HERMES_WEB_BACKEND");
    if (v) snprintf(cfg->tools.web_backend, sizeof(cfg->tools.web_backend), "%s", v);

    v = getenv("HERMES_WEB_SEARCH_BACKEND");
    if (v) snprintf(cfg->tools.web_search_backend, sizeof(cfg->tools.web_search_backend), "%s", v);

    v = getenv("HERMES_WEB_EXTRACT_BACKEND");
    if (v) snprintf(cfg->tools.web_extract_backend, sizeof(cfg->tools.web_extract_backend), "%s", v);

    v = getenv("HERMES_WEB_SEARCH_TIMEOUT");
    if (v) { int t = atoi(v); if (t > 0) cfg->tools.web_search_timeout = t; }

    /* P5-P14 env overrides */
    v = getenv("HERMES_DELEGATION_MAX_CONCURRENT");
    if (v) { int t = atoi(v); if (t > 0) cfg->delegation.max_concurrent_children = t; }

    v = getenv("HERMES_DELEGATION_SPAWN_DEPTH");
    if (v) { int t = atoi(v); if (t > 0) cfg->delegation.max_spawn_depth = t; }

    v = getenv("HERMES_DELEGATION_CHILD_TIMEOUT");
    if (v) { int t = atoi(v); if (t > 0) cfg->delegation.child_timeout = t; }

    v = getenv("HERMES_DELEGATION_CHILD_MODEL");
    if (v) snprintf(cfg->delegation.child_model, sizeof(cfg->delegation.child_model), "%s", v);

    v = getenv("HERMES_DELEGATION_CHILD_PROVIDER");
    if (v) snprintf(cfg->delegation.child_provider, sizeof(cfg->delegation.child_provider), "%s", v);

    v = getenv("HERMES_BROWSER_HEADLESS");
    if (v && (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0))
        cfg->browser_cfg.headless = false;

    v = getenv("HERMES_BROWSER_TIMEOUT");
    if (v) { int t = atoi(v); if (t > 0) cfg->browser_cfg.timeout = t; }

    v = getenv("HERMES_MEMORY_PROVIDER");
    if (v) snprintf(cfg->memory.provider, sizeof(cfg->memory.provider), "%s", v);

    v = getenv("HERMES_COMPRESSION_STRATEGY");
    if (v) snprintf(cfg->compression.strategy, sizeof(cfg->compression.strategy), "%s", v);

    v = getenv("HERMES_COMPRESSION_PROTECT_LAST_N");
    if (v) { int t = atoi(v); if (t > 0) cfg->compression.protect_last_n = t; }

    v = getenv("HERMES_COMPRESSION_PROTECT_FIRST_N");
    if (v) { int t = atoi(v); if (t >= 0) cfg->compression.protect_first_n = t; }

    v = getenv("HERMES_COMPRESSION_HARD_LIMIT");
    if (v) { int t = atoi(v); if (t > 0) cfg->compression.hygiene_hard_message_limit = t; }

    v = getenv("HERMES_CRON_DIR");
    if (v) snprintf(cfg->cron.dir, sizeof(cfg->cron.dir), "%s", v);

    v = getenv("HERMES_CRON_MAX_JOBS");
    if (v) { int t = atoi(v); if (t > 0) cfg->cron.max_concurrent_jobs = t; }

    v = getenv("HERMES_NOTIFY_PROVIDER");
    if (v) snprintf(cfg->notification.provider, sizeof(cfg->notification.provider), "%s", v);

    v = getenv("HERMES_TIRITH_PATH");
    if (v) snprintf(cfg->security.tirith_path, sizeof(cfg->security.tirith_path), "%s", v);

    v = getenv("HERMES_TIRITH_ENABLED");
    if (v && (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0))
        cfg->security.tirith_enabled = false;

    v = getenv("HERMES_SESSION_RETENTION_DAYS");
    if (v) { int t = atoi(v); if (t > 0) cfg->session.retention_days = t; }

    v = getenv("HERMES_MCP_TIMEOUT");
    if (v) { int t = atoi(v); if (t > 0) cfg->mcp.timeout = t; }

    v = getenv("HERMES_MCP_AUTH");
    if (v && (strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0))
        cfg->mcp.auth_enabled = true;

    /* Terminal env overrides */
    v = getenv("HERMES_TERMINAL_BACKEND");
    if (v) {
        snprintf(cfg->terminal.backend, sizeof(cfg->terminal.backend), "%s", v);
        snprintf(cfg->tools.terminal_backend, sizeof(cfg->tools.terminal_backend), "%s", v);
    }
    v = getenv("HERMES_TERMINAL_TIMEOUT");
    if (v) { int t = atoi(v); if (t > 0) {
        cfg->terminal.timeout = t;
        cfg->tools.terminal_timeout = t;
    }}
    v = getenv("HERMES_TERMINAL_PERSISTENT_SHELL");
    if (v && (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0)) {
        cfg->terminal.persistent_shell = false;
        cfg->tools.persistent_shell = false;
    }
    v = getenv("HERMES_TERMINAL_CWD");
    if (v) snprintf(cfg->terminal.cwd, sizeof(cfg->terminal.cwd), "%s", v);
    v = getenv("HERMES_TERMINAL_DOCKER_IMAGE");
    if (v) snprintf(cfg->terminal.docker_image, sizeof(cfg->terminal.docker_image), "%s", v);
    v = getenv("HERMES_TERMINAL_CONTAINER_CPU");
    if (v) { int t = atoi(v); if (t > 0) cfg->terminal.container_cpu = t; }
    v = getenv("HERMES_TERMINAL_CONTAINER_MEMORY");
    if (v) { int t = atoi(v); if (t > 0) cfg->terminal.container_memory = t; }
    v = getenv("HERMES_TERMINAL_CONTAINER_DISK");
    if (v) { int t = atoi(v); if (t > 0) cfg->terminal.container_disk = t; }
    v = getenv("HERMES_TERMINAL_CONTAINER_PERSISTENT");
    if (v && (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0))
        cfg->terminal.container_persistent = false;

    /* Logging env overrides */
    v = getenv("HERMES_LOG_LEVEL");
    if (v) snprintf(cfg->logging.level, sizeof(cfg->logging.level), "%s", v);
    v = getenv("HERMES_LOG_FORMAT");
    if (v) snprintf(cfg->logging.format, sizeof(cfg->logging.format), "%s", v);
    v = getenv("HERMES_LOG_DIR");
    if (v) snprintf(cfg->logging.dir, sizeof(cfg->logging.dir), "%s", v);
    v = getenv("HERMES_LOG_MAX_FILES");
    if (v) { int t = atoi(v); if (t > 0) cfg->logging.max_files = t; }
    v = getenv("HERMES_LOG_MAX_SIZE_MB");
    if (v) { int t = atoi(v); if (t > 0) cfg->logging.max_size_mb = t; }

    /* Skills env overrides */
    v = getenv("HERMES_SKILLS_DIR");
    if (v) snprintf(cfg->skills.dir, sizeof(cfg->skills.dir), "%s", v);
    v = getenv("HERMES_SKILLS_ENABLED");
    if (v) snprintf(cfg->skills.enabled, sizeof(cfg->skills.enabled), "%s", v);
    v = getenv("HERMES_SKILLS_AUTO_DISCOVER");
    if (v && (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0))
        cfg->skills.auto_discover = false;

    /* Checkpoints env overrides */
    v = getenv("HERMES_CHECKPOINTS_ENABLED");
    if (v && (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0))
        cfg->checkpoints.enabled = false;
    v = getenv("HERMES_CHECKPOINTS_INTERVAL");
    if (v) { int t = atoi(v); if (t > 0) cfg->checkpoints.interval = t; }
    v = getenv("HERMES_CHECKPOINTS_MAX");
    if (v) { int t = atoi(v); if (t > 0) cfg->checkpoints.max_checkpoints = t; }
    v = getenv("HERMES_CHECKPOINTS_DIR");
    if (v) snprintf(cfg->checkpoints.dir, sizeof(cfg->checkpoints.dir), "%s", v);

    /* Secrets env overrides (L01: Bitwarden Secrets Manager) */
    v = getenv("HERMES_SECRETS_BITWARDEN_ENABLED");
    if (v && (strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0))
        cfg->secrets.enabled = true;
    if (v && (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0))
        cfg->secrets.enabled = false;
    v = getenv("HERMES_SECRETS_BITWARDEN_ACCESS_TOKEN");
    if (v) snprintf(cfg->secrets.access_token, sizeof(cfg->secrets.access_token), "%s", v);
    v = getenv("HERMES_SECRETS_BITWARDEN_ORGANIZATION_ID");
    if (v) snprintf(cfg->secrets.organization_id, sizeof(cfg->secrets.organization_id), "%s", v);
    v = getenv("HERMES_SECRETS_BITWARDEN_BWS_PATH");
    if (v) snprintf(cfg->secrets.bws_path, sizeof(cfg->secrets.bws_path), "%s", v);
    v = getenv("HERMES_SECRETS_BITWARDEN_INSTALL_TIMEOUT");
    if (v) { int t = atoi(v); if (t > 0) cfg->secrets.install_timeout = t; }

    return true;
}

/* ================================================================
 *  P15: Config Validation
 * ================================================================ */

static void add_issue(config_validation_t *r, const char *key, const char *fmt, ...) {
    if (!r || r->count >= 64) return;
    snprintf(r->issues[r->count].key, sizeof(r->issues[r->count].key), "%s", key);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(r->issues[r->count].message, sizeof(r->issues[r->count].message), fmt, ap);
    va_end(ap);
    r->count++;
}

bool hermes_config_validate(const hermes_config_t *cfg, config_validation_t *result) {
    if (!cfg) return false;
    if (result) memset(result, 0, sizeof(*result));

    /* --- Provider group --- */
    if (cfg->provider_cfg.model[0] == '\0')
        add_issue(result, "model.default", "model name is empty");
    if (cfg->provider_cfg.provider[0] == '\0')
        add_issue(result, "model.provider", "provider name is empty");
    if (cfg->provider_cfg.temperature < 0.0f || cfg->provider_cfg.temperature > 2.0f)
        add_issue(result, "model.temperature", "must be 0.0-2.0, got %.1f", cfg->provider_cfg.temperature);
    if (cfg->provider_cfg.top_p < 0.0f || cfg->provider_cfg.top_p > 1.0f)
        add_issue(result, "model.top_p", "must be 0.0-1.0, got %.2f", cfg->provider_cfg.top_p);
    if (cfg->provider_cfg.max_tokens < 1 || cfg->provider_cfg.max_tokens > 1048576)
        add_issue(result, "model.max_tokens", "unreasonable value %d", cfg->provider_cfg.max_tokens);
    if (cfg->provider_cfg.presence_penalty < -2.0f || cfg->provider_cfg.presence_penalty > 2.0f)
        add_issue(result, "model.presence_penalty", "must be -2.0-2.0, got %.1f", cfg->provider_cfg.presence_penalty);
    if (cfg->provider_cfg.frequency_penalty < -2.0f || cfg->provider_cfg.frequency_penalty > 2.0f)
        add_issue(result, "model.frequency_penalty", "must be -2.0-2.0, got %.1f", cfg->provider_cfg.frequency_penalty);
    if (cfg->provider_cfg.top_logprobs < 0 || cfg->provider_cfg.top_logprobs > 20)
        add_issue(result, "model.top_logprobs", "must be 0-20, got %d", cfg->provider_cfg.top_logprobs);

    /* L04: Check each model slot for xAI retirement */
    {
        char repl[128], reff[64];
        if (xai_is_model_retired(cfg->provider_cfg.model, repl, sizeof(repl), reff, sizeof(reff)))
            add_issue(result, "model.default", "xAI model '%s' retired May 15, 2026 → use '%s'%s",
                      cfg->provider_cfg.model, repl,
                      reff[0] ? reff : "");
        if (cfg->tools.vision_model[0] &&
            xai_is_model_retired(cfg->tools.vision_model, repl, sizeof(repl), reff, sizeof(reff)))
            add_issue(result, "tools.vision_model",
                      "xAI model '%s' retired → use '%s'", cfg->tools.vision_model, repl);
        if (cfg->delegation.child_model[0] &&
            xai_is_model_retired(cfg->delegation.child_model, repl, sizeof(repl), reff, sizeof(reff)))
            add_issue(result, "delegation.model",
                      "xAI model '%s' retired → use '%s'", cfg->delegation.child_model, repl);
        if (cfg->compression.model[0] &&
            xai_is_model_retired(cfg->compression.model, repl, sizeof(repl), reff, sizeof(reff)))
            add_issue(result, "compression.model",
                      "xAI model '%s' retired → use '%s'", cfg->compression.model, repl);
    }

    /* Validate api_mode enum */
    if (cfg->provider_cfg.api_mode[0] &&
        strcmp(cfg->provider_cfg.api_mode, "chat_completions") != 0 &&
        strcmp(cfg->provider_cfg.api_mode, "codex_responses") != 0)
        add_issue(result, "model.api_mode", "unknown mode '%s'", cfg->provider_cfg.api_mode);

    /* Validate reasoning_effort enum */
    if (cfg->provider_cfg.reasoning_effort[0] &&
        strcmp(cfg->provider_cfg.reasoning_effort, "low") != 0 &&
        strcmp(cfg->provider_cfg.reasoning_effort, "medium") != 0 &&
        strcmp(cfg->provider_cfg.reasoning_effort, "high") != 0)
        add_issue(result, "model.reasoning_effort", "unknown '%s' (low/medium/high)", cfg->provider_cfg.reasoning_effort);

    /* --- Display group --- */
    if (cfg->display.language[0] &&
        strcmp(cfg->display.language, "en") != 0 &&
        strcmp(cfg->display.language, "zh") != 0 &&
        strcmp(cfg->display.language, "ja") != 0)
        add_issue(result, "display.language", "unsupported '%s'", cfg->display.language);

    /* --- Agent group --- */
    if (cfg->agent.max_iterations < 1 || cfg->agent.max_iterations > 10000)
        add_issue(result, "agent.max_turns", "unreasonable %d", cfg->agent.max_iterations);
    if (cfg->agent.max_tool_calls_round < 0 || cfg->agent.max_tool_calls_round > 1000)
        add_issue(result, "agent.max_tool_calls_round", "unreasonable %d", cfg->agent.max_tool_calls_round);
    if (cfg->agent.verbose_level < 0 || cfg->agent.verbose_level > 2)
        add_issue(result, "agent.verbose", "must be 0-2, got %d", cfg->agent.verbose_level);
    if (cfg->agent.compress_threshold < 0.0f || cfg->agent.compress_threshold > 1.0f)
        add_issue(result, "compression.threshold", "must be 0.0-1.0, got %.2f", cfg->agent.compress_threshold);
    if (cfg->agent.api_max_retries < 0 || cfg->agent.api_max_retries > 100)
        add_issue(result, "agent.api_max_retries", "unreasonable %d", cfg->agent.api_max_retries);

    /* --- Tools group --- */
    if (cfg->tools.approval_mode[0] &&
        strcmp(cfg->tools.approval_mode, "off") != 0 &&
        strcmp(cfg->tools.approval_mode, "manual") != 0 &&
        strcmp(cfg->tools.approval_mode, "auto") != 0)
        add_issue(result, "approvals.mode", "unknown '%s'", cfg->tools.approval_mode);
    if (cfg->tools.approval_timeout < 0 || cfg->tools.approval_timeout > 86400)
        add_issue(result, "approvals.timeout", "unreasonable %d", cfg->tools.approval_timeout);
    if (cfg->tools.max_result_size < 256)
        add_issue(result, "tool_output.max_bytes", "too small %d, min 256", cfg->tools.max_result_size);
    if (cfg->tools.terminal_timeout < 1 || cfg->tools.terminal_timeout > 86400)
        add_issue(result, "terminal.timeout", "unreasonable %d", cfg->tools.terminal_timeout);

    /* --- Delegation --- */
    if (cfg->delegation.max_concurrent_children < 1 || cfg->delegation.max_concurrent_children > 50)
        add_issue(result, "delegation.max_concurrent_children", "unreasonable %d", cfg->delegation.max_concurrent_children);
    if (cfg->delegation.max_spawn_depth < 0 || cfg->delegation.max_spawn_depth > 10)
        add_issue(result, "delegation.max_spawn_depth", "unreasonable %d", cfg->delegation.max_spawn_depth);

    /* --- Security --- */
    if (cfg->security.tirith_timeout < 0 || cfg->security.tirith_timeout > 300)
        add_issue(result, "security.tirith_timeout", "unreasonable %d", cfg->security.tirith_timeout);

    /* --- Session --- */
    if (cfg->session.retention_days < 0 || cfg->session.retention_days > 3650)
        add_issue(result, "sessions.retention_days", "unreasonable %d", cfg->session.retention_days);

    /* --- Browser (P6) --- */
    if (cfg->browser_cfg.browser_type[0] &&
        strcmp(cfg->browser_cfg.browser_type, "auto") != 0 &&
        strcmp(cfg->browser_cfg.browser_type, "chromium") != 0 &&
        strcmp(cfg->browser_cfg.browser_type, "firefox") != 0)
        add_issue(result, "browser.engine", "unknown '%s' (auto/chromium/firefox)", cfg->browser_cfg.browser_type);
    if (cfg->browser_cfg.viewport_width < 320 || cfg->browser_cfg.viewport_width > 7680)
        add_issue(result, "browser.viewport_width", "unreasonable %d", cfg->browser_cfg.viewport_width);
    if (cfg->browser_cfg.viewport_height < 240 || cfg->browser_cfg.viewport_height > 4320)
        add_issue(result, "browser.viewport_height", "unreasonable %d", cfg->browser_cfg.viewport_height);
    if (cfg->browser_cfg.timeout < 1 || cfg->browser_cfg.timeout > 300)
        add_issue(result, "browser.timeout", "unreasonable %d", cfg->browser_cfg.timeout);

    /* --- Memory (P7) --- */
    if (cfg->memory.char_limit < 100 || cfg->memory.char_limit > 1000000)
        add_issue(result, "memory.char_limit", "unreasonable %d", cfg->memory.char_limit);
    if (cfg->memory.ttl_days < 0 || cfg->memory.ttl_days > 36500)
        add_issue(result, "memory.ttl_days", "out of range 0-36500, got %d", cfg->memory.ttl_days);
    if (cfg->memory.storage_type < 0 || cfg->memory.storage_type > 3)
        add_issue(result, "memory.storage_type", "must be 0-3 (0=inmem,1=file,2=sqlite,3=plugin), got %d", cfg->memory.storage_type);

    /* --- Compression (P8) --- */
    if (cfg->compression.strategy[0] &&
        strcmp(cfg->compression.strategy, "smart") != 0 &&
        strcmp(cfg->compression.strategy, "summary") != 0 &&
        strcmp(cfg->compression.strategy, "extractive") != 0)
        add_issue(result, "compression.strategy", "unknown '%s' (smart/summary/extractive)", cfg->compression.strategy);
    if (cfg->compression.target_ratio < 0.1f || cfg->compression.target_ratio > 0.9f)
        add_issue(result, "compression.target_ratio", "must be 0.1-0.9, got %.2f", cfg->compression.target_ratio);
    if (cfg->compression.min_messages < 2 || cfg->compression.min_messages > 1000)
        add_issue(result, "compression.min_messages", "unreasonable %d", cfg->compression.min_messages);

    /* --- Cron (P9) --- */
    if (cfg->cron.max_concurrent_jobs < 0 || cfg->cron.max_concurrent_jobs > 100)
        add_issue(result, "cron.max_concurrent_jobs", "unreasonable %d", cfg->cron.max_concurrent_jobs);
    if (cfg->cron.job_timeout < 1 || cfg->cron.job_timeout > 86400)
        add_issue(result, "cron.job_timeout", "unreasonable %d", cfg->cron.job_timeout);
    if (cfg->cron.retention_days < 0 || cfg->cron.retention_days > 36500)
        add_issue(result, "cron.retention_days", "unreasonable %d", cfg->cron.retention_days);

    /* --- Plugin (P13) --- */
    if (cfg->plugin.dirs[0] == '\0')
        add_issue(result, "plugin.dirs", "plugin directories not configured");

    /* --- MCP (P14) --- */
    if (cfg->mcp.timeout < 1 || cfg->mcp.timeout > 300)
        add_issue(result, "mcp.timeout", "unreasonable %d", cfg->mcp.timeout);
    if (cfg->mcp.max_tools < 1 || cfg->mcp.max_tools > 256)
        add_issue(result, "mcp.max_tools", "unreasonable %d", cfg->mcp.max_tools);

    /* --- Terminal --- */
    if (cfg->terminal.timeout < 1 || cfg->terminal.timeout > 86400)
        add_issue(result, "terminal.timeout", "unreasonable %d", cfg->terminal.timeout);
    if (cfg->terminal.backend[0] &&
        strcmp(cfg->terminal.backend, "local") != 0 &&
        strcmp(cfg->terminal.backend, "ssh") != 0 &&
        strcmp(cfg->terminal.backend, "docker") != 0 &&
        strcmp(cfg->terminal.backend, "modal") != 0 &&
        strcmp(cfg->terminal.backend, "daytona") != 0 &&
        strcmp(cfg->terminal.backend, "singularity") != 0)
        add_issue(result, "terminal.backend", "unknown '%s' (local/ssh/docker/modal/daytona/singularity)", cfg->terminal.backend);
    if (cfg->terminal.container_cpu < 1 || cfg->terminal.container_cpu > 128)
        add_issue(result, "terminal.container_cpu", "unreasonable %d", cfg->terminal.container_cpu);
    if (cfg->terminal.container_memory < 128 || cfg->terminal.container_memory > 1048576)
        add_issue(result, "terminal.container_memory", "unreasonable %d MB", cfg->terminal.container_memory);

    /* --- Logging --- */
    if (cfg->logging.level[0] &&
        strcmp(cfg->logging.level, "debug") != 0 &&
        strcmp(cfg->logging.level, "info") != 0 &&
        strcmp(cfg->logging.level, "warning") != 0 &&
        strcmp(cfg->logging.level, "error") != 0)
        add_issue(result, "logging.level", "unknown '%s' (debug/info/warning/error)", cfg->logging.level);
    if (cfg->logging.format[0] &&
        strcmp(cfg->logging.format, "text") != 0 &&
        strcmp(cfg->logging.format, "json") != 0)
        add_issue(result, "logging.format", "unknown '%s' (text/json)", cfg->logging.format);
    if (cfg->logging.max_files < 1 || cfg->logging.max_files > 1000)
        add_issue(result, "logging.max_files", "unreasonable %d", cfg->logging.max_files);
    if (cfg->logging.max_size_mb < 1 || cfg->logging.max_size_mb > 10240)
        add_issue(result, "logging.max_size_mb", "unreasonable %d MB", cfg->logging.max_size_mb);

    /* --- Skills --- */
    if (cfg->skills.validate_on_load < 0 || cfg->skills.validate_on_load > 2)
        add_issue(result, "skills.validate", "must be 0-2 (0=no,1=warn,2=strict), got %d", cfg->skills.validate_on_load);
    if (cfg->skills.bundle_size_limit < 1 || cfg->skills.bundle_size_limit > 65536)
        add_issue(result, "skills.bundle_size_limit", "unreasonable %d KB", cfg->skills.bundle_size_limit);

    /* --- Checkpoints --- */
    if (cfg->checkpoints.interval < 1 || cfg->checkpoints.interval > 1000)
        add_issue(result, "checkpoints.interval", "unreasonable %d turns", cfg->checkpoints.interval);
    if (cfg->checkpoints.max_checkpoints < 1 || cfg->checkpoints.max_checkpoints > 1000)
        add_issue(result, "checkpoints.max", "unreasonable %d", cfg->checkpoints.max_checkpoints);
    if (cfg->checkpoints.compression_level < 0 || cfg->checkpoints.compression_level > 9)
        add_issue(result, "checkpoints.compression", "must be 0-9, got %d", cfg->checkpoints.compression_level);

    /* --- Secrets --- */
    if (cfg->secrets.enabled && !cfg->secrets.access_token[0])
        add_issue(result, "secrets.bitwarden.access_token", "required when enabled=true");

    /* --- Auxiliary --- */
    if (cfg->auxiliary.vision.timeout < 1 || cfg->auxiliary.vision.timeout > 3600)
        add_issue(result, "auxiliary.vision.timeout", "unreasonable %d", cfg->auxiliary.vision.timeout);
    if (cfg->auxiliary.vision_download_timeout < 1 || cfg->auxiliary.vision_download_timeout > 300)
        add_issue(result, "auxiliary.vision.download_timeout", "unreasonable %d", cfg->auxiliary.vision_download_timeout);
    if (cfg->auxiliary.web_extract.timeout < 1 || cfg->auxiliary.web_extract.timeout > 3600)
        add_issue(result, "auxiliary.web_extract.timeout", "unreasonable %d", cfg->auxiliary.web_extract.timeout);

    /* --- TTS --- */
    if (cfg->tts.xai_sample_rate < 8000 || cfg->tts.xai_sample_rate > 192000)
        add_issue(result, "tts.xai.sample_rate", "unreasonable %d", cfg->tts.xai_sample_rate);

    /* --- STT --- */
    if (cfg->voice.silence_threshold < 0 || cfg->voice.silence_threshold > 32767)
        add_issue(result, "voice.silence_threshold", "unreasonable %d", cfg->voice.silence_threshold);

    return result ? result->count == 0 : true;
}

/* ================================================================
 *  P17: Config Profiles
 * ================================================================ */

bool hermes_config_load_profile(hermes_config_t *cfg, const char *profile_name, const char *config_dir) {
    if (!profile_name || profile_name[0] == '\0') return false;

    char profile_path[HERMES_PATH_MAX];
    if (config_dir && config_dir[0])
        snprintf(profile_path, sizeof(profile_path), "%s/profiles/%s.yaml", config_dir, profile_name);
    else {
        char profiles_sub[HERMES_PATH_MAX];
        snprintf(profiles_sub, sizeof(profiles_sub), "profiles/%s.yaml", profile_name);
        hermes_resolve_path(profiles_sub, profile_path, sizeof(profile_path));
    }

    /* Check file exists */
    struct stat st;
    if (stat(profile_path, &st) != 0) return false;

    char *err = NULL;
    yaml_doc_t *doc = yaml_parse_file(profile_path, &err);
    if (!doc) {
        if (err) fprintf(stderr, "Warning: profile '%s' parse error: %s\n", profile_name, err);
        if (err) free(err);
        return false;
    }

    /* Merge profile settings — only override what's set in profile */
    /* Model */
    const char *v;
    v = yaml_get_string(doc, "model.default");
    if (v) snprintf(cfg->provider_cfg.model, sizeof(cfg->provider_cfg.model), "%s", v);
    v = yaml_get_string(doc, "model.provider");
    if (v) snprintf(cfg->provider_cfg.provider, sizeof(cfg->provider_cfg.provider), "%s", v);
    v = yaml_get_string(doc, "model.base_url");
    if (v) snprintf(cfg->provider_cfg.base_url, sizeof(cfg->provider_cfg.base_url), "%s", v);
    v = yaml_get_string(doc, "model.api_key");
    if (v) snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", v);
    v = yaml_get_string(doc, "model.api_mode");
    if (v) snprintf(cfg->provider_cfg.api_mode, sizeof(cfg->provider_cfg.api_mode), "%s", v);

    /* Agent */
    int iv = yaml_get_int(doc, "agent.max_turns", 0);
    if (iv > 0) { cfg->agent.max_iterations = iv; cfg->max_turns = iv; }
    iv = yaml_get_int(doc, "agent.verbose", 0);
    if (iv >= 0 && iv <= 2) { cfg->agent.verbose_level = iv; cfg->verbose = iv; }

    /* Display */
    v = yaml_get_string(doc, "display.skin");
    if (v) snprintf(cfg->display.skin, sizeof(cfg->display.skin), "%s", v);

    yaml_free(doc);
    return true;
}

/* ================================================================
 *  P18: Config default factory
 * ================================================================ */

void hermes_config_defaults(hermes_config_t *cfg) {
    hermes_config_load(cfg, NULL);
    /* Don't touch env_path/config_path — caller sets those */
}

/* Helper: add a diff entry for string or int fields */
static void diff_str(cfg_diff_t *d, const char *key,
                     const char *def, const char *act) {
    if (d->count >= 128) return;
    if (def[0] == '\0' && act[0] != '\0') {
        d->entries[d->count].type = CFG_DIFF_ADDED;
    } else if (def[0] != '\0' && act[0] == '\0') {
        d->entries[d->count].type = CFG_DIFF_MISSING;
    } else if (strcmp(def, act) != 0) {
        d->entries[d->count].type = CFG_DIFF_CHANGED;
    } else {
        return; /* identical, skip */
    }
    snprintf(d->entries[d->count].key, sizeof(d->entries[d->count].key), "%s", key);
    snprintf(d->entries[d->count].default_value, sizeof(d->entries[d->count].default_value), "%s", def);
    snprintf(d->entries[d->count].active_value, sizeof(d->entries[d->count].active_value), "%s", act);
    d->count++;
}

static void diff_int(cfg_diff_t *d, const char *key, int def, int act) {
    char dbuf[32], abuf[32];
    snprintf(dbuf, sizeof(dbuf), "%d", def);
    snprintf(abuf, sizeof(abuf), "%d", act);
    diff_str(d, key, dbuf, abuf);
}

static void diff_bool(cfg_diff_t *d, const char *key, bool def, bool act) {
    diff_str(d, key, def ? "true" : "false", act ? "true" : "false");
}

static void diff_float(cfg_diff_t *d, const char *key, float def, float act) {
    char dbuf[32], abuf[32];
    snprintf(dbuf, sizeof(dbuf), "%.2f", (double)def);
    snprintf(abuf, sizeof(abuf), "%.2f", (double)act);
    diff_str(d, key, dbuf, abuf);
}

bool hermes_config_diff(const hermes_config_t *active, cfg_diff_t *diff) {
    memset(diff, 0, sizeof(*diff));
    hermes_config_t def;
    hermes_config_defaults(&def);

    /* Provider group */
    diff_str(diff, "model.default", def.provider_cfg.model, active->provider_cfg.model);
    diff_str(diff, "model.provider", def.provider_cfg.provider, active->provider_cfg.provider);
    diff_str(diff, "model.base_url", def.provider_cfg.base_url, active->provider_cfg.base_url);
    diff_str(diff, "model.api_mode", def.provider_cfg.api_mode, active->provider_cfg.api_mode);
    diff_str(diff, "model.fallback_model", def.provider_cfg.fallback_model, active->provider_cfg.fallback_model);
    diff_str(diff, "model.fallback_providers", def.provider_cfg.fallback_providers, active->provider_cfg.fallback_providers);
    diff_str(diff, "model.service_tier", def.provider_cfg.service_tier, active->provider_cfg.service_tier);
    diff_int(diff, "model.max_tokens", def.provider_cfg.max_tokens, active->provider_cfg.max_tokens);
    diff_float(diff, "model.temperature", def.provider_cfg.temperature, active->provider_cfg.temperature);
    diff_float(diff, "model.top_p", def.provider_cfg.top_p, active->provider_cfg.top_p);
    diff_float(diff, "model.presence_penalty", def.provider_cfg.presence_penalty, active->provider_cfg.presence_penalty);
    diff_float(diff, "model.frequency_penalty", def.provider_cfg.frequency_penalty, active->provider_cfg.frequency_penalty);
    diff_int(diff, "model.seed", def.provider_cfg.seed, active->provider_cfg.seed);
    diff_bool(diff, "model.logprobs", def.provider_cfg.logprobs, active->provider_cfg.logprobs);
    diff_int(diff, "model.top_logprobs", def.provider_cfg.top_logprobs, active->provider_cfg.top_logprobs);
    diff_str(diff, "model.user", def.provider_cfg.user, active->provider_cfg.user);
    diff_str(diff, "model.response_format", def.provider_cfg.response_format, active->provider_cfg.response_format);
    diff_str(diff, "model.metadata", def.provider_cfg.metadata, active->provider_cfg.metadata);
    diff_str(diff, "model.tool_choice", def.provider_cfg.tool_choice, active->provider_cfg.tool_choice);
    diff_bool(diff, "model.parallel_tool_calls", def.provider_cfg.parallel_tool_calls, active->provider_cfg.parallel_tool_calls);
    diff_int(diff, "model.max_tool_calls", def.provider_cfg.max_tool_calls, active->provider_cfg.max_tool_calls);
    diff_int(diff, "model.n", def.provider_cfg.n, active->provider_cfg.n);
    diff_int(diff, "model.top_k", def.provider_cfg.top_k, active->provider_cfg.top_k);
    diff_int(diff, "model.candidate_count", def.provider_cfg.candidate_count, active->provider_cfg.candidate_count);
    diff_bool(diff, "model.json_mode", def.provider_cfg.json_mode, active->provider_cfg.json_mode);
    diff_bool(diff, "model.response_format_strict", def.provider_cfg.response_format_strict, active->provider_cfg.response_format_strict);
    diff_str(diff, "model.safety_settings", def.provider_cfg.safety_settings, active->provider_cfg.safety_settings);
    diff_str(diff, "azure.deployment_id", def.provider_cfg.azure_deployment_id, active->provider_cfg.azure_deployment_id);
    diff_str(diff, "azure.api_version", def.provider_cfg.azure_api_version, active->provider_cfg.azure_api_version);
    diff_str(diff, "openrouter.provider", def.provider_cfg.openrouter_provider, active->provider_cfg.openrouter_provider);
    diff_str(diff, "bedrock.inference_profile", def.provider_cfg.bedrock_inference_profile, active->provider_cfg.bedrock_inference_profile);
    diff_str(diff, "bedrock.guardrail_config", def.provider_cfg.bedrock_guardrail_config, active->provider_cfg.bedrock_guardrail_config);
    diff_bool(diff, "bedrock.trace_enabled", def.provider_cfg.bedrock_trace_enabled, active->provider_cfg.bedrock_trace_enabled);
    diff_bool(diff, "model.supports_vision", def.provider_cfg.supports_vision, active->provider_cfg.supports_vision);

    /* Display group */
    diff_str(diff, "display.skin", def.display.skin, active->display.skin);
    diff_str(diff, "display.banner", def.display.banner, active->display.banner);
    diff_str(diff, "display.spinner_style", def.display.spinner_style, active->display.spinner_style);
    diff_str(diff, "display.indicator", def.display.indicator, active->display.indicator);
    diff_str(diff, "display.language", def.display.language, active->display.language);
    diff_bool(diff, "display.streaming", def.display.stream, active->display.stream);
    diff_bool(diff, "display.show_reasoning", def.display.show_reasoning, active->display.show_reasoning);
    diff_bool(diff, "display.compact", def.display.compact, active->display.compact);

    /* Agent group */
    diff_int(diff, "agent.max_turns", def.agent.max_iterations, active->agent.max_iterations);
    diff_int(diff, "agent.max_tool_calls_round", def.agent.max_tool_calls_round, active->agent.max_tool_calls_round);
    diff_int(diff, "agent.verbose", def.agent.verbose_level, active->agent.verbose_level);
    diff_int(diff, "agent.api_max_retries", def.agent.api_max_retries, active->agent.api_max_retries);
    diff_int(diff, "agent.clarify_timeout", def.agent.clarify_timeout, active->agent.clarify_timeout);
    diff_float(diff, "compression.threshold", def.agent.compress_threshold, active->agent.compress_threshold);
    diff_int(diff, "compression.protect_last_n", def.compression.protect_last_n, active->compression.protect_last_n);
    diff_int(diff, "compression.protect_first_n", def.compression.protect_first_n, active->compression.protect_first_n);
    diff_int(diff, "compression.hygiene_hard_message_limit", def.compression.hygiene_hard_message_limit, active->compression.hygiene_hard_message_limit);
    diff_str(diff, "agent.system_prompt", def.agent.system_prompt, active->agent.system_prompt);
    diff_str(diff, "agent.profile", def.agent.profile, active->agent.profile);

    /* Tools group */
    diff_str(diff, "approvals.mode", def.tools.approval_mode, active->tools.approval_mode);
    diff_int(diff, "approvals.timeout", def.tools.approval_timeout, active->tools.approval_timeout);
    diff_int(diff, "tool_output.max_bytes", def.tools.max_result_size, active->tools.max_result_size);
    diff_int(diff, "terminal.timeout", def.tools.terminal_timeout, active->tools.terminal_timeout);
    /* Auxiliary group — diff all 11 sub-tasks */
    #define D_AUX_STR(task, field) diff_str(diff, "auxiliary." #task "." #field, def.auxiliary.task.field, active->auxiliary.task.field)
    #define D_AUX_INT(task, field) diff_int(diff, "auxiliary." #task "." #field, def.auxiliary.task.field, active->auxiliary.task.field)
    D_AUX_STR(vision, provider); D_AUX_STR(vision, model); D_AUX_STR(vision, base_url);
    D_AUX_STR(vision, api_key); D_AUX_INT(vision, timeout); D_AUX_STR(vision, extra_body);
    diff_int(diff, "auxiliary.vision.download_timeout", def.auxiliary.vision_download_timeout, active->auxiliary.vision_download_timeout);
    D_AUX_STR(web_extract, provider); D_AUX_STR(web_extract, model); D_AUX_STR(web_extract, base_url);
    D_AUX_STR(web_extract, api_key); D_AUX_INT(web_extract, timeout); D_AUX_STR(web_extract, extra_body);
    D_AUX_STR(compression, provider); D_AUX_STR(compression, model); D_AUX_STR(compression, base_url);
    D_AUX_STR(compression, api_key); D_AUX_INT(compression, timeout); D_AUX_STR(compression, extra_body);
    D_AUX_STR(skills_hub, provider); D_AUX_STR(skills_hub, model); D_AUX_STR(skills_hub, base_url);
    D_AUX_STR(skills_hub, api_key); D_AUX_INT(skills_hub, timeout); D_AUX_STR(skills_hub, extra_body);
    D_AUX_STR(approval, provider); D_AUX_STR(approval, model); D_AUX_STR(approval, base_url);
    D_AUX_STR(approval, api_key); D_AUX_INT(approval, timeout); D_AUX_STR(approval, extra_body);
    D_AUX_STR(mcp, provider); D_AUX_STR(mcp, model); D_AUX_STR(mcp, base_url);
    D_AUX_STR(mcp, api_key); D_AUX_INT(mcp, timeout); D_AUX_STR(mcp, extra_body);
    D_AUX_STR(title_generation, provider); D_AUX_STR(title_generation, model); D_AUX_STR(title_generation, base_url);
    D_AUX_STR(title_generation, api_key); D_AUX_INT(title_generation, timeout); D_AUX_STR(title_generation, extra_body);
    D_AUX_STR(triage_specifier, provider); D_AUX_STR(triage_specifier, model); D_AUX_STR(triage_specifier, base_url);
    D_AUX_STR(triage_specifier, api_key); D_AUX_INT(triage_specifier, timeout); D_AUX_STR(triage_specifier, extra_body);
    D_AUX_STR(kanban_decomposer, provider); D_AUX_STR(kanban_decomposer, model); D_AUX_STR(kanban_decomposer, base_url);
    D_AUX_STR(kanban_decomposer, api_key); D_AUX_INT(kanban_decomposer, timeout); D_AUX_STR(kanban_decomposer, extra_body);
    D_AUX_STR(profile_describer, provider); D_AUX_STR(profile_describer, model); D_AUX_STR(profile_describer, base_url);
    D_AUX_STR(profile_describer, api_key); D_AUX_INT(profile_describer, timeout); D_AUX_STR(profile_describer, extra_body);
    D_AUX_STR(curator, provider); D_AUX_STR(curator, model); D_AUX_STR(curator, base_url);
    D_AUX_STR(curator, api_key); D_AUX_INT(curator, timeout); D_AUX_STR(curator, extra_body);
    #undef D_AUX_STR
    #undef D_AUX_INT
    diff_str(diff, "terminal.backend", def.tools.terminal_backend, active->tools.terminal_backend);
    diff_bool(diff, "terminal.persistent_shell", def.tools.persistent_shell, active->tools.persistent_shell);
    diff_str(diff, "web.backend", def.tools.web_backend, active->tools.web_backend);
    diff_str(diff, "web.search_backend", def.tools.web_search_backend, active->tools.web_search_backend);
    diff_str(diff, "web.extract_backend", def.tools.web_extract_backend, active->tools.web_extract_backend);
    diff_int(diff, "web.search_timeout", def.tools.web_search_timeout, active->tools.web_search_timeout);
    diff_str(diff, "tools.enabled_toolsets", def.tools.enabled_toolsets, active->tools.enabled_toolsets);
    diff_str(diff, "tools.disabled_toolsets", def.tools.disabled_toolsets, active->tools.disabled_toolsets);

    /* Delegation */
    diff_int(diff, "delegation.max_concurrent_children",
             def.delegation.max_concurrent_children, active->delegation.max_concurrent_children);
    diff_int(diff, "delegation.max_spawn_depth",
             def.delegation.max_spawn_depth, active->delegation.max_spawn_depth);
    diff_str(diff, "delegation.model", def.delegation.child_model, active->delegation.child_model);

    /* Security */
    diff_bool(diff, "security.tirith_enabled", def.security.tirith_enabled, active->security.tirith_enabled);

    /* Session */
    diff_int(diff, "sessions.retention_days", def.session.retention_days, active->session.retention_days);

    /* Terminal (expanded) */
    diff_str(diff, "terminal.backend", def.terminal.backend, active->terminal.backend);
    diff_int(diff, "terminal.timeout", def.terminal.timeout, active->terminal.timeout);
    diff_str(diff, "terminal.cwd", def.terminal.cwd, active->terminal.cwd);
    diff_str(diff, "terminal.env_passthrough", def.terminal.env_passthrough, active->terminal.env_passthrough);
    diff_bool(diff, "terminal.auto_source_bashrc", def.terminal.auto_source_bashrc, active->terminal.auto_source_bashrc);
    diff_str(diff, "terminal.docker_image", def.terminal.docker_image, active->terminal.docker_image);

    /* Logging */
    diff_str(diff, "logging.level", def.logging.level, active->logging.level);
    diff_str(diff, "logging.format", def.logging.format, active->logging.format);
    diff_int(diff, "logging.max_files", def.logging.max_files, active->logging.max_files);
    diff_int(diff, "logging.max_size_mb", def.logging.max_size_mb, active->logging.max_size_mb);

    /* Skills */
    diff_bool(diff, "skills.auto_discover", def.skills.auto_discover, active->skills.auto_discover);
    diff_int(diff, "skills.validate", def.skills.validate_on_load, active->skills.validate_on_load);

    /* Checkpoints */
    diff_bool(diff, "checkpoints.enabled", def.checkpoints.enabled, active->checkpoints.enabled);
    diff_int(diff, "checkpoints.interval", def.checkpoints.interval, active->checkpoints.interval);
    diff_int(diff, "checkpoints.max", def.checkpoints.max_checkpoints, active->checkpoints.max_checkpoints);

    /* Secrets */
    diff_bool(diff, "secrets.bitwarden.enabled", def.secrets.enabled, active->secrets.enabled);
    diff_str(diff, "secrets.bitwarden.access_token",
             def.secrets.access_token[0] ? "***set***" : "",
             active->secrets.access_token[0] ? "***set***" : "");

    /* TTS */
    diff_str(diff, "tts.provider", def.tts.provider, active->tts.provider);
    diff_str(diff, "tts.edge.voice", def.tts.edge_voice, active->tts.edge_voice);
    diff_str(diff, "tts.openai.model", def.tts.openai_model, active->tts.openai_model);
    diff_str(diff, "tts.openai.voice", def.tts.openai_voice, active->tts.openai_voice);
    diff_str(diff, "tts.piper.voice", def.tts.piper_voice, active->tts.piper_voice);

    /* STT */
    diff_bool(diff, "stt.enabled", def.stt.enabled, active->stt.enabled);
    diff_str(diff, "stt.provider", def.stt.provider, active->stt.provider);
    diff_str(diff, "stt.local.model", def.stt.local_model, active->stt.local_model);

    /* Voice */
    diff_str(diff, "voice.record_key", def.voice.record_key, active->voice.record_key);
    diff_int(diff, "voice.max_recording_seconds", def.voice.max_recording_seconds, active->voice.max_recording_seconds);
    diff_bool(diff, "voice.auto_tts", def.voice.auto_tts, active->voice.auto_tts);

    return diff->count > 0;
}

/* ================================================================
 *  P20: Config Export
 * ================================================================ */

/* Helper: write a config value line */
static void exp_str(FILE *f, const char *key, const char *val) {
    if (val && val[0]) fprintf(f, "%s: %s\n", key, val);
}
static void exp_int(FILE *f, const char *key, int val) {
    fprintf(f, "%s: %d\n", key, val);
}
static void exp_bool(FILE *f, const char *key, bool val) {
    fprintf(f, "%s: %s\n", key, val ? "true" : "false");
}
static void exp_float(FILE *f, const char *key, float val) {
    fprintf(f, "%s: %.2f\n", key, (double)val);
}

bool hermes_config_export(const hermes_config_t *cfg, const char *path) {
    FILE *f = stdout;
    bool close_file = false;
    if (path && path[0]) {
        f = fopen(path, "w");
        if (!f) { fprintf(stderr, "Error: cannot write %s\n", path); return false; }
        close_file = true;
    }

    fprintf(f, "# Hermes C Config Export\n\n");
    exp_int(f, "config_version", cfg->config_version > 0 ? cfg->config_version : HERMES_CONFIG_VERSION);

    fprintf(f, "\nmodel:\n");
    exp_str(f, "  default", cfg->provider_cfg.model);
    exp_str(f, "  provider", cfg->provider_cfg.provider);
    exp_str(f, "  base_url", cfg->provider_cfg.base_url);
    exp_str(f, "  api_mode", cfg->provider_cfg.api_mode);
    exp_str(f, "  fallback_model", cfg->provider_cfg.fallback_model);
    exp_str(f, "  fallback_providers", cfg->provider_cfg.fallback_providers);
    exp_str(f, "  service_tier", cfg->provider_cfg.service_tier);
    exp_int(f, "  max_tokens", cfg->provider_cfg.max_tokens);
    exp_float(f, "  temperature", cfg->provider_cfg.temperature);
    exp_float(f, "  top_p", cfg->provider_cfg.top_p);
    exp_str(f, "  response_format", cfg->provider_cfg.response_format);
    exp_str(f, "  metadata", cfg->provider_cfg.metadata);
    exp_str(f, "  tool_choice", cfg->provider_cfg.tool_choice);
    exp_bool(f, "  parallel_tool_calls", cfg->provider_cfg.parallel_tool_calls);
    exp_int(f, "  max_tool_calls", cfg->provider_cfg.max_tool_calls);
    exp_int(f, "  n", cfg->provider_cfg.n);
    exp_str(f, "  azure_deployment_id", cfg->provider_cfg.azure_deployment_id);
    exp_str(f, "  azure_api_version", cfg->provider_cfg.azure_api_version);
    exp_str(f, "  openrouter_provider", cfg->provider_cfg.openrouter_provider);
    exp_str(f, "  bedrock_inference_profile", cfg->provider_cfg.bedrock_inference_profile);
    exp_str(f, "  bedrock_guardrail_config", cfg->provider_cfg.bedrock_guardrail_config);
    exp_bool(f, "  bedrock_trace_enabled", cfg->provider_cfg.bedrock_trace_enabled);
    exp_bool(f, "  json_mode", cfg->provider_cfg.json_mode);
    exp_bool(f, "  response_format_strict", cfg->provider_cfg.response_format_strict);
    exp_bool(f, "  supports_vision", cfg->provider_cfg.supports_vision);

    fprintf(f, "\ndisplay:\n");
    exp_str(f, "  skin", cfg->display.skin);
    exp_str(f, "  banner", cfg->display.banner);
    exp_str(f, "  spinner", cfg->display.spinner_style);
    exp_str(f, "  indicator", cfg->display.indicator);
    exp_str(f, "  language", cfg->display.language);
    exp_str(f, "  personality", cfg->display.personality);
    exp_bool(f, "  streaming", cfg->display.stream);
    exp_bool(f, "  show_reasoning", cfg->display.show_reasoning);
    exp_bool(f, "  compact", cfg->display.compact);
    exp_bool(f, "  show_cost", cfg->display.show_cost);
    exp_bool(f, "  timestamps", cfg->display.timestamps);

    fprintf(f, "\nagent:\n");
    exp_int(f, "  max_turns", cfg->agent.max_iterations);
    exp_int(f, "  max_tool_calls_round", cfg->agent.max_tool_calls_round);
    exp_int(f, "  verbose", cfg->agent.verbose_level);
    exp_int(f, "  api_max_retries", cfg->agent.api_max_retries);
    exp_int(f, "  clarify_timeout", cfg->agent.clarify_timeout);
    exp_float(f, "  compress_threshold", cfg->agent.compress_threshold);
    exp_str(f, "  system_prompt", cfg->agent.system_prompt);
    exp_str(f, "  profile", cfg->agent.profile);
    exp_str(f, "  reasoning_effort", cfg->agent.reasoning_effort);
    exp_bool(f, "  fast", cfg->agent.fast_mode);
    exp_bool(f, "  quiet", cfg->agent.quiet_mode);

    fprintf(f, "\nterminal:\n");
    exp_int(f, "  timeout", cfg->tools.terminal_timeout);
    exp_str(f, "  backend", cfg->tools.terminal_backend);
    exp_bool(f, "  persistent_shell", cfg->tools.persistent_shell);

    fprintf(f, "\nweb:\n");
    exp_str(f, "  backend", cfg->tools.web_backend);
    exp_str(f, "  search_backend", cfg->tools.web_search_backend);
    exp_str(f, "  extract_backend", cfg->tools.web_extract_backend);
    exp_int(f, "  search_timeout", cfg->tools.web_search_timeout);

    fprintf(f, "\napprovals:\n");
    exp_str(f, "  mode", cfg->tools.approval_mode);
    exp_int(f, "  timeout", cfg->tools.approval_timeout);

    fprintf(f, "\ntool_output:\n");
    exp_int(f, "  max_bytes", cfg->tools.max_result_size);

    fprintf(f, "\ncompression:\n");
    exp_str(f, "  model", cfg->compression.model);
    exp_str(f, "  strategy", cfg->compression.strategy);
    exp_float(f, "  target_ratio", cfg->compression.target_ratio);
    exp_int(f, "  min_messages", cfg->compression.min_messages);
    exp_bool(f, "  preserve_system", cfg->compression.preserve_system);
    exp_int(f, "  protect_last_n", cfg->compression.protect_last_n);
    exp_int(f, "  protect_first_n", cfg->compression.protect_first_n);
    exp_int(f, "  hygiene_hard_message_limit", cfg->compression.hygiene_hard_message_limit);
    exp_bool(f, "  abort_on_summary_failure", cfg->compression.abort_on_summary_failure);
    exp_bool(f, "  enabled", cfg->compress_enabled);

    fprintf(f, "\ndelegation:\n");
    exp_int(f, "  max_concurrent_children", cfg->delegation.max_concurrent_children);
    exp_int(f, "  max_spawn_depth", cfg->delegation.max_spawn_depth);
    exp_int(f, "  child_timeout_seconds", cfg->delegation.child_timeout);
    exp_str(f, "  model", cfg->delegation.child_model);
    exp_str(f, "  provider", cfg->delegation.child_provider);
    exp_int(f, "  max_iterations", cfg->delegation.child_max_turns);

    fprintf(f, "\nbrowser:\n");
    exp_str(f, "  cdp_url", cfg->browser_cfg.cdp_url);
    exp_str(f, "  engine", cfg->browser_cfg.browser_type);
    exp_bool(f, "  headless", cfg->browser_cfg.headless);
    exp_int(f, "  command_timeout", cfg->browser_cfg.timeout);

    fprintf(f, "\nmemory:\n");
    exp_str(f, "  provider", cfg->memory.provider);
    exp_int(f, "  memory_char_limit", cfg->memory.char_limit);
    exp_int(f, "  user_char_limit", cfg->memory.user_char_limit);

    fprintf(f, "\nsecurity:\n");
    exp_str(f, "  tirith_path", cfg->security.tirith_path);
    exp_int(f, "  tirith_timeout", cfg->security.tirith_timeout);
    exp_bool(f, "  tirith_enabled", cfg->security.tirith_enabled);
    exp_bool(f, "  allow_private_urls", cfg->security.allow_private_urls);

    fprintf(f, "\nsessions:\n");
    exp_int(f, "  retention_days", cfg->session.retention_days);

    fprintf(f, "\nmcp:\n");
    exp_int(f, "  timeout", cfg->mcp.timeout);
    exp_bool(f, "  auth_enabled", cfg->mcp.auth_enabled);

    fprintf(f, "\nterminal:\n");
    exp_str(f, "  backend", cfg->terminal.backend);
    exp_int(f, "  timeout", cfg->terminal.timeout);
    exp_bool(f, "  persistent_shell", cfg->terminal.persistent_shell);
    exp_str(f, "  cwd", cfg->terminal.cwd);
    exp_str(f, "  env_passthrough", cfg->terminal.env_passthrough);
    exp_str(f, "  docker_image", cfg->terminal.docker_image);
    exp_str(f, "  docker_forward_env", cfg->terminal.docker_forward_env);
    exp_str(f, "  singularity_image", cfg->terminal.singularity_image);
    exp_str(f, "  modal_image", cfg->terminal.modal_image);
    exp_int(f, "  container_cpu", cfg->terminal.container_cpu);
    exp_int(f, "  container_memory", cfg->terminal.container_memory);
    exp_int(f, "  container_disk", cfg->terminal.container_disk);
    exp_bool(f, "  container_persistent", cfg->terminal.container_persistent);

    fprintf(f, "\nlogging:\n");
    exp_str(f, "  level", cfg->logging.level);
    exp_str(f, "  format", cfg->logging.format);
    exp_str(f, "  dir", cfg->logging.dir);
    exp_int(f, "  max_files", cfg->logging.max_files);
    exp_int(f, "  max_size_mb", cfg->logging.max_size_mb);

    fprintf(f, "\nskills:\n");
    exp_str(f, "  dir", cfg->skills.dir);
    exp_str(f, "  enabled", cfg->skills.enabled);
    exp_bool(f, "  auto_discover", cfg->skills.auto_discover);
    exp_int(f, "  bundle_size_limit", cfg->skills.bundle_size_limit);
    exp_int(f, "  validate", cfg->skills.validate_on_load);

    fprintf(f, "\ncheckpoints:\n");
    exp_bool(f, "  enabled", cfg->checkpoints.enabled);
    exp_int(f, "  interval", cfg->checkpoints.interval);
    exp_int(f, "  max", cfg->checkpoints.max_checkpoints);
    exp_str(f, "  dir", cfg->checkpoints.dir);

    fprintf(f, "\nsecrets:\n  bitwarden:\n");
    exp_bool(f, "    enabled", cfg->secrets.enabled);
    exp_str(f, "    access_token", cfg->secrets.access_token);
    exp_str(f, "    organization_id", cfg->secrets.organization_id);
    exp_str(f, "    bws_path", cfg->secrets.bws_path);
    exp_int(f, "    install_timeout", cfg->secrets.install_timeout);

    /* Auxiliary export */
    #define E_AUX_TASK(f, task, nm) do {         fprintf(f, "\nauxiliary." #task ":\n");         exp_str(f, "  provider", cfg->auxiliary.task.provider);         exp_str(f, "  model", cfg->auxiliary.task.model);         exp_str(f, "  base_url", cfg->auxiliary.task.base_url);         exp_str(f, "  api_key", cfg->auxiliary.task.api_key);         exp_int(f, "  timeout", cfg->auxiliary.task.timeout);         exp_str(f, "  extra_body", cfg->auxiliary.task.extra_body);     } while(0)
    E_AUX_TASK(f, vision, "vision");
    exp_int(f, "  download_timeout", cfg->auxiliary.vision_download_timeout);
    E_AUX_TASK(f, web_extract, "web_extract");
    E_AUX_TASK(f, compression, "compression");
    E_AUX_TASK(f, skills_hub, "skills_hub");
    E_AUX_TASK(f, approval, "approval");
    E_AUX_TASK(f, mcp, "mcp");
    E_AUX_TASK(f, title_generation, "title_generation");
    E_AUX_TASK(f, triage_specifier, "triage_specifier");
    E_AUX_TASK(f, kanban_decomposer, "kanban_decomposer");
    E_AUX_TASK(f, profile_describer, "profile_describer");
    E_AUX_TASK(f, curator, "curator");
    #undef E_AUX_TASK

    fprintf(f, "\ntts:\n");
    exp_str(f, "  provider", cfg->tts.provider);
    exp_str(f, "  edge.voice", cfg->tts.edge_voice);
    exp_str(f, "  elevenlabs.voice_id", cfg->tts.elevenlabs_voice_id);
    exp_str(f, "  elevenlabs.model_id", cfg->tts.elevenlabs_model_id);
    exp_str(f, "  openai.model", cfg->tts.openai_model);
    exp_str(f, "  openai.voice", cfg->tts.openai_voice);
    exp_str(f, "  xai.voice_id", cfg->tts.xai_voice_id);
    exp_str(f, "  xai.language", cfg->tts.xai_language);
    exp_int(f, "  xai.sample_rate", cfg->tts.xai_sample_rate);
    exp_int(f, "  xai.bit_rate", cfg->tts.xai_bit_rate);
    exp_str(f, "  mistral.model", cfg->tts.mistral_model);
    exp_str(f, "  mistral.voice_id", cfg->tts.mistral_voice_id);
    exp_str(f, "  neutts.ref_audio", cfg->tts.neutts_ref_audio);
    exp_str(f, "  neutts.ref_text", cfg->tts.neutts_ref_text);
    exp_str(f, "  neutts.model", cfg->tts.neutts_model);
    exp_str(f, "  neutts.device", cfg->tts.neutts_device);
    exp_str(f, "  piper.voice", cfg->tts.piper_voice);

    fprintf(f, "\nstt:\n");
    exp_bool(f, "  enabled", cfg->stt.enabled);
    exp_str(f, "  provider", cfg->stt.provider);
    exp_str(f, "  local.model", cfg->stt.local_model);
    exp_str(f, "  local.language", cfg->stt.local_language);
    exp_str(f, "  openai.model", cfg->stt.openai_model);
    exp_str(f, "  mistral.model", cfg->stt.mistral_model);

    fprintf(f, "\nvoice:\n");
    exp_str(f, "  record_key", cfg->voice.record_key);
    exp_int(f, "  max_recording_seconds", cfg->voice.max_recording_seconds);
    exp_bool(f, "  auto_tts", cfg->voice.auto_tts);
    exp_bool(f, "  beep_enabled", cfg->voice.beep_enabled);
    exp_int(f, "  silence_threshold", cfg->voice.silence_threshold);
    exp_float(f, "  silence_duration", cfg->voice.silence_duration);

    if (close_file) fclose(f);
    return true;
}

bool hermes_config_import(hermes_config_t *cfg, const char *path) {
    if (!path || !path[0]) return false;

    char *err = NULL;
    yaml_doc_t *doc = yaml_parse_file(path, &err);
    if (!doc) {
        if (err) { fprintf(stderr, "Import error: %s\n", err); free(err); }
        return false;
    }

    /* Merge imported values over current config */
    const char *v;
    v = yaml_get_string(doc, "model.default");
    if (v) snprintf(cfg->provider_cfg.model, sizeof(cfg->provider_cfg.model), "%s", v);
    v = yaml_get_string(doc, "model.provider");
    if (v) snprintf(cfg->provider_cfg.provider, sizeof(cfg->provider_cfg.provider), "%s", v);
    v = yaml_get_string(doc, "model.base_url");
    if (v) snprintf(cfg->provider_cfg.base_url, sizeof(cfg->provider_cfg.base_url), "%s", v);
    v = yaml_get_string(doc, "model.api_key");
    if (v) snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", v);

    int iv;
    iv = yaml_get_int(doc, "agent.max_turns", 0);
    if (iv > 0) { cfg->agent.max_iterations = iv; cfg->max_turns = iv; }

    v = yaml_get_string(doc, "display.skin");
    if (v) snprintf(cfg->display.skin, sizeof(cfg->display.skin), "%s", v);

    v = yaml_get_string(doc, "approvals.mode");
    if (v) snprintf(cfg->tools.approval_mode, sizeof(cfg->tools.approval_mode), "%s", v);

    yaml_free(doc);
    return true;
}

/* ================================================================
 *  P22: Config merge — deep merge src into dst
 *  Only non-default (non-zero, non-empty) src fields overwrite dst.
 * ================================================================ */

/* Helpers: check if a string field is "set" (non-default) */
static bool is_set_str(const char *s) { return s && s[0] != '\0'; }
static bool is_set_int(int v) { return v != 0; }
static bool is_set_bool(bool v) { return v; }  /* bools always overwrite if true */

void hermes_config_merge(hermes_config_t *dst, const hermes_config_t *src) {
    /* Provider */
    if (is_set_str(src->provider_cfg.model))
        snprintf(dst->provider_cfg.model, sizeof(dst->provider_cfg.model), "%s", src->provider_cfg.model);
    if (is_set_str(src->provider_cfg.provider))
        snprintf(dst->provider_cfg.provider, sizeof(dst->provider_cfg.provider), "%s", src->provider_cfg.provider);
    if (is_set_str(src->provider_cfg.base_url))
        snprintf(dst->provider_cfg.base_url, sizeof(dst->provider_cfg.base_url), "%s", src->provider_cfg.base_url);
    if (is_set_str(src->provider_cfg.api_key))
        snprintf(dst->provider_cfg.api_key, sizeof(dst->provider_cfg.api_key), "%s", src->provider_cfg.api_key);
    if (is_set_str(src->provider_cfg.api_mode))
        snprintf(dst->provider_cfg.api_mode, sizeof(dst->provider_cfg.api_mode), "%s", src->provider_cfg.api_mode);
    if (is_set_str(src->provider_cfg.fallback_model))
        snprintf(dst->provider_cfg.fallback_model, sizeof(dst->provider_cfg.fallback_model), "%s", src->provider_cfg.fallback_model);
    if (is_set_str(src->provider_cfg.service_tier))
        snprintf(dst->provider_cfg.service_tier, sizeof(dst->provider_cfg.service_tier), "%s", src->provider_cfg.service_tier);
    if (src->provider_cfg.temperature >= 0.01f)
        dst->provider_cfg.temperature = src->provider_cfg.temperature;
    if (src->provider_cfg.top_p >= 0.01f)
        dst->provider_cfg.top_p = src->provider_cfg.top_p;
    if (is_set_int(src->provider_cfg.max_tokens))
        dst->provider_cfg.max_tokens = src->provider_cfg.max_tokens;

    /* Display */
    if (is_set_str(src->display.skin))
        snprintf(dst->display.skin, sizeof(dst->display.skin), "%s", src->display.skin);
    if (is_set_str(src->display.banner))
        snprintf(dst->display.banner, sizeof(dst->display.banner), "%s", src->display.banner);
    if (is_set_str(src->display.spinner_style))
        snprintf(dst->display.spinner_style, sizeof(dst->display.spinner_style), "%s", src->display.spinner_style);
    if (is_set_str(src->display.indicator))
        snprintf(dst->display.indicator, sizeof(dst->display.indicator), "%s", src->display.indicator);
    if (is_set_str(src->display.language))
        snprintf(dst->display.language, sizeof(dst->display.language), "%s", src->display.language);
    if (is_set_str(src->display.personality))
        snprintf(dst->display.personality, sizeof(dst->display.personality), "%s", src->display.personality);
    dst->display.stream = src->display.stream;
    dst->display.show_reasoning = src->display.show_reasoning;
    dst->display.compact = src->display.compact;
    dst->display.show_cost = src->display.show_cost;
    dst->display.timestamps = src->display.timestamps;

    /* Agent */
    if (is_set_int(src->agent.max_iterations))
        dst->agent.max_iterations = src->agent.max_iterations;
    if (is_set_int(src->agent.max_tool_calls_round))
        dst->agent.max_tool_calls_round = src->agent.max_tool_calls_round;
    if (is_set_int(src->agent.verbose_level))
        dst->agent.verbose_level = src->agent.verbose_level;
    if (is_set_int(src->agent.api_max_retries))
        dst->agent.api_max_retries = src->agent.api_max_retries;
    if (is_set_int(src->agent.clarify_timeout))
        dst->agent.clarify_timeout = src->agent.clarify_timeout;
    if (src->agent.compress_threshold >= 0.01f)
        dst->agent.compress_threshold = src->agent.compress_threshold;
    dst->agent.fast_mode = src->agent.fast_mode;
    dst->agent.quiet_mode = src->agent.quiet_mode;
    if (is_set_str(src->agent.system_prompt))
        snprintf(dst->agent.system_prompt, sizeof(dst->agent.system_prompt), "%s", src->agent.system_prompt);
    if (is_set_str(src->agent.profile))
        snprintf(dst->agent.profile, sizeof(dst->agent.profile), "%s", src->agent.profile);
    if (is_set_str(src->agent.reasoning_effort))
        snprintf(dst->agent.reasoning_effort, sizeof(dst->agent.reasoning_effort), "%s", src->agent.reasoning_effort);

    /* Tools */
    if (is_set_str(src->tools.approval_mode))
        snprintf(dst->tools.approval_mode, sizeof(dst->tools.approval_mode), "%s", src->tools.approval_mode);
    if (is_set_int(src->tools.approval_timeout))
        dst->tools.approval_timeout = src->tools.approval_timeout;
    if (is_set_int(src->tools.max_result_size))
        dst->tools.max_result_size = src->tools.max_result_size;
    if (is_set_int(src->tools.terminal_timeout))
        dst->tools.terminal_timeout = src->tools.terminal_timeout;
    if (is_set_str(src->tools.vision_model)) {
        snprintf(dst->tools.vision_model, sizeof(dst->tools.vision_model), "%s", src->tools.vision_model);
        snprintf(dst->auxiliary.vision.model, sizeof(dst->auxiliary.vision.model), "%s", src->tools.vision_model);
    }
    if (is_set_str(src->tools.terminal_backend))
        snprintf(dst->tools.terminal_backend, sizeof(dst->tools.terminal_backend), "%s", src->tools.terminal_backend);
    if (is_set_int(src->tools.web_search_timeout))
        dst->tools.web_search_timeout = src->tools.web_search_timeout;
    if (is_set_str(src->tools.web_backend))
        snprintf(dst->tools.web_backend, sizeof(dst->tools.web_backend), "%s", src->tools.web_backend);
    if (is_set_str(src->tools.web_search_backend))
        snprintf(dst->tools.web_search_backend, sizeof(dst->tools.web_search_backend), "%s", src->tools.web_search_backend);
    if (is_set_str(src->tools.web_extract_backend))
        snprintf(dst->tools.web_extract_backend, sizeof(dst->tools.web_extract_backend), "%s", src->tools.web_extract_backend);

    /* Delegation */
    if (is_set_int(src->delegation.max_concurrent_children))
        dst->delegation.max_concurrent_children = src->delegation.max_concurrent_children;
    if (is_set_int(src->delegation.max_spawn_depth))
        dst->delegation.max_spawn_depth = src->delegation.max_spawn_depth;
    if (is_set_int(src->delegation.child_timeout))
        dst->delegation.child_timeout = src->delegation.child_timeout;
    if (is_set_str(src->delegation.child_model))
        snprintf(dst->delegation.child_model, sizeof(dst->delegation.child_model), "%s", src->delegation.child_model);
    if (is_set_str(src->delegation.child_provider))
        snprintf(dst->delegation.child_provider, sizeof(dst->delegation.child_provider), "%s", src->delegation.child_provider);
    if (is_set_int(src->delegation.child_max_turns))
        dst->delegation.child_max_turns = src->delegation.child_max_turns;

    /* Browser */
    if (is_set_str(src->browser_cfg.cdp_url))
        snprintf(dst->browser_cfg.cdp_url, sizeof(dst->browser_cfg.cdp_url), "%s", src->browser_cfg.cdp_url);
    if (is_set_str(src->browser_cfg.browser_type))
        snprintf(dst->browser_cfg.browser_type, sizeof(dst->browser_cfg.browser_type), "%s", src->browser_cfg.browser_type);
    if (is_set_int(src->browser_cfg.timeout))
        dst->browser_cfg.timeout = src->browser_cfg.timeout;
    if (is_set_int(src->browser_cfg.viewport_width))
        dst->browser_cfg.viewport_width = src->browser_cfg.viewport_width;
    if (is_set_int(src->browser_cfg.viewport_height))
        dst->browser_cfg.viewport_height = src->browser_cfg.viewport_height;
    dst->browser_cfg.enable_javascript = src->browser_cfg.enable_javascript;

    /* Memory */
    if (is_set_str(src->memory.provider))
        snprintf(dst->memory.provider, sizeof(dst->memory.provider), "%s", src->memory.provider);
    if (is_set_int(src->memory.char_limit))
        dst->memory.char_limit = src->memory.char_limit;
    if (is_set_int(src->memory.user_char_limit))
        dst->memory.user_char_limit = src->memory.user_char_limit;
    if (is_set_int(src->memory.ttl_days))
        dst->memory.ttl_days = src->memory.ttl_days;
    if (is_set_int(src->memory.search_limit))
        dst->memory.search_limit = src->memory.search_limit;
    dst->memory.auto_save = src->memory.auto_save;
    dst->memory.compression_enabled = src->memory.compression_enabled;

    /* Compression */
    if (is_set_str(src->compression.model))
        snprintf(dst->compression.model, sizeof(dst->compression.model), "%s", src->compression.model);
    if (is_set_str(src->compression.strategy))
        snprintf(dst->compression.strategy, sizeof(dst->compression.strategy), "%s", src->compression.strategy);
    if (src->compression.target_ratio >= 0.01f)
        dst->compression.target_ratio = src->compression.target_ratio;
    if (is_set_int(src->compression.min_messages))
        dst->compression.min_messages = src->compression.min_messages;
    dst->compression.preserve_system = src->compression.preserve_system;
    if (is_set_int(src->compression.protect_last_n))
        dst->compression.protect_last_n = src->compression.protect_last_n;
    if (is_set_int(src->compression.protect_first_n))
        dst->compression.protect_first_n = src->compression.protect_first_n;
    if (is_set_int(src->compression.hygiene_hard_message_limit))
        dst->compression.hygiene_hard_message_limit = src->compression.hygiene_hard_message_limit;
    dst->compression.abort_on_summary_failure = src->compression.abort_on_summary_failure;

    /* Cron */
    if (is_set_str(src->cron.dir))
        snprintf(dst->cron.dir, sizeof(dst->cron.dir), "%s", src->cron.dir);
    if (is_set_int(src->cron.max_concurrent_jobs))
        dst->cron.max_concurrent_jobs = src->cron.max_concurrent_jobs;
    if (is_set_int(src->cron.job_timeout))
        dst->cron.job_timeout = src->cron.job_timeout;
    if (is_set_int(src->cron.retention_days))
        dst->cron.retention_days = src->cron.retention_days;
    dst->cron.notify_on_failure = src->cron.notify_on_failure;

    /* Notification */
    if (is_set_str(src->notification.provider))
        snprintf(dst->notification.provider, sizeof(dst->notification.provider), "%s", src->notification.provider);
    if (is_set_str(src->notification.sound))
        snprintf(dst->notification.sound, sizeof(dst->notification.sound), "%s", src->notification.sound);
    dst->notification.on_complete = src->notification.on_complete;
    dst->notification.on_error = src->notification.on_error;
    dst->notification.on_approval = src->notification.on_approval;

    /* Plugin */
    if (is_set_str(src->plugin.dirs))
        snprintf(dst->plugin.dirs, sizeof(dst->plugin.dirs), "%s", src->plugin.dirs);
    if (is_set_str(src->plugin.enabled))
        snprintf(dst->plugin.enabled, sizeof(dst->plugin.enabled), "%s", src->plugin.enabled);

    /* Security */
    if (is_set_int(src->security.tirith_timeout))
        dst->security.tirith_timeout = src->security.tirith_timeout;
    dst->security.tirith_enabled = src->security.tirith_enabled;
    dst->security.allow_private_urls = src->security.allow_private_urls;
    dst->security.website_blocklist_enabled = src->security.website_blocklist_enabled;

    /* Session */
    if (is_set_int(src->session.retention_days))
        dst->session.retention_days = src->session.retention_days;
    if (is_set_int(src->session.auto_save_interval))
        dst->session.auto_save_interval = src->session.auto_save_interval;
    dst->session.compress = src->session.compress;
    dst->session.store_trajectories = src->session.store_trajectories;

    /* MCP */
    if (is_set_int(src->mcp.timeout))
        dst->mcp.timeout = src->mcp.timeout;
    if (is_set_int(src->mcp.max_tools))
        dst->mcp.max_tools = src->mcp.max_tools;
    if (is_set_str(src->mcp.credential_store))
        snprintf(dst->mcp.credential_store, sizeof(dst->mcp.credential_store), "%s", src->mcp.credential_store);
    dst->mcp.auth_enabled = src->mcp.auth_enabled;

    /* Auxiliary merge */
    #define M_AUX_STR(task, field) if (is_set_str(src->auxiliary.task.field)) snprintf(dst->auxiliary.task.field, sizeof(dst->auxiliary.task.field), "%s", src->auxiliary.task.field)
    #define M_AUX_INT(task, field) if (is_set_int(src->auxiliary.task.field)) dst->auxiliary.task.field = src->auxiliary.task.field
    M_AUX_STR(vision, provider); M_AUX_STR(vision, model); M_AUX_STR(vision, base_url);
    M_AUX_STR(vision, api_key); M_AUX_INT(vision, timeout); M_AUX_STR(vision, extra_body);
    if (is_set_int(src->auxiliary.vision_download_timeout)) dst->auxiliary.vision_download_timeout = src->auxiliary.vision_download_timeout;
    M_AUX_STR(web_extract, provider); M_AUX_STR(web_extract, model); M_AUX_STR(web_extract, base_url);
    M_AUX_STR(web_extract, api_key); M_AUX_INT(web_extract, timeout); M_AUX_STR(web_extract, extra_body);
    M_AUX_STR(compression, provider); M_AUX_STR(compression, model); M_AUX_STR(compression, base_url);
    M_AUX_STR(compression, api_key); M_AUX_INT(compression, timeout); M_AUX_STR(compression, extra_body);
    M_AUX_STR(skills_hub, provider); M_AUX_STR(skills_hub, model); M_AUX_STR(skills_hub, base_url);
    M_AUX_STR(skills_hub, api_key); M_AUX_INT(skills_hub, timeout); M_AUX_STR(skills_hub, extra_body);
    M_AUX_STR(approval, provider); M_AUX_STR(approval, model); M_AUX_STR(approval, base_url);
    M_AUX_STR(approval, api_key); M_AUX_INT(approval, timeout); M_AUX_STR(approval, extra_body);
    M_AUX_STR(mcp, provider); M_AUX_STR(mcp, model); M_AUX_STR(mcp, base_url);
    M_AUX_STR(mcp, api_key); M_AUX_INT(mcp, timeout); M_AUX_STR(mcp, extra_body);
    M_AUX_STR(title_generation, provider); M_AUX_STR(title_generation, model); M_AUX_STR(title_generation, base_url);
    M_AUX_STR(title_generation, api_key); M_AUX_INT(title_generation, timeout); M_AUX_STR(title_generation, extra_body);
    M_AUX_STR(triage_specifier, provider); M_AUX_STR(triage_specifier, model); M_AUX_STR(triage_specifier, base_url);
    M_AUX_STR(triage_specifier, api_key); M_AUX_INT(triage_specifier, timeout); M_AUX_STR(triage_specifier, extra_body);
    M_AUX_STR(kanban_decomposer, provider); M_AUX_STR(kanban_decomposer, model); M_AUX_STR(kanban_decomposer, base_url);
    M_AUX_STR(kanban_decomposer, api_key); M_AUX_INT(kanban_decomposer, timeout); M_AUX_STR(kanban_decomposer, extra_body);
    M_AUX_STR(profile_describer, provider); M_AUX_STR(profile_describer, model); M_AUX_STR(profile_describer, base_url);
    M_AUX_STR(profile_describer, api_key); M_AUX_INT(profile_describer, timeout); M_AUX_STR(profile_describer, extra_body);
    M_AUX_STR(curator, provider); M_AUX_STR(curator, model); M_AUX_STR(curator, base_url);
    M_AUX_STR(curator, api_key); M_AUX_INT(curator, timeout); M_AUX_STR(curator, extra_body);
    #undef M_AUX_STR
    #undef M_AUX_INT

    /* TTS merge */
    if (is_set_str(src->tts.provider)) snprintf(dst->tts.provider, sizeof(dst->tts.provider), "%s", src->tts.provider);
    if (is_set_str(src->tts.edge_voice)) snprintf(dst->tts.edge_voice, sizeof(dst->tts.edge_voice), "%s", src->tts.edge_voice);
    if (is_set_str(src->tts.elevenlabs_voice_id)) snprintf(dst->tts.elevenlabs_voice_id, sizeof(dst->tts.elevenlabs_voice_id), "%s", src->tts.elevenlabs_voice_id);
    if (is_set_str(src->tts.elevenlabs_model_id)) snprintf(dst->tts.elevenlabs_model_id, sizeof(dst->tts.elevenlabs_model_id), "%s", src->tts.elevenlabs_model_id);
    if (is_set_str(src->tts.openai_model)) snprintf(dst->tts.openai_model, sizeof(dst->tts.openai_model), "%s", src->tts.openai_model);
    if (is_set_str(src->tts.openai_voice)) snprintf(dst->tts.openai_voice, sizeof(dst->tts.openai_voice), "%s", src->tts.openai_voice);
    if (is_set_str(src->tts.xai_voice_id)) snprintf(dst->tts.xai_voice_id, sizeof(dst->tts.xai_voice_id), "%s", src->tts.xai_voice_id);
    if (is_set_str(src->tts.xai_language)) snprintf(dst->tts.xai_language, sizeof(dst->tts.xai_language), "%s", src->tts.xai_language);
    if (is_set_int(src->tts.xai_sample_rate)) dst->tts.xai_sample_rate = src->tts.xai_sample_rate;
    if (is_set_int(src->tts.xai_bit_rate)) dst->tts.xai_bit_rate = src->tts.xai_bit_rate;
    if (is_set_str(src->tts.mistral_model)) snprintf(dst->tts.mistral_model, sizeof(dst->tts.mistral_model), "%s", src->tts.mistral_model);
    if (is_set_str(src->tts.mistral_voice_id)) snprintf(dst->tts.mistral_voice_id, sizeof(dst->tts.mistral_voice_id), "%s", src->tts.mistral_voice_id);
    if (is_set_str(src->tts.neutts_ref_audio)) snprintf(dst->tts.neutts_ref_audio, sizeof(dst->tts.neutts_ref_audio), "%s", src->tts.neutts_ref_audio);
    if (is_set_str(src->tts.neutts_ref_text)) snprintf(dst->tts.neutts_ref_text, sizeof(dst->tts.neutts_ref_text), "%s", src->tts.neutts_ref_text);
    if (is_set_str(src->tts.neutts_model)) snprintf(dst->tts.neutts_model, sizeof(dst->tts.neutts_model), "%s", src->tts.neutts_model);
    if (is_set_str(src->tts.neutts_device)) snprintf(dst->tts.neutts_device, sizeof(dst->tts.neutts_device), "%s", src->tts.neutts_device);
    if (is_set_str(src->tts.piper_voice)) snprintf(dst->tts.piper_voice, sizeof(dst->tts.piper_voice), "%s", src->tts.piper_voice);

    /* STT merge */
    dst->stt.enabled = src->stt.enabled;
    if (is_set_str(src->stt.provider)) snprintf(dst->stt.provider, sizeof(dst->stt.provider), "%s", src->stt.provider);
    if (is_set_str(src->stt.local_model)) snprintf(dst->stt.local_model, sizeof(dst->stt.local_model), "%s", src->stt.local_model);
    if (is_set_str(src->stt.local_language)) snprintf(dst->stt.local_language, sizeof(dst->stt.local_language), "%s", src->stt.local_language);
    if (is_set_str(src->stt.openai_model)) snprintf(dst->stt.openai_model, sizeof(dst->stt.openai_model), "%s", src->stt.openai_model);
    if (is_set_str(src->stt.mistral_model)) snprintf(dst->stt.mistral_model, sizeof(dst->stt.mistral_model), "%s", src->stt.mistral_model);

    /* Voice merge */
    if (is_set_str(src->voice.record_key)) snprintf(dst->voice.record_key, sizeof(dst->voice.record_key), "%s", src->voice.record_key);
    if (is_set_int(src->voice.max_recording_seconds)) dst->voice.max_recording_seconds = src->voice.max_recording_seconds;
    dst->voice.auto_tts = src->voice.auto_tts;
    dst->voice.beep_enabled = src->voice.beep_enabled;
    if (is_set_int(src->voice.silence_threshold)) dst->voice.silence_threshold = src->voice.silence_threshold;
    if (src->voice.silence_duration >= 0.1f) dst->voice.silence_duration = src->voice.silence_duration;

    /* Secrets merge */
    dst->secrets.enabled = src->secrets.enabled;
    if (is_set_str(src->secrets.access_token))
        snprintf(dst->secrets.access_token, sizeof(dst->secrets.access_token), "%s", src->secrets.access_token);
    if (is_set_str(src->secrets.organization_id))
        snprintf(dst->secrets.organization_id, sizeof(dst->secrets.organization_id), "%s", src->secrets.organization_id);
    if (is_set_str(src->secrets.bws_path))
        snprintf(dst->secrets.bws_path, sizeof(dst->secrets.bws_path), "%s", src->secrets.bws_path);
    if (is_set_int(src->secrets.install_timeout))
        dst->secrets.install_timeout = src->secrets.install_timeout;

    /* Sync flat fields for backward compat */
    snprintf(dst->model, sizeof(dst->model), "%s", dst->provider_cfg.model);
    snprintf(dst->provider, sizeof(dst->provider), "%s", dst->provider_cfg.provider);
    snprintf(dst->base_url, sizeof(dst->base_url), "%s", dst->provider_cfg.base_url);
    snprintf(dst->api_key, sizeof(dst->api_key), "%s", dst->provider_cfg.api_key);
    dst->max_turns = dst->agent.max_iterations;
    dst->verbose = dst->agent.verbose_level;
    dst->fast_mode = dst->agent.fast_mode;
    dst->quiet_mode = dst->agent.quiet_mode;
}

/* ================================================================
 *  P24: Config Schema Generation — build JSON Schema from config_t
 * ================================================================ */

/* Helper: create schema property definition */
static json_t *schema_prop(const char *type, const char *desc, const char *default_val) {
    json_t *prop = json_object();
    json_set(prop, "type", json_string(type));
    if (desc && desc[0]) json_set(prop, "description", json_string(desc));
    if (default_val && default_val[0]) json_set(prop, "default", json_string(default_val));
    return prop;
}

static json_t *schema_prop_int(const char *desc, int def, int min, int max) {
    json_t *prop = json_object();
    json_set(prop, "type", json_string("integer"));
    if (desc) json_set(prop, "description", json_string(desc));
    json_set(prop, "default", json_number((double)def));
    json_set(prop, "minimum", json_number((double)min));
    json_set(prop, "maximum", json_number((double)max));
    return prop;
}

static json_t *schema_prop_num(const char *desc, double def, double min, double max) {
    json_t *prop = json_object();
    json_set(prop, "type", json_string("number"));
    if (desc) json_set(prop, "description", json_string(desc));
    json_set(prop, "default", json_number(def));
    json_set(prop, "minimum", json_number(min));
    json_set(prop, "maximum", json_number(max));
    return prop;
}

static json_t *schema_prop_bool(const char *desc, bool def) {
    json_t *prop = json_object();
    json_set(prop, "type", json_string("boolean"));
    if (desc) json_set(prop, "description", json_string(desc));
    json_set(prop, "default", json_bool(def));
    return prop;
}

static void schema_add_enum(json_t *prop, const char **values, int count) {
    json_t *arr = json_array();
    for (int i = 0; i < count; i++)
        json_append(arr, json_string(values[i]));
    json_set(prop, "enum", arr);
}

/* Build JSON Schema for the entire config struct */
char *hermes_config_schema(void) {
    json_t *root = json_object();
    json_set(root, "$schema", json_string("https://json-schema.org/draft-07/schema#"));
    json_set(root, "title", json_string("Hermes C Config"));
    json_set(root, "description", json_string("Configuration schema for Hermes C CLI"));

    /* Type definitions */
    json_t *defs = json_object();
    json_t *properties = json_object();

    /* config_version */
    json_set(properties, "config_version",
             schema_prop_int("Config file format version", 1, 1, 999));

    /* --- model --- */
    {
        json_t *model = json_object();
        json_set(model, "type", json_string("object"));
        json_set(model, "description", json_string("Provider, model, and API connection settings"));
        json_t *mprops = json_object();
        json_set(mprops, "default", schema_prop("string", "Model name", ""));
        json_set(mprops, "provider", schema_prop("string", "Provider name", ""));
        json_set(mprops, "base_url", schema_prop("string", "API base URL", ""));
        json_set(mprops, "api_mode", schema_prop("string", "API mode", "chat_completions"));
        json_set(mprops, "fallback_model", schema_prop("string", "Fallback model", ""));
        json_set(mprops, "fallback_providers", schema_prop("string", "Comma-separated fallback providers", ""));
        json_set(mprops, "service_tier", schema_prop("string", "Service tier", "auto"));
        json_set(mprops, "reasoning_effort", schema_prop("string", "Reasoning effort", "medium"));
        json_set(mprops, "max_tokens", schema_prop_int("Max output tokens", 4096, 1, 1048576));
        json_set(mprops, "temperature", schema_prop_num("Sampling temperature", 1.0, 0.0, 2.0));
        json_set(mprops, "top_p", schema_prop_num("Nucleus sampling", 1.0, 0.0, 1.0));
        json_set(model, "properties", mprops);
        json_set(properties, "model", model);
    }

    /* --- display --- */
    {
        json_t *disp = json_object();
        json_set(disp, "type", json_string("object"));
        json_set(disp, "description", json_string("UI theme, skin, streaming, language"));
        json_t *dprops = json_object();
        json_set(dprops, "skin", schema_prop("string", "Display skin/theme", "default"));
        json_set(dprops, "banner", schema_prop("string", "Custom banner text", ""));
        json_set(dprops, "spinner", schema_prop("string", "Spinner style", ""));
        json_set(dprops, "indicator", schema_prop("string", "Busy indicator style", ""));
        json_set(dprops, "language", schema_prop("string", "UI language", "en"));
        json_set(dprops, "personality", schema_prop("string", "System prompt override", ""));
        json_set(dprops, "footer", schema_prop("string", "Custom footer text", ""));
        json_set(dprops, "streaming", schema_prop_bool("Token streaming", false));
        json_set(dprops, "show_reasoning", schema_prop_bool("Show reasoning content", true));
        json_set(dprops, "compact", schema_prop_bool("Compact mode", false));
        json_set(dprops, "show_cost", schema_prop_bool("Show token cost", false));
        json_set(dprops, "timestamps", schema_prop_bool("Show message timestamps", false));
        json_set(dprops, "statusbar", schema_prop_bool("Show status bar", true));
        json_set(disp, "properties", dprops);
        json_set(properties, "display", disp);
    }

    /* --- agent --- */
    {
        json_t *agent = json_object();
        json_set(agent, "type", json_string("object"));
        json_set(agent, "description", json_string("Iterations, verbosity, system prompt"));
        json_t *aprops = json_object();
        json_set(aprops, "max_turns", schema_prop_int("Max tool-calling turns", 90, 1, 10000));
        json_set(aprops, "max_tool_calls_round", schema_prop_int("Max tool calls per turn", 0, 0, 1000));
        json_set(aprops, "max_output_tokens", schema_prop_int("Max output tokens per response", 4096, 1, 1048576));
        json_set(aprops, "verbose", schema_prop_int("Verbosity level", 0, 0, 2));
        json_set(aprops, "api_max_retries", schema_prop_int("API call retries", 3, 0, 100));
        json_set(aprops, "clarify_timeout", schema_prop_int("Clarify timeout seconds", 300, 0, 3600));
        json_set(aprops, "compress_threshold", schema_prop_num("Auto-compress ratio", 0.38, 0.0, 1.0));
        json_set(aprops, "system_prompt", schema_prop("string", "Custom system prompt", ""));
        json_set(aprops, "profile", schema_prop("string", "Active profile name", ""));
        json_set(aprops, "reasoning_effort", schema_prop("string", "Reasoning effort", "medium"));
        json_set(aprops, "fast", schema_prop_bool("Fast mode", false));
        json_set(aprops, "quiet", schema_prop_bool("Quiet mode", false));
        json_set(agent, "properties", aprops);
        json_set(properties, "agent", agent);
    }

    /* --- tools --- */
    {
        json_t *tools = json_object();
        json_set(tools, "type", json_string("object"));
        json_set(tools, "description", json_string("Terminal, approvals, vision settings"));
        json_t *tprops = json_object();
        json_set(tprops, "allow_background", schema_prop_bool("Allow background processes", true));
        json_set(tprops, "local_process_cleanup", schema_prop_bool("Auto-cleanup on exit", true));
        json_set(tprops, "approval_mode", schema_prop("string", "Approval mode", "manual"));
        const char *approval_modes[] = {"off", "manual", "auto"};
        schema_add_enum(json_obj_get(tprops, "approval_mode"), approval_modes, 3);
        json_set(tprops, "approval_timeout", schema_prop_int("Approval timeout seconds", 600, 0, 86400));
        json_set(tprops, "max_result_size", schema_prop_int("Max tool result bytes", 50000, 256, 10485760));
        json_set(tprops, "terminal_timeout", schema_prop_int("Terminal timeout seconds", 1800, 1, 86400));
        json_set(tprops, "vision_timeout", schema_prop_int("Vision timeout seconds", 300, 1, 3600));
        json_set(tprops, "vision_model", schema_prop("string", "Vision model name", ""));
        json_set(tprops, "terminal_backend", schema_prop("string", "Terminal backend", "local"));
        json_set(tprops, "persistent_shell", schema_prop_bool("Persistent shell across commands", true));
        json_set(tprops, "web_backend", schema_prop("string", "Web search backend (shared fallback)", ""));
        json_set(tprops, "web_search_backend", schema_prop("string", "Web search backend override", ""));
        json_set(tprops, "web_extract_backend", schema_prop("string", "Web extract backend override", ""));
        json_set(tprops, "web_search_timeout", schema_prop_int("Web search timeout seconds", 30, 1, 300));
        json_set(tools, "properties", tprops);
        json_set(properties, "tools", tools);
    }

    /* --- delegation --- */
    {
        json_t *del = json_object();
        json_set(del, "type", json_string("object"));
        json_set(del, "description", json_string("Subagent spawning and child config"));
        json_t *dprops = json_object();
        json_set(dprops, "max_concurrent_children", schema_prop_int("Max parallel subagents", 3, 1, 50));
        json_set(dprops, "max_spawn_depth", schema_prop_int("Max nesting depth", 1, 0, 10));
        json_set(dprops, "child_timeout", schema_prop_int("Child timeout seconds", 600, 1, 36000));
        json_set(dprops, "child_max_turns", schema_prop_int("Child max iterations", 50, 1, 1000));
        json_set(dprops, "child_model", schema_prop("string", "Child model override", ""));
        json_set(dprops, "child_provider", schema_prop("string", "Child provider override", ""));
        json_set(del, "properties", dprops);
        json_set(properties, "delegation", del);
    }

    /* --- browser --- */
    {
        json_t *browser = json_object();
        json_set(browser, "type", json_string("object"));
        json_set(browser, "description", json_string("CDP browser engine settings"));
        json_t *bprops = json_object();
        json_set(bprops, "cdp_url", schema_prop("string", "CDP WebSocket URL", ""));
        json_set(bprops, "engine", schema_prop("string", "Browser engine", ""));
        json_set(bprops, "headless", schema_prop_bool("Headless mode", true));
        json_set(bprops, "javascript", schema_prop_bool("Enable JavaScript", true));
        json_set(bprops, "viewport_width", schema_prop_int("Viewport width", 1280, 320, 7680));
        json_set(bprops, "viewport_height", schema_prop_int("Viewport height", 720, 240, 4320));
        json_set(bprops, "command_timeout", schema_prop_int("Browser command timeout", 30, 1, 300));
        json_set(browser, "properties", bprops);
        json_set(properties, "browser", browser);
    }

    /* --- memory --- */
    {
        json_t *mem = json_object();
        json_set(mem, "type", json_string("object"));
        json_set(mem, "description", json_string("Memory provider, char limits, TTL"));
        json_t *mprops = json_object();
        json_set(mprops, "provider", schema_prop("string", "Memory provider backend", ""));
        json_set(mprops, "char_limit", schema_prop_int("Memory char limit", 2200, 100, 100000));
        json_set(mprops, "user_char_limit", schema_prop_int("User profile char limit", 1375, 100, 50000));
        json_set(mprops, "ttl_days", schema_prop_int("Memory TTL days", 30, 1, 3650));
        json_set(mprops, "auto_save", schema_prop_bool("Auto-save memory", true));
        json_set(mem, "properties", mprops);
        json_set(properties, "memory", mem);
    }

    /* --- compression --- */
    {
        json_t *comp = json_object();
        json_set(comp, "type", json_string("object"));
        json_set(comp, "description", json_string("Context compression strategy and thresholds"));
        json_t *cprops = json_object();
        json_set(cprops, "model", schema_prop("string", "Compression model override", ""));
        json_set(cprops, "strategy", schema_prop("string", "Compression strategy", "smart"));
        json_set(cprops, "target_ratio", schema_prop_num("Target compression ratio", 0.20, 0.01, 1.0));
        json_set(cprops, "min_messages", schema_prop_int("Minimum messages before compress", 20, 2, 1000));
        json_set(cprops, "protect_last_n", schema_prop_int("Recent messages to keep", 20, 1, 100));
        json_set(cprops, "protect_first_n", schema_prop_int("Head messages to preserve", 3, 0, 20));
        json_set(cprops, "hygiene_hard_message_limit", schema_prop_int("Force-compress threshold", 400, 50, 10000));
        json_set(cprops, "abort_on_summary_failure", schema_prop_bool("Abort on LLM error", false));
        json_set(cprops, "preserve_system", schema_prop_bool("Preserve system prompt", true));
        json_set(comp, "properties", cprops);
        json_set(properties, "compression", comp);
    }

    /* --- cron --- */
    {
        json_t *cron = json_object();
        json_set(cron, "type", json_string("object"));
        json_set(cron, "description", json_string("Scheduler directory, job limits"));
        json_t *cprops = json_object();
        json_set(cprops, "dir", schema_prop("string", "Cron jobs directory", ""));
        json_set(cprops, "max_concurrent_jobs", schema_prop_int("Max concurrent jobs", 5, 1, 100));
        json_set(cprops, "job_timeout", schema_prop_int("Job timeout seconds", 3600, 1, 86400));
        json_set(cprops, "retention_days", schema_prop_int("Job retention days", 30, 0, 3650));
        json_set(cprops, "notify_on_failure", schema_prop_bool("Notify on job failure", true));
        json_set(cron, "properties", cprops);
        json_set(properties, "cron", cron);
    }

    /* --- notification --- */
    {
        json_t *notif = json_object();
        json_set(notif, "type", json_string("object"));
        json_set(notif, "description", json_string("Completion/error notification settings"));
        json_t *nprops = json_object();
        json_set(nprops, "provider", schema_prop("string", "Notification provider", ""));
        json_set(nprops, "sound", schema_prop("string", "Notification sound", ""));
        json_set(nprops, "on_complete", schema_prop_bool("Notify on complete", true));
        json_set(nprops, "on_error", schema_prop_bool("Notify on error", true));
        json_set(nprops, "on_approval", schema_prop_bool("Notify on approval request", false));
        json_set(notif, "properties", nprops);
        json_set(properties, "notification", notif);
    }

    /* --- security --- */
    {
        json_t *sec = json_object();
        json_set(sec, "type", json_string("object"));
        json_set(sec, "description", json_string("Tirith, URL safety, redaction"));
        json_t *sprops = json_object();
        json_set(sprops, "tirith_path", schema_prop("string", "Tirith policy path", ""));
        json_set(sprops, "redact_patterns", schema_prop("string", "Redaction patterns", ""));
        json_set(sprops, "tirith_timeout", schema_prop_int("Tirith timeout seconds", 5, 0, 300));
        json_set(sprops, "tirith_enabled", schema_prop_bool("Tirith security enabled", true));
        json_set(sprops, "allow_private_urls", schema_prop_bool("Allow private URLs", false));
        json_set(sprops, "website_blocklist_enabled", schema_prop_bool("Website blocklist enabled", false));
        json_set(sec, "properties", sprops);
        json_set(properties, "security", sec);
    }

    /* --- sessions --- */
    {
        json_t *sess = json_object();
        json_set(sess, "type", json_string("object"));
        json_set(sess, "description", json_string("DB path, retention, auto-save"));
        json_t *sprops = json_object();
        json_set(sprops, "db_path", schema_prop("string", "Sessions DB path", ""));
        json_set(sprops, "retention_days", schema_prop_int("Session retention days", 90, 0, 3650));
        json_set(sprops, "auto_save_interval", schema_prop_int("Auto-save interval turns", 10, 1, 1000));
        json_set(sprops, "compress", schema_prop_bool("Compress sessions", false));
        json_set(sprops, "store_trajectories", schema_prop_bool("Store tool trajectories", false));
        json_set(sess, "properties", sprops);
        json_set(properties, "sessions", sess);
    }

    /* --- plugin --- */
    {
        json_t *plug = json_object();
        json_set(plug, "type", json_string("object"));
        json_set(plug, "description", json_string("Plugin directories and enabled plugins"));
        json_t *pprops = json_object();
        json_set(pprops, "dirs", schema_prop("string", "Plugin directories (comma-separated)", ""));
        json_set(pprops, "enabled", schema_prop("string", "Enabled plugins (comma-separated)", ""));
        json_set(plug, "properties", pprops);
        json_set(properties, "plugin", plug);
    }

    /* --- mcp --- */
    {
        json_t *mcp = json_object();
        json_set(mcp, "type", json_string("object"));
        json_set(mcp, "description", json_string("MCP server timeout, auth, tool limit"));
        json_t *mprops = json_object();
        json_set(mprops, "timeout", schema_prop_int("MCP server timeout seconds", 120, 1, 3600));
        json_set(mprops, "max_tools", schema_prop_int("Max MCP tools per server", 50, 1, 500));
        json_set(mprops, "auth_enabled", schema_prop_bool("MCP auth enabled", false));
        json_set(mcp, "properties", mprops);
        json_set(properties, "mcp", mcp);
    }

    /* --- auxiliary --- */
    #define S_AUX_TASK(task, nm, desc, to) do {         json_t *obj = json_object();         json_set(obj, "type", json_string("object"));         json_set(obj, "description", json_string("Auxiliary " desc " LLM routing"));         json_t *oprops = json_object();         json_set(oprops, "provider", schema_prop("string", "Provider for " desc, "auto"));         json_set(oprops, "model", schema_prop("string", "Model for " desc, ""));         json_set(oprops, "base_url", schema_prop("string", "Base URL for " desc, ""));         json_set(oprops, "api_key", schema_prop("string", "API key for " desc, ""));         json_set(oprops, "timeout", schema_prop_int("Timeout for " desc, to, 1, 3600));         json_set(oprops, "extra_body", schema_prop("string", "Extra request body fields", ""));         json_set(obj, "properties", oprops);         json_set(properties, "auxiliary." #task, obj);     } while(0)
    S_AUX_TASK(vision, "vision", "vision analysis", 120);
    S_AUX_TASK(web_extract, "web_extract", "web page extraction", 360);
    S_AUX_TASK(compression, "compression", "context compression", 120);
    S_AUX_TASK(skills_hub, "skills_hub", "skill hub", 30);
    S_AUX_TASK(approval, "approval", "approval review", 30);
    S_AUX_TASK(mcp, "mcp", "MCP routing", 30);
    S_AUX_TASK(title_generation, "title_generation", "title generation", 30);
    S_AUX_TASK(triage_specifier, "triage_specifier", "Kanban triage specification", 120);
    S_AUX_TASK(kanban_decomposer, "kanban_decomposer", "Kanban decomposition", 180);
    S_AUX_TASK(profile_describer, "profile_describer", "profile description", 60);
    S_AUX_TASK(curator, "curator", "skill-usage review", 600);
    #undef S_AUX_TASK

    /* --- tts --- */
    {
        json_t *tts = json_object();
        json_set(tts, "type", json_string("object"));
        json_set(tts, "description", json_string("Text-to-speech configuration"));
        json_t *tprops = json_object();
        json_set(tprops, "provider", schema_prop("string", "TTS provider", "edge"));
        json_set(tprops, "edge.voice", schema_prop("string", "Edge TTS voice name", "en-US-AriaNeural"));
        json_set(tprops, "elevenlabs.voice_id", schema_prop("string", "ElevenLabs voice ID", "pNInz6obpgDQGcFmaJgB"));
        json_set(tprops, "openai.model", schema_prop("string", "OpenAI TTS model", "gpt-4o-mini-tts"));
        json_set(tprops, "openai.voice", schema_prop("string", "OpenAI TTS voice", "alloy"));
        json_set(tprops, "xai.sample_rate", schema_prop_int("xAI sample rate", 24000, 8000, 192000));
        json_set(tprops, "piper.voice", schema_prop("string", "Piper TTS voice", "en_US-lessac-medium"));
        json_set(tts, "properties", tprops);
        json_set(properties, "tts", tts);
    }

    /* --- stt --- */
    {
        json_t *stt = json_object();
        json_set(stt, "type", json_string("object"));
        json_set(stt, "description", json_string("Speech-to-text configuration"));
        json_t *sprops = json_object();
        json_set(sprops, "enabled", schema_prop_bool("STT enabled", true));
        json_set(sprops, "provider", schema_prop("string", "STT provider", "local"));
        json_set(sprops, "local.model", schema_prop("string", "Local whisper model", "base"));
        json_set(sprops, "openai.model", schema_prop("string", "OpenAI whisper model", "whisper-1"));
        json_set(stt, "properties", sprops);
        json_set(properties, "stt", stt);
    }

    /* --- voice --- */
    {
        json_t *voice = json_object();
        json_set(voice, "type", json_string("object"));
        json_set(voice, "description", json_string("Voice input recording settings"));
        json_t *vprops = json_object();
        json_set(vprops, "record_key", schema_prop("string", "Record key binding", "ctrl+b"));
        json_set(vprops, "max_recording_seconds", schema_prop_int("Max recording length", 120, 1, 600));
        json_set(vprops, "auto_tts", schema_prop_bool("Auto-TTS on voice input", false));
        json_set(vprops, "beep_enabled", schema_prop_bool("Record start/stop beeps", true));
        json_set(voice, "properties", vprops);
        json_set(properties, "voice", voice);
    }

    json_set(root, "properties", properties);
    json_set(root, "definitions", defs);

    char *result = json_serialize_pretty(root, 2);
    json_free(root);
    return result;
}

/* ================================================================
 *  P25: Config Migration
 * ================================================================ */

/* Apply migration from version N to N+1. Return 0 on success, -1 on error. */
static int migrate_v0_to_v1(hermes_config_t *cfg, const char *config_path) {
    (void)cfg;
    /* v0→v1: Add config_version field to YAML file.
     * Read file, find or insert config_version: 1, write back. */
    FILE *f = fopen(config_path, "r");
    if (!f) return 0; /* No file to migrate */

    /* Read entire file into memory */
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize <= 0) { fclose(f); return 0; }
    if (fsize > 1024 * 1024) { fclose(f); return -1; } /* Sanity cap */

    char *buf = (char *)malloc((size_t)fsize + 1);
    if (!buf) { fclose(f); return -1; }
    size_t nread = fread(buf, 1, (size_t)fsize, f);
    buf[nread] = '\0';
    fclose(f);

    /* Check if config_version already present */
    if (strstr(buf, HERMES_CONFIG_VERSION_KEY)) {
        free(buf);
        return 0; /* Already has version field */
    }

    /* Insert config_version: 1 after the first line (comment or blank) */
    char *insert_point = buf;
    /* Skip shebang or first comment line */
    while (*insert_point && *insert_point != '\n') insert_point++;
    if (*insert_point == '\n') insert_point++;

    char *new_buf;
    size_t pre_len = (size_t)(insert_point - buf);
    size_t remaining = nread - pre_len;
    /* Insert: config_version: 1\n */
    const char *version_line = "config_version: 1\n";
    size_t ver_len = strlen(version_line);
    new_buf = (char *)malloc(pre_len + ver_len + remaining + 1);
    if (!new_buf) { free(buf); return -1; }
    memcpy(new_buf, buf, pre_len);
    memcpy(new_buf + pre_len, version_line, ver_len);
    memcpy(new_buf + pre_len + ver_len, insert_point, remaining);
    new_buf[pre_len + ver_len + remaining] = '\0';
    free(buf);

    /* Write back */
    f = fopen(config_path, "w");
    if (!f) { free(new_buf); return -1; }
    size_t written = fwrite(new_buf, 1, pre_len + ver_len + remaining, f);
    fclose(f);
    free(new_buf);

    return (written == pre_len + ver_len + remaining) ? 0 : -1;
}

/* O15: File permission hardening — secure sensitive files to 0600/0700 */
/* Skips if owner is 0 (root). Hardens: home dir (0700), config.yaml (0600),
   .env (0600), session DB (0600), vault file (0600), cron store (0600). */
void hermes_file_permissions_harden(const char *hermes_home,
                                    const char *session_db_path,
                                    const char *cron_store_path,
                                    uid_t owner)
{
    if (owner == 0) return;  /* root — skip, permissions are moot */

    /* 1. Home directory — 0700 */
    if (hermes_home && *hermes_home) {
        struct stat st;
        if (stat(hermes_home, &st) == 0 && S_ISDIR(st.st_mode))
            chmod(hermes_home, 0700);
    }

    /* 2. Config file — 0600 */
    if (hermes_home && *hermes_home) {
        char path[HERMES_PATH_MAX];
        snprintf(path, sizeof(path), "%s/config.yaml", hermes_home);
        struct stat st;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
            chmod(path, 0600);
    }

    /* 3. .env file — 0600 */
    if (hermes_home && *hermes_home) {
        char path[HERMES_PATH_MAX];
        snprintf(path, sizeof(path), "%s/.env", hermes_home);
        struct stat st;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
            chmod(path, 0600);
    }

    /* 4. Session DB — 0600 */
    if (session_db_path && *session_db_path) {
        struct stat st;
        if (stat(session_db_path, &st) == 0 && S_ISREG(st.st_mode))
            chmod(session_db_path, 0600);
    }

    /* 5. Vault file — 0600 (standard location under HERMES_HOME) */
    if (hermes_home && *hermes_home) {
        char path[HERMES_PATH_MAX];
        snprintf(path, sizeof(path), "%s/data/vault.dat", hermes_home);
        struct stat st;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
            chmod(path, 0600);
        /* Also check ~/.slermes/ location */
        snprintf(path, sizeof(path), "%s/.slermes/vault.dat", hermes_home);
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
            chmod(path, 0600);
        /* And ~/.hermes/ location */
        snprintf(path, sizeof(path), "%s/.hermes/vault.dat", hermes_home);
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
            chmod(path, 0600);
    }

    /* 6. Cron store — 0600 (contains job configs that may embed API keys) */
    if (cron_store_path && *cron_store_path) {
        struct stat st;
        if (stat(cron_store_path, &st) == 0 && S_ISREG(st.st_mode))
            chmod(cron_store_path, 0600);
    }
}

bool hermes_config_migrate(hermes_config_t *cfg, const char *config_dir) {
    if (!cfg) return false;

    char config_path[HERMES_PATH_MAX];
    if (config_dir && config_dir[0])
        snprintf(config_path, sizeof(config_path), "%s/config.yaml", config_dir);
    else {
        char home[HERMES_PATH_MAX];
        hermes_get_home(home, sizeof(home));
        snprintf(config_path, sizeof(config_path), "%s/config.yaml", home);
    }

    int version = cfg->config_version;

    /* If version not set (fresh config or legacy), check file */
    if (version <= 0) {
        /* Try reading version from file */
        char *err = NULL;
        yaml_doc_t *doc = yaml_parse_file(config_path, &err);
        if (doc) {
            int fv = yaml_get_int(doc, HERMES_CONFIG_VERSION_KEY, 0);
            cfg->config_version = fv;
            version = fv;
            yaml_free(doc);
        }
        if (err) free(err);
    }

    if (version >= HERMES_CONFIG_VERSION)
        return false; /* Already current, no migration needed */

    fprintf(stderr, "Config migration: v%d → v%d\n", version, HERMES_CONFIG_VERSION);

    /* Run migrations sequentially */
    int current = version;
    bool changed = false;

    if (current < 1) {
        if (migrate_v0_to_v1(cfg, config_path) == 0) {
            current = 1;
            cfg->config_version = 1;
            changed = true;
        } else {
            fprintf(stderr, "Error: v0→v1 migration failed\n");
            return false;
        }
    }

    /* Future migrations:
     * if (current < 2) { migrate_v1_to_v2(cfg, config_path); current = 2; changed = true; }
     */

    if (changed) {
        fprintf(stderr, "Config migration complete: v%d → v%d\n", version, HERMES_CONFIG_VERSION);
    }

    return changed;
}

/* ================================================================
 *  P19: Config hot-reload via SIGHUP
 * ================================================================ */

static volatile sig_atomic_t g_config_reload_requested = 0;

static void config_sighup_handler(int sig) {
    (void)sig;
    g_config_reload_requested = 1;
}

void hermes_config_setup_reload(void) {
    signal(SIGHUP, config_sighup_handler);
}

bool hermes_config_check_reload(hermes_config_t *cfg, const char *config_dir) {
    if (!g_config_reload_requested)
        return false;
    g_config_reload_requested = 0;

    fprintf(stderr, "SIGHUP received — reloading config...\n");

    /* Save current config as fallback */
    hermes_config_t old = *cfg;

    /* Reload — this resets to defaults then overlays YAML + env */
    if (!hermes_config_load(cfg, config_dir)) {
        fprintf(stderr, "Config reload FAILED — restoring previous config\n");
        *cfg = old;
        return false;
    }

    fprintf(stderr, "Config reloaded successfully.\n");
    return true;
}
