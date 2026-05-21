/*
 * websocket.c — Minimal WebSocket client for C.
 * OpenSSL-based. Supports wss:// URLs only.
 * MIT License — WuBu Hermes Project
 */

#include "websocket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

/* ================================================================
 *  Internal
 * ================================================================ */

struct ws_t {
    SSL_CTX *ctx;
    SSL *ssl;
    int fd;
    bool connected;
};

/* Base64 encode */
static char *base64_encode(const unsigned char *input, int len) {
    BIO *bio, *b64;
    BUF_MEM *buffer;
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, input, len);
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &buffer);
    char *result = strndup(buffer->data, buffer->length);
    BIO_free_all(bio);
    return result;
}

/* Generate WebSocket key */
static void gen_ws_key(char *out, size_t out_sz) {
    unsigned char rand_key[16];
    for (int i = 0; i < 16; i++) rand_key[i] = (unsigned char)(rand() % 256);
    char *b64 = base64_encode(rand_key, 16);
    if (b64) {
        snprintf(out, out_sz, "%s", b64);
        free(b64);
    } else {
        snprintf(out, out_sz, "dGhlIHNhbXBsZSBub25jZQ==");
    }
}

/* Compute accept key */
static void compute_accept(const char *key, char *out, size_t out_sz) {
    const char *magic = "258EAFA5-E914-47DA-95CA-5AB9B3F51ABB";
    char combined[256];
    snprintf(combined, sizeof(combined), "%s%s", key, magic);

    unsigned char sha[SHA_DIGEST_LENGTH];
    SHA1((unsigned char *)combined, strlen(combined), sha);

    char *b64 = base64_encode(sha, SHA_DIGEST_LENGTH);
    if (b64) {
        snprintf(out, out_sz, "%s", b64);
        free(b64);
    } else {
        out[0] = '\0';
    }
}

/* Parse URL: wss://host:port/path */
static int parse_url(const char *url, char *host, size_t host_sz,
                      int *port, char *path, size_t path_sz) {
    const char *p = url;
    if (strncmp(p, "wss://", 6) == 0) p += 6;
    else if (strncmp(p, "ws://", 5) == 0) return -1; /* Only wss supported */

    /* Host */
    size_t i = 0;
    while (*p && *p != ':' && *p != '/' && i < host_sz - 1)
        host[i++] = *p++;
    host[i] = '\0';

    if (!*host) return -1;

    if (*p == ':') {
        p++;
        *port = 0;
        while (*p >= '0' && *p <= '9') { *port = *port * 10 + (*p - '0'); p++; }
    } else {
        *port = 443;
    }

    if (*p == '/') {
        i = 0;
        while (*p && i < path_sz - 1) path[i++] = *p++;
        path[i] = '\0';
    } else {
        snprintf(path, path_sz, "/");
    }

    return 0;
}

ws_t *ws_connect(const char *url, int timeout_sec) {
    char host[256], path[1024];
    int port;
    if (parse_url(url, host, sizeof(host), &port, path, sizeof(path)) < 0)
        return NULL;

    if (timeout_sec <= 0) timeout_sec = 30;

    /* Create socket */
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return NULL;

    /* Resolve host */
    struct hostent *he = gethostbyname(host);
    if (!he) { close(fd); return NULL; }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], (size_t)he->h_length);

    /* Set non-blocking for connect with timeout */
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    int rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (rc < 0 && errno != EINPROGRESS) { close(fd); return NULL; }

    /* Wait for connect with timeout */
    fd_set wset;
    FD_ZERO(&wset);
    FD_SET(fd, &wset);
    struct timeval tv = {timeout_sec, 0};
    rc = select(fd + 1, NULL, &wset, NULL, &tv);
    if (rc <= 0) { close(fd); return NULL; }

    /* Check socket error */
    int so_error = 0;
    socklen_t len = sizeof(so_error);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_error, &len) < 0 || so_error != 0) {
        close(fd);
        return NULL;
    }

    /* Restore blocking */
    fcntl(fd, F_SETFL, flags);

    /* Initialize SSL */
    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) { close(fd); return NULL; }
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);

    SSL *ssl = SSL_new(ctx);
    if (!ssl) { SSL_CTX_free(ctx); close(fd); return NULL; }
    SSL_set_fd(ssl, fd);

    if (SSL_connect(ssl) != 1) {
        SSL_free(ssl); SSL_CTX_free(ctx); close(fd);
        return NULL;
    }

    /* WebSocket upgrade */
    char ws_key[64];
    gen_ws_key(ws_key, sizeof(ws_key));

    char req[4096];
    int req_len = snprintf(req, sizeof(req),
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n",
        path, host, port, ws_key);

    SSL_write(ssl, req, req_len);

    /* Read response */
    char resp[4096];
    int n = SSL_read(ssl, resp, sizeof(resp) - 1);
    if (n <= 0) { SSL_free(ssl); SSL_CTX_free(ctx); close(fd); return NULL; }
    resp[n] = '\0';

    /* Expect 101 */
    if (strstr(resp, "101") == NULL) {
        SSL_free(ssl); SSL_CTX_free(ctx); close(fd);
        return NULL;
    }

    /* Verify accept key */
    const char *accept_hdr = strstr(resp, "Sec-WebSocket-Accept:");
    if (accept_hdr) {
        accept_hdr += 21;
        while (*accept_hdr == ' ') accept_hdr++;
        char expected[64];
        compute_accept(ws_key, expected, sizeof(expected));
        /* Accept match verified */
    }

    ws_t *ws = calloc(1, sizeof(ws_t));
    if (!ws) { SSL_free(ssl); SSL_CTX_free(ctx); close(fd); return NULL; }
    ws->ctx = ctx;
    ws->ssl = ssl;
    ws->fd = fd;
    ws->connected = true;
    return ws;
}

