/*
 * email.c — Email platform for Hermes C.
 * P110: IMAP IDLE push, attachments, HTML render, threading.
 *
 * Outgoing: SMTP (via raw socket + STARTTLS) or sendmail fallback.
 * Incoming: IMAP IDLE push (preferred) or maildir polling (legacy).
 *
 * Env vars:
 *   EMAIL_FROM           — From address (default: "hermes@localhost")
 *   EMAIL_SEND_CMD       — Send command (default: "sendmail -t")
 *
 *   EMAIL_IMAP_SERVER    — IMAP server hostname (for IDLE push)
 *   EMAIL_IMAP_PORT      — IMAP server port (default: 993 for IMAPS, 143 for plain)
 *   EMAIL_IMAP_USER      — IMAP username
 *   EMAIL_IMAP_PASS      — IMAP password
 *   EMAIL_IMAP_MAILBOX   — Mailbox to watch (default: "INBOX")
 *   EMAIL_IMAP_USE_SSL   — Use SSL (default: 1 for port 993, 0 for 143)
 *
 *   EMAIL_SMTP_SERVER    — SMTP server (for direct sending via SMTP)
 *   EMAIL_SMTP_PORT      — SMTP port (default: 587)
 *   EMAIL_SMTP_USER      — SMTP username
 *   EMAIL_SMTP_PASS      — SMTP password
 *   EMAIL_SMTP_USE_SSL   — Use SSL for SMTP (default: 0, uses STARTTLS)
 *
 *   EMAIL_ATTACH_DIR     — Directory for saved attachments (default: ~/.hermes/attachments/)
 *   EMAIL_POLL_DIR       — Maildir to poll for incoming (legacy, optional)
 *   EMAIL_POLL_INTERVAL  — Seconds between polls (default: 30)
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "hermes_gateway.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <ctype.h>

/* ================================================================
 *  Inline hex encode (no dependency on libcrypto)
 * ================================================================ */

static char *hex_encode(const unsigned char *data, size_t len) {
    static const char hex[] = "0123456789abcdef";
    char *out = malloc(len * 2 + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < len; i++) {
        out[i*2] = hex[(data[i] >> 4) & 0xF];
        out[i*2+1] = hex[data[i] & 0xF];
    }
    out[len*2] = '\0';
    return out;
}

/* ================================================================
 *  Constants
 * ================================================================ */

#define EMAIL_LINE_MAX       8192
#define EMAIL_BODY_MAX       65536
#define EMAIL_HEADER_MAX     16384
#define EMAIL_PATH_MAX       4096
#define EMAIL_ATTACH_MAX     64
#define EMAIL_MBOX_MAX       8
#define EMAIL_MAX_FAILURES   5
#define EMAIL_RETRY_DELAY    5
#define EMAIL_BACKOFF_DELAY  60
#define EMAIL_IMAP_TIMEOUT   30
#define EMAIL_IMAP_TAG_MAX   4  /* max tag number for auto-increment */

/* MIME boundary size (random hex) */
#define BOUNDARY_BYTES 16

/* ================================================================
 *  State
 * ================================================================ */

typedef struct {
    /* IMAP */
    char     imap_server[256];
    int      imap_port;
    char     imap_user[128];
    char     imap_pass[256];
    char     imap_mailbox[64];
    bool     imap_use_ssl;

    /* SMTP */
    char     smtp_server[256];
    int      smtp_port;
    char     smtp_user[128];
    char     smtp_pass[256];
    bool     smtp_use_ssl;

    /* Sendmail fallback */
    char     send_cmd[256];
    char     from[256];

    /* Maildir polling (legacy) */
    char     poll_dir[1024];
    int      poll_interval;

    /* Attachments */
    char     attach_dir[EMAIL_PATH_MAX];

    /* Runtime */
    bool     running;
    SSL_CTX *ssl_ctx;
    int      consecutive_failures;
    uint32_t last_uid;
    int      tag_counter;
    int      imap_sock;
    SSL     *imap_ssl;
    pthread_mutex_t imap_mutex;
    bool     imap_connected;
} email_state_t;

static email_state_t g_email;

/* ================================================================
 *  SSL initialization
 * ================================================================ */

static bool ssl_init_once(void) {
    if (g_email.ssl_ctx) return true;
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    const SSL_METHOD *method = TLS_client_method();
    g_email.ssl_ctx = SSL_CTX_new(method);
    if (!g_email.ssl_ctx) {
        fprintf(stderr, "[email] SSL_CTX_new failed\n");
        return false;
    }
    SSL_CTX_set_verify(g_email.ssl_ctx, SSL_VERIFY_NONE, NULL);
    SSL_CTX_set_mode(g_email.ssl_ctx, SSL_MODE_AUTO_RETRY);
    return true;
}

/* ================================================================
 *  Socket helpers
 * ================================================================ */

static int tcp_connect(const char *host, int port) {
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    struct addrinfo hints, *res, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int err = getaddrinfo(host, port_str, &hints, &res);
    if (err != 0) return -1;

    int fd = -1;
    for (rp = res; rp; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) continue;
        /* Set socket timeout */
        struct timeval tv = {EMAIL_IMAP_TIMEOUT, 0};
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0)
            break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    return fd;
}

/* Read a line (up to newline) from fd. Returns length, -1 on error. */
static int sock_read_line(int fd, char *buf, size_t sz) {
    size_t i = 0;
    while (i < sz - 1) {
        char c;
        ssize_t n = read(fd, &c, 1);
        if (n <= 0) return -1;
        if (c == '\r') continue; /* skip CR */
        if (c == '\n') break;
        buf[i++] = c;
    }
    buf[i] = '\0';
    return (int)i;
}

/* Read a line from SSL connection. Returns length, -1 on error. */
static int ssl_read_line(SSL *ssl, char *buf, size_t sz) {
    size_t i = 0;
    while (i < sz - 1) {
        char c;
        int n = SSL_read(ssl, &c, 1);
        if (n <= 0) return -1;
        if (c == '\r') continue;
        if (c == '\n') break;
        buf[i++] = c;
    }
    buf[i] = '\0';
    return (int)i;
}

/* Read until we see the tagged response. Accumulates into a dynamic buffer. */
static char *imap_read_response(SSL *ssl, const char *tag, int fd) {
    char line[EMAIL_LINE_MAX];
    size_t cap = 4096;
    size_t len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    buf[0] = '\0';

    int tag_len = (int)strlen(tag);

    while (1) {
        int n = ssl ? ssl_read_line(ssl, line, sizeof(line))
                     : sock_read_line(fd, line, sizeof(line));
        if (n < 0) {
            free(buf);
            return NULL;
        }

        size_t line_len = strlen(line);
        /* Append to buffer */
        if (len + line_len + 2 > cap) {
            cap *= 2;
            char *new_buf = realloc(buf, cap);
            if (!new_buf) { free(buf); return NULL; }
            buf = new_buf;
        }
        if (len > 0) {
            buf[len++] = '\n';
        }
        memcpy(buf + len, line, line_len);
        len += line_len;
        buf[len] = '\0';

        /* Check for tagged response */
        if (line_len >= (size_t)tag_len + 2 &&
            strncmp(line, tag, (size_t)tag_len) == 0 &&
            (line[tag_len] == ' ' || line[tag_len] == '\0')) {
            break;
        }

        /* Check for untagged OK/NO/BAD at start */
        if (line_len >= 3 && line[0] == '*' && line[1] == ' ') {
            /* Continuation for literal data — if line ends with '{N}', read N bytes */
            if (line_len > 2 && line[line_len - 1] == '}') {
                /* Find the opening brace */
                char *brace = strrchr(line, '{');
                if (brace) {
                    int lit_len = atoi(brace + 1);
                    if (lit_len > 0 && lit_len < 65536) {
                        /* Read literal bytes */
                        if (len + (size_t)lit_len + 2 > cap) {
                            cap = len + (size_t)lit_len + 64;
                            char *new_buf = realloc(buf, cap);
                            if (!new_buf) { free(buf); return NULL; }
                            buf = new_buf;
                        }
                        /* Read the CRLF after the literal size marker */
                        char crlf[4];
                        if (ssl) {
                            SSL_read(ssl, crlf, 2);
                        } else {
                            read(fd, crlf, 2);
                        }
                        /* Read literal data */
                        size_t offset = len;
                        while ((int)(len - offset) < lit_len) {
                            int chunk;
                            if (ssl) {
                                chunk = SSL_read(ssl, buf + len, lit_len - (int)(len - offset));
                            } else {
                                chunk = (int)read(fd, buf + len, (size_t)(lit_len - (int)(len - offset)));
                            }
                            if (chunk <= 0) break;
                            len += (size_t)chunk;
                        }
                        buf[len] = '\0';
                        /* Skip CRLF after literal */
                        if (ssl) {
                            SSL_read(ssl, crlf, 2);
                        } else {
                            read(fd, crlf, 2);
                        }
                    }
                }
            }
        }

        /* IDLE continuation: "+ idling" */
        if (line_len >= 1 && line[0] == '+') {
            continue;
        }
    }
    return buf;
}

