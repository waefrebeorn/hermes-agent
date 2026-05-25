/*
 * server.c — Multi-platform gateway server for Hermes C.
 * Supports Telegram, Discord, Slack, Matrix, Mattermost, Webhook, WhatsApp.
 * Platforms run concurrently via pthread. Each gets its own HTTP client.
 * Configured via --platform flag (single) or config.yaml gateway.platforms list.
 */

#include "hermes.h"
#include "hermes_agent.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "hermes_gateway.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <strings.h>
#include <time.h>
#include <sys/stat.h>

/* ================================================================
 *  Gateway state
 * ================================================================ */

gateway_state_t g_gw;

/* ================================================================
 *  P101: Monotonic time helper
 * ================================================================ */

static double gw_mono_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* ================================================================
 *  P101: Message queue (thread-safe, bounded circular buffer)
 * ================================================================ */

void gw_queue_init(void) {
    g_gw.msg_queue_head = 0;
    g_gw.msg_queue_tail = 0;
    pthread_mutex_init(&g_gw.queue_mutex, NULL);
    pthread_cond_init(&g_gw.queue_cond, NULL);
}

bool gw_queue_push(const char *platform, const char *chat_id,
                    const char *text, const char *thread_id) {
    if (!platform || !chat_id || !text) return false;

    pthread_mutex_lock(&g_gw.queue_mutex);

    /* Check if queue is full */
    int next = (g_gw.msg_queue_head + 1) % GW_QUEUE_MAX;
    if (next == g_gw.msg_queue_tail) {
        /* Queue full — drop oldest */
        g_gw.msg_queue_tail = (g_gw.msg_queue_tail + 1) % GW_QUEUE_MAX;
    }

    gateway_msg_t *slot = &g_gw.msg_queue[g_gw.msg_queue_head];
    snprintf(slot->platform, sizeof(slot->platform), "%s", platform);
    snprintf(slot->chat_id, sizeof(slot->chat_id), "%s", chat_id);
    snprintf(slot->text, sizeof(slot->text), "%s", text);
    if (thread_id)
        snprintf(slot->thread_id, sizeof(slot->thread_id), "%s", thread_id);
    else
        slot->thread_id[0] = '\0';
    slot->timestamp = gw_mono_time();

    g_gw.msg_queue_head = next;

    pthread_cond_signal(&g_gw.queue_cond);
    pthread_mutex_unlock(&g_gw.queue_mutex);
    return true;
}

bool gw_queue_pop(gateway_msg_t *msg) {
    if (!msg) return false;

    pthread_mutex_lock(&g_gw.queue_mutex);
    if (g_gw.msg_queue_head == g_gw.msg_queue_tail) {
        pthread_mutex_unlock(&g_gw.queue_mutex);
        return false; /* empty */
    }

    *msg = g_gw.msg_queue[g_gw.msg_queue_tail];
    g_gw.msg_queue_tail = (g_gw.msg_queue_tail + 1) % GW_QUEUE_MAX;
    pthread_mutex_unlock(&g_gw.queue_mutex);
    return true;
}

int gw_queue_depth(void) {
    pthread_mutex_lock(&g_gw.queue_mutex);
    int depth = (g_gw.msg_queue_head - g_gw.msg_queue_tail + GW_QUEUE_MAX) % GW_QUEUE_MAX;
    pthread_mutex_unlock(&g_gw.queue_mutex);
    return depth;
}

/* ================================================================
 *  Gateway stderr log-to-file with rotation (B15)
 * ================================================================ */

#define GW_LOG_MAX_BYTES (10 * 1024 * 1024)  /* 10 MB before rotation */
#define GW_LOG_PATH_MAX 512

static FILE *g_gw_log_fp = NULL;
static char  g_gw_log_path[GW_LOG_PATH_MAX] = {0};

