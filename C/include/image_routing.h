#ifndef HERMES_IMAGE_ROUTING_H
#define HERMES_IMAGE_ROUTING_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Image routing helpers for inbound user-attached images.
 *
 * Two modes:
 *   "native"  -- attach images as data URL content parts for provider
 *   "text"    -- run vision_analyze up-front, model sees text summary only
 *
 * The decision is made once per message turn by decide_image_input_mode().
 * It reads agent.image_input_mode from config (auto|native|text, default auto)
 * and the active model's capability metadata.
 *
 * In auto mode:
 *   - If auxiliary.vision.provider is explicitly configured → text pipeline
 *   - Else if model reports supports_vision=true → native
 *   - Else → text
 */

/* ── Public API ────────────────────────────────────────────────── */

/**
 * decide_image_input_mode: Return "native" or "text" for the given turn.
 *
 * provider: active inference provider ID (e.g. "anthropic", "openrouter").
 * model:    active model slug.
 * cfg:      loaded config (hermes_config_t), or NULL for auto-behaviour.
 *
 * Returns a static string "native" or "text" — do NOT free.
 */
const char *decide_image_input_mode(const char *provider,
                                    const char *model,
                                    const void *cfg);

/**
 * image_routing_decide_mode: Like decide_image_input_mode but also checks
 * vision_disabled flag from the runtime agent state.
 * Returns "text" if vision is disabled, otherwise delegates.
 */
const char *image_routing_decide_mode(const void *state,
                                       const char *provider,
                                       const char *model,
                                       const void *cfg);

/**
 * build_native_content_parts: Build OpenAI-style content list JSON string.
 *
 * user_text:    the user's text input.
 * image_paths:  array of file paths to attach as image_url parts.
 * num_paths:    count of paths in array.
 * skipped_out:  on return, points to malloc'd array of skipped paths.
 * skipped_cnt:  on return, count of skipped paths.
 *
 * Returns malloc'd JSON string (caller free()), or NULL on error.
 * On success, content has the shape:
 *   [{"type":"text","text":"..."},
 *    {"type":"image_url","image_url":{"url":"data:image/png;base64,..."}}]
 */
char *build_native_content_parts(const char *user_text,
                                  const char **image_paths,
                                  size_t num_paths,
                                  char ***skipped_out,
                                  size_t *skipped_cnt);

/**
 * file_to_data_url: Encode local image as base64 data URL string.
 * Returns malloc'd string (caller free()), or NULL if file can't be read.
 */
char *file_to_data_url(const char *path);

/**
 * sniff_mime_from_bytes: Detect image MIME type from magic bytes.
 * data: raw file bytes.
 * len:  byte count.
 * Returns static string (e.g. "image/png") or NULL if unrecognised.
 */
const char *sniff_mime_from_bytes(const unsigned char *data, size_t len);

/**
 * guess_mime: Return MIME type for a file path.
 * Uses magic bytes when data is provided, falls back to suffix.
 * Returns static string — do NOT free.
 */
const char *guess_mime(const char *path,
                       const unsigned char *data,
                       size_t data_len);

/**
 * image_routing_disable_vision: Mark vision as disabled for this session.
 * Called when a provider returns an error indicating images are not supported.
 */
void image_routing_disable_vision(void *state);

/**
 * image_routing_vision_disabled: Check if vision is disabled.
 * Returns true if a provider error previously triggered disable_vision.
 */
bool image_routing_vision_disabled(const void *state);

/**
 * image_routing_notify_error: Feed an error string to the image router.
 * If the error message suggests the model doesn't support images,
 * auto-disables vision for the rest of the session.
 * Returns true if vision was newly disabled by this call.
 */
bool image_routing_notify_error(void *state, const char *error_text);

#endif /* HERMES_IMAGE_ROUTING_H */
