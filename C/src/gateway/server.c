/*
 * server.c — Minimal gateway server for Hermes C.
 * Long-polling based: polls Telegram API for updates.
 */

#include "hermes.h"
#include "hermes_agent.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

/* ================================================================
 *  Gateway state
 * ================================================================ */

typedef struct {
    agent_state_t agent;
    hermes_config_t config;
    http_client_t *http;
    char bot_token[256];
    int  poll_interval;  /* seconds between polls */
    int  offset;         /* last update_id + 1 */
    bool running;
} gateway_state_t;

static gateway_state_t g_gw;

/* ================================================================
 *  Telegram API helpers (inline — also in telegram.c)
 * ================================================================ */

static char *tg_api_url(const char *method) {
    static char url[512];
    snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/%s",
             g_gw.bot_token, method);
    return url;
}

static void tg_send_message(const char *chat_id, const char *text) {
    if (!chat_id || !text) return;

    json_node_t *body = json_new_object();
    json_object_set(body, "chat_id", json_new_string(chat_id));
    json_object_set(body, "text", json_new_string(text));
    json_object_set(body, "parse_mode", json_new_string("Markdown"));

    char *payload = json_serialize(body);
    json_free(body);

    http_response_t *resp = http_request_json(g_gw.http, HTTP_POST,
                                               tg_api_url("sendMessage"),
                                               payload);
    free(payload);
    if (resp) http_response_free(resp);
}

/* ================================================================
 *  Poll loop
 * ================================================================ */

static void process_update(json_node_t *update) {
    json_node_t *msg = json_object_get(update, "message");
    if (!msg) return;

    const char *chat_id_str = NULL;
    json_node_t *chat = json_object_get(msg, "chat");
    if (chat) {
        double chat_id = json_object_get_number(chat, "id", 0);
        static char cid[32];
        if (chat_id == (double)(long long)chat_id)
            snprintf(cid, sizeof(cid), "%lld", (long long)chat_id);
        else
            snprintf(cid, sizeof(cid), "%.0f", chat_id);
        chat_id_str = cid;
    }

    const char *text = json_object_get_string(msg, "text", "");
    if (!chat_id_str || !text || !*text) return;

    printf("[gateway] Message from %s: %s\n", chat_id_str, text);

    /* Send typing indicator */
    tg_send_message(chat_id_str, "_thinking..._");

    /* Run agent */
    char *resp = agent_chat(&g_gw.agent, text);
    if (resp) {
        /* Send response in chunks if too long (Telegram limit: 4096) */
        size_t len = strlen(resp);
        if (len > 4000) {
            /* Send first 4000 chars */
            char chunk[4001];
            memcpy(chunk, resp, 4000);
            chunk[4000] = '\0';
            tg_send_message(chat_id_str, chunk);

            /* Send rest */
            if (len > 4000)
                tg_send_message(chat_id_str, resp + 4000);
        } else {
            tg_send_message(chat_id_str, resp);
        }
        free(resp);
    }
}

static void poll_loop(void) {
    printf("[gateway] Starting poll loop (interval: %ds)\n", g_gw.poll_interval);

    while (g_gw.running) {
        /* Build getUpdates request */
        json_node_t *body = json_new_object();
        json_object_set(body, "offset", json_new_number((double)g_gw.offset));
        json_object_set(body, "timeout", json_new_number(30));
        json_object_set(body, "allowed_updates", json_new_array());
        json_node_t *allowed = json_object_get(body, "allowed_updates");
        json_array_append(allowed, json_new_string("message"));

        char *payload = json_serialize(body);
        json_free(body);

        http_response_t *resp = http_request_json(g_gw.http, HTTP_POST,
                                                   tg_api_url("getUpdates"),
                                                   payload);
        free(payload);

        if (!resp) {
            sleep(g_gw.poll_interval);
            continue;
        }

        if (resp->status == 200 && resp->body) {
            char *err = NULL;
            json_node_t *root = json_parse(resp->body, &err);

            if (root) {
                json_node_t *result = json_object_get(root, "result");
                if (result && result->type == JSON_ARRAY) {
                    size_t n = json_array_count(result);
                    for (size_t i = 0; i < n; i++) {
                        json_node_t *update = json_array_get(result, i);
                        double update_id = json_object_get_number(update, "update_id", 0);
                        if (update_id > 0)
                            g_gw.offset = (int)update_id + 1;
                        process_update(update);
                    }
                }
                json_free(root);
            } else {
                fprintf(stderr, "[gateway] JSON parse error: %s\n", err ? err : "unknown");
                free(err);
            }
        }

        http_response_free(resp);

        if (g_gw.running)
            sleep(g_gw.poll_interval);
    }

    printf("[gateway] Poll loop stopped\n");
}

/* ================================================================
 *  Gateway entry point
 * ================================================================ */

int hermes_gateway_main(int argc, char **argv) {
    memset(&g_gw, 0, sizeof(g_gw));
    g_gw.running = true;
    g_gw.poll_interval = 1;  /* Poll every 1 second */
    g_gw.offset = 0;

    /* Load config */
    hermes_config_load(&g_gw.config, NULL);
    hermes_config_load_env(&g_gw.config);

    /* Get bot token from config or env */
    const char *token = getenv("TELEGRAM_BOT_TOKEN");
    if (!token) token = getenv("HERMES_TELEGRAM_TOKEN");
    if (!token) {
        fprintf(stderr, "Error: TELEGRAM_BOT_TOKEN not set\n");
        fprintf(stderr, "Usage: %s gateway\n", argv[0] ? argv[0] : "hermes");
        fprintf(stderr, "Set TELEGRAM_BOT_TOKEN env var or in .env\n");
        return 1;
    }
    snprintf(g_gw.bot_token, sizeof(g_gw.bot_token), "%s", token);

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
    printf("[gateway] Bot token: %s...\n", g_gw.bot_token[0] ? g_gw.bot_token : "(not set)");
    printf("[gateway] Model: %s\n", g_gw.config.model);
    printf("[gateway] Press Ctrl+C to stop\n");

    /* Poll loop */
    poll_loop();

    /* Cleanup */
    agent_free(&g_gw.agent);
    http_client_free(g_gw.http);
    printf("[gateway] Shutdown complete\n");
    return 0;
}