/** Open gateway log file, rotate if >10 MB, for persistent log capture. */
static void gw_log_open(void) {
    const char *home = getenv("SLERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) return;

    snprintf(g_gw_log_path, sizeof(g_gw_log_path),
             "%s/.slermes/logs/gateway.log", home);

    struct stat st;
    if (stat(g_gw_log_path, &st) == 0 && st.st_size > GW_LOG_MAX_BYTES) {
        char old[GW_LOG_PATH_MAX];
        snprintf(old, sizeof(old), "%s.1", g_gw_log_path);
        rename(g_gw_log_path, old);
    }

    g_gw_log_fp = fopen(g_gw_log_path, "a");
}

static void gw_log_close(void) {
    if (g_gw_log_fp) { fclose(g_gw_log_fp); g_gw_log_fp = NULL; }
}

/* ================================================================
 *  P101: Rate limiter (token bucket)
 * ================================================================ */

void gw_rate_limit_init(int idx, double tokens_per_sec, double max_burst) {
    if (idx < 0 || idx >= GW_MAX_PLATFORMS) return;
    g_gw.rate_limiters[idx].tokens_per_sec = tokens_per_sec;
    g_gw.rate_limiters[idx].max_tokens = max_burst;
    g_gw.rate_limiters[idx].tokens = max_burst;
    g_gw.rate_limiters[idx].last_refill = gw_mono_time();
}

bool gw_rate_limit_check(int idx) {
    if (idx < 0 || idx >= GW_MAX_PLATFORMS) return true; /* no limit if out of range */

    gw_rate_limiter_t *rl = &g_gw.rate_limiters[idx];
    double now = gw_mono_time();

    /* Refill tokens based on elapsed time */
    double elapsed = now - rl->last_refill;
    rl->tokens += elapsed * rl->tokens_per_sec;
    if (rl->tokens > rl->max_tokens)
        rl->tokens = rl->max_tokens;
    rl->last_refill = now;

    if (rl->tokens >= 1.0) {
        rl->tokens -= 1.0;
        return true; /* allowed */
    }
    return false; /* rate-limited */
}

/* ================================================================
 *  P101: HTTP connection pool
 * ================================================================ */

http_client_t *gw_pool_get_client(const char *endpoint) {
    pthread_mutex_lock(&g_gw.pool_mutex);

    /* Look for an idle client with matching endpoint */
    for (int i = 0; i < g_gw.pool_count; i++) {
        if (!g_gw.http_pool[i].in_use &&
            strcmp(g_gw.http_pool[i].endpoint, endpoint) == 0) {
            g_gw.http_pool[i].in_use = true;
            pthread_mutex_unlock(&g_gw.pool_mutex);
            return g_gw.http_pool[i].client;
        }
    }

    /* Create new client if pool not full */
    if (g_gw.pool_count < GW_POOL_MAX) {
        int i = g_gw.pool_count++;
        g_gw.http_pool[i].client = http_client_new(30);
        g_gw.http_pool[i].in_use = true;
        snprintf(g_gw.http_pool[i].endpoint, sizeof(g_gw.http_pool[i].endpoint), "%s", endpoint ? endpoint : "");
        g_gw.http_pool[i].last_used = gw_mono_time();
        pthread_mutex_unlock(&g_gw.pool_mutex);
        return g_gw.http_pool[i].client;
    }

    /* Pool full — return NULL, caller should create one-off */
    pthread_mutex_unlock(&g_gw.pool_mutex);
    return http_client_new(30);
}

void gw_pool_return_client(http_client_t *client, const char *endpoint) {
    if (!client) return;

    pthread_mutex_lock(&g_gw.pool_mutex);

    for (int i = 0; i < g_gw.pool_count; i++) {
        if (g_gw.http_pool[i].client == client) {
            g_gw.http_pool[i].in_use = false;
            g_gw.http_pool[i].last_used = gw_mono_time();
            pthread_mutex_unlock(&g_gw.pool_mutex);
            return;
        }
    }

    /* Not found in pool — free it */
    pthread_mutex_unlock(&g_gw.pool_mutex);
    http_client_free(client);
}

void gw_pool_cleanup(void) {
    pthread_mutex_lock(&g_gw.pool_mutex);
    double now = gw_mono_time();
    for (int i = 0; i < g_gw.pool_count; i++) {
        if (!g_gw.http_pool[i].in_use &&
            (now - g_gw.http_pool[i].last_used) > 300.0) {
            http_client_free(g_gw.http_pool[i].client);
            if (i < g_gw.pool_count - 1) {
                g_gw.http_pool[i] = g_gw.http_pool[g_gw.pool_count - 1];
            }
            g_gw.pool_count--;
            i--;
        }
    }
    pthread_mutex_unlock(&g_gw.pool_mutex);
}

/* ================================================================
 *  E27: HTTP keepalive per-platform (set via config)
 * ================================================================ */

void gw_set_keepalive(int plat_idx, double keepalive_sec) {
    if (plat_idx >= 0 && plat_idx < GW_MAX_PLATFORMS)
        g_gw.platform_keepalive_sec[plat_idx] = keepalive_sec;
}

/* E28: Message deduplication (TTL-based ring buffer) */
/* Forward declaration for process_update (defined below) */
static void process_update(const char *platform, const char *chat_id, const char *text);

bool gw_dedup_check(const char *message_id) {
    if (!message_id || !*message_id) return false;
    double now = gw_mono_time();

    /* Prune expired entries */
    while (g_gw.dedup_count > 0 &&
           (now - g_gw.dedup_timestamps[g_gw.dedup_head]) > g_gw.dedup_ttl) {
        g_gw.dedup_head = (g_gw.dedup_head + 1) % 64;
        g_gw.dedup_count--;
    }

    /* Linear scan for match (small ring, <64 entries) */
    for (int i = 0; i < g_gw.dedup_count; i++) {
        int idx = (g_gw.dedup_head + i) % 64;
        if (strcmp(g_gw.dedup_ids[idx], message_id) == 0)
            return true; /* duplicate */
    }
    return false;
}

void gw_dedup_add(const char *message_id) {
    if (!message_id || !*message_id) return;
    if (g_gw.dedup_count >= 64) return; /* ring full, skip */

    int idx = (g_gw.dedup_head + g_gw.dedup_count) % 64;
    snprintf(g_gw.dedup_ids[idx], sizeof(g_gw.dedup_ids[idx]), "%s", message_id);
    g_gw.dedup_timestamps[idx] = gw_mono_time();
    g_gw.dedup_count++;
}

/* ================================================================
 *  E29: Batch aggregation — coalesce fragmented messages
 * ================================================================ */

void gw_batch_accumulate(const char *platform, const char *chat_id, const char *fragment) {
    if (!platform || !chat_id || !fragment) return;

    double now = gw_mono_time();
    double BATCH_TIMEOUT = 2.0; /* seconds to wait for more fragments */

    /* If no active batch or different source, flush first */
    if (g_gw.batch_active &&
        (strcmp(g_gw.batch_platform, platform) != 0 ||
         strcmp(g_gw.batch_chat_id, chat_id) != 0 ||
         (now - g_gw.batch_start_time) > BATCH_TIMEOUT)) {
        gw_batch_flush();
    }

    /* Start or continue batch */
    if (!g_gw.batch_active) {
        snprintf(g_gw.batch_platform, sizeof(g_gw.batch_platform), "%s", platform);
        snprintf(g_gw.batch_chat_id, sizeof(g_gw.batch_chat_id), "%s", chat_id);
        g_gw.batch_buf[0] = '\0';
        g_gw.batch_start_time = now;
        g_gw.batch_active = true;
    }

    size_t remaining = sizeof(g_gw.batch_buf) - strlen(g_gw.batch_buf) - 1;
    if (remaining > 0) {
        strncat(g_gw.batch_buf, fragment, remaining);
    }
}

void gw_batch_flush(void) {
    if (!g_gw.batch_active) return;
    if (g_gw.batch_buf[0]) {
        process_update(g_gw.batch_platform, g_gw.batch_chat_id, g_gw.batch_buf);
    }
    g_gw.batch_buf[0] = '\0';
    g_gw.batch_active = false;
}

/* ================================================================
 *  E30: Markdown stripping per-platform
 * ================================================================ */

static char *gw_strip_markdown(const char *text, bool strip_code, bool strip_bold,
                                bool strip_italic) {
    if (!text) return NULL;
    /* Simple in-place markdown stripping. Allocates for worst case. */
    char *out = (char *)malloc(strlen(text) + 1);
    if (!out) return NULL;
    int j = 0;
    for (int i = 0; text[i]; i++) {
        if (text[i] == '`' && strip_code) continue;
        if (text[i] == '*' && strip_bold) {
            /* Skip ** */
            if (text[i+1] == '*') i++;
            continue;
        }
        if (text[i] == '_' && strip_italic) continue;
        if (text[i] == '~' && text[i+1] == '~') { i++; continue; } /* strikethrough ~~ */
        if (text[i] == '#' && (i == 0 || text[i-1] == '\n')) continue; /* headers */
        if (text[i] == '>') { /* block quotes */
            if (i == 0 || text[i-1] == '\n') continue;
        }
        out[j++] = text[i];
    }
    out[j] = '\0';
    return out;
}

/* ================================================================
 *  E31: Per-platform cooldown
 * ================================================================ */

double gw_cooldown_remaining(int plat_idx) {
    if (plat_idx < 0 || plat_idx >= GW_MAX_PLATFORMS) return 0.0;
    double remaining = g_gw.platform_cooldown_sec[plat_idx] -
        (gw_mono_time() - g_gw.platform_last_action[plat_idx]);
    return remaining > 0.0 ? remaining : 0.0;
}

void gw_cooldown_mark(int plat_idx) {
    if (plat_idx >= 0 && plat_idx < GW_MAX_PLATFORMS)
        g_gw.platform_last_action[plat_idx] = gw_mono_time();
}

/* ================================================================
 *  E32: Reconnect backoff (exponential with jitter)
 * ================================================================ */

double gw_reconnect_delay(int plat_idx) {
    if (plat_idx < 0 || plat_idx >= GW_MAX_PLATFORMS) return GW_RECONNECT_BASE_SEC;

    g_gw.reconnect_attempt[plat_idx]++;

    /* Exponential: base * 2 ^ (attempt - 1) with jitter */
    double base = GW_RECONNECT_BASE_SEC *
        (1 << (g_gw.reconnect_attempt[plat_idx] - 1));
    if (base > GW_RECONNECT_MAX_SEC) base = GW_RECONNECT_MAX_SEC;

    /* Add random jitter ±10% */
    double jitter = ((double)rand() / RAND_MAX) * 2.0 * GW_RECONNECT_JITTER * base
        - GW_RECONNECT_JITTER * base;
    double delay = base + jitter;
    if (delay < GW_RECONNECT_BASE_SEC) delay = GW_RECONNECT_BASE_SEC;

    g_gw.reconnect_delay_sec[plat_idx] = delay;
    return delay;
}

void gw_reconnect_reset(int plat_idx) {
    if (plat_idx >= 0 && plat_idx < GW_MAX_PLATFORMS) {
        g_gw.reconnect_attempt[plat_idx] = 0;
        g_gw.reconnect_delay_sec[plat_idx] = 0.0;
    }
}

/* ================================================================
 *  E33: Proxy support per-platform
 * ================================================================ */

bool gw_set_proxy(int plat_idx, const char *proxy_url) {
    if (plat_idx < 0 || plat_idx >= GW_MAX_PLATFORMS) return false;
    if (!proxy_url || !*proxy_url) {
        g_gw.proxy_enabled[plat_idx] = false;
        g_gw.platform_proxy[plat_idx][0] = '\0';
        return true;
    }
    snprintf(g_gw.platform_proxy[plat_idx], sizeof(g_gw.platform_proxy[plat_idx]),
             "%s", proxy_url);
    g_gw.proxy_enabled[plat_idx] = true;
    return true;
}

/* ================================================================
 *  E34: Group observe — observe unmentioned group messages
 * ================================================================ */

/* Forward declarations for functions defined later */
static void gateway_send(const char *platform, const char *target, const char *text);
static void gateway_send_fallback(const char *platform, const char *target,
                                   const char *text);

void gw_set_group_observe(const char *prefix, bool enabled) {
    if (prefix)
        snprintf(g_gw.group_observe_prefix, sizeof(g_gw.group_observe_prefix), "%s", prefix);
    g_gw.group_observe_enabled = enabled;
}

/* L08: Append message to observe buffer (thread-safe, rolling). */
void gw_observe_append(const char *platform, const char *chat_id, const char *text) {
    if (!platform || !chat_id || !text || !*text) return;
    pthread_mutex_lock(&g_gw.observe_mutex);
    size_t cur = strlen(g_gw.observe_buffer);
    size_t add = strlen(platform) + 1 + strlen(chat_id) + 2 + strlen(text) + 3;
    if (cur + add >= sizeof(g_gw.observe_buffer)) {
        /* Buffer full — trim from front */
        char *nl = strchr(g_gw.observe_buffer, '\n');
        if (nl) {
            size_t remain = strlen(nl + 1);
            memmove(g_gw.observe_buffer, nl + 1, remain + 1);
            cur = remain;
        } else {
            g_gw.observe_buffer[0] = '\0';
            cur = 0;
        }
    }
    char entry[2048];
    snprintf(entry, sizeof(entry), "[%s:%s] %s\n", platform, chat_id, text);
    strncat(g_gw.observe_buffer, entry,
            sizeof(g_gw.observe_buffer) - strlen(g_gw.observe_buffer) - 1);
    pthread_mutex_unlock(&g_gw.observe_mutex);
}

/* L08: Consume and clear observe buffer for a given platform+chat. */
char *gw_observe_consume(const char *platform, const char *chat_id) {
    if (!platform || !chat_id) return NULL;
    pthread_mutex_lock(&g_gw.observe_mutex);
    if (g_gw.observe_buffer[0] == '\0') {
        pthread_mutex_unlock(&g_gw.observe_mutex);
        return NULL;
    }
    char *result = strdup(g_gw.observe_buffer);
    g_gw.observe_buffer[0] = '\0';
    pthread_mutex_unlock(&g_gw.observe_mutex);
    return result;
}

/* ================================================================
 *  E35-E39: Gateway hooks/middleware system
 * ================================================================ */

/* Hook function types */
typedef json_node_t *(*gw_hook_t)(json_node_t *data, void *userdata);

#define GW_HOOKS_MAX 16

static struct {
    gw_hook_t pre_send[GW_HOOKS_MAX];      /* E35: transform outgoing messages */
    void     *pre_send_data[GW_HOOKS_MAX];
    int       pre_send_count;

    gw_hook_t post_receive[GW_HOOKS_MAX];  /* E36: process incoming */
    void     *post_receive_data[GW_HOOKS_MAX];
    int       post_receive_count;

    gw_hook_t interceptor[GW_HOOKS_MAX];   /* E37: censor/modify in transit */
    void     *interceptor_data[GW_HOOKS_MAX];
    int       interceptor_count;
} gw_hooks;

void gw_register_pre_send(gw_hook_t hook, void *userdata) {
    if (gw_hooks.pre_send_count >= GW_HOOKS_MAX) return;
    gw_hooks.pre_send[gw_hooks.pre_send_count] = hook;
    gw_hooks.pre_send_data[gw_hooks.pre_send_count] = userdata;
    gw_hooks.pre_send_count++;
}

void gw_register_post_receive(gw_hook_t hook, void *userdata) {
    if (gw_hooks.post_receive_count >= GW_HOOKS_MAX) return;
    gw_hooks.post_receive[gw_hooks.post_receive_count] = hook;
    gw_hooks.post_receive_data[gw_hooks.post_receive_count] = userdata;
    gw_hooks.post_receive_count++;
}

void gw_register_interceptor(gw_hook_t hook, void *userdata) {
    if (gw_hooks.interceptor_count >= GW_HOOKS_MAX) return;
    gw_hooks.interceptor[gw_hooks.interceptor_count] = hook;
    gw_hooks.interceptor_data[gw_hooks.interceptor_count] = userdata;
    gw_hooks.interceptor_count++;
}

/* E38: Event bus — broadcast a JSON event to all registered listeners */
#define GW_EVENT_LISTENERS_MAX 16

typedef void (*gw_event_listener_t)(const char *event_type, json_node_t *data, void *userdata);

static struct {
    gw_event_listener_t listeners[GW_EVENT_LISTENERS_MAX];
    void               *data[GW_EVENT_LISTENERS_MAX];
    int                 count;
} gw_event_bus;

void gw_event_register(gw_event_listener_t listener, void *userdata) {
    if (gw_event_bus.count >= GW_EVENT_LISTENERS_MAX) return;
    gw_event_bus.listeners[gw_event_bus.count] = listener;
    gw_event_bus.data[gw_event_bus.count] = userdata;
    gw_event_bus.count++;
}

void gw_event_emit(const char *event_type, json_node_t *data) {
    for (int i = 0; i < gw_event_bus.count; i++) {
        gw_event_bus.listeners[i](event_type, data, gw_event_bus.data[i]);
    }
}

/* E35: Apply pre-send hooks to a message before sending */
static char *gw_apply_pre_send_hooks(const char *platform, const char *text) {
    if (!text) return NULL;

    json_node_t *data = json_new_object();
    json_object_set(data, "platform", json_new_string(platform));
    json_object_set(data, "text", json_new_string(text));

    for (int i = 0; i < gw_hooks.pre_send_count; i++) {
        json_node_t *result = gw_hooks.pre_send[i](data, gw_hooks.pre_send_data[i]);
        if (result) {
            const char *new_text = json_object_get_string(result, "text", NULL);
            if (new_text) {
                json_object_set(data, "text", json_new_string(new_text));
            }
            json_free(result);
        }
    }

    const char *final_text = json_object_get_string(data, "text", "");
    char *out = strdup(final_text);
    json_free(data);
    return out;
}

/* E36: Apply post-receive hooks on incoming message */
static char *gw_apply_post_receive_hooks(const char *platform, const char *chat_id,
                                          const char *text) {
    if (!text) return NULL;

    json_node_t *data = json_new_object();
    json_object_set(data, "platform", json_new_string(platform));
    json_object_set(data, "chat_id", json_new_string(chat_id));
    json_object_set(data, "text", json_new_string(text));

    for (int i = 0; i < gw_hooks.post_receive_count; i++) {
        json_node_t *result = gw_hooks.post_receive[i](data, gw_hooks.post_receive_data[i]);
        if (result) {
            const char *new_text = json_object_get_string(result, "text", NULL);
            if (new_text)
                json_object_set(data, "text", json_new_string(new_text));
            json_free(result);
        }
    }

    const char *final_text = json_object_get_string(data, "text", "");
    char *out = strdup(final_text);
    json_free(data);
    return out;
}

/* E37: Apply interceptors — can return NULL to drop message */
static char *gw_apply_interceptors(const char *platform, const char *chat_id,
                                    const char *text) {
    if (!text) return NULL;

    json_node_t *data = json_new_object();
    json_object_set(data, "platform", json_new_string(platform));
    json_object_set(data, "chat_id", json_new_string(chat_id));
    json_object_set(data, "text", json_new_string(text));

    for (int i = 0; i < gw_hooks.interceptor_count; i++) {
        json_node_t *result = gw_hooks.interceptor[i](data, gw_hooks.interceptor_data[i]);
        if (!result) {
            /* Interceptor dropped the message */
            json_free(data);
            return NULL;
        }
        const char *new_text = json_object_get_string(result, "text", NULL);
        if (new_text)
            json_object_set(data, "text", json_new_string(new_text));
        json_free(result);
    }

    const char *final_text = json_object_get_string(data, "text", "");
    char *out = strdup(final_text);
    json_free(data);
    return out;
}

/* E39: Cooldown manager — enforce min interval between sends */
static bool gw_cooldown_allow(int plat_idx) {
    if (plat_idx < 0 || plat_idx >= GW_MAX_PLATFORMS) return true;
    double remaining = gw_cooldown_remaining(plat_idx);
    if (remaining > 0.0) return false;
    gw_cooldown_mark(plat_idx);
    return true;
}

/* ================================================================
 *  E40-E43: Gateway message formatting
 * ================================================================ */

/* E40: Convert markdown to HTML for platforms that support it.
 * Simple conversion: **bold** → <b>bold</b>, *italic* → <i>italic</i>,
 * `code` → <code>code</code> */
char *gw_markdown_to_html(const char *text) {
    if (!text) return NULL;
    char *out = (char *)malloc(strlen(text) * 2 + 1);
    if (!out) return NULL;
    int j = 0;
    for (int i = 0; text[i]; i++) {
        if (text[i] == '*' && text[i+1] == '*') {
            out[j++] = '<'; out[j++] = 'b'; out[j++] = '>';
            i++;
            while (text[i+1] && !(text[i+1] == '*' && text[i+2] == '*')) {
                out[j++] = text[++i];
            }
            out[j++] = '<'; out[j++] = '/'; out[j++] = 'b'; out[j++] = '>';
            i += 2;
        } else if (text[i] == '*' && text[i+1] != '*') {
            out[j++] = '<'; out[j++] = 'i'; out[j++] = '>';
            i++;
            while (text[i] && text[i] != '*') {
                out[j++] = text[i++];
            }
            out[j++] = '<'; out[j++] = '/'; out[j++] = 'i'; out[j++] = '>';
        } else if (text[i] == '`') {
            out[j++] = '<'; out[j++] = 'c'; out[j++] = 'o'; out[j++] = 'd';
            out[j++] = 'e'; out[j++] = '>';
            i++;
            while (text[i] && text[i] != '`') {
                if (text[i] == '\\' && text[i+1] == '`') i++;
                out[j++] = text[i++];
            }
            out[j++] = '<'; out[j++] = '/'; out[j++] = 'c'; out[j++] = 'o';
            out[j++] = 'd'; out[j++] = 'e'; out[j++] = '>';
        } else {
            /* Escape HTML entities */
            if (text[i] == '<') { out[j++] = '&'; out[j++] = 'l'; out[j++] = 't'; out[j++] = ';'; }
            else if (text[i] == '>') { out[j++] = '&'; out[j++] = 'g'; out[j++] = 't'; out[j++] = ';'; }
            else if (text[i] == '&') { out[j++] = '&'; out[j++] = 'a'; out[j++] = 'm'; out[j++] = 'p'; out[j++] = ';'; }
            else out[j++] = text[i];
        }
    }
    out[j] = '\0';
    return out;
}

/* E41: Telegram MarkdownV2 escaping — escape reserved chars */
char *gw_markdown_v2_escape(const char *text) {
    if (!text) return NULL;
    char *out = (char *)malloc(strlen(text) * 2 + 1);
    if (!out) return NULL;
    int j = 0;
    for (int i = 0; text[i]; i++) {
        /* Characters that need escaping in MarkdownV2: _ * [ ] ( ) ~ ` > # + - = | { } . ! */
        if (strchr("_*[]()~`>#+-=|{}.!", text[i])) {
            out[j++] = '\\';
        }
        out[j++] = text[i];
    }
    out[j] = '\0';
    return out;
}

/* E42: Strip all formatting for plain text platforms */
static char *gw_strip_all_formatting(const char *text) {
    return gw_strip_markdown(text, true, true, true);
}

/* E43: Smart message truncation with ellipsis.
 * Truncates at word boundary if possible. */
char *gw_truncate_message(const char *text, size_t max_len) {
    if (!text || max_len == 0) return NULL;
    size_t len = strlen(text);
    if (len <= max_len) return strdup(text);

    char *out = (char *)malloc(max_len + 4);
    if (!out) return NULL;
    memcpy(out, text, max_len);

    /* Try to break at word boundary (space) */
    int break_at = (int)max_len;
    while (break_at > 0 && out[break_at - 1] != ' ') break_at--;

    if (break_at > (int)max_len / 2) {
        out[break_at] = '\0';
        strcat(out, "...");
    } else {
        out[max_len] = '\0';
        strcat(out, "...");
    }
    return out;
}

/* ================================================================
 *  E44-E47: Gateway error handling
 * ================================================================ */

/* E44: Retry an API call with exponential backoff on 429/5xx.
 * Returns true if at least one attempt succeeded. */
bool gw_retry_with_backoff(bool (*api_call)(void *ctx), void *ctx,
                                   int max_retries, int base_delay_ms) {
    for (int attempt = 0; attempt <= max_retries; attempt++) {
        if (api_call(ctx)) return true;
        if (attempt < max_retries) {
            int delay = base_delay_ms * (1 << attempt); /* exponential */
            /* Add jitter ±20% */
            delay += (int)(((double)rand() / RAND_MAX) * 2.0 * 0.2 * delay - 0.2 * delay);
            usleep(delay * 1000);
        }
    }
    return false;
}

/* E45: Token refresh — re-init platform when token expires.
 * Checks platform state and re-runs setup. */
bool gw_refresh_token(int plat_idx) {
    if (plat_idx < 0 || plat_idx >= GW_MAX_PLATFORMS) return false;
    /* Re-initialize the platform's HTTP client */
    if (g_gw.platform_http[plat_idx]) {
        http_client_free(g_gw.platform_http[plat_idx]);
    }
    g_gw.platform_http[plat_idx] = http_client_new(30);
    /* Apply proxy if configured */
    if (g_gw.proxy_enabled[plat_idx] && g_gw.platform_proxy[plat_idx][0]) {
        http_client_set_proxy(g_gw.platform_http[plat_idx],
                              g_gw.platform_proxy[plat_idx]);
    }
    gw_reconnect_reset(plat_idx);
    return true;
}

/* E47: Send a plain text fallback when rich formatting fails */
static void gateway_send_fallback(const char *platform, const char *target,
                                   const char *text) {
    if (!platform || !target || !text) return;
    /* Strip all formatting and truncate */
    char *plain = gw_strip_all_formatting(text);
    char *truncated = gw_truncate_message(plain ? plain : text, 4000);
    if (truncated) {
        gateway_send(platform, target, truncated);
        free(truncated);
    }
    free(plain);
}

/* ================================================================
 *  Thread-safe agent chat
 * ================================================================ */

char *gateway_agent_chat(const char *message) {
    pthread_mutex_lock(&g_gw.agent_mutex);
    char *resp = agent_chat(&g_gw.agent, message);
    pthread_mutex_unlock(&g_gw.agent_mutex);
    return resp;
}

/* ================================================================
 *  P102: Per-chat session management
 * ================================================================ */

/* Build session key: "platform:chat_id" */
static void session_build_key(char *buf, size_t sz,
                               const char *platform, const char *chat_id) {
    snprintf(buf, sz, "%s:%s", platform ? platform : "?", chat_id ? chat_id : "?");
}

/* Find existing session entry by platform+chat_id. Returns index or -1. */
static int session_find(const char *platform, const char *chat_id) {
    char key[192];
    session_build_key(key, sizeof(key), platform, chat_id);
    for (int i = 0; i < g_gw.session_count; i++) {
        if (strcmp(g_gw.sessions[i].key, key) == 0 && g_gw.sessions[i].in_use)
            return i;
    }
    return -1;
}

/* Create a new session for a platform:chat_id pair. Returns index or -1. */
static int session_create(const char *platform, const char *chat_id) {
    if (g_gw.session_count >= GW_SESSIONS_MAX) {
        /* Evict oldest inactive session */
        int oldest = -1;
        double oldest_time = 1e18;
        for (int i = 0; i < g_gw.session_count; i++) {
            if (g_gw.sessions[i].last_active < oldest_time) {
                oldest_time = g_gw.sessions[i].last_active;
                oldest = i;
            }
        }
        if (oldest < 0) return -1;
        /* Save and free */
        if (g_gw.sessions[oldest].db)
            agent_save_session(&g_gw.sessions[oldest].agent);
        agent_free(&g_gw.sessions[oldest].agent);
        g_gw.sessions[oldest].in_use = false;
    }

    int idx = -1;
    for (int i = 0; i < GW_SESSIONS_MAX; i++) {
        if (!g_gw.sessions[i].in_use) {
            idx = i;
            break;
        }
    }
    if (idx < 0) idx = g_gw.session_count; /* fallback: use next slot */

    gw_session_entry_t *se = &g_gw.sessions[idx];
    memset(se, 0, sizeof(*se));
    session_build_key(se->key, sizeof(se->key), platform, chat_id);
    se->in_use = true;
    se->last_active = gw_mono_time();

    /* Initialize agent */
    agent_init(&se->agent);

    /* Copy config from main agent */
    memcpy(&se->agent.llm, &g_gw.agent.llm, sizeof(se->agent.llm));
    se->agent.max_iterations = g_gw.agent.max_iterations;
    se->agent.compress_enabled = g_gw.agent.compress_enabled;

    /* Open session DB (persistent) */
    if (g_gw.session_db_path[0]) {
        se->db = db_open(g_gw.session_db_path, NULL);
    }

    if (idx >= g_gw.session_count)
        g_gw.session_count = idx + 1;

    return idx;
}

/* Get or create a session for platform:chat_id. Returns index or -1. */
static int session_get_or_create(const char *platform, const char *chat_id) {
    int idx = session_find(platform, chat_id);
    if (idx >= 0) {
        g_gw.sessions[idx].last_active = gw_mono_time();
        return idx;
    }
    return session_create(platform, chat_id);
}

/* Auto-save all active sessions */
static void session_save_all(void) {
    for (int i = 0; i < g_gw.session_count; i++) {
        if (g_gw.sessions[i].in_use && g_gw.sessions[i].db) {
            agent_save_session(&g_gw.sessions[i].agent);
        }
    }
}

/* Clean up idle sessions (last used > 30 min) */
static void session_cleanup_idle(void) {
    double now = gw_mono_time();
    for (int i = 0; i < g_gw.session_count; i++) {
        if (g_gw.sessions[i].in_use &&
            (now - g_gw.sessions[i].last_active) > 1800.0) {
            /* Save and free */
            if (g_gw.sessions[i].db) {
                db_save(g_gw.sessions[i].db, g_gw.sessions[i].session_id, NULL);
                db_close(g_gw.sessions[i].db);
            }
            agent_free(&g_gw.sessions[i].agent);
            memset(&g_gw.sessions[i], 0, sizeof(g_gw.sessions[i]));
        }
    }
}

/* ================================================================
 *  Platform-aware message send
 * ================================================================ */

static void gateway_send(const char *platform, const char *target, const char *text) {
    if (!platform || !target || !text) return;

    /* P103: Try registered platform interface first */
    if (gw_platform_send(platform, target, text))
        return;

    /* Legacy fallback for unregistered platforms */
    if (strcmp(platform, "telegram") == 0) {
        size_t len = strlen(text);
        if (len > 4000) {
            char chunk[4001];
            memcpy(chunk, text, 4000);
            chunk[4000] = '\0';
            telegram_send_message(g_gw.http, target, chunk, "Markdown");
            if (len > 4000)
                telegram_send_message(g_gw.http, target, text + 4000, "Markdown");
        } else {
            telegram_send_message(g_gw.http, target, text, "Markdown");
        }
    } else if (strcmp(platform, "discord") == 0) {
        discord_send_message(g_gw.http, text);
    } else if (strcmp(platform, "mattermost") == 0) {
        mattermost_send_message(g_gw.http, text);
    }
    /* WhatsApp sends via webhook handler, not gateway_send */
}

static void gateway_send_typing(const char *platform, const char *target) {
    if (!platform) return;

    /* P103: Try registered platform interface first */
    gw_platform_send_typing(platform, target);

    /* Legacy fallback */
    if (strcmp(platform, "telegram") == 0)
        telegram_send_chat_action(g_gw.http, target, "typing");
    else if (strcmp(platform, "discord") == 0)
        discord_send_typing(g_gw.http);
}

/* ================================================================
 *  P103: Platform interface implementation
 * ================================================================ */

void gw_platform_register(const gw_platform_t *plat) {
    if (!plat || !plat->name) return;
    if (g_gw.platform_def_count >= GW_MAX_PLATFORMS) return;
    g_gw.platform_defs[g_gw.platform_def_count++] = *plat;
}

static gw_platform_t *gw_platform_find(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < g_gw.platform_def_count; i++) {
        if (strcasecmp(g_gw.platform_defs[i].name, name) == 0)
            return &g_gw.platform_defs[i];
    }
    return NULL;
}

