/*
 * webhook.c — HTTP API gateway for Hermes C.
 * Accepts POST /webhook with JSON {"text":"...", "chat_id":"..."}
 * Returns JSON {"status":"ok", "response":"..."}
 *
 * P112: HMAC signature verification, subscription-based outgoing webhooks
 *       with exponential backoff retry, custom headers, and per-endpoint
 *       rate limiting.
 *
 * MIT License — WuBu Hermes Project
 */

#include "hermes.h"
#include "hermes_agent.h"
#include "hermes_json.h"
#include "hermes_gateway.h"
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <math.h>
#include <time.h>
#include <ctype.h>

/* SHA-256 hash is 32 bytes */
#define WH_SHA256_LEN 32

/* ================================================================
 *  P112: Standalone hex encoding (avoid pulling in crypto.h directly)
 * ================================================================ */

static char *wh_hex_encode(const unsigned char *data, size_t len) {
    if (!data || len == 0) return NULL;
    char *out = (char *)malloc(len * 2 + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < len; i++)
        snprintf(out + i * 2, 3, "%02x", data[i]);
    return out;
}

/* ================================================================
 *  P112: Standalone rate limiter helpers (operate on gw_rate_limiter_t*)
 * ================================================================ */

static double wh_mono_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void wh_rate_limit_init(gw_rate_limiter_t *rl,
                                double tokens_per_sec, double max_burst) {
    if (!rl) return;
    rl->tokens_per_sec = tokens_per_sec;
    rl->max_tokens = max_burst;
    rl->tokens = max_burst;
    rl->last_refill = wh_mono_time();
}

static bool wh_rate_limit_check(gw_rate_limiter_t *rl) {
    if (!rl) return true;
    double now = wh_mono_time();
    double elapsed = now - rl->last_refill;
    rl->tokens += elapsed * rl->tokens_per_sec;
    if (rl->tokens > rl->max_tokens)
        rl->tokens = rl->max_tokens;
    rl->last_refill = now;
    if (rl->tokens >= 1.0) {
        rl->tokens -= 1.0;
        return true;
    }
    return false;
}

/* ================================================================
 *  P112: Static state
 * ================================================================ */

/* HMAC secret for incoming webhook verification */
static char g_verify_secret[128] = {0};
static bool g_verify_enabled = false;

/* Subscription table */
static webhook_subscription_t g_subscriptions[WEBHOOK_SUBS_MAX];
static int g_sub_count = 0;
static pthread_mutex_t g_sub_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Default rate limit for subscriptions (10 req/s, burst 20) */
#define WEBHOOK_DEFAULT_RATE   10.0
#define WEBHOOK_DEFAULT_BURST  20.0

/* ================================================================
 *  Safe write wrapper (discards return value to suppress warnings)
 * ================================================================ */

static inline void safe_write(int fd, const void *buf, size_t count) {
    ssize_t r = write(fd, buf, count);
    (void)r;
}

/* ================================================================
 *  HTTP response builder
 * ================================================================ */

static char *build_http_response(int status, const char *status_text,
                                  const char *content_type,
                                  const char *body) {
    int blen = strlen(body);
    /* header: ~256 bytes + body */
    int total = 256 + blen + 1;
    char *resp = malloc(total);
    if (!resp) return NULL;

    int n = snprintf(resp, total,
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type, X-Hub-Signature-256\r\n"
        "\r\n"
        "%s",
        status, status_text,
        content_type,
        blen,
        body);
    if (n < 0) { free(resp); return NULL; }
    return resp;
}

/* ================================================================
 *  JSON string escaper
 * ================================================================ */

