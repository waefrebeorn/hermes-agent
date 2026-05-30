/*
 * transcribe.c — Audio transcription for Hermes C.
 * Port of Python tools/transcription_tools.py.
 * Supports groq, openai, xai providers via HTTP multipart POST.
 *
 * MIT License — WuBu Hermes Project
 */

#include "transcribe.h"
#include "http.h"
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_http.h"
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
#include <time.h>

/* ================================================================
 *  Supported formats (sorted for bsearch)
 * ================================================================ */

static const char *SUPPORTED_FORMATS[] = {
    ".aac", ".flac", ".m4a", ".mp3", ".mp4",
    ".mpeg", ".mpga", ".ogg", ".wav", ".webm"
};

const char *transcribe_supported_formats[] = {
    ".aac", ".flac", ".m4a", ".mp3", ".mp4",
    ".mpeg", ".mpga", ".ogg", ".wav", ".webm",
    NULL
};

const int transcribe_supported_format_count = 10;

static int strcmp_wrapper(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

bool transcribe_is_supported_format(const char *ext) {
    if (!ext || !*ext) return false;
    /* Case-insensitive comparison */
    char lower[16];
    size_t i;
    for (i = 0; ext[i] && i < sizeof(lower) - 1; i++)
        lower[i] = (ext[i] >= 'A' && ext[i] <= 'Z') ? ext[i] + 32 : ext[i];
    lower[i] = '\0';
    const char *key = lower;
    return bsearch(&key, SUPPORTED_FORMATS,
                   sizeof(SUPPORTED_FORMATS) / sizeof(SUPPORTED_FORMATS[0]),
                   sizeof(char *), strcmp_wrapper) != NULL;
}

/* ================================================================
 *  JSON result helpers
 * ================================================================ */

static char *make_result(bool success, const char *transcript,
                          const char *error, const char *provider) {
    json_t *j = json_object();
    json_set(j, "success", json_bool(success));
    json_set(j, "transcript", json_string(transcript ? transcript : ""));
    if (error && *error)
        json_set(j, "error", json_string(error));
    if (provider && *provider)
        json_set(j, "provider", json_string(provider));
    char *s = json_serialize(j);
    json_free(j);
    return s;
}

/* ================================================================
 *  Multipart form-data builder (adapted from feishu.c)
 * ================================================================ */

typedef struct {
    const char *name;
    const char *filename;
    const char *data;
    size_t      data_len;
} multipart_part_t;

static char *build_multipart_body(const char *boundary,
                                   const multipart_part_t *parts,
                                   size_t *out_len) {
    if (!boundary || !parts || !out_len) return NULL;

    size_t total = 0;
    for (const multipart_part_t *p = parts; p->name; p++) {
        total += 2 + strlen(boundary) + 2;            /* --boundary\r\n */
        total += 44 + strlen(p->name);                 /* Content-Disposition: form-data; name="..." */
        if (p->filename)
            total += 12 + strlen(p->filename);         /* ; filename="..." */
        total += 2;                                    /* \r\n */
        if (p->filename)
            total += 38;                               /* Content-Type: application/octet-stream\r\n */
        total += 2;                                    /* \r\n (blank line before data) */
        total += p->data_len;
        total += 2;                                    /* \r\n */
    }
    total += 2 + strlen(boundary) + 2 + 2;             /* --boundary--\r\n */

    char *buf = (char *)malloc(total + 1);
    if (!buf) return NULL;

    size_t pos = 0;
    for (const multipart_part_t *p = parts; p->name; p++) {
        pos += snprintf(buf + pos, total - pos + 1, "--%s\r\n", boundary);
        if (p->filename) {
            pos += snprintf(buf + pos, total - pos + 1,
                            "Content-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\n"
                            "Content-Type: application/octet-stream\r\n\r\n",
                            p->name, p->filename);
        } else {
            pos += snprintf(buf + pos, total - pos + 1,
                            "Content-Disposition: form-data; name=\"%s\"\r\n\r\n",
                            p->name);
        }
        if (p->data_len > 0) {
            memcpy(buf + pos, p->data, p->data_len);
            pos += p->data_len;
        }
        pos += snprintf(buf + pos, total - pos + 1, "\r\n");
    }
    pos += snprintf(buf + pos, total - pos + 1, "--%s--\r\n", boundary);
    buf[pos] = '\0';
    *out_len = pos;
    return buf;
}

static void generate_boundary(char *buf, size_t buf_size) {
    const char *chars = "abcdefghijklmnopqrstuvwxyz0123456789";
    srand((unsigned int)(time(NULL) ^ (getpid() << 16)));
    size_t len = buf_size - 1;
    if (len > 48) len = 48;
    snprintf(buf, buf_size, "----HermesFormBoundary");
    size_t pos = strlen(buf);
    for (size_t i = 0; i < len - pos; i++)
        buf[pos + i] = chars[rand() % 36];
    buf[pos + (len - pos)] = '\0';
}

/* ================================================================
 *  File reading helper
 * ================================================================ */

static char *read_file_binary(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) { *out_len = 0; return NULL; }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < 0 || (size_t)len > TRANSCRIBE_MAX_FILE_SIZE) {
        fclose(f);
        *out_len = 0;
        return NULL;
    }
    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf) { fclose(f); *out_len = 0; return NULL; }
    size_t n = fread(buf, 1, (size_t)len, f);
    fclose(f);
    buf[n] = '\0';
    *out_len = n;
    return buf;
}