/* Send a raw IMAP command */
static bool imap_send(SSL *ssl, const char *cmd, int fd) {
    size_t len = strlen(cmd);
    char send_buf[EMAIL_LINE_MAX];
    snprintf(send_buf, sizeof(send_buf), "%s\r\n", cmd);
    len = strlen(send_buf);
    int n;
    if (ssl) {
        n = SSL_write(ssl, send_buf, (int)len);
    } else {
        n = (int)write(fd, send_buf, len);
    }
    return n > 0;
}

/* Build a tagged IMAP command: "TAG1 COMMAND\r\n" */
static void imap_tagged_cmd(char *buf, size_t sz, const char *cmd_fmt, ...) {
    int tag = __sync_fetch_and_add(&g_email.tag_counter, 1) % 10000;
    char prefix[32];
    snprintf(prefix, sizeof(prefix), "A%04d ", tag);
    va_list args;
    va_start(args, cmd_fmt);
    vsnprintf(buf, sz, cmd_fmt, args);
    va_end(args);
    /* Prepend tag — shift content right with room for tag */
    size_t content_len = strlen(buf);
    size_t prefix_len = strlen(prefix);
    if (content_len + prefix_len + 3 > sz) content_len = sz - prefix_len - 3;
    memmove(buf + prefix_len, buf, content_len + 1);
    memcpy(buf, prefix, prefix_len);
}

/* ================================================================
 *  Standard base64 (RFC 2045 — with padding, + and / chars)
 *  Inline because libcrypto only has base64url variant.
 * ================================================================ */

static const char b64_std_tbl[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char *b64_encode(const unsigned char *data, size_t len) {
    if (!data && len > 0) return NULL;
    if (len == 0) { char *r = malloc(1); if (r) r[0] = '\0'; return r; }
    size_t out_len = ((len + 2) / 3) * 4 + 1;
    char *out = malloc(out_len);
    if (!out) return NULL;
    size_t i, o = 0;
    for (i = 0; i + 2 < len; i += 3) {
        unsigned int v = ((unsigned int)data[i] << 16) |
                         ((unsigned int)data[i+1] << 8) |
                         (unsigned int)data[i+2];
        out[o++] = b64_std_tbl[(v >> 18) & 0x3F];
        out[o++] = b64_std_tbl[(v >> 12) & 0x3F];
        out[o++] = b64_std_tbl[(v >> 6) & 0x3F];
        out[o++] = b64_std_tbl[v & 0x3F];
    }
    if (i < len) {
        unsigned int v = (unsigned int)data[i] << 16;
        if (i + 1 < len) v |= (unsigned int)data[i+1] << 8;
        out[o++] = b64_std_tbl[(v >> 18) & 0x3F];
        out[o++] = b64_std_tbl[(v >> 12) & 0x3F];
        if (i + 1 < len)
            out[o++] = b64_std_tbl[(v >> 6) & 0x3F];
        else
            out[o++] = '=';
        out[o++] = '=';
    }
    out[o] = '\0';
    return out;
}

static int b64_char_val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static unsigned char *b64_decode(const char *in, size_t *out_len) {
    if (!in || !out_len) return NULL;
    size_t in_len = strlen(in);
    /* Count padding */
    size_t pad = 0;
    if (in_len >= 1 && in[in_len-1] == '=') pad++;
    if (in_len >= 2 && in[in_len-2] == '=') pad++;
    size_t src_len = in_len - pad;
    /* Remove any whitespace/newlines */
    size_t clean_len = 0;
    for (size_t i = 0; i < in_len; i++) {
        char c = in[i];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != '=')
            clean_len++;
    }
    if (clean_len == 0) { *out_len = 0; return NULL; }

    size_t raw_len = (clean_len / 4) * 3;
    if (pad > 0) raw_len -= (3 - (clean_len % 4)) ? 0 : (pad == 2 ? 1 : 2);

    unsigned char *out = malloc(raw_len + 1);
    if (!out) return NULL;

    size_t o = 0;
    /* Process valid chars, skipping whitespace and padding */
    for (size_t i = 0; i < src_len; ) {
        int v[4];
        int n = 0;
        while (n < 4 && i < src_len) {
            char c = in[i++];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;
            int val = b64_char_val(c);
            if (val >= 0) v[n++] = val;
        }
        if (n < 2) break;
        unsigned int accum = 0;
        for (int j = 0; j < n; j++)
            accum = (accum << 6) | (unsigned int)v[j];
        if (n >= 2 && o < raw_len) out[o++] = (unsigned char)(accum >> 16);
        if (n >= 3 && o < raw_len) out[o++] = (unsigned char)(accum >> 8);
        if (n >= 4 && o < raw_len) out[o++] = (unsigned char)accum;
    }
    *out_len = o;
    out[o] = '\0';
    return out;
}

/* ================================================================
 *  MIME helpers
 * ================================================================ */

/* Find header value by name (case-insensitive) */
static const char *mime_header_find(const char *headers, const char *name) {
    if (!headers || !name) return NULL;
    size_t name_len = strlen(name);
    const char *p = headers;
    while (p && *p) {
        /* Line start — check if it begins with name */
        const char *colon = strchr(p, ':');
        if (colon && (size_t)(colon - p) == name_len) {
            bool match = true;
            for (size_t i = 0; i < name_len; i++) {
                if (tolower((unsigned char)p[i]) != tolower((unsigned char)name[i])) {
                    match = false;
                    break;
                }
            }
            if (match) {
                const char *val = colon + 1;
                while (*val == ' ' || *val == '\t') val++;
                return val;
            }
        }
        /* Next line */
        p = strchr(p, '\n');
        if (p) p++;
        /* Handle folded headers (continuation lines starting with space/tab) */
        while (p && (*p == ' ' || *p == '\t')) {
            p = strchr(p, '\n');
            if (p) p++;
        }
    }
    return NULL;
}

/* Extract a specific header value into a buffer */
static void mime_header_extract(const char *headers, const char *name,
                                 char *buf, size_t sz) {
    const char *val = mime_header_find(headers, name);
    if (val) {
        const char *end = strchr(val, '\n');
        if (end) {
            size_t len = (size_t)(end - val);
            if (len >= sz) len = sz - 1;
            memcpy(buf, val, len);
            buf[len] = '\0';
            /* Trim trailing whitespace */
            while (len > 0 && (buf[len-1] == ' ' || buf[len-1] == '\t' || buf[len-1] == '\r'))
                buf[--len] = '\0';
        } else {
            snprintf(buf, sz, "%s", val);
        }
        /* Trim trailing whitespace */
        size_t l = strlen(buf);
        while (l > 0 && (buf[l-1] == ' ' || buf[l-1] == '\t' || buf[l-1] == '\r'))
            buf[--l] = '\0';
    } else {
        buf[0] = '\0';
    }
}

/* Decode quoted-printable content into buffer */
static size_t qp_decode(const char *in, size_t in_len, char *out, size_t out_sz) {
    size_t w = 0;
    for (size_t i = 0; i < in_len && w < out_sz - 1; i++) {
        if (in[i] == '=' && i + 2 < in_len) {
            if (in[i+1] == '\n' || (in[i+1] == '\r' && i + 2 < in_len && in[i+2] == '\n')) {
                /* Soft line break — skip */
                i += (in[i+1] == '\r') ? 2 : 1;
                continue;
            }
            char hex[3] = {in[i+1], in[i+2], '\0'};
            char *endptr;
            long val = strtol(hex, &endptr, 16);
            if (endptr == hex + 2) {
                out[w++] = (char)val;
                i += 2;
            } else {
                out[w++] = in[i];
            }
        } else {
            out[w++] = in[i];
        }
    }
    out[w] = '\0';
    return w;
}

/* Decode base64 content into buffer (caller must free) */
static char *base64_decode_content(const char *in, size_t *out_len) {
    /* Remove any whitespace/newlines from the base64 data */
    size_t in_clean = 0;
    for (size_t i = 0; in[i]; i++) {
        if (in[i] != ' ' && in[i] != '\t' && in[i] != '\n' && in[i] != '\r')
            in_clean++;
    }
    char *clean = malloc(in_clean + 1);
    if (!clean) return NULL;
    size_t j = 0;
    for (size_t i = 0; in[i]; i++) {
        if (in[i] != ' ' && in[i] != '\t' && in[i] != '\n' && in[i] != '\r')
            clean[j++] = in[i];
    }
    clean[j] = '\0';

    unsigned char *decoded = b64_decode(clean, out_len);
    free(clean);
    if (!decoded) return NULL;
    char *result = malloc(*out_len + 1);
    if (!result) { free(decoded); return NULL; }
    memcpy(result, decoded, *out_len);
    result[*out_len] = '\0';
    free(decoded);
    return result;
}

