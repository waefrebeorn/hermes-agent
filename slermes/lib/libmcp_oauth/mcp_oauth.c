/*
 * mcp_oauth.c — MCP OAuth 2.1 Client Support for Hermes C.
 * Port of Python tools/mcp_oauth.py.
 *
 * Implements token storage, PKCE, callback server, metadata discovery,
 * token exchange/refresh, and browser helpers.
 *
 * MIT License — WuBu Hermes Project
 */

#include "mcp_oauth.h"
#include "json.h"
#include "http.h"
#include "crypto.h"
#include "base64.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>

/* ================================================================
 *  Helpers
 * ================================================================ */

/* Sanitize a server name for use as a filename */
static void safe_filename(const char *name, char *out, size_t out_size) {
    size_t j = 0;
    for (size_t i = 0; name[i] && j < out_size - 1; i++) {
        char c = name[i];
        if (isalnum((unsigned char)c) || c == '-' || c == '_')
            out[j++] = c;
        else
            out[j++] = '_';
    }
    out[j] = '\0';
    /* Trim trailing underscores */
    while (j > 0 && out[j - 1] == '_') out[--j] = '\0';
    if (j == 0) {
        snprintf(out, out_size, "default");
    }
}

/* Get HERMES_HOME directory */
static const char *get_hermes_home_dir(void) {
    const char *home = getenv("HERMES_HOME");
    if (home && *home) return home;
    home = getenv("HOME");
    if (!home) home = "/tmp";
    /* Use static buffer */
    static char buf[512];
    snprintf(buf, sizeof(buf), "%s/.hermes", home);
    return buf;
}

/* Get token directory: HERMES_HOME/mcp-tokens/ */
static const char *get_token_dir(void) {
    static char buf[512];
    snprintf(buf, sizeof(buf), "%s/mcp-tokens", get_hermes_home_dir());
    return buf;
}

/* Build full path for a server's token file */
static void __attribute__((unused)) token_path(const char *server_name, const char *suffix,
                       char *out, size_t out_size) {
    char safe[128];
    safe_filename(server_name, safe, sizeof(safe));
    snprintf(out, out_size, "%s/%s%s", get_token_dir(), safe, suffix);
}

/* Read JSON file into malloc'd string */
static char *read_json_file(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode))
        return NULL;
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    size_t size = (size_t)st.st_size;
    char *data = (char *)malloc(size + 1);
    if (!data) { fclose(f); return NULL; }
    size_t n = fread(data, 1, size, f);
    fclose(f);
    data[n] = '\0';
    /* Validate it's valid JSON */
    json_t *j = json_parse(data, NULL);
    if (!j) { free(data); return NULL; }
    json_free(j);
    return data;
}

/* Write JSON file with restricted permissions (0o600), atomic write */
static bool write_json_file_secure(const char *path, const char *data) {
    /* Ensure parent directory exists */
    char *dir = strdup(path);
    if (!dir) return false;
    char *parent = dirname(dir);
    if (parent) {
        mkdir(parent, 0700);
    }
    free(dir);

    /* Create parent dir with secure perms */
    parent = strdup(path);
    if (!parent) return false;
    char *p = dirname(parent);
    if (p) {
        mkdir(p, 0700);
        chmod(p, 0700);
    }
    free(parent);

    /* Atomic write: tmp file then rename */
    char tmp_path[1024];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp.%d", path, getpid());

    int fd = open(tmp_path, O_WRONLY | O_CREAT | O_EXCL,
                  S_IRUSR | S_IWUSR);
    if (fd < 0) return false;

    size_t len = strlen(data);
    ssize_t written = write(fd, data, len);
    close(fd);

    if (written < 0 || (size_t)written != len) {
        unlink(tmp_path);
        return false;
    }

    if (rename(tmp_path, path) != 0) {
        unlink(tmp_path);
        return false;
    }

    /* Set strict perms (umask may have interfered) */
    chmod(path, S_IRUSR | S_IWUSR);
    return true;
}

/* Create a JSON result string */
static char *make_result_json(bool success, const char *val, const char *err) {
    json_t *j = json_object();
    json_set(j, "success", json_bool(success));
    if (val) json_set(j, "access_token", json_string(val));
    if (err) json_set(j, "error", json_string(err));
    char *s = json_serialize(j);
    json_free(j);
    return s;
}

/* ================================================================
 *  Token storage
 * ================================================================ */

struct mcp_oauth_storage {
    char server_name[128];
    char tokens_path[1024];
    char client_path[1024];
    char meta_path[1024];
};

mcp_oauth_storage_t *mcp_oauth_storage_new(const char *server_name) {
    mcp_oauth_storage_t *s = (mcp_oauth_storage_t *)calloc(1, sizeof(*s));
    if (!s) return NULL;
    safe_filename(server_name, s->server_name, sizeof(s->server_name));
    snprintf(s->tokens_path, sizeof(s->tokens_path), "%s/%s.json",
             get_token_dir(), s->server_name);
    snprintf(s->client_path, sizeof(s->client_path), "%s/%s.client.json",
             get_token_dir(), s->server_name);
    snprintf(s->meta_path, sizeof(s->meta_path), "%s/%s.meta.json",
             get_token_dir(), s->server_name);
    return s;
}

