/*
 * server.c — Multi-platform gateway server for Hermes C.
 * Supports Telegram (long-polling) and Discord (REST polling).
 * Select platform with --platform flag or HERMES_GATEWAY_PLATFORM env.
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

/* ================================================================
 *  Gateway state (non-static — shared with webhook.c)
 * ================================================================ */

gateway_state_t g_gw;

/* ================================================================
 *  Platform-aware message send
 * ================================================================ */

static void gateway_send(const char *target, const char *text) {
    if (!target || !text) return;

    if (strcmp(g_gw.platform, "telegram") == 0) {
        /* Telegram: send with Markdown, chunk if >4000 chars */
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
    } else if (strcmp(g_gw.platform, "discord") == 0) {
        discord_send_message(g_gw.http, text);
    } else if (strcmp(g_gw.platform, "mattermost") == 0) {
        mattermost_send_message(g_gw.http, text);
    } else if (strcmp(g_gw.platform, "whatsapp") == 0) {
        /* WhatsApp sends via API from webhook handler, not gateway_send */
        (void)target;
        (void)text;
    }
}

static void gateway_send_typing(const char *target) {
    if (strcmp(g_gw.platform, "telegram") == 0) {
        telegram_send_chat_action(g_gw.http, target, "typing");
    } else if (strcmp(g_gw.platform, "discord") == 0) {
        discord_send_typing(g_gw.http);
    }
    /* Mattermost: no typing indicator API */
}

/* ================================================================
 *  Process a single update
 * ================================================================ */

static void process_update(const char *chat_id, const char *text) {
    if (!chat_id || !text || !*text) return;

    printf("[gateway:%s] Message: %s\n", g_gw.platform, text);

    /* Send typing indicator */
    gateway_send_typing(chat_id);

    /* Run agent */
    char *resp = agent_chat(&g_gw.agent, text);
    if (resp) {
        gateway_send(chat_id, resp);
        free(resp);
    }
}

/* ================================================================
 *  Telegram poll loop
 * ================================================================ */

static void poll_telegram(void) {
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
                    process_update(telegram_get_chat_id(update),
                                   telegram_get_text(update));
                }
            }
            json_free(root);
        }

        if (g_gw.running)
            sleep(g_gw.poll_interval);
    }
}

/* ================================================================
 *  Discord poll loop
 * ================================================================ */

static void poll_discord(void) {
    printf("[gateway] Discord polling (interval: %ds)\n", g_gw.poll_interval);

    while (g_gw.running) {
        json_node_t *updates = discord_poll_messages(g_gw.http);

        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update(discord_get_chat_id(update),
                               discord_get_text(update));
            }
        }

        json_free(updates);

        if (g_gw.running)
            sleep(g_gw.poll_interval);
    }
}

/* ================================================================
 *  Slack poll loop
 * ================================================================ */

static void poll_slack(void) {
    printf("[gateway] Slack polling (interval: %ds)\n", g_gw.poll_interval);

    while (g_gw.running) {
        json_node_t *updates = slack_poll_messages(g_gw.http);

        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update(slack_get_chat_id(update),
                               slack_get_text(update));
            }
        }

        json_free(updates);

        if (g_gw.running)
            sleep(g_gw.poll_interval);
    }
}

/* ================================================================
 *  Matrix poll loop
 * ================================================================ */

static void poll_matrix(void) {
    printf("[gateway] Matrix polling (interval: %ds)\n", g_gw.poll_interval);

    while (g_gw.running) {
        json_node_t *updates = matrix_poll_messages(g_gw.http);

        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update(matrix_get_chat_id(update),
                               matrix_get_text(update));
            }
        }

        json_free(updates);

        if (g_gw.running)
            sleep(g_gw.poll_interval);
    }
}

/* ================================================================
 *  Mattermost poll loop
 * ================================================================ */

static void poll_mattermost(void) {
    printf("[gateway] Mattermost polling (interval: %ds)\n", g_gw.poll_interval);

    while (g_gw.running) {
        json_node_t *updates = mattermost_poll_messages(g_gw.http);

        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update(mattermost_get_chat_id(update),
                               mattermost_get_text(update));
            }
        }

        json_free(updates);

        if (g_gw.running)
            sleep(g_gw.poll_interval);
    }
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
 *  Gateway entry point
 * ================================================================ */

