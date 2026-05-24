#ifndef SKILL_BUNDLES_H
#define SKILL_BUNDLES_H

/*
 * skill_bundles.h -- Skill bundle loader for C Hermes.

 * Scans ~/.slermes/skill-bundles/YAML files for skill bundles -- named groups
 * of skills that load together under a single slash command.
 *
 * YAML format:
 *   name: backend-dev
 *   description: Backend feature work
 *   skills:
 *     - github-code-review
 *     - test-driven-development
 *   instruction: Optional extra guidance
 *
 * MIT License — WuBu Hermes Project
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BUNDLE_NAME_MAX     128
#define BUNDLE_SLUG_MAX     128
#define BUNDLE_DESC_MAX     256
#define BUNDLE_INSTR_MAX    4096
#define BUNDLE_SKILLS_MAX   64
#define BUNDLE_SKILL_NAME   128
#define BUNDLE_REG_MAX      64

typedef struct {
    char name[BUNDLE_NAME_MAX];
    char slug[BUNDLE_SLUG_MAX];
    char description[BUNDLE_DESC_MAX];
    char instruction[BUNDLE_INSTR_MAX];
    char skills[BUNDLE_SKILLS_MAX][BUNDLE_SKILL_NAME];
    int  skill_count;
} skill_bundle_t;

typedef struct {
    skill_bundle_t bundles[BUNDLE_REG_MAX];
    int            count;
} skill_bundle_registry_t;

/* Scan ~/.slermes/skill-bundles/ (or HERMES_BUNDLES_DIR) and populate registry.
 * Returns number of bundles loaded, or -1 on error. */
int skill_bundles_scan(skill_bundle_registry_t *reg);

/* Find a bundle by slug (e.g. "backend-dev"). Returns NULL if not found. */
const skill_bundle_t *skill_bundle_find(const skill_bundle_registry_t *reg, const char *slug);

/* Print all bundles to stdout for display. */
void skill_bundles_print(const skill_bundle_registry_t *reg);

#ifdef __cplusplus
}
#endif

#endif /* SKILL_BUNDLES_H */