void mcp_oauth_storage_free(mcp_oauth_storage_t *s) {
    free(s);
}

bool mcp_oauth_storage_has_tokens(mcp_oauth_storage_t *s) {
    struct stat st;
    return s && stat(s->tokens_path, &st) == 0 && S_ISREG(st.st_mode);
}

char *mcp_oauth_storage_get_tokens(mcp_oauth_storage_t *s) {
    if (!s) return NULL;
    return read_json_file(s->tokens_path);
}

bool mcp_oauth_storage_set_tokens(mcp_oauth_storage_t *s, const char *json) {
    if (!s || !json) return false;
    return write_json_file_secure(s->tokens_path, json);
}

char *mcp_oauth_storage_get_client_info(mcp_oauth_storage_t *s) {
    if (!s) return NULL;
    return read_json_file(s->client_path);
}

bool mcp_oauth_storage_set_client_info(mcp_oauth_storage_t *s, const char *json) {
    if (!s || !json) return false;
    return write_json_file_secure(s->client_path, json);
}

char *mcp_oauth_storage_get_metadata(mcp_oauth_storage_t *s) {
    if (!s) return NULL;
    return read_json_file(s->meta_path);
}

bool mcp_oauth_storage_set_metadata(mcp_oauth_storage_t *s, const char *json) {
    if (!s || !json) return false;
    return write_json_file_secure(s->meta_path, json);
}

void mcp_oauth_storage_remove(mcp_oauth_storage_t *s) {
    if (!s) return;
    unlink(s->tokens_path);
    unlink(s->client_path);
    unlink(s->meta_path);
}

/* ================================================================
 *  PKCE helpers
 * ================================================================ */

static const char PKCE_CHARS[] =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._~";

char *mcp_oauth_generate_code_verifier(void) {
    /* 64 random chars from the unreserved set (RFC 7636) */
    size_t len = 64;
    char *verifier = (char *)malloc(len + 1);
    if (!verifier) return NULL;

    /* Seed once (approximate — good enough for non-adversarial use) */
    static bool seeded = false;
    if (!seeded) {
        srand((unsigned int)(time(NULL) ^ (getpid() << 16)));
        seeded = true;
    }

    size_t charset_len = strlen(PKCE_CHARS);
    for (size_t i = 0; i < len; i++) {
        verifier[i] = PKCE_CHARS[rand() % charset_len];
    }
    verifier[len] = '\0';
    return verifier;
}

char *mcp_oauth_code_challenge(const char *code_verifier) {
    if (!code_verifier) return NULL;

    /* SHA256 of code_verifier */
    unsigned char hash[CRYPTO_SHA256_LEN];
    crypto_sha256((const unsigned char *)code_verifier,
                  strlen(code_verifier), hash);

    /* base64url encode the hash (no padding) */
    return base64url_encode(hash, CRYPTO_SHA256_LEN);
}

/* ================================================================
 *  Port finding
 * ================================================================ */

int mcp_oauth_find_free_port(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return 0;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return 0;
    }

    socklen_t addr_len = sizeof(addr);
    if (getsockname(fd, (struct sockaddr *)&addr, &addr_len) < 0) {
        close(fd);
        return 0;
    }

    int port = ntohs(addr.sin_port);
    close(fd);
    return port;
}

/* ================================================================
 *  Callback server
 * ================================================================ */

/*
 * Parse the query string from the HTTP request path.
 * Extracts "code", "state", and "error" params.
 */
static void parse_callback_query(const char *path,
                                  char **out_code, char **out_state,
                                  char **out_error) {
    *out_code = NULL;
    *out_state = NULL;
    *out_error = NULL;

    const char *q = strchr(path, '?');
    if (!q) return;
    q++; /* skip '?' */

    /* Parse key=value pairs */
    char buf[2048];
    snprintf(buf, sizeof(buf), "%s", q);
    char *save;
    char *pair = strtok_r(buf, "&", &save);
    while (pair) {
        char *eq = strchr(pair, '=');
        if (eq) {
            *eq = '\0';
            char *key = pair;
            char *val = eq + 1;
            /* URL-decode: replace %XX and + with space */
            /* Simple: just check for exact match */
            if (strcmp(key, "code") == 0)
                *out_code = strdup(val);
            else if (strcmp(key, "state") == 0)
                *out_state = strdup(val);
            else if (strcmp(key, "error") == 0)
                *out_error = strdup(val);
        }
        pair = strtok_r(NULL, "&", &save);
    }
}

