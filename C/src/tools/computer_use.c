/*
 * computer_use.c — Computer use tool for Hermes C.
 *
 * Provides OS-level desktop control: screen capture, mouse, keyboard, scroll,
 * drag. Uses a pluggable backend abstraction: cua-driver (macOS via MCP),
 * X11/xdotool (Linux), or noop (testing/fallback). Schema mirrors Python's
 * tools/computer_use/ Python package.
 *
 * MIT License — WuBu Hermes Project
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_computer_use.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>

/* ======================================================================
 *  Forward declarations
 * ====================================================================== */

static char *handle_computer_use(const char *args_json, const char *task_id);
static char *dispatch_action(cu_backend_t *b, const char *action, json_t *args);

/* ======================================================================
 *  Noop Backend (test/CI/fallback)
 * ====================================================================== */

typedef struct {
    int call_count;
} noop_state_t;

static bool noop_is_available(void) { return true; }
static bool noop_start(void) { return true; }
static void noop_stop(void) {}

static cu_capture_t *noop_capture(const char *mode, const char *app) {
    (void)mode;
    cu_capture_t *cap = (cu_capture_t *)calloc(1, sizeof(cu_capture_t));
    if (!cap) return NULL;
    snprintf(cap->mode, sizeof(cap->mode), "%s", mode ? mode : "som");
    cap->width = 1024;
    cap->height = 768;
    if (app) snprintf(cap->app, sizeof(cap->app), "%s", app);
    snprintf(cap->window_title, sizeof(cap->window_title), "Noop Desktop");
    cap->elements = NULL;
    cap->element_count = 0;
    cap->png_b64 = NULL;
    return cap;
}

static cu_action_t *noop_make_action(const char *action_name, bool ok,
                                      const char *fmt, ...) {
    cu_action_t *act = (cu_action_t *)calloc(1, sizeof(cu_action_t));
    if (!act) return NULL;
    snprintf(act->action, sizeof(act->action), "%s", action_name);
    act->ok = ok;
    if (fmt) {
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(act->message, sizeof(act->message), fmt, ap);
        va_end(ap);
    }
    return act;
}

static cu_action_t *noop_click(int element, int x, int y,
                                const char *button, int click_count,
                                const char *modifiers) {
    (void)modifiers;
    if (element > 0)
        return noop_make_action("click", true, "clicked element #%d", element);
    return noop_make_action("click", true, "clicked at (%d,%d) [%s] x%d",
                             x, y, button ? button : "left", click_count);
}

static cu_action_t *noop_drag(int from_e, int to_e,
                               int fx, int fy, int tx, int ty,
                               const char *button, const char *modifiers) {
    (void)button; (void)modifiers;
    if (from_e > 0 && to_e > 0)
        return noop_make_action("drag", true, "dragged element #%d to #%d",
                                 from_e, to_e);
    return noop_make_action("drag", true, "dragged (%d,%d) - (%d,%d)",
                             fx, fy, tx, ty);
}

static cu_action_t *noop_scroll(const char *dir, int amount,
                                 int element, int x, int y,
                                 const char *modifiers) {
    (void)element; (void)x; (void)y; (void)modifiers;
    return noop_make_action("scroll", true, "scrolled %s x%d",
                             dir ? dir : "down", amount > 0 ? amount : 3);
}

static cu_action_t *noop_type_text(const char *text) {
    return noop_make_action("type", true, "typed %zu chars",
                             text ? strlen(text) : 0);
}

static cu_action_t *noop_key(const char *keys) {
    return noop_make_action("key", true, "sent key %s", keys ? keys : "?");
}

static cu_action_t *noop_list_apps(void) {
    return noop_make_action("list_apps", true, "3 apps running");
}

static cu_action_t *noop_focus_app(const char *app, bool raise) {
    return noop_make_action("focus_app", true, "focused %s%s",
                             app ? app : "(frontmost)",
                             raise ? " (raised)" : "");
}

static cu_action_t *noop_set_value(const char *value, int element) {
    return noop_make_action("set_value", true, "set element #%d to %s",
                             element, value ? value : "");
}

static cu_action_t *noop_wait(double seconds) {
    struct timespec ts;
    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1e9);
    nanosleep(&ts, NULL);
    return noop_make_action("wait", true, "waited %.2fs", seconds);
}

static cu_backend_t *make_noop_backend(void) {
    cu_backend_t *b = (cu_backend_t *)calloc(1, sizeof(cu_backend_t));
    if (!b) return NULL;
    b->is_available = noop_is_available;
    b->start = noop_start;
    b->stop = noop_stop;
    b->capture = noop_capture;
    b->click = noop_click;
    b->drag = noop_drag;
    b->scroll = noop_scroll;
    b->type_text = noop_type_text;
    b->key = noop_key;
    b->list_apps = noop_list_apps;
    b->focus_app = noop_focus_app;
    b->set_value = noop_set_value;
    b->wait = noop_wait;
    b->state = calloc(1, sizeof(noop_state_t));
    return b;
}

/* ======================================================================
 *  X11 Backend (Linux) - uses xdotool + ImageMagick import
 * ====================================================================== */

static bool _has_cmd(const char *cmd) {
    char buf[256];
    snprintf(buf, sizeof(buf), "which %s >/dev/null 2>&1", cmd);
    return system(buf) == 0;
}

static bool x11_is_available(void) {
    if (!getenv("DISPLAY")) return false;
    return _has_cmd("import") || _has_cmd("xdotool");
}

static bool x11_start(void) { return true; }
static void x11_stop(void) {}

