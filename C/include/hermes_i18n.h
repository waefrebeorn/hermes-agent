#ifndef HERMES_I18N_H
#define HERMES_I18N_H

/*
 * hermes_i18n.h — Lightweight internationalization.
 * Port of Python agent/i18n.py.
 *
 * Loads YAML locale files from HERMES_HOME/locales/ or fallback path,
 * resolves language from env/config/default, and looks up dotted keys
 * with {placeholder} format substitution.
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Constants ────────────────────────────────────────────────── */

#define I18N_KEY_MAX    256    /* max dotted key length */
#define I18N_LANG_MAX   16     /* max language code length */
#define I18N_VALUE_MAX  4096   /* max translated value length after format */
#define I18N_TEMP_MAX   8192   /* format buffer size */
#define I18N_MAX_ARGS   16     /* max format arguments */

/* ── Types ────────────────────────────────────────────────────── */

/* A single flat key→value entry */
typedef struct {
    char key[I18N_KEY_MAX];
    char value[I18N_VALUE_MAX];
} i18n_entry_t;

/* A loaded locale catalog */
typedef struct {
    char          lang[I18N_LANG_MAX];
    i18n_entry_t *entries;
    size_t        count;
    size_t        capacity;
} i18n_catalog_t;

/* Format argument key→value pair */
typedef struct {
    const char *key;
    const char *value;
} i18n_arg_t;

/* ── Public API ──────────────────────────────────────────────── */

/* Initialize the i18n system. Loads catalogs for all found locale files.
 * Path: first HERMES_HOME/locales/, then fallback to repo-relative locales/
 * Returns number of catalogs loaded, or 0 on failure. */
int i18n_init(const char *locales_path);

/* Set a locale directory explicitly (useful for testing) */
void i18n_set_locales_dir(const char *path);

/* Get the current language string. Resolution: HERMES_LANGUAGE env →
 * config.language → "en". Caller does NOT own the returned pointer. */
const char *i18n_get_language(void);

/* Translate a dotted key to the current language.
 * Returns allocated string (caller must free), or NULL on error.
 * If key not found in current or fallback language, returns strdup(key). */
char *i18n_t(const char *key);

/* Translate with explicit language override */
char *i18n_t_lang(const char *key, const char *lang);

/* Translate with format substitution: t("hello {name}", "name", "World", NULL)
 * Supports up to I18N_MAX_ARGS pairs. Pass NULL terminator. */
char *i18n_t_fmt(const char *key, ...);

/* Translate with format via va_list */
char *i18n_t_vfmt(const char *key, const i18n_arg_t *args, size_t nargs);

/* Translate with explicit language + format args */
char *i18n_t_lang_vfmt(const char *key, const char *lang,
                        const i18n_arg_t *args, size_t nargs);

/* Flush all cached catalogs (call when locales dir changes) */
void i18n_reset(void);

/* Free all i18n resources */
void i18n_cleanup(void);

/* Load a single YAML locale file into a catalog.
 * Public primarily for testing. */
int i18n_load_catalog(const char *lang, const char *yaml_path,
                       i18n_catalog_t *catalog);

/* Free a single catalog's entries */
void i18n_free_catalog(i18n_catalog_t *catalog);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_I18N_H */