/* ================================================================
 *  File validation
 * ================================================================ */

char *transcribe_validate_file(const char *file_path) {
    if (!file_path || !*file_path)
        return make_result(false, "", "No file path provided", NULL);

    struct stat st;
    if (stat(file_path, &st) != 0)
        return make_result(false, "", "Audio file not found", NULL);

    if (!S_ISREG(st.st_mode))
        return make_result(false, "", "Path is not a file", NULL);

    /* Check extension */
    const char *dot = strrchr(file_path, '.');
    if (!dot || !transcribe_is_supported_format(dot)) {
        char err[256];
        snprintf(err, sizeof(err),
                 "Unsupported format: %s. Supported: .aac, .flac, .m4a, .mp3, .mp4, .mpeg, .mpga, .ogg, .wav, .webm",
                 dot ? dot : "(none)");
        return make_result(false, "", err, NULL);
    }

    /* Check size */
    if (st.st_size > TRANSCRIBE_MAX_FILE_SIZE) {
        char err[128];
        snprintf(err, sizeof(err),
                 "File too large: %.1f MB (max 25 MB)",
                 (double)st.st_size / (1024.0 * 1024.0));
        return make_result(false, "", err, NULL);
    }

    return NULL;  /* Valid */
}

/* ================================================================
 *  Provider detection
 * ================================================================ */

/* Check if an env var is set and non-empty */
static bool has_env(const char *name) {
    const char *val = getenv(name);
    return val && *val;
}

static const char *detect_provider(void) {
    /* Check explicit config via env */
    const char *explicit = getenv("HERMES_STT_PROVIDER");
    if (explicit && *explicit) {
        if (strcmp(explicit, "groq") == 0)
            return has_env("GROQ_API_KEY") ? TRANSCRIBE_PROVIDER_GROQ : NULL;
        if (strcmp(explicit, "openai") == 0) {
            if (has_env("VOICE_TOOLS_OPENAI_KEY") || has_env("OPENAI_API_KEY"))
                return TRANSCRIBE_PROVIDER_OPENAI;
            return NULL;
        }
        if (strcmp(explicit, "xai") == 0)
            return has_env("XAI_API_KEY") ? TRANSCRIBE_PROVIDER_XAI : NULL;
        if (strcmp(explicit, "deepgram") == 0)
            return has_env("DEEPGRAM_API_KEY") ? TRANSCRIBE_PROVIDER_DEEPGRAM : NULL;
        return explicit;  /* Unknown — let it fail downstream */
    }

    /* Auto-detect: groq (free) > openai (paid) > xai */
    if (has_env("GROQ_API_KEY"))
        return TRANSCRIBE_PROVIDER_GROQ;
    if (has_env("VOICE_TOOLS_OPENAI_KEY") || has_env("OPENAI_API_KEY"))
        return TRANSCRIBE_PROVIDER_OPENAI;
    if (has_env("XAI_API_KEY"))
        return TRANSCRIBE_PROVIDER_XAI;
    if (has_env("DEEPGRAM_API_KEY"))
        return TRANSCRIBE_PROVIDER_DEEPGRAM;

    return NULL;
}

/* ================================================================
 *  Model normalization
 * ================================================================ */

static const char *normalize_model_for_groq(const char *model) {
    /* OpenAI models aren't available on Groq — map to default */
    if (!model) return TRANSCRIBE_DEFAULT_MODEL_GROQ;
    if (strcmp(model, "whisper-1") == 0 ||
        strcmp(model, "gpt-4o-mini-transcribe") == 0 ||
        strcmp(model, "gpt-4o-transcribe") == 0)
        return TRANSCRIBE_DEFAULT_MODEL_GROQ;
    return model;
}