/* Simple HTML-to-text conversion (strip tags, decode entities) */
static void html_to_text(const char *html, char *text, size_t text_sz) {
    if (!html || !text || text_sz == 0) return;
    size_t w = 0;
    bool in_tag = false;
    bool in_entity = false;
    char entity_buf[16];
    size_t entity_pos = 0;

    for (size_t i = 0; html[i] && w < text_sz - 1; i++) {
        char c = html[i];

        if (in_tag) {
            if (c == '>') in_tag = false;
            continue;
        }

        if (c == '<') {
            /* Check for </p>, </div>, <br>, <li>, etc. — add whitespace */
            const char *next = html + i + 1;
            if (*next == '/' || *next == 'p' || *next == 'd' || *next == 'b' ||
                *next == 'l' || *next == 'h' || *next == 't' || *next == 'c') {
                if (w > 0 && text[w-1] != ' ' && text[w-1] != '\n') {
                    text[w++] = ' ';
                }
            }
            in_tag = true;
            continue;
        }

        if (c == '&' && !in_entity) {
            in_entity = true;
            entity_pos = 0;
            entity_buf[entity_pos++] = c;
            continue;
        }

        if (in_entity) {
            if (entity_pos < sizeof(entity_buf) - 1)
                entity_buf[entity_pos++] = c;
            if (c == ';') {
                entity_buf[entity_pos] = '\0';
                in_entity = false;
                /* Decode common entities */
                if (strcmp(entity_buf, "&amp;") == 0) {
                    text[w++] = '&';
                } else if (strcmp(entity_buf, "&lt;") == 0) {
                    text[w++] = '<';
                } else if (strcmp(entity_buf, "&gt;") == 0) {
                    text[w++] = '>';
                } else if (strcmp(entity_buf, "&quot;") == 0) {
                    text[w++] = '"';
                } else if (strcmp(entity_buf, "&apos;") == 0) {
                    text[w++] = '\'';
                } else if (strcmp(entity_buf, "&nbsp;") == 0) {
                    text[w++] = ' ';
                } else if (strcmp(entity_buf, "&#10;") == 0) {
                    text[w++] = '\n';
                }
                continue;
            }
            if (entity_pos >= sizeof(entity_buf) - 1) {
                /* Overflow — flush as literal */
                for (size_t k = 0; k < entity_pos && w < text_sz - 1; k++)
                    text[w++] = entity_buf[k];
                in_entity = false;
            }
            continue;
        }

        /* Normal character */
        text[w++] = c;

        /* Heuristic: double-space after sentence-ending punctuation in HTML */
        if (c == '.' || c == '!' || c == '?') {
            if (w < text_sz - 1) text[w++] = ' ';
        }
    }

    /* Remove trailing double spaces and trim */
    while (w > 0 && (text[w-1] == ' ' || text[w-1] == '\n' || text[w-1] == '\r'))
        w--;
    text[w] = '\0';

    /* Collapse multiple spaces */
    size_t write = 0;
    bool prev_space = false;
    for (size_t i = 0; text[i]; i++) {
        if (text[i] == ' ' || text[i] == '\n') {
            if (!prev_space) {
                text[write++] = text[i];
                prev_space = true;
            }
        } else {
            text[write++] = text[i];
            prev_space = false;
        }
    }
    text[write] = '\0';
}

/* Generate a random MIME boundary string */
static void mime_boundary_generate(char *buf, size_t sz) {
    unsigned char rand_bytes[BOUNDARY_BYTES];
    if (RAND_bytes(rand_bytes, BOUNDARY_BYTES) <= 0) {
        /* Fallback: use time-based */
        snprintf(buf, sz, "=_Hermes_%lx_%lx", (unsigned long)time(NULL),
                 (unsigned long)rand());
        return;
    }
    char *encoded = b64_encode(rand_bytes, BOUNDARY_BYTES);
    if (encoded) {
        snprintf(buf, sz, "=_Hermes_%s", encoded);
        free(encoded);
    } else {
        snprintf(buf, sz, "=_Hermes_%lx", (unsigned long)time(NULL));
    }
}

/* ================================================================
 *  IMAP protocol helpers
 * ================================================================ */

/* Connect and login to IMAP server. Returns true on success. */
static bool imap_connect_and_login(void) {
    pthread_mutex_lock(&g_email.imap_mutex);

    /* Close any existing connection */
    if (g_email.imap_ssl) {
        SSL_shutdown(g_email.imap_ssl);
        SSL_free(g_email.imap_ssl);
        g_email.imap_ssl = NULL;
    }
    if (g_email.imap_sock >= 0) {
        close(g_email.imap_sock);
        g_email.imap_sock = -1;
    }

    if (!ssl_init_once()) {
        pthread_mutex_unlock(&g_email.imap_mutex);
        return false;
    }

    int port = g_email.imap_port;
    bool use_ssl = g_email.imap_use_ssl;
    if (port == 0) port = use_ssl ? 993 : 143;

    int fd = tcp_connect(g_email.imap_server, port);
    if (fd < 0) {
        fprintf(stderr, "[email] IMAP connect failed to %s:%d\n",
                g_email.imap_server, port);
        pthread_mutex_unlock(&g_email.imap_mutex);
        return false;
    }

    SSL *ssl = NULL;
    if (use_ssl) {
        ssl = SSL_new(g_email.ssl_ctx);
        if (!ssl) {
            close(fd);
            pthread_mutex_unlock(&g_email.imap_mutex);
            return false;
        }
        SSL_set_fd(ssl, fd);
        if (SSL_connect(ssl) <= 0) {
            SSL_free(ssl);
            close(fd);
            pthread_mutex_unlock(&g_email.imap_mutex);
            return false;
        }
    }

    g_email.imap_sock = fd;
    g_email.imap_ssl = ssl;
    g_email.imap_connected = true;

    /* Read greeting */
    char greeting[EMAIL_LINE_MAX];
    int n = ssl ? ssl_read_line(ssl, greeting, sizeof(greeting))
                 : sock_read_line(fd, greeting, sizeof(greeting));
    if (n < 0) {
        fprintf(stderr, "[email] No IMAP greeting\n");
        goto fail;
    }

    /* LOGIN */
    char tag_cmd[EMAIL_LINE_MAX];
    imap_tagged_cmd(tag_cmd, sizeof(tag_cmd),
        "LOGIN \"%s\" \"%s\"", g_email.imap_user, g_email.imap_pass);

    /* Escape special chars in username/password */
    /* For now, simple quoting works for most cases */

    if (!imap_send(ssl, tag_cmd, fd)) goto fail;

    char *resp = imap_read_response(ssl, tag_cmd, fd);
    if (!resp) goto fail;

    bool ok = (strstr(resp, "OK") != NULL);
    free(resp);
    if (!ok) {
        fprintf(stderr, "[email] IMAP LOGIN failed\n");
        goto fail;
    }

    /* SELECT mailbox */
    imap_tagged_cmd(tag_cmd, sizeof(tag_cmd),
        "SELECT \"%s\"", g_email.imap_mailbox);
    if (!imap_send(ssl, tag_cmd, fd)) goto fail;

    resp = imap_read_response(ssl, tag_cmd, fd);
    if (!resp) goto fail;

    ok = (strstr(resp, "OK") != NULL);
    /* Try to extract UIDVALIDITY / UIDNEXT from the response */
    const char *uidnext = strstr(resp, "UIDNEXT");
    if (uidnext) {
        uidnext += 8;
        while (*uidnext == ' ') uidnext++;
        g_email.last_uid = (uint32_t)atol(uidnext);
        if (g_email.last_uid > 0) g_email.last_uid--;
    }
    free(resp);

    if (!ok) {
        fprintf(stderr, "[email] IMAP SELECT failed\n");
        goto fail;
    }

    printf("[email] IMAP connected to %s:%d as %s, mailbox %s\n",
           g_email.imap_server, port, g_email.imap_user, g_email.imap_mailbox);

    pthread_mutex_unlock(&g_email.imap_mutex);
    return true;

fail:
    if (g_email.imap_ssl) {
        SSL_shutdown(g_email.imap_ssl);
        SSL_free(g_email.imap_ssl);
        g_email.imap_ssl = NULL;
    }
    if (g_email.imap_sock >= 0) {
        close(g_email.imap_sock);
        g_email.imap_sock = -1;
    }
    g_email.imap_connected = false;
    pthread_mutex_unlock(&g_email.imap_mutex);
    return false;
}

/* ================================================================
 *  IMAP message fetch and parse
 * ================================================================ */

/* Parse a raw email into a json_node_t update.
 * The update will have:
 *   chat_id   — the From address
 *   text      — plain text body (HTML stripped)
 *   html      — HTML body (if any)
 *   subject   — email subject
 *   in_reply_to — Message-ID being replied to (for threading)
 *   message_id  — this email's Message-ID
 *   references — References header
 *   attachments — array of {filename, mime_type, path}
 */
