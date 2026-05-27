/*
 * tts.c — Text-to-speech tool for Hermes C.
 * Converts text to speech audio using espeak-ng or edge-tts.
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "hermes_tool_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"text\":{\"type\":\"string\",\"description\":\"Text to convert to speech\"},"
      "\"output_path\":{\"type\":\"string\",\"description\":\"Optional output file path\"},"
      "\"provider\":{\"type\":\"string\",\"description\":\"TTS backend: espeak (default), edge, elevenlabs, openai, xai\"},"
      "\"voice\":{\"type\":\"string\",\"description\":\"Voice/model (provider-specific: e.g., 'alloy' for openai, '21m00Tcm4TlvDq8ikWAM' for elevenlabs)\"},"
      "\"speed\":{\"type\":\"number\",\"description\":\"Speech speed multiplier (0.5-2.0, default: 1.0). Edge/espeak providers.\"},"
      "\"max_chunk_duration_s\":{\"type\":\"integer\",\"description\":\"L10: Max seconds per audio chunk (default 60). Longer text is split.\"}"
    "},"
    "\"required\":[\"text\"]"
"}";

/*
 * TTS provider backends — shell-based (espeak, edge-tts) and API-based
 * (elevenlabs, openai, xai).
 */

/* Generate default output path if none provided */
static const char *tts_default_path(char *buf, size_t buf_size) {
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    char dir[4096];
    snprintf(dir, sizeof(dir), "%s/.hermes/audio_cache", home);
    mkdir(dir, 0755);
    snprintf(buf, buf_size, "%s/.hermes/audio_cache/tts_%ld.mp3",
             home, (long)time(NULL));
    return buf;
}

/* Escape single quotes for shell command */
static void tts_escape_text(const char *text, char *out, size_t out_size) {
    size_t pos = 0;
    for (const char *p = text; *p && pos < out_size - 4; p++) {
        if (*p == '\'') {
            if (pos + 4 < out_size) {
                out[pos++] = '\'';
                out[pos++] = '\\';
                out[pos++] = '\'';
                out[pos++] = '\'';
            }
        } else {
            out[pos++] = *p;
        }
    }
    out[pos] = '\0';
}

/* === API-based TTS backends (elevenlabs, openai, xai) === */

/* ElevenLabs TTS: POST https://api.elevenlabs.io/v1/text-to-speech/{voice_id} */
static bool tts_elevenlabs(const char *text, const char *voice, const char *output_path) {
    const char *api_key = tool_config_get("elevenlabs", "api_key");
    if (!api_key) api_key = getenv("ELEVENLABS_API_KEY");
    if (!api_key) return false;

    const char *voice_id = voice ? voice :
        tool_config_get("elevenlabs", "voice_id");
    if (!voice_id) voice_id = "21m00Tcm4TlvDq8ikWAM"; /* default Rachel */

    /* Build request body */
    json_t *body = json_object();
    json_set(body, "text", json_string(text));
    char url[512];
    snprintf(url, sizeof(url),
             "https://api.elevenlabs.io/v1/text-to-speech/%s", voice_id);

    char *payload = json_serialize(body);
    json_free(body);

    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header),
             "Accept: audio/mpeg, xi-api-key: %s, Content-Type: application/json",
             api_key);

    http_client_t *client = http_client_new(60);
    http_response_t *resp = http_request(client, HTTP_POST, url,
                                          auth_header, payload, strlen(payload));
    free(payload);

    if (!resp || resp->status != 200) {
        http_response_free(resp);
        http_client_free(client);
        return false;
    }

    /* Write binary response to file */
    bool ok = false;
    if (resp->body && resp->body_len > 0) {
        FILE *f = fopen(output_path, "wb");
        if (f) {
            fwrite(resp->body, 1, resp->body_len, f);
            fclose(f);
            ok = true;
        }
    }
    http_response_free(resp);
    http_client_free(client);
    return ok;
}

/* OpenAI TTS: POST https://api.openai.com/v1/audio/speech */
static bool tts_openai(const char *text, const char *voice, const char *output_path) {
    const char *api_key = tool_config_get("openai", "api_key");
    if (!api_key) api_key = getenv("OPENAI_API_KEY");
    if (!api_key) return false;

    const char *voice_id = voice ? voice : "alloy"; /* alloy/echo/fable/onyx/nova/shimmer */
    const char *model = tool_config_get("openai", "tts_model");
    if (!model) model = "tts-1";

    json_t *body = json_object();
    json_set(body, "model", json_string(model));
    json_set(body, "input", json_string(text));
    json_set(body, "voice", json_string(voice_id));
    json_set(body, "response_format", json_string("mp3"));

    char *payload = json_serialize(body);
    json_free(body);

    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header),
             "Authorization: Bearer %s, Content-Type: application/json",
             api_key);

    http_client_t *client = http_client_new(60);
    http_response_t *resp = http_request(client, HTTP_POST,
        "https://api.openai.com/v1/audio/speech",
        auth_header, payload, strlen(payload));
    free(payload);

    if (!resp || resp->status != 200) {
        http_response_free(resp);
        http_client_free(client);
        return false;
    }

    bool ok = false;
    if (resp->body && resp->body_len > 0) {
        FILE *f = fopen(output_path, "wb");
        if (f) {
            fwrite(resp->body, 1, resp->body_len, f);
            fclose(f);
            ok = true;
        }
    }
    http_response_free(resp);
    http_client_free(client);
    return ok;
}