bool gw_platform_send(const char *platform_name, const char *chat_id, const char *text) {
    gw_platform_t *p = gw_platform_find(platform_name);
    if (!p || !p->send) return false;
    return p->send(chat_id, text);
}

void gw_platform_send_typing(const char *platform_name, const char *chat_id) {
    gw_platform_t *p = gw_platform_find(platform_name);
    if (p && p->send_typing)
        p->send_typing(chat_id);
}

void gw_platform_shutdown_all(void) {
    for (int i = 0; i < g_gw.platform_def_count; i++) {
        if (g_gw.platform_defs[i].shutdown)
            g_gw.platform_defs[i].shutdown();
    }
}

/* ================================================================
 *  Process a single update (called from platform threads)
 * ================================================================ */

static void process_update(const char *platform, const char *chat_id, const char *text) {
    if (!platform || !chat_id || !text || !*text) return;

    printf("[gateway:%s] Message: %s\n", platform, text);

    /* L08: Prepend any accumulated observe buffer before processing
     * a triggered message (one where the bot IS mentioned). */
    char *observe_ctx = gw_observe_consume(platform, chat_id);
    const char *actual_text = text;
    char combined[66560]; /* 65536 observe buf + 1024 message */
    if (observe_ctx) {
        snprintf(combined, sizeof(combined),
                 "[Observed context from this chat]\n%s\n[Trigger message]\n%s",
                 observe_ctx, text);
        actual_text = combined;
        free(observe_ctx);
    }

    /* P101: Find platform index for rate limiting */
    int plat_idx = -1;
    for (int i = 0; i < g_gw.platform_count; i++) {
        if (strcasecmp(g_gw.platforms[i], platform) == 0) {
            plat_idx = i;
            break;
        }
    }

    /* P101: Check rate limit — if exceeded, queue the message */
    if (plat_idx >= 0 && !gw_rate_limit_check(plat_idx)) {
        gw_queue_push(platform, chat_id, text, NULL);
        printf("[gateway:%s] Rate limited, queued\n", platform);
        return;
    }

    /* P102: Get or create per-chat session */
    pthread_mutex_lock(&g_gw.session_mutex);
    int sess_idx = session_get_or_create(platform, chat_id);
    if (sess_idx < 0) {
        pthread_mutex_unlock(&g_gw.session_mutex);
        gateway_send(platform, chat_id,
                     "Error: Could not create session (max sessions reached)");
        return;
    }
    agent_state_t *session_agent = &g_gw.sessions[sess_idx].agent;
    pthread_mutex_unlock(&g_gw.session_mutex);

    /* Send typing indicator */
    gateway_send_typing(platform, chat_id);

    /* Run agent on per-chat session */
    char *resp = agent_chat(session_agent, text);
    if (resp) {
        gateway_send(platform, chat_id, resp);
        free(resp);
    }
}

