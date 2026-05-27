/*
 * transcribe.c — Audio transcription tool for Hermes C.
 * Wraps lib/libtranscribe/ as a registered tool.
 * Port of Python tools/transcription_tools.py tool registration.
 */

#include "hermes.h"
#include "hermes_json.h"
#include "transcribe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"file_path\":{\"type\":\"string\",\"description\":\"Absolute path to audio file (mp3, wav, ogg, m4a, etc.)\"},"
      "\"model\":{\"type\":\"string\",\"description\":\"Provider model name, e.g. 'whisper-large-v3-turbo' (groq), 'whisper-1' (openai), 'grok-stt' (xai). Omit for auto-detect.\"},"
      "\"language\":{\"type\":\"string\",\"description\":\"Language code (e.g., 'en', 'fr', 'de', 'zh'). Improves accuracy for known language.\"}"
    "},"
    "\"required\":[\"file_path\"]"
"}";

char *transcribe_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"success\":false,\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) {
        free(err);
        return strdup("{\"success\":false,\"error\":\"JSON parse error\"}");
    }

    const char *file_path = json_object_get_string(args, "file_path", NULL);
    if (!file_path || !*file_path) {
        json_free(args);
        return strdup("{\"success\":false,\"error\":\"file_path is required\"}");
    }

    const char *model = json_object_get_string(args, "model", NULL);
    const char *language = json_object_get_string(args, "language", NULL);

    /* Validate file first */
    char *validation = transcribe_validate_file(file_path);
    if (validation) {
        json_free(args);
        return validation; /* already JSON */
    }

    /* Transcribe */
    char *result = transcribe_audio(file_path, model);
    json_free(args);

    if (!result) {
        return strdup("{\"success\":false,\"error\":\"Transcription failed (no result)\"}");
    }

    return result;
}

__attribute__((constructor))
static void init_transcribe(void) {
    registry_register("transcribe", "Transcribe audio to text using Whisper (groq/openai/xai).",
                       SCHEMA, transcribe_handler);
}
