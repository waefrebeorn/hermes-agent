/*
 * tts.c — Text-to-speech tool for Slermes C.
 * Converts text to speech audio using espeak-ng or edge-tts.
 */

#include "slermes.h"
#include "slermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"text\":{\"type\":\"string\",\"description\":\"Text to convert to speech\"},"
      "\"output_path\":{\"type\":\"string\",\"description\":\"Optional output file path\"}"
    "},"
    "\"required\":[\"text\"]"
"}";

char *tts_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *text = json_object_get_string(args, "text", NULL);
    const char *output_path = json_object_get_string(args, "output_path", NULL);

    json_node_t *result = json_new_object();

    if (!text || !*text) {
        json_object_set(result, "error", json_new_string("Missing text"));
    } else {
        /* Generate output path if not provided */
        char default_path[4096];
        if (!output_path) {
            const char *home = getenv("HOME");
            if (!home) home = "/tmp";
            snprintf(default_path, sizeof(default_path),
                     "%s/.slermes/audio_cache/tts_%ld.wav",
                     home, (long)time(NULL));

            /* Ensure directory exists */
            char dir[4096];
            snprintf(dir, sizeof(dir), "%s/.slermes/audio_cache", home);
            mkdir(dir, 0755);
            output_path = default_path;
        }

        /* Escape single quotes for shell */
        char escaped[16384];
        size_t pos = 0;
        for (const char *p = text; *p && pos < sizeof(escaped) - 4; p++) {
            if (*p == '\'') {
                if (pos + 4 < sizeof(escaped)) {
                    escaped[pos++] = '\'';
                    escaped[pos++] = '\\';
                    escaped[pos++] = '\'';
                    escaped[pos++] = '\'';
                }
            } else {
                escaped[pos++] = *p;
            }
        }
        escaped[pos] = '\0';

        /* Try espeak-ng first, fall back to edge-tts */
        char cmd[65536];
        int exit_code;

        /* Check espeak-ng length limit (~10K chars) */
        if (strlen(escaped) < 10000) {
            snprintf(cmd, sizeof(cmd),
                     "espeak-ng -w '%s' -- '%s' 2>/dev/null",
                     output_path, escaped);
            exit_code = system(cmd);
        } else {
            exit_code = 1; /* Force fallback */
        }

        /* Fallback: use edge-tts (Python) */
        if (exit_code != 0) {
            snprintf(cmd, sizeof(cmd),
                     "edge-tts --text '%s' --write-media '%s' 2>/dev/null",
                     escaped, output_path);
            exit_code = system(cmd);
        }

        struct stat st;
        if (exit_code == 0 && stat(output_path, &st) == 0 && st.st_size > 0) {
            json_object_set(result, "status", json_new_string("generated"));
            json_object_set(result, "output_path", json_new_string(output_path));
            json_object_set(result, "file_size", json_new_number((double)st.st_size));
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
        "Convert text to speech audio. Uses espeak-ng or edge-tts. "
        "Returns path to generated audio file.",
        SCHEMA, tts_handler);
}