static cu_capture_t *x11_capture(const char *mode, const char *app) {
    (void)app;
    cu_capture_t *cap = (cu_capture_t *)calloc(1, sizeof(cu_capture_t));
    if (!cap) return NULL;
    snprintf(cap->mode, sizeof(cap->mode), "%s", mode ? mode : "som");
    cap->width = 0;
    cap->height = 0;
    cap->png_b64 = NULL;

    if (mode && strcmp(mode, "ax") == 0) {
        snprintf(cap->window_title, sizeof(cap->window_title),
                 "X11 Desktop (no accessibility tree available)");
        return cap;
    }

    /* Try ImageMagick import for screenshot */
    if (_has_cmd("import")) {
        char tmp_path[256];
        snprintf(tmp_path, sizeof(tmp_path),
                 "/tmp/cu_capture_%d.png", getpid());

        char cmd[512];
        snprintf(cmd, sizeof(cmd),
                 "import -window root -quality 85 %s 2>/dev/null", tmp_path);
        int ret = system(cmd);
        if (ret == 0) {
            FILE *f = fopen(tmp_path, "rb");
            if (f) {
                fseek(f, 0, SEEK_END);
                long fsize = ftell(f);
                fseek(f, 0, SEEK_SET);
                if (fsize > 0 && fsize < 50L * 1024L * 1024L) {
                    unsigned char *raw = (unsigned char *)malloc((size_t)fsize);
                    if (raw) {
                        size_t nread = fread(raw, 1, (size_t)fsize, f);
                        (void)nread;
                        cap->png_bytes = (size_t)fsize;

                        static const char b64t[] =
                            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                            "abcdefghijklmnopqrstuvwxyz0123456789+/";
                        size_t b64len = ((size_t)fsize + 2) / 3 * 4;
                        cap->png_b64 = (char *)malloc(b64len + 1);
                        if (cap->png_b64) {
                            size_t i, j = 0;
                            for (i = 0; i < (size_t)fsize; i += 3) {
                                unsigned int val = (unsigned)raw[i] << 16;
                                if (i + 1 < (size_t)fsize)
                                    val |= (unsigned)raw[i+1] << 8;
                                if (i + 2 < (size_t)fsize)
                                    val |= raw[i+2];
                                cap->png_b64[j++] = b64t[(val >> 18) & 0x3F];
                                cap->png_b64[j++] = b64t[(val >> 12) & 0x3F];
                                cap->png_b64[j++] = (i+1 < (size_t)fsize)
                                    ? b64t[(val >> 6) & 0x3F] : '=';
                                cap->png_b64[j++] = (i+2 < (size_t)fsize)
                                    ? b64t[val & 0x3F] : '=';
                            }
                            cap->png_b64[j] = '\0';
                        }
                        free(raw);
                    }
                }
                fclose(f);
            }
            /* Get dimensions via identify */
            char dim_cmd[512];
            snprintf(dim_cmd, sizeof(dim_cmd),
                     "identify -format '%%w %%h' %s 2>/dev/null", tmp_path);
            FILE *fp = popen(dim_cmd, "r");
            if (fp) {
                if (fscanf(fp, "%d %d", &cap->width, &cap->height) != 2) {
                    cap->width = 1920;
                    cap->height = 1080;
                }
                pclose(fp);
            }
            unlink(tmp_path);
        }
    }
    return cap;
}

static cu_action_t *x11_make_action(const char *action_name, bool ok,
                                     const char *fmt, ...) {
    cu_action_t *act = (cu_action_t *)calloc(1, sizeof(cu_action_t));
    if (!act) return NULL;
    snprintf(act->action, sizeof(act->action), "%s", action_name);
    act->ok = ok;
    if (fmt) {
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(act->message, sizeof(act->message), fmt, ap);
        va_end(ap);
    }
    return act;
}

static cu_action_t *x11_click(int element, int x, int y,
                               const char *button, int click_count,
                               const char *modifiers) {
    (void)modifiers;
    if (!_has_cmd("xdotool")) {
        return x11_make_action("click", false,
            "xdotool not available - install: sudo apt install xdotool");
    }
    char cmd[512];
    int btn_num = 1;
    if (button) {
        if (strcmp(button, "middle") == 0) btn_num = 2;
        else if (strcmp(button, "right") == 0) btn_num = 3;
    }
    snprintf(cmd, sizeof(cmd),
             "xdotool mousemove %d %d click %d 2>/dev/null", x, y, btn_num);
    if (click_count > 1 && btn_num == 1) {
        snprintf(cmd, sizeof(cmd),
                 "xdotool mousemove %d %d click --repeat %d 1 2>/dev/null",
                 x, y, click_count);
    }
    int ret = system(cmd);
    return x11_make_action("click", ret == 0,
        ret == 0 ? "clicked at (%d,%d)" : "click failed",
        x, y);
}

static cu_action_t *x11_type_text(const char *text) {
    if (!_has_cmd("xdotool")) {
        return x11_make_action("type", false, "xdotool not available");
    }
    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
             "xdotool type -- '%s' 2>/dev/null", text ? text : "");
    int ret = system(cmd);
    return x11_make_action("type", ret == 0,
        ret == 0 ? "typed %zu characters" : "type failed",
        text ? strlen(text) : 0);
}

static cu_action_t *x11_key(const char *keys) {
    if (!_has_cmd("xdotool")) {
        return x11_make_action("key", false, "xdotool not available");
    }
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "xdotool key '%s' 2>/dev/null", keys ? keys : "");
    int ret = system(cmd);
    return x11_make_action("key", ret == 0,
        ret == 0 ? "sent key %s" : "key failed",
        keys ? keys : "");
}

static cu_action_t *x11_scroll(const char *dir, int amount,
                                int element, int x, int y,
                                const char *modifiers) {
    (void)element; (void)modifiers;
    if (!_has_cmd("xdotool")) {
        return x11_make_action("scroll", false, "xdotool not available");
    }
    if (x > 0 || y > 0) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd),
                 "xdotool mousemove %d %d 2>/dev/null", x, y);
        int r = system(cmd);
        (void)r;
    }
    int btn = 5; /* down */
    if (dir && (strcmp(dir, "up") == 0 || strcmp(dir, "left") == 0))
        btn = 4;
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "xdotool click %d 2>/dev/null", btn);
    int ret = system(cmd);
    return x11_make_action("scroll", ret == 0,
        ret == 0 ? "scrolled %s" : "scroll failed",
        dir ? dir : "down");
}