static const char *normalize_model_for_openai(const char *model) {
    /* Groq-only models aren't available on OpenAI */
    if (!model) return TRANSCRIBE_DEFAULT_MODEL_OPENAI;
    static const char *groq_only[] = {"whisper-large-v3", "whisper-large-v3-turbo", "distil-whisper-large-v3-en"};
    for (size_t i = 0; i < sizeof(groq_only)/sizeof(groq_only[0]); i++) {
        if (strcmp(model, groq_only[i]) == 0)
            return TRANSCRIBE_DEFAULT_MODEL_OPENAI;
    }
    return model;
}

/* ================================================================
 *  Provider: Groq Whisper API
 * ================================================================ */

static char *transcribe_groq(const char *file_path, const char *model) {
    const char *api_key = getenv("GROQ_API_KEY");
    if (!api_key || !*api_key)
        return make_result(false, "", "GROQ_API_KEY not set", TRANSCRIBE_PROVIDER_GROQ);

    const char *model_name = normalize_model_for_groq(model);
    const char *base_url = getenv("GROQ_BASE_URL");
    if (!base_url || !*base_url)
        base_url = "https://api.groq.com/openai/v1";

    /* Read audio file */
    size_t file_len;
    char *file_data = read_file_binary(file_path, &file_len);
    if (!file_data)
        return make_result(false, "", "Failed to read audio file", TRANSCRIBE_PROVIDER_GROQ);

    /* Get filename from path */
    const char *filename = strrchr(file_path, '/');
    filename = filename ? filename + 1 : file_path;

    /* Build multipart body */
    char boundary[64];
    generate_boundary(boundary, sizeof(boundary));

    /* Get model string length for size estimation */
    size_t model_len = strlen(model_name);

    /* model part */
    multipart_part_t parts[] = {
        {"model",    NULL, model_name, model_len},
        {"file",     filename, file_data, file_len},
        {NULL, NULL, NULL, 0}
    };

    size_t body_len;
    char *body = build_multipart_body(boundary, parts, &body_len);
    free(file_data);
    if (!body)
        return make_result(false, "", "Failed to build multipart body", TRANSCRIBE_PROVIDER_GROQ);

    /* Build URL */
    char url[512];
    snprintf(url, sizeof(url), "%s/audio/transcriptions", base_url);

    /* Build Content-Type header with boundary */
    char content_type[128];
    snprintf(content_type, sizeof(content_type),
             "Content-Type: multipart/form-data; boundary=%s", boundary);

    /* Build auth header */
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header),
             "Authorization: Bearer %s\r\n%s", api_key, content_type);

    /* Do HTTP POST */
    http_t *h = http_new(60);  /* 60s timeout for audio upload */
    if (!h) {
        free(body);
        return make_result(false, "", "Failed to create HTTP client", TRANSCRIBE_PROVIDER_GROQ);
    }

    http_resp_t *resp = http_request(h, HTTP_POST, url, auth_header, body, body_len);
    free(body);
    if (!resp) {
        http_free(h);
        return make_result(false, "", "HTTP request failed", TRANSCRIBE_PROVIDER_GROQ);
    }

    char *result;
    if (resp->status != 200) {
        char err[512];
        /* Try to extract error from JSON response */
        json_t *j = json_parse(resp->body, NULL);
        if (j) {
            json_t *err_obj = json_obj_get(j, "error");
            const char *msg = err_obj ? json_get_str(err_obj, "message", "") : NULL;
            snprintf(err, sizeof(err), "Groq API error (HTTP %d): %s",
                     resp->status, msg ? msg : resp->body);
            json_free(j);
        } else {
            snprintf(err, sizeof(err), "Groq API error (HTTP %d): %s",
                     resp->status, resp->body);
        }
        result = make_result(false, "", err, TRANSCRIBE_PROVIDER_GROQ);
    } else {
        /* Parse JSON response */
        json_t *j = json_parse(resp->body, NULL);
        if (!j) {
            /* Response may be plain text (response_format=text) */
            const char *text = resp->body;
            while (*text == ' ' || *text == '\n' || *text == '\r') text++;
            if (*text && *text != '{') {
                result = make_result(true, text, NULL, TRANSCRIBE_PROVIDER_GROQ);
            } else {
                result = make_result(false, "", "Failed to parse Groq response",
                                     TRANSCRIBE_PROVIDER_GROQ);
            }
        } else {
            const char *text = json_get_str(j, "text", "");
            if (text && *text)
                result = make_result(true, text, NULL, TRANSCRIBE_PROVIDER_GROQ);
            else
                result = make_result(false, "", "Groq returned empty transcript",
                                     TRANSCRIBE_PROVIDER_GROQ);
            json_free(j);
        }
    }

    http_resp_free(resp);
    http_free(h);
    return result;
}

