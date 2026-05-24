/*
 * video_gen_registry.h — Video Generation Provider Registry for Hermes C.
 * Port of Python agent/video_gen_registry.py (117 lines).
 *
 * Central map of registered providers. Populated at init-time via
 * video_gen_register_provider(); consumed by the video_generate tool.
 *
 * Active selection mirrors Python logic:
 * 1. video_gen.provider env/config → return that provider (even if unavailable)
 * 2. Exactly one registered and available → return it
 * 3. Otherwise → NULL
 */
#ifndef VIDEO_GEN_REGISTRY_H
#define VIDEO_GEN_REGISTRY_H

#include <stdbool.h>
#include <stddef.h>

/* Provider max name length */
#define VIDEO_GEN_PROVIDER_NAME_MAX 64

/* Provider struct — backend for video generation */
typedef struct {
    char name[VIDEO_GEN_PROVIDER_NAME_MAX];
    char display_name[VIDEO_GEN_PROVIDER_NAME_MAX];
    bool (*is_available)(void);
    char *(*generate)(const char *prompt, const char *aspect_ratio,
                      const char *image_url, int duration, int seed,
                      bool has_audio, const char *negative_prompt,
                      const char *operation, const char *resolution);
} video_gen_provider_t;

/* Maximum registered providers */
#define VIDEO_GEN_MAX_PROVIDERS 16

/* Register a provider. name must be non-empty. Overwrites on re-registration.
 * Returns true on success, false if registry is full. */
bool video_gen_register_provider(const video_gen_provider_t *provider);

/* Return number of registered providers */
int video_gen_provider_count(void);

/* Get provider by index (0..count-1). Returns NULL if out of range. */
const video_gen_provider_t *video_gen_get_provider_by_index(int idx);

/* Lookup by name. Returns NULL if not found. */
const video_gen_provider_t *video_gen_get_provider(const char *name);

/* Resolve active provider.
 *
 * Reads VIDEO_GEN_PROVIDER env var first. Falls back:
 * 1. If exactly one provider registered and is_available(), use it.
 * 2. If a provider named "fal" is registered and is_available(), use it.
 * 3. Return NULL.
 */
const video_gen_provider_t *video_gen_get_active_provider(void);

/* Clear registry. Test-only. */
void video_gen_reset_registry(void);

#endif /* VIDEO_GEN_REGISTRY_H */