bool mcp_oauth_wait_for_callback(int port, int timeout_sec,
                                  char **out_code, char **out_state,
                                  char **out_error) {
    *out_code = NULL;
    *out_state = NULL;
    *out_error = NULL;

    /* Create listening socket */
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) return false;

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(server_fd);
        return false;
    }

    if (listen(server_fd, 1) < 0) {
        close(server_fd);
        return false;
    }

    /* Set timeout */
    struct timeval tv;
    tv.tv_sec = timeout_sec > 0 ? timeout_sec : 300;
    tv.tv_usec = 0;
    setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    /* Accept one connection */
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr,
                           &client_len);
    close(server_fd);  /* No more connections needed */

    if (client_fd < 0) {
        /* Timeout or error */
        return false;
    }

    /* Read HTTP request */
    char req_buf[4096];
    ssize_t n = read(client_fd, req_buf, sizeof(req_buf) - 1);
    if (n <= 0) {
        close(client_fd);
        return false;
    }
    req_buf[n] = '\0';

    /* Parse first line: GET /callback?code=... HTTP/1.1 */
    char method[64], path[2048];
    if (sscanf(req_buf, "%63s %2047s", method, path) < 2) {
        close(client_fd);
        return false;
    }

    /* Build response */
    bool got_code = false;
    parse_callback_query(path, out_code, out_state, out_error);

    const char *body;
    if (*out_code) {
        body = "<html><body><h2>Authorization Successful</h2>"
               "<p>You can close this tab and return to Hermes.</p></body></html>";
        got_code = true;
    } else {
        body = "<html><body><h2>Authorization Failed</h2>"
               "<p>Error occurred during OAuth. Check the error in the application.</p></body></html>";
    }

    char resp[4096];
    snprintf(resp, sizeof(resp),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html; charset=utf-8\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             strlen(body), body);

    write(client_fd, resp, strlen(resp));
    close(client_fd);

    return got_code;
}

/* ================================================================
 *  OAuth metadata discovery
 * ================================================================ */

char *mcp_oauth_discover_metadata(const char *server_url) {
    if (!server_url || !*server_url) return NULL;

    /* Extract base URL: strip path to get the server origin */
    char base[1024];
    snprintf(base, sizeof(base), "%s", server_url);

    /* Try well-known URL */
    char well_known[2048];
    /* For MCP servers, the well-known endpoint is at the server origin */
    /* Strip path from URL to get origin */
    char origin[1024];
    snprintf(origin, sizeof(origin), "%s", server_url);

    /* Remove trailing path component */
    char *last_slash = strrchr(origin, '/');
    if (last_slash && last_slash > origin + 8) {
        /* Check for double-slash (https://host/path → origin is https://host) */
        char *second_slash = origin + 8;  /* past https:// */
        char *third = strchr(second_slash, '/');
        if (third) {
            /* Try /.well-known/oauth-authorization-server relative to origin */
            *third = '\0';
        }
    }

    snprintf(well_known, sizeof(well_known),
             "%s/.well-known/oauth-authorization-server", origin);

    /* Fallback: try the full URL path */
    char urls[2][2048];
    snprintf(urls[0], sizeof(urls[0]), "%s", well_known);
    snprintf(urls[1], sizeof(urls[1]), "%s/.well-known/oauth-authorization-server",
             server_url);

    for (int attempt = 0; attempt < 2; attempt++) {
        http_t *h = http_new(10);
        if (!h) continue;

        http_resp_t *r = http_get(h, urls[attempt], NULL);
        if (r && r->status == 200) {
            /* Validate it's JSON */
            json_t *j = json_parse(r->body, NULL);
            if (j) {
                char *result = strdup(r->body);
                json_free(j);
                http_resp_free(r);
                http_free(h);
                return result;
            }
            json_free(j);
        }
        if (r) http_resp_free(r);
        http_free(h);
    }

    return NULL;
}

/* ================================================================
 *  Token exchange / refresh (HTTP POST helpers)
 * ================================================================ */

/*
 * Build a URL-encoded form body from key-value pairs.
 * keys_values: alternating key, value, NULL-terminated.
 */
static char *build_form_body(const char *keys_values[], size_t *out_len) {
    /* Calculate length */
    size_t total = 0;
    for (int i = 0; keys_values[i]; i += 2) {
        if (i > 0) total++;  /* & */
        total += strlen(keys_values[i]) + 1 + strlen(keys_values[i + 1]);
    }

    char *body = (char *)malloc(total + 1);
    if (!body) { *out_len = 0; return NULL; }

    size_t pos = 0;
    for (int i = 0; keys_values[i]; i += 2) {
        if (i > 0) body[pos++] = '&';
        pos += snprintf(body + pos, total - pos + 1, "%s=%s",
                        keys_values[i], keys_values[i + 1]);
    }
    body[pos] = '\0';
    *out_len = pos;
    return body;
}