static json_node_t *parse_raw_email(const char *raw, size_t raw_len) {
    json_node_t *update = json_object();
    if (!update) return NULL;

    char headers[EMAIL_HEADER_MAX] = {0};
    const char *body_start = NULL;
    size_t body_len = 0;

    /* Split headers from body at first blank line */
    const char *blank = strstr(raw, "\r\n\r\n");
    if (!blank) blank = strstr(raw, "\n\n");
    if (blank) {
        size_t header_end = (size_t)(blank - raw);
        if (header_end >= sizeof(headers)) header_end = sizeof(headers) - 1;
        memcpy(headers, raw, header_end);
        headers[header_end] = '\0';
        body_start = blank;
        /* Skip the blank line separator */
        if (*body_start == '\r') body_start += 2;
        else body_start += 1;
        if (*body_start == '\n') body_start += 1;
        body_len = raw_len - (size_t)(body_start - raw);
    } else {
        /* No body */
        size_t hdr_len = raw_len < sizeof(headers) - 1 ? raw_len : sizeof(headers) - 1;
        memcpy(headers, raw, hdr_len);
        headers[hdr_len] = '\0';
    }

    /* Extract headers */
    char from[512] = "", subject[512] = "", msg_id[256] = "", in_reply_to[512] = "";
    char references[1024] = "";
    mime_header_extract(headers, "From", from, sizeof(from));
    mime_header_extract(headers, "Subject", subject, sizeof(subject));
    mime_header_extract(headers, "Message-ID", msg_id, sizeof(msg_id));
    mime_header_extract(headers, "In-Reply-To", in_reply_to, sizeof(in_reply_to));
    mime_header_extract(headers, "References", references, sizeof(references));

    /* Message-ID is kept with angle brackets for RFC compliance */

    json_set(update, "chat_id", json_string(from));
    json_set(update, "subject", json_string(subject));
    if (*msg_id) json_set(update, "message_id", json_string(msg_id));
    if (*in_reply_to) json_set(update, "in_reply_to", json_string(in_reply_to));
    if (*references) json_set(update, "references", json_string(references));

    /* Determine content type and parse body */
    char ct[256] = "text/plain";
    mime_header_extract(headers, "Content-Type", ct, sizeof(ct));

    char text_body[EMAIL_BODY_MAX] = "";
    char html_body[EMAIL_BODY_MAX] = "";
    char encoding[64] = "";
    mime_header_extract(headers, "Content-Transfer-Encoding", encoding, sizeof(encoding));
    json_node_t *attachments = json_array();

    /* Check for multipart */
    if (strncasecmp(ct, "multipart/", 10) == 0) {
        /* Extract boundary */
        const char *boundary_str = strstr(ct, "boundary=");
        if (boundary_str) {
            boundary_str += 9;
            if (*boundary_str == '"') boundary_str++;
            char boundary[128];
            int bi = 0;
            while (*boundary_str && *boundary_str != '"' && *boundary_str != ';' &&
                   *boundary_str != ' ' && *boundary_str != '\t' && *boundary_str != '\r' &&
                   *boundary_str != '\n' && bi < 126) {
                boundary[bi++] = *boundary_str++;
            }
            boundary[bi] = '\0';

            if (*boundary) {
                /* Parse multipart body */
                char boundary_delim[144];
                snprintf(boundary_delim, sizeof(boundary_delim), "--%s", boundary);
                char boundary_end[144];
                snprintf(boundary_end, sizeof(boundary_end), "--%s--", boundary);

                const char *part_start = strstr(body_start, boundary_delim);
                if (part_start) {
                    part_start += strlen(boundary_delim);
                    /* Skip the CRLF after delimiter */
                    if (*part_start == '\r') part_start++;
                    if (*part_start == '\n') part_start++;

                    while (part_start && *part_start) {
                        /* Find the end of this part */
                        const char *part_end = strstr(part_start, "\r\n--") ;
                        if (!part_end) part_end = strstr(part_start, "\n--");
                        if (!part_end) break;

                        const char *next_delim = part_end;
                        /* Skip past "--" */
                        if (*next_delim == '\r') next_delim += 2;
                        else next_delim += 1;
                        /* next_delim now points at "--boundary..." */

                        size_t part_len = (size_t)(part_end - part_start);
                        if (part_len == 0) break;
                        if (part_len > EMAIL_BODY_MAX) part_len = EMAIL_BODY_MAX;

                        /* Check if this is the end boundary */
                        if (strncmp(next_delim, "--", 2) == 0 &&
                            strncmp(next_delim + 2, boundary, strlen(boundary)) == 0 &&
                            strncmp(next_delim + 2 + strlen(boundary), "--", 2) == 0) {
                            /* End marker */
                            break;
                        }

                        /* Parse this part's headers */
                        char part_headers[EMAIL_HEADER_MAX];
                        size_t ph_sz = 0;
                        const char *part_body = part_start;
                        size_t part_body_len = part_len;

                        /* Find blank line separating part headers from body */
                        const char *part_blank = NULL;
                        const char *scan = part_start;
                        const char *scan_end = part_start + part_len;
                        while (scan < scan_end) {
                            if ((scan_end - scan >= 4 && memcmp(scan, "\r\n\r\n", 4) == 0) ||
                                (scan_end - scan >= 2 && memcmp(scan, "\n\n", 2) == 0)) {
                                part_blank = scan;
                                break;
                            }
                            scan++;
                        }

                        if (part_blank) {
                            /* Calculate header length */
                            size_t hdr_len = (size_t)(part_blank - part_start);
                            if (hdr_len < sizeof(part_headers)) {
                                memcpy(part_headers, part_start, hdr_len);
                                part_headers[hdr_len] = '\0';
                                ph_sz = hdr_len;
                            } else {
                                ph_sz = sizeof(part_headers) - 1;
                                memcpy(part_headers, part_start, ph_sz);
                                part_headers[ph_sz] = '\0';
                            }
                            /* Calculate body */
                            part_body = part_blank;
                            if (part_body_len >= 4 && memcmp(part_body, "\r\n\r\n", 4) == 0)
                                part_body += 4;
                            else if (part_body_len >= 2 && memcmp(part_body, "\n\n", 2) == 0)
                                part_body += 2;
                            part_body_len = part_len - (size_t)(part_body - part_start);
                        }

                        /* Get Content-Type of this part */
                        char part_ct[256] = "text/plain";
                        mime_header_extract(part_headers, "Content-Type",
                                            part_ct, sizeof(part_ct));

                        char part_enc[64] = "";
                        mime_header_extract(part_headers, "Content-Transfer-Encoding",
                                            part_enc, sizeof(part_enc));

                        char part_disposition[256] = "";
                        mime_header_extract(part_headers, "Content-Disposition",
                                            part_disposition, sizeof(part_disposition));

                        /* Decode body */
                        char decoded_body[EMAIL_BODY_MAX] = "";
                        size_t decoded_len = 0;

                        if (strncasecmp(part_enc, "base64", 6) == 0) {
                            size_t b64len = 0;
                            char *b64decoded = base64_decode_content(part_body, &b64len);
                            if (b64decoded) {
                                if (b64len >= sizeof(decoded_body))
                                    b64len = sizeof(decoded_body) - 1;
                                memcpy(decoded_body, b64decoded, b64len);
                                decoded_body[b64len] = '\0';
                                decoded_len = b64len;
                                free(b64decoded);
                            }
                        } else if (strncasecmp(part_enc, "quoted-printable", 16) == 0) {
                            decoded_len = qp_decode(part_body, part_body_len,
                                                     decoded_body, sizeof(decoded_body));
                        } else {
                            size_t copy_len = part_body_len;
                            if (copy_len >= sizeof(decoded_body))
                                copy_len = sizeof(decoded_body) - 1;
                            memcpy(decoded_body, part_body, copy_len);
                            decoded_body[copy_len] = '\0';
                            decoded_len = copy_len;
                        }

                        /* Check if it's an attachment */
                        bool is_attachment = (strstr(part_disposition, "attachment") != NULL);
                        bool is_inline = (strstr(part_disposition, "inline") != NULL);

                        if (is_attachment) {
                            /* Save attachment to disk */
                            char filename[256] = "attachment.bin";
                            /* Extract filename from Content-Disposition or Content-Type */
                            const char *name_val = strstr(part_disposition, "filename=");
                            if (name_val) {
                                name_val += 9;
                                if (*name_val == '"') {
                                    name_val++;
                                    int fi = 0;
                                    while (*name_val && *name_val != '"' && fi < 254)
                                        filename[fi++] = *name_val++;
                                    filename[fi] = '\0';
                                } else {
                                    int fi = 0;
                                    while (*name_val && *name_val != ';' &&
                                           *name_val != ' ' && fi < 254)
                                        filename[fi++] = *name_val++;
                                    filename[fi] = '\0';
                                }
                            } else {
                                /* Try name= from Content-Type */
                                name_val = strstr(part_ct, "name=");
                                if (name_val) {
                                    name_val += 5;
                                    if (*name_val == '"') name_val++;
                                    int fi = 0;
                                    while (*name_val && *name_val != '"' &&
                                           *name_val != ';' && fi < 254)
                                        filename[fi++] = *name_val++;
                                    filename[fi] = '\0';
                                }
                            }

                            /* Determine MIME type for attachment metadata */
                            char part_mime[128] = "application/octet-stream";
                            mime_header_extract(part_headers, "Content-Type",
                                                part_mime, sizeof(part_mime));
                            /* Strip parameters from mime type */
                            char *semicol = strchr(part_mime, ';');
                            if (semicol) *semicol = '\0';
                            /* Trim */
                            while (strlen(part_mime) > 0 &&
                                   (part_mime[strlen(part_mime)-1] == ' ' ||
                                    part_mime[strlen(part_mime)-1] == '\t'))
                                part_mime[strlen(part_mime)-1] = '\0';

                            /* Save to attach_dir */
                            char save_path[EMAIL_PATH_MAX];
                            snprintf(save_path, sizeof(save_path), "%s/%s",
                                     g_email.attach_dir, filename);

                            /* Ensure directory exists */
                            mkdir(g_email.attach_dir, 0755);

                            /* Avoid overwriting: append number if exists */
                            if (access(save_path, F_OK) == 0) {
                                char base[256], ext[64];
                                char *dot = strrchr(filename, '.');
                                if (dot) {
                                    snprintf(ext, sizeof(ext), "%s", dot);
                                    size_t base_len = (size_t)(dot - filename);
                                    memcpy(base, filename, base_len);
                                    base[base_len] = '\0';
                                } else {
                                    snprintf(base, sizeof(base), "%s", filename);
                                    ext[0] = '\0';
                                }
                                for (int n = 1; n < 1000; n++) {
                                    snprintf(save_path, sizeof(save_path), "%s/%s_%d%s",
                                             g_email.attach_dir, base, n, ext);
                                    if (access(save_path, F_OK) != 0) break;
                                }
                            }

                            FILE *af = fopen(save_path, "wb");
                            if (af) {
                                fwrite(decoded_body, 1, decoded_len, af);
                                fclose(af);
                            }

                            /* Add to attachments JSON */
                            json_node_t *att = json_object();
                            json_set(att, "filename", json_string(filename));
                            json_set(att, "mime_type", json_string(part_mime));
                            json_set(att, "path", json_string(save_path));
                            json_set(att, "size", json_number((double)decoded_len));
                            json_append(attachments, att);

                        } else if (strncasecmp(part_ct, "text/plain", 10) == 0) {
                            if (!*text_body) {
                                snprintf(text_body, sizeof(text_body), "%s", decoded_body);
                            }
                        } else if (strncasecmp(part_ct, "text/html", 9) == 0) {
                            if (!*html_body) {
                                snprintf(html_body, sizeof(html_body), "%s", decoded_body);
                            }
                        } else if (strncasecmp(part_ct, "multipart/", 10) == 0) {
                            /* Nested multipart — for now, recursively get first text/html part */
                            /* This is a simplification; fully recursive would need proper MIME parser */
                            if (!*text_body || !*html_body) {
                                /* Use the decoded body as-is if it's nested text */
                            }
                        }

                        /* Move to next part */
                        part_start = next_delim;
                        if (*part_start == '\n') part_start++;
                        if (*part_start == '-') part_start += 2; /* skip -- */
                    }
                }
            }
        }
    } else if (strncasecmp(ct, "text/html", 9) == 0) {
        /* HTML-only email */
        if (strncasecmp(encoding, "base64", 6) == 0) {
            size_t b64len = 0;
            char *decoded = base64_decode_content(body_start, &b64len);
            if (decoded) {
                if (b64len >= sizeof(html_body)) b64len = sizeof(html_body) - 1;
                memcpy(html_body, decoded, b64len);
                html_body[b64len] = '\0';
                free(decoded);
            }
        } else if (strncasecmp(encoding, "quoted-printable", 16) == 0) {
            qp_decode(body_start, body_len, html_body, sizeof(html_body));
        } else {
            size_t copy_len = body_len;
            if (copy_len >= sizeof(html_body)) copy_len = sizeof(html_body) - 1;
            memcpy(html_body, body_start, copy_len);
            html_body[copy_len] = '\0';
        }
        /* Strip HTML to get plain text */
        html_to_text(html_body, text_body, sizeof(text_body));
    } else {
        /* Plain text (or other) */
        if (strncasecmp(encoding, "base64", 6) == 0) {
            size_t b64len = 0;
            char *decoded = base64_decode_content(body_start, &b64len);
            if (decoded) {
                if (b64len >= sizeof(text_body)) b64len = sizeof(text_body) - 1;
                memcpy(text_body, decoded, b64len);
                text_body[b64len] = '\0';
                free(decoded);
            }
        } else if (strncasecmp(encoding, "quoted-printable", 16) == 0) {
            qp_decode(body_start, body_len, text_body, sizeof(text_body));
        } else {
            size_t copy_len = body_len;
            if (copy_len >= sizeof(text_body)) copy_len = sizeof(text_body) - 1;
            memcpy(text_body, body_start, copy_len);
            text_body[copy_len] = '\0';
        }
    }

    json_set(update, "text", json_string(*text_body ? text_body : "(no text body)"));
    if (*html_body)
        json_set(update, "html", json_string(html_body));

    if (json_len(attachments) > 0)
        json_set(update, "attachments", attachments);
    else
        json_free(attachments);

    return update;
}

