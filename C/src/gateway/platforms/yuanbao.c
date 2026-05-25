/*
 * yuanbao.c — Yuanbao (Tencent) WebSocket gateway for Hermes C.
 * Uses libwebsocket + libprotobuf for the Yuanbao protocol stack.
 * ConnMsg → Head+Data → business payloads.
 * MIT License — WuBu Slermes Project
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_gateway.h"
#include "websocket.h"
#include "protobuf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

/* ================================================================
 *  Constants
 * ================================================================ */

#define YB_DEFAULT_WS_URL "wss://yuanbao.example.com/ws"
#define YB_DEFAULT_API_DOMAIN "https://api.yuanbao.example.com"
#define HEARTBEAT_INTERVAL_SEC 30
#define RECONNECT_DELAY_SEC 5
#define MAX_BUF 65536

/* ConnMsg Head fields */
#define HEAD_CMD_TYPE  1  /* varint */
#define HEAD_CMD       2  /* string */
#define HEAD_SEQ_NO    3  /* varint */
#define HEAD_MSG_ID    4  /* string */
#define HEAD_MODULE    5  /* string */

/* ConnMsg fields */
#define CONN_FIELD_HEAD 1  /* message */
#define CONN_FIELD_DATA 2  /* bytes */

/* CMD_TYPE values */
#define CT_REQUEST  0
#define CT_RESPONSE 1
#define CT_PUSH     2
#define CT_PUSH_ACK 3

/* CMD strings */
#define CMD_AUTH_BIND  "auth-bind"
#define CMD_PING       "ping"

/* Biz service */
#define BIZ_PKG "yuanbao_openclaw_proxy"

/* Yuanbao instance ID for protobuf encoding */
#define YB_INSTANCE_ID 17

/* ================================================================
 *  State
 * ================================================================ */

typedef struct {
    char ws_url[1024];
    char app_id[256];
    char app_secret[256];
    char bot_id[256];
    char api_domain[1024];
    bool running;
    ws_t *ws;
    uint32_t seq_no;
} yuanbao_state_t;

static yuanbao_state_t g_yb;
static pthread_mutex_t g_yb_lock = PTHREAD_MUTEX_INITIALIZER;

/* ================================================================
 *  Protobuf helpers specific to Yuanbao
 * ================================================================ */

/* Encode a Head message */
static int encode_head(uint8_t *buf, size_t buf_len,
                        int cmd_type, const char *cmd,
                        uint32_t seq_no, const char *msg_id,
                        const char *module) {
    int pos = 0;
    int n;

    n = pb_encode_varint_field(buf + pos, buf_len - (size_t)pos,
                               HEAD_CMD_TYPE, (uint64_t)cmd_type);
    if (n <= 0) return -1; pos += n;

    if (cmd && *cmd) {
        n = pb_encode_delimited_field(buf + pos, buf_len - (size_t)pos,
                                       HEAD_CMD, (const uint8_t *)cmd, strlen(cmd));
        if (n <= 0) return -1; pos += n;
    }

    n = pb_encode_varint_field(buf + pos, buf_len - (size_t)pos,
                               HEAD_SEQ_NO, seq_no);
    if (n <= 0) return -1; pos += n;

    if (msg_id && *msg_id) {
        n = pb_encode_delimited_field(buf + pos, buf_len - (size_t)pos,
                                       HEAD_MSG_ID, (const uint8_t *)msg_id, strlen(msg_id));
        if (n <= 0) return -1; pos += n;
    }

    if (module && *module) {
        n = pb_encode_delimited_field(buf + pos, buf_len - (size_t)pos,
                                       HEAD_MODULE, (const uint8_t *)module, strlen(module));
        if (n <= 0) return -1; pos += n;
    }

    return pos;
}