static char *post_token_request(const char *token_url,
                                 const char *body, size_t body_len) {
    if (!token_url || !body) return NULL;

    http_t *h = http_new(30);
    if (!h) return NULL;

    char headers[256];
    snprintf(headers, sizeof(headers),
             "Content-Type: application/x-www-form-urlencoded\r\n"
             "Accept: application/json");

    http_resp_t *r = http_request(h, HTTP_POST, token_url, headers, body, body_len);
    if (!r) { http_free(h); return NULL; }

    char *result = NULL;
    if (r->status == 200) {
        json_t *j = json_parse(r->body, NULL);
        if (j) {
            result = strdup(r->body);
            json_free(j);
        }
    }

    http_resp_free(r);
    http_free(h);
    return result;
}

char *mcp_oauth_exchange_code(const char *token_url,
                               const char *client_id,
                               const char *code,
                               const char *code_verifier,
                               const char *redirect_uri) {
    if (!token_url || !code) return NULL;

    /* Build form body */
    const char *kv[11];
    int idx = 0;
    kv[idx++] = "grant_type";    kv[idx++] = "authorization_code";
    kv[idx++] = "code";          kv[idx++] = code;
    if (code_verifier) {
        kv[idx++] = "code_verifier"; kv[idx++] = code_verifier;
    }
    if (client_id) {
        kv[idx++] = "client_id"; kv[idx++] = client_id;
    }
    if (redirect_uri) {
        kv[idx++] = "redirect_uri"; kv[idx++] = redirect_uri;
    }
    kv[idx] = NULL;

    size_t body_len;
    char *body = build_form_body(kv, &body_len);
    if (!body) return NULL;

    char *result = post_token_request(token_url, body, body_len);
    free(body);
    return result;
}

char *mcp_oauth_refresh_token(const char *token_url,
                               const char *client_id,
                               const char *refresh_token) {
    if (!token_url || !refresh_token) return NULL;

    const char *kv[7];
    int idx = 0;
    kv[idx++] = "grant_type";    kv[idx++] = "refresh_token";
    kv[idx++] = "refresh_token"; kv[idx++] = refresh_token;
    if (client_id) {
        kv[idx++] = "client_id"; kv[idx++] = client_id;
    }
    kv[idx] = NULL;

    size_t body_len;
    char *body = build_form_body(kv, &body_len);
    if (!body) return NULL;

    char *result = post_token_request(token_url, body, body_len);
    free(body);
    return result;
}

/* ================================================================
 *  Browser helpers
 * ================================================================ */

bool mcp_oauth_can_open_browser(void) {
    /* SSH session → no display */
    if (getenv("SSH_CLIENT") || getenv("SSH_TTY"))
        return false;
    /* Check for display */
    if (getenv("DISPLAY") || getenv("WAYLAND_DISPLAY"))
        return true;
    return false;
}

bool mcp_oauth_open_browser(const char *url) {
    if (!url) return false;

    /* Try xdg-open (Linux), open (macOS) */
    const char *browsers[] = {
        "xdg-open",
        "open",
        "sensible-browser",
        "gnome-open",
        "kde-open",
    };

    for (size_t i = 0; i < sizeof(browsers) / sizeof(browsers[0]); i++) {
        pid_t pid = fork();
        if (pid == 0) {
            /* Child */
            /* Suppress stdout/stderr */
            int fd = open("/dev/null", O_WRONLY);
            if (fd >= 0) {
                dup2(fd, 1);
                dup2(fd, 2);
                close(fd);
            }
            execlp(browsers[i], browsers[i], url, NULL);
            _exit(1);
        } else if (pid > 0) {
            /* Parent */
            int status;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
                return true;
        }
    }

    return false;
}

/* ================================================================
 *  High-level API
 * ================================================================ */

