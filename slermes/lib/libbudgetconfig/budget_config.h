#ifndef HERMES_BUDGET_CONFIG_H
#define HERMES_BUDGET_CONFIG_H

/*
 * budget_config.h — Configurable budget constants for tool result persistence.
 * Port of Python tools/budget_config.py.
 *
 * Per-tool resolution: pinned > tool_overrides > default.
 */

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Defaults matching Python's budget_config.py ── */
#define BUDGET_DEFAULT_RESULT_SIZE_CHARS  100000
#define BUDGET_DEFAULT_TURN_BUDGET_CHARS  200000
#define BUDGET_DEFAULT_PREVIEW_SIZE_CHARS 1500

/* Pinned tool — read_file has infinite threshold to prevent persist->read->persist loops */
#define BUDGET_PINNED_READ_FILE_THRESHOLD -1  /* sentinel: infinite */

/* ── Per-tool override (simple linked list) ── */
typedef struct budget_override {
    char tool_name[64];
    int  threshold;               /* chars, 0 = use default, -1 = infinite */
    struct budget_override *next;
} budget_override_t;

/* ── Budget config (mutable, for runtime configuration) ── */
typedef struct {
    int  default_result_size;     /* Layer 2 per-result char threshold */
    int  turn_budget;             /* Layer 3 per-turn aggregate budget */
    int  preview_size;            /* inline snippet size after persistence */
    budget_override_t *overrides; /* per-tool overrides (linked list) */
} budget_config_t;

/* ── Public API ── */

/* Initialize with defaults */
void budget_config_init(budget_config_t *cfg);

/* Set a per-tool override threshold. Pass threshold=-1 for infinite. */
void budget_config_set_override(budget_config_t *cfg,
                                 const char *tool_name, int threshold);

/* Resolve persistence threshold for a tool.
 * Priority: pinned (read_file=inf) > tool_overrides > default.
 * Returns threshold in chars, or -1 for infinite. */
int budget_config_resolve_threshold(const budget_config_t *cfg,
                                     const char *tool_name);

/* Free overrides memory */
void budget_config_cleanup(budget_config_t *cfg);

/* Get a pointer to the internal default config singleton */
const budget_config_t *budget_config_default(void);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_BUDGET_CONFIG_H */
