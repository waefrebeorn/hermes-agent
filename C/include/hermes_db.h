#ifndef HERMES_DB_H
#define HERMES_DB_H

/*
 * hermes_db.h — File-based session store for Hermes C.
 * No SQLite dependency. Stores sessions as individual JSON files
 * under HERMES_HOME/sessions/. Supports FTS5-like search via
 * grep + line index.
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handle */
typedef struct db_t db_t;

/* Open (or create) session database at dir path. */
db_t *db_open(const char *dir_path);

/* Close database. */
void db_close(db_t *db);

/* Save a message to current session. Returns session_id string. */
bool db_save_message(db_t *db, const char *session_id,
                     const char *role, const char *content);

/* Load all messages for a session. Sets *count. Caller must free result. */
char **db_load_session(db_t *db, const char *session_id, size_t *count,
                       const char **roles_out);

/* List recent sessions. Returns array of session_id strings. */
char **db_list_sessions(db_t *db, size_t *count, int limit);

/* Search session content for query. Returns matching session_ids. */
char **db_search(db_t *db, const char *query, size_t *count);

/* Generate a new unique session ID. */
char *db_gen_session_id(db_t *db);

/* Delete a session. */
bool db_delete_session(db_t *db, const char *session_id);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_DB_H */