char *mcp_oauth_build_auth(const char *server_name,
                            const char *server_url,
                            const char *oauth_config_json) {
    if (!server_name || !server_url) {
        return make_result_json(false, NULL, "server_name and server_url are required");
    }

    /* Parse oauth_config */
    json_t *cfg = NULL;
    int redirect_port = 0;
    const char *client_id = NULL;
    const char *client_secret = NULL;
    const char *client_name = NULL;
    const char *scope = NULL;
    int timeout = 300;

    if (oauth_config_json && *oauth_config_json) {
        cfg = json_parse(oauth_config_json, NULL);
        if (cfg) {
            redirect_port = (int)json_get_num(cfg, "redirect_port", 0);
            client_id = json_get_str(cfg, "client_id", NULL);
            client_secret = json_get_str(cfg, "client_secret", NULL);
            client_name = json_get_str(cfg, "client_name", NULL);
            scope = json_get_str(cfg, "scope", NULL);
            timeout = (int)json_get_num(cfg, "timeout", 300);
        }
    }

    /* Check for cached tokens first */
    mcp_oauth_storage_t *storage = mcp_oauth_storage_new(server_name);
    if (!storage) {
        if (cfg) json_free(cfg);
        return make_result_json(false, NULL, "Failed to create token storage");
    }

    char *cached_tokens = mcp_oauth_storage_get_tokens(storage);
    if (cached_tokens) {
        /* Check if they have an access_token that's not expired */
        json_t *tj = json_parse(cached_tokens, NULL);
        free(cached_tokens);
        if (tj) {
            const char *access_token = json_get_str(tj, "access_token", NULL);
            const char *token_type = json_get_str(tj, "token_type", NULL);
            double expires_in = json_get_num(tj, "expires_in", 0);
            double expires_at = json_get_num(tj, "expires_at", 0);

            if (access_token && *access_token) {
                bool expired = false;
                if (expires_at > 0) {
                    expired = (time(NULL) >= (time_t)expires_at);
                } else if (expires_in > 0) {
                    /* No absolute expiry — use file mtime as proxy */
                    /* For now, just use it */
                }

                if (!expired) {
                    /* Return cached token */
                    json_t *rj = json_object();
                    json_set(rj, "success", json_bool(true));
                    json_set(rj, "access_token", json_string(access_token));
                    if (token_type)
                        json_set(rj, "token_type", json_string(token_type));
                    json_set(rj, "expires_in", json_number(expires_in));
                    json_set(rj, "cached", json_bool(true));
                    char *result = json_serialize(rj);
                    json_free(rj);
                    json_free(tj);
                    mcp_oauth_storage_free(storage);
                    if (cfg) json_free(cfg);
                    return result;
                }
            }
            /* Try refresh if we have a refresh_token */
            const char *refresh = json_get_str(tj, "refresh_token", NULL);
            if (refresh && *refresh) {
                /* Get stored metadata for token_url */
                char *meta = mcp_oauth_storage_get_metadata(storage);
                if (meta) {
                    json_t *mj = json_parse(meta, NULL);
                    free(meta);
                    if (mj) {
                        const char *token_url = json_get_str(mj, "token_endpoint", NULL);
                        if (token_url) {
                            char *new_tokens = mcp_oauth_refresh_token(token_url, client_id, refresh);
                            if (new_tokens) {
                                /* Validate and store */
                                json_t *ntj = json_parse(new_tokens, NULL);
                                if (ntj) {
                                    const char *new_access = json_get_str(ntj, "access_token", NULL);
                                    if (new_access && *new_access) {
                                        /* Add expires_at */
                                        double new_expires = json_get_num(ntj, "expires_in", 0);
                                        if (new_expires > 0) {
                                            json_t *ntj_mut = (json_t *)ntj;  /* discard const */
                                            json_set(ntj_mut, "expires_at",
                                                     json_number(time(NULL) + new_expires));
                                        }
                                        char *updated = json_serialize(ntj);
                                        mcp_oauth_storage_set_tokens(storage, updated);
                                        free(updated);

                                        const char *new_type = json_get_str(ntj, "token_type", NULL);
                                        json_t *rj = json_object();
                                        json_set(rj, "success", json_bool(true));
                                        json_set(rj, "access_token", json_string(new_access));
                                        if (new_type)
                                            json_set(rj, "token_type", json_string(new_type));
                                        json_set(rj, "expires_in", json_number(new_expires));
                                        json_set(rj, "refreshed", json_bool(true));
                                        char *result = json_serialize(rj);
                                        json_free(rj);
                                        json_free(ntj);
                                        free(new_tokens);
                                        json_free(mj);
                                        mcp_oauth_storage_free(storage);
                                        if (cfg) json_free(cfg);
                                        json_free(tj);
                                        return result;
                                    }
                                    json_free(ntj);
                                }
                                free(new_tokens);
                            }
                        }
                        json_free(mj);
                    }
                }
            }
            json_free(tj);
        }
    }

    /* No cached tokens — run OAuth flow */

    /* 1. Find free port for callback */
    int port = redirect_port > 0 ? redirect_port : mcp_oauth_find_free_port();
    if (port == 0) {
        mcp_oauth_storage_free(storage);
        if (cfg) json_free(cfg);
        return make_result_json(false, NULL, "Failed to find free callback port");
    }

    /* 2. Generate PKCE challenge */
    char *code_verifier = mcp_oauth_generate_code_verifier();
    char *code_challenge = mcp_oauth_code_challenge(code_verifier);
    if (!code_verifier || !code_challenge) {
        free(code_verifier);
        free(code_challenge);
        mcp_oauth_storage_free(storage);
        if (cfg) json_free(cfg);
        return make_result_json(false, NULL, "Failed to generate PKCE challenge");
    }

    /* 3. Build redirect URI and authorization URL */
    char redirect_uri[128];
    snprintf(redirect_uri, sizeof(redirect_uri),
             "http://127.0.0.1:%d/callback", port);

    char auth_url[2048];
    const char *auth_endpoint = NULL;

    /* Try to discover metadata for the server */
    char *metadata = mcp_oauth_discover_metadata(server_url);
    json_t *mj = NULL;
    const char *token_url = NULL;

    if (metadata) {
        mj = json_parse(metadata, NULL);
        if (mj) {
            auth_endpoint = json_get_str(mj, "authorization_endpoint", NULL);
            token_url = json_get_str(mj, "token_endpoint", NULL);
            /* Save metadata */
            mcp_oauth_storage_set_metadata(storage, metadata);
        }
        free(metadata);
    }

    if (!auth_endpoint) {
        /* Fallback: construct well-known paths */
        free(code_verifier);
        free(code_challenge);
        if (mj) json_free(mj);
        mcp_oauth_storage_free(storage);
        if (cfg) json_free(cfg);
        return make_result_json(false, NULL,
                                "Could not discover OAuth authorization endpoint");
    }

    /* Build authorization URL */
    snprintf(auth_url, sizeof(auth_url),
             "%s?response_type=code&client_id=%s&redirect_uri=%s"
             "&code_challenge=%s&code_challenge_method=S256"
             "%s%s",
             auth_endpoint,
             client_id ? client_id : "hermes-agent",
             redirect_uri,
             code_challenge,
             scope ? "&scope=" : "",
             scope ? scope : "");

    /* 4. Print URL and try to open browser */
    fprintf(stderr, "\n  MCP OAuth: authorization required for '%s'.\n"
                    "  Open this URL in your browser:\n\n"
                    "    %s\n\n",
            server_name, auth_url);

    if (mcp_oauth_can_open_browser()) {
        if (mcp_oauth_open_browser(auth_url)) {
            fprintf(stderr, "  (Browser opened automatically.)\n\n");
        } else {
            fprintf(stderr, "  (Could not open browser — open the URL manually.)\n\n");
        }
    } else {
        fprintf(stderr, "  (Headless environment — open the URL manually.)\n\n");
        if (getenv("SSH_CLIENT") || getenv("SSH_TTY")) {
            fprintf(stderr,
                    "  Remote session detected. Forward the port if needed:\n"
                    "    ssh -N -L %d:127.0.0.1:%d <user>@<host>\n\n",
                    port, port);
        }
    }

    /* 5. Wait for callback */
    char *auth_code = NULL, *state = NULL, *error = NULL;
    bool got_code = mcp_oauth_wait_for_callback(port, timeout,
                                                 &auth_code, &state, &error);

    if (!got_code) {
        const char *err = error ? error : "Callback timed out or failed";
        json_t *rj = json_object();
        json_set(rj, "success", json_bool(false));
        json_set(rj, "error", json_string(err));
        char *result = json_serialize(rj);
        json_free(rj);
        free(auth_code);
        free(state);
        free(error);
        free(code_verifier);
        free(code_challenge);
        if (mj) json_free(mj);
        mcp_oauth_storage_free(storage);
        if (cfg) json_free(cfg);
        return result;
    }

    /* 6. Exchange code for tokens */
    if (!token_url && mj) {
        token_url = json_get_str(mj, "token_endpoint", NULL);
    }
    if (!token_url) {
        token_url = server_url;  /* fallback */
    }

    char *tokens_json = mcp_oauth_exchange_code(
        token_url,
        client_id ? client_id : "hermes-agent",
        auth_code,
        code_verifier,
        redirect_uri);

    free(auth_code);
    free(state);
    free(error);
    free(code_verifier);
    free(code_challenge);

    if (!tokens_json) {
        if (mj) json_free(mj);
        mcp_oauth_storage_free(storage);
        if (cfg) json_free(cfg);
        return make_result_json(false, NULL, "Token exchange failed");
    }

    /* 7. Store tokens + add expires_at */
    json_t *ntj = json_parse(tokens_json, NULL);
    char *result = NULL;
    if (ntj) {
        const char *access = json_get_str(ntj, "access_token", NULL);
        const char *token_type = json_get_str(ntj, "token_type", NULL);
        double expires_in = json_get_num(ntj, "expires_in", 0);

        if (access && *access) {
            /* Add expires_at for future expiry checks */
            if (expires_in > 0) {
                json_set((json_t *)ntj, "expires_at",
                         json_number(time(NULL) + expires_in));
            }
            /* Save client info if we have a pre-registered client_id */
            if (client_id) {
                json_t *ci = json_object();
                json_set(ci, "client_id", json_string(client_id));
                if (client_secret)
                    json_set(ci, "client_secret", json_string(client_secret));
                if (client_name)
                    json_set(ci, "client_name", json_string(client_name));
                if (scope)
                    json_set(ci, "scope", json_string(scope));
                // Also set grant_types, response_types, token_endpoint_auth_method
                json_set(ci, "grant_types", json_string("[\"authorization_code\",\"refresh_token\"]"));
                json_set(ci, "response_types", json_string("[\"code\"]"));
                char *ci_json = json_serialize(ci);
                mcp_oauth_storage_set_client_info(storage, ci_json);
                free(ci_json);
                json_free(ci);
            }

            char *updated = json_serialize(ntj);
            mcp_oauth_storage_set_tokens(storage, updated);
            free(updated);

            /* Build success result */
            json_t *rj = json_object();
            json_set(rj, "success", json_bool(true));
            json_set(rj, "access_token", json_string(access));
            if (token_type)
                json_set(rj, "token_type", json_string(token_type));
            json_set(rj, "expires_in", json_number(expires_in));
            result = json_serialize(rj);
            json_free(rj);
        } else {
            result = make_result_json(false, NULL, "Token exchange returned no access_token");
        }
        json_free(ntj);
    } else {
        result = strdup(tokens_json);  /* pass through raw response as error */
    }

    free(tokens_json);
    if (mj) json_free(mj);
    mcp_oauth_storage_free(storage);
    if (cfg) json_free(cfg);
    return result;
}