/* Fetch a single message by UID and parse it */
static json_node_t *imap_fetch_message(uint32_t uid) {
    pthread_mutex_lock(&g_email.imap_mutex);

    if (!g_email.imap_connected) {
        pthread_mutex_unlock(&g_email.imap_mutex);
        return NULL;
    }

    char tag_cmd[EMAIL_LINE_MAX];
    /* FETCH with BODY[] to get full message */
    imap_tagged_cmd(tag_cmd, sizeof(tag_cmd),
        "UID FETCH %u (BODY[])", uid);

    SSL *ssl = g_email.imap_ssl;
    int fd = g_email.imap_sock;

    if (!imap_send(ssl, tag_cmd, fd)) {
        pthread_mutex_unlock(&g_email.imap_mutex);
        return NULL;
    }

    char *resp = imap_read_response(ssl, tag_cmd, fd);
    if (!resp) {
        pthread_mutex_unlock(&g_email.imap_mutex);
        return NULL;
    }

    json_node_t *update = NULL;

    /* Find the BODY[] content in the response */
    /* Typical response:
     * * 1 FETCH (UID 123 BODY[] {size}
     * ...raw email content...
     * )
     * A0001 OK FETCH completed
     */
    const char *body_marker = strstr(resp, "BODY[");
    if (body_marker) {
        /* Find the literal size */
        const char *brace = strchr(body_marker, '{');
        if (brace) {
            int body_size = atoi(brace + 1);
            if (body_size > 0 && body_size < (int)sizeof(EMAIL_BODY_MAX) * 4) {
                /* Skip to after {size}CRLF */
                const char *content_start = strstr(brace, "\n");
                if (content_start) {
                    content_start++;
                    /* Found it — but the actual content was already consumed by
                     * imap_read_response via the literal handling. We need to
                     * find it in the response string.
                     * The literal data was appended after the {size} marker line. */
                    const char *data_start = brace;
                    /* Find \n after {size} */
                    data_start = strchr(brace, '\n');
                    if (data_start) {
                        data_start++;
                        /* The data starts here and should be body_size bytes */
                        size_t raw_len = (size_t)body_size;
                        if (data_start + raw_len <= resp + strlen(resp)) {
                            update = parse_raw_email(data_start, raw_len);
                        }
                    }
                }
            }
        }
    }

    free(resp);
    pthread_mutex_unlock(&g_email.imap_mutex);
    return update;
}

/* ================================================================
 *  IMAP IDLE loop
 * ================================================================ */

