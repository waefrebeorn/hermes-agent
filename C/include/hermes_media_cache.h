/*
 * media_cache.h — Local media file cache for Hermes C.
 * Saves images/audio/video bytes to ~/.hermes/cache/<type>/.
 * Provides cleanup of expired files.
 */

#ifndef HERMES_MEDIA_CACHE_H
#define HERMES_MEDIA_CACHE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Save raw data to the media cache and return the absolute path.
 * type: "images", "audio", or "videos".
 * data: raw bytes to save.
 * data_len: length of data.
 * ext: file extension including dot (e.g. ".jpg", ".ogg").
 * is_image: if true, validates magic bytes before saving.
 * Returns malloc'd path string (caller must free) or NULL on error.
 * Mirrors Python base.py cache_image_from_bytes() / cache_audio_from_bytes(). */
char *media_cache_save(const char *type, const unsigned char *data, size_t data_len,
                        const char *ext, bool is_image);

/* Delete cached files in type directory older than max_age_hours.
 * type: "images", "audio", or "videos".
 * max_age_hours: age threshold in hours. Must be > 0.
 * Returns number of files removed.
 * Mirrors Python base.py cleanup_image_cache(). */
int media_cache_cleanup(const char *type, int max_age_hours);

/* Determine if a media file should be sent as audio (voice message).
 * For Telegram: Opus/OGG only when is_voice=true, MP3/M4A always audio.
 * Other platforms: all known audio extensions are sent as audio.
 * Returns true if the file should use the audio delivery path.
 * Mirrors Python base.py should_send_media_as_audio(). */
bool media_should_send_as_audio(const char *ext, bool is_telegram, bool is_voice);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_MEDIA_CACHE_H */