/* xAI TTS: POST https://api.x.ai/v1/audio/speech (OpenAI-compatible) */
static bool tts_xai(const char *text, const char *voice, const char *output_path) {
    const char *api_key = tool_config_get("xai", "api_key");
    if (!api_key) api_key = getenv("XAI_API_KEY");
    if (!api_key) api_key = getenv("GROK_API_KEY");
    if (!api_key) return false;

    const char *voice_id = voice ? voice : "alloy";

    json_t *body = json_object();
    json_set(body, "model", json_string("grok-audio-1"));
    json_set(body, "input", json_string(text));
    json_set(body, "voice", json_string(voice_id));
    json_set(body, "response_format", json_string("mp3"));

    char *payload = json_serialize(body);
    json_free(body);

    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header),
             "Authorization: Bearer %s, Content-Type: application/json",
             api_key);

    http_client_t *client = http_client_new(60);
    http_response_t *resp = http_request(client, HTTP_POST,
        "https://api.x.ai/v1/audio/speech",
        auth_header, payload, strlen(payload));
    free(payload);

    if (!resp || resp->status != 200) {
        http_response_free(resp);
        http_client_free(client);
        return false;
    }

    bool ok = false;
    if (resp->body && resp->body_len > 0) {
        FILE *f = fopen(output_path, "wb");
        if (f) {
            fwrite(resp->body, 1, resp->body_len, f);
            fclose(f);
            ok = true;
        }
    }
    http_response_free(resp);
    http_client_free(client);
    return ok;
}

/* L10: Split text into chunks at sentence boundaries.
 * Returns array of strings (caller must free each + the array).
 * chunk_chars: approximate max characters per chunk (~15 chars/sec speech). */
static char **tts_chunk_text(const char *text, int chunk_chars, int *nchunks) {
    *nchunks = 0;
    if (!text || !*text) return NULL;
    size_t total = strlen(text);
    if ((int)total <= chunk_chars) {
        /* Single chunk */
        char **chunks = malloc(sizeof(char *));
        chunks[0] = strdup(text);
        *nchunks = 1;
        return chunks;
    }

    /* Estimate: allocate max chunks */
    int max_chunks = (int)(total / (chunk_chars / 2)) + 2;
    char **chunks = calloc((size_t)max_chunks, sizeof(char *));
    if (!chunks) return NULL;

    const char *start = text;
    int idx = 0;
    while (*start && idx < max_chunks) {
        if ((int)strlen(start) <= chunk_chars) {
            chunks[idx++] = strdup(start);
            break;
        }
        /* Find break point within chunk_chars: prefer sentence end (. ! ? then \n) */
        const char *end = start + chunk_chars;
        const char *break_at = NULL;
        const char *p = end;
        while (p > start) {
            if (*p == '.' || *p == '!' || *p == '?') {
                if (p + 1 == end || *(p+1) == ' ' || *(p+1) == '\n' || *(p+1) == '\0') {
                    break_at = p + 1;
                    break;
                }
            }
            p--;
        }
        if (!break_at) {
            /* Try newline */
            p = end;
            while (p > start) { if (*p == '\n') { break_at = p + 1; break; } p--; }
        }
        if (!break_at) {
            /* Try comma */
            p = end;
            while (p > start) { if (*p == ',') { break_at = p + 1; break; } p--; }
        }
        if (!break_at) {
            /* Fallback: word boundary */
            p = end;
            while (p > start && *p != ' ') p--;
            break_at = (p > start) ? p : end;
        }
        if (break_at <= start) break_at = start + chunk_chars;
        size_t len = (size_t)(break_at - start);
        char *chunk = malloc(len + 1);
        if (chunk) {
            memcpy(chunk, start, len);
            chunk[len] = '\0';
            chunks[idx++] = chunk;
        }
        start = break_at;
        while (*start == ' ') start++; /* trim leading spaces */
    }

    *nchunks = idx;
    return chunks;
}