/* Encode a ConnMsg: Head + optional Data */
static int encode_conn_msg(uint8_t *buf, size_t buf_len,
                            int cmd_type, const char *cmd,
                            uint32_t seq_no, const char *msg_id,
                            const char *module,
                            const uint8_t *data, size_t data_len) {
    int pos = 0;
    int n;

    /* Encode Head as nested message */
    uint8_t head_buf[1024];
    int head_len = encode_head(head_buf, sizeof(head_buf),
                                cmd_type, cmd, seq_no, msg_id, module);
    if (head_len <= 0) return -1;

    n = pb_encode_delimited_field(buf + pos, buf_len - (size_t)pos,
                                   CONN_FIELD_HEAD, head_buf, (size_t)head_len);
    if (n <= 0) return -1; pos += n;

    if (data && data_len > 0) {
        n = pb_encode_delimited_field(buf + pos, buf_len - (size_t)pos,
                                       CONN_FIELD_DATA, data, data_len);
        if (n <= 0) return -1; pos += n;
    }

    return pos;
}

/* Encode AuthBindReq */
static int encode_auth_bind(uint8_t *buf, size_t buf_len) {
    uint8_t body[2048];
    int body_pos = 0;
    int n;

    /* field 1: app_id (string) */
    n = pb_encode_delimited_field(body, sizeof(body), 1,
                                   (const uint8_t *)g_yb.app_id, strlen(g_yb.app_id));
    if (n <= 0) return -1; body_pos += n;

    /* field 2: app_secret (string) */
    n = pb_encode_delimited_field(body, sizeof(body), 2,
                                   (const uint8_t *)g_yb.app_secret, strlen(g_yb.app_secret));
    if (n <= 0) return -1; body_pos += n;

    /* field 3: bot_id (string) */
    if (g_yb.bot_id[0]) {
        n = pb_encode_delimited_field(body, sizeof(body), 3,
                                       (const uint8_t *)g_yb.bot_id, strlen(g_yb.bot_id));
        if (n <= 0) return -1; body_pos += n;
    }

    /* Wrap in ConnMsg */
    return encode_conn_msg(buf, buf_len,
                            CT_REQUEST, CMD_AUTH_BIND,
                            g_yb.seq_no++, "auth-1", "conn_access",
                            body, (size_t)body_pos);
}

/* Encode PingReq */
static int encode_ping_req(uint8_t *buf, size_t buf_len) {
    /* Empty body for PingReq */
    return encode_conn_msg(buf, buf_len,
                            CT_REQUEST, CMD_PING,
                            g_yb.seq_no++, "ping-1", "conn_access",
                            NULL, 0);
}

/* Encode SendC2CMessageReq for text message */
static int encode_send_c2c(uint8_t *buf, size_t buf_len,
                            const char *to_uid, const char *text, int64_t client_msg_id) {
    uint8_t body[4096];
    int body_pos = 0;
    int n;

    /* field 1: receiver_id (string) */
    n = pb_encode_delimited_field(body, sizeof(body), 1,
                                   (const uint8_t *)to_uid, strlen(to_uid));
    if (n <= 0) return -1; body_pos += n;

    /* field 3: sdk_app_id (uint32) — use YB_INSTANCE_ID */
    n = pb_encode_varint_field(body, sizeof(body), 3, YB_INSTANCE_ID);
    if (n <= 0) return -1; body_pos += n;

    /* field 5: client_msg_id (int64 / sint64) */
    n = pb_encode_varint_field(body, sizeof(body), 5, (uint64_t)client_msg_id);
    if (n <= 0) return -1; body_pos += n;

    /* field 6: msg_body (repeated MsgBodyElement) — one text element */
    uint8_t elem[2048];
    int elem_pos = 0;
    /* msg_type = "TIMTextElem" (field 1) */
    n = pb_encode_delimited_field(elem, sizeof(elem), 1,
                                   (const uint8_t *)"TIMTextElem", 11);
    if (n <= 0) return -1; elem_pos += n;

    /* msg_content.text = text (field 2, nested MsgContent) */
    uint8_t content[2048];
    int content_pos = 0;
    n = pb_encode_delimited_field(content, sizeof(content), 1,
                                   (const uint8_t *)text, strlen(text));
    if (n <= 0) return -1; content_pos += n;

    /* Wrap content as nested message field 2 */
    uint8_t elem_content[2048];
    int ec_pos = pb_encode_delimited_field(elem_content, sizeof(elem_content), 2,
                                            content, (size_t)content_pos);
    if (ec_pos <= 0) return -1;

    /* Add to elem */
    if ((size_t)elem_pos + (size_t)ec_pos > sizeof(elem)) return -1;
    memcpy(elem + elem_pos, elem_content, (size_t)ec_pos);
    elem_pos += ec_pos;

    /* Add elem as field 6 (repeated) */
    n = pb_encode_delimited_field(body + body_pos, sizeof(body) - (size_t)body_pos,
                                   6, elem, (size_t)elem_pos);
    if (n <= 0) return -1; body_pos += n;

    /* field 7: msg_type (uint32) = 0 for C2C */
    n = pb_encode_varint_field(body, sizeof(body), 7, 0);
    if (n <= 0) return -1; body_pos += n;

    /* field 8: instance_id (uint32) */
    n = pb_encode_varint_field(body, sizeof(body), 8, YB_INSTANCE_ID);
    if (n <= 0) return -1; body_pos += n;

    /* Wrap as biz msg */
    char msg_id_str[64];
    snprintf(msg_id_str, sizeof(msg_id_str), "c2c-%ld", (long)client_msg_id);

    return encode_conn_msg(buf, buf_len,
                            CT_REQUEST, "send_c2c_message",
                            g_yb.seq_no++, msg_id_str, BIZ_PKG,
                            body, (size_t)body_pos);
}

