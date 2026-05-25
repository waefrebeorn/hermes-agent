/* File safety — denied write paths, read-block for credential files.
 *
 * Python equivalent: agent/file_safety.py
 *
 * Provides write-deny checks (SSH keys, .env, shell configs, /etc/)
 * and read-block checks (credential stores, skill cache, mcp-tokens).
 */

#define _GNU_SOURCE
#include "hermes_file_safety.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

/* ================================================================
 *  Test-mode overrides
 * ================================================================ */

static char g_test_home[PATH_MAX] = "";
static char g_test_root[PATH_MAX] = "";

void file_safety_set_test_paths(const char *hermes_home, const char *hermes_root)
{
    if (hermes_home)
        snprintf(g_test_home, sizeof(g_test_home), "%s", hermes_home);
    else
        g_test_home[0] = '\0';

    if (hermes_root)
        snprintf(g_test_root, sizeof(g_test_root), "%s", hermes_root);
    else
        g_test_root[0] = '\0';
}

/* ================================================================
 *  Internal helpers
 * ================================================================ */

/* Resolve path to canonical absolute form. Returns empty on failure. */
static void resolve_path(const char *in, char *out, size_t out_sz)
{
    if (!in || !*in) { out[0] = '\0'; return; }
    char *expanded = NULL;
    /* Expand ~/ */
    if (in[0] == '~' && (in[1] == '/' || in[1] == '\0')) {
        const char *home = getenv("HOME");
        if (!home) home = "/root";
        size_t hlen = strlen(home);
        size_t ilen = strlen(in);
        expanded = malloc(hlen + ilen + 1); /* +1 for / separator */
        if (expanded) {
            memcpy(expanded, home, hlen);
            expanded[hlen] = '/';
            if (in[1] == '/')
                memcpy(expanded + hlen + 1, in + 2, ilen - 1); /* includes \0 */
            else
                expanded[hlen + 1] = '\0';
        }
    }
    const char *to_resolve = expanded ? expanded : in;
    char *real = realpath(to_resolve, NULL);
    if (real) {
        snprintf(out, out_sz, "%s", real);
        free(real);
    } else {
        /* Fallback: use as-is if realpath fails */
        snprintf(out, out_sz, "%s", to_resolve);
    }
    free(expanded);
}

/* Get the user's home directory. */
static const char *user_home(void)
{
    const char *h = getenv("HOME");
    return h ? h : "/root";
}

/* Get hermes_home path: test override, then $HERMES_HOME, then ~/.hermes */
static void get_hermes_home_path(char *out, size_t sz)
{
    if (g_test_home[0]) {
        snprintf(out, sz, "%s", g_test_home);
        return;
    }
    const char *env = getenv("HERMES_HOME");
    if (env && *env) {
        snprintf(out, sz, "%s", env);
        return;
    }
    snprintf(out, sz, "%s/.hermes", user_home());
}

/* Get hermes_root path: test override, then $HERMES_ROOT, then ~/.hermes */
static void get_hermes_root_path(char *out, size_t sz)
{
    if (g_test_root[0]) {
        snprintf(out, sz, "%s", g_test_root);
        return;
    }
    /* Same as hermes_home when HERMES_ROOT is unset */
    const char *env = getenv("HERMES_ROOT");
    if (env && *env) {
        snprintf(out, sz, "%s", env);
        return;
    }
    get_hermes_home_path(out, sz);
}

/* ================================================================
 *  Denied path builders (static lists)
 * ================================================================ */

/* Check if resolved path matches an exact denied path */
static bool denied_exact_match(const char *resolved)
{
    const char *home = user_home();
    /* Build denied paths on the fly to avoid allocation */
    char buf[PATH_MAX];
    const char *denied[] = {
        ".ssh/authorized_keys",
        ".ssh/id_rsa",
        ".ssh/id_ed25519",
        ".ssh/config",
        ".bashrc",
        ".zshrc",
        ".profile",
        ".bash_profile",
        ".zprofile",
        ".netrc",
        ".pgpass",
        ".npmrc",
        ".pypirc",
        NULL
    };

    for (int i = 0; denied[i]; i++) {
        snprintf(buf, sizeof(buf), "%s/%s", home, denied[i]);
        char rbuf[PATH_MAX];
        resolve_path(buf, rbuf, sizeof(rbuf));
        if (rbuf[0] && strcmp(resolved, rbuf) == 0)
            return true;
    }

    /* Absolute paths */
    const char *abs_denied[] = {
        "/etc/sudoers",
        "/etc/passwd",
        "/etc/shadow",
        NULL
    };
    for (int i = 0; abs_denied[i]; i++) {
        char rbuf[PATH_MAX];
        resolve_path(abs_denied[i], rbuf, sizeof(rbuf));
        if (rbuf[0] && strcmp(resolved, rbuf) == 0)
            return true;
    }

    return false;
}

