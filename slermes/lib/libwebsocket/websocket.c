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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
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

/* Helper: write_raw — uses SSL_write if ssl is set, otherwise write() */
static int write_raw(ws_t *ws, const void *buf, int len) {
    if (ws->ssl) return SSL_write(ws->ssl, buf, len);
    return (int)write(ws->fd, buf, (size_t)len);
}

/* Helper: read_raw — uses SSL_read if ssl is set, otherwise read() */
static int read_raw(ws_t *ws, void *buf, int len) {
    if (ws->ssl) return SSL_read(ws->ssl, buf, len);
    return (int)read(ws->fd, buf, (size_t)len);
}

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

/* Parse URL: wss://host:port/path or ws://host:port/path */
/* Returns: 0 on success, -1 on error. Sets is_ssl true for wss:// */
static int parse_url(const char *url, char *host, size_t host_sz,
                      int *port, char *path, size_t path_sz, bool *is_ssl) {
    const char *p = url;
    bool ssl = false;
    if (strncmp(p, "wss://", 6) == 0) { p += 6; ssl = true; }
    else if (strncmp(p, "ws://", 5) == 0) p += 5;
    else return -1; /* Must have ws:// or wss:// */
    if (is_ssl) *is_ssl = ssl;

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
    } else if (!ssl) {
        *port = 80;
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
    bool use_ssl = true;
    if (!url || parse_url(url, host, sizeof(host), &port, path, sizeof(path), &use_ssl) < 0)
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

    /* Initialize SSL if needed */
    SSL_CTX *ctx = NULL;
    SSL *ssl = NULL;

    if (use_ssl) {
        ctx = SSL_CTX_new(TLS_client_method());
        if (!ctx) { close(fd); return NULL; }
        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);

        ssl = SSL_new(ctx);
        if (!ssl) { SSL_CTX_free(ctx); close(fd); return NULL; }
        SSL_set_fd(ssl, fd);

        if (SSL_connect(ssl) != 1) {
            SSL_free(ssl); SSL_CTX_free(ctx); close(fd);
            return NULL;
        }
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

    /* Send upgrade request */
    if (ssl) {
        SSL_write(ssl, req, req_len);
    } else {
        write(fd, req, (size_t)req_len);
    }

    /* Read response */
    char resp[4096];
    int n;
    if (ssl) {
        n = SSL_read(ssl, resp, sizeof(resp) - 1);
    } else {
        n = (int)read(fd, resp, sizeof(resp) - 1);
    }
    if (n <= 0) {
        if (ssl) { SSL_free(ssl); SSL_CTX_free(ctx); }
        else if (ctx) SSL_CTX_free(ctx);
        close(fd); return NULL;
    }
    resp[n] = '\0';

    /* Expect 101 */
    if (strstr(resp, "101") == NULL) {
        if (ssl) { SSL_free(ssl); SSL_CTX_free(ctx); }
        else if (ctx) SSL_CTX_free(ctx);
        close(fd);
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
    if (!ws) {
        if (ssl) { SSL_free(ssl); SSL_CTX_free(ctx); }
        else if (ctx) SSL_CTX_free(ctx);
        close(fd); return NULL;
    }
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

    write_raw(ws, header, hdr_len);

    /* Mask payload */
    unsigned char *masked = NULL;
    if (len > 0) {
        masked = malloc(len);
        if (!masked) return -1;
        for (size_t i = 0; i < len; i++)
            masked[i] = ((const unsigned char *)data)[i] ^ mask[i % 4];
        write_raw(ws, masked, (int)len);
        free(masked);
    }

    return (int)(hdr_len + len);
}

int ws_recv(ws_t *ws, ws_frame_t *frame, int timeout_sec) {
    if (!ws || !ws->connected || !frame) return -1;
    memset(frame, 0, sizeof(*frame));

    /* Wait for data with timeout */
    struct timeval tv;
    fd_set rset;
    if (timeout_sec > 0) {
        FD_ZERO(&rset);
        FD_SET(ws->fd, &rset);
        tv.tv_sec = timeout_sec;
        tv.tv_usec = 0;
        int rc = select(ws->fd + 1, &rset, NULL, NULL, &tv);
        if (rc == 0) return 0; /* Timeout */
        if (rc < 0) return -1;
    }

    /* Read first 2 bytes */
    unsigned char hdr[2];
    int n = read_raw(ws, hdr, 2);
    if (n < 2) return -1;

    uint8_t opcode = hdr[0] & 0x0F;
    bool masked = (hdr[1] & 0x80) != 0;
    size_t len = hdr[1] & 0x7F;

    if (len == 126) {
        unsigned char ext[2];
        if (read_raw(ws, ext, 2) < 2) return -1;
        len = ((size_t)ext[0] << 8) | ext[1];
    } else if (len == 127) {
        unsigned char ext[8];
        if (read_raw(ws, ext, 8) < 8) return -1;
        len = 0;
        for (int i = 0; i < 8; i++)
            len = (len << 8) | ext[i];
    }

    unsigned char mask[4];
    if (masked) {
        if (read_raw(ws, mask, 4) < 4) return -1;
    }

    unsigned char *payload = NULL;
    if (len > 0) {
        payload = malloc(len);
        if (!payload) return -1;
        size_t total_read = 0;
        while (total_read < len) {
            n = read_raw(ws, payload + total_read, (int)(len - total_read));
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
        write_raw(ws, close_frame, 2);
    }
    if (ws->ssl) SSL_free(ws->ssl);
    if (ws->ctx) SSL_CTX_free(ws->ctx);
    if (ws->fd >= 0) close(ws->fd);
    free(ws);
}

/* ── WebSocket Server ── */

struct ws_server_t {
    int listen_fd;
    int port;
    SSL_CTX *ctx;  /* NULL for ws:// */
};

ws_server_t *ws_server_listen(int port, const char *cert_path, const char *key_path) {
    ws_server_t *server = (ws_server_t *)calloc(1, sizeof(ws_server_t));
    if (!server) return NULL;
    server->port = port;
    server->listen_fd = -1;

    /* Create SSL context if cert provided */
    if (cert_path && key_path) {
        server->ctx = SSL_CTX_new(TLS_server_method());
        if (!server->ctx) { free(server); return NULL; }
        if (SSL_CTX_use_certificate_file(server->ctx, cert_path, SSL_FILETYPE_PEM) <= 0) {
            SSL_CTX_free(server->ctx); free(server); return NULL;
        }
        if (SSL_CTX_use_PrivateKey_file(server->ctx, key_path, SSL_FILETYPE_PEM) <= 0) {
            SSL_CTX_free(server->ctx); free(server); return NULL;
        }
    }

    /* Create TCP socket */
    server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->listen_fd < 0) {
        if (server->ctx) SSL_CTX_free(server->ctx);
        free(server);
        return NULL;
    }

    /* Allow immediate reuse */
    int opt = 1;
    setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* Make non-blocking so accept with timeout=0 works */
    int flags = fcntl(server->listen_fd, F_GETFL, 0);
    fcntl(server->listen_fd, F_SETFL, flags | O_NONBLOCK);

    /* Bind */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)(port > 0 ? port : 0));

    if (bind(server->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ws_server_close(server);
        return NULL;
    }

    /* Get actual port if port 0 was requested */
    if (port == 0) {
        socklen_t addrlen = sizeof(addr);
        if (getsockname(server->listen_fd, (struct sockaddr *)&addr, &addrlen) == 0) {
            server->port = ntohs(addr.sin_port);
        }
    } else {
        server->port = port;
    }

    if (listen(server->listen_fd, 5) < 0) {
        ws_server_close(server);
        return NULL;
    }

    return server;
}