int hermes_gateway_main(int argc, char **argv) {
    memset(&g_gw, 0, sizeof(g_gw));
    g_gw.running = true;
    g_gw.poll_interval = 1;
    g_gw.tg_offset = 0;
    snprintf(g_gw.platform, sizeof(g_gw.platform), "telegram"); /* default */

    /* Parse args */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--platform") == 0 && i + 1 < argc) {
            snprintf(g_gw.platform, sizeof(g_gw.platform), "%s", argv[++i]);
        }
    }

    /* Load config */
    hermes_config_load(&g_gw.config, NULL);
    hermes_config_load_env(&g_gw.config);

    /* Create HTTP client */
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
    printf("[gateway] Platform: %s\n", g_gw.platform);

    /* Platform-specific setup */
    bool ok = false;
    if (strcmp(g_gw.platform, "telegram") == 0) {
        const char *token = getenv("TELEGRAM_BOT_TOKEN");
        if (!token) token = getenv("HERMES_TELEGRAM_TOKEN");
        if (!token) {
            fprintf(stderr, "Error: TELEGRAM_BOT_TOKEN not set\n");
            goto cleanup;
        }
        telegram_set_token(token);
        ok = true;
    } else if (strcmp(g_gw.platform, "discord") == 0) {
        const char *token = getenv("DISCORD_BOT_TOKEN");
        const char *channel = getenv("DISCORD_CHANNEL_ID");
        if (!token || !channel) {
            fprintf(stderr, "Error: DISCORD_BOT_TOKEN and DISCORD_CHANNEL_ID must be set\n");
            goto cleanup;
        }
        discord_set_token(token);
        discord_set_channel(channel);
        ok = true;
    } else if (strcmp(g_gw.platform, "webhook") == 0) {
        /* Webhook API — no token needed */
        ok = true;
    } else if (strcmp(g_gw.platform, "slack") == 0) {
        const char *token = getenv("SLACK_BOT_TOKEN");
        const char *channel = getenv("SLACK_CHANNEL_ID");
        if (!token || !channel) {
            fprintf(stderr, "Error: SLACK_BOT_TOKEN and SLACK_CHANNEL_ID must be set\n");
            goto cleanup;
        }
        slack_set_token(token);
        slack_set_channel(channel);
        ok = true;
    } else if (strcmp(g_gw.platform, "matrix") == 0) {
        const char *hs = getenv("MATRIX_HOMESERVER");
        const char *token = getenv("MATRIX_ACCESS_TOKEN");
        const char *room = getenv("MATRIX_ROOM_ID");
        if (!token) {
            fprintf(stderr, "Error: MATRIX_ACCESS_TOKEN must be set\n");
            goto cleanup;
        }
        matrix_set_homeserver(hs && hs[0] ? hs : "https://matrix.org");
        matrix_set_token(token);
        if (room) matrix_set_room(room);
        ok = true;
    } else if (strcmp(g_gw.platform, "mattermost") == 0) {
        const char *url = getenv("MATTERMOST_URL");
        const char *token = getenv("MATTERMOST_TOKEN");
        const char *channel = getenv("MATTERMOST_CHANNEL_ID");
        if (!token || !channel) {
            fprintf(stderr, "Error: MATTERMOST_TOKEN and MATTERMOST_CHANNEL_ID must be set\n");
            goto cleanup;
        }
        mattermost_set_url(url && url[0] ? url : "http://localhost:8065");
        mattermost_set_token(token);
        mattermost_set_channel(channel);
        ok = true;
    } else if (strcmp(g_gw.platform, "whatsapp") == 0) {
        const char *token = getenv("WHATSAPP_TOKEN");
        const char *phone = getenv("WHATSAPP_PHONE_NUMBER_ID");
        const char *verify = getenv("WHATSAPP_VERIFY_TOKEN");
        if (!token || !phone) {
            fprintf(stderr, "Error: WHATSAPP_TOKEN and WHATSAPP_PHONE_NUMBER_ID must be set\n");
            goto cleanup;
        }
        whatsapp_set_token(token);
        whatsapp_set_phone_id(phone);
        if (verify) whatsapp_set_verify_token(verify);
        ok = true;
    } else {
        fprintf(stderr, "Error: Unknown platform '%s'. Choose 'telegram', 'discord', 'webhook', 'slack', 'matrix', 'mattermost', or 'whatsapp'.\n",
                g_gw.platform);
        goto cleanup;
    }

    /* Setup signal handler */
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    printf("[gateway] Press Ctrl+C to stop\n");

    /* Start polling for selected platform */
    if (strcmp(g_gw.platform, "telegram") == 0)
        poll_telegram();
    else if (strcmp(g_gw.platform, "discord") == 0)
        poll_discord();
    else if (strcmp(g_gw.platform, "webhook") == 0) {
        const char *port_str = getenv("HERMES_WEBHOOK_PORT");
        int port = port_str ? atoi(port_str) : 8080;
        if (port <= 0 || port > 65535) port = 8080;
        webhook_server_run(port);
    } else if (strcmp(g_gw.platform, "slack") == 0)
        poll_slack();
    else if (strcmp(g_gw.platform, "matrix") == 0)
        poll_matrix();
    else if (strcmp(g_gw.platform, "mattermost") == 0)
        poll_mattermost();
    else if (strcmp(g_gw.platform, "whatsapp") == 0) {
        const char *port_str = getenv("HERMES_WEBHOOK_PORT");
        int port = port_str ? atoi(port_str) : 8080;
        if (port <= 0 || port > 65535) port = 8080;
        webhook_server_run(port);
    }

cleanup:
    agent_free(&g_gw.agent);
    http_client_free(g_gw.http);
    printf("[gateway] Shutdown complete\n");
    return ok ? 0 : 1;
}