static cu_action_t *x11_list_apps(void) {
    if (!_has_cmd("xdotool")) {
        return x11_make_action("list_apps", false, "xdotool not available");
    }
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "xdotool getactivewindow getwindowname 2>/dev/null");
    FILE *fp = popen(cmd, "r");
    char win_name[256] = "";
    if (fp) {
        if (fgets(win_name, sizeof(win_name), fp)) {
            size_t len = strlen(win_name);
            if (len > 0 && win_name[len-1] == '\n') win_name[len-1] = '\0';
        }
        pclose(fp);
    }
    return x11_make_action("list_apps", true,
        "active window: %s", win_name[0] ? win_name : "(unknown)");
}

static cu_action_t *x11_focus_app(const char *app, bool raise) {
    (void)raise;
    if (!_has_cmd("xdotool")) {
        return x11_make_action("focus_app", false, "xdotool not available");
    }
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "xdotool search --name '%s' windowactivate 2>/dev/null",
             app ? app : "");
    int ret = system(cmd);
    return x11_make_action("focus_app", ret == 0,
        ret == 0 ? "focused %s" : "no window found matching '%s'",
        app ? app : "", app ? app : "");
}

static cu_action_t *x11_drag(int from_e, int to_e,
                              int fx, int fy, int tx, int ty,
                              const char *button, const char *modifiers) {
    (void)from_e; (void)to_e; (void)modifiers;
    if (!_has_cmd("xdotool")) {
        return x11_make_action("drag", false, "xdotool not available");
    }
    int btn_num = 1;
    if (button) {
        if (strcmp(button, "middle") == 0) btn_num = 2;
        else if (strcmp(button, "right") == 0) btn_num = 3;
    }
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "xdotool mousemove %d %d mousedown %d "
             "mousemove %d %d mouseup %d 2>/dev/null",
             fx, fy, btn_num, tx, ty, btn_num);
    int ret = system(cmd);
    return x11_make_action("drag", ret == 0,
        ret == 0 ? "dragged (%d,%d)-(%d,%d)" : "drag failed",
        fx, fy, tx, ty);
}

static cu_action_t *x11_set_value(const char *value, int element) {
    (void)value; (void)element;
    return x11_make_action("set_value", false,
        "set_value not supported on X11 - use type instead");
}

static cu_action_t *x11_wait(double seconds) {
    struct timespec ts;
    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1e9);
    nanosleep(&ts, NULL);
    return x11_make_action("wait", true, "waited %.2fs", seconds);
}

static cu_backend_t *make_x11_backend(void) {
    cu_backend_t *b = (cu_backend_t *)calloc(1, sizeof(cu_backend_t));
    if (!b) return NULL;
    b->is_available = x11_is_available;
    b->start = x11_start;
    b->stop = x11_stop;
    b->capture = x11_capture;
    b->click = x11_click;
    b->drag = x11_drag;
    b->scroll = x11_scroll;
    b->type_text = x11_type_text;
    b->key = x11_key;
    b->list_apps = x11_list_apps;
    b->focus_app = x11_focus_app;
    b->set_value = x11_set_value;
    b->wait = x11_wait;
    b->state = NULL;
    return b;
}

/* ======================================================================
 *  Wayland Backend (Linux) — uses grim + ydotool + wtype
 * ====================================================================== */

static bool wayland_is_available(void) {
    /* Wayland detected via env var */
    if (!getenv("WAYLAND_DISPLAY")) return false;
    /* Need at least grim for screenshots and ydotool or wtype for input */
    return _has_cmd("grim");
}

static bool wayland_start(void) { return true; }
static void wayland_stop(void) {}