static char *json_escape_str(const char *s) {
    if (!s) return strdup("null");
    size_t cap = strlen(s) * 2 + 3; /* worst case: all chars escaped */
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t j = 0;
    out[j++] = '"';
    for (const char *p = s; *p; p++) {
        switch (*p) {
            case '"':  out[j++] = '\\'; out[j++] = '"';  break;
            case '\\': out[j++] = '\\'; out[j++] = '\\'; break;
            case '\n': out[j++] = '\\'; out[j++] = 'n';  break;
            case '\r': out[j++] = '\\'; out[j++] = 'r';  break;
            case '\t': out[j++] = '\\'; out[j++] = 't';  break;
            default:
                if ((unsigned char)*p < 0x20) {
                    /* control char — skip or encode as \u00xx */
                    j += snprintf(out + j, cap - j, "\\u%04x", (unsigned char)*p);
                } else {
                    out[j++] = *p;
                }
                break;
        }
        if (j >= cap - 8) {
            cap *= 2;
            char *tmp = realloc(out, cap);
            if (!tmp) { free(out); return NULL; }
            out = tmp;
        }
    }
    out[j++] = '"';
    out[j] = '\0';
    return out;
}

/* ================================================================
 *  HTTP request body extractor
 * ================================================================
 * Returns pointer to body within buf (after \r\n\r\n), or NULL.
 */

static const char *extract_http_body(const char *buf) {
    const char *body = strstr(buf, "\r\n\r\n");
    return body ? body + 4 : NULL;
}

/* ================================================================
 *  Extract HTTP method + path
 * ================================================================
 * Parses "POST /webhook HTTP/1.1" from buf.
 * Returns -1 on error.
 */

static int parse_http_request_line(const char *buf, char *method, size_t msize,
                                    char *path, size_t psize) {
    /* Skip leading whitespace */
    while (*buf == ' ' || *buf == '\t' || *buf == '\r' || *buf == '\n') buf++;
    if (!*buf) return -1;

    /* Read method up to space */
    size_t i = 0;
    while (*buf && *buf != ' ' && *buf != '\r' && i < msize - 1) {
        method[i++] = *buf++;
    }
    method[i] = '\0';
    if (*buf != ' ') return -1;
    buf++; /* skip space */

    /* Read path up to space */
    i = 0;
    while (*buf && *buf != ' ' && *buf != '\r' && i < psize - 1) {
        path[i++] = *buf++;
    }
    path[i] = '\0';
    return 0;
}

/* ================================================================
 *  P112: Extract a specific header value from raw HTTP request
 * ================================================================
 * Scans buf for "Header-Name: value\r\n" after the request line.
 * Returns malloc'd value string, or NULL if not found.
 */

static char *extract_header(const char *buf, const char *header_name) {
    if (!buf || !header_name) return NULL;

    const char *p = buf;
    /* Skip the request line (first line) */
    p = strstr(p, "\r\n");
    if (!p) return NULL;
    p += 2; /* skip \r\n */

    size_t name_len = strlen(header_name);

    while (*p) {
        /* Check for end of headers */
        if (p[0] == '\r' && p[1] == '\n') break;

        /* Case-insensitive prefix check */
        bool match = true;
        for (size_t i = 0; i < name_len; i++) {
            char a = (char)tolower((unsigned char)p[i]);
            char b = (char)tolower((unsigned char)header_name[i]);
            if (a != b) { match = false; break; }
        }
        if (match && p[name_len] == ':') {
            p += name_len + 1;
            /* Skip whitespace after colon */
            while (*p == ' ' || *p == '\t') p++;
            /* Read value up to \r\n */
            const char *end = strstr(p, "\r\n");
            if (!end) return NULL;
            size_t val_len = (size_t)(end - p);
            char *val = (char *)malloc(val_len + 1);
            if (!val) return NULL;
            memcpy(val, p, val_len);
            val[val_len] = '\0';
            return val;
        }

        /* Skip to next line */
        p = strstr(p, "\r\n");
        if (!p) break;
        p += 2;
    }
    return NULL;
}

/* ================================================================
 *  P112: HMAC verification
 * ================================================================ */

void webhook_set_verify_secret(const char *secret) {
    pthread_mutex_lock(&g_sub_mutex);
    if (secret && *secret) {
        strncpy(g_verify_secret, secret, sizeof(g_verify_secret) - 1);
        g_verify_secret[sizeof(g_verify_secret) - 1] = '\0';
        g_verify_enabled = true;
    } else {
        g_verify_secret[0] = '\0';
        g_verify_enabled = false;
    }
    pthread_mutex_unlock(&g_sub_mutex);
}

