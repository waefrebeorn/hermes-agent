/*
 * interrupt.c — Per-thread interrupt signaling implementation.
 *
 * Thread-safe set of interrupted thread IDs protected by a mutex.
 */

#include "interrupt.h"
#include <pthread.h>
#include <string.h>

/* ─── State ───────────────────────────────────────────────── */

static pthread_mutex_t interrupt_lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned long interrupted_threads[INTERRUPT_MAX_THREADS];
static int interrupted_count = 0;

/* ─── Internal helpers ────────────────────────────────────── */

static int find_thread(unsigned long tid) {
    for (int i = 0; i < interrupted_count; i++) {
        if (interrupted_threads[i] == tid)
            return i;
    }
    return -1;
}

/* ─── Public API ──────────────────────────────────────────── */

void interrupt_set(bool active, unsigned long thread_id) {
    if (thread_id == 0)
        thread_id = (unsigned long)pthread_self();

    pthread_mutex_lock(&interrupt_lock);

    if (active) {
        /* Add thread if not already tracked */
        if (find_thread(thread_id) == -1 && interrupted_count < INTERRUPT_MAX_THREADS) {
            interrupted_threads[interrupted_count++] = thread_id;
        }
    } else {
        /* Remove thread */
        int idx = find_thread(thread_id);
        if (idx >= 0) {
            interrupted_threads[idx] = interrupted_threads[--interrupted_count];
        }
    }

    pthread_mutex_unlock(&interrupt_lock);
}

bool interrupt_is_interrupted(void) {
    unsigned long tid = (unsigned long)pthread_self();
    pthread_mutex_lock(&interrupt_lock);
    bool found = (find_thread(tid) >= 0);
    pthread_mutex_unlock(&interrupt_lock);
    return found;
}

void interrupt_clear_all(void) {
    pthread_mutex_lock(&interrupt_lock);
    interrupted_count = 0;
    pthread_mutex_unlock(&interrupt_lock);
}

int interrupt_count(void) {
    pthread_mutex_lock(&interrupt_lock);
    int count = interrupted_count;
    pthread_mutex_unlock(&interrupt_lock);
    return count;
}
