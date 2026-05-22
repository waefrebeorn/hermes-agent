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

/* Resolve SLERMES_HOME. Default: ~/.slermes */
static void get_slermes_home(char *buf, size_t sz) {
    const char *env = getenv("SLERMES_HOME");
    if (env) {
        snprintf(buf, sz, "%s", env);
        return;
    }
    const char *home = getenv("HOME");
    if (home)
        snprintf(buf, sz, "%s/.slermes", home);
    else
        snprintf(buf, sz, "/tmp/.slermes");
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

    /* P10: Notification section */
    const char *np = yaml_get_string(doc, "notification.provider");
    if (np) snprintf(cfg->notification.provider, sizeof(cfg->notification.provider), "%s", np);
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

    /* P14: MCP section */
    int mto = yaml_get_int(doc, "mcp_servers.timeout", 0);
    if (mto > 0) cfg->mcp.timeout = mto;
    cfg->mcp.auth_enabled = yaml_get_bool(doc, "mcp.auth_enabled", false);

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
        char slermes_home[HERMES_PATH_MAX];
        const char *env = getenv("SLERMES_HOME");
        if (env) snprintf(slermes_home, sizeof(slermes_home), "%s", env);
        else snprintf(slermes_home, sizeof(slermes_home), "%s/.slermes", getenv("HOME") ? getenv("HOME") : "/tmp");
        snprintf(profile_path, sizeof(profile_path), "%s/profiles/%s.yaml", slermes_home, profile_name);
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