bool webhook_verify_hmac(const char *signature, const unsigned char *body, size_t body_len) {
    if (!signature || !body) return false;

    /* Expected format: "sha256=<hex>" */
    const char *prefix = "sha256=";
    size_t prefix_len = strlen(prefix);

    if (strncmp(signature, prefix, prefix_len) != 0) {
        printf("[webhook:hmac] Invalid signature format (expected sha256=<hex>)\n");
        return false;
    }

    const char *hex_sig = signature + prefix_len;
    size_t hex_len = strlen(hex_sig);

    /* SHA-256 hex is 64 chars */
    if (hex_len != 64) {
        printf("[webhook:hmac] Invalid signature length: %zu (expected 64)\n", hex_len);
        return false;
    }

    /* Compute expected HMAC */
    unsigned char expected[WH_SHA256_LEN];
    pthread_mutex_lock(&g_sub_mutex);
    const char *secret = g_verify_secret;
    size_t secret_len = strlen(secret);
    crypto_hmac_sha256((const unsigned char *)secret, secret_len,
                       body, body_len, expected);
    pthread_mutex_unlock(&g_sub_mutex);

    /* Convert to hex for comparison */
    char *expected_hex = wh_hex_encode(expected, WH_SHA256_LEN);
    if (!expected_hex) return false;

    /* Constant-time comparison */
    int result = 0;
    for (int i = 0; i < 64; i++) {
        result |= (expected_hex[i] ^ hex_sig[i]);
    }

    free(expected_hex);
    return result == 0;
}

/* ================================================================
 *  P112: Subscription management
 * ================================================================ */

int webhook_subscription_add(const char *endpoint, const char *secret,
                              int max_retries, int backoff_ms) {
    if (!endpoint || !*endpoint) return -1;

    pthread_mutex_lock(&g_sub_mutex);

    /* Find a free slot */
    int idx = -1;
    for (int i = 0; i < WEBHOOK_SUBS_MAX; i++) {
        if (!g_subscriptions[i].in_use) {
            idx = i;
            break;
        }
    }

    if (idx < 0) {
        pthread_mutex_unlock(&g_sub_mutex);
        printf("[webhook] Subscription table full (%d max)\n", WEBHOOK_SUBS_MAX);
        return -1;
    }

    webhook_subscription_t *sub = &g_subscriptions[idx];
    memset(sub, 0, sizeof(*sub));

    strncpy(sub->endpoint, endpoint, sizeof(sub->endpoint) - 1);
    if (secret && *secret) {
        strncpy(sub->hmac_secret, secret, sizeof(sub->hmac_secret) - 1);
    }

    sub->max_retries = (max_retries > 0) ? max_retries : 3;
    sub->backoff_ms = (backoff_ms > 0) ? backoff_ms : 1000;
    sub->header_count = 0;
    sub->in_use = true;

    /* Initialize rate limiter: 10 req/s, burst 20 */
    wh_rate_limit_init(&sub->rate_limiter, WEBHOOK_DEFAULT_RATE, WEBHOOK_DEFAULT_BURST);

    g_sub_count++;
    pthread_mutex_unlock(&g_sub_mutex);

    printf("[webhook] Subscription #%d added: %s (retries=%d, backoff=%dms)\n",
           idx, endpoint, sub->max_retries, sub->backoff_ms);

    return idx;
}

bool webhook_subscription_remove(int idx) {
    if (idx < 0 || idx >= WEBHOOK_SUBS_MAX) return false;

    pthread_mutex_lock(&g_sub_mutex);
    if (!g_subscriptions[idx].in_use) {
        pthread_mutex_unlock(&g_sub_mutex);
        return false;
    }
    memset(&g_subscriptions[idx], 0, sizeof(webhook_subscription_t));
    g_sub_count--;
    pthread_mutex_unlock(&g_sub_mutex);

    printf("[webhook] Subscription #%d removed\n", idx);
    return true;
}