/* ================================================================
 *  Provider: OpenAI Whisper API
 * ================================================================ */

static char *transcribe_openai(const char *file_path, const char *model) {
    const char *api_key = getenv("VOICE_TOOLS_OPENAI_KEY");
    if (!api_key || !*api_key)
        api_key = getenv("OPENAI_API_KEY");
    if (!api_key || !*api_key)
        return make_result(false, "", "No OpenAI API key (set VOICE_TOOLS_OPENAI_KEY or OPENAI_API_KEY)",
                           TRANSCRIBE_PROVIDER_OPENAI);

    const char *model_name = normalize_model_for_openai(model);
    const char *base_url = getenv("STT_OPENAI_BASE_URL");
    if (!base_url || !*base_url)
        base_url = "https://api.openai.com/v1";

    /* Read audio file */
    size_t file_len;
    char *file_data = read_file_binary(file_path, &file_len);
    if (!file_data)
        return make_result(false, "", "Failed to read audio file", TRANSCRIBE_PROVIDER_OPENAI);

    const char *filename = strrchr(file_path, '/');
    filename = filename ? filename + 1 : file_path;

    /* Build multipart */
    char boundary[64];
    generate_boundary(boundary, sizeof(boundary));

    size_t model_len = strlen(model_name);
    multipart_part_t parts[] = {
        {"model",           NULL, model_name, model_len},
        {"file",            filename, file_data, file_len},
        {"response_format", NULL, "json", 4},
        {NULL, NULL, NULL, 0}
    };

    size_t body_len;
    char *body = build_multipart_body(boundary, parts, &body_len);
    free(file_data);
    if (!body)
        return make_result(false, "", "Failed to build multipart body", TRANSCRIBE_PROVIDER_OPENAI);

    char url[512];
    snprintf(url, sizeof(url), "%s/audio/transcriptions", base_url);

    char content_type[128];
    snprintf(content_type, sizeof(content_type),
             "Content-Type: multipart/form-data; boundary=%s", boundary);
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header),
             "Authorization: Bearer %s\r\n%s", api_key, content_type);

    http_t *h = http_new(60);
    if (!h) { free(body); return make_result(false, "", "Failed to create HTTP client", TRANSCRIBE_PROVIDER_OPENAI); }

    http_resp_t *resp = http_request(h, HTTP_POST, url, auth_header, body, body_len);
    free(body);
    if (!resp) { http_free(h); return make_result(false, "", "HTTP request failed", TRANSCRIBE_PROVIDER_OPENAI); }

    char *result;
    if (resp->status != 200) {
        char err[512];
        json_t *j = json_parse(resp->body, NULL);
        if (j) {
            json_t *err_obj = json_obj_get(j, "error");
            const char *msg = err_obj ? json_get_str(err_obj, "message", "") : NULL;
            snprintf(err, sizeof(err), "OpenAI API error (HTTP %d): %s",
                     resp->status, msg ? msg : resp->body);
            json_free(j);
        } else {
            snprintf(err, sizeof(err), "OpenAI API error (HTTP %d): %s",
                     resp->status, resp->body);
        }
        result = make_result(false, "", err, TRANSCRIBE_PROVIDER_OPENAI);
    } else {
        json_t *j = json_parse(resp->body, NULL);
        if (!j) {
            result = make_result(false, "", "Failed to parse OpenAI response",
                                 TRANSCRIBE_PROVIDER_OPENAI);
        } else {
            /* OpenAI returns {"text": "..."} when response_format=json */
            const char *text = json_get_str(j, "text", "");
            if (!text || !*text)
                text = resp->body;  /* fallback to raw */
            result = make_result(true, text && *text ? text : resp->body, NULL,
                                 TRANSCRIBE_PROVIDER_OPENAI);
            json_free(j);
        }
    }

    http_resp_free(resp);
    http_free(h);
    return result;
}

/* ================================================================
 *  Provider: xAI Grok STT API
 * ================================================================ */

