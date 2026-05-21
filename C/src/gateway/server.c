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

/* ================================================================
 *  Gateway state
 * ================================================================ */

gateway_state_t g_gw;

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
 *  Platform-aware message send
 * ================================================================ */

static void gateway_send(const char *platform, const char *target, const char *text) {
    if (!platform || !target || !text) return;

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
    if (strcmp(platform, "telegram") == 0)
        telegram_send_chat_action(g_gw.http, target, "typing");
    else if (strcmp(platform, "discord") == 0)
        discord_send_typing(g_gw.http);
}

/* ================================================================
 *  Process a single update (called from platform threads)
 * ================================================================ */

static void process_update(const char *platform, const char *chat_id, const char *text) {
    if (!platform || !chat_id || !text || !*text) return;

    printf("[gateway:%s] Message: %s\n", platform, text);

    /* Send typing indicator */
    gateway_send_typing(platform, chat_id);

    /* Run agent (thread-safe via mutex) */
    char *resp = gateway_agent_chat(text);
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
                    process_update("telegram",
                                   telegram_get_chat_id(update),
                                   telegram_get_text(update));
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
    return true;
}

static void *thread_poll_sms(void *arg) {
    (void)arg;
    printf("[gateway] SMS platform (outbound only via Twilio). Idle.\n");
    while (g_gw.running) sleep(g_gw.poll_interval * 10);
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
    printf("[gateway] WeCom platform (webhook-based). Idle.\n");
    while (g_gw.running) sleep(g_gw.poll_interval * 10);
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
    printf("[gateway] DingTalk platform (webhook-based). Idle.\n");
    while (g_gw.running) sleep(g_gw.poll_interval * 10);
    return NULL;
}

/* ================================================================
 *  Get port from env with HERMES_ or SLERMES_ prefix
 * ================================================================ */

static int get_webhook_port(void) {
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

int hermes_gateway_main(int argc, char **argv) {
    memset(&g_gw, 0, sizeof(g_gw));
    g_gw.running = true;
    g_gw.poll_interval = 1;
    g_gw.tg_offset = 0;
    pthread_mutex_init(&g_gw.agent_mutex, NULL);

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
    g_gw.http = http_client_new(30);

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

    printf("[gateway] WuBu Hermes Gateway v%s\n", HERMES_VERSION);

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
        {NULL, NULL, NULL, 0}
    };

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

    /* Setup signal handler */
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    printf("[gateway] %d platform(s) running. Press Ctrl+C to stop\n",
           g_gw.platform_count);

    /* Wait for all threads */
    for (int i = 0; i < g_gw.platform_count; i++)
        pthread_join(g_gw.threads[i], NULL);

cleanup:
    pthread_mutex_destroy(&g_gw.agent_mutex);
    agent_free(&g_gw.agent);
    http_client_free(g_gw.http);
    printf("[gateway] Shutdown complete\n");
    return g_gw.platform_count > 0 ? 0 : 1;
}
