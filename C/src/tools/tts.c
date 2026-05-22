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
      "\"voice\":{\"type\":\"string\",\"description\":\"Voice/model (provider-specific: e.g., 'alloy' for openai, '21m00Tcm4TlvDq8ikWAM' for elevenlabs)\"}"
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

    json_node_t *result = json_new_object();

    if (!text || !*text) {
        json_object_set(result, "error", json_new_string("Missing text"));
    } else {
        /* Generate default output path */
        char default_path[4096];
        if (!output_path) {
            output_path = tts_default_path(default_path, sizeof(default_path));
        }

        bool success = false;

        if (strcmp(provider, "elevenlabs") == 0) {
            success = tts_elevenlabs(text, voice, output_path);
        } else if (strcmp(provider, "openai") == 0) {
            success = tts_openai(text, voice, output_path);
        } else if (strcmp(provider, "xai") == 0) {
            success = tts_xai(text, voice, output_path);
        } else if (strcmp(provider, "edge") == 0) {
            /* edge-tts (Python) */
            char escaped[16384];
            tts_escape_text(text, escaped, sizeof(escaped));
            char cmd[65536];
            snprintf(cmd, sizeof(cmd),
                     "edge-tts --text '%s' --write-media '%s' 2>/dev/null",
                     escaped, output_path);
            success = (system(cmd) == 0);
        } else {
            /* default: espeak-ng */
            char escaped[16384];
            tts_escape_text(text, escaped, sizeof(escaped));
            char cmd[65536];
            if (strlen(escaped) < 10000) {
                snprintf(cmd, sizeof(cmd),
                         "espeak-ng -w '%s' -- '%s' 2>/dev/null",
                         output_path, escaped);
                success = (system(cmd) == 0);
            }
            /* Fallback to edge-tts if espeak fails */
            if (!success) {
                snprintf(cmd, sizeof(cmd),
                         "edge-tts --text '%s' --write-media '%s' 2>/dev/null",
                         escaped, output_path);
                success = (system(cmd) == 0);
            }
        }

        struct stat st;
        if (success && stat(output_path, &st) == 0 && st.st_size > 0) {
            json_object_set(result, "status", json_new_string("generated"));
            json_object_set(result, "output_path", json_new_string(output_path));
            json_object_set(result, "file_size", json_new_number((double)st.st_size));
            json_object_set(result, "provider", json_new_string(provider));
        } else {
            json_object_set(result, "error", json_new_string("TTS generation failed"));
        }
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