bool mcp_oauth_remove_tokens(const char *server_name) {
    if (!server_name) return false;
    mcp_oauth_storage_t *s = mcp_oauth_storage_new(server_name);
    if (!s) return false;
    mcp_oauth_storage_remove(s);
    mcp_oauth_storage_free(s);
    return true;
}

/* ================================================================
 *  Manager layer (D86) — per-server registry with disk-change detection
 * ================================================================ */

#define MCP_OAUTH_MAX_SERVERS 32

typedef struct {
    char   server_name[128];
    time_t last_mtime;     /* last-seen st_mtime of tokens file, 0 = uninitialized */
    char   cached_token_json[8192]; /* last returned token JSON */
    bool   has_cached;
} mcp_oauth_entry_t;

static mcp_oauth_entry_t g_oauth_entries[MCP_OAUTH_MAX_SERVERS];
static int g_oauth_entry_count = 0;

/* Find or allocate a per-server entry */
static mcp_oauth_entry_t *get_or_create_entry(const char *server_name) {
    if (!server_name) return NULL;

    /* Look for existing */
    for (int i = 0; i < g_oauth_entry_count; i++) {
        if (strcmp(g_oauth_entries[i].server_name, server_name) == 0)
            return &g_oauth_entries[i];
    }

    /* Allocate new */
    if (g_oauth_entry_count >= MCP_OAUTH_MAX_SERVERS)
        return NULL;

    mcp_oauth_entry_t *e = &g_oauth_entries[g_oauth_entry_count++];
    memset(e, 0, sizeof(*e));
    snprintf(e->server_name, sizeof(e->server_name), "%s", server_name);
    return e;
}