static char *transcribe_xai(const char *file_path, const char *model) {
    (void)model;  /* xAI doesn't use a model parameter */

    const char *api_key = getenv("XAI_API_KEY");
    if (!api_key || !*api_key)
        return make_result(false, "", "XAI_API_KEY not set", TRANSCRIBE_PROVIDER_XAI);

    const char *base_url = getenv("XAI_STT_BASE_URL");
    if (!base_url || !*base_url)
        base_url = "https://api.x.ai/v1";

    /* Read audio file */
    size_t file_len;
    char *file_data = read_file_binary(file_path, &file_len);
    if (!file_data)
        return make_result(false, "", "Failed to read audio file", TRANSCRIBE_PROVIDER_XAI);

    const char *filename = strrchr(file_path, '/');
    filename = filename ? filename + 1 : file_path;

    /* xAI STT uses POST /v1/stt with multipart, optional parameters */
    char boundary[64];
    generate_boundary(boundary, sizeof(boundary));

    const char *language = getenv("HERMES_LOCAL_STT_LANGUAGE");
    if (!language || !*language) language = "en";

    multipart_part_t text_parts[] = {
        {"file",     filename, file_data, file_len},
        {"language", NULL,     language,  strlen(language)},
        {NULL, NULL, NULL, 0}
    };

    size_t body_len;
    char *body = build_multipart_body(boundary, text_parts, &body_len);
    free(file_data);
    if (!body)
        return make_result(false, "", "Failed to build multipart body", TRANSCRIBE_PROVIDER_XAI);

    char url[512];
    snprintf(url, sizeof(url), "%s/stt", base_url);

    char content_type[128];
    snprintf(content_type, sizeof(content_type),
             "Content-Type: multipart/form-data; boundary=%s", boundary);
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header),
             "Authorization: Bearer %s\r\n%s", api_key, content_type);

    http_t *h = http_new(120);  /* 120s timeout — xAI STT can be slow */
    if (!h) { free(body); return make_result(false, "", "Failed to create HTTP client", TRANSCRIBE_PROVIDER_XAI); }

    http_resp_t *resp = http_request(h, HTTP_POST, url, auth_header, body, body_len);
    free(body);
    if (!resp) { http_free(h); return make_result(false, "", "HTTP request failed", TRANSCRIBE_PROVIDER_XAI); }

    char *result;
    if (resp->status != 200) {
        char err[512];
        json_t *j = json_parse(resp->body, NULL);
        if (j) {
            json_t *err_obj = json_obj_get(j, "error");
            const char *msg = err_obj ? json_get_str(err_obj, "message", "") : NULL;
            snprintf(err, sizeof(err), "xAI STT error (HTTP %d): %s",
                     resp->status, msg ? msg : resp->body);
            json_free(j);
        } else {
            snprintf(err, sizeof(err), "xAI STT error (HTTP %d): %s",
                     resp->status, resp->body);
        }
        result = make_result(false, "", err, TRANSCRIBE_PROVIDER_XAI);
    } else {
        json_t *j = json_parse(resp->body, NULL);
        if (!j) {
            result = make_result(false, "", "Failed to parse xAI STT response",
                                 TRANSCRIBE_PROVIDER_XAI);
        } else {
            const char *text = json_get_str(j, "text", "");
            if (text && *text)
                result = make_result(true, text, NULL, TRANSCRIBE_PROVIDER_XAI);
            else
                result = make_result(false, "", "xAI STT returned empty transcript",
                                     TRANSCRIBE_PROVIDER_XAI);
            json_free(j);
        }
    }

    http_resp_free(resp);
    http_free(h);
    return result;
}

/* ================================================================
 *  Public API
 * ================================================================ */

char *transcribe_audio(const char *file_path, const char *model) {
    /* Validate input */
    char *validation_err = transcribe_validate_file(file_path);
    if (validation_err)
        return validation_err;  /* Already a JSON result string */

    /* Check if STT is disabled */
    const char *disabled = getenv("HERMES_STT_DISABLED");
    if (disabled && (strcmp(disabled, "true") == 0 || strcmp(disabled, "1") == 0))
        return make_result(false, "", "STT is disabled (HERMES_STT_DISABLED=true)", NULL);

    /* Detect provider */
    const char *provider = detect_provider();
    if (!provider) {
        return make_result(false, "", "No STT provider available. Set GROQ_API_KEY (free), "
                           "VOICE_TOOLS_OPENAI_KEY or OPENAI_API_KEY (paid), or XAI_API_KEY.",
                           NULL);
    }

    /* Dispatch */
    if (strcmp(provider, TRANSCRIBE_PROVIDER_GROQ) == 0)
        return transcribe_groq(file_path, model);
    if (strcmp(provider, TRANSCRIBE_PROVIDER_OPENAI) == 0)
        return transcribe_openai(file_path, model);
    if (strcmp(provider, TRANSCRIBE_PROVIDER_XAI) == 0)
        return transcribe_xai(file_path, model);

    return make_result(false, "", "Unknown STT provider", provider);
}