/* Parse incoming HTTP WebSocket upgrade request and send 101 response.
 * Returns 0 on success, -1 on error.
 */
static int handle_upgrade(int client_fd) {
    char buf[4096];
    char key[256] = {0};
    int n = (int)read(client_fd, buf, sizeof(buf) - 1);
    if (n <= 0) return -1;
    buf[n] = '\0';

    /* Parse Sec-WebSocket-Key */
    const char *key_hdr = strstr(buf, "Sec-WebSocket-Key: ");
    if (!key_hdr) return -1;
    key_hdr += 19;
    for (int i = 0; i < 255 && *key_hdr && *key_hdr != '\r' && *key_hdr != '\n'; i++)
        key[i] = *key_hdr++;

    if (!key[0]) return -1;

    /* Compute accept key */
    const char *magic = "258EAFA5-E914-47DA-95CA-5AB9B3F51ABB";
    char combined[512];
    snprintf(combined, sizeof(combined), "%s%s", key, magic);
    unsigned char sha[SHA_DIGEST_LENGTH];
    SHA1((unsigned char *)combined, strlen(combined), sha);
    char *accept_b64 = base64_encode(sha, SHA_DIGEST_LENGTH);
    if (!accept_b64) return -1;

    /* Send 101 response */
    char response[1024];
    int rlen = snprintf(response, sizeof(response),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n\r\n",
        accept_b64);
    free(accept_b64);

    write(client_fd, response, (size_t)rlen);
    return 0;
}

ws_t *ws_server_accept(ws_server_t *server, int timeout_sec) {
    if (!server || server->listen_fd < 0) return NULL;

    /* Poll for incoming connection */
    if (timeout_sec > 0) {
        struct pollfd pfd = {.fd = server->listen_fd, .events = POLLIN};
        int ret = poll(&pfd, 1, timeout_sec * 1000);
        if (ret <= 0) return NULL;
    }

    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);
    int client_fd = accept(server->listen_fd, (struct sockaddr *)&client_addr, &addrlen);
    if (client_fd < 0) return NULL;

    /* Perform WebSocket upgrade handshake */
    if (handle_upgrade(client_fd) < 0) {
        close(client_fd);
        return NULL;
    }

    /* Create ws_t from the accepted connection */
    ws_t *ws = (ws_t *)calloc(1, sizeof(ws_t));
    if (!ws) { close(client_fd); return NULL; }
    ws->fd = client_fd;
    ws->connected = true;

    /* If server has SSL context, wrap the connection */
    if (server->ctx) {
        ws->ctx = server->ctx;  /* Share server context */
        ws->ssl = SSL_new(server->ctx);
        if (ws->ssl) {
            SSL_set_fd(ws->ssl, client_fd);
            SSL_accept(ws->ssl);
        }
    }

    return ws;
}

int ws_server_port(ws_server_t *server) {
    return server ? server->port : -1;
}

void ws_server_close(ws_server_t *server) {
    if (!server) return;
    if (server->listen_fd >= 0) close(server->listen_fd);
    if (server->ctx) SSL_CTX_free(server->ctx);
    free(server);
}
