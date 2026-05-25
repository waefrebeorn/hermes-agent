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

/* Resolve HERMES_HOME. Default: ~/.hermes */
static void get_hermes_home(char *buf, size_t sz) {
    const char *env = getenv("HERMES_HOME");
    if (env) {
        snprintf(buf, sz, "%s", env);
        return;
    }
    const char *home = getenv("HOME");
    if (home)
        snprintf(buf, sz, "%s/.hermes", home);
    else
        snprintf(buf, sz, "/tmp/.hermes");
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
        if (strncmp(p, "HERMES_API_KEY", key_len) == 0 && key_len == 14)
            snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", val);
        else if (strncmp(p, "OPENAI_API_KEY", key_len) == 0 && key_len == 14) {
            if (cfg->api_key[0] == '\0')
                snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", val);
        }
        /* Add more as needed */
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

    char hermes_home[HERMES_PATH_MAX];
    if (config_dir && config_dir[0])
        snprintf(hermes_home, sizeof(hermes_home), "%s", config_dir);
    else
        get_hermes_home(hermes_home, sizeof(hermes_home));

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
        if (model_env) snprintf(cfg->model, sizeof(cfg->model), "%s", model_env);
        const char *prov_env = getenv("HERMES_PROVIDER");
        if (prov_env) snprintf(cfg->provider, sizeof(cfg->provider), "%s", prov_env);
        const char *url_env = getenv("HERMES_BASE_URL");
        if (url_env) snprintf(cfg->base_url, sizeof(cfg->base_url), "%s", url_env);
        const char *key_env = getenv("HERMES_API_KEY");
        if (key_env) snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", key_env);
        return true;
    }

    /* Extract model section */
    const char *model_name = yaml_get_string(doc, "model.default");
    if (model_name) snprintf(cfg->model, sizeof(cfg->model), "%s", model_name);

    const char *provider = yaml_get_string(doc, "model.provider");
    if (provider) snprintf(cfg->provider, sizeof(cfg->provider), "%s", provider);

    const char *base_url = yaml_get_string(doc, "model.base_url");
    if (base_url) snprintf(cfg->base_url, sizeof(cfg->base_url), "%s", base_url);

    const char *api_key = yaml_get_string(doc, "model.api_key");
    if (api_key) snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", api_key);

    /* Agent section */
    int max_turns = yaml_get_int(doc, "agent.max_turns", 90);
    cfg->max_turns = max_turns > 0 ? max_turns : 90;

    yaml_free(doc);

    /* Parse .env (overrides config.yaml) */
    parse_env_file(cfg->env_path, cfg);

    return true;
}

bool hermes_config_load_env(hermes_config_t *cfg) {
    /* Environment overrides for all config fields */
    const char *v;

    v = getenv("HERMES_MODEL");
    if (v) snprintf(cfg->model, sizeof(cfg->model), "%s", v);

    v = getenv("HERMES_PROVIDER");
    if (v) snprintf(cfg->provider, sizeof(cfg->provider), "%s", v);

    v = getenv("HERMES_BASE_URL");
    if (v) snprintf(cfg->base_url, sizeof(cfg->base_url), "%s", v);

    v = getenv("HERMES_API_KEY");
    if (v) snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", v);

    v = getenv("HERMES_MAX_TURNS");
    if (v) { int t = atoi(v); if (t > 0) cfg->max_turns = t; }

    return true;
}

/* U01: Config init — create default config.yaml + .env template */
bool hermes_config_init(const char *config_dir) {
    char dir[4096];
    if (config_dir && config_dir[0]) {
        snprintf(dir, sizeof(dir), "%s", config_dir);
    } else {
        const char *home = getenv("HERMES_HOME");
        if (!home) home = getenv("HOME");
        if (!home) { fprintf(stderr, "Error: cannot determine home.\n"); return false; }
        if (getenv("HERMES_HOME"))
            snprintf(dir, sizeof(dir), "%s", home);
        else
            snprintf(dir, sizeof(dir), "%s/.hermes", home);
    }

    struct stat st;
    if (stat(dir, &st) != 0) mkdir(dir, 0700);

    char path[4096];
    snprintf(path, sizeof(path), "%s/config.yaml", dir);
    if (stat(path, &st) == 0) {
        printf("Config already exists at %s\n", path);
    } else {
        FILE *f = fopen(path, "w");
        if (!f) { fprintf(stderr, "Error: cannot write %s\n", path); return false; }
        fprintf(f, "# Hermes Config\nmodel: claude-sonnet-4\nprovider: anthropic\nmax_turns: 90\n");
        fclose(f);
        printf("Created: %s\n", path);
    }

    snprintf(path, sizeof(path), "%s/.env", dir);
    if (stat(path, &st) == 0) {
        printf("Env file already exists at %s\n", path);
    } else {
        FILE *f = fopen(path, "w");
        if (f) {
            fprintf(f, "# Hermes API Keys\n");
            fprintf(f, "#OPENAI_API_KEY=sk-...\n");
            fprintf(f, "#ANTHROPIC_API_KEY=sk-ant-...\n");
            fprintf(f, "#GOOGLE_API_KEY=...\n");
            fclose(f);
            printf("Created: %s (edit to add API keys)\n", path);
        }
    }

    printf("\nHermes config initialized at %s\n", dir);
    printf("Next: edit %s/.env, then run ./hermes\n", dir);
    return true;
}