/* Decode a ConnMsg — extract head fields and optional data */
/* Returns: 0 = decoded, -1 = error */
static int decode_conn_msg(const uint8_t *data, size_t data_len,
                            int *out_cmd_type, char *out_cmd, size_t cmd_sz,
                            uint32_t *out_seq_no, char *out_module, size_t mod_sz,
                            const uint8_t **out_body, size_t *out_body_len) {
    *out_body = NULL;
    *out_body_len = 0;

    /* Extract Head message (field 1) */
    size_t head_len;
    const uint8_t *head_data = pb_read_delimited(data, data_len, 1, &head_len);
    if (!head_data) return -1;

    /* Parse head fields */
    if (out_cmd_type) {
        uint64_t v;
        if (pb_read_varint(head_data, head_len, 1, &v))
            *out_cmd_type = (int)v;
    }
    if (out_seq_no) {
        uint64_t v;
        if (pb_read_varint(head_data, head_len, 3, &v))
            *out_seq_no = (uint32_t)v;
    }
    if (out_cmd && cmd_sz > 0) {
        size_t cmd_len;
        const uint8_t *cmd_ptr = pb_read_delimited(head_data, head_len, 2, &cmd_len);
        if (cmd_ptr && cmd_len < cmd_sz) {
            memcpy(out_cmd, cmd_ptr, cmd_len);
            out_cmd[cmd_len] = '\0';
        }
    }
    if (out_module && mod_sz > 0) {
        size_t mod_len;
        const uint8_t *mod_ptr = pb_read_delimited(head_data, head_len, 5, &mod_len);
        if (mod_ptr && mod_len < mod_sz) {
            memcpy(out_module, mod_ptr, mod_len);
            out_module[mod_len] = '\0';
        }
    }

    /* Extract body data (field 2) */
    size_t body_len;
    const uint8_t *body_data = pb_read_delimited(data, data_len, 2, &body_len);
    if (body_data) {
        *out_body = body_data;
        *out_body_len = body_len;
    }

    return 0;
}

/* ================================================================
 *  Message processing
 * ================================================================ */

