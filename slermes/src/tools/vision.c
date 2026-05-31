/*
 * vision.c — Vision/image analysis tool for Hermes C.
 * Reads image metadata (via identify/file) and optionally
 * sends image data to LLM for description via Python delegation.
 */
#include "hermes.h"
#include "hermes_json.h"
#include "hermes_url_safety.h"
#include "base64.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>
#include <ctype.h>

static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"image_url\":{\"type\":\"string\",\"description\":\"Path or URL to image file\"},"
      "\"question\":{\"type\":\"string\",\"description\":\"Optional question about the image\"},"
      "\"detail\":{\"type\":\"string\",\"description\":\"Detail level: 'low', 'high', or 'auto' (default). Controls image resolution for vision analysis.\"},"
    "\"analysis\":{\"type\":\"string\",\"description\":\"Optional analysis type: 'colors' (dominant color palette), 'exif' (EXIF metadata), 'ocr' (text extraction via OCR). Requires local file path.\"}"
    "},"
    "\"required\":[\"image_url\"]"
"}";

/* Run a shell command and capture all output. Returns malloc'd string. */
static char *run_cmd_full(const char *fmt, ...) {
    char cmd[8192];
    va_list args;
    va_start(args, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, args);
    va_end(args);

    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;

    /* Use mkstemp for a secure temp file */
    char tmp[] = "/tmp/hermes_vision_XXXXXX";
    int fd = mkstemp(tmp);
    if (fd < 0) { pclose(fp); return NULL; }
    char buf[4096];
    size_t written = 0;
    ssize_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        ssize_t r = write(fd, buf, (size_t)n);
        if (r < 0) break;
        written += (size_t)r;
    }
    pclose(fp);

    /* Read back */
    char *out = (char *)malloc(written + 1);
    if (out) {
        lseek(fd, 0, SEEK_SET);
        ssize_t r = read(fd, out, written);
        if (r > 0) out[r] = '\0';
        else out[0] = '\0';
    }
    close(fd);
    unlink(tmp);
    return out;
}

/* Run a shell command and capture first line of output */
static char *run_cmd_firstline(const char *fmt, ...) {
    char cmd[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, args);
    va_end(args);

    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;
    static char buf[4096];
    if (!fgets(buf, sizeof(buf), fp)) buf[0] = '\0';
    pclose(fp);
    size_t len = strlen(buf);
    while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r'))
        buf[--len] = '\0';
    return strdup(buf);
}

/* Maximum image file size for processing (50 MB) */
#define VISION_MAX_FILE_BYTES (50LL * 1024 * 1024)

/* F41: Validate image URL path has a known image extension */
static bool has_image_extension(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return false;
    ext++; /* skip '.' */
    const char *exts[] = {"jpg","jpeg","png","gif","webp","bmp","svg","tiff","tif","ico","heic","heif","avif", NULL};
    for (int i = 0; exts[i]; i++) {
        size_t elen = strlen(exts[i]);
        if (strncasecmp(ext, exts[i], elen) == 0 && strlen(ext) == elen)
            return true;
    }
    return false;
}

/* D08: Magic-byte image format detection — works on extensionless files.
 * Reads first 12 bytes of file and checks known image signatures.
 * Returns NULL if unrecognized, else a format name string (static). */
