/**
 * @file resource.c
 * @brief ACP resource link processing.
 *
 * Converts ACP resource_link / embedded_resource blocks from editors
 * into inline text content for the agent loop.
 *
 * @see acp_adapter/server.py lines 105-442 for Python prototype
 */
#include "acp/resource.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ---------------------------------------------------------------------------
 *  Base64 encoder (standard alphabet, with padding) — for data: URLs.
 *  ------------------------------------------------------------------------ */

static char b64_char(unsigned char c) {
    const char *table = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                        "abcdefghijklmnopqrstuvwxyz"
                        "0123456789+/";
    return table[c & 0x3F];
}

static char *b64_encode(const unsigned char *data, size_t len) {
    if (!data && len > 0) return NULL;
    size_t out_len = ((len + 2) / 3) * 4 + 1; /* includes NUL */
    char *out = (char *)malloc(out_len);
    if (!out) return NULL;

    size_t i = 0, o = 0;
    while (i + 3 <= len) {
        unsigned int v = ((unsigned int)data[i] << 16) |
                         ((unsigned int)data[i+1] << 8) |
                         data[i+2];
        out[o++] = b64_char(v >> 18);
        out[o++] = b64_char(v >> 12);
        out[o++] = b64_char(v >> 6);
        out[o++] = b64_char(v);
        i += 3;
    }
    if (i < len) {
        unsigned int v = (unsigned int)data[i] << 16;
        size_t pad = 3;
        if (i + 1 < len) { v |= (unsigned int)data[i+1] << 8; pad = 2; }
        if (i + 2 < len) { v |= data[i+2]; pad = 1; }
        out[o++] = b64_char(v >> 18);
        out[o++] = b64_char(v >> 12);
        if (pad >= 2) out[o++] = b64_char(v >> 6); else out[o++] = '=';
        if (pad >= 1) out[o++] = b64_char(v); else out[o++] = '=';
    }
    out[o] = '\0';
    return out;
}

/* ---------------------------------------------------------------------------
 *  MIME helpers
 *  ------------------------------------------------------------------------ */

static bool is_image_mime(const char *mime) {
    if (!mime || !*mime) return false;
    return (strncmp(mime, "image/", 6) == 0);
}

static const char *guess_image_mime(const char *path) {
    if (!path) return NULL;
    const char *dot = strrchr(path, '.');
    if (!dot) return NULL;

    /* Lowercase extension comparison */
    char ext[16];
    size_t ext_len = 0;
    dot++; /* skip '.' */
    while (*dot && ext_len < 15) {
        ext[ext_len++] = (char)tolower((unsigned char)*dot);
        dot++;
    }
    ext[ext_len] = '\0';

    if (strcmp(ext, "png") == 0)  return "image/png";
    if (strcmp(ext, "jpg") == 0)  return "image/jpeg";
    if (strcmp(ext, "jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, "gif") == 0)  return "image/gif";
    if (strcmp(ext, "webp") == 0) return "image/webp";
    if (strcmp(ext, "bmp") == 0)  return "image/bmp";
    if (strcmp(ext, "svg") == 0)  return "image/svg+xml";
    return NULL;
}

/* ---------------------------------------------------------------------------
 *  Path conversion — file:// URIs with WSL drive translation
 *  ------------------------------------------------------------------------ */

static char *path_from_file_uri(const char *uri) {
    if (!uri || !*uri) return NULL;

    /* Strip file:// scheme */
    const char *path_text = uri;
    if (strncmp(uri, "file://", 7) == 0) {
        path_text = uri + 7;
        /* Check for netloc (file://host/path) — non-local => return NULL */
        if (*path_text && *path_text != '/') {
            /* Has a netloc component like file://host/path */
            return NULL;
        }
    }

    /* Clone and unquote (basic percent-decode) */
    size_t len = strlen(path_text);
    char *decoded = (char *)malloc(len + 1);
    if (!decoded) return NULL;

    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (path_text[i] == '%' && i + 2 < len) {
            char hex[3] = {path_text[i+1], path_text[i+2], '\0'};
            char *end = NULL;
            long val = strtol(hex, &end, 16);
            if (end && *end == '\0') {
                decoded[j++] = (char)val;
                i += 2;
                continue;
            }
        }
        decoded[j++] = path_text[i];
    }
    decoded[j] = '\0';

    /* Windows drive path translation: /C:/Users/... or C:\Users\... */
    if (j >= 3 && decoded[0] == '/' && decoded[2] == ':' && isalpha((unsigned char)decoded[1])) {
        char drive = (char)tolower((unsigned char)decoded[1]);
        char *rest = decoded + 3;
        /* Skip leading slashes/backslashes */
        while (*rest == '/' || *rest == '\\') rest++;
        size_t rest_len = strlen(rest);
        size_t wsl_path_len = 6 + 1 + rest_len + 1; /* /mnt/X/... */
        char *wsl_path = (char *)malloc(wsl_path_len);
        if (wsl_path) {
            snprintf(wsl_path, wsl_path_len, "/mnt/%c/%s", drive, rest);
        }
        free(decoded);
        return wsl_path;
    }

    /* Windows bare path: C:\Users\... */
    if (j >= 2 && decoded[1] == ':' && isalpha((unsigned char)decoded[0])) {
        char drive = (char)tolower((unsigned char)decoded[0]);
        char *rest = decoded + 2;
        while (*rest == '/' || *rest == '\\') rest++;
        size_t rest_len = strlen(rest);
        size_t wsl_path_len = 6 + 1 + rest_len + 1;
        char *wsl_path = (char *)malloc(wsl_path_len);
        if (wsl_path) {
            snprintf(wsl_path, wsl_path_len, "/mnt/%c/%s", drive, rest);
        }
        free(decoded);
        return wsl_path;
    }

    /* Normal POSIX path */
    return decoded;
}

/* ---------------------------------------------------------------------------
 *  File reading
 *  ------------------------------------------------------------------------ */

/* Read up to max_bytes from a file. Returns malloc'd buffer (caller free). */
static unsigned char *read_file_bytes(const char *path, size_t max_bytes, size_t *out_len) {
    *out_len = 0;
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size < 0) {
        fclose(fp);
        return NULL;
    }

    size_t read_size = ((size_t)file_size < max_bytes) ? (size_t)file_size : max_bytes;
    unsigned char *buf = (unsigned char *)malloc(read_size + 1);
    if (!buf) {
        fclose(fp);
        return NULL;
    }

    size_t actual = fread(buf, 1, read_size, fp);
    fclose(fp);
    buf[actual] = '\0';
    *out_len = actual;
    return buf;
}