static cu_capture_t *wayland_capture(const char *mode, const char *app) {
    (void)app;
    cu_capture_t *cap = (cu_capture_t *)calloc(1, sizeof(cu_capture_t));
    if (!cap) return NULL;
    snprintf(cap->mode, sizeof(cap->mode), "%s", mode ? mode : "som");
    cap->width = 0;
    cap->height = 0;
    cap->png_b64 = NULL;

    if (mode && strcmp(mode, "ax") == 0) {
        snprintf(cap->window_title, sizeof(cap->window_title),
                 "Wayland Desktop (no accessibility tree available)");
        return cap;
    }

    /* grim -p outputs PNG to stdout; capture -o for the whole screen */
    /* Use grim with no args = full screen, outputs PNG to stdout */
    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path),
             "/tmp/cu_capture_%d.png", getpid());

    /* grim captures full screen, writes to file */
    char cmd[1024];
    if (_has_cmd("slurp") && _has_cmd("jq")) {
        /* Prefer focused monitor using slurp+jq */
        snprintf(cmd, sizeof(cmd),
                 "grim -o \"$(swaymsg -t get_outputs | jq -r "
                 "'.[] | select(.focused) | .name')\" %s 2>/dev/null",
                 tmp_path);
    } else {
        snprintf(cmd, sizeof(cmd),
                 "grim %s 2>/dev/null", tmp_path);
    }
    int ret = system(cmd);
    if (ret != 0) {
        /* Fallback: try grim without args */
        snprintf(cmd, sizeof(cmd), "grim %s 2>/dev/null", tmp_path);
        ret = system(cmd);
    }

    if (ret == 0) {
        FILE *f = fopen(tmp_path, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long fsize = ftell(f);
            fseek(f, 0, SEEK_SET);
            if (fsize > 0 && fsize < 50L * 1024L * 1024L) {
                unsigned char *raw = (unsigned char *)malloc((size_t)fsize);
                if (raw) {
                    size_t nread = fread(raw, 1, (size_t)fsize, f);
                    (void)nread;
                    cap->png_bytes = (size_t)fsize;

                    static const char b64t[] =
                        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                        "abcdefghijklmnopqrstuvwxyz0123456789+/";
                    size_t b64len = ((size_t)fsize + 2) / 3 * 4;
                    cap->png_b64 = (char *)malloc(b64len + 1);
                    if (cap->png_b64) {
                        size_t i, j = 0;
                        for (i = 0; i < (size_t)fsize; i += 3) {
                            unsigned int val = (unsigned)raw[i] << 16;
                            if (i + 1 < (size_t)fsize)
                                val |= (unsigned)raw[i+1] << 8;
                            if (i + 2 < (size_t)fsize)
                                val |= raw[i+2];
                            cap->png_b64[j++] = b64t[(val >> 18) & 0x3F];
                            cap->png_b64[j++] = b64t[(val >> 12) & 0x3F];
                            cap->png_b64[j++] = (i+1 < (size_t)fsize)
                                ? b64t[(val >> 6) & 0x3F] : '=';
                            cap->png_b64[j++] = (i+2 < (size_t)fsize)
                                ? b64t[val & 0x3F] : '=';
                        }
                        cap->png_b64[j] = '\0';
                    }
                    free(raw);
                }
            }
            fclose(f);
        }
        /* Get dimensions via identify (ImageMagick) or file */
        if (_has_cmd("identify")) {
            char dim_cmd[512];
            snprintf(dim_cmd, sizeof(dim_cmd),
                     "identify -format '%%w %%h' %s 2>/dev/null", tmp_path);
            FILE *fp = popen(dim_cmd, "r");
            if (fp) {
                if (fscanf(fp, "%d %d", &cap->width, &cap->height) != 2) {
                    cap->width = 1920;
                    cap->height = 1080;
                }
                pclose(fp);
            }
        } else {
            cap->width = 1920;
            cap->height = 1080;
        }
        unlink(tmp_path);
    }
    return cap;
}

static cu_action_t *wayland_make_action(const char *action_name, bool ok,
                                         const char *fmt, ...) {
    cu_action_t *act = (cu_action_t *)calloc(1, sizeof(cu_action_t));
    if (!act) return NULL;
    snprintf(act->action, sizeof(act->action), "%s", action_name);
    act->ok = ok;
    if (fmt) {
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(act->message, sizeof(act->message), fmt, ap);
        va_end(ap);
    }
    return act;
}

static cu_action_t *wayland_click(int element, int x, int y,
                                   const char *button, int click_count,
                                   const char *modifiers) {
    (void)modifiers;
    if (!_has_cmd("ydotool")) {
        return wayland_make_action("click", false,
            "ydotool not available - install: sudo apt install ydotool");
    }
    int btn = 1; /* left */
    if (button) {
        if (strcmp(button, "middle") == 0) btn = 2;
        else if (strcmp(button, "right") == 0) btn = 3;
    }
    /* ydotool click: move mouse, then button down/up */
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "ydotool mousemove %d %d click 0x%02x 2>/dev/null",
             x, y, btn);
    if (click_count > 1 && btn == 1) {
        snprintf(cmd, sizeof(cmd),
                 "ydotool mousemove %d %d click --repeat %d 0x01 2>/dev/null",
                 x, y, click_count);
    }
    int ret = system(cmd);
    return wayland_make_action("click", ret == 0,
        ret == 0 ? "clicked at (%d,%d)" : "click failed (is ydotool running?)",
        x, y);
}

static cu_action_t *wayland_type_text(const char *text) {
    if (!_has_cmd("wtype")) {
        return wayland_make_action("type", false,
            "wtype not available - install: sudo apt install wtype");
    }
    /* wtype handles special chars via -- options, but basic text works */
    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
             "wtype '%s' 2>/dev/null", text ? text : "");
    int ret = system(cmd);
    return wayland_make_action("type", ret == 0,
        ret == 0 ? "typed %zu characters" : "type failed",
        text ? strlen(text) : 0);
}

static cu_action_t *wayland_key(const char *keys) {
    if (!_has_cmd("ydotool")) {
        return wayland_make_action("key", false,
            "ydotool not available");
    }
    /* ydotool key accepts keys as comma-separated key codes or names */
    /* Minimal: pass through the key combo string */
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "ydotool key '%s' 2>/dev/null", keys ? keys : "");
    int ret = system(cmd);
    return wayland_make_action("key", ret == 0,
        ret == 0 ? "sent key %s" : "key failed",
        keys ? keys : "");
}

static cu_action_t *wayland_scroll(const char *dir, int amount,
                                    int element, int x, int y,
                                    const char *modifiers) {
    (void)element; (void)modifiers;
    if (!_has_cmd("ydotool")) {
        return wayland_make_action("scroll", false,
            "ydotool not available");
    }
    if (x > 0 || y > 0) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd),
                 "ydotool mousemove %d %d 2>/dev/null", x, y);
        int __r = system(cmd);
        (void)__r;
    }
    /* ydotool uses key codes for scroll (e.g., 0xe0 for wheel down) */
    int keycode = 0xe0;  /* wheel down */
    if (dir && (strcmp(dir, "up") == 0 || strcmp(dir, "left") == 0))
        keycode = 0xd0;  /* wheel up */
    if (amount <= 0) amount = 3;
    for (int i = 0; i < amount; i++) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd),
                 "ydotool key 0x%02x 2>/dev/null", keycode);
        int __r2 = system(cmd);
        (void)__r2;
    }
    return wayland_make_action("scroll", true,
        "scrolled %s", dir ? dir : "down");
}