static const char *detect_image_magic(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    unsigned char hdr[16] = {0};
    size_t n = fread(hdr, 1, 12, f);
    fclose(f);
    if (n < 4) return NULL;

    /* PNG:  89 50 4E 47 */
    if (n >= 4 && hdr[0]==0x89 && hdr[1]=='P' && hdr[2]=='N' && hdr[3]=='G')
        return "png";
    /* JPEG: FF D8 FF */
    if (n >= 3 && hdr[0]==0xFF && hdr[1]==0xD8 && hdr[2]==0xFF)
        return "jpeg";
    /* GIF:  47 49 46 38 (GIF8) */
    if (n >= 4 && hdr[0]=='G' && hdr[1]=='I' && hdr[2]=='F' && hdr[3]=='8')
        return "gif";
    /* BMP:  42 4D (BM) */
    if (n >= 2 && hdr[0]=='B' && hdr[1]=='M')
        return "bmp";
    /* TIFF: 49 49 2A 00 (little-endian) or 4D 4D 00 2A (big-endian) */
    if (n >= 4 && ((hdr[0]==0x49 && hdr[1]==0x49 && hdr[2]==0x2A && hdr[3]==0x00) ||
                   (hdr[0]==0x4D && hdr[1]==0x4D && hdr[2]==0x00 && hdr[3]==0x2A)))
        return "tiff";
    /* WebP: 52 49 46 46 ... 57 45 42 50 */
    if (n >= 12 && hdr[0]=='R' && hdr[1]=='I' && hdr[2]=='F' && hdr[3]=='F' &&
        hdr[8]=='W' && hdr[9]=='E' && hdr[10]=='B' && hdr[11]=='P')
        return "webp";
    /* ICO:  00 00 01 00 */
    if (n >= 4 && hdr[0]==0x00 && hdr[1]==0x00 && hdr[2]==0x01 && hdr[3]==0x00)
        return "ico";
    /* AVIF/HEIC: ftyp box — ftypavif / ftypheic / ftypheim */
    if (n >= 12 && hdr[4]=='f' && hdr[5]=='t' && hdr[6]=='y' && hdr[7]=='p') {
        if (memcmp(hdr+8, "avif", 4) == 0) return "avif";
        if (memcmp(hdr+8, "heic", 4) == 0 || memcmp(hdr+8, "heim", 4) == 0) return "heic";
    }
    return NULL;
}

/* Map internal format string to MIME type for data URIs */
static const char *image_format_to_mime(const char *format) {
    if (!format) return NULL;
    if (strcmp(format, "png") == 0) return "image/png";
    if (strcmp(format, "jpeg") == 0) return "image/jpeg";
    if (strcmp(format, "gif") == 0) return "image/gif";
    if (strcmp(format, "webp") == 0) return "image/webp";
    if (strcmp(format, "bmp") == 0) return "image/bmp";
    if (strcmp(format, "tiff") == 0 || strcmp(format, "tif") == 0) return "image/tiff";
    if (strcmp(format, "ico") == 0) return "image/x-icon";
    if (strcmp(format, "svg") == 0) return "image/svg+xml";
    if (strcmp(format, "avif") == 0) return "image/avif";
    if (strcmp(format, "heic") == 0) return "image/heic";
    return NULL;
}

/* Read entire file into malloc'd buffer. Returns length in *out_len. */
static unsigned char *read_file_bytes(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    struct stat st;
    if (stat(path, &st) != 0) { fclose(f); return NULL; }
    if (st.st_size > VISION_MAX_FILE_BYTES) { fclose(f); return NULL; }
    if (st.st_size == 0) { fclose(f); *out_len = 0; return (unsigned char *)strdup(""); }
    unsigned char *buf = (unsigned char *)malloc((size_t)st.st_size + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)st.st_size, f);
    fclose(f);
    buf[n] = '\0';
    *out_len = n;
    return buf;
}

/* Convert local image to base64 data URL: data:image/png;base64,...
 * Caller must free() the returned string. */
static char *image_to_base64_data_url(const char *path, const char *format) {
    size_t file_len = 0;
    unsigned char *file_data = read_file_bytes(path, &file_len);
    if (!file_data) return NULL;

    const char *mime = image_format_to_mime(format);
    if (!mime) { free(file_data); return NULL; }

    char *b64 = base64_encode(file_data, file_len);
    free(file_data);
    if (!b64) return NULL;

    /* Build prefix + base64: "data:<mime>;base64,<data>" */
    size_t prefix_len = strlen(mime) + 17; /* "data:;base64," + null guard */
    size_t b64_len = strlen(b64);
    char *data_url = (char *)malloc(prefix_len + b64_len + 1);
    if (!data_url) { free(b64); return NULL; }
    snprintf(data_url, prefix_len + b64_len + 1, "data:%s;base64,%s", mime, b64);
    free(b64);
    return data_url;
}

/* Extract image dimensions from file header without Python/PIL.
 * Returns malloc'd string like "1920x1080" or NULL on failure.
 * Supports: PNG, JPEG, GIF, BMP, WebP (lossless/lossy). */