/* Get the current mtime of a server's tokens file */
static time_t get_token_file_mtime(const char *server_name) {
    mcp_oauth_storage_t *s = mcp_oauth_storage_new(server_name);
    if (!s) return 0;

    /* Construct the tokens path */
    char safe[128];
    safe_filename(server_name, safe, sizeof(safe));
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.json", get_token_dir(), safe);

    struct stat st;
    time_t mtime = 0;
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
        mtime = st.st_mtime;

    mcp_oauth_storage_free(s);
    return mtime;
}

/* Try to get a cached access token from storage (no OAuth flow). Returns NULL if fails. */
static char *cached_access_token(const char *server_name,
                                  const char *server_url) {
    (void)server_url;  /* kept for API symmetry */
    mcp_oauth_storage_t *s = mcp_oauth_storage_new(server_name);
    if (!s) return NULL;

    char *tokens = mcp_oauth_storage_get_tokens(s);
    if (!tokens) { mcp_oauth_storage_free(s); return NULL; }

    json_t *j = json_parse(tokens, NULL);
    free(tokens);
    if (!j) { mcp_oauth_storage_free(s); return NULL; }

    const char *access = json_get_str(j, "access_token", NULL);
    const char *token_type = json_get_str(j, "token_type", NULL);
    double expires_in = json_get_num(j, "expires_in", 0);
    double expires_at = json_get_num(j, "expires_at", 0);

    char *result = NULL;

    /* Check expiry */
    bool expired = false;
    if (expires_at > 0) {
        expired = (time(NULL) >= (time_t)expires_at);
    }

    if (access && *access && !expired) {
        json_t *rj = json_object();
        json_set(rj, "success", json_bool(true));
        json_set(rj, "access_token", json_string(access));
        if (token_type) json_set(rj, "token_type", json_string(token_type));
        json_set(rj, "expires_in", json_number(expires_in));
        result = json_serialize(rj);
        json_free(rj);
    } else if (access && *access && expired) {
        /* Try refresh */
        const char *refresh = json_get_str(j, "refresh_token", NULL);
        if (refresh && *refresh) {
            char *meta = mcp_oauth_storage_get_metadata(s);
            if (meta) {
                json_t *mj = json_parse(meta, NULL);
                free(meta);
                if (mj) {
                    const char *token_url = json_get_str(mj, "token_endpoint", NULL);
                    if (token_url) {
                        const char *client_id = NULL;
                        /* Try to load client_id from stored client info */
                        char *ci = mcp_oauth_storage_get_client_info(s);
                        if (ci) {
                            json_t *cij = json_parse(ci, NULL);
                            free(ci);
                            if (cij) {
                                client_id = json_get_str(cij, "client_id", NULL);
                                json_free(cij);
                            }
                        }
                        char *new_tokens = mcp_oauth_refresh_token(token_url, client_id, refresh);
                        if (new_tokens) {
                            json_t *ntj = json_parse(new_tokens, NULL);
                            if (ntj) {
                                const char *new_access = json_get_str(ntj, "access_token", NULL);
                                if (new_access && *new_access) {
                                    /* Store with updated expires_at */
                                    double new_expires = json_get_num(ntj, "expires_in", 0);
                                    if (new_expires > 0) {
                                        json_set((json_t *)ntj, "expires_at",
                                                 json_number(time(NULL) + new_expires));
                                    }
                                    char *updated = json_serialize(ntj);
                                    mcp_oauth_storage_set_tokens(s, updated);
                                    free(updated);

                                    const char *new_type = json_get_str(ntj, "token_type", NULL);
                                    json_t *rj = json_object();
                                    json_set(rj, "success", json_bool(true));
                                    json_set(rj, "access_token", json_string(new_access));
                                    if (new_type)
                                        json_set(rj, "token_type", json_string(new_type));
                                    json_set(rj, "expires_in", json_number(new_expires));
                                    json_set(rj, "refreshed", json_bool(true));
                                    result = json_serialize(rj);
                                    json_free(rj);
                                }
                                json_free(ntj);
                            }
                            free(new_tokens);
                        }
                    }
                    json_free(mj);
                }
            }
        }
    }

    json_free(j);
    mcp_oauth_storage_free(s);
    return result;
}