/* Extract text from InboundMessagePush body */
static void extract_push_text(const uint8_t *body, size_t body_len,
                               char *text_out, size_t text_sz,
                               char *sender_out, size_t sender_sz) {
    text_out[0] = '\0';
    sender_out[0] = '\0';

    /* field 2: sender_id (string) */
    size_t slen;
    const uint8_t *sptr = pb_read_delimited(body, body_len, 2, &slen);
    if (sptr && slen < sender_sz - 1) {
        memcpy(sender_out, sptr, slen);
        sender_out[slen] = '\0';
    }

    /* field 6: msg_body (repeated MsgBodyElement) */
    /* Each element has field 1: msg_type (string), field 2: msg_content (message) */
    /* We need to iterate the repeated field 6 manually */
    size_t offset = 0;
    while (offset < body_len) {
        uint32_t tag_no;
        int wire_type;
        int n = pb_parse_tag(body + offset, body_len - offset, &tag_no, &wire_type);
        if (n <= 0) break;
        offset += (size_t)n;

        if (tag_no == 6 && wire_type == PB_WIRE_LENDELIM) {
            uint64_t elem_len;
            int m = pb_decode_varint(body + offset, body_len - offset, &elem_len);
            if (m <= 0) break;
            offset += (size_t)m;

            if (offset + (size_t)elem_len > body_len) break;

            const uint8_t *elem_data = body + offset;
            size_t elem_data_len = (size_t)elem_len;
            offset += elem_data_len;

            /* Check msg_type is TIMTextElem */
            size_t mt_len;
            const uint8_t *mt = pb_read_delimited(elem_data, elem_data_len, 1, &mt_len);
            if (mt && mt_len == 11 && memcmp(mt, "TIMTextElem", 11) == 0) {
                /* Extract msg_content (field 2) */
                size_t mc_len;
                const uint8_t *mc = pb_read_delimited(elem_data, elem_data_len, 2, &mc_len);
                if (mc) {
                    /* Extract text (field 1 of MsgContent) */
                    size_t txt_len;
                    const uint8_t *txt = pb_read_delimited(mc, mc_len, 1, &txt_len);
                    if (txt && txt_len < text_sz - 1) {
                        memcpy(text_out, txt, txt_len);
                        text_out[txt_len] = '\0';
                    }
                }
            }
        } else {
            int skipped = pb_skip_field(body + offset, body_len - offset, wire_type);
            if (skipped <= 0) break;
            offset += (size_t)skipped;
        }
    }
}

/* ================================================================
 *  WS event loop
 * ================================================================ */

static void yuanbao_event_loop(void) {
    uint8_t recv_buf[MAX_BUF];
    ws_frame_t frame;

    printf("[gateway:yuanbao] Connected, sending AUTH_BIND...\n");

    /* Send AUTH_BIND */
    uint8_t auth_buf[4096];
    int auth_len = encode_auth_bind(auth_buf, sizeof(auth_buf));
    if (auth_len <= 0) {
        fprintf(stderr, "[gateway:yuanbao] Failed to encode AUTH_BIND\n");
        return;
    }
    ws_send(g_yb.ws, WS_OP_BIN, auth_buf, (size_t)auth_len);

    /* Main loop: receive messages + periodic heartbeat */
    time_t last_heartbeat = time(NULL);
    char cmd_buf[128], module_buf[128];

    while (g_yb.running) {
        /* Check for heartbeat */
        time_t now = time(NULL);
        if (now - last_heartbeat >= HEARTBEAT_INTERVAL_SEC) {
            printf("[gateway:yuanbao] Sending heartbeat...\n");
            uint8_t ping_buf[256];
            int ping_len = encode_ping_req(ping_buf, sizeof(ping_buf));
            if (ping_len > 0) {
                ws_send(g_yb.ws, WS_OP_BIN, ping_buf, (size_t)ping_len);
            }
            last_heartbeat = now;
        }

        /* Wait for incoming frame with 5s timeout */
        int rc = ws_recv(g_yb.ws, &frame, 5);
        if (rc == 0) continue; /* timeout — loop back to check heartbeat */
        if (rc < 0) {
            fprintf(stderr, "[gateway:yuanbao] WS recv error\n");
            break;
        }

        if (frame.opcode == WS_OP_CLOSE) {
            printf("[gateway:yuanbao] Connection closed by server\n");
            ws_frame_free(&frame);
            break;
        }

        if (frame.opcode == WS_OP_BIN) {
            /* Decode ConnMsg */
            int cmd_type = -1;
            uint32_t seq_no = 0;
            const uint8_t *body = NULL;
            size_t body_len = 0;
            cmd_buf[0] = '\0';
            module_buf[0] = '\0';

            int dc = decode_conn_msg(frame.payload, frame.len,
                                      &cmd_type, cmd_buf, sizeof(cmd_buf),
                                      &seq_no, module_buf, sizeof(module_buf),
                                      &body, &body_len);
            ws_frame_free(&frame);

            if (dc < 0) continue;

            printf("[gateway:yuanbao] Recv: cmd=%s cmd_type=%d seq=%u\n",
                   cmd_buf, cmd_type, seq_no);

            if (cmd_type == CT_PUSH && strcmp(cmd_buf, "InboundMessagePush") == 0) {
                /* Extract text from push */
                char text[4096], sender[256];
                extract_push_text(body, body_len, text, sizeof(text), sender, sizeof(sender));

                if (text[0] && sender[0]) {
                    printf("[gateway:yuanbao] Message from %s: %.200s\n", sender, text);

                    /* Send PushAck (cmd_type = 3) */
                    uint8_t ack_buf[256];
                    int ack_len = encode_conn_msg(ack_buf, sizeof(ack_buf),
                                                   CT_PUSH_ACK, cmd_buf,
                                                   g_yb.seq_no++, "ack-1", module_buf,
                                                   body, body_len);
                    if (ack_len > 0)
                        ws_send(g_yb.ws, WS_OP_BIN, ack_buf, (size_t)ack_len);

                    /* Forward to agent */
                    extern gateway_state_t g_gw;
                    pthread_mutex_lock(&g_gw.agent_mutex);
                    char *resp = agent_chat(&g_gw.agent, text);
                    pthread_mutex_unlock(&g_gw.agent_mutex);

                    if (resp) {
                        /* Send reply via C2C message */
                        uint8_t reply_buf[8192];
                        int64_t client_id = (int64_t)(now * 1000);
                        int rlen = encode_send_c2c(reply_buf, sizeof(reply_buf),
                                                    sender, resp, client_id);
                        if (rlen > 0) {
                            ws_send(g_yb.ws, WS_OP_BIN, reply_buf, (size_t)rlen);
                            printf("[gateway:yuanbao] Reply sent via C2C\n");
                        }
                        free(resp);
                    }
                }
            } else if (cmd_type == CT_RESPONSE && strcmp(cmd_buf, CMD_AUTH_BIND) == 0) {
                printf("[gateway:yuanbao] AUTH_BIND response received (seq=%u)\n", seq_no);
            } else if (cmd_type == CT_RESPONSE && strcmp(cmd_buf, CMD_PING) == 0) {
                /* Pong received — heartbeat OK */
            }
        } else {
            ws_frame_free(&frame);
        }
    }
}