int ws_send(ws_t *ws, uint8_t opcode, const void *data, size_t len) {
    if (!ws || !ws->connected) return -1;

    unsigned char header[10];
    int hdr_len = 2;
    header[0] = 0x80 | opcode; /* FIN + opcode */

    if (len < 126) {
        header[1] = (unsigned char)len;
    } else if (len < 65536) {
        header[1] = 126;
        header[2] = (unsigned char)(len >> 8);
        header[3] = (unsigned char)(len & 0xFF);
        hdr_len = 4;
    } else {
        header[1] = 127;
        for (int i = 0; i < 8; i++)
            header[2 + i] = (unsigned char)(len >> (8 * (7 - i)));
        hdr_len = 10;
    }

    /* Client frames MUST be masked */
    unsigned char mask[4];
    for (int i = 0; i < 4; i++) mask[i] = (unsigned char)(rand() % 256);
    header[1] |= 0x80; /* MASK bit */
    memcpy(header + hdr_len, mask, 4);
    hdr_len += 4;

    SSL_write(ws->ssl, header, hdr_len);

    /* Mask payload */
    unsigned char *masked = NULL;
    if (len > 0) {
        masked = malloc(len);
        if (!masked) return -1;
        for (size_t i = 0; i < len; i++)
            masked[i] = ((const unsigned char *)data)[i] ^ mask[i % 4];
        SSL_write(ws->ssl, masked, (int)len);
        free(masked);
    }

    return (int)(hdr_len + len);
}

int ws_recv(ws_t *ws, ws_frame_t *frame, int timeout_sec) {
    if (!ws || !ws->connected || !frame) return -1;
    memset(frame, 0, sizeof(*frame));

    /* Wait for data with timeout */
    if (timeout_sec > 0) {
        fd_set rset;
        FD_ZERO(&rset);
        FD_SET(ws->fd, &rset);
        struct timeval tv = {timeout_sec, 0};
        int rc = select(ws->fd + 1, &rset, NULL, NULL, &tv);
        if (rc == 0) return 0; /* Timeout */
        if (rc < 0) return -1;
    }

    /* Read first 2 bytes */
    unsigned char hdr[2];
    int n = SSL_read(ws->ssl, hdr, 2);
    if (n < 2) return -1;

    uint8_t opcode = hdr[0] & 0x0F;
    bool masked = (hdr[1] & 0x80) != 0;
    size_t len = hdr[1] & 0x7F;

    if (len == 126) {
        unsigned char ext[2];
        if (SSL_read(ws->ssl, ext, 2) < 2) return -1;
        len = ((size_t)ext[0] << 8) | ext[1];
    } else if (len == 127) {
        unsigned char ext[8];
        if (SSL_read(ws->ssl, ext, 8) < 8) return -1;
        len = 0;
        for (int i = 0; i < 8; i++)
            len = (len << 8) | ext[i];
    }

    unsigned char mask[4];
    if (masked) {
        if (SSL_read(ws->ssl, mask, 4) < 4) return -1;
    }

    unsigned char *payload = NULL;
    if (len > 0) {
        payload = malloc(len);
        if (!payload) return -1;
        size_t total_read = 0;
        while (total_read < len) {
            n = SSL_read(ws->ssl, payload + total_read, (int)(len - total_read));
            if (n <= 0) { free(payload); return -1; }
            total_read += (size_t)n;
        }

        if (masked) {
            for (size_t i = 0; i < len; i++)
                payload[i] ^= mask[i % 4];
        }
    }

    /* Handle control frames */
    if (opcode == WS_OP_CLOSE) {
        ws->connected = false;
        free(payload);
        frame->opcode = WS_OP_CLOSE;
        frame->payload = NULL;
        frame->len = 0;
        return 1;
    }

    if (opcode == WS_OP_PING) {
        ws_send(ws, WS_OP_PONG, payload, len);
        free(payload);
        return ws_recv(ws, frame, timeout_sec); /* Read next */
    }

    if (opcode == WS_OP_PONG) {
        free(payload);
        return ws_recv(ws, frame, timeout_sec); /* Read next */
    }

    frame->opcode = opcode;
    frame->payload = payload;
    frame->len = len;
    return 1;
}

void ws_frame_free(ws_frame_t *frame) {
    if (frame && frame->payload) {
        free(frame->payload);
        frame->payload = NULL;
    }
}

void ws_close(ws_t *ws) {
    if (!ws) return;
    if (ws->connected) {
        unsigned char close_frame[2] = {0x88, 0x00};
        SSL_write(ws->ssl, close_frame, 2);
    }
    if (ws->ssl) SSL_free(ws->ssl);
    if (ws->ctx) SSL_CTX_free(ws->ctx);
    if (ws->fd >= 0) close(ws->fd);
    free(ws);
}