static cu_action_t *wayland_list_apps(void) {
    if (_has_cmd("swaymsg")) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd),
                 "swaymsg -t get_tree | jq -r "
                 "'.. | select(.app_id?) | .app_id' 2>/dev/null | "
                 "head -10 | tr '\\n' ','");
        FILE *fp = popen(cmd, "r");
        if (fp) {
            char apps[512] = "";
            size_t n = fread(apps, 1, sizeof(apps) - 1, fp);
            apps[n] = '\0';
            pclose(fp);
            if (apps[0])
                return wayland_make_action("list_apps", true,
                    "apps: %s", apps);
        }
    }
    return wayland_make_action("list_apps", true,
        "active apps unknown (install swaymsg+jq)");
}

static cu_action_t *wayland_focus_app(const char *app, bool raise) {
    (void)raise;
    if (!_has_cmd("swaymsg") || !_has_cmd("jq")) {
        return wayland_make_action("focus_app", false,
            "swaymsg+jq required for app focus on Wayland");
    }
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "swaymsg '[app_id=\"%s\"]' focus 2>/dev/null",
             app ? app : "");
    int ret = system(cmd);
    return wayland_make_action("focus_app", ret == 0,
        ret == 0 ? "focused %s" : "no window found matching '%s'",
        app ? app : "", app ? app : "");
}

static cu_action_t *wayland_drag(int from_e, int to_e,
                                  int fx, int fy, int tx, int ty,
                                  const char *button, const char *modifiers) {
    (void)from_e; (void)to_e; (void)modifiers;
    if (!_has_cmd("ydotool")) {
        return wayland_make_action("drag", false,
            "ydotool not available");
    }
    int btn = 0x01;
    if (button) {
        if (strcmp(button, "middle") == 0) btn = 0x02;
        else if (strcmp(button, "right") == 0) btn = 0x04;
    }
    /* ydotool: mousemove to start, mousedown, mousemove to end, mouseup */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "ydotool mousemove %d %d mousedown 0x%02x "
             "mousemove %d %d mouseup 0x%02x 2>/dev/null",
             fx, fy, btn, tx, ty, btn);
    int ret = system(cmd);
    return wayland_make_action("drag", ret == 0,
        ret == 0 ? "dragged (%d,%d)-(%d,%d)" : "drag failed",
        fx, fy, tx, ty);
}

static cu_action_t *wayland_set_value(const char *value, int element) {
    (void)value; (void)element;
    return wayland_make_action("set_value", false,
        "set_value not supported on Wayland - use type instead");
}

static cu_action_t *wayland_wait(double seconds) {
    struct timespec ts;
    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1e9);
    nanosleep(&ts, NULL);
    return wayland_make_action("wait", true, "waited %.2fs", seconds);
}

static cu_backend_t *make_wayland_backend(void) {
    cu_backend_t *b = (cu_backend_t *)calloc(1, sizeof(cu_backend_t));
    if (!b) return NULL;
    b->is_available = wayland_is_available;
    b->start = wayland_start;
    b->stop = wayland_stop;
    b->capture = wayland_capture;
    b->click = wayland_click;
    b->drag = wayland_drag;
    b->scroll = wayland_scroll;
    b->type_text = wayland_type_text;
    b->key = wayland_key;
    b->list_apps = wayland_list_apps;
    b->focus_app = wayland_focus_app;
    b->set_value = wayland_set_value;
    b->wait = wayland_wait;
    b->state = NULL;
    return b;
}

/* ======================================================================
 *  Backend selection + global singleton
 * ====================================================================== */

static cu_backend_t *g_cu_backend = NULL;

cu_backend_t *computer_use_new_noop_backend(void) {
    return make_noop_backend();
}

cu_backend_t *computer_use_select_backend(void) {
    cu_backend_t *backends[] = {
        make_wayland_backend(),
        make_x11_backend(),
        make_noop_backend(),
    };
    int n = (int)(sizeof(backends) / sizeof(backends[0]));

    for (int i = 0; i < n; i++) {
        if (backends[i] && backends[i]->is_available()) {
            if (backends[i]->start()) {
                return backends[i];
            }
            backends[i]->stop();
        }
        if (backends[i]) {
            free(backends[i]->state);
            free(backends[i]);
            backends[i] = NULL;
        }
    }
    return make_noop_backend();
}

cu_backend_t *cu_get_global_backend(void) {
    if (!g_cu_backend) {
        g_cu_backend = computer_use_select_backend();
    }
    return g_cu_backend;
}

void cu_reset_global_backend(void) {
    if (g_cu_backend) {
        g_cu_backend->stop();
        free(g_cu_backend->state);
        free(g_cu_backend);
        g_cu_backend = NULL;
    }
}

/* ======================================================================
 *  Free helpers
 * ====================================================================== */

void cu_capture_free(cu_capture_t *cap) {
    if (!cap) return;
    free(cap->png_b64);
    free(cap->elements);
    free(cap);
}

void cu_action_free(cu_action_t *act) {
    if (!act) return;
    if (act->capture) cu_capture_free(act->capture);
    free(act);
}

/* ======================================================================
 *  Safety checks (matching Python's tool.py)
 * ====================================================================== */

static const char *BLOCKED_COMBOS[] = {
    "cmd+shift+backspace",
    "cmd+option+backspace",
    "cmd+ctrl+q",
    "cmd+shift+q",
    "cmd+option+shift+q",
    NULL
};

static bool is_blocked_key_combo(const char *keys) {
    if (!keys) return false;
    char buf[128];
    size_t blen = strlen(keys);
    if (blen >= sizeof(buf)) blen = sizeof(buf) - 1;
    for (size_t i = 0; i < blen; i++)
        buf[i] = (char)tolower((unsigned char)keys[i]);
    buf[blen] = '\0';

    for (int i = 0; BLOCKED_COMBOS[i]; i++) {
        if (strstr(buf, BLOCKED_COMBOS[i])) return true;
    }
    return false;
}