/* ================================================================
 *  Per-platform thread functions
 * ================================================================ */

static void *thread_poll_telegram(void *arg) {
    (void)arg;
    printf("[gateway] Telegram polling (interval: %ds)\n", g_gw.poll_interval);

    /* L08: Fetch bot identity on first run for @mention detection */
    telegram_get_me(g_gw.http);
    if (telegram_get_username()[0])
        printf("[gateway] Telegram bot: @%s\n", telegram_get_username());

    while (g_gw.running) {
        json_node_t *root = telegram_get_updates(g_gw.http, g_gw.tg_offset, 30);

        if (root) {
            json_node_t *result = json_obj_get(root, "result");
            if (result && json_len(result) > 0) {
                size_t n = json_len(result);
                for (size_t i = 0; i < n; i++) {
                    json_node_t *update = json_get(result, i);
                    double update_id = json_get_num(update, "update_id", 0);
                    if (update_id > 0)
                        g_gw.tg_offset = (int)update_id + 1;

                    const char *chat_id = telegram_get_chat_id(update);
                    const char *text = telegram_get_text(update);
                    if (!chat_id || !text) continue;

                    /* L08: Group observe — silently accumulate unmentioned group messages */
                    if (g_gw.group_observe_enabled &&
                        telegram_is_group(update) &&
                        !telegram_is_mentioned(update)) {
                        gw_observe_append("telegram", chat_id, text);
                        printf("[gateway] Observed (no @mention): %s\n", text);
                        continue;
                    }

                    process_update("telegram", chat_id, text);
                }
            }
            json_free(root);
        }

        if (g_gw.running)
            sleep(g_gw.poll_interval);
    }
    return NULL;
}