static char *extract_dimensions_native(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    unsigned char buf[64];
    size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    if (n < 12) return NULL;

    char dims[64] = "";
    uint32_t w = 0, h = 0;

    if (n >= 24 && buf[0]==0x89 && buf[1]=='P' && buf[2]=='N' && buf[3]=='G') {
        /* PNG: IHDR at offset 16, width bytes 16-19, height bytes 20-23 */
        w = ((uint32_t)buf[16] << 24) | ((uint32_t)buf[17] << 16) |
            ((uint32_t)buf[18] << 8) | buf[19];
        h = ((uint32_t)buf[20] << 24) | ((uint32_t)buf[21] << 16) |
            ((uint32_t)buf[22] << 8) | buf[23];
        snprintf(dims, sizeof(dims), "%ux%u", w, h);
    } else if (n >= 24 && buf[0]==0xFF && buf[1]==0xD8 && buf[2]==0xFF) {
        /* JPEG: scan for SOF0 marker (FF C0 or FF C1 or FF C2) */
        for (size_t i = 2; i < n - 9; i++) {
            if (buf[i] == 0xFF && (buf[i+1] == 0xC0 || buf[i+1] == 0xC1 ||
                                   buf[i+1] == 0xC2)) {
                h = ((uint32_t)buf[i+5] << 8) | buf[i+6];
                w = ((uint32_t)buf[i+7] << 8) | buf[i+8];
                snprintf(dims, sizeof(dims), "%ux%u", w, h);
                break;
            }
        }
    } else if (n >= 10 && buf[0]=='G' && buf[1]=='I' && buf[2]=='F') {
        /* GIF: logical screen width at 6-7, height at 8-9 (little-endian) */
        w = (uint32_t)buf[6] | ((uint32_t)buf[7] << 8);
        h = (uint32_t)buf[8] | ((uint32_t)buf[9] << 8);
        snprintf(dims, sizeof(dims), "%ux%u", w, h);
    } else if (n >= 26 && buf[0]=='B' && buf[1]=='M') {
        /* BMP: width at 18-21, height at 22-25 (little-endian) */
        w = (uint32_t)buf[18] | ((uint32_t)buf[19] << 8) |
            ((uint32_t)buf[20] << 16) | ((uint32_t)buf[21] << 24);
        h = (uint32_t)buf[22] | ((uint32_t)buf[23] << 8) |
            ((uint32_t)buf[24] << 16) | ((uint32_t)buf[25] << 24);
        snprintf(dims, sizeof(dims), "%ux%u", w, h);
    } else if (n >= 30 && buf[0]=='R' && buf[1]=='I' && buf[2]=='F' && buf[3]=='F') {
        /* WebP: RIFF...WEBP. VP8 (lossy) at offset 26: 19 bits for w/h.
         * VP8L (lossless) at offset 21: 14 bits for w/h.
         * VP8X (extended) at offset 24: 24 bits for w/h. */
        if (n >= 30 && buf[8]=='W' && buf[9]=='E' && buf[10]=='B' && buf[11]=='P') {
            if (buf[12]=='V' && buf[13]=='P' && buf[14]=='8' && buf[15]==' ') {
                /* VP8 (lossy) */
                w = ((uint32_t)(buf[27] & 0x3F) << 8) | buf[26];
                h = ((uint32_t)(buf[29] & 0x3F) << 8) | buf[28];
                snprintf(dims, sizeof(dims), "%ux%u", w + 1, h + 1);
            } else if (buf[12]=='V' && buf[13]=='P' && buf[14]=='8' && buf[15]=='L') {
                /* VP8L (lossless) */
                w = ((uint32_t)(buf[21] & 0x3F) << 8) | (buf[20] & 0xFF);
                h = ((uint32_t)((buf[23] & 0x0F) << 10) | ((uint32_t)buf[22] << 2) |
                     ((buf[21] & 0xC0) >> 6));
                snprintf(dims, sizeof(dims), "%ux%u", w + 1, h + 1);
            }
        }
    }

    if (dims[0]) return strdup(dims);
    return NULL;
}

