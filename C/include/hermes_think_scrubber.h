#ifndef HERMES_THINK_SCRUBBER_H
#define HERMES_THINK_SCRUBBER_H

/*
 * hermes_think_scrubber.h — Streaming think/reasoning block scrubber.
 *
 * Stateful scrubber that strips <think>, <thinking>, <reasoning>,
 * <thought>, and <REASONING_SCRATCHPAD> blocks from streamed assistant
 * text. Handles partial tags split across streaming deltas by holding
 * back potential tag prefixes until the next delta resolves them.
 *
 * Usage:
 *   think_scrubber_t *sc = think_scrubber_new();
 *   for each delta in stream:
 *       const char *visible = think_scrubber_feed(sc, delta);
 *       if (visible) emit(visible);
 *   const char *tail = think_scrubber_flush(sc);
 *   if (tail) emit(tail);
 *   think_scrubber_free(sc);
 *
 * Call think_scrubber_reset() at the start of each new turn to prevent
 * a hung block from a prior interrupted stream tainting the next output.
 *
 * Ported from Python agent/think_scrubber.py.
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque scrubber handle */
typedef struct think_scrubber think_scrubber_t;

/* Create a new streaming think scrubber */
think_scrubber_t *think_scrubber_new(void);

/* Free scrubber and all held state */
void think_scrubber_free(think_scrubber_t *sc);

/* Reset all state. Call at the top of each new turn. */
void think_scrubber_reset(think_scrubber_t *sc);

/* Feed one streaming delta. Returns pointer to internal buffer holding
 * the visible (scrubbed) portion, or NULL if the entire delta was
 * reasoning content. The pointer is valid until the next feed() call. */
const char *think_scrubber_feed(think_scrubber_t *sc, const char *text);

/* Flush held-back content at end of stream. Returns pointer to any
 * remaining visible text, or NULL. */
const char *think_scrubber_flush(think_scrubber_t *sc);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_THINK_SCRUBBER_H */