static void *thread_poll_discord(void *arg) {
    (void)arg;
    printf("[gateway] Discord polling (interval: %ds)\n", g_gw.poll_interval);

    while (g_gw.running) {
        json_node_t *updates = discord_poll_messages(g_gw.http);
        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update("discord",
                               discord_get_chat_id(update),
                               discord_get_text(update));
            }
        }
        json_free(updates);
        if (g_gw.running) sleep(g_gw.poll_interval);
    }
    return NULL;
}

static void *thread_poll_slack(void *arg) {
    (void)arg;
    printf("[gateway] Slack polling (interval: %ds)\n", g_gw.poll_interval);

    while (g_gw.running) {
        json_node_t *updates = slack_poll_messages(g_gw.http);
        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update("slack",
                               slack_get_chat_id(update),
                               slack_get_text(update));
            }
        }
        json_free(updates);
        if (g_gw.running) sleep(g_gw.poll_interval);
    }
    return NULL;
}

static void *thread_poll_matrix(void *arg) {
    (void)arg;
    printf("[gateway] Matrix polling (interval: %ds)\n", g_gw.poll_interval);

    while (g_gw.running) {
        json_node_t *updates = matrix_poll_messages(g_gw.http);
        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update("matrix",
                               matrix_get_chat_id(update),
                               matrix_get_text(update));
            }
        }
        json_free(updates);
        if (g_gw.running) sleep(g_gw.poll_interval);
    }
    return NULL;
}

static void *thread_poll_mattermost(void *arg) {
    (void)arg;
    printf("[gateway] Mattermost polling (interval: %ds)\n", g_gw.poll_interval);

    while (g_gw.running) {
        json_node_t *updates = mattermost_poll_messages(g_gw.http);
        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update("mattermost",
                               mattermost_get_chat_id(update),
                               mattermost_get_text(update));
            }
        }
        json_free(updates);
        if (g_gw.running) sleep(g_gw.poll_interval);
    }
    return NULL;
}

static void *thread_webhook(void *arg) {
    int port = *(int *)arg;
    printf("[gateway] Webhook HTTP API on port %d\n", port);
    webhook_server_run(port);
    return NULL;
}

/* ================================================================
 *  Signal handler
 * ================================================================ */

static void handle_signal(int sig) {
    (void)sig;
    printf("\n[gateway] Shutting down...\n");
    g_gw.running = false;
}

/* ================================================================
 *  Platform setup helpers
 * ================================================================ */

typedef struct {
    const char *name;
    bool (*setup)(void);
    void *(*thread_fn)(void *);
    int arg_int; /* For port numbers etc. */
} platform_def_t;