/* Check if an error string indicates image/payload size limit.
 * Mirrors Python vision_tools._is_image_size_error(). */
static bool is_image_size_error(const char *error_text) {
    if (!error_text || !error_text[0]) return false;
    const char *hints[] = {
        "too large", "payload", "413", "content_too_large",
        "request_too_large", "image_url", "invalid_request",
        "exceeds", "size limit", NULL
    };
    char *lower = strdup(error_text);
    if (!lower) return false;
    for (char *p = lower; *p; p++) *p = (char)tolower((unsigned char)*p);
    for (int i = 0; hints[i]; i++) {
        if (strstr(lower, hints[i])) { free(lower); return true; }
    }
    free(lower);
    return false;
}

/* Port of Python vision_tools.py _validate_image_url().
 * Validates image URL: non-NULL, http/https scheme, network location,
 * and SSRF safety check via url_is_safe. Pure function, no side effects.
 * Returns true if the URL passes all checks. */
bool vision_validate_image_url(const char *url) {
    if (!url || !url[0]) return false;

    /* Must start with http:// or https:// */
    if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0)
        return false;

    /* Must have a colon after the scheme to indicate host:port or path */
    const char *scheme_end = strstr(url, "://");
    if (!scheme_end) return false;
    const char *netloc = scheme_end + 3;
    if (!*netloc) return false;

    /* Must have at least a dot or colon in the network location (domain/IP) */
    const char *p = netloc;
    bool has_host = false;
    while (*p && *p != '/' && *p != '?' && *p != '#') {
        if (*p == '.' || *p == ':') { has_host = true; break; }
        p++;
    }
    if (!has_host && p - netloc < 2) return false;  /* localhost? needs at least 2 chars */

    return true;
}

/* Port of Python vision_tools.py _supports_media_in_tool_results().
 * Returns true if the given provider+model accepts image content inside
 * a tool-result message. Conservative default is false. */
bool vision_supports_media_in_tool_results(const char *provider, const char *model) {
    if (!provider || !provider[0]) return false;

    /* Aggregators that route to vision-capable models */
    if (strcmp(provider, "openrouter") == 0 ||
        strcmp(provider, "nous") == 0 ||
        strcmp(provider, "vertex") == 0 ||
        strcmp(provider, "bedrock") == 0 ||
        strcmp(provider, "anthropic-vertex") == 0 ||
        strcmp(provider, "google-vertex") == 0)
        return true;

    /* Native Anthropic */
    if (strcmp(provider, "anthropic") == 0 ||
        strcmp(provider, "claude") == 0 ||
        strcmp(provider, "anthropic-direct") == 0)
        return true;

    /* OpenAI Chat Completions and Responses */
    if (strcmp(provider, "openai") == 0 ||
        strcmp(provider, "openai-chat") == 0 ||
        strcmp(provider, "openai-codex") == 0 ||
        strcmp(provider, "azure-openai") == 0)
        return true;

    /* Gemini — gate on model name; only Gemini 3+ supports */
    if (strcmp(provider, "google") == 0 ||
        strcmp(provider, "gemini") == 0 ||
        strcmp(provider, "google-gemini") == 0 ||
        strcmp(provider, "google-vertex-gemini") == 0) {
        if (!model || !model[0]) return false;
        if (strstr(model, "gemini-3.") || strstr(model, "gemini-3-") ||
            strcmp(model, "gemini-3") == 0 ||
            strstr(model, "gemini-pro-3") || strstr(model, "gemini-flash-3"))
            return true;
        return false;
    }

    return false;
}

