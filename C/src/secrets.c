/*
 * src/secrets.c — Secrets Manager (L01: Bitwarden Secrets Manager)
 *
 * Lazy-installs bws CLI, authenticates with BSM access token, and
 * resolves ${BSM:secret-name} references in config values.
 */
#include "hermes_secrets.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* Cache of authenticated project ID */
static char g_project_id[256] = {0};
static bool g_authenticated = false;

/* ----------------------------------------------------------------
 *  Helpers
 * ---------------------------------------------------------------- */

/* Run a command via popen, return stdout as malloc'd string, or NULL */
static char *popen_read(const char *cmd) {
    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;
    char buf[4096];
    size_t total = 0;
    char *result = NULL;
    while (fgets(buf, sizeof(buf), fp)) {
        size_t len = strlen(buf);
        char *newr = (char *)realloc(result, total + len + 1);
        if (!newr) { free(result); pclose(fp); return NULL; }
        result = newr;
        memcpy(result + total, buf, len + 1);
        total += len;
    }
    int rc = pclose(fp);
    if (rc != 0) { free(result); return NULL; }
    /* Trim trailing newline */
    if (result && total > 0 && result[total-1] == '\n')
        result[total-1] = '\0';
    return result;
}

/* Locate bws on PATH or at configured path */
static char *find_bws(const hermes_config_t *cfg) {
    if (cfg->secrets.bws_path[0]) {
        struct stat st;
        if (stat(cfg->secrets.bws_path, &st) == 0 && (st.st_mode & S_IXUSR))
            return strdup(cfg->secrets.bws_path);
    }
    /* Search PATH */
    const char *path = getenv("PATH");
    if (!path) path = "/usr/local/bin:/usr/bin:/bin";
    char *dup = strdup(path);
    if (!dup) return NULL;
    char *result = NULL;
    char *save = NULL;
    for (char *tok = strtok_r(dup, ":", &save); tok; tok = strtok_r(NULL, ":", &save)) {
        char full[1024];
        snprintf(full, sizeof(full), "%s/bws", tok);
        struct stat st;
        if (stat(full, &st) == 0 && (st.st_mode & S_IXUSR)) {
            result = strdup(full);
            break;
        }
    }
    free(dup);
    return result;
}

/* ----------------------------------------------------------------
 *  Lazy install bws
 * ---------------------------------------------------------------- */

static bool lazy_install_bws(void) {
    fprintf(stderr, "[secrets] bws CLI not found. Attempting lazy install...\n");

    /* Determine target directory */
    const char *home = getenv("HOME");
    if (!home) { fprintf(stderr, "[secrets] $HOME not set, cannot install bws\n"); return false; }

    char bindir[512];
    snprintf(bindir, sizeof(bindir), "%s/.local/bin", home);
    mkdir(bindir, 0755);

    /* Detect OS/arch for download */
    char *os = popen_read("uname -s | tr '[:upper:]' '[:lower:]'");
    char *arch = popen_read("uname -m | sed 's/aarch64/arm64/;s/x86_64/amd64/'");
    if (!os || !arch) {
        free(os); free(arch);
        fprintf(stderr, "[secrets] cannot detect OS/arch\n");
        return false;
    }

    char url[512];
    snprintf(url, sizeof(url),
        "https://github.com/bitwarden/sm-cli/releases/latest/download/bws-%s-%s",
        os, arch);
    char outpath[512];
    snprintf(outpath, sizeof(outpath), "%s/bws", bindir);

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "curl -fsSL '%s' -o '%s' && chmod +x '%s'", url, outpath, outpath);

    int rc = system(cmd);
    free(os); free(arch);

    if (rc != 0) {
        fprintf(stderr, "[secrets] failed to install bws (curl exited %d)\n", rc);
        return false;
    }
    fprintf(stderr, "[secrets] bws installed to %s\n", outpath);
    return true;
}

/* ----------------------------------------------------------------
 *  BSM authentication
 * ---------------------------------------------------------------- */

static bool authenticate_bws(const char *bws_path, const char *access_token) {
    if (!access_token || !access_token[0]) {
        fprintf(stderr, "[secrets] BSM access_token not configured\n");
        return false;
    }

    /* Set env for bws */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "BWS_ACCESS_TOKEN='%s' %s project list --limit 1 2>/dev/null | head -c 100",
        access_token, bws_path);

    char *output = popen_read(cmd);
    if (!output) {
        fprintf(stderr, "[secrets] bws authentication failed (invalid token or network error)\n");
        return false;
    }

    /* Try to extract project ID from first project */
    json_t *root = json_parse(output, NULL);
    if (root && json_len(root) > 0) {
        json_t *first = json_get(root, 0);
        if (first) {
            const char *pid = json_get_str(first, "id", NULL);
            if (pid) snprintf(g_project_id, sizeof(g_project_id), "%s", pid);
        }
    }
    json_free(root);
    free(output);

    if (!g_project_id[0]) {
        fprintf(stderr, "[secrets] bws authenticated but no project found\n");
        return false;
    }

    g_authenticated = true;
    fprintf(stderr, "[secrets] bws authenticated (project: %s)\n", g_project_id);
    return true;
}