/* ================================================================
 *  Public API
 * ================================================================ */

bool yuanbao_init(const char *app_id, const char *app_secret,
                   const char *bot_id, const char *ws_url,
                   const char *api_domain) {
    memset(&g_yb, 0, sizeof(g_yb));
    if (app_id) snprintf(g_yb.app_id, sizeof(g_yb.app_id), "%s", app_id);
    if (app_secret) snprintf(g_yb.app_secret, sizeof(g_yb.app_secret), "%s", app_secret);
    if (bot_id) snprintf(g_yb.bot_id, sizeof(g_yb.bot_id), "%s", bot_id);
    snprintf(g_yb.ws_url, sizeof(g_yb.ws_url), "%s", ws_url ? ws_url : YB_DEFAULT_WS_URL);
    snprintf(g_yb.api_domain, sizeof(g_yb.api_domain), "%s", api_domain ? api_domain : YB_DEFAULT_API_DOMAIN);
    g_yb.seq_no = 1;
    return *g_yb.app_id && *g_yb.app_secret;
}

void yuanbao_start(void) {
    if (!*g_yb.app_id || !*g_yb.app_secret) {
        fprintf(stderr, "[gateway:yuanbao] app_id and app_secret required\n");
        return;
    }

    while (g_yb.running) {
        printf("[gateway:yuanbao] Connecting to %s...\n", g_yb.ws_url);

        g_yb.ws = ws_connect(g_yb.ws_url, 30);
        if (!g_yb.ws) {
            fprintf(stderr, "[gateway:yuanbao] WS connect failed, retry in %ds\n",
                    RECONNECT_DELAY_SEC);
            for (int i = 0; i < RECONNECT_DELAY_SEC && g_yb.running; i++) sleep(1);
            continue;
        }

        yuanbao_event_loop();

        ws_close(g_yb.ws);
        g_yb.ws = NULL;

        if (g_yb.running) {
            printf("[gateway:yuanbao] Disconnected, reconnecting in %ds...\n",
                   RECONNECT_DELAY_SEC);
            for (int i = 0; i < RECONNECT_DELAY_SEC && g_yb.running; i++) sleep(1);
        }
    }
}

void yuanbao_stop(void) {
    g_yb.running = false;
}

/* Server.c setup/thread wrappers */
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