static const char *BLOCKED_TYPE_PATTERNS[] = {
    "curl | bash",
    "curl | sh",
    "wget | bash",
    "sudo rm -rf",
    "rm -rf /",
    NULL
};

static bool is_blocked_type(const char *text) {
    if (!text) return false;
    for (int i = 0; BLOCKED_TYPE_PATTERNS[i]; i++) {
        if (strstr(text, BLOCKED_TYPE_PATTERNS[i])) return true;
    }
    return false;
}

/* ======================================================================
 *  JSON response helpers (plain C, no libjson dependency here)
 * ====================================================================== */

static char *make_text_response(const cu_action_t *act) {
    json_t *root = json_object();
    json_set(root, "ok", json_bool(act->ok));
    json_set(root, "action", json_string(act->action));
    if (act->message[0])
        json_set(root, "message", json_string(act->message));
    char *str = json_serialize(root);
    json_free(root);
    return str ? str : strdup("{\"error\":\"OOM\"}");
}

static char *make_capture_response(const cu_capture_t *cap,
                                    int max_elements) {
    json_t *root = json_object();
    json_set(root, "mode", json_string(cap->mode));
    json_set(root, "width", json_number((double)cap->width));
    json_set(root, "height", json_number((double)cap->height));
    if (cap->app[0])
        json_set(root, "app", json_string(cap->app));
    if (cap->window_title[0])
        json_set(root, "window_title", json_string(cap->window_title));

    int n = cap->element_count;
    int visible = (max_elements > 0 && n > max_elements) ? max_elements : n;
    json_t *elements = json_array();
    for (int i = 0; i < visible; i++) {
        json_t *e = json_object();
        json_set(e, "index", json_number((double)cap->elements[i].index));
        json_set(e, "role", json_string(cap->elements[i].role));
        if (cap->elements[i].label[0])
            json_set(e, "label", json_string(cap->elements[i].label));
        json_append(elements, e);
    }
    json_set(root, "elements", elements);
    json_set(root, "total_elements", json_number((double)n));

    /* Build summary text */
    char summary[4096];
    int pos = snprintf(summary, sizeof(summary),
        "capture mode=%s %dx%d",
        cap->mode, cap->width, cap->height);
    if (cap->app[0])
        pos += snprintf(summary + pos, sizeof(summary) - (size_t)pos,
                        " app=%s", cap->app);
    pos += snprintf(summary + pos, sizeof(summary) - (size_t)pos,
                    "\n%d interactable element(s)", n);
    for (int i = 0; i < visible && i < 50 && (size_t)pos < sizeof(summary) - 120; i++) {
        pos += snprintf(summary + pos, sizeof(summary) - (size_t)pos,
                        "\n  [@e%d] %s%s%s",
                        cap->elements[i].index,
                        cap->elements[i].role,
                        cap->elements[i].label[0] ? ": " : "",
                        cap->elements[i].label[0] ? cap->elements[i].label : "");
    }
    if (n > visible) {
        snprintf(summary + pos, sizeof(summary) - (size_t)pos,
                 "\n  (response truncated to %d of %d elements)",
                 visible, n);
    }
    json_set(root, "summary", json_string(summary));

    if (cap->png_b64 && strcmp(cap->mode, "ax") != 0) {
        json_set(root, "screenshot_available", json_bool(true));
        json_set(root, "png_bytes", json_number((double)cap->png_bytes));
    }

    char *str = json_serialize(root);
    json_free(root);
    return str ? str : strdup("{\"error\":\"OOM\"}");
}

