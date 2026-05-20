#ifndef LIBDB_H
#define LIBDB_H

/*
 * libdb.h — File-based JSON session store for C.
 * Zero external deps. Each session is a .json file on disk.
 * Replaces Python's SQLite-based session store + hermes_state.
 *
 * MIT License — WuBu Hermes Project
 *
 * Usage:
 *   db_t *db = db_open("/home/user/.hermes/sessions", NULL);
 *   db_save(db, "session_123", "{\"messages\":[...]}");
 *   char *data = db_load(db, "session_123", NULL);
 *   db_delete(db, "session_123");
 *   db_close(db);
 */

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque database handle */
typedef struct db_t db_t;

/* === Core operations === */

/* Open/create database in directory. Returns NULL on error. */
db_t *db_open(const char *dir_path, char **error_msg);

/* Close database and flush all pending writes. */
void db_close(db_t *db);

/* Save a session by ID. Overwrites existing. */
bool db_save(db_t *db, const char *session_id, const char *json_data);

/* Load a session by ID. Returns malloc'd string, caller must free. */
char *db_load(const db_t *db, const char *session_id, char **error_msg);

/* Delete a session. Returns true if existed. */
bool db_delete(db_t *db, const char *session_id);

/* Check if session exists. */
bool db_exists(const db_t *db, const char *session_id);

/* === Listing === */

/* List all session IDs. Returns NULL-terminated array. Caller free each + array. */
char **db_list(const db_t *db, size_t *count);

/* Get total number of sessions. */
size_t db_count(const db_t *db);

/* === Maintenance === */

/* Get storage size in bytes. */
long long db_storage_size(const db_t *db);

/* Remove all sessions. */
bool db_clear(db_t *db);

/* Flush pending writes to disk. */
bool db_flush(db_t *db);

#ifdef __cplusplus
}
#endif

#endif /* LIBDB_H */