static bool setup_telegram(void) {
    const char *token = getenv("TELEGRAM_BOT_TOKEN");
    if (!token) token = getenv("HERMES_TELEGRAM_TOKEN");
    if (!token) { fprintf(stderr, "Warning: TELEGRAM_BOT_TOKEN not set\n"); return false; }
    telegram_set_token(token);
    return true;
}

static bool setup_discord(void) {
    const char *token = getenv("DISCORD_BOT_TOKEN");
    const char *channel = getenv("DISCORD_CHANNEL_ID");
    if (!token || !channel) {
        fprintf(stderr, "Warning: DISCORD_BOT_TOKEN or DISCORD_CHANNEL_ID not set\n");
        return false;
    }
    discord_set_token(token);
    discord_set_channel(channel);
    return true;
}

static bool setup_slack(void) {
    const char *token = getenv("SLACK_BOT_TOKEN");
    const char *channel = getenv("SLACK_CHANNEL_ID");
    if (!token || !channel) {
        fprintf(stderr, "Warning: SLACK_BOT_TOKEN or SLACK_CHANNEL_ID not set\n");
        return false;
    }
    slack_set_token(token);
    slack_set_channel(channel);
    return true;
}

static bool setup_matrix(void) {
    const char *hs = getenv("MATRIX_HOMESERVER");
    const char *token = getenv("MATRIX_ACCESS_TOKEN");
    const char *room = getenv("MATRIX_ROOM_ID");
    if (!token) { fprintf(stderr, "Warning: MATRIX_ACCESS_TOKEN not set\n"); return false; }
    matrix_set_homeserver(hs && hs[0] ? hs : "https://matrix.org");
    matrix_set_token(token);
    if (room) matrix_set_room(room);
    return true;
}

static bool setup_mattermost(void) {
    const char *url = getenv("MATTERMOST_URL");
    const char *token = getenv("MATTERMOST_TOKEN");
    const char *channel = getenv("MATTERMOST_CHANNEL_ID");
    if (!token || !channel) {
        fprintf(stderr, "Warning: MATTERMOST_TOKEN or MATTERMOST_CHANNEL_ID not set\n");
        return false;
    }
    mattermost_set_url(url && url[0] ? url : "http://localhost:8065");
    mattermost_set_token(token);
    mattermost_set_channel(channel);
    return true;
}

static bool setup_webhook(void) {
    /* No token needed */
    return true;
}

static bool setup_whatsapp(void) {
    const char *token = getenv("WHATSAPP_TOKEN");
    const char *phone = getenv("WHATSAPP_PHONE_NUMBER_ID");
    const char *verify = getenv("WHATSAPP_VERIFY_TOKEN");
    if (!token || !phone) {
        fprintf(stderr, "Warning: WHATSAPP_TOKEN or WHATSAPP_PHONE_NUMBER_ID not set\n");
        return false;
    }
    whatsapp_set_token(token);
    whatsapp_set_phone_id(phone);
    if (verify) whatsapp_set_verify_token(verify);
    return true;
}

static bool setup_email(void) {
    const char *from = getenv("EMAIL_FROM");
    if (from) email_set_from(from);

    /* Validate: email needs IMAP (for incoming) or SMTP/sendmail (for outgoing) */
    const char *imap = getenv("EMAIL_IMAP_SERVER");
    const char *smtp = getenv("EMAIL_SMTP_SERVER");
    const char *cmd = getenv("EMAIL_SEND_CMD");
    if (!imap && !smtp && !cmd) {
        fprintf(stderr, "Warning: neither EMAIL_IMAP_SERVER nor EMAIL_SMTP_SERVER nor"
                        " EMAIL_SEND_CMD set. Email will not function.\n");
        return false;
    }
    return true;
}

static bool setup_signal(void) {
    const char *number = getenv("SIGNAL_NUMBER");
    const char *cli_path = getenv("SIGNAL_CLI_PATH");
    if (!number) {
        fprintf(stderr, "Warning: SIGNAL_NUMBER not set\n");
        return false;
    }
    signal_set_number(number);
    if (cli_path) signal_set_cli_path(cli_path);
    return true;
}

/* Email poll thread */
static void *thread_poll_email(void *arg) {
    (void)arg;
    int poll_int = g_gw.poll_interval * 3; /* Email polls less frequently */
    printf("[gateway] Email polling (interval: %ds)\n", poll_int);
    while (g_gw.running) {
        json_node_t *updates = email_poll_messages(g_gw.http);
        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update("email",
                               email_get_chat_id(update),
                               email_get_text(update));
            }
        }
        json_free(updates);
        if (g_gw.running) sleep(poll_int);
    }
    return NULL;
}

/* Signal poll thread */
static void *thread_poll_signal(void *arg) {
    (void)arg;
    /* Check if signal-cli is available */
    if (!signal_check_available()) {
        printf("[gateway] signal-cli not found. Signal platform disabled.\n");
        return NULL;
    }
    printf("[gateway] Signal polling (interval: %ds)\n", g_gw.poll_interval);
    while (g_gw.running) {
        json_node_t *updates = signal_poll_messages(g_gw.http);
        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update("signal",
                               signal_get_chat_id(update),
                               signal_get_text(update));
            }
        }
        json_free(updates);
        if (g_gw.running) sleep(g_gw.poll_interval);
    }
    return NULL;
}

/* HomeAssistant setup + thread */
static bool setup_ha(void) {
    const char *url = getenv("HA_URL");
    const char *token = getenv("HA_TOKEN");
    if (!url || !token) {
        fprintf(stderr, "Warning: HA_URL and HA_TOKEN must be set\n");
        return false;
    }
    ha_set_url(url);
    ha_set_token(token);
    const char *entity = getenv("HA_NOTIFY_ENTITY");
    if (entity) ha_set_notify_entity(entity);
    return true;
}

static void *thread_poll_ha(void *arg) {
    (void)arg;
    printf("[gateway] HomeAssistant polling (interval: %ds)\n", g_gw.poll_interval * 5);
    while (g_gw.running) {
        json_node_t *updates = ha_poll_messages(g_gw.http);
        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update("homeassistant",
                               ha_get_chat_id(update),
                               ha_get_text(update));
            }
        }
        json_free(updates);
        if (g_gw.running) sleep(g_gw.poll_interval * 5);
    }
    return NULL;
}

/* SMS setup + thread */
static bool setup_sms(void) {
    const char *sid = getenv("TWILIO_ACCOUNT_SID");
    const char *token = getenv("TWILIO_AUTH_TOKEN");
    const char *from = getenv("TWILIO_FROM_NUMBER");
    if (!sid || !from) {
        fprintf(stderr, "Warning: TWILIO_ACCOUNT_SID and TWILIO_FROM_NUMBER must be set\n");
        return false;
    }
    sms_set_twilio(sid, token, from);

    /* P111: Optional status callback URL for delivery status */
    const char *cb = getenv("TWILIO_STATUS_CALLBACK");
    if (cb) {
        sms_set_status_callback(cb);
        printf("[gateway] SMS status callbacks configured\n");
    }

    /* P111: Optional webhook path (default /sms-webhook on the webhook server) */
    const char *wh = getenv("TWILIO_WEBHOOK_PATH");
    if (wh) {
        sms_set_webhook_url(wh);
    }
    return true;
}

static void *thread_poll_sms(void *arg) {
    (void)arg;
    printf("[gateway] SMS/Twilio polling (interval: %ds)\n", g_gw.poll_interval * 5);
    while (g_gw.running) {
        json_node_t *updates = sms_poll_messages(g_gw.http);
        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update("sms",
                               sms_get_chat_id(update),
                               sms_get_text(update));
            }
            json_free(updates);
        } else {
            json_free(updates);
        }
        if (g_gw.running) sleep(g_gw.poll_interval * 5);
    }
    return NULL;
}

/* Feishu setup */
static bool setup_feishu(void) {
    const char *url = getenv("FEISHU_WEBHOOK_URL");
    if (!url) {
        fprintf(stderr, "Warning: FEISHU_WEBHOOK_URL not set\n");
        return false;
    }
    feishu_set_webhook(url);
    return true;
}

static void *thread_poll_feishu(void *arg) {
    (void)arg;
    printf("[gateway] Feishu platform (webhook-based). Idle.\n");
    while (g_gw.running) sleep(g_gw.poll_interval * 10);
    return NULL;
}

/* WeCom setup */
static bool setup_wecom(void) {
    const char *url = getenv("WECOM_WEBHOOK_URL");
    if (!url) {
        fprintf(stderr, "Warning: WECOM_WEBHOOK_URL not set\n");
        return false;
    }
    wecom_set_webhook(url);
    return true;
}