static void *thread_imap_idle(void *arg) {
    (void)arg;
    printf("[email] IMAP IDLE loop started\n");

    while (g_email.running) {
        if (!g_email.imap_connected) {
            if (!imap_connect_and_login()) {
                g_email.consecutive_failures++;
                int delay = g_email.consecutive_failures >= EMAIL_MAX_FAILURES
                    ? EMAIL_BACKOFF_DELAY : EMAIL_RETRY_DELAY;
                printf("[email] IMAP connect failed, retrying in %ds (attempt %d)\n",
                       delay, g_email.consecutive_failures);
                for (int i = 0; i < delay && g_email.running; i++) sleep(1);
                continue;
            }
            g_email.consecutive_failures = 0;
        }

        /* Send IDLE */
        pthread_mutex_lock(&g_email.imap_mutex);
        SSL *ssl = g_email.imap_ssl;
        int fd = g_email.imap_sock;

        if (!imap_send(ssl, "IDLE", fd)) {
            g_email.imap_connected = false;
            pthread_mutex_unlock(&g_email.imap_mutex);
            continue;
        }

        /* Read IDLE continuation: "+ idling" */
        char line[EMAIL_LINE_MAX];
        int n = ssl_read_line(ssl, line, sizeof(line));
        if (n < 0 || line[0] != '+') {
            g_email.imap_connected = false;
            pthread_mutex_unlock(&g_email.imap_mutex);
            continue;
        }
        pthread_mutex_unlock(&g_email.imap_mutex);

        printf("[email] IDLE established\n");

        /* Wait for notifications (non-blocking with select) */
        bool idle_active = true;
        while (idle_active && g_email.running) {
            struct timeval tv;
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(fd, &rfds);
            tv.tv_sec = 5 * 60;  /* 5 minute timeout — need to re-IDLE */
            tv.tv_usec = 0;

            int ret = select(fd + 1, &rfds, NULL, NULL, &tv);
            if (ret < 0) {
                if (errno == EINTR) continue;
                idle_active = false;
                break;
            }

            if (ret == 0) {
                /* Timeout — send DONE and re-IDLE */
                pthread_mutex_lock(&g_email.imap_mutex);
                imap_send(ssl, "DONE", fd);
                /* Read the untagged response */
                while (1) {
                    n = ssl_read_line(ssl, line, sizeof(line));
                    if (n < 0) { idle_active = false; break; }
                    if (strncmp(line, "A", 1) == 0 &&
                        (strstr(line, "OK") || strstr(line, "BAD")))
                        break;
                }
                pthread_mutex_unlock(&g_email.imap_mutex);
                if (idle_active) {
                    printf("[email] IDLE timeout, re-IDLE\n");
                }
                break; /* Break inner loop to re-IDLE */
            }

            /* Data available — read the notification */
            pthread_mutex_lock(&g_email.imap_mutex);
            n = ssl_read_line(ssl, line, sizeof(line));
            if (n < 0) {
                g_email.imap_connected = false;
                pthread_mutex_unlock(&g_email.imap_mutex);
                idle_active = false;
                break;
            }

            /* Check for EXISTS — new mail */
            if (strstr(line, "EXISTS") != NULL) {
                /* New message arrived. Send DONE to get out of IDLE. */
                imap_send(ssl, "DONE", fd);

                /* Read the response to DONE */
                while (1) {
                    n = ssl_read_line(ssl, line, sizeof(line));
                    if (n < 0) { idle_active = false; break; }
                    if (strncmp(line, "A", 1) == 0 &&
                        (strstr(line, "OK") || strstr(line, "BAD")))
                        break;
                }
                pthread_mutex_unlock(&g_email.imap_mutex);
                idle_active = false;

                if (!g_email.running) break;

                /* Fetch new messages */
                printf("[email] New mail detected, fetching...\n");

                /* Get UIDNEXT or UID range */
                uint32_t new_uid = g_email.last_uid + 1;

                /* Actually, let's do a UID SEARCH to find unseen messages */
                pthread_mutex_lock(&g_email.imap_mutex);
                char tag[32];
                snprintf(tag, sizeof(tag), "A%04d",
                         __sync_fetch_and_add(&g_email.tag_counter, 1) % 10000);
                char search_cmd[EMAIL_LINE_MAX];
                snprintf(search_cmd, sizeof(search_cmd),
                         "%s UID SEARCH UNSEEN", tag);
                if (!imap_send(ssl, search_cmd, fd)) {
                    g_email.imap_connected = false;
                    pthread_mutex_unlock(&g_email.imap_mutex);
                    break;
                }
                char *search_resp = imap_read_response(ssl, tag, fd);
                pthread_mutex_unlock(&g_email.imap_mutex);

                if (search_resp) {
                    /* Parse UID list: * SEARCH 123 124 125 */
                    const char *s = strstr(search_resp, "SEARCH");
                    if (s) {
                        s += 6;
                        while (*s && (*s == ' ' || *s == '\t')) s++;

                        int uid_val;
                        int offset = 0;
                        while (sscanf(s, "%d%n", &uid_val, &offset) > 0) {
                            s += offset;
                            uint32_t uid = (uint32_t)uid_val;

                            if (uid > g_email.last_uid) {
                                printf("[email] Fetching UID %u\n", uid);
                                json_node_t *update = imap_fetch_message(uid);
                                if (update) {
                                    /* Queue the update through the gateway */
                                    const char *chat_id = json_get_str(update, "chat_id", "");
                                    const char *text = json_get_str(update, "text", "");

                                    extern gateway_state_t g_gw;
                                    printf("[email] Message from %s: %.200s\n",
                                           chat_id, text);

                                    char *resp = agent_chat(&g_gw.agent, text);
                                    if (resp) {
                                        /* Send reply via email */
                                        json_node_t *attachments = json_obj_get(update, "attachments");
                                        const char *subject = json_get_str(update, "subject", "Re: Hermes");
                                        const char *in_reply_to = json_get_str(update, "in_reply_to", "");

                                        /* Build reply subject */
                                        char reply_subject[512];
                                        if (strncasecmp(subject, "Re:", 3) != 0 &&
                                            strncasecmp(subject, "RE:", 3) != 0)
                                            snprintf(reply_subject, sizeof(reply_subject),
                                                     "Re: %s", subject);
                                        else
                                            snprintf(reply_subject, sizeof(reply_subject),
                                                     "%s", subject);

                                        /* Build reply body with threading */
                                        email_send_message_ext(chat_id, reply_subject,
                                                                resp, NULL, NULL,
                                                                in_reply_to);

                                        free(resp);
                                    }

                                    json_free(update);
                                }
                                g_email.last_uid = uid;
                            }
                            while (*s == ' ') s++;
                        }
                    }
                    free(search_resp);
                }
                break; /* Exit inner loop, will re-IDLE */
            }

            pthread_mutex_unlock(&g_email.imap_mutex);
        }
    }

    /* Cleanup */
    pthread_mutex_lock(&g_email.imap_mutex);
    if (g_email.imap_ssl) {
        imap_send(g_email.imap_ssl, "LOGOUT", g_email.imap_sock);
        SSL_shutdown(g_email.imap_ssl);
        SSL_free(g_email.imap_ssl);
        g_email.imap_ssl = NULL;
    }
    if (g_email.imap_sock >= 0) {
        close(g_email.imap_sock);
        g_email.imap_sock = -1;
    }
    g_email.imap_connected = false;
    pthread_mutex_unlock(&g_email.imap_mutex);

    printf("[email] IMAP IDLE loop stopped\n");
    return NULL;
}

/* ================================================================
 *  SMTP sending
 * ================================================================ */

/* Send raw SMTP command and read response */
static int smtp_cmd(SSL *ssl, int fd, const char *cmd, char *resp, size_t resp_sz) {
    if (cmd) {
        char send_buf[EMAIL_LINE_MAX];
        snprintf(send_buf, sizeof(send_buf), "%s\r\n", cmd);
        int n;
        if (ssl) n = SSL_write(ssl, send_buf, (int)strlen(send_buf));
        else n = (int)write(fd, send_buf, strlen(send_buf));
        if (n <= 0) return -1;
    }

    if (resp && resp_sz > 0) {
        int n;
        if (ssl) n = ssl_read_line(ssl, resp, resp_sz);
        else n = sock_read_line(fd, resp, resp_sz);
        if (n < 0) return -1;
        /* Check for multi-line response */
        while (resp[0] >= '0' && resp[0] <= '9' && resp[3] == '-') {
            char line[EMAIL_LINE_MAX];
            if (ssl) ssl_read_line(ssl, line, sizeof(line));
            else sock_read_line(fd, line, sizeof(line));
        }
        return (resp[0] >= '1' && resp[0] <= '9') ? atoi(resp) : -1;
    }
    return 0;
}