/* Check if resolved path starts with a denied prefix */
static bool denied_prefix_match(const char *resolved)
{
    const char *home = user_home();
    char buf[PATH_MAX];

    const char *prefixes[] = {
        ".ssh/",
        ".aws/",
        ".gnupg/",
        ".kube/",
        ".docker/",
        ".azure/",
        ".config/gh/",
        NULL
    };

    for (int i = 0; prefixes[i]; i++) {
        snprintf(buf, sizeof(buf), "%s/%s", home, prefixes[i]);
        char rbuf[PATH_MAX];
        resolve_path(buf, rbuf, sizeof(rbuf));
        if (rbuf[0] && strncmp(resolved, rbuf, strlen(rbuf)) == 0)
            return true;
    }

    /* Absolute prefixes */
    const char *abs_prefixes[] = {
        "/etc/sudoers.d/",
        "/etc/systemd/",
        NULL
    };
    for (int i = 0; abs_prefixes[i]; i++) {
        char rbuf[PATH_MAX];
        resolve_path(abs_prefixes[i], rbuf, sizeof(rbuf));
        if (rbuf[0] && strncmp(resolved, rbuf, strlen(rbuf)) == 0)
            return true;
    }

    return false;
}

/* Check if path is a Hermes control file (auth.json, config.yaml, etc.) */
static bool denied_hermes_control(const char *resolved)
{
    char hermes_home[PATH_MAX], hermes_root[PATH_MAX];
    get_hermes_home_path(hermes_home, sizeof(hermes_home));
    get_hermes_root_path(hermes_root, sizeof(hermes_root));

    char r_home[PATH_MAX], r_root[PATH_MAX];
    resolve_path(hermes_home, r_home, sizeof(r_home));
    resolve_path(hermes_root, r_root, sizeof(r_root));

    /* .env at root and home */
    const char *check_bases[] = { r_home, r_root };
    for (size_t i = 0; i < 2; i++) {
        if (!check_bases[i][0]) continue;

        /* .env */
        char buf[PATH_MAX];
        snprintf(buf, sizeof(buf), "%s/.env", check_bases[i]);
        char rbuf[PATH_MAX];
        resolve_path(buf, rbuf, sizeof(rbuf));
        if (rbuf[0] && strcmp(resolved, rbuf) == 0)
            return true;

        /* auth.json, config.yaml, webhook_subscriptions.json */
        const char *control_files[] = {
            "auth.json", "config.yaml", "webhook_subscriptions.json", NULL
        };
        for (int f = 0; control_files[f]; f++) {
            snprintf(buf, sizeof(buf), "%s/%s", check_bases[i], control_files[f]);
            resolve_path(buf, rbuf, sizeof(rbuf));
            if (rbuf[0] && strcmp(resolved, rbuf) == 0)
                return true;
        }

        /* mcp-tokens/ */
        snprintf(buf, sizeof(buf), "%s/mcp-tokens", check_bases[i]);
        resolve_path(buf, rbuf, sizeof(rbuf));
        if (rbuf[0] && strncmp(resolved, rbuf, strlen(rbuf)) == 0)
            return true;
    }

    return false;
}

/* Check HERMES_WRITE_SAFE_ROOT */
static bool outside_safe_root(const char *resolved)
{
    const char *env = getenv("HERMES_WRITE_SAFE_ROOT");
    if (!env || !*env)
        return false; /* not set → no restriction */

    char root[PATH_MAX];
    resolve_path(env, root, sizeof(root));
    if (!root[0])
        return true; /* failed to resolve → deny */

    /* Allow exact match or prefix */
    if (strcmp(resolved, root) == 0)
        return false;
    size_t rlen = strlen(root);
    if (rlen > 0 && root[rlen - 1] != '/') {
        /* Add trailing slash for prefix matching */
        char root_slash[PATH_MAX];
        snprintf(root_slash, sizeof(root_slash), "%s/", root);
        rlen = strlen(root_slash);
        if (strncmp(resolved, root_slash, rlen) == 0)
            return false;
        return true;
    }
    if (strncmp(resolved, root, rlen) == 0)
        return false;
    return true;
}