static void *thread_poll_wecom(void *arg) {
    (void)arg;
    printf("[gateway] WeCom polling (interval: %ds)\n", g_gw.poll_interval * 5);
    while (g_gw.running) {
        json_node_t *updates = wecom_poll_messages(g_gw.http);
        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update("wecom",
                               wecom_get_chat_id(update),
                               wecom_get_text(update));
            }
            json_free(updates);
        } else {
            json_free(updates);
        }
        if (g_gw.running) sleep(g_gw.poll_interval * 5);
    }
    return NULL;
}

/* DingTalk setup */
static bool setup_dingtalk(void) {
    const char *url = getenv("DINGTALK_WEBHOOK_URL");
    if (!url) {
        fprintf(stderr, "Warning: DINGTALK_WEBHOOK_URL not set\n");
        return false;
    }
    dingtalk_set_webhook(url);
    return true;
}

static void *thread_poll_dingtalk(void *arg) {
    (void)arg;
    printf("[gateway] DingTalk polling (interval: %ds)\n", g_gw.poll_interval * 5);
    while (g_gw.running) {
        json_node_t *updates = dingtalk_poll_messages(g_gw.http);
        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update("dingtalk",
                               dingtalk_get_chat_id(update),
                               dingtalk_get_text(update));
            }
            json_free(updates);
        } else {
            json_free(updates);
        }
        if (g_gw.running) sleep(g_gw.poll_interval * 5);
    }
    return NULL;
}

/* QQ Bot setup */
static bool setup_qqbot(void) {
    const char *url = getenv("QQ_BOT_WEBHOOK_URL");
    const char *token = getenv("QQ_BOT_TOKEN");
    if (!url) {
        fprintf(stderr, "Warning: QQ_BOT_WEBHOOK_URL not set\n");
        return false;
    }
    qqbot_set_webhook(url);
    if (token) qqbot_set_token(token);
    return true;
}

static void *thread_poll_qqbot(void *arg) {
    (void)arg;
    printf("[gateway] QQ Bot polling (interval: %ds)\n", g_gw.poll_interval * 5);
    while (g_gw.running) {
        json_node_t *updates = qqbot_poll_messages(g_gw.http);
        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update("qqbot",
                               qqbot_get_chat_id(update),
                               qqbot_get_text(update));
            }
            json_free(updates);
        } else {
            json_free(updates);
        }
        if (g_gw.running) sleep(g_gw.poll_interval * 5);
    }
    return NULL;
}

/* BlueBubbles setup */
static bool setup_bluebubbles(void) {
    const char *url = getenv("BLUEBUBBLES_URL");
    const char *pwd = getenv("BLUEBUBBLES_PASSWORD");
    if (!url || !pwd) {
        fprintf(stderr, "Warning: BLUEBUBBLES_URL and BLUEBUBBLES_PASSWORD must be set\n");
        return false;
    }
    bluebubbles_set_url(url);
    bluebubbles_set_password(pwd);
    return true;
}

static void *thread_poll_bluebubbles(void *arg) {
    (void)arg;
    printf("[gateway] BlueBubbles platform (iMessage). Polling every %ds.\\n", g_gw.poll_interval);

    /* Check for optional poll GUID env var */
    const char *poll_guid = getenv("BLUEBUBBLES_POLL_GUID");
    if (poll_guid) {
        bluebubbles_set_poll_guid(poll_guid);
        printf("[gateway] BlueBubbles polling chat GUID: %s\\n", poll_guid);
    } else {
        bluebubbles_set_poll_guid(NULL);
        printf("[gateway] BlueBubbles is webhook-driven. Set BLUEBUBBLES_POLL_GUID for polling.\\n");
    }

    while (g_gw.running) {
        json_node_t *updates = bluebubbles_poll_messages(g_gw.http);
        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                const char *chat_id = bluebubbles_get_chat_id(update);
                const char *text = bluebubbles_get_text(update);
                if (chat_id && text && text[0]) {
                    process_update("bluebubbles", chat_id, text);
                }
            }
        }
        json_free(updates);
        if (g_gw.running) sleep(g_gw.poll_interval);
    }
    return NULL;
}

/* ================================================================
 *  Get port from env with HERMES_ or SLERMES_ prefix
 * ================================================================ */

static int get_webhook_port(void) {
    /* 1. Config value (from YAML) takes priority */
    if (g_gw.config.webhook_port > 0 && g_gw.config.webhook_port <= 65535)
        return g_gw.config.webhook_port;
    /* 2. Env vars */
    const char *port_str = getenv("SLERMES_WEBHOOK_PORT");
    if (!port_str) port_str = getenv("HERMES_WEBHOOK_PORT");
    if (!port_str) port_str = getenv("WEBHOOK_PORT");
    int port = port_str ? atoi(port_str) : 8080;
    if (port <= 0 || port > 65535) port = 8080;
    return port;
}

/* ================================================================
 *  Gateway entry point
 * ================================================================ */

static bool setup_msgraph_webhook(void) {
    const char *port_str = getenv("MSGRAPH_WEBHOOK_PORT");
    int port = port_str ? atoi(port_str) : 8646;
    if (port <= 0 || port > 65535) port = 8646;
    msgraph_webhook_init(NULL, NULL, port);
    return true;
}

static void *thread_msgraph_webhook(void *arg) {
    (void)arg;
    msgraph_webhook_run();
    return NULL;
}

/* weixin setup + thread */
extern bool weixin_init(const char *token, const char *account_id);
extern void weixin_start(void);
extern void weixin_stop(void);

static bool setup_weixin(void) {
    const char *token = getenv("WEIXIN_TOKEN");
    const char *account_id = getenv("WEIXIN_ACCOUNT_ID");
    if (!token || !account_id) {
        fprintf(stderr, "Warning: WEIXIN_TOKEN and WEIXIN_ACCOUNT_ID must be set\n");
        return false;
    }
    weixin_init(token, account_id);
    return true;
}

static void *thread_weixin(void *arg) {
    (void)arg;
    weixin_start();
    return NULL;
}

/* yuanbao setup + thread */
extern bool yuanbao_init(const char *app_id, const char *app_secret,
                         const char *bot_id, const char *ws_url,
                         const char *api_domain);
extern void yuanbao_start(void);
extern void yuanbao_stop(void);

static bool setup_yuanbao(void) {
    const char *app_id = getenv("YUANBAO_APP_ID");
    const char *app_secret = getenv("YUANBAO_APP_SECRET");
    const char *bot_id = getenv("YUANBAO_BOT_ID");
    const char *ws_url = getenv("YUANBAO_WS_URL");
    const char *api_domain = getenv("YUANBAO_API_DOMAIN");
    if (!app_id || !app_secret) {
        fprintf(stderr, "Warning: YUANBAO_APP_ID and YUANBAO_APP_SECRET must be set\n");
        return false;
    }
    return yuanbao_init(app_id, app_secret, bot_id, ws_url, api_domain);
}

static void *thread_yuanbao(void *arg) {
    (void)arg;
    yuanbao_start();
    return NULL;
}

