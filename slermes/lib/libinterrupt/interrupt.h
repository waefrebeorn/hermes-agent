/*
 * interrupt.h — Per-thread interrupt signaling for tools.
 *
 * Port of tools/interrupt.py. Used by all tools to check if the current
 * agent session has been interrupted. Thread-safe via pthreads mutex.
 */

#ifndef HERMES_INTERRUPT_H
#define HERMES_INTERRUPT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum concurrent threads tracked. */
#define INTERRUPT_MAX_THREADS 64

/** interrupt_set(active, thread_id) — Set or clear interrupt for a thread.
 *  When thread_id is 0, targets the current thread (pthread_self()). */
void interrupt_set(bool active, unsigned long thread_id);

/** interrupt_is_interrupted() — Check if current thread has been interrupted.
 *  Each thread only sees its own interrupt state. */
bool interrupt_is_interrupted(void);

/** interrupt_clear_all() — Clear interrupts for all threads.
 *  Used during cleanup. */
void interrupt_clear_all(void);

/** interrupt_count() — Number of currently interrupted threads.
 *  For diagnostics/test assertions. */
int interrupt_count(void);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_INTERRUPT_H */