/* ---------------------------------------------------------------- */

bool hermes_secrets_init(const hermes_config_t *cfg) {
    if (!cfg->secrets.enabled) return true; /* not enabled = not an error */

    /* Find or install bws */
    char *bws = find_bws(cfg);
    if (!bws) {
        if (!lazy_install_bws()) return false;
        bws = find_bws(cfg);
        if (!bws) { fprintf(stderr, "[secrets] bws install succeeded but binary not found\n"); return false; }
    }

    bool ok = authenticate_bws(bws, cfg->secrets.access_token);
    free(bws);
    return ok;
}

/* ----------------------------------------------------------------
 *  Secret resolution
 * ---------------------------------------------------------------- */

char *hermes_secrets_resolve(const char *ref) {
    if (!ref || !ref[0] || !g_authenticated) return NULL;

    /* Expect format: ${BSM:secret-name} or secret-name */
    const char *name = ref;
    if (name[0] == '$' && name[1] == '{' && strncmp(name + 2, "BSM:", 4) == 0) {
        name += 6; /* skip "${BSM:" */
        size_t len = strlen(name);
        if (len > 0 && name[len-1] == '}')
            len--;
        char secret_name[256] = "";
        snprintf(secret_name, sizeof(secret_name), "%.*s", (int)len, name);
        name = secret_name;
    }

    if (!name || !name[0]) return NULL;

    /* Find bws on PATH */
    char bws_path[1024] = "bws";
    struct stat st;
    /* Try PATH first */
    const char *path_env = getenv("PATH");
    if (path_env) {
        char *path_dup = strdup(path_env);
        char *save = NULL;
        bool found = false;
        for (char *tok = strtok_r(path_dup, ":", &save); tok; tok = strtok_r(NULL, ":", &save)) {
            char full[1024];
            snprintf(full, sizeof(full), "%s/bws", tok);
            if (stat(full, &st) == 0 && (st.st_mode & S_IXUSR)) {
                snprintf(bws_path, sizeof(bws_path), "%s", full);
                found = true;
                break;
            }
        }
        free(path_dup);
        if (!found) return NULL;
    } else {
        if (stat("bws", &st) != 0) return NULL;
    }

    /* Get BSM access token from environment (already set by authenticate_bws) */
    const char *token = getenv("BWS_ACCESS_TOKEN");
    if (!token) return NULL;

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
        "BWS_ACCESS_TOKEN='%s' %s secret get '%s' 2>/dev/null",
        token, bws_path, name);

    char *output = popen_read(cmd);
    if (!output) return NULL;

    /* Parse JSON response */
    json_t *root = json_parse(output, NULL);
    free(output);
    if (!root) return NULL;

    const char *value = json_get_str(root, "value", NULL);
    char *result = value ? strdup(value) : NULL;
    json_free(root);
    return result;
}

char *hermes_secrets_resolve_string(const char *input, bool strict) {
    if (!input || !input[0]) return strdup("");

    size_t out_len = strlen(input) * 2 + 1; /* generous */
    char *output = (char *)malloc(out_len);
    if (!output) return NULL;
    output[0] = '\0';

    const char *p = input;
    char *ow = output;
    size_t remaining = out_len;

    while (*p) {
        /* Look for ${BSM: */
        const char *start = strstr(p, "${BSM:");
        if (!start) {
            /* No more refs, copy rest */
            size_t left = strlen(p);
            if (left + 1 > remaining) break;
            memcpy(ow, p, left + 1);
            ow += left;
            break;
        }

        /* Copy text before this ref */
        size_t pre_len = (size_t)(start - p);
        if (pre_len > 0) {
            if (pre_len + 1 > remaining) break;
            memcpy(ow, p, pre_len);
            ow += pre_len;
            remaining -= pre_len;
        }

        /* Find closing brace */
        const char *end = strchr(start, '}');
        if (!end) {
            /* Malformed reference — copy as-is */
            size_t left = strlen(start);
            if (left + 1 > remaining) break;
            memcpy(ow, start, left + 1);
            ow += left;
            break;
        }

        /* Extract reference */
        size_t ref_len = (size_t)(end - start + 1);
        char ref[HERMES_SECRET_REF_MAX];
        if (ref_len >= sizeof(ref)) ref_len = sizeof(ref) - 1;
        memcpy(ref, start, ref_len);
        ref[ref_len] = '\0';

        /* Resolve */
        char *resolved = hermes_secrets_resolve(ref);
        if (resolved) {
            size_t rlen = strlen(resolved);
            if (rlen + 1 > remaining) { free(resolved); break; }
            memcpy(ow, resolved, rlen);
            ow += rlen;
            remaining -= rlen;
            free(resolved);
        } else if (strict) {
            free(output);
            return NULL;
        } else {
            /* Copy reference as-is */
            if (ref_len + 1 > remaining) break;
            memcpy(ow, ref, ref_len);
            ow += ref_len;
            remaining -= ref_len;
        }

        p = end + 1;
    }

    /* Ensure null-terminated */
    *ow = '\0';
    return output;
}

void hermes_secrets_cleanup(void) {
    g_project_id[0] = '\0';
    g_authenticated = false;
}