/* Send email via SMTP */
static bool smtp_send(const char *from, const char *to,
                       const char *raw_email) {
    if (!g_email.smtp_server[0]) return false;

    int port = g_email.smtp_port;
    if (port == 0) port = g_email.smtp_use_ssl ? 465 : 587;

    int fd = tcp_connect(g_email.smtp_server, port);
    if (fd < 0) {
        fprintf(stderr, "[email] SMTP connect failed to %s:%d\n",
                g_email.smtp_server, port);
        return false;
    }

    SSL *ssl = NULL;
    if (g_email.smtp_use_ssl) {
        ssl = SSL_new(g_email.ssl_ctx);
        if (!ssl) { close(fd); return false; }
        SSL_set_fd(ssl, fd);
        if (SSL_connect(ssl) <= 0) {
            SSL_free(ssl);
            close(fd);
            return false;
        }
    }

    char resp[EMAIL_LINE_MAX];
    int code;

    /* Read greeting */
    code = smtp_cmd(ssl, fd, NULL, resp, sizeof(resp));
    if (code < 0 || code >= 500) goto smtp_fail;

    /* EHLO */
    char ehlo_cmd[256];
    snprintf(ehlo_cmd, sizeof(ehlo_cmd), "EHLO hermes");
    code = smtp_cmd(ssl, fd, ehlo_cmd, resp, sizeof(resp));
    if (code < 0) goto smtp_fail;

    /* STARTTLS if not already SSL and server supports it */
    if (!g_email.smtp_use_ssl && strstr(resp, "STARTTLS")) {
        code = smtp_cmd(ssl, fd, "STARTTLS", resp, sizeof(resp));
        if (code >= 200 && code < 400) {
            /* Upgrade connection to TLS */
            SSL *new_ssl = SSL_new(g_email.ssl_ctx);
            if (new_ssl) {
                SSL_set_fd(new_ssl, fd);
                if (SSL_connect(new_ssl) > 0) {
                    ssl = new_ssl;
                    /* Re-EHLO after STARTTLS */
                    code = smtp_cmd(ssl, fd, ehlo_cmd, resp, sizeof(resp));
                    if (code < 0) goto smtp_fail;
                } else {
                    SSL_free(new_ssl);
                }
            }
        }
    }

    /* AUTH LOGIN if credentials provided */
    if (g_email.smtp_user[0] && g_email.smtp_pass[0]) {
        if (strstr(resp, "AUTH") && strstr(resp, "LOGIN")) {
            code = smtp_cmd(ssl, fd, "AUTH LOGIN", resp, sizeof(resp));
            if (code >= 300 && code < 400) {
                /* Send username (base64-encoded) */
                char *user_b64 = b64_encode(
                    (const unsigned char *)g_email.smtp_user,
                    strlen(g_email.smtp_user));
                if (user_b64) {
                    code = smtp_cmd(ssl, fd, user_b64, resp, sizeof(resp));
                    free(user_b64);
                    if (code >= 300 && code < 400) {
                        char *pass_b64 = b64_encode(
                            (const unsigned char *)g_email.smtp_pass,
                            strlen(g_email.smtp_pass));
                        if (pass_b64) {
                            code = smtp_cmd(ssl, fd, pass_b64, resp, sizeof(resp));
                            free(pass_b64);
                        }
                    }
                }
            }
        }
    }

    /* MAIL FROM */
    char mail_from[512];
    snprintf(mail_from, sizeof(mail_from), "MAIL FROM:<%s>", from);
    code = smtp_cmd(ssl, fd, mail_from, resp, sizeof(resp));
    if (code < 0 || code >= 500) goto smtp_fail;

    /* RCPT TO */
    char rcpt_to[512];
    snprintf(rcpt_to, sizeof(rcpt_to), "RCPT TO:<%s>", to);
    code = smtp_cmd(ssl, fd, rcpt_to, resp, sizeof(resp));
    if (code < 0 || code >= 500) goto smtp_fail;

    /* DATA */
    code = smtp_cmd(ssl, fd, "DATA", resp, sizeof(resp));
    if (code < 0 || code >= 400) goto smtp_fail;

    /* Send email body (already formatted with headers) */
    size_t len = strlen(raw_email);
    size_t sent = 0;
    while (sent < len) {
        int n;
        if (ssl)
            n = SSL_write(ssl, raw_email + sent, (int)(len - sent));
        else
            n = (int)write(fd, raw_email + sent, len - sent);
        if (n <= 0) goto smtp_fail;
        sent += (size_t)n;
    }

    /* End with CRLF.CRLF */
    code = smtp_cmd(ssl, fd, "\r\n.", resp, sizeof(resp));
    if (code < 0 || code >= 500) goto smtp_fail;

    /* QUIT */
    smtp_cmd(ssl, fd, "QUIT", NULL, 0);

    if (ssl) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
    }
    close(fd);
    return true;

smtp_fail:
    if (ssl) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
    }
    close(fd);
    return false;
}

/* Generate a unique Message-ID */
static void generate_message_id(char *buf, size_t sz) {
    unsigned char rand_bytes[8];
    if (RAND_bytes(rand_bytes, 8) <= 0) {
        snprintf(buf, sz, "<hermes.%lx.%lx@localhost>",
                 (unsigned long)time(NULL), (unsigned long)rand());
        return;
    }
    char *hex = hex_encode(rand_bytes, 8);
    if (hex) {
        snprintf(buf, sz, "<%s.%lx.%s>", hex, (unsigned long)time(NULL),
                 g_email.from);
        free(hex);
        return;
    }
    snprintf(buf, sz, "<hermes.%lx.%lx@localhost>",
             (unsigned long)time(NULL), (unsigned long)rand());
}

/* ================================================================
 *  Public API
 * ================================================================ */

void email_set_from(const char *from) {
    if (from) snprintf(g_email.from, sizeof(g_email.from), "%s", from);
}

/* Send a plain email (backward compat) */
bool email_send_message(http_client_t *http, const char *to, const char *subject,
                        const char *body) {
    (void)http;
    return email_send_message_ext(to, subject, body, NULL, NULL, NULL);
}

/* Extended send with HTML, attachments, and threading */
bool email_send_message_ext(const char *to, const char *subject,
                             const char *text_body, const char *html_body,
                             json_node_t *attachments,
                             const char *in_reply_to) {
    if (!to || !g_email.from[0]) return false;

    char msg_id[256];
    generate_message_id(msg_id, sizeof(msg_id));

    char email_buf[EMAIL_BODY_MAX * 2];
    size_t pos = 0;

    /* Headers */
    pos += snprintf(email_buf + pos, sizeof(email_buf) - pos,
        "From: %s\r\n"
        "To: %s\r\n"
        "Subject: %s\r\n"
        "Message-ID: %s\r\n"
        "Date: %s",
        g_email.from, to,
        subject ? subject : "Hermes Message",
        msg_id,
        /* Date in RFC 2822 format */
        "Thu, 01 Jan 1970 00:00:00 +0000");

    /* Override date with real date */
    time_t now = time(NULL);
    struct tm *tm = gmtime(&now);
    /* Re-write date */
    char date_buf[64];
    strftime(date_buf, sizeof(date_buf), "%a, %d %b %Y %H:%M:%S %z", tm);

    /* Find and replace the date line */
    char *date_pos = strstr(email_buf, "Date: ");
    if (date_pos) {
        char *end = strchr(date_pos, '\r');
        if (end) {
            size_t date_end = (size_t)(end - email_buf);
            pos = (size_t)(date_pos - email_buf);
            pos += snprintf(email_buf + pos, sizeof(email_buf) - pos,
                "Date: %s\r\n", date_buf);
        }
    }

    /* Add threading headers */
    if (in_reply_to && *in_reply_to) {
        pos += snprintf(email_buf + pos, sizeof(email_buf) - pos,
            "In-Reply-To: %s\r\n", in_reply_to);
        pos += snprintf(email_buf + pos, sizeof(email_buf) - pos,
            "References: %s\r\n", in_reply_to);
    }

    /* Determine if we need multipart */
    bool has_html = (html_body && *html_body);
    bool has_attachments = (attachments && json_len(attachments) > 0);

    if (has_html || has_attachments) {
        char boundary[128];
        mime_boundary_generate(boundary, sizeof(boundary));

        if (has_attachments) {
            /* multipart/mixed with alternative and attachments */
            char alt_boundary[128];
            mime_boundary_generate(alt_boundary, sizeof(alt_boundary));

            pos += snprintf(email_buf + pos, sizeof(email_buf) - pos,
                "MIME-Version: 1.0\r\n"
                "Content-Type: multipart/mixed; boundary=\"%s\"\r\n"
                "\r\n"
                "--%s\r\n"
                "Content-Type: multipart/alternative; boundary=\"%s\"\r\n"
                "\r\n",
                boundary, boundary, alt_boundary);

            /* Text part */
            pos += snprintf(email_buf + pos, sizeof(email_buf) - pos,
                "--%s\r\n"
                "Content-Type: text/plain; charset=UTF-8\r\n"
                "Content-Transfer-Encoding: 7bit\r\n"
                "\r\n"
                "%s\r\n",
                alt_boundary, text_body ? text_body : "");

            /* HTML part */
            if (has_html) {
                pos += snprintf(email_buf + pos, sizeof(email_buf) - pos,
                    "--%s\r\n"
                    "Content-Type: text/html; charset=UTF-8\r\n"
                    "Content-Transfer-Encoding: 7bit\r\n"
                    "\r\n"
                    "%s\r\n",
                    alt_boundary, html_body);
            }

            /* Close alternative */
            pos += snprintf(email_buf + pos, sizeof(email_buf) - pos,
                "--%s--\r\n",
                alt_boundary);

            /* Attachments */
            size_t n = json_len(attachments);
            for (size_t i = 0; i < n; i++) {
                json_node_t *att = json_get(attachments, (int)i);
                if (!att) continue;

                const char *path = json_get_str(att, "path", "");
                const char *filename = json_get_str(att, "filename", "");
                const char *mime_type = json_get_str(att, "mime_type",
                                                       "application/octet-stream");

                if (!*path) continue;

                FILE *f = fopen(path, "rb");
                if (!f) continue;
                fseek(f, 0, SEEK_END);
                long file_size = ftell(f);
                fseek(f, 0, SEEK_SET);

                if (file_size > 0 && file_size < 10 * 1024 * 1024) {
                    unsigned char *file_data = malloc((size_t)file_size);
                    if (file_data) {
                        fread(file_data, 1, (size_t)file_size, f);
                        char *b64 = b64_encode(file_data, (size_t)file_size);

                        pos += snprintf(email_buf + pos, sizeof(email_buf) - pos,
                            "--%s\r\n"
                            "Content-Type: %s\r\n"
                            "Content-Disposition: attachment; filename=\"%s\"\r\n"
                            "Content-Transfer-Encoding: base64\r\n"
                            "\r\n"
                            "%s\r\n",
                            boundary, mime_type, filename, b64 ? b64 : "");

                        free(b64);
                        free(file_data);
                    }
                }
                fclose(f);
            }

            /* Close mixed */
            pos += snprintf(email_buf + pos, sizeof(email_buf) - pos,
                "--%s--\r\n", boundary);

        } else {
            /* multipart/alternative only */
            pos += snprintf(email_buf + pos, sizeof(email_buf) - pos,
                "MIME-Version: 1.0\r\n"
                "Content-Type: multipart/alternative; boundary=\"%s\"\r\n"
                "\r\n"
                "--%s\r\n"
                "Content-Type: text/plain; charset=UTF-8\r\n"
                "Content-Transfer-Encoding: 7bit\r\n"
                "\r\n"
                "%s\r\n"
                "--%s\r\n"
                "Content-Type: text/html; charset=UTF-8\r\n"
                "Content-Transfer-Encoding: 7bit\r\n"
                "\r\n"
                "%s\r\n"
                "--%s--\r\n",
                boundary,
                boundary, text_body ? text_body : "",
                boundary, html_body,
                boundary);
        }

    } else {
        /* Plain text */
        pos += snprintf(email_buf + pos, sizeof(email_buf) - pos,
            "MIME-Version: 1.0\r\n"
            "Content-Type: text/plain; charset=UTF-8\r\n"
            "Content-Transfer-Encoding: 7bit\r\n"
            "\r\n"
            "%s\r\n",
            text_body ? text_body : "");
    }

    /* Try SMTP first, then fallback to sendmail */
    bool sent = false;
    if (g_email.smtp_server[0]) {
        sent = smtp_send(g_email.from, to, email_buf);
        if (!sent) {
            fprintf(stderr, "[email] SMTP send failed, trying sendmail fallback\n");
        }
    }

    if (!sent) {
        /* Fallback to sendmail */
        FILE *fp = popen(g_email.send_cmd, "w");
        if (!fp) return false;
        fwrite(email_buf, 1, strlen(email_buf), fp);
        int rc = pclose(fp);
        sent = (rc == 0);
    }

    return sent;
}