bool webhook_subscription_add_header(int idx, const char *key, const char *value) {
    if (idx < 0 || idx >= WEBHOOK_SUBS_MAX || !key || !*key) return false;

    pthread_mutex_lock(&g_sub_mutex);
    webhook_subscription_t *sub = &g_subscriptions[idx];
    if (!sub->in_use || sub->header_count >= WEBHOOK_HEADERS_MAX) {
        pthread_mutex_unlock(&g_sub_mutex);
        return false;
    }

    webhook_header_t *h = &sub->headers[sub->header_count];
    strncpy(h->key, key, sizeof(h->key) - 1);
    if (value) {
        strncpy(h->value, value, sizeof(h->value) - 1);
    } else {
        h->value[0] = '\0';
    }
    sub->header_count++;
    pthread_mutex_unlock(&g_sub_mutex);

    return true;
}

int webhook_subscription_count(void) {
    return g_sub_count;
}

int webhook_subscription_list(webhook_subscription_t *out, int max_out) {
    if (!out || max_out <= 0) return 0;

    pthread_mutex_lock(&g_sub_mutex);
    int written = 0;
    for (int i = 0; i < WEBHOOK_SUBS_MAX && written < max_out; i++) {
        if (g_subscriptions[i].in_use) {
            memcpy(&out[written], &g_subscriptions[i], sizeof(webhook_subscription_t));
            written++;
        }
    }
    pthread_mutex_unlock(&g_sub_mutex);
    return written;
}

/* ================================================================
 *  P112: Build custom headers string from subscription
 * ================================================================
 * Returns a malloc'd string like "Key1: Val1\r\nKey2: Val2\r\n"
 * or NULL if no custom headers. Caller must free().
 */

static char *build_custom_headers_str(const webhook_subscription_t *sub) {
    if (!sub || sub->header_count == 0) return NULL;

    /* Estimate: each header ~ key(128) + ": " + value(512) + "\r\n" = ~650 */
    size_t cap = sub->header_count * 650 + 1;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    out[0] = '\0';
    size_t pos = 0;

    for (int i = 0; i < sub->header_count; i++) {
        int n = snprintf(out + pos, cap - pos, "%s: %s\r\n",
                         sub->headers[i].key, sub->headers[i].value);
        if (n < 0 || (size_t)n >= cap - pos) {
            free(out);
            return NULL;
        }
        pos += (size_t)n;
    }
    return out;
}

/* ================================================================
 *  P112: Build HMAC signature header for outgoing webhook
 * ================================================================
 * Returns a malloc'd header line like "X-Hub-Signature-256: sha256=<hex>\r\n"
 * or NULL if no secret configured. Caller must free().
 */

static char *build_hmac_header_line(const webhook_subscription_t *sub,
                                     const char *payload) {
    if (!sub || !sub->hmac_secret[0] || !payload) return NULL;

    unsigned char sig[WH_SHA256_LEN];
    size_t secret_len = strlen(sub->hmac_secret);
    size_t pay_len = strlen(payload);

    crypto_hmac_sha256((const unsigned char *)sub->hmac_secret, secret_len,
                       (const unsigned char *)payload, pay_len, sig);

    char *hex = wh_hex_encode(sig, WH_SHA256_LEN);
    if (!hex) return NULL;

    /* "X-Hub-Signature-256: sha256=" + 64 hex chars + "\r\n" = ~95 */
    char *line = (char *)malloc(128);
    if (!line) { free(hex); return NULL; }
    snprintf(line, 128, "X-Hub-Signature-256: sha256=%s\r\n", hex);
    free(hex);
    return line;
}

/* ================================================================
 *  P112: One-shot webhook send
 * ================================================================
 * Sends payload to endpoint with optional retry.
 * custom_headers is optional raw header string (e.g. "Auth: Bearer x\r\n").
 */

