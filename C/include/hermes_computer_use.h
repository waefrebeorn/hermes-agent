/*
 * hermes_computer_use.h — Computer use backend abstraction for Hermes C.
 *
 * Mirrors Python's tools/computer_use/backend.py ABC with a C struct-of-pointers
 * vtable pattern. Each backend (cua-driver via MCP, Linux X11, noop) implements
 * the function table.
 *
 * MIT License — WuBu Hermes Project
 */

#ifndef HERMES_COMPUTER_USE_H
#define HERMES_COMPUTER_USE_H

#include "hermes_json.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Data types mirroring Python's dataclasses ────────────────────────── */

/* Bounding rectangle (logical pixels). */
typedef struct {
    int x, y, w, h;
} cu_rect_t;

/* One interactable UI element on screen. */
typedef struct {
    int         index;       /* 1-based SOM index */
    char        role[64];    /* AX role: AXButton, AXTextField, ... */
    char        label[256];  /* AX title / description */
    cu_rect_t   bounds;      /* x, y, w, h in logical pixels */
    char        app[128];    /* owning app name/bundle */
    int         pid;         /* owning process PID */
    int         window_id;   /* window ID */
} cu_element_t;

/* Screen capture result. */
typedef struct {
    char        mode[16];    /* "som", "vision", "ax" */
    int         width;
    int         height;
    char       *png_b64;     /* malloc'd base64 PNG, or NULL */
    size_t      png_bytes;   /* raw PNG byte count */
    cu_element_t *elements;  /* malloc'd array of elements */
    int         element_count;
    char        app[128];
    char        window_title[256];
} cu_capture_t;

/* Action result. */
typedef struct {
    bool        ok;
    char        action[32];
    char        message[512];
    /* Optional trailing capture (for capture_after). */
    cu_capture_t *capture;   /* malloc'd, or NULL */
} cu_action_t;

/* ── Backend vtable ──────────────────────────────────────────────────── */

/* All functions return a heap-allocated result or NULL on internal failure.
 * String parameters are null-terminated C strings. */
typedef struct cu_backend_t {
    /* Lifecycle */
    bool  (*is_available)(void);
    bool  (*start)(void);
    void  (*stop)(void);

    /* Screen capture. mode: "som"|"vision"|"ax". app: NULL means frontmost. */
    cu_capture_t *(*capture)(const char *mode, const char *app);

    /* Pointer actions */
    cu_action_t *(*click)(int element, int x, int y,
                          const char *button, int click_count,
                          const char *modifiers);
    cu_action_t *(*drag)(int from_element, int to_element,
                         int from_x, int from_y, int to_x, int to_y,
                         const char *button, const char *modifiers);
    cu_action_t *(*scroll)(const char *direction, int amount,
                           int element, int x, int y,
                           const char *modifiers);

    /* Keyboard */
    cu_action_t *(*type_text)(const char *text);
    cu_action_t *(*key)(const char *keys);

    /* Introspection */
    cu_action_t *(*list_apps)(void);
    cu_action_t *(*focus_app)(const char *app, bool raise_window);

    /* Native-value mutation */
    cu_action_t *(*set_value)(const char *value, int element);

    /* Timing — default implementation sleeps. Override for custom wait. */
    cu_action_t *(*wait)(double seconds);

    /* Opaque backend state (cast to per-backend struct). */
    void *state;
} cu_backend_t;

/* ── Public API ──────────────────────────────────────────────────────── */

/* Factory: select best available backend for the current platform.
 * Falls back to noop if nothing available. Must be called once at init. */
cu_backend_t *computer_use_select_backend(void);

/* Register the noop backend explicitly (for testing). */
cu_backend_t *computer_use_new_noop_backend(void);

/* Free a capture result (frees all nested allocations). */
void cu_capture_free(cu_capture_t *cap);

/* Free an action result (frees capture if present). */
void cu_action_free(cu_action_t *act);

/* ── Lifecycle helpers ───────────────────────────────────────────────── */

/* Global singleton — created by first call to computer_use_select_backend(). */
cu_backend_t *cu_get_global_backend(void);

/* Reset singleton (for testing). */
void cu_reset_global_backend(void);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_COMPUTER_USE_H */