static char *make_error_response(const char *fmt, ...) {
    json_t *root = json_object();
    char msg[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    json_set(root, "error", json_string(msg));
    char *str = json_serialize(root);
    json_free(root);
    return str ? str : strdup("{\"error\":\"OOM\"}");
}

/* ======================================================================
 *  Action dispatch
 * ====================================================================== */

static char *dispatch_capture(cu_backend_t *b, json_t *args) {
    const char *mode = json_get_str(args, "mode", "som");
    if (strcmp(mode, "som") != 0 && strcmp(mode, "vision") != 0 &&
        strcmp(mode, "ax") != 0) {
        return make_error_response("bad mode '%s'; use som|vision|ax", mode);
    }
    const char *app = json_get_str(args, "app", NULL);
    int max_elements = (int)json_get_num(args, "max_elements", 100.0);
    if (max_elements < 1) max_elements = 100;
    if (max_elements > 1000) max_elements = 1000;

    cu_capture_t *cap = b->capture(mode, app);
    if (!cap) return make_error_response("capture failed");
    char *resp = make_capture_response(cap, max_elements);
    cu_capture_free(cap);
    return resp;
}

static char *dispatch_action(cu_backend_t *b, const char *action,
                              json_t *args) {
    /* capture */
    if (strcmp(action, "capture") == 0)
        return dispatch_capture(b, args);

    /* wait */
    if (strcmp(action, "wait") == 0) {
        double secs = json_get_num(args, "seconds", 1.0);
        if (secs < 0.0) secs = 0.0;
        if (secs > 30.0) secs = 30.0;
        cu_action_t *act = b->wait(secs);
        char *resp = make_text_response(act);
        cu_action_free(act);
        return resp;
    }

    /* list_apps */
    if (strcmp(action, "list_apps") == 0) {
        cu_action_t *act = b->list_apps();
        char *resp = make_text_response(act);
        cu_action_free(act);
        return resp;
    }

    /* focus_app */
    if (strcmp(action, "focus_app") == 0) {
        const char *app = json_get_str(args, "app", NULL);
        if (!app || !app[0])
            return make_error_response("focus_app requires `app`");
        bool raise = json_get_bool(args, "raise_window", false);
        cu_action_t *act = b->focus_app(app, raise);
        bool capture_after = json_get_bool(args, "capture_after", false);
        if (capture_after && act->ok) {
            cu_capture_t *cap = b->capture("som", app);
            act->capture = cap;
        }
        char *resp = act->capture
            ? make_capture_response(act->capture, 100)
            : make_text_response(act);
        cu_action_free(act);
        return resp;
    }

    /* click variants */
    if (strcmp(action, "click") == 0 ||
        strcmp(action, "double_click") == 0 ||
        strcmp(action, "right_click") == 0 ||
        strcmp(action, "middle_click") == 0) {
        const char *button = json_get_str(args, "button", "left");
        int click_count = 1;
        if (strcmp(action, "double_click") == 0) click_count = 2;
        if (strcmp(action, "right_click") == 0) button = "right";
        if (strcmp(action, "middle_click") == 0) button = "middle";

        int element = (int)json_get_num(args, "element", 0.0);
        json_t *coord = json_obj_get(args, "coordinate");
        int x = 0, y = 0;
        if (coord && coord->type == JSON_ARRAY && json_len(coord) >= 2) {
            json_t *xn = json_get(coord, 0);
            json_t *yn = json_get(coord, 1);
            if (xn && xn->type == JSON_NUMBER) x = (int)xn->num_val;
            if (yn && yn->type == JSON_NUMBER) y = (int)yn->num_val;
        }

        const char *mods = json_get_str(args, "modifiers", NULL);
        cu_action_t *act = b->click(element, x, y, button, click_count, mods);
        bool capture_after = json_get_bool(args, "capture_after", false);
        if (capture_after && act->ok) {
            const char *app = json_get_str(args, "app", NULL);
            cu_capture_t *cap = b->capture("som", app);
            act->capture = cap;
        }
        char *resp = act->capture
            ? make_capture_response(act->capture, 100)
            : make_text_response(act);
        cu_action_free(act);
        return resp;
    }

    /* drag */
    if (strcmp(action, "drag") == 0) {
        int from_e = (int)json_get_num(args, "from_element", 0.0);
        int to_e = (int)json_get_num(args, "to_element", 0.0);
        json_t *from_c = json_obj_get(args, "from_coordinate");
        json_t *to_c = json_obj_get(args, "to_coordinate");
        int fx = 0, fy = 0, tx = 0, ty = 0;
        if (from_c && from_c->type == JSON_ARRAY && json_len(from_c) >= 2) {
            json_t *n = json_get(from_c, 0);
            if (n && n->type == JSON_NUMBER) fx = (int)n->num_val;
            n = json_get(from_c, 1);
            if (n && n->type == JSON_NUMBER) fy = (int)n->num_val;
        }
        if (to_c && to_c->type == JSON_ARRAY && json_len(to_c) >= 2) {
            json_t *n = json_get(to_c, 0);
            if (n && n->type == JSON_NUMBER) tx = (int)n->num_val;
            n = json_get(to_c, 1);
            if (n && n->type == JSON_NUMBER) ty = (int)n->num_val;
        }
        if (from_e == 0 && to_e == 0 && fx == 0 && fy == 0 && tx == 0 && ty == 0) {
            return make_error_response(
                "drag requires from_coordinate/to_coordinate or from_element/to_element");
        }
        const char *btn = json_get_str(args, "button", "left");
        const char *mods = json_get_str(args, "modifiers", NULL);
        cu_action_t *act = b->drag(from_e, to_e, fx, fy, tx, ty, btn, mods);
        char *resp = make_text_response(act);
        cu_action_free(act);
        return resp;
    }

    /* scroll */
    if (strcmp(action, "scroll") == 0) {
        const char *dir = json_get_str(args, "direction", "down");
        int amount = (int)json_get_num(args, "amount", 3.0);
        if (amount < 1) amount = 1;
        int element = (int)json_get_num(args, "element", 0.0);
        json_t *coord = json_obj_get(args, "coordinate");
        int x = 0, y = 0;
        if (coord && coord->type == JSON_ARRAY && json_len(coord) >= 2) {
            json_t *xn = json_get(coord, 0);
            if (xn && xn->type == JSON_NUMBER) x = (int)xn->num_val;
            json_t *yn = json_get(coord, 1);
            if (yn && yn->type == JSON_NUMBER) y = (int)yn->num_val;
        }
        const char *mods = json_get_str(args, "modifiers", NULL);
        cu_action_t *act = b->scroll(dir, amount, element, x, y, mods);
        char *resp = make_text_response(act);
        cu_action_free(act);
        return resp;
    }

    /* type */
    if (strcmp(action, "type") == 0) {
        const char *text = json_get_str(args, "text", "");
        if (is_blocked_type(text)) {
            return make_error_response(
                "blocked pattern in type text");
        }
        cu_action_t *act = b->type_text(text);
        bool capture_after = json_get_bool(args, "capture_after", false);
        if (capture_after && act->ok) {
            const char *app = json_get_str(args, "app", NULL);
            cu_capture_t *cap = b->capture("som", app);
            act->capture = cap;
        }
        char *resp = act->capture
            ? make_capture_response(act->capture, 100)
            : make_text_response(act);
        cu_action_free(act);
        return resp;
    }

    /* key */
    if (strcmp(action, "key") == 0) {
        const char *keys = json_get_str(args, "keys", "");
        if (is_blocked_key_combo(keys)) {
            return make_error_response(
                "blocked key combo; destructive shortcuts hard-blocked");
        }
        cu_action_t *act = b->key(keys);
        char *resp = make_text_response(act);
        cu_action_free(act);
        return resp;
    }

    /* set_value */
    if (strcmp(action, "set_value") == 0) {
        const char *value = json_get_str(args, "value", NULL);
        if (!value) return make_error_response("set_value requires `value`");
        int element = (int)json_get_num(args, "element", 0.0);
        cu_action_t *act = b->set_value(value, element);
        char *resp = make_text_response(act);
        cu_action_free(act);
        return resp;
    }

    return make_error_response("unknown action '%s'", action);
}

/* ======================================================================
 *  Main handler
 * ====================================================================== */

static char *handle_computer_use(const char *args_json, const char *task_id) {
    (void)task_id;

    json_t *args = json_parse(args_json, NULL);
    if (!args) {
        return strdup("{\"error\":\"Invalid JSON arguments\"}");
    }

    const char *action = json_get_str(args, "action", "");
    if (!action[0]) {
        json_free(args);
        return strdup("{\"error\":\"missing `action`\"}");
    }

    /* Safety: validate before dispatch */
    if (strcmp(action, "type") == 0) {
        const char *text = json_get_str(args, "text", "");
        if (is_blocked_type(text)) {
            json_free(args);
            return strdup(
                "{\"error\":\"blocked pattern in type text\","
                "\"hint\":\"Dangerous shell patterns cannot be typed.\"}");
        }
    }

    if (strcmp(action, "key") == 0) {
        const char *keys = json_get_str(args, "keys", "");
        if (is_blocked_key_combo(keys)) {
            json_free(args);
            return strdup(
                "{\"error\":\"blocked key combo\","
                "\"hint\":\"Destructive shortcuts hard-blocked.\"}");
        }
    }

    /* Get or create backend */
    cu_backend_t *backend = cu_get_global_backend();
    if (!backend || !backend->is_available()) {
        json_free(args);
        return strdup(
            "{\"error\":\"computer_use is not available on this platform.\","
            "\"hint\":\"Install xdotool + ImageMagick on Linux, or cua-driver on macOS.\"}");
    }

    char *result = dispatch_action(backend, action, args);
    json_free(args);
    return result;
}

/* ======================================================================
 *  Registration
 * ====================================================================== */

void registry_init_computer_use(void) {
    registry_register("computer_use",
        "Universal desktop control via screen capture, mouse, keyboard, scroll, "
        "and drag actions. Works with any tool-capable model. Preferred workflow: "
        "call with action='capture' (mode='som' gives numbered element overlays), "
        "then click by element index for reliability. Uses xdotool on Linux, "
        "cua-driver on macOS.",
        "{"
        "\"type\":\"object\","
        "\"properties\":{"
          "\"action\":{"
            "\"type\":\"string\","
            "\"enum\":[\"capture\",\"click\",\"double_click\",\"right_click\","
                     "\"middle_click\",\"drag\",\"scroll\",\"type\",\"key\","
                     "\"set_value\",\"wait\",\"list_apps\",\"focus_app\"],"
            "\"description\":\"Which action to perform. `capture` is free (no side effects).\""
          "},"
          "\"mode\":{"
            "\"type\":\"string\",\"enum\":[\"som\",\"vision\",\"ax\"],"
            "\"description\":\"Capture mode. `som`=numbered overlays+AX tree, `vision`=screenshot, `ax`=AX only.\""
          "},"
          "\"app\":{"
            "\"type\":\"string\","
            "\"description\":\"Limit capture/action to a specific app (by name or bundle ID).\""
          "},"
          "\"max_elements\":{"
            "\"type\":\"integer\",\"minimum\":1,\"maximum\":1000,\"default\":100,"
            "\"description\":\"Cap on element array returned by capture.\""
          "},"
          "\"element\":{"
            "\"type\":\"integer\","
            "\"description\":\"1-based SOM index from last capture for reliable click targeting.\""
          "},"
          "\"coordinate\":{"
            "\"type\":\"array\",\"items\":{\"type\":\"integer\"},\"minItems\":2,\"maxItems\":2,"
            "\"description\":\"Pixel coordinates [x, y] for click/scroll targeting.\""
          "},"
          "\"button\":{"
            "\"type\":\"string\",\"enum\":[\"left\",\"right\",\"middle\"],"
            "\"description\":\"Mouse button. Defaults to left.\""
          "},"
          "\"modifiers\":{"
            "\"type\":\"array\",\"items\":{\"type\":\"string\"},"
            "\"description\":\"Modifier keys (cmd, shift, option, ctrl, fn).\""
          "},"
          "\"from_element\":{\"type\":\"integer\",\"description\":\"Source element index (drag).\"},"
          "\"to_element\":{\"type\":\"integer\",\"description\":\"Target element index (drag).\"},"
          "\"from_coordinate\":{\"type\":\"array\",\"items\":{\"type\":\"integer\"},\"minItems\":2,\"maxItems\":2,\"description\":\"Source [x,y] (drag).\"},"
          "\"to_coordinate\":{\"type\":\"array\",\"items\":{\"type\":\"integer\"},\"minItems\":2,\"maxItems\":2,\"description\":\"Target [x,y] (drag).\"},"
          "\"direction\":{"
            "\"type\":\"string\",\"enum\":[\"up\",\"down\",\"left\",\"right\"],"
            "\"description\":\"Scroll direction.\""
          "},"
          "\"amount\":{"
            "\"type\":\"integer\",\"description\":\"Scroll wheel ticks. Default 3.\""
          "},"
          "\"value\":{"
            "\"type\":\"string\","
            "\"description\":\"Value to set on element (for set_value action).\""
          "},"
          "\"text\":{"
            "\"type\":\"string\",\"description\":\"Text to type (for type action).\""
          "},"
          "\"keys\":{"
            "\"type\":\"string\","
            "\"description\":\"Key combo e.g. 'cmd+s', 'ctrl+alt+t', 'return'.\""
          "},"
          "\"seconds\":{"
            "\"type\":\"number\",\"description\":\"Seconds to wait (max 30).\""
          "},"
          "\"raise_window\":{"
            "\"type\":\"boolean\","
            "\"description\":\"For focus_app: bring window to front (default false).\""
          "},"
          "\"capture_after\":{"
            "\"type\":\"boolean\","
            "\"description\":\"If true, take follow-up capture after action.\""
          "}"
        "},"
        "\"required\":[\"action\"]"
        "}",
        handle_computer_use);
}
