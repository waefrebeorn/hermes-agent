#ifndef LIBDOTENV_H
#define LIBDOTENV_H

/*
 * libdotenv.h — .env file parser for C.
 * Zero external deps. Replaces Python's python-dotenv.
 *
 * MIT License — WuBu Hermes Project
 *
 * Usage:
 *   env_t *env = dotenv_load(".env", NULL);
 *   const char *val = dotenv_get(env, "API_KEY");
 *   dotenv_free(env);
 */

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque env store */
typedef struct env_t env_t;

/* Load .env file. Returns NULL on error (sets *error_msg). */
env_t *dotenv_load(const char *path, char **error_msg);

/* Load from string (e.g., embedded defaults). */
env_t *dotenv_parse(const char *input, char **error_msg);

/* Get value by key. Returns NULL if not found. */
const char *dotenv_get(const env_t *env, const char *key);

/* Set or override a key. */
bool dotenv_set(env_t *env, const char *key, const char *value);

/* Export all env vars to the process environment (setenv). */
bool dotenv_export(env_t *env);

/* Iterate all entries. Returns false when done. */
bool dotenv_iter(const env_t *env, size_t *idx, const char **key, const char **value);

/* Get count of entries. */
size_t dotenv_count(const env_t *env);

/* Free env store. */
void dotenv_free(env_t *env);

#ifdef __cplusplus
}
#endif

#endif /* LIBDOTENV_H */