/* Try to decode bytes as text (UTF-8). Returns NULL for binary (first null byte). */
static bool is_text_content(const unsigned char *data, size_t len) {
    /* Check for null byte in first 4KB — indicates binary */
    size_t check = (len < 4096) ? len : 4096;
    for (size_t i = 0; i < check; i++) {
        if (data[i] == '\0') return false;
    }
    return true;
}

/* Build a [Attached file: name] header string. Caller free(). */
static char *format_resource_header(const char *uri, const char *display_name,
                                     const char *note) {
    /* Derive display name from URI if not provided */
    const char *name = display_name;
    char fallback[256];
    if (!name || !*name) {
        if (uri) {
            const char *last_slash = strrchr(uri, '/');
            if (last_slash && last_slash[1])
                snprintf(fallback, sizeof(fallback), "%s", last_slash + 1);
            else
                snprintf(fallback, sizeof(fallback), "resource");
        } else {
            snprintf(fallback, sizeof(fallback), "resource");
        }
        name = fallback;
    }

    /* Build header: [Attached file: name] + optional (note) */
    size_t header_len = 256 + (note ? strlen(note) : 0) + (uri ? strlen(uri) : 0);
    char *header = (char *)malloc(header_len);
    if (!header) return NULL;

    if (note && *note) {
        snprintf(header, header_len, "[Attached file: %s] (%s)\nURI: %s\n\n",
                 name, note, uri ? uri : "");
    } else {
        snprintf(header, header_len, "[Attached file: %s]\nURI: %s\n\n",
                 name, uri ? uri : "");
    }
    return header;
}

/* ---------------------------------------------------------------------------
 *  Resource processing
 *  ------------------------------------------------------------------------ */