bool webhook_send_payload(const char *endpoint, const char *payload,
                           const char *custom_headers, int timeout_sec,
                           int max_retries, int backoff_ms) {
    if (!endpoint || !payload) return false;

    if (timeout_sec <= 0) timeout_sec = 10;
    if (max_retries <= 0) max_retries = 3;
    if (backoff_ms <= 0) backoff_ms = 1000;

    /* Build combined headers */
    char headers_buf[4096];
    int hlen = snprintf(headers_buf, sizeof(headers_buf),
                        "Content-Type: application/json\r\n");
    if (hlen < 0) return false;

    if (custom_headers && *custom_headers) {
        size_t clen = strlen(custom_headers);
        if ((size_t)hlen + clen < sizeof(headers_buf)) {
            memcpy(headers_buf + hlen, custom_headers, clen);
            hlen += (int)clen;
        }
    }

    headers_buf[hlen] = '\0';

    for (int attempt = 0; attempt <= max_retries; attempt++) {
        http_t *client = http_new(timeout_sec);
        if (!client) return false;

        http_resp_t *resp = http_request(client, HTTP_POST, endpoint,
                                          headers_buf, payload, strlen(payload));

        if (resp && resp->status >= 200 && resp->status < 300) {
            printf("[webhook] Sent to %s: status %d (attempt %d/%d)\n",
                   endpoint, resp->status, attempt + 1, max_retries + 1);
            http_resp_free(resp);
            http_free(client);
            return true;
        }

        int status = resp ? resp->status : -1;
        printf("[webhook] Send to %s failed: status %d (attempt %d/%d)\n",
               endpoint, status, attempt + 1, max_retries + 1);
        http_resp_free(resp);
        http_free(client);

        if (attempt < max_retries) {
            /* Exponential backoff with jitter */
            int delay = backoff_ms * (1 << attempt);
            /* Add ~25% jitter */
            int jitter = rand() % (delay / 4 + 1);
            delay += jitter;
            usleep((useconds_t)delay * 1000);
        }
    }

    printf("[webhook] All %d attempts to %s failed\n", max_retries + 1, endpoint);
    return false;
}

/* ================================================================
 *  P112: Subscription-based delivery with rate limiting
 * ================================================================ */

bool webhook_subscription_deliver(int idx, const char *payload) {
    if (idx < 0 || idx >= WEBHOOK_SUBS_MAX || !payload) return false;

    pthread_mutex_lock(&g_sub_mutex);
    webhook_subscription_t *sub = &g_subscriptions[idx];
    if (!sub->in_use) {
        pthread_mutex_unlock(&g_sub_mutex);
        return false;
    }

    /* Rate limit check */
    if (!wh_rate_limit_check(&sub->rate_limiter)) {
        printf("[webhook] Rate limited: subscription #%d (%s) — skipping delivery\n",
               idx, sub->endpoint);
        pthread_mutex_unlock(&g_sub_mutex);
        return false;
    }

    /* Build headers: custom headers + optional HMAC signature */
    char *custom_hdrs = build_custom_headers_str(sub);
    char *hmac_line = build_hmac_header_line(sub, payload);

    /* Combine into a single header string */
    char headers_buf[8192];
    int hlen = snprintf(headers_buf, sizeof(headers_buf),
                        "Content-Type: application/json\r\n");
    if (hlen < 0) {
        free(custom_hdrs);
        free(hmac_line);
        pthread_mutex_unlock(&g_sub_mutex);
        return false;
    }

    if (hmac_line) {
        size_t hlen2 = strlen(hmac_line);
        if ((size_t)hlen + hlen2 < sizeof(headers_buf)) {
            memcpy(headers_buf + hlen, hmac_line, hlen2);
            hlen += (int)hlen2;
        }
    }

    if (custom_hdrs) {
        size_t clen = strlen(custom_hdrs);
        if ((size_t)hlen + clen < sizeof(headers_buf)) {
            memcpy(headers_buf + hlen, custom_hdrs, clen);
            hlen += (int)clen;
        }
    }
    headers_buf[hlen] = '\0';

    int max_retries = sub->max_retries;
    int backoff_ms = sub->backoff_ms;
    char endpoint[512];
    strncpy(endpoint, sub->endpoint, sizeof(endpoint) - 1);
    endpoint[sizeof(endpoint) - 1] = '\0';
    pthread_mutex_unlock(&g_sub_mutex);

    free(custom_hdrs);
    free(hmac_line);

    /* Perform delivery with retries */
    for (int attempt = 0; attempt <= max_retries; attempt++) {
        http_t *client = http_new(10);
        if (!client) continue;

        http_resp_t *resp = http_request(client, HTTP_POST, endpoint,
                                          headers_buf, payload, strlen(payload));

        if (resp && resp->status >= 200 && resp->status < 300) {
            printf("[webhook] Subscription #%d delivered: status %d (attempt %d/%d)\n",
                   idx, resp->status, attempt + 1, max_retries + 1);
            http_resp_free(resp);
            http_free(client);
            return true;
        }

        int status = resp ? resp->status : -1;
        printf("[webhook] Subscription #%d delivery failed: status %d (attempt %d/%d)\n",
               idx, status, attempt + 1, max_retries + 1);
        http_resp_free(resp);
        http_free(client);

        if (attempt < max_retries) {
            /* Exponential backoff with jitter */
            int delay = backoff_ms * (1 << attempt);
            int jitter = rand() % (delay / 4 + 1);
            delay += jitter;
            usleep((useconds_t)delay * 1000);
        }
    }

    printf("[webhook] Subscription #%d all %d attempts failed\n",
           idx, max_retries + 1);
    return false;
}