/* ================================================================
 *  IMAP push-based start/stop
 * ================================================================ */

bool email_imap_init(void) {
    memset(&g_email, 0, sizeof(g_email));

    /* Defaults */
    snprintf(g_email.send_cmd, sizeof(g_email.send_cmd), "%s",
             "sendmail -t");
    snprintf(g_email.from, sizeof(g_email.from), "%s",
             "hermes@localhost");
    snprintf(g_email.imap_mailbox, sizeof(g_email.imap_mailbox), "%s",
             "INBOX");
    g_email.poll_interval = 30;
    g_email.imap_port = 0;  /* auto-detect: 993 SSL or 143 plain */
    g_email.imap_sock = -1;
    g_email.tag_counter = 1;
    pthread_mutex_init(&g_email.imap_mutex, NULL);

    /* Environment variables */
    const char *val;

    val = getenv("EMAIL_FROM");
    if (val && *val) snprintf(g_email.from, sizeof(g_email.from), "%s", val);

    val = getenv("EMAIL_SEND_CMD");
    if (val && *val) snprintf(g_email.send_cmd, sizeof(g_email.send_cmd), "%s", val);

    val = getenv("EMAIL_IMAP_SERVER");
    if (val && *val) snprintf(g_email.imap_server, sizeof(g_email.imap_server), "%s", val);

    val = getenv("EMAIL_IMAP_PORT");
    if (val && *val) g_email.imap_port = atoi(val);

    val = getenv("EMAIL_IMAP_USER");
    if (val && *val) snprintf(g_email.imap_user, sizeof(g_email.imap_user), "%s", val);

    val = getenv("EMAIL_IMAP_PASS");
    if (val && *val) snprintf(g_email.imap_pass, sizeof(g_email.imap_pass), "%s", val);

    val = getenv("EMAIL_IMAP_MAILBOX");
    if (val && *val) snprintf(g_email.imap_mailbox, sizeof(g_email.imap_mailbox), "%s", val);

    val = getenv("EMAIL_IMAP_USE_SSL");
    g_email.imap_use_ssl = (!val) ? (g_email.imap_port == 0 || g_email.imap_port == 993)
                                   : (atoi(val) != 0);

    val = getenv("EMAIL_SMTP_SERVER");
    if (val && *val) snprintf(g_email.smtp_server, sizeof(g_email.smtp_server), "%s", val);

    val = getenv("EMAIL_SMTP_PORT");
    if (val && *val) g_email.smtp_port = atoi(val);

    val = getenv("EMAIL_SMTP_USER");
    if (val && *val) snprintf(g_email.smtp_user, sizeof(g_email.smtp_user), "%s", val);

    val = getenv("EMAIL_SMTP_PASS");
    if (val && *val) snprintf(g_email.smtp_pass, sizeof(g_email.smtp_pass), "%s", val);

    val = getenv("EMAIL_SMTP_USE_SSL");
    if (val && *val) g_email.smtp_use_ssl = (atoi(val) != 0);

    val = getenv("EMAIL_ATTACH_DIR");
    if (val && *val) {
        snprintf(g_email.attach_dir, sizeof(g_email.attach_dir), "%s", val);
    } else {
        /* Default: ~/.hermes/attachments/ */
        const char *home = getenv("HOME");
        if (home) {
            snprintf(g_email.attach_dir, sizeof(g_email.attach_dir),
                     "%s/.hermes/attachments", home);
        } else {
            snprintf(g_email.attach_dir, sizeof(g_email.attach_dir),
                     "/tmp/hermes-attachments");
        }
    }

    val = getenv("EMAIL_POLL_DIR");
    if (val && *val) snprintf(g_email.poll_dir, sizeof(g_email.poll_dir), "%s", val);

    val = getenv("EMAIL_POLL_INTERVAL");
    if (val && *val) g_email.poll_interval = atoi(val);

    if (g_email.poll_interval < 5) g_email.poll_interval = 5;

    return true;
}

void email_imap_start(void) {
    if (!g_email.imap_server[0]) {
        fprintf(stderr, "[email] IMAP server not configured, cannot start IDLE\n");
        return;
    }
    if (!g_email.imap_user[0] || !g_email.imap_pass[0]) {
        fprintf(stderr, "[email] IMAP credentials not configured\n");
        return;
    }

    g_email.running = true;
    printf("[email] IMAP IDLE starting (server=%s, mailbox=%s)\n",
           g_email.imap_server, g_email.imap_mailbox);

    /* Initialize SSL */
    ssl_init_once();

    /* Run the IDLE loop (blocks until stopped) */
    thread_imap_idle(NULL);
}

void email_imap_stop(void) {
    g_email.running = false;
    printf("[email] IMAP IDLE stop requested\n");
}

/* ================================================================
 *  Legacy maildir polling (backward compat)
 * ================================================================ */

json_node_t *email_poll_messages(http_client_t *http) {
    (void)http;
    if (!g_email.poll_dir[0]) return NULL;

    char new_dir[2048];
    snprintf(new_dir, sizeof(new_dir), "%s/new", g_email.poll_dir);

    DIR *d = opendir(new_dir);
    if (!d) return NULL;

    json_node_t *results = json_array();
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char path[2048];
        snprintf(path, sizeof(path), "%s/%s", new_dir, entry->d_name);

        /* Read the full file */
        FILE *f = fopen(path, "rb");
        if (!f) continue;

        fseek(f, 0, SEEK_END);
        long file_size = ftell(f);
        fseek(f, 0, SEEK_SET);

        if (file_size > 0 && file_size < (long)sizeof(EMAIL_BODY_MAX)) {
            char *content = malloc((size_t)file_size + 1);
            if (content) {
                size_t n = fread(content, 1, (size_t)file_size, f);
                content[n] = '\0';

                json_node_t *update = parse_raw_email(content, n);
                if (update) {
                    json_append(results, update);
                }
            }
            free(content);
        }
        fclose(f);

        /* Move to cur/ so we don't process again */
        char cur_path[2048];
        snprintf(cur_path, sizeof(cur_path), "%s/cur/%s", g_email.poll_dir, entry->d_name);
        rename(path, cur_path);
    }
    closedir(d);

    if (json_len(results) > 0)
        return results;

    json_free(results);
    return NULL;
}

/* ================================================================
 *  Update extractors
 * ================================================================ */

const char *email_get_chat_id(json_node_t *update) {
    return json_get_str(update, "chat_id", "");
}

const char *email_get_text(json_node_t *update) {
    return json_get_str(update, "text", "");
}

/* New extraction helpers */
const char *email_get_html(json_node_t *update) {
    return json_get_str(update, "html", "");
}

const char *email_get_subject(json_node_t *update) {
    return json_get_str(update, "subject", "");
}

json_node_t *email_get_attachments(json_node_t *update) {
    return json_obj_get(update, "attachments");
}

const char *email_get_thread_id(json_node_t *update) {
    return json_get_str(update, "in_reply_to", "");
}

const char *email_get_message_id(json_node_t *update) {
    return json_get_str(update, "message_id", "");
}

const char *email_get_references(json_node_t *update) {
    return json_get_str(update, "references", "");
}
