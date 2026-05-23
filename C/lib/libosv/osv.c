/*
 * osv.c — OSV malware check for MCP extension packages.
 * Port of Python tools/osv_check.py.
 *
 * Before launching an MCP server via npx/uvx, queries the OSV
 * (Open Source Vulnerabilities) API to check if the package has any
 * known malware advisories (MAL-* IDs). Regular CVEs are ignored.
 *
 * Fail-open: network errors allow the package to proceed.
 */

#include "osv.h"
#include "http.h"
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ─── Constants ─────────────────────────────────────────── */

#define OSV_ENDPOINT "https://api.osv.dev/v1/query"
#define OSV_TIMEOUT  10  /* seconds */

/* ─── Ecosystem Detection ───────────────────────────────── */

const char *osv_infer_ecosystem(const char *command)
{
    if (!command) return NULL;

    /* Extract basename */
    const char *base = strrchr(command, '/');
    base = base ? base + 1 : command;
    const char *bs = strrchr(base, '\\');
    if (bs) base = bs + 1;

    /* Normalize to lowercase for comparison */
    size_t len = strlen(base);
    char lower[64];
    if (len >= sizeof(lower)) return NULL;

    for (size_t i = 0; i < len; i++)
        lower[i] = (base[i] >= 'A' && base[i] <= 'Z') ? base[i] + 32 : base[i];
    lower[len] = '\0';

    if (strcmp(lower, "npx") == 0 || strcmp(lower, "npx.cmd") == 0)
        return "npm";
    if (strcmp(lower, "uvx") == 0 || strcmp(lower, "uvx.cmd") == 0
        || strcmp(lower, "pipx") == 0)
        return "PyPI";

    return NULL;
}

/* ─── Package Parsing ───────────────────────────────────── */

static int skip_flags(const char **args, int arg_count)
{
    for (int i = 0; i < arg_count; i++) {
        if (!args[i]) continue;
        if (args[i][0] != '-')
            return i;
    }
    return -1;
}

int osv_parse_package(const char **args, int arg_count,
                      const char *ecosystem,
                      char **out_name, char **out_version)
{
    *out_name = NULL;
    *out_version = NULL;

    if (!args || arg_count < 1 || !ecosystem) return -1;

    int pkg_idx = skip_flags(args, arg_count);
    if (pkg_idx < 0) return -1;

    const char *token = args[pkg_idx];
    if (!token || !*token) return -1;

    if (strcmp(ecosystem, "npm") == 0) {
        if (token[0] == '@') {
            /* Scoped: @scope/name@version */
            const char *at = strchr(token + 1, '@');
            if (at) {
                size_t name_len = (size_t)(at - token);
                *out_name = strndup(token, name_len);
                *out_version = strdup(at + 1);
            } else {
                *out_name = strdup(token);
            }
        } else {
            /* Unscoped: name@version */
            const char *at = strrchr(token, '@');
            if (at && at != token) {
                size_t name_len = (size_t)(at - token);
                *out_name = strndup(token, name_len);
                const char *ver = at + 1;
                if (*ver && strcmp(ver, "latest") != 0)
                    *out_version = strdup(ver);
            } else {
                *out_name = strdup(token);
            }
        }
    } else if (strcmp(ecosystem, "PyPI") == 0) {
        /* PyPI: name[extras]==version */
        const char *eq = strstr(token, "==");
        if (eq) {
            size_t name_len = (size_t)(eq - token);
            const char *bracket = memchr(token, '[', name_len);
            if (bracket)
                name_len = (size_t)(bracket - token);
            *out_name = strndup(token, name_len);
            *out_version = strdup(eq + 2);
        } else {
            const char *bracket = strchr(token, '[');
            if (bracket)
                *out_name = strndup(token, (size_t)(bracket - token));
            else
                *out_name = strdup(token);
        }
    } else {
        return -1;
    }

    return *out_name ? 0 : -1;
}

/* ─── OSV API Query ─────────────────────────────────────── */