char *tts_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *text = json_object_get_string(args, "text", NULL);
    const char *output_path = json_object_get_string(args, "output_path", NULL);
    const char *provider = json_object_get_string(args, "provider", "espeak");
    const char *voice = json_object_get_string(args, "voice", NULL);
    double speed = json_object_get_number(args, "speed", 1.0);
    int max_chunk_s = (int)json_object_get_number(args, "max_chunk_duration_s", 60);

    json_node_t *result = json_new_object();

    if (!text || !*text) {
        json_object_set(result, "error", json_new_string("Missing text"));
    } else {
        /* L10: Chunk text if too long (~15 chars/sec speech) */
        int chunk_chars = max_chunk_s * 15; /* ~15 chars/sec */
        int nchunks = 0;
        char **chunks = tts_chunk_text(text, chunk_chars, &nchunks);
        if (!chunks || nchunks == 0) {
            chunks = malloc(sizeof(char *));
            chunks[0] = strdup(text);
            nchunks = 1;
        }

        json_node_t *files = json_new_array();
        bool all_ok = true;

        for (int i = 0; i < nchunks; i++) {
            /* Generate per-chunk output path */
            char chunk_path[4096];
            if (output_path && nchunks == 1) {
                snprintf(chunk_path, sizeof(chunk_path), "%s", output_path);
            } else {
                char default_path[4096];
                const char *base = output_path ? output_path :
                    tts_default_path(default_path, sizeof(default_path));
                /* Insert _chunkN before extension */
                const char *dot = strrchr(base, '.');
                if (dot) {
                    size_t prelen = (size_t)(dot - base);
                    snprintf(chunk_path, sizeof(chunk_path), "%.*s_chunk%d%s",
                             (int)prelen, base, i, dot);
                } else {
                    snprintf(chunk_path, sizeof(chunk_path), "%s_chunk%d", base, i);
                }
            }

            bool ok = false;
            if (strcmp(provider, "elevenlabs") == 0) {
                ok = tts_elevenlabs(chunks[i], voice, chunk_path);
            } else if (strcmp(provider, "openai") == 0) {
                ok = tts_openai(chunks[i], voice, chunk_path);
            } else if (strcmp(provider, "xai") == 0) {
                ok = tts_xai(chunks[i], voice, chunk_path);
            } else if (strcmp(provider, "edge") == 0) {
                char escaped[16384];
                tts_escape_text(chunks[i], escaped, sizeof(escaped));
                char cmd[65536];
                snprintf(cmd, sizeof(cmd),
                         "edge-tts --text '%s' --write-media '%s' 2>/dev/null",
                         escaped, chunk_path);
                ok = (system(cmd) == 0);
            } else {
                char escaped[16384];
                tts_escape_text(chunks[i], escaped, sizeof(escaped));
                char cmd[65536];
                if (strlen(escaped) < 10000) {
                    int espeed = (int)(175.0 * speed); /* espeak default 175 wpm */
                    if (espeed < 80) espeed = 80;
                    if (espeed > 600) espeed = 600;
                    snprintf(cmd, sizeof(cmd),
                             "espeak-ng -s %d -w '%s' -- '%s' 2>/dev/null",
                             espeed, chunk_path, escaped);
                    ok = (system(cmd) == 0);
                }
                if (!ok) {
                    snprintf(cmd, sizeof(cmd),
                             "edge-tts --text '%s' --write-media '%s' 2>/dev/null",
                             escaped, chunk_path);
                    ok = (system(cmd) == 0);
                }
            }

            struct stat st;
            json_node_t *file_info = json_new_object();
            json_object_set(file_info, "chunk", json_new_number((double)i));
            json_object_set(file_info, "path", json_new_string(chunk_path));
            if (ok && stat(chunk_path, &st) == 0 && st.st_size > 0) {
                json_object_set(file_info, "status", json_new_string("generated"));
                json_object_set(file_info, "file_size", json_new_number((double)st.st_size));
            } else {
                json_object_set(file_info, "status", json_new_string("failed"));
                all_ok = false;
            }
            json_append(files, file_info);
            free(chunks[i]);
        }
        free(chunks);

        json_object_set(result, "files", files);
        json_object_set(result, "chunk_count", json_new_number((double)nchunks));
        json_object_set(result, "provider", json_new_string(provider));
        json_object_set(result, "speed", json_new_number(speed));
        if (all_ok)
            json_object_set(result, "status", json_new_string("generated"));
        else
            json_object_set(result, "status", json_new_string("partial"));
    }

    char *json_out = json_serialize(result);
    json_free(result);
    json_free(args);
    return json_out;
}

void registry_init_tts(void) {
    registry_register("text_to_speech",
        "Convert text to speech audio. Supports multiple backends: "
        "espeak-ng (default offline TTS), edge-tts (Python), "
        "elevenlabs, openai (tts-1), xai (grok-audio-1). "
        "Use 'provider' param to select backend. "
        "Returns path to generated audio file.",
        SCHEMA, tts_handler);
}