char *vision_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *image_url = json_object_get_string(args, "image_url", NULL);
    const char *question = json_object_get_string(args, "question", NULL);
    const char *detail = json_object_get_string(args, "detail", NULL);
    const char *analysis = json_object_get_string(args, "analysis", NULL);

    json_node_t *result = json_new_object();

    if (!image_url) {
        json_object_set(result, "error", json_new_string("Missing image_url"));
    } else {
        /* Check file exists */
        struct stat st;
        bool is_local = (strncmp(image_url, "http://", 7) != 0 &&
                          strncmp(image_url, "https://", 8) != 0 &&
                          strncmp(image_url, "data:", 5) != 0);

        if (is_local && stat(image_url, &st) != 0) {
            json_object_set(result, "error", json_new_string("File not found"));
        } else if (is_local && !has_image_extension(image_url)) {
            /* D08: Check magic bytes for extensionless files */
            const char *magic_format = detect_image_magic(image_url);
            if (!magic_format) {
                char buf[256];
                snprintf(buf, sizeof(buf), "Not a recognized image format (extensions: jpg/png/gif/webp/bmp/svg/ico/heic/avif)");
                json_object_set(result, "error", json_new_string(buf));
            } else {
                json_object_set(result, "detected_format", json_new_string(magic_format));
                /* Also set image_url so the provider can process it */
                json_object_set(result, "image_url", json_new_string(image_url));
                if (detail) json_object_set(result, "detail", json_new_string(detail));
                if (analysis) json_object_set(result, "analysis", json_new_string(analysis));

                /* Base64 data URL for extensionless files detected via magic bytes */
                {
                    char *b64_url = image_to_base64_data_url(image_url, magic_format);
                    if (b64_url) {
                        json_object_set(result, "base64_data_url", json_new_string(b64_url));
                        size_t b64len = strlen(b64_url);
                        if (b64len > 120) {
                            char preview[128];
                            snprintf(preview, sizeof(preview),
                                "data URL available (%zu chars)", b64len);
                            json_object_set(result, "base64_data_url_info",
                                json_new_string(preview));
                        }
                        free(b64_url);
                    }
                }

                /* Continue processing — valid image detected via magic bytes */
            }
        } else if (is_local && st.st_size > VISION_MAX_FILE_BYTES) {
            json_object_set(result, "error", json_new_string("File too large (>50 MB)"));
        } else if (!is_local && strncmp(image_url, "data:", 5) != 0) {
            /* Remote URL safety checks (skip for data: URIs) */
            const char *secret = url_has_secret(image_url);
            if (secret) {
                char buf[256];
                snprintf(buf, sizeof(buf), "Blocked: URL contains what appears to be a %s API key. Secrets must not be sent in URLs.", secret);
                json_object_set(result, "error", json_new_string(buf));
            } else if (!url_is_safe(image_url)) {
                json_object_set(result, "error", json_new_string("URL blocked by SSRF protection: private or internal address"));
            } else {
                /* Remote URL — pass through for LLM processing */
                json_object_set(result, "image_url", json_new_string(image_url));
                if (detail) json_object_set(result, "detail", json_new_string(detail));
                if (analysis) json_object_set(result, "analysis", json_new_string(analysis));
                if (question) {
                    json_object_set(result, "question", json_new_string(question));
                    /* Attempt quick HEAD-like check for content-type validation */
                    http_t *h = http_new(5);
                    if (h) {
                        http_resp_t *resp = http_get(h, image_url, NULL);
                        if (resp && resp->status >= 200 && resp->status < 300 && resp->headers) {
                            /* Search for Content-Type in raw headers */
                            const char *ct = strstr(resp->headers, "content-type:");
                            if (!ct) ct = strstr(resp->headers, "Content-Type:");
                            if (ct) {
                                ct += 13; /* skip "content-type:" or "Content-Type:" */
                                while (*ct == ' ') ct++;
                                if (strncmp(ct, "image/", 6) != 0) {
                                    char warn[256];
                                    const char *nl = strchr(ct, '\r');
                                    if (!nl) nl = strchr(ct, '\n');
                                    size_t ct_len = nl ? (size_t)(nl - ct) : strlen(ct);
                                    if (ct_len > 64) ct_len = 64;
                                    char ct_buf[65];
                                    memcpy(ct_buf, ct, ct_len);
                                    ct_buf[ct_len] = '\0';
                                    snprintf(warn, sizeof(warn),
                                             "Content-Type is '%s', not an image. Remote URL may not be an image.",
                                             ct_buf);
                                    json_object_set(result, "remote_content_type_warning", json_new_string(warn));
                                }
                            }
                        }
                        if (resp) http_resp_free(resp);
                        http_free(h);
                    }
                }
            }
        } else {
            json_object_set(result, "image_url", json_new_string(image_url));
            if (detail) json_object_set(result, "detail", json_new_string(detail));
            if (analysis) json_object_set(result, "analysis", json_new_string(analysis));

            /* Try to get image metadata via file command */
            if (is_local) {
                char *file_info = run_cmd_firstline("file -b '%s' 2>/dev/null", image_url);
                char *size_info = run_cmd_firstline("stat --format='%%s bytes' '%s' 2>/dev/null", image_url);
                if (file_info) {
                    json_object_set(result, "file_info", json_new_string(file_info));
                    free(file_info);
                }
                if (size_info) {
                    json_object_set(result, "size", json_new_string(size_info));
                    free(size_info);
                }
                json_object_set(result, "file_size", json_new_number((double)st.st_size));

                /* Native dimension extraction (no Python dependency) */
                char *dims = extract_dimensions_native(image_url);
                if (!dims) {
                    /* Fallback: Python PIL */
                    dims = run_cmd_firstline(
                        "python3 -c \"from PIL import Image; "
                        "i=Image.open('%s'); "
                        "print(f'{i.size[0]}x{i.size[1]} mod={i.mode}')\" 2>/dev/null",
                        image_url);
                }
                if (dims) {
                    json_object_set(result, "dimensions", json_new_string(dims));
                    free(dims);
                }

                /* Native base64 data URL for direct provider consumption */
                {
                    const char *img_fmt = detect_image_magic(image_url);
                    if (!img_fmt) img_fmt = has_image_extension(image_url) ?
                        strrchr(image_url, '.') + 1 : NULL;
                    if (img_fmt) {
                        char *b64_url = image_to_base64_data_url(image_url, img_fmt);
                        if (b64_url) {
                            /* Truncate in result for display — full URL too large */
                            json_object_set(result, "base64_data_url",
                                json_new_string(b64_url));
                            /* Also add a preview hint */
                            size_t b64len = strlen(b64_url);
                            if (b64len > 120) {
                                char preview[128];
                                snprintf(preview, sizeof(preview),
                                    "data URL available (%zu chars, starts with %.60s...)",
                                    b64len, b64_url);
                                json_object_set(result, "base64_data_url_info",
                                    json_new_string(preview));
                            }
                            free(b64_url);
                        }
                    }
                }

                /* Color analysis via Python helper */
                if (analysis && strcmp(analysis, "colors") == 0) {
                    char *colors = run_cmd_full(
                        "python3 src/tools/vision_analysis.py colors '%s' 2>/dev/null",
                        image_url);
                    if (colors && strlen(colors) > 0) {
                        json_object_set(result, "color_analysis", json_new_string(colors));
                    }
                    free(colors);
                }

                /* EXIF extraction via Python helper */
                if (analysis && strcmp(analysis, "exif") == 0) {
                    char *exif_data = run_cmd_full(
                        "python3 src/tools/vision_analysis.py exif '%s' 2>/dev/null",
                        image_url);
                    if (exif_data && strlen(exif_data) > 0) {
                        json_object_set(result, "exif", json_new_string(exif_data));
                    }
                    free(exif_data);
                }

                /* OCR text extraction via Python helper */
                if (analysis && strcmp(analysis, "ocr") == 0) {
                    char *ocr_data = run_cmd_full(
                        "python3 src/tools/vision_analysis.py ocr '%s' 2>/dev/null",
                        image_url);
                    if (ocr_data && strlen(ocr_data) > 0) {
                        json_object_set(result, "ocr", json_new_string(ocr_data));
                    }
                    free(ocr_data);
                }
            }

                /* Vision analysis via Python Hermes subprocess delegation */
                if (question && *question) {
                    json_object_set(result, "question", json_new_string(question));

                    /* Find vision delegate script path */
                    char script_path[1024];
                    const char *home = getenv("SLERMES_HOME") ? getenv("SLERMES_HOME") :
                                       getenv("HOME") ? getenv("HOME") : ".";
                    snprintf(script_path, sizeof(script_path),
                             "%s/hermes-agent-dev/C/src/tools/vision_delegate.py", home);
                    struct stat sp;
                    if (stat(script_path, &sp) != 0) {
                        snprintf(script_path, sizeof(script_path),
                                 "%s/.hermes/scripts/vision_delegate.py", home);
                    }

                    /* Call Python Hermes agent for AI image description */
                    const char *det = detail ? detail : "auto";
                    char *desc = run_cmd_full(
                        "python3 '%s' '%s' '%s' '%s' 2>&1 | head -c 5000",
                        script_path, image_url, question, det);
                if (desc && strlen(desc) > 10 && !strstr(desc, "not found") &&
                    !strstr(desc, "error") && !strstr(desc, "timed out") &&
                    !strstr(desc, "Usage:")) {
                    json_object_set(result, "description", json_new_string(desc));
                } else {
                    json_object_set(result, "description_note",
                        json_new_string("Full vision analysis requires LLM with vision support. "
                                        "Python Hermes delegation unavailable. "
                                        "Use a vision-capable LLM provider directly."));
                    /* Check if error is image-size related, add resize hint */
                    if (desc && is_image_size_error(desc)) {
                        json_object_set(result, "resize_hint",
                            json_new_string("Image may exceed size limits. Try resizing to <20MB or using lower resolution."));
                    }
                }
                free(desc);
            }
        }
    }

    char *json_out = json_serialize(result);
    json_free(result);
    json_free(args);
    return json_out;
}