static int osv_query(const char *package, const char *ecosystem,
                     const char *version, json_t **out_response)
{
    *out_response = NULL;

    /* Build JSON request body */
    json_t *body = json_object();
    if (!body) return -1;

    json_t *pkg = json_object();
    if (!pkg) { json_free(body); return -1; }

    json_set(pkg, "name", json_string(package));
    json_set(pkg, "ecosystem", json_string(ecosystem));
    json_set(body, "package", pkg);

    if (version && *version)
        json_set(body, "version", json_string(version));

    char *body_str = json_serialize(body);
    json_free(body);
    if (!body_str) return -1;

    /* HTTP POST via http client */
    http_t *h = http_new(OSV_TIMEOUT);
    if (!h) { free(body_str); return -1; }

    http_resp_t *resp = http_post_json(h, OSV_ENDPOINT, body_str);
    free(body_str);

    if (!resp) { http_free(h); return -1; }

    /* Check HTTP status */
    if (resp->status < 200 || resp->status >= 300) {
        http_resp_free(resp);
        http_free(h);
        return -1;
    }

    /* Parse response JSON */
    json_t *parsed = json_parse(resp->body, NULL);
    http_resp_free(resp);
    http_free(h);

    if (!parsed) return -1;
    *out_response = parsed;
    return 0;
}

/* ─── Main API ───────────────────────────────────────────── */

char *osv_check_package_for_malware(const char *command,
                                     const char **args,
                                     int arg_count)
{
    /* Infer ecosystem */
    const char *ecosystem = osv_infer_ecosystem(command);
    if (!ecosystem) return NULL;

    /* Parse package */
    char *package = NULL;
    char *version = NULL;
    if (osv_parse_package(args, arg_count, ecosystem,
                          &package, &version) != 0 || !package) {
        free(version);
        return NULL;
    }

    /* Query OSV API */
    json_t *response = NULL;
    int ret = osv_query(package, ecosystem, version, &response);

    if (ret != 0 || !response) {
        json_free(response);
        free(package);
        free(version);
        return NULL;
    }

    /* Extract vulns array */
    json_t *vulns = json_obj_get(response, "vulns");
    if (!vulns || json_len(vulns) == 0) {
        json_free(response);
        free(package);
        free(version);
        return NULL;
    }

    /* Filter for MAL-* advisories only */
    size_t count = json_len(vulns);
    json_t *malware_ids[32];
    int mal_count = 0;

    for (size_t i = 0; i < count && mal_count < 32; i++) {
        json_t *vuln = json_get(vulns, i);
        if (!vuln) continue;
        const char *id = json_get_str(vuln, "id", NULL);
        if (id && strncmp(id, "MAL-", 4) == 0) {
            malware_ids[mal_count++] = vuln;
        }
    }

    if (mal_count == 0) {
        json_free(response);
        free(package);
        free(version);
        return NULL;
    }

    /* Build error message — save package name before freeing */
    const char *pkg_name = package;
    const char *eco_name = ecosystem;

    size_t msg_len = 256;
    for (int i = 0; i < mal_count && i < 3; i++) {
        const char *id = json_get_str(malware_ids[i], "id", "");
        msg_len += strlen(id) + 4;
    }

    char *msg = (char *)calloc(1, msg_len);
    if (!msg) {
        json_free(response);
        free(package);
        free(version);
        return NULL;
    }

    int written = snprintf(msg, msg_len,
        "BLOCKED: Package '%s' (%s) has known malware advisories: ",
        pkg_name, eco_name);

    for (int i = 0; i < mal_count && i < 3; i++) {
        const char *id = json_get_str(malware_ids[i], "id", "");
        written += snprintf(msg + written, msg_len - (size_t)written,
            "%s%s", i > 0 ? ", " : "", id);
    }

    if (mal_count > 0) {
        const char *summary = json_get_str(malware_ids[0], "summary", NULL);
        if (summary) {
            size_t summary_len = strlen(summary);
            if (summary_len > 100) summary_len = 100;
            snprintf(msg + written, msg_len - (size_t)written,
                ". %.*s", (int)summary_len, summary);
        }
    }

    json_free(response);
    free(package);
    free(version);
    return msg;
}