/* Process a resource_link block from the content array. Returns malloc'd text. */
static char *process_resource_link(json_node_t *block) {
    json_node_t *resource = json_object_get(block, "resource");
    if (!resource) return NULL;

    const char *uri = json_object_get_string(resource, "uri", NULL);
    if (!uri) return NULL;

    const char *name = json_object_get_string(resource, "name", NULL);
    const char *mime = json_object_get_string(resource, "mime_type", NULL);

    /* Convert URI to filesystem path */
    char *path = path_from_file_uri(uri);

    /* Non-file URIs / unreadable paths: return placeholder */
    if (!path) {
        char *header = format_resource_header(uri, name, NULL);
        if (!header) return NULL;

        size_t text_len = strlen(header) + 100;
        char *text = (char *)malloc(text_len);
        if (!text) { free(header); return NULL; }
        snprintf(text, text_len, "%s[Resource link only — Hermes cannot read non-file ACP resource URIs directly.]",
                 header);
        free(header);
        return text;
    }

    /* Attempt to read the file */
    size_t data_len = 0;
    unsigned char *data = read_file_bytes(path, ACP_MAX_RESOURCE_BYTES, &data_len);
    free(path);

    if (!data) {
        char *header = format_resource_header(uri, name, NULL);
        if (!header) return NULL;

        size_t text_len = strlen(header) + 100;
        char *text = (char *)malloc(text_len);
        if (!text) { free(header); return NULL; }
        snprintf(text, text_len, "%s[Could not read attached file: %s]",
                 header, uri);
        free(header);
        return text;
    }

    /* Determine MIME type — use explicit or guess from path */
    const char *effective_mime = mime;
    if (!effective_mime || !*effective_mime) {
        effective_mime = guess_image_mime(uri);
    }

    /* Image files: return text header + data: URI marker */
    if (effective_mime && is_image_mime(effective_mime)) {
        char *b64 = b64_encode(data, data_len);
        free(data);
        if (!b64) return NULL;

        /* Build: [Attached image: name]\nURI: ...\ndata:image/...;base64,... */
        size_t text_len = 512 + strlen(uri) + strlen(b64) +
                          (name ? strlen(name) : 5);
        char *text = (char *)malloc(text_len);
        if (!text) { free(b64); return NULL; }

        snprintf(text, text_len,
                 "[Attached image: %s]\nURI: %s\ndata:%s;base64,%s",
                 name && *name ? name : (uri ? uri : "image"),
                 uri ? uri : "",
                 effective_mime,
                 b64);
        free(b64);
        return text;
    }

    /* Text files: inline with metadata header */
    if (is_text_content(data, data_len)) {
        char *header = format_resource_header(uri, name,
            data_len >= ACP_MAX_RESOURCE_BYTES ? "truncated to max size" : NULL);
        if (!header) { free(data); return NULL; }

        size_t header_len = strlen(header);
        size_t text_len = header_len + data_len + 1;
        char *text = (char *)malloc(text_len);
        if (!text) { free(header); free(data); return NULL; }

        memcpy(text, header, header_len);
        memcpy(text + header_len, data, data_len);
        text[header_len + data_len] = '\0';

        /* Remove trailing newlines from file content for cleaner output */
        char *end = text + header_len + data_len - 1;
        while (end > text + header_len && (*end == '\n' || *end == '\r'))
            *end-- = '\0';

        free(header);
        free(data);
        return text;
    }

    /* Binary non-image files */
    free(data);
    char *header = format_resource_header(uri, name, NULL);
    if (!header) return NULL;

    size_t text_len = strlen(header) + 200;
    char *text = (char *)malloc(text_len);
    if (!text) { free(header); return NULL; }
    snprintf(text, text_len,
             "%s[Binary file omitted: %zu bytes, mime=%s]",
             header, data_len, effective_mime ? effective_mime : "unknown");
    free(header);
    return text;
}

/* Process a single content block (text, resource_link, embedded_resource, image).
 * Returns malloc'd string or NULL. */
