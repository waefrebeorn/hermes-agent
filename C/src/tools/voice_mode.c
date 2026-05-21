/*
 * voice_mode.c — Voice input/output mode for Hermes C.
 * Wraps arecord + sox for microphone input + TTS for output.
 * Speech recognition via API (Whisper) or external asr binary.
 *
 * Phase 131-135: Voice mode parity with Python voice_mode.py.
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ================================================================
 *  Configuration
 * ================================================================ */

static int g_voice_enabled = 0;
static char g_voice_device[128] = "default";  /* ALSA device */
static char g_voice_asr_cmd[512] = "";        /* External STT command */
static int g_voice_timeout = 5;               /* Record timeout seconds */

void voice_set_enabled(int enabled) {
    g_voice_enabled = enabled;
}

int voice_is_enabled(void) {
    return g_voice_enabled;
}

void voice_set_device(const char *dev) {
    if (dev) snprintf(g_voice_device, sizeof(g_voice_device), "%s", dev);
}

void voice_set_asr_cmd(const char *cmd) {
    if (cmd) snprintf(g_voice_asr_cmd, sizeof(g_voice_asr_cmd), "%s", cmd);
}

/* ================================================================
 *  Audio Recording
 * ================================================================ */

/* Record audio from microphone. Returns path to WAV file, or NULL.
 * Caller must free the returned path. */
char *voice_record(const char *output_path, int max_seconds) {
    if (max_seconds <= 0) max_seconds = g_voice_timeout;
    if (!output_path) output_path = "/tmp/hermes_voice_input.wav";

    /* Try arecord first, fall back to sox */
    char cmd[1024];
    int r = snprintf(cmd, sizeof(cmd),
        "arecord -q -D %s -f cd -t wav -d %d %s 2>/dev/null || "
        "sox -q -d -t wav %s trim 0 %d 2>/dev/null",
        g_voice_device, max_seconds, output_path,
        output_path, max_seconds);

    if (r < 0 || (size_t)r >= sizeof(cmd)) return NULL;

    int rc = system(cmd);
    if (rc != 0) return NULL;

    /* Check file was created */
    if (access(output_path, F_OK) != 0) return NULL;

    return strdup(output_path);
}

/* ================================================================
 *  Speech-to-Text (STT)
 * ================================================================ */

/* Transcribe audio file to text using configured ASR command or API.
 * Returns malloc'd text or NULL. */
char *voice_transcribe(const char *audio_path) {
    if (!audio_path) return NULL;

    /* If external ASR command configured, use it */
    if (g_voice_asr_cmd[0]) {
        char cmd[4096];
        int r = snprintf(cmd, sizeof(cmd), "%s %s 2>/dev/null", g_voice_asr_cmd, audio_path);
        if (r < 0 || (size_t)r >= sizeof(cmd)) return NULL;

        FILE *fp = popen(cmd, "r");
        if (!fp) return NULL;

        char result[4096];
        result[0] = '\0';
        if (fgets(result, (int)sizeof(result) - 1, fp)) {
            size_t len = strlen(result);
            while (len > 0 && (result[len-1] == '\n' || result[len-1] == '\r'))
                result[--len] = '\0';
        }
        pclose(fp);
        return result[0] ? strdup(result) : NULL;
    }

    /* Default: try to use a local model or API */
    /* Check for whisper.cpp or similar */
    if (access("whisper", X_OK) == 0 || access("/usr/local/bin/whisper", X_OK) == 0) {
        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "whisper %s --language en --output-txt -o /tmp 2>/dev/null "
                 "&& cat /tmp/$(basename %s .wav).txt 2>/dev/null",
                 audio_path, audio_path);
        FILE *fp = popen(cmd, "r");
        if (fp) {
            char result[4096]; result[0] = '\0';
            fgets(result, (int)sizeof(result) - 1, fp);
            pclose(fp);
            if (result[0]) return strdup(result);
        }
    }

    /* No STT available */
    return NULL;
}

/* ================================================================
 *  Voice Loop (single turn: listen → transcribe → return text)
 * ================================================================ */

/* Record and transcribe one voice input. Returns transcribed text or NULL. */
char *voice_listen(void) {
    printf("🎤 Listening (timeout: %ds)...\n", g_voice_timeout);
    fflush(stdout);

    char *audio = voice_record(NULL, g_voice_timeout);
    if (!audio) {
        printf("❌ Recording failed. Check microphone.\n");
        return NULL;
    }

    printf("🔄 Transcribing...\n");
    fflush(stdout);

    char *text = voice_transcribe(audio);
    free(audio);

    if (!text) {
        printf("❌ Transcription failed.\n");
        return NULL;
    }

    printf("📝 You said: %s\n", text);
    return text;
}

/* Speak text via TTS */
void voice_speak(const char *text) {
    if (!text) return;
    printf("🔊 Speaking...\n");
    fflush(stdout);

    /* Try espeak first, fall back to TTS tool */
    char cmd[8192];
    int r = snprintf(cmd, sizeof(cmd),
        "espeak \"%s\" 2>/dev/null || say \"%s\" 2>/dev/null || echo 'TTS not available'",
        text, text);
    if (r > 0 && (size_t)r < sizeof(cmd))
        system(cmd);
}

/* ================================================================
 *  Toggle handler (called from CLI /voice command)
 * ================================================================ */

/* Register voice-related tools */
static char *voice_listen_handler(const char *args_json, const char *task_id) {
    (void)args_json; (void)task_id;
    if (!g_voice_enabled)
        return strdup("{\"error\": \"Voice mode not enabled. Use /voice to enable.\"}");

    char *text = voice_listen();
    if (!text)
        return strdup("{\"error\": \"Voice input failed\"}");

    /* Return transcribed text for the agent to process */
    char *result = (char *)malloc(strlen(text) + 64);
    if (!result) { free(text); return strdup("{\"error\": \"OOM\"}"); }
    snprintf(result, strlen(text) + 64,
        "{\"transcript\": \"%s\", \"status\": \"ok\"}", text);
    free(text);
    return result;
}

static char *voice_speak_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\": \"Invalid JSON\"}");

    const char *text = json_get_str(args, "text", "");
    json_free(args);

    if (!text || !*text)
        return strdup("{\"error\": \"Missing 'text' parameter\"}");

    voice_speak(text);
    return strdup("{\"status\": \"spoken\"}");
}

void registry_init_voice(void) {
    registry_register("voice_listen",
        "Record from microphone and return transcribed text.",
        "{\"type\":\"object\",\"properties\":{}}",
        voice_listen_handler);

    registry_register("voice_speak",
        "Speak the given text using text-to-speech.",
        "{\"type\":\"object\",\"properties\":{"
        "\"text\":{\"type\":\"string\"}"
        "},\"required\":[\"text\"]}",
        voice_speak_handler);
}
