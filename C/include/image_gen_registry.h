/*
 * image_gen_registry.h — Image Generation Provider Registry for Hermes C.
 * Port of Python agent/image_gen_registry.py (145 lines).
 *
 * Central map of registered providers. Populated at init-time via
 * image_gen_register_provider(); consumed by the image_generate tool.
 *
 * Active selection mirrors Python logic:
 * 1. image_gen.provider env/config → return that provider (even if unavailable)
 * 2. Exactly one registered and available → return it
 * 3. Prefer legacy FAL when available
 * 4. Otherwise → NULL
 */
#ifndef IMAGE_GEN_REGISTRY_H
#define IMAGE_GEN_REGISTRY_H

#include <stdbool.h>
#include <stddef.h>

#define IMAGE_GEN_PROVIDER_NAME_MAX 64
#define IMAGE_GEN_MAX_PROVIDERS 16

/* Provider struct — backend for image generation */
typedef struct {
    char name[IMAGE_GEN_PROVIDER_NAME_MAX];
    char display_name[IMAGE_GEN_PROVIDER_NAME_MAX];
    bool (*is_available)(void);
} image_gen_provider_t;

/* Register a provider. name must be non-empty. Overwrites on re-registration. */
bool image_gen_register_provider(const image_gen_provider_t *provider);

/* Return number of registered providers */
int image_gen_provider_count(void);

/* Get provider by index (0..count-1). Returns NULL if out of range. */
const image_gen_provider_t *image_gen_get_provider_by_index(int idx);

/* Lookup by name. Returns NULL if not found. */
const image_gen_provider_t *image_gen_get_provider(const char *name);

/* Resolve active provider.
 * Reads IMAGE_GEN_PROVIDER env var first. Falls back:
 * 1. If exactly one provider registered and is_available(), use it.
 * 2. If provider named "fal" registered and is_available(), use it.
 * 3. Return NULL.
 */
const image_gen_provider_t *image_gen_get_active_provider(void);

/* Clear registry. Test-only. */
void image_gen_reset_registry(void);

#endif /* IMAGE_GEN_REGISTRY_H */