static char *process_content_block(json_node_t *block) {
    if (!block) return NULL;

    /* Plain text block */
    if (block->type == JSON_STRING) {
        return strdup(block->str_val);
    }

    /* Object block — check for type field */
    const char *type = json_object_get_string(block, "type", NULL);
    if (!type) return NULL;

    if (strcmp(type, "text") == 0) {
        const char *text = json_object_get_string(block, "text", NULL);
        return text ? strdup(text) : NULL;
    }

    if (strcmp(type, "resource") == 0 ||
        strcmp(type, "resource_link") == 0) {
        return process_resource_link(block);
    }

    if (strcmp(type, "image") == 0 ||
        strcmp(type, "image_url") == 0) {
        /* Image block: try data or uri */
        const char *data_str = json_object_get_string(block, "data", NULL);
        const char *uri = json_object_get_string(block, "uri", NULL);
        const char *mime = json_object_get_string(block, "mime_type", NULL);
        if (!mime) mime = "image/png";

        if (data_str) {
            /* Inline base64 data */
            size_t text_len = 512 + strlen(data_str);
            char *text = (char *)malloc(text_len);
            if (!text) return NULL;

            const char *prefix = strstr(data_str, "data:") ? "" : "data:image/png;base64,";
            if (uri)
                snprintf(text, text_len, "[Attached image]\nURI: %s\n%s%s",
                         uri, prefix, data_str);
            else
                snprintf(text, text_len, "[Attached image]\n%s%s", prefix, data_str);
            return text;
        }

        if (uri) {
            size_t text_len = 256 + strlen(uri);
            char *text = (char *)malloc(text_len);
            if (!text) return NULL;
            snprintf(text, text_len, "[Attached image]\nURI: %s", uri);
            return text;
        }

        return strdup("[Attached image — no data or URI]");
    }

    if (strcmp(type, "embedded_resource") == 0 ||
        strcmp(type, "data") == 0) {
        json_node_t *resource = json_object_get(block, "resource");
        if (!resource) return NULL;

        const char *uri2 = json_object_get_string(resource, "uri", NULL);
        const char *mime2 = json_object_get_string(resource, "mime_type", NULL);
        const char *text_val = json_object_get_string(resource, "text", NULL);

        if (text_val) {
            char *header = format_resource_header(uri2, NULL, NULL);
            if (!header) return strdup(text_val);
            size_t text_len = strlen(header) + strlen(text_val) + 1;
            char *text = (char *)malloc(text_len);
            if (!text) { free(header); return NULL; }
            snprintf(text, text_len, "%s%s", header, text_val);
            free(header);
            return text;
        }

        /* Blob resource — might be base64-encoded */
        const char *blob = json_object_get_string(resource, "blob", NULL);
        if (blob) {
            const char *mime = mime2;
            if (mime && is_image_mime(mime) && strlen(blob) < ACP_MAX_RESOURCE_BYTES) {
                size_t text_len = 512 + strlen(uri2) + strlen(blob);
                char *text = (char *)malloc(text_len);
                if (!text) return NULL;
                snprintf(text, text_len,
                         "[Attached image: %s]\nURI: %s\ndata:%s;base64,%s",
                         uri2 ? uri2 : "image",
                         uri2 ? uri2 : "",
                         mime, blob);
                return text;
            }
            /* Non-image blob — just note it */
            char *header = format_resource_header(uri2, NULL, NULL);
            if (!header) return strdup("[Embedded blob resource]");
            size_t text_len = strlen(header) + 128;
            char *text = (char *)malloc(text_len);
            if (!text) { free(header); return NULL; }
            snprintf(text, text_len,
                     "%s[Embedded blob: %zu chars]", header, strlen(blob));
            free(header);
            return text;
        }

        return strdup("[Embedded resource — no recognizable content]");
    }

    return NULL;
}

/* ---------------------------------------------------------------------------
 *  Public API
 *  ------------------------------------------------------------------------ */

char *acp_content_to_text(json_node_t *content) {
    if (!content) return NULL;

    /* String content — return verbatim */
    if (content->type == JSON_STRING) {
        return strdup(content->str_val);
    }

    /* Array of content blocks */
    if (content->type == JSON_ARRAY) {
        size_t count = 0;
        size_t cap = 16;
        size_t total = 0;
        char **parts = (char **)malloc(cap * sizeof(char *));
        if (!parts) return NULL;

        size_t len = json_len(content);
        for (size_t idx = 0; idx < len; idx++) {
            json_node_t *child = json_get(content, idx);
            char *part = process_content_block(child);
            if (part) {
                if (count >= cap) {
                    cap *= 2;
                    char **new_parts = (char **)realloc(parts, cap * sizeof(char *));
                    if (!new_parts) {
                        for (size_t i = 0; i < count; i++) free(parts[i]);
                        free(parts);
                        return NULL;
                    }
                    parts = new_parts;
                }
                parts[count++] = part;
                total += strlen(part);
            }
        }

        if (count == 0) {
            free(parts);
            return NULL;
        }

        /* Join with newlines */
        size_t buf_size = total + count + 1;
        char *result = (char *)malloc(buf_size);
        if (!result) {
            for (size_t i = 0; i < count; i++) free(parts[i]);
            free(parts);
            return NULL;
        }

        size_t pos = 0;
        for (size_t i = 0; i < count; i++) {
            if (i > 0) result[pos++] = '\n';
            size_t len = strlen(parts[i]);
            memcpy(result + pos, parts[i], len);
            pos += len;
            free(parts[i]);
        }
        result[pos] = '\0';
        free(parts);
        return result;
    }

    /* Single object block (uncommon but valid per ACP spec) */
    if (content->type == JSON_OBJECT) {
        return process_content_block(content);
    }

    return NULL;
}