int hermes_gateway_main(int argc, char **argv) {
    memset(&g_gw, 0, sizeof(g_gw));
    g_gw.running = true;
    g_gw.poll_interval = 1;
    g_gw.tg_offset = 0;
    pthread_mutex_init(&g_gw.agent_mutex, NULL);

    /* Open log file with rotation (B15) */
    gw_log_open();

    /* P101: Initialize message queue and HTTP pool */
    gw_queue_init();
    g_gw.observe_buffer[0] = '\0';
    pthread_mutex_init(&g_gw.observe_mutex, NULL);
    pthread_mutex_init(&g_gw.pool_mutex, NULL);
    /* P102: Initialize session pool */
    pthread_mutex_init(&g_gw.session_mutex, NULL);
    {
        char db_path[GW_PATH_MAX];
        const char *home = getenv("SLERMES_HOME");
        if (!home) home = getenv("HERMES_HOME");
        if (!home) home = getenv("HOME");
        snprintf(db_path, sizeof(db_path), "%s/.slermes/sessions", home ? home : "/tmp");
        snprintf(g_gw.session_db_path, sizeof(g_gw.session_db_path), "%s", db_path);
    }

    /* Parse --platform flag for backwards compat (single-platform mode) */
    char cli_platform[32] = {0};
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--platform") == 0 && i + 1 < argc) {
            snprintf(cli_platform, sizeof(cli_platform), "%s", argv[++i]);
        }
    }

    /* Load config */
    hermes_config_load(&g_gw.config, NULL);
    hermes_config_load_env(&g_gw.config);

    /* Create default HTTP client */
    g_gw.http = http_client_new_with_retry(30, 3, 1000);

    /* Initialize agent */
    agent_init(&g_gw.agent);
    tools_init_all();
    g_gw.agent.tools = *registry_get();

    /* Copy config to agent */
    memcpy(g_gw.agent.llm.base_url, g_gw.config.base_url, sizeof(g_gw.agent.llm.base_url));
    memcpy(g_gw.agent.llm.api_key, g_gw.config.api_key, sizeof(g_gw.agent.llm.api_key));
    memcpy(g_gw.agent.llm.model, g_gw.config.model, sizeof(g_gw.agent.llm.model));
    memcpy(g_gw.agent.llm.provider, g_gw.config.provider, sizeof(g_gw.agent.llm.provider));
    g_gw.agent.max_iterations = g_gw.config.max_turns;
    g_gw.agent.compress_enabled = g_gw.config.compress_enabled;
    /* P150: Forward enabled/disabled toolsets to agent */
    if (g_gw.config.tools.enabled_toolsets[0])
        snprintf(g_gw.agent.enabled_toolsets, sizeof(g_gw.agent.enabled_toolsets), "%s", g_gw.config.tools.enabled_toolsets);
    if (g_gw.config.tools.disabled_toolsets[0])
        snprintf(g_gw.agent.disabled_toolsets, sizeof(g_gw.agent.disabled_toolsets), "%s", g_gw.config.tools.disabled_toolsets);
    /* Also copy yolo/fast/verbose for gateway runtime */
    approval_set_yolo(g_gw.config.yolo_mode);

    /* Apply CDP URL */
    if (g_gw.config.cdp_url[0])
        cdp_set_url(g_gw.config.cdp_url);

    printf("[gateway] WuBu Slermes Gateway v%s\n", HERMES_VERSION);

    /* Determine platforms to run */
    platform_def_t all_platforms[] = {
        {"telegram",   setup_telegram,   thread_poll_telegram,   0},
        {"discord",    setup_discord,    thread_poll_discord,    0},
        {"slack",      setup_slack,      thread_poll_slack,      0},
        {"matrix",     setup_matrix,     thread_poll_matrix,     0},
        {"mattermost", setup_mattermost, thread_poll_mattermost, 0},
        {"webhook",    setup_webhook,    thread_webhook,         0},
        {"whatsapp",   setup_whatsapp,   thread_webhook,         0},
        {"email",      setup_email,      thread_poll_email,      0},
        {"signal",     setup_signal,     thread_poll_signal,     0},
        {"homeassistant", setup_ha,      thread_poll_ha,         0},
        {"sms",        setup_sms,        thread_poll_sms,        0},
        {"api_server", setup_webhook,    thread_webhook,         0},
        {"feishu",     setup_feishu,     thread_poll_feishu,     0},
        {"wecom",      setup_wecom,      thread_poll_wecom,      0},
        {"dingtalk",   setup_dingtalk,   thread_poll_dingtalk,   0},
        {"qqbot",      setup_qqbot,      thread_poll_qqbot,      0},
        {"bluebubbles",setup_bluebubbles,thread_poll_bluebubbles,0},
        {"msgraph_webhook", setup_msgraph_webhook, thread_msgraph_webhook, 0},
        {"weixin", setup_weixin, thread_weixin, 0},
        {"yuanbao", setup_yuanbao, thread_yuanbao, 0},
        {NULL, NULL, NULL, 0}
    };

    /* P103: Register base platform interface adapters.
     * Each polling-based platform gets its adapter populated from
     * the individual static functions in the platform modules. */

    /* Build platform list:
     * 1. If --platform flag given, add that single one
     * 2. Otherwise, read from config.gateway_platforms (comma-separated)
     * 3. Fallback: "telegram" if no platforms specified */
    char platforms_buf[256];
    platforms_buf[0] = '\0';

    if (cli_platform[0]) {
        snprintf(platforms_buf, sizeof(platforms_buf), "%s", cli_platform);
    } else if (g_gw.config.gateway_platforms[0]) {
        snprintf(platforms_buf, sizeof(platforms_buf), "%s",
                 g_gw.config.gateway_platforms);
    } else {
        /* Default: try env var HERMES_GATEWAY_PLATFORMS */
        const char *env_platforms = getenv("HERMES_GATEWAY_PLATFORMS");
        if (env_platforms)
            snprintf(platforms_buf, sizeof(platforms_buf), "%s", env_platforms);
        else
            snprintf(platforms_buf, sizeof(platforms_buf), "telegram");
    }

    /* Parse comma-separated platform list and start each */
    char *saveptr = NULL;
    char *tok = strtok_r(platforms_buf, ", ", &saveptr);
    while (tok && g_gw.platform_count < GW_MAX_PLATFORMS) {
        /* Find platform definition */
        bool found = false;
        for (int i = 0; all_platforms[i].name; i++) {
            if (strcasecmp(tok, all_platforms[i].name) == 0) {
                /* Setup platform */
                if (all_platforms[i].setup()) {
                    snprintf(g_gw.platforms[g_gw.platform_count],
                             sizeof(g_gw.platforms[0]), "%s", tok);

                    /* Set arg_int for webhook/whatsapp (port number) */
                    all_platforms[i].arg_int = get_webhook_port();

                    printf("[gateway] Starting platform: %s\n", tok);

                    /* Create thread */
                    if (pthread_create(&g_gw.threads[g_gw.platform_count], NULL,
                                       all_platforms[i].thread_fn,
                                       &all_platforms[i].arg_int) == 0) {
                        /* P101: Initialize rate limiter for this platform */
                        double rps = (strcmp(tok, "email") == 0) ? 0.2 :
                                     (strcmp(tok, "sms") == 0) ? 0.1 :
                                     (strcmp(tok, "signal") == 0) ? 0.5 :
                                     (strcmp(tok, "telegram") == 0) ? 30.0 :
                                     (strcmp(tok, "discord") == 0) ? 5.0 : 3.0;
                        gw_rate_limit_init(g_gw.platform_count, rps, rps * 3);
                        /* P103: Register this platform in the interface registry */
                        {
                            gw_platform_t plat;
                            memset(&plat, 0, sizeof(plat));
                            plat.name = g_gw.platforms[g_gw.platform_count];
                            /* All polling-based platforms: init=setup, send=poll-based functions */
                            plat.init = all_platforms[i].setup;
                            plat.shutdown = NULL; /* no-op for now — S07 resolved with gw_platform_shutdown_all() */
                            gw_platform_register(&plat);
                        }
                        g_gw.platform_count++;
                    } else {
                        fprintf(stderr, "Error: Failed to create thread for %s\n", tok);
                    }
                } else {
                    fprintf(stderr, "[gateway] Skipping platform %s (setup failed)\n", tok);
                }
                found = true;
                break;
            }
        }
        if (!found)
            fprintf(stderr, "Warning: Unknown platform '%s'\n", tok);
        tok = strtok_r(NULL, ", ", &saveptr);
    }

    if (g_gw.platform_count == 0) {
        fprintf(stderr, "Error: No platforms could be started.\n");
        goto cleanup;
    }

    printf("[gateway] %d platform(s) running, %s configured\n",
           g_gw.platform_count, platforms_buf);

    /* Setup signal handler */
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    /* Wire cron notifications through gateway */
    cron_notify_set_send_fn(gw_platform_send);

    /* Set cron notification channel from env var (format: "platform:chat_id") */
    {
        const char *cron_chan = getenv("HERMES_CRON_NOTIFY_CHANNEL");
        if (cron_chan && cron_chan[0]) {
            cron_notify_set_channel(cron_chan);
            printf("[gateway] Cron notification channel: %s\n", cron_chan);
        }
    }

    printf("[gateway] %d platform(s) running. Press Ctrl+C to stop\n",
           g_gw.platform_count);

    /* Wait for all threads */
    for (int i = 0; i < g_gw.platform_count; i++)
        pthread_join(g_gw.threads[i], NULL);

cleanup:
    /* Shutdown all platforms */
    gw_platform_shutdown_all();
    /* P102: Save and free all sessions */
    session_save_all();
    pthread_mutex_destroy(&g_gw.session_mutex);
    for (int i = 0; i < g_gw.session_count; i++) {
        if (g_gw.sessions[i].in_use) {
            if (g_gw.sessions[i].db) db_close(g_gw.sessions[i].db);
            agent_free(&g_gw.sessions[i].agent);
        }
    }
    pthread_mutex_destroy(&g_gw.agent_mutex);
    /* P101: Cleanup HTTP pool and queue */
    gw_pool_cleanup();
    pthread_mutex_destroy(&g_gw.pool_mutex);
    pthread_mutex_destroy(&g_gw.queue_mutex);
    pthread_cond_destroy(&g_gw.queue_cond);
    agent_free(&g_gw.agent);
    http_client_free(g_gw.http);
    gw_log_close();
    printf("[gateway] Shutdown complete\n");
    return g_gw.platform_count > 0 ? 0 : 1;
}
