/*
 * helpers.h — Shared gateway helper utilities.
 * Port of Python gateway/platforms/helpers.py.
 *
 * Provides: message deduplication, markdown stripping, thread
 * participation tracking, phone number redaction.
 */

#ifndef GATEWAY_HELPERS_H
#define GATEWAY_HELPERS_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Message Deduplicator — TTL-based message ID dedup cache
 * ================================================================ */

typedef struct {
    char   **msg_ids;       /* array of message ID strings */
    double *timestamps;     /* array of timestamps */
    int     count;          /* current count */
    int     max_size;       /* max entries (default 2000) */
    double  ttl_seconds;    /* TTL in seconds (default 300) */
} msg_dedup_t;

/* Initialize a deduplicator with defaults (max_size=2000, ttl=300). */
void msg_dedup_init(msg_dedup_t *d);

/* Initialize with custom parameters. */
void msg_dedup_init_custom(msg_dedup_t *d, int max_size, double ttl_seconds);

/* Check if msg_id is a duplicate within the TTL window.
 * Returns true if already seen (and updates timestamp).
 * Returns false if not seen (registers it). */
bool msg_dedup_is_duplicate(msg_dedup_t *d, const char *msg_id);

/* Clear all tracked messages. */
void msg_dedup_clear(msg_dedup_t *d);

/* Free internal resources. Does NOT free the struct itself. */
void msg_dedup_destroy(msg_dedup_t *d);

/* ================================================================
 *  Markdown Stripping — strip formatting for plain-text platforms
 * ================================================================ */

/* Strip markdown formatting from text.
 * Returns a malloc'd string (caller must free).
 * Handles: **bold**, *italic*, __bold__, _italic_, ```code blocks```,
 * `inline code`, # headings, [links](url), and collapses 3+ newlines. */
char *strip_markdown(const char *text);

/* ================================================================
 *  Phone Number Redaction — for logging
 * ================================================================ */

/* Redact a phone number, preserving country code and last 4 digits.
 * Returns a malloc'd string. Input "<none>" returns "<none>". */
char *redact_phone(const char *phone);

/* ================================================================
 *  Thread Participation Tracker — persistent JSON file tracking
 * ================================================================ */

typedef struct {
    char   **thread_ids;    /* array of thread ID strings */
    int     count;
    int     max_tracked;    /* default 500 */
    char    platform[64];   /* platform name, for state file path */
    char    state_dir[512]; /* hermes_home directory */
} thread_tracker_t;

/* Initialize a thread participation tracker.
 * platform: platform name (e.g. "discord", "matrix").
 * state_dir: hermes_home path for the JSON state file. */
void thread_tracker_init(thread_tracker_t *t, const char *platform,
                         const char *state_dir);

/* Load persisted threads from the JSON state file. */
void thread_tracker_load(thread_tracker_t *t);

/* Mark thread_id as participated and persist. */
void thread_tracker_mark(thread_tracker_t *t, const char *thread_id);

/* Check if thread_id has been participated in. */
bool thread_tracker_has(thread_tracker_t *t, const char *thread_id);

/* Free internal resources. Does NOT free the struct itself. */
void thread_tracker_destroy(thread_tracker_t *t);

#ifdef __cplusplus
}
#endif

#endif /* GATEWAY_HELPERS_H */