char *mcp_oauth_manager_get_token(const char *server_name,
                                   const char *server_url,
                                   const char *oauth_config_json) {
    if (!server_name || !server_url) {
        json_t *j = json_object();
        json_set(j, "success", json_bool(false));
        json_set(j, "error", json_string("server_name and server_url are required"));
        char *r = json_serialize(j);
        json_free(j);
        return r;
    }

    mcp_oauth_entry_t *entry = get_or_create_entry(server_name);
    if (!entry) {
        json_t *j = json_object();
        json_set(j, "success", json_bool(false));
        json_set(j, "error", json_string("Max MCP OAuth servers reached"));
        char *r = json_serialize(j);
        json_free(j);
        return r;
    }

    /* Check if token file mtime changed (external refresh) */
    time_t current_mtime = get_token_file_mtime(server_name);
    bool disk_changed = (current_mtime != entry->last_mtime);

    /* If disk changed or no cached token, try storage */
    if (disk_changed || !entry->has_cached) {
        entry->last_mtime = current_mtime;

        char *token_result = cached_access_token(server_name, server_url);
        if (token_result) {
            /* Cache the result */
            json_t *j = json_parse(token_result, NULL);
            if (j) {
                snprintf(entry->cached_token_json, sizeof(entry->cached_token_json),
                         "%s", token_result);
                entry->has_cached = true;
                json_free(j);
            }
            return token_result;
        }

        /* No cached token — run full OAuth flow */
        char *auth_result = mcp_oauth_build_auth(server_name, server_url,
                                                   oauth_config_json);
        if (auth_result) {
            json_t *j = json_parse(auth_result, NULL);
            if (j) {
                bool success = json_get_bool(j, "success", false);
                if (success) {
                    snprintf(entry->cached_token_json, sizeof(entry->cached_token_json),
                             "%s", auth_result);
                    entry->has_cached = true;
                    /* Update mtime since we just wrote tokens */
                    entry->last_mtime = get_token_file_mtime(server_name);
                }
                json_free(j);
            }
        }
        return auth_result;
    }

    /* Disk unchanged and we have cached token — return it */
    char *copy = strdup(entry->cached_token_json);
    return copy;
}

char *mcp_oauth_manager_reauthorize(const char *server_name,
                                     const char *server_url,
                                     const char *oauth_config_json) {
    if (!server_name) {
        json_t *j = json_object();
        json_set(j, "success", json_bool(false));
        json_set(j, "error", json_string("server_name is required"));
        char *r = json_serialize(j);
        json_free(j);
        return r;
    }

    /* Clear storage */
    mcp_oauth_remove_tokens(server_name);

    /* Clear cached state */
    mcp_oauth_entry_t *entry = get_or_create_entry(server_name);
    if (entry) {
        entry->has_cached = false;
        entry->last_mtime = 0;
        entry->cached_token_json[0] = '\0';
    }

    /* Run full OAuth flow */
    return mcp_oauth_build_auth(server_name, server_url, oauth_config_json);
}
