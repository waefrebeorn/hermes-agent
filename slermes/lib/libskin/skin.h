#ifndef LIBSKIN_H
#define LIBSKIN_H

/*
 * libskin.h — Skin engine for data-driven CLI theming.
 * Replaces Python's skin engine (slermes_cli/skin_engine.py).
 *
 * MIT License — WuBu Hermes Project
 *
 * A skin defines colors, symbols, and formatting for every display element.
 * Skins are loaded from JSON files.
 *
 * Usage:
 *   skin_t *s = skin_load("/path/to/skin.json");
 *   if (!s) s = skin_default(); // fallback
 *   const char *prompt = skin_get(s, "prompt");
 *   const char *color  = skin_get(s, "colors.banner");
 *   skin_free(s);
 *
 * Skin JSON format:
 *   {
 *     "name": "default",
 *     "colors": {
 *       "banner": "cyan",
 *       "prompt": "green",
 *       "response": "white",
 *       "error": "red",
 *       "dim": "white:dim",
 *       "tool_output": "yellow"
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

/* Get ANSI color for a skin color key, resolved to the display layer. */
void skin_apply_color(const skin_t *s, const char *key, const char **fg, int *bold);

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