/* ================================================================
 *  Public API
 * ================================================================ */

bool file_is_write_denied(const char *path)
{
    if (!path || !*path)
        return true; /* empty path → deny */

    char resolved[PATH_MAX];
    resolve_path(path, resolved, sizeof(resolved));
    if (!resolved[0])
        return true; /* cannot resolve → deny */

    if (denied_exact_match(resolved))
        return true;
    if (denied_prefix_match(resolved))
        return true;
    if (denied_hermes_control(resolved))
        return true;
    if (outside_safe_root(resolved))
        return true;

    return false;
}

char *file_get_read_block_error(const char *path)
{
    if (!path || !*path)
        return NULL;

    char resolved[PATH_MAX];
    resolve_path(path, resolved, sizeof(resolved));
    if (!resolved[0])
        return NULL;

    char hermes_home[PATH_MAX], hermes_root[PATH_MAX];
    get_hermes_home_path(hermes_home, sizeof(hermes_home));
    get_hermes_root_path(hermes_root, sizeof(hermes_root));

    char r_home[PATH_MAX], r_root[PATH_MAX];
    resolve_path(hermes_home, r_home, sizeof(r_home));
    resolve_path(hermes_root, r_root, sizeof(r_root));

    /* Check skills/.hub/ cache (prompt-injection carrier) */
    const char *check_bases[] = { r_home, r_root };
    for (size_t i = 0; i < 2; i++) {
        if (!check_bases[i][0]) continue;

        char buf[PATH_MAX];

        /* skills/.hub/ and skills/.hub/index-cache */
        snprintf(buf, sizeof(buf), "%s/skills/.hub", check_bases[i]);
        char rbuf[PATH_MAX];
        resolve_path(buf, rbuf, sizeof(rbuf));
        if (rbuf[0] && strncmp(resolved, rbuf, strlen(rbuf)) == 0) {
            char *err;
            asprintf(&err,
                "Access denied: %s is an internal Hermes cache file "
                "and cannot be read directly to prevent prompt injection. "
                "Use the skills_list or skill_view tools instead.", path);
            return err;
        }

        /* Credential files */
        const char *cred_files[] = {
            "auth.json", "auth.lock", ".anthropic_oauth.json",
            ".env", "webhook_subscriptions.json", NULL
        };
        for (int f = 0; cred_files[f]; f++) {
            snprintf(buf, sizeof(buf), "%s/%s", check_bases[i], cred_files[f]);
            resolve_path(buf, rbuf, sizeof(rbuf));
            if (rbuf[0] && strcmp(resolved, rbuf) == 0) {
                char *err;
                asprintf(&err,
                    "Access denied: %s is a Hermes credential store "
                    "and cannot be read directly. Provider tools consume "
                    "these credentials through internal channels. "
                    "(Defense-in-depth \342\200\224 not a security boundary; "
                    "the terminal tool can still bypass.)", path);
                return err;
            }
        }

        /* mcp-tokens/ */
        snprintf(buf, sizeof(buf), "%s/mcp-tokens", check_bases[i]);
        resolve_path(buf, rbuf, sizeof(rbuf));
        if (rbuf[0]) {
            size_t rlen = strlen(rbuf);
            if (strcmp(resolved, rbuf) == 0) {
                char *err;
                asprintf(&err,
                    "Access denied: %s is the Hermes MCP token directory "
                    "and cannot be read directly. (Defense-in-depth \342\200\224 "
                    "not a security boundary; the terminal tool can still "
                    "bypass.)", path);
                return err;
            }
            if (strncmp(resolved, rbuf, rlen) == 0 && resolved[rlen] == '/') {
                char *err;
                asprintf(&err,
                    "Access denied: %s is the Hermes MCP token directory "
                    "and cannot be read directly. (Defense-in-depth \342\200\224 "
                    "not a security boundary; the terminal tool can still "
                    "bypass.)", path);
                return err;
            }
            if (strncmp(resolved, rbuf, strlen(rbuf)) == 0 && resolved[strlen(rbuf)] == '/') {
                char *err;
                asprintf(&err,
                    "Access denied: %s is a Hermes MCP token file "
                    "and cannot be read directly. (Defense-in-depth \342\200\224 "
                    "not a security boundary; the terminal tool can still "
                    "bypass.)", path);
                return err;
            }
        }
    }

    return NULL;
}
