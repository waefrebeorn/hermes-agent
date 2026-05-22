#ifndef LIBDB_H
#define LIBDB_H

/*
 * libdb.h — File-based JSON session store for C.
 * Zero external deps. Each session is a .json file on disk
 * with an optional .meta.json sidecar for metadata.
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
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque database handle */
typedef struct db_t db_t;

/* ================================================================
 *  P141: Session metadata structure
 * ================================================================ */
#define SESSION_SCHEMA_VERSION 1
#define SESSION_TAGS_MAX 32
#define SESSION_TAG_LEN 64

typedef struct {
    char     title[256];          /* session title */
    char     model[128];          /* model used */
    int      schema_version;      /* P150: schema version for migration */
    int      token_count;         /* total tokens used */
    int      message_count;       /* total messages */
    time_t   created_at;          /* creation timestamp */
    time_t   updated_at;          /* last update timestamp */
    /* P146: Tags */
    char     tags[SESSION_TAGS_MAX][SESSION_TAG_LEN];
    int      tag_count;
    /* P149: Branch parent info */
    char     parent_id[64];       /* empty if root session */
    int      branch_point;        /* message index where branch happened, -1 if root */
} session_meta_t;

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

/* === P141: Metadata operations === */

/* Save metadata alongside session data. Sidecar .meta.json file. */
bool db_save_meta(db_t *db, const char *session_id, const session_meta_t *meta);

/* Load metadata for a session. Returns true if meta file exists. */
bool db_load_meta(const db_t *db, const char *session_id, session_meta_t *meta);

/* Initialize metadata with defaults (title, model, timestamps). */
void db_meta_init(session_meta_t *meta);

/* === Listing === */

/* List all session IDs. Returns NULL-terminated array. Caller free each + array. */
char **db_list(const db_t *db, size_t *count);

/* Get total number of sessions. */
size_t db_count(const db_t *db);

/* === P143: List with metadata === */
typedef struct {
    char id[64];
    session_meta_t meta;
} db_session_entry_t;

/* Session list entry (used by agent_session_list) */
typedef struct {
    char           id[64];
    session_meta_t meta;
} session_entry_t;

/* List sessions with metadata. Returns malloc'd array. Caller must free each + array. */
db_session_entry_t *db_list_with_meta(const db_t *db, size_t *count);

/* === P145: Prune === */

/* Remove sessions older than retention_days. Returns number removed. */
int db_prune_by_age(db_t *db, int retention_days);

/* === P148: Export === */

/* Export session as JSON string (full data + metadata merged). Caller must free. */
char *db_export_json(db_t *db, const char *session_id);

/* Export session as Markdown string. Caller must free. */
char *db_export_markdown(db_t *db, const char *session_id);

/* === P149: Branch === */

/* Branch a session: copy messages [0..branch_point] into new session_id.
 * Returns NULL on error or malloc'd string with new session data. */
char *db_branch(db_t *db, const char *source_id, const char *new_id, int branch_point);

/* === P150: Migration === */

/* Check and upgrade schema version for all sessions. Returns number migrated. */
int db_migrate(db_t *db);

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
