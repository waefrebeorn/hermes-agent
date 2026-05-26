#ifndef LIBSKIN_H
#define LIBSKIN_H

/*
 * libskin.h — Skin engine for data-driven CLI theming.
 * Replaces Python's skin engine (hermes_cli/skin_engine.py).
 *
 * MIT License — WuBu Hermes Project
 *
 * A skin defines colors, symbols, branding, and formatting for every
 * display element.  Skins are loaded from JSON files.
 *
 * Usage:
 *   skin_t *s = skin_load("/path/to/skin.json");
 *   if (!s) s = skin_default(); // fallback
 *   const char *prompt = skin_get(s, "prompt");
 *   const char *color  = skin_get(s, "colors.banner");
 *   const char *brand  = skin_get_branding(s, "agent_name", "Hermes Agent");
 *   skin_free(s);
 *
 * Colors support:
 *   - Named ANSI:    black, red, green, yellow, blue, magenta, cyan, white
 *   - With modifier: cyan:bold, white:dim
 *   - Hex 24-bit:    #FFD700, #CD7F32
 *
 * Skin JSON format:
 *   {
 *     "name": "default",
 *     "colors": {
 *       "banner_border": "#CD7F32",
 *       "banner_title": "#FFD700",
 *       "banner_accent": "#FFBF00",
 *       "banner_dim": "#B8860B",
 *       "banner_text": "#FFF8DC",
 *       "ui_accent": "#FFBF00",
 *       "ui_label": "#DAA520",
 *       "ui_ok": "#4caf50",
 *       "ui_error": "#ef5350",
 *       "ui_warn": "#ffa726",
 *       "prompt": "#FFF8DC",
 *       "input_rule": "#CD7F32",
 *       "response_border": "#FFD700",
 *       "status_bar_bg": "#1a1a2e",
 *       "status_bar_text": "#C0C0C0",
 *       "status_bar_strong": "#FFD700",
 *       "status_bar_dim": "#8B8682",
 *       "session_label": "#DAA520",
 *       "session_border": "#8B8682",
 *       "tool": "yellow"
 *     },
 *     "branding": {
 *       "agent_name": "Hermes Agent",
 *       "welcome": "Welcome to Hermes Agent! ...",
 *       "goodbye": "Goodbye! ⚕",
 *       "response_label": " ⚕ Hermes ",
 *       "prompt_symbol": "❯",
 *       "help_header": "(^_^)? Available Commands"
 *     },
 *     "symbols": {
 *       "prompt": "λ",
 *       "tool": "⚙",
 *       "thinking": "⋯"
 *     },
 *     "format": {
 *       "banner_bold": true,
 *       "show_model": true
 *     }
 *   }
 */

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Skin types
 * ================================================================ */

typedef struct skin_t skin_t;

/* Create default skin (fallback). Never returns NULL. */
skin_t *skin_default(void);

/* Load skin from JSON file. Returns NULL on error (call skin_error). */
skin_t *skin_load(const char *path);

/* Load skin from JSON string. */
skin_t *skin_load_string(const char *json);

/* Create skin with custom overrides (applied on top of default). */
skin_t *skin_with_overrides(const char *overrides_json);

/* ================================================================
 *  Skin queries
 * ================================================================ */

/* Get a skin value by dotted path. Returns default if not found.
 * Examples: "colors.banner", "symbols.prompt", "format.banner_bold" */
const char *skin_get(const skin_t *s, const char *key, const char *def);

/* Get a boolean skin value. */
bool skin_get_bool(const skin_t *s, const char *key, bool def);

/* Get skin name. */
const char *skin_name(const skin_t *s);

/* ================================================================
 *  ANSI color resolution
 * ================================================================ */

/* Resolve a color name to ANSI escape code prefix.
 * Supports: black, red, green, yellow, blue, magenta, cyan, white,
 * and modifiers: bold, dim (e.g. "cyan:bold", "white:dim") */
const char *skin_ansi_color(const char *name);

/* Get the ANSI color for a skin color key. Convenience wrapper. */
const char *skin_color(const skin_t *s, const char *key);

/* Get the ANSI color for a skin color key, resolved to the display layer. */
void skin_apply_color(const skin_t *s, const char *key, const char **fg, int *bold);

/* ================================================================
 *  Raw JSON access
 * ================================================================ */

/* Get a raw JSON node by dotted path.
 * Returns NULL if the path doesn't resolve or the skin is NULL.
 * The returned pointer is owned by the skin — do NOT free it.
 * Useful for iterating arrays or checking nested object structure. */
const void *skin_get_json(const skin_t *s, const char *key);

/* ================================================================
 *  Branding
 * ================================================================ */

/* Get a branding string value by key (e.g. "agent_name", "prompt_symbol"). */
const char *skin_get_branding(const skin_t *s, const char *key, const char *def);

/* ================================================================
 *  Built-in skin enumeration
 * ================================================================ */

/* Return the number of built-in skins available. */
int skin_builtin_count(void);

/* Get the name of the Nth built-in skin (0-indexed). Returns NULL if out of bounds. */
const char *skin_builtin_name(int index);

/* ================================================================
 *  Error handling
 * ================================================================ */

const char *skin_error(void);

/* Free skin. Safe to call with NULL. */
void skin_free(skin_t *s);

#ifdef __cplusplus
}
#endif

#endif /* LIBSKIN_H */
