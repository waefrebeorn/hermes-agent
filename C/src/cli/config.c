/*
 * config.c — Config loading for Hermes C.
 * Reads ~/.hermes/config.yaml + ~/.hermes/.env
 * Merges env vars over YAML.
 */

#include "hermes.h"
#include "hermes_yaml.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <unistd.h>
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
    snprintf(cfg->provider_cfg.api_mode, sizeof(cfg->provider_cfg.api_mode), "chat_completions");
    snprintf(cfg->provider_cfg.service_tier, sizeof(cfg->provider_cfg.service_tier), "default");
    snprintf(cfg->provider_cfg.reasoning_effort, sizeof(cfg->provider_cfg.reasoning_effort), "medium");

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

    /* Display config defaults */
    cfg->display.stream = false;
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

    /* Parse config.yaml */
    char *err = NULL;
    yaml_doc_t *doc = yaml_parse_file(cfg->config_path, &err);
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
    if (model_name) snprintf(cfg->provider_cfg.model, sizeof(cfg->provider_cfg.model), "%s", model_name);

    const char *provider_str = yaml_get_string(doc, "model.provider");
    if (provider_str) snprintf(cfg->provider_cfg.provider, sizeof(cfg->provider_cfg.provider), "%s", provider_str);

    const char *base_url = yaml_get_string(doc, "model.base_url");
    if (base_url) snprintf(cfg->provider_cfg.base_url, sizeof(cfg->provider_cfg.base_url), "%s", base_url);

    const char *api_key = yaml_get_string(doc, "model.api_key");
    if (api_key) snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", api_key);

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

    const char *fallback_model = yaml_get_string(doc, "model.fallback");
    if (!fallback_model) fallback_model = yaml_get_string(doc, "model.fallback_model");
    if (fallback_model) snprintf(cfg->provider_cfg.fallback_model,
                                  sizeof(cfg->provider_cfg.fallback_model), "%s", fallback_model);

    const char *service_tier = yaml_get_string(doc, "agent.service_tier");
    if (service_tier) snprintf(cfg->provider_cfg.service_tier,
                                sizeof(cfg->provider_cfg.service_tier), "%s", service_tier);

    const char *reasoning_effort = yaml_get_string(doc, "agent.reasoning_effort");
    if (reasoning_effort) snprintf(cfg->provider_cfg.reasoning_effort,
                                    sizeof(cfg->provider_cfg.reasoning_effort), "%s", reasoning_effort);

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

    /* Auxiliary vision section */
    const char *vis_model = yaml_get_string(doc, "auxiliary.vision.model");
    if (vis_model) snprintf(cfg->tools.vision_model, sizeof(cfg->tools.vision_model), "%s", vis_model);

    int vis_timeout = yaml_get_int(doc, "auxiliary.vision.timeout", 0);
    if (vis_timeout > 0) cfg->tools.vision_timeout = vis_timeout;

    /* Fast mode */
    cfg->fast_mode = yaml_get_bool(doc, "agent.fast", false);
    cfg->agent.fast_mode = cfg->fast_mode;

    /* Compression section */
    cfg->compress_enabled = yaml_get_bool(doc, "compression.enabled", false);

    /* Quiet mode */
    cfg->agent.quiet_mode = cfg->quiet_mode;

    yaml_free(doc);

    /* Parse .env (overrides config.yaml) */
    parse_env_file(cfg->env_path, cfg);

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

    v = getenv("HERMES_TOP_P");
    if (v) { float f = (float)atof(v); if (f >= 0.0f && f <= 1.0f) cfg->provider_cfg.top_p = f; }

    v = getenv("HERMES_FALLBACK_MODEL");
    if (v) snprintf(cfg->provider_cfg.fallback_model, sizeof(cfg->provider_cfg.fallback_model), "%s", v);

    v = getenv("HERMES_SERVICE_TIER");
    if (v) snprintf(cfg->provider_cfg.service_tier, sizeof(cfg->provider_cfg.service_tier), "%s", v);

    v = getenv("HERMES_REASONING_EFFORT");
    if (v) snprintf(cfg->provider_cfg.reasoning_effort, sizeof(cfg->provider_cfg.reasoning_effort), "%s", v);

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
    if (v) snprintf(cfg->tools.vision_model, sizeof(cfg->tools.vision_model), "%s", v);

    v = getenv("HERMES_VISION_TIMEOUT");
    if (v) { int t = atoi(v); if (t > 0) cfg->tools.vision_timeout = t; }

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

    return true;
}

/* ================================================================
 *  P15: Config Validation
 * ================================================================ */

static void add_issue(config_validation_t *r, const char *key, const char *fmt, ...) {
    if (r->count >= 64) return;
    snprintf(r->issues[r->count].key, sizeof(r->issues[r->count].key), "%s", key);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(r->issues[r->count].message, sizeof(r->issues[r->count].message), fmt, ap);
    va_end(ap);
    r->count++;
}

bool hermes_config_validate(const hermes_config_t *cfg, config_validation_t *result) {
    memset(result, 0, sizeof(*result));

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

    return result->count == 0;
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
    diff_str(diff, "model.service_tier", def.provider_cfg.service_tier, active->provider_cfg.service_tier);
    diff_int(diff, "model.max_tokens", def.provider_cfg.max_tokens, active->provider_cfg.max_tokens);
    diff_float(diff, "model.temperature", def.provider_cfg.temperature, active->provider_cfg.temperature);
    diff_float(diff, "model.top_p", def.provider_cfg.top_p, active->provider_cfg.top_p);

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
    diff_str(diff, "agent.system_prompt", def.agent.system_prompt, active->agent.system_prompt);
    diff_str(diff, "agent.profile", def.agent.profile, active->agent.profile);

    /* Tools group */
    diff_str(diff, "approvals.mode", def.tools.approval_mode, active->tools.approval_mode);
    diff_int(diff, "approvals.timeout", def.tools.approval_timeout, active->tools.approval_timeout);
    diff_int(diff, "tool_output.max_bytes", def.tools.max_result_size, active->tools.max_result_size);
    diff_int(diff, "terminal.timeout", def.tools.terminal_timeout, active->tools.terminal_timeout);
    diff_str(diff, "auxiliary.vision.model", def.tools.vision_model, active->tools.vision_model);

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
    exp_str(f, "  service_tier", cfg->provider_cfg.service_tier);
    exp_int(f, "  max_tokens", cfg->provider_cfg.max_tokens);
    exp_float(f, "  temperature", cfg->provider_cfg.temperature);
    exp_float(f, "  top_p", cfg->provider_cfg.top_p);

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
    if (is_set_str(src->tools.vision_model))
        snprintf(dst->tools.vision_model, sizeof(dst->tools.vision_model), "%s", src->tools.vision_model);

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
        json_set(mprops, "service_tier", schema_prop("string", "Service tier", "default"));
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
        json_set(cprops, "target_ratio", schema_prop_num("Target compression ratio", 0.2, 0.0, 1.0));
        json_set(cprops, "min_messages", schema_prop_int("Min messages before compress", 10, 2, 1000));
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