/* ================================================================
 *  Handle a single webhook request
 * ================================================================ */

static void handle_webhook_request(int client_fd) {
    char buf[65536];
    int n = (int)read(client_fd, buf, sizeof(buf) - 1);
    if (n <= 0) {
        close(client_fd);
        return;
    }
    buf[n] = '\0';

    char method[16], path[512];
    if (parse_http_request_line(buf, method, sizeof(method),
                                 path, sizeof(path)) < 0) {
        char *r = build_http_response(400, "Bad Request",
                                       "text/plain", "Bad Request");
        if (r) { safe_write(client_fd, r, strlen(r)); free(r); }
        close(client_fd);
        return;
    }

    /* CORS preflight */
    if (strcmp(method, "OPTIONS") == 0) {
        char *r = build_http_response(204, "No Content",
                                       "text/plain", "");
        if (r) { safe_write(client_fd, r, strlen(r)); free(r); }
        close(client_fd);
        return;
    }

    /* GET /health — simple health check */
    if (strcmp(method, "GET") == 0 && strcmp(path, "/health") == 0) {
        char *r = build_http_response(200, "OK",
                                       "application/json",
                                       "{\"status\":\"ok\",\"service\":\"hermes-webhook\"}");
        if (r) { safe_write(client_fd, r, strlen(r)); free(r); }
        close(client_fd);
        return;
    }

    /* Only POST /webhook */
    if (strcmp(method, "POST") != 0 || strcmp(path, "/webhook") != 0) {
        /* Check for WhatsApp webhook callback on GET /whatsapp-webhook */
        if (strcmp(method, "GET") == 0 && strcmp(path, "/whatsapp-webhook") == 0) {
            const char *qs = strchr(buf, '?');
            if (qs) qs++; else qs = "";
            const char *challenge = whatsapp_verify_webhook(qs);
            if (challenge) {
                char *r = build_http_response(200, "OK",
                                               "text/plain", challenge);
                if (r) { safe_write(client_fd, r, strlen(r)); free(r); }
            } else {
                char *r = build_http_response(403, "Forbidden",
                                               "text/plain", "Verification failed");
                if (r) { safe_write(client_fd, r, strlen(r)); free(r); }
            }
            close(client_fd);
            return;
        }

        /* Check for WhatsApp POST callback */
        if (strcmp(method, "POST") == 0 && strcmp(path, "/whatsapp-webhook") == 0) {
            const char *body = extract_http_body(buf);
            if (body) {
                json_node_t *updates = whatsapp_parse_webhook(body);
                if (updates && json_len(updates) > 0) {
                    size_t n = json_len(updates);
                    for (size_t i = 0; i < n; i++) {
                        json_node_t *update = json_get(updates, i);
                        const char *from = whatsapp_get_chat_id(update);
                        const char *text = whatsapp_get_text(update);
                        if (from && text) {
                            printf("[gateway:whatsapp] Message from %s: %s\n",
                                   from, text);
                            char *resp = agent_chat(&g_gw.agent, text);
                            if (resp) {
                                whatsapp_send_message(g_gw.http, from, resp);
                                free(resp);
                            }
                        }
                    }
                    json_free(updates);
                }
            }
            char *r = build_http_response(200, "OK",
                                           "application/json",
                                           "{\"status\":\"ok\"}");
            if (r) { safe_write(client_fd, r, strlen(r)); free(r); }
            close(client_fd);
            return;
        }

        /* P111: SMS webhook — GET for Twilio number verification */
        if (strcmp(method, "GET") == 0 && strcmp(path, "/sms-webhook") == 0) {
            const char *qs = strchr(buf, '?');
            if (qs) qs++; else qs = "";
            const char *challenge = sms_verify_webhook(qs);
            if (challenge) {
                char *r = build_http_response(200, "OK",
                                               "text/xml",
                                               "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                                               "<Response></Response>");
                if (r) { safe_write(client_fd, r, strlen(r)); free(r); }
            } else {
                char *r = build_http_response(403, "Forbidden",
                                               "text/plain", "Verification failed");
                if (r) { safe_write(client_fd, r, strlen(r)); free(r); }
            }
            close(client_fd);
            return;
        }

        /* P111: SMS webhook — POST for inbound SMS/MMS + delivery callbacks */
        if (strcmp(method, "POST") == 0 && strcmp(path, "/sms-webhook") == 0) {
            const char *body = extract_http_body(buf);
            if (body) {
                json_node_t *updates = sms_parse_webhook(body);
                if (updates && json_len(updates) > 0) {
                    size_t n = json_len(updates);
                    for (size_t i = 0; i < n; i++) {
                        json_node_t *update = json_get(updates, i);
                        const char *from = sms_get_chat_id(update);
                        const char *text = sms_get_text(update);
                        const char *status = sms_get_status(update);
                        const char *msg_sid = sms_get_message_sid(update);
                        size_t num_media = sms_get_num_media(update);

                        /* Status callback — just log delivery status */
                        if (status && status[0]) {
                            printf("[gateway:sms] Status callback: SID=%s status=%s from=%s\n",
                                   msg_sid, status, from);
                            continue;
                        }

                        /* Inbound message — respond */
                        if (from && text) {
                            printf("[gateway:sms] Message from %s: %s",
                                   from, text);
                            if (num_media > 0) {
                                const char *murl = sms_get_media_url(update, 0);
                                printf(" + %zu media (first: %s)", num_media, murl ? murl : "?");
                            }
                            printf("\n");

                            char *resp = agent_chat(&g_gw.agent, text);
                            if (resp) {
                                sms_send_message(g_gw.http, from, resp);
                                free(resp);
                            }
                        }
                    }
                    json_free(updates);
                }
            }
            /* Twilio expects TwiML response for SMS webhooks */
            char *r = build_http_response(200, "OK",
                                           "text/xml",
                                           "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                                           "<Response></Response>");
            if (r) { safe_write(client_fd, r, strlen(r)); free(r); }
            close(client_fd);
            return;
        }

        char *r = build_http_response(404, "Not Found",
                                       "application/json",
                                       "{\"error\":\"Not Found. Use POST /webhook\"}");
        if (r) { safe_write(client_fd, r, strlen(r)); free(r); }
        close(client_fd);
        return;
    }

    const char *body = extract_http_body(buf);
    if (!body) {
        char *r = build_http_response(400, "Bad Request",
                                       "application/json",
                                       "{\"error\":\"No request body\"}");
        if (r) { safe_write(client_fd, r, strlen(r)); free(r); }
        close(client_fd);
        return;
    }

    /* P112: HMAC signature verification for incoming webhooks */
    if (g_verify_enabled) {
        char *signature = extract_header(buf, "x-hub-signature-256");
        if (!signature) {
            printf("[webhook] HMAC verification enabled but no X-Hub-Signature-256 header\n");
            char *r = build_http_response(401, "Unauthorized",
                                           "application/json",
                                           "{\"error\":\"Missing signature header\"}");
            if (r) { safe_write(client_fd, r, strlen(r)); free(r); }
            close(client_fd);
            return;
        }

        if (!webhook_verify_hmac(signature,
                                  (const unsigned char *)body, strlen(body))) {
            printf("[webhook] HMAC signature verification failed\n");
            free(signature);
            char *r = build_http_response(403, "Forbidden",
                                           "application/json",
                                           "{\"error\":\"Invalid signature\"}");
            if (r) { safe_write(client_fd, r, strlen(r)); free(r); }
            close(client_fd);
            return;
        }
        free(signature);
        printf("[webhook] HMAC signature verified\n");
    }

    json_node_t *root = json_parse(body, NULL);
    if (!root) {
        char *r = build_http_response(400, "Bad Request",
                                       "application/json",
                                       "{\"error\":\"Invalid JSON\"}");
        if (r) { safe_write(client_fd, r, strlen(r)); free(r); }
        close(client_fd);
        return;
    }

    const char *text = json_get_str(root, "text", "");
    const char *chat_id = json_get_str(root, "chat_id", "");

    if (!*text) {
        json_free(root);
        char *r = build_http_response(400, "Bad Request",
                                       "application/json",
                                       "{\"error\":\"Missing 'text' field\"}");
        if (r) { safe_write(client_fd, r, strlen(r)); free(r); }
        close(client_fd);
        return;
    }

    printf("[gateway:webhook] POST /webhook from '%s': %s\n",
           *chat_id ? chat_id : "(anonymous)", text);

    /* Get agent state via gateway extern — defined in server.c */
    extern gateway_state_t g_gw;
    char *agent_resp = agent_chat(&g_gw.agent, text);

    /* Build JSON response */
    char *escaped_resp = json_escape_str(agent_resp ? agent_resp : "");
    char *escaped_chat = json_escape_str(chat_id);

    char json_buf[65536];
    snprintf(json_buf, sizeof(json_buf),
        "{\"status\":\"ok\",\"response\":%s,\"chat_id\":%s}",
        escaped_resp, escaped_chat);

    char *http_resp = build_http_response(200, "OK",
                                           "application/json", json_buf);
    if (http_resp) {
        safe_write(client_fd, http_resp, strlen(http_resp));
        free(http_resp);
    }

    free(escaped_resp);
    free(escaped_chat);
    json_free(root);
    free(agent_resp);
    close(client_fd);
}

/* ================================================================
 *  Main webhook server loop
 * ================================================================
 * Creates TCP server socket, accepts connections, handles requests.
 * Blocks until g_gw.running is set to false.
 */

void webhook_server_run(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("[webhook] socket");
        return;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)port);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[webhook] bind");
        close(server_fd);
        return;
    }

    if (listen(server_fd, 10) < 0) {
        perror("[webhook] listen");
        close(server_fd);
        return;
    }

    printf("[webhook] HTTP API server listening on port %d\n", port);
    printf("[webhook] HMAC verification: %s\n",
           g_verify_enabled ? "enabled" : "disabled");

    /* Seed random for jitter */
    srand((unsigned int)(time(NULL) ^ (time_t)port));

    /* Make server socket non-blocking so we can check g_gw.running */
    int flags = fcntl(server_fd, F_GETFL, 0);
    fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);

    extern gateway_state_t g_gw;

    while (g_gw.running) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(server_fd,
                               (struct sockaddr*)&client_addr, &addr_len);

        if (client_fd >= 0) {
            handle_webhook_request(client_fd);
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            /* Real error, not just no connection waiting */
            if (g_gw.running) {
                perror("[webhook] accept");
                sleep(1);
            }
        } else {
            /* No pending connection — sleep briefly to avoid busy-wait */
            usleep(100000); /* 100ms */
        }
    }

    close(server_fd);
    printf("[webhook] Server stopped\n");
}