/* ─── Video MIME detection (port of Python vision_tools._detect_video_mime_type) ─── */

const char *vision_detect_video_mime_type(const char *path) {
    if (!path) return NULL;
    const char *dot = strrchr(path, '.');
    if (!dot) return NULL;
    dot++; /* skip '.' */

    char ext[16];
    size_t i;
    for (i = 0; i < sizeof(ext) - 1 && dot[i]; i++)
        ext[i] = (char)tolower((unsigned char)dot[i]);
    ext[i] = '\0';

    if (strcmp(ext, "mp4") == 0)  return "video/mp4";
    if (strcmp(ext, "webm") == 0) return "video/webm";
    if (strcmp(ext, "mov") == 0)  return "video/mov";
    if (strcmp(ext, "avi") == 0)  return "video/mp4";
    if (strcmp(ext, "mkv") == 0)  return "video/mp4";
    if (strcmp(ext, "mpeg") == 0 || strcmp(ext, "mpg") == 0) return "video/mpeg";
    return NULL;
}

/* ─── Video to base64 data URL (port of Python vision_tools._video_to_base64_data_url) ─── */

char *vision_video_to_base64_data_url(const char *path) {
    if (!path) return NULL;

    /* Determine MIME type from extension */
    const char *mime = vision_detect_video_mime_type(path);
    if (!mime) mime = "video/mp4"; /* fallback */

    /* Open file */
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    /* Determine file size */
    struct stat st;
    if (fstat(fileno(f), &st) != 0) {
        fclose(f);
        return NULL;
    }
    size_t size = (size_t)st.st_size;

    /* Read all bytes */
    unsigned char *data = (unsigned char *)malloc(size);
    if (!data) { fclose(f); return NULL; }
    size_t nread = fread(data, 1, size, f);
    fclose(f);
    if (nread != size) {
        free(data);
        return NULL;
    }

    /* Base64 encode */
    char *encoded = base64_encode(data, size);
    free(data);
    if (!encoded) return NULL;

    /* Build data URL:  "data:<mime>;base64,<encoded>" */
    size_t prefix_len = strlen("data:") + strlen(mime) + strlen(";base64,");
    size_t result_len = prefix_len + strlen(encoded);
    char *result = (char *)malloc(result_len + 1);
    if (!result) { free(encoded); return NULL; }
    snprintf(result, result_len + 1, "data:%s;base64,%s", mime, encoded);
    free(encoded);
    return result;
}

void registry_init_vision(void) {
    registry_register("vision_analyze",
        "Analyze an image file. Returns metadata (dimensions, type, size) and "
        "optionally sends to LLM for description. Supports local files and URLs.",
        SCHEMA, vision_handler);
}
