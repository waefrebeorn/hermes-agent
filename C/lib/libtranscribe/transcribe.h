#ifndef LIBTRANSCRIBE_H
#define LIBTRANSCRIBE_H

/*
 * libtranscribe.h — Audio transcription for Hermes C.
 * Port of Python tools/transcription_tools.py.
 *
 * Provides speech-to-text via three HTTP providers:
 *   - groq:  Groq Whisper API (free tier)
 *   - openai: OpenAI Whisper API (paid)
 *   - xai:   xAI Grok STT API
 *
 * Local (faster-whisper) and local_command (whisper CLI) are NOT ported
 * — they require Python/ffmpeg subprocess. Mistral Voxtral is quarantined.
 *
 * Public API:
 *   char *transcribe_audio(const char *file_path, const char *model);
 *     Returns a malloc'd JSON string. Caller free()s.
 *     JSON keys: success(bool), transcript(str), error(str), provider(str).
 *
 * Supported formats: mp3, mp4, mpeg, mpga, m4a, wav, webm, ogg, aac, flac
 * Max file size: 25 MB
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* === Constants === */

#define TRANSCRIBE_MAX_FILE_SIZE (25 * 1024 * 1024)  /* 25 MB */

#define TRANSCRIBE_PROVIDER_GROQ    "groq"
#define TRANSCRIBE_PROVIDER_OPENAI  "openai"
#define TRANSCRIBE_PROVIDER_XAI    "xai"
#define TRANSCRIBE_PROVIDER_DEEPGRAM "deepgram"
#define TRANSCRIBE_PROVIDER_NONE    "none"

#define TRANSCRIBE_DEFAULT_MODEL_GROQ   "whisper-large-v3-turbo"
#define TRANSCRIBE_DEFAULT_MODEL_OPENAI "whisper-1"
#define TRANSCRIBE_DEFAULT_MODEL_XAI    "grok-stt"
#define TRANSCRIBE_DEFAULT_MODEL_DEEPGRAM "nova-2"

#define TRANSCRIBE_DEFAULT_PROVIDER "local"  /* auto-detect */

/* Supported audio extensions (sorted for bsearch) */
extern const char *transcribe_supported_formats[];
extern const int   transcribe_supported_format_count;

/* === Public API === */

/*
 * Transcribe an audio file.
 * file_path: absolute path to audio file.
 * model: provider model name, or NULL for default.
 * Returns malloc'd JSON string. Caller free()s.
 */
char *transcribe_audio(const char *file_path, const char *model);

/*
 * Check if a file extension is supported for transcription.
 * Returns true if ext (e.g. ".mp3", ".wav") is in the supported list.
 */
bool transcribe_is_supported_format(const char *ext);

/*
 * Validate an audio file without transcribing.
 * Returns NULL if valid, or a malloc'd error JSON if invalid.
 */
char *transcribe_validate_file(const char *file_path);

#ifdef __cplusplus
}
#endif

#endif /* LIBTRANSCRIBE_H */
