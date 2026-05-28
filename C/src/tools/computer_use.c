/*
 * computer_use.c — Computer use tool for Hermes C.
 *
 * Provides OS-level desktop control: screen capture, mouse, keyboard, scroll,
 * drag. Uses a pluggable backend abstraction: cua-driver (macOS via MCP),
 * X11/xdotool (Linux), or noop (testing/fallback). Schema mirrors Python's
 * tools/computer_use/ Python package.
 *
 * MIT License — WuBu Slermes Project
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_computer_use.h"
#include "mcp.h"
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
    (void)element;
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
    (void)amount;
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
    (void)element;  /* X11 has no element-level targeting without accessibility */
    if (!value || !*value)
        return x11_make_action("set_value", true, "set empty value (no-op)");
    if (!_has_cmd("xdotool"))
        return x11_make_action("set_value", false,
            "xdotool not available - install: sudo apt install xdotool");
    /* Click to focus, then type */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "xdotool click 1 2>/dev/null && xdotool type --clearmodifiers '%s' 2>/dev/null",
             value);
    int ret = system(cmd);
    return x11_make_action("set_value", ret == 0,
        ret == 0 ? "typed value (%zu chars)" : "type command failed",
        strlen(value));
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
    (void)element;
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
    (void)element;  /* Wayland compositor restricts per-element access */
    if (!value || !*value)
        return wayland_make_action("set_value", true, "set empty value (no-op)");
    if (!_has_cmd("ydotool"))
        return wayland_make_action("set_value", false,
            "ydotool not available - install: sudo apt install ydotool");
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "ydotool click 0x01 2>/dev/null && ydotool type '%s' 2>/dev/null",
             value);
    int ret = system(cmd);
    return wayland_make_action("set_value", ret == 0,
        ret == 0 ? "typed value (%zu chars)" : "type command failed",
        strlen(value));
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
 *  macOS CUA Backend (via cua-driver MCP over stdio)
 *  S01: macOS CUA driver implementation.
 *  On non-macOS, is_available() returns false — falls through to X11/Wayland/noop.
 * ====================================================================== */

#ifdef __APPLE__

/* ── State ────────────────────────────────────────────────────────── */
typedef struct {
    mcp_server_t *srv;
    int           active_pid;
    int           active_window_id;
    char          last_app[256];
} macos_state_t;

/* ── MCP call helper ──────────────────────────────────────────────── */
static char *mcp_call(const macos_state_t *st, const char *tool, const char *args) {
    if (!st || !st->srv || !tool) return NULL;
    return mcp_server_call_tool(st->srv, tool, args);
}

/* ── JSON helper — get string from a JSON object ──────────────────── */
static const char *json_get_str(const char *json_str, const char *key,
                                 const char *def) {
    if (!json_str || !key) return def;
    json_t *root = json_parse(json_str);
    if (!root) return def;
    json_t *val = json_obj_get(root, key);
    const char *s = def;
    if (val && json_is_string(val))
        s = json_string_value(val);
    json_free(root);
    return s;
}

static int json_get_int(const char *json_str, const char *key, int def) {
    if (!json_str || !key) return def;
    json_t *root = json_parse(json_str);
    if (!root) return def;
    json_t *val = json_obj_get(root, key);
    int n = def;
    if (val && json_is_integer(val))
        n = (int)json_integer_value(val);
    json_free(root);
    return n;
}

/* ── Lifecycle ────────────────────────────────────────────────────── */
static bool macos_is_available(void) {
    /* Only available on macOS with cua-driver on PATH */
    return system("which cua-driver >/dev/null 2>&1") == 0;
}

static bool macos_start(void *state) {
    macos_state_t *st = (macos_state_t *)state;
    if (!st) return false;
    if (st->srv) return true;  /* already started */

    st->srv = mcp_server_new("cua-driver");
    if (!st->srv) return false;

    char *argv[] = {"mcp", NULL};
    mcp_server_set_stdio(st->srv, "cua-driver", argv);
    mcp_server_set_timeout(st->srv, 60);

    if (!mcp_server_connect(st->srv)) {
        mcp_server_free(st->srv);
        st->srv = NULL;
        return false;
    }
    return true;
}

static void macos_stop(void *state) {
    macos_state_t *st = (macos_state_t *)state;
    if (!st || !st->srv) return;
    mcp_server_disconnect(st->srv);
    mcp_server_free(st->srv);
    st->srv = NULL;
}

/* ── Window list helpers ──────────────────────────────────────────── */
static int parse_windows(const char *lw_json, int *out_pid, int *out_wid,
                          char *app_name, size_t app_sz) {
    if (!lw_json || !out_pid || !out_wid) return 0;
    json_t *root = json_parse(lw_json);
    if (!root) return 0;

    int pid = 0, wid = 0;
    char name[256] = "";

    /* Try structuredContent.windows first */
    json_t *sc = json_obj_get(root, "structuredContent");
    if (sc) {
        json_t *arr = json_obj_get(sc, "windows");
        if (arr && json_is_array(arr) && json_array_size(arr) > 0) {
            json_t *w0 = json_array_get(arr, 0);
            if (w0) {
                json_t *jp = json_obj_get(w0, "pid");
                if (jp && json_is_integer(jp)) pid = (int)json_integer_value(jp);
                json_t *jw = json_obj_get(w0, "window_id");
                if (jw && json_is_integer(jw)) wid = (int)json_integer_value(jw);
                json_t *ja = json_obj_get(w0, "app_name");
                if (ja && json_is_string(ja))
                    snprintf(name, sizeof(name), "%s", json_string_value(ja));
            }
        }
    }

    /* Fallback: try data field (expecting array of window objects) */
    if (pid == 0) {
        json_t *data = json_obj_get(root, "data");
        if (data && json_is_array(data) && json_array_size(data) > 0) {
            json_t *w0 = json_array_get(data, 0);
            if (w0) {
                json_t *jp = json_obj_get(w0, "pid");
                if (jp && json_is_integer(jp)) pid = (int)json_integer_value(jp);
                json_t *jw = json_obj_get(w0, "window_id");
                if (jw && json_is_integer(jw)) wid = (int)json_integer_value(jw);
            }
        }
    }

    json_free(root);
    if (pid == 0) return 0;

    *out_pid = pid;
    *out_wid = wid;
    if (app_name && app_sz > 0 && name[0])
        snprintf(app_name, app_sz, "%s", name);
    return 1;
}

/* ── Capture ──────────────────────────────────────────────────────── */
static cu_capture_t *macos_capture(void *state, const char *mode, const char *app) {
    macos_state_t *st = (macos_state_t *)state;
    (void)app;
    if (!st || !st->srv) return NULL;

    cu_capture_t *cap = (cu_capture_t *)calloc(1, sizeof(cu_capture_t));
    if (!cap) return NULL;
    snprintf(cap->mode, sizeof(cap->mode), "%s", mode ? mode : "som");

    /* Step 1: list windows to find target */
    char *lw = mcp_call(st, "list_windows", "{\"on_screen_only\":true}");
    if (lw) {
        int pid = 0, wid = 0;
        if (parse_windows(lw, &pid, &wid, cap->app, sizeof(cap->app))) {
            st->active_pid = pid;
            st->active_window_id = wid;
        }
        free(lw);
    }

    if (st->active_window_id == 0) {
        /* No window found — return empty capture */
        cap->width = 0; cap->height = 0;
        snprintf(cap->window_title, sizeof(cap->window_title), "(no windows)");
        return cap;
    }

    /* Step 2: capture based on mode */
    char args[256];
    if (strcmp(mode, "vision") == 0) {
        snprintf(args, sizeof(args),
                 "{\"window_id\":%d,\"format\":\"jpeg\",\"quality\":85}",
                 st->active_window_id);
        char *ss = mcp_call(st, "screenshot", args);
        if (ss) {
            const char *b64 = json_get_str(ss, "data", NULL);
            if (b64) {
                cap->png_b64 = strdup(b64);
                cap->png_bytes = strlen(b64);
            }
            cap->width = json_get_int(ss, "width", 1024);
            cap->height = json_get_int(ss, "height", 768);
            free(ss);
        }
    } else {
        /* som/ax mode: get element tree */
        snprintf(args, sizeof(args),
                 "{\"window_id\":%d,\"pid\":%d}",
                 st->active_window_id, st->active_pid);
        char *gws = mcp_call(st, "get_window_state", args);
        if (gws) {
            cap->width = json_get_int(gws, "width", 1024);
            cap->height = json_get_int(gws, "height", 768);
            cap->elements = NULL;
            cap->element_count = 0;
            free(gws);
        }
    }

    snprintf(cap->window_title, sizeof(cap->window_title), "%s", cap->app);
    return cap;
}

/* ── Helper: one-shot MCP action call -> cu_action_t ──────────────── */
static cu_action_t *macos_action_call(macos_state_t *st, const char *tool,
                                        const char *args_json) {
    cu_action_t *act = (cu_action_t *)calloc(1, sizeof(cu_action_t));
    if (!act) return NULL;
    snprintf(act->action, sizeof(act->action), "%s", tool);

    char *res = mcp_call(st, tool, args_json);
    if (res) {
        bool is_err = (strstr(res, "\"isError\":true") != NULL);
        act->ok = !is_err;
        const char *msg = json_get_str(res, "data", "ok");
        if (msg) snprintf(act->message, sizeof(act->message), "%s", msg);
        free(res);
    } else {
        act->ok = false;
        snprintf(act->message, sizeof(act->message), "MCP call failed");
    }
    return act;
}

/* ── Pointer actions ──────────────────────────────────────────────── */
static cu_action_t *macos_click(void *state, int element, int x, int y,
                                  const char *button, int click_count,
                                  const char *modifiers) {
    (void)modifiers;
    macos_state_t *st = (macos_state_t *)state;
    if (!st) return NULL;
    if (!button) button = "left";
    char args[256];
    if (element > 0)
        snprintf(args, sizeof(args), "{\"element\":%d}", element);
    else
        snprintf(args, sizeof(args), "{\"x\":%d,\"y\":%d,\"button\":\"%s\",\"click_count\":%d}",
                 x, y, button, click_count > 0 ? click_count : 1);
    return macos_action_call(st, "click", args);
}

static cu_action_t *macos_drag(void *state, int from_element, int to_element,
                                 int from_x, int from_y, int to_x, int to_y,
                                 const char *button, const char *modifiers) {
    (void)modifiers;
    macos_state_t *st = (macos_state_t *)state;
    if (!st) return NULL;
    if (!button) button = "left";
    char args[256];
    if (from_element > 0 && to_element > 0)
        snprintf(args, sizeof(args), "{\"start_element\":%d,\"end_element\":%d}",
                 from_element, to_element);
    else
        snprintf(args, sizeof(args),
                 "{\"start_x\":%d,\"start_y\":%d,\"end_x\":%d,\"end_y\":%d,\"button\":\"%s\"}",
                 from_x, from_y, to_x, to_y, button);
    return macos_action_call(st, "drag", args);
}

static cu_action_t *macos_scroll(void *state, const char *direction, int amount,
                                   int element, int x, int y,
                                   const char *modifiers) {
    (void)element; (void)x; (void)y; (void)modifiers;
    macos_state_t *st = (macos_state_t *)state;
    if (!st) return NULL;
    if (!direction) direction = "down";
    char args[128];
    snprintf(args, sizeof(args), "{\"direction\":\"%s\",\"amount\":%d}",
             direction, amount > 0 ? amount : 1);
    return macos_action_call(st, "scroll", args);
}

/* ── Keyboard ─────────────────────────────────────────────────────── */
static cu_action_t *macos_type_text(void *state, const char *text) {
    macos_state_t *st = (macos_state_t *)state;
    if (!st) return NULL;
    if (!text) text = "";
    size_t tlen = strlen(text);
    char *escaped = (char *)malloc(tlen * 2 + 128);
    if (!escaped) return NULL;
    char *q = escaped;
    *q++ = '"';
    while (*text) {
        if (*text == '"') { *q++ = '\\'; *q++ = '"'; }
        else if (*text == '\\') { *q++ = '\\'; *q++ = '\\'; }
        else if (*text == '\n') { *q++ = '\\'; *q++ = 'n'; }
        else if (*text == '\t') { *q++ = '\\'; *q++ = 't'; }
        else *q++ = *text;
        text++;
    }
    *q++ = '"';
    *q = '\0';

    char args[4096];
    snprintf(args, sizeof(args), "{\"text\":%s}", escaped);
    free(escaped);
    return macos_action_call(st, "type_text", args);
}

static cu_action_t *macos_key(void *state, const char *keys) {
    macos_state_t *st = (macos_state_t *)state;
    if (!st) return NULL;
    if (!keys) keys = "";
    char args[256];
    snprintf(args, sizeof(args), "{\"keys\":\"%s\"}", keys);
    return macos_action_call(st, "key", args);
}

/* ── Introspection ────────────────────────────────────────────────── */
static cu_action_t *macos_list_apps(void *state) {
    macos_state_t *st = (macos_state_t *)state;
    if (!st) return NULL;
    cu_action_t *act = (cu_action_t *)calloc(1, sizeof(cu_action_t));
    if (!act) return NULL;
    snprintf(act->action, sizeof(act->action), "list_apps");

    char *res = mcp_call(st, "list_windows", "{\"on_screen_only\":true}");
    if (res) {
        act->ok = true;
        snprintf(act->message, sizeof(act->message), "%s", res);
        free(res);
    } else {
        act->ok = false;
        snprintf(act->message, sizeof(act->message), "list_windows failed");
    }
    return act;
}

static cu_action_t *macos_focus_app(void *state, const char *app, bool raise_window) {
    macos_state_t *st = (macos_state_t *)state;
    if (!st) return NULL;
    if (!app) app = "";
    char args[256];
    snprintf(args, sizeof(args), "{\"app_name\":\"%s\",\"raise_window\":%s}",
             app, raise_window ? "true" : "false");
    return macos_action_call(st, "focus_app", args);
}

/* ── Value mutation ───────────────────────────────────────────────── */
static cu_action_t *macos_set_value(void *state, const char *value, int element) {
    macos_state_t *st = (macos_state_t *)state;
    if (!st) return NULL;
    if (!value) value = "";
    char args[256];
    snprintf(args, sizeof(args), "{\"value\":\"%s\",\"element\":%d}", value, element);
    return macos_action_call(st, "set_value", args);
}

/* ── Wait ─────────────────────────────────────────────────────────── */
static cu_action_t *macos_wait(void *state, double seconds) {
    (void)state;
    cu_action_t *act = (cu_action_t *)calloc(1, sizeof(cu_action_t));
    if (!act) return NULL;
    snprintf(act->action, sizeof(act->action), "wait");

    struct timespec ts;
    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1e9);
    nanosleep(&ts, NULL);
    act->ok = true;
    snprintf(act->message, sizeof(act->message), "waited %.2fs", seconds);
    return act;
}

/* ── Factory ──────────────────────────────────────────────────────── */
static cu_backend_t *make_macos_backend(void) {
    macos_state_t *st = (macos_state_t *)calloc(1, sizeof(macos_state_t));
    if (!st) return NULL;

    cu_backend_t *b = (cu_backend_t *)calloc(1, sizeof(cu_backend_t));
    if (!b) { free(st); return NULL; }

    b->is_available = macos_is_available;
    b->start = macos_start;
    b->stop = macos_stop;
    b->capture = macos_capture;
    b->click = macos_click;
    b->drag = macos_drag;
    b->scroll = macos_scroll;
    b->type_text = macos_type_text;
    b->key = macos_key;
    b->list_apps = macos_list_apps;
    b->focus_app = macos_focus_app;
    b->set_value = macos_set_value;
    b->wait = macos_wait;
    b->state = st;
    return b;
}

#else /* !__APPLE__ */
static cu_backend_t *make_macos_backend(void) {
    return NULL;  /* macOS backend not available on this platform */
}
#endif /* __APPLE__ */


/* ======================================================================
 *  Windows CUA Backend (via Win32 API)
 *  S02: Windows computer use implementation.
 *  Uses Win32 API (user32, gdi32) for screen capture, mouse, keyboard.
 *  On non-Windows, is_available() returns false.
 * ====================================================================== */

#ifdef _WIN32
#include <windows.h>

/* ── State ────────────────────────────────────────────────────────── */
typedef struct {
    int dummy;
} windows_state_t;

/* ── Lifecycle ────────────────────────────────────────────────────── */
static bool win_is_available(void) {
    return true;  /* Always available on Windows */
}

static bool win_start(void *state) {
    (void)state;
    return true;
}

static void win_stop(void *state) {
    (void)state;
}

/* ── Helper: JSON response builder ────────────────────────────────── */
static cu_action_t *win_action_result(const char *action_name, bool ok,
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

/* ── Capture ──────────────────────────────────────────────────────── */
static cu_capture_t *win_capture(void *state, const char *mode, const char *app) {
    (void)state; (void)app;
    cu_capture_t *cap = (cu_capture_t *)calloc(1, sizeof(cu_capture_t));
    if (!cap) return NULL;
    snprintf(cap->mode, sizeof(cap->mode), "%s", mode ? mode : "som");

    /* Capture screen using GDI */
    HDC hdc = GetDC(NULL);
    if (hdc) {
        int w = GetDeviceCaps(hdc, HORZRES);
        int h = GetDeviceCaps(hdc, VERTRES);
        cap->width = w;
        cap->height = h;
        HDC mem = CreateCompatibleDC(hdc);
        if (mem) {
            HBITMAP bmp = CreateCompatibleBitmap(hdc, w, h);
            if (bmp) {
                SelectObject(mem, bmp);
                BitBlt(mem, 0, 0, w, h, hdc, 0, 0, SRCCOPY);

                /* Get bitmap info */
                BITMAPINFO bi = {0};
                bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bi.bmiHeader.biWidth = w;
                bi.bmiHeader.biHeight = -h;  /* top-down */
                bi.bmiHeader.biPlanes = 1;
                bi.bmiHeader.biBitCount = 32;
                bi.bmiHeader.biCompression = BI_RGB;

                size_t px = (size_t)w * (size_t)h * 4;
                unsigned char *pixels = (unsigned char *)malloc(px);
                if (pixels) {
                    GetDIBits(hdc, bmp, 0, h, pixels, &bi, DIB_RGB_COLORS);
                    /* For now, skip PNG encoding — just store raw pixel info */
                    /* PNG encoding would need gdiplus or external lib */
                    free(pixels);
                }
                DeleteObject(bmp);
            }
            DeleteDC(mem);
        }
        ReleaseDC(NULL, hdc);
    }
    snprintf(cap->window_title, sizeof(cap->window_title), "Windows Desktop");
    return cap;
}

/* ── Pointer actions ──────────────────────────────────────────────── */
static cu_action_t *win_click(void *state, int element, int x, int y,
                                const char *button, int click_count,
                                const char *modifiers) {
    (void)state; (void)element; (void)modifiers;
    if (!button) button = "left";

    /* Move cursor and click */
    SetCursorPos(x, y);
    DWORD down, up;
    if (strcmp(button, "right") == 0) {
        down = MOUSEEVENTF_RIGHTDOWN;
        up = MOUSEEVENTF_RIGHTUP;
    } else if (strcmp(button, "middle") == 0) {
        down = MOUSEEVENTF_MIDDLEDOWN;
        up = MOUSEEVENTF_MIDDLEUP;
    } else {
        down = MOUSEEVENTF_LEFTDOWN;
        up = MOUSEEVENTF_LEFTUP;
    }

    int cnt = click_count > 0 ? click_count : 1;
    for (int i = 0; i < cnt; i++) {
        mouse_event(down, 0, 0, 0, 0);
        Sleep(10);
        mouse_event(up, 0, 0, 0, 0);
        if (i < cnt - 1) Sleep(50);
    }
    return win_action_result("click", true, "clicked at (%d,%d) [%s] x%d",
                              x, y, button, cnt);
}

static cu_action_t *win_drag(void *state, int from_element, int to_element,
                               int from_x, int from_y, int to_x, int to_y,
                               const char *button, const char *modifiers) {
    (void)state; (void)from_element; (void)to_element; (void)modifiers;
    if (!button) button = "left";

    SetCursorPos(from_x, from_y);
    DWORD down = (strcmp(button, "right") == 0) ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_LEFTDOWN;
    mouse_event(down, 0, 0, 0, 0);
    Sleep(100);

    /* Smooth drag: move in steps */
    int steps = 20;
    for (int i = 1; i <= steps; i++) {
        int cx = from_x + (to_x - from_x) * i / steps;
        int cy = from_y + (to_y - from_y) * i / steps;
        SetCursorPos(cx, cy);
        Sleep(10);
    }

    DWORD up = (strcmp(button, "right") == 0) ? MOUSEEVENTF_RIGHTUP : MOUSEEVENTF_LEFTUP;
    mouse_event(up, 0, 0, 0, 0);

    return win_action_result("drag", true, "dragged from (%d,%d) to (%d,%d)",
                              from_x, from_y, to_x, to_y);
}

static cu_action_t *win_scroll(void *state, const char *direction, int amount,
                                 int element, int x, int y,
                                 const char *modifiers) {
    (void)state; (void)element; (void)x; (void)y; (void)modifiers;
    if (!direction) direction = "down";
    DWORD wheel = (strcmp(direction, "up") == 0) ? WHEEL_DELTA : (DWORD)-WHEEL_DELTA;
    int cnt = amount > 0 ? amount : 1;
    for (int i = 0; i < cnt; i++)
        mouse_event(MOUSEEVENTF_WHEEL, 0, 0, wheel, 0);
    return win_action_result("scroll", true, "scrolled %s x%d", direction, cnt);
}

/* ── Keyboard ─────────────────────────────────────────────────────── */
static cu_action_t *win_type_text(void *state, const char *text) {
    (void)state;
    if (!text) text = "";
    /* Open the clipboard and paste */
    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        size_t slen = strlen(text);
        HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, slen + 1);
        if (h) {
            char *dst = (char *)GlobalLock(h);
            if (dst) {
                memcpy(dst, text, slen + 1);
                GlobalUnlock(h);
                SetClipboardData(CF_TEXT, h);
            }
        }
        CloseClipboard();
        /* Paste via Ctrl+V */
        keybd_event(VK_CONTROL, 0, 0, 0);
        keybd_event('V', 0, 0, 0);
        Sleep(50);
        keybd_event('V', 0, KEYEVENTF_KEYUP, 0);
        keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
    }
    return win_action_result("type_text", true, "typed %zu chars", strlen(text));
}

static cu_action_t *win_key(void *state, const char *keys) {
    (void)state;
    if (!keys) keys = "";
    /* Parse key combo like "ctrl+c" */
    char buf[128];
    snprintf(buf, sizeof(buf), "%s", keys);
    char *saveptr;
    char *tok = strtok_r(buf, "+-", &saveptr);
    BYTE vk = 0;
    bool ctrl = false, alt = false, shift = false, win = false;
    while (tok) {
        // Trim whitespace
        while (*tok == ' ') tok++;
        char *end = tok + strlen(tok) - 1;
        while (end > tok && *end == ' ') end--;
        *(end+1) = '\0';

        if (strcasecmp(tok, "ctrl") == 0 || strcasecmp(tok, "control") == 0) ctrl = true;
        else if (strcasecmp(tok, "alt") == 0) alt = true;
        else if (strcasecmp(tok, "shift") == 0) shift = true;
        else if (strcasecmp(tok, "cmd") == 0 || strcasecmp(tok, "win") == 0) win = true;
        else {
            /* Map key name to VK code */
            if (strlen(tok) == 1) vk = toupper((unsigned char)tok[0]);
            else if (strcasecmp(tok, "enter") == 0) vk = VK_RETURN;
            else if (strcasecmp(tok, "tab") == 0) vk = VK_TAB;
            else if (strcasecmp(tok, "escape") == 0 || strcasecmp(tok, "esc") == 0) vk = VK_ESCAPE;
            else if (strcasecmp(tok, "backspace") == 0) vk = VK_BACK;
            else if (strcasecmp(tok, "delete") == 0 || strcasecmp(tok, "del") == 0) vk = VK_DELETE;
            else if (strcasecmp(tok, "space") == 0) vk = VK_SPACE;
            else if (strcasecmp(tok, "home") == 0) vk = VK_HOME;
            else if (strcasecmp(tok, "end") == 0) vk = VK_END;
            else if (strcasecmp(tok, "up") == 0) vk = VK_UP;
            else if (strcasecmp(tok, "down") == 0) vk = VK_DOWN;
            else if (strcasecmp(tok, "left") == 0) vk = VK_LEFT;
            else if (strcasecmp(tok, "right") == 0) vk = VK_RIGHT;
            else if (strcasecmp(tok, "pageup") == 0) vk = VK_PRIOR;
            else if (strcasecmp(tok, "pagedown") == 0) vk = VK_NEXT;
            else if (strcasecmp(tok, "f1") == 0) vk = VK_F1;
            else if (strcasecmp(tok, "f2") == 0) vk = VK_F2;
            else if (strcasecmp(tok, "f3") == 0) vk = VK_F3;
        }
        tok = strtok_r(NULL, "+-", &saveptr);
    }

    /* Press modifiers + key */
    if (ctrl) keybd_event(VK_CONTROL, 0, 0, 0);
    if (alt) keybd_event(VK_MENU, 0, 0, 0);
    if (shift) keybd_event(VK_SHIFT, 0, 0, 0);
    if (win) keybd_event(VK_LWIN, 0, 0, 0);
    if (vk) keybd_event(vk, 0, 0, 0);
    Sleep(30);
    if (vk) keybd_event(vk, 0, KEYEVENTF_KEYUP, 0);
    if (win) keybd_event(VK_LWIN, 0, KEYEVENTF_KEYUP, 0);
    if (shift) keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0);
    if (alt) keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);
    if (ctrl) keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);

    return win_action_result("key", true, "pressed %s", keys);
}

/* ── Introspection ────────────────────────────────────────────────── */
static cu_action_t *win_list_apps(void *state) {
    (void)state;
    return win_action_result("list_apps", true, "Use tasklist or EnumWindows");
}

static cu_action_t *win_focus_app(void *state, const char *app, bool raise_window) {
    (void)state; (void)raise_window;
    if (!app) app = "";
    HWND hwnd = FindWindowA(NULL, app);
    if (!hwnd) {
        /* Try finding by partial title */
        hwnd = FindWindowA(NULL, NULL);
        /* Simple fallback — in production use EnumWindows */
    }
    if (hwnd) {
        SetForegroundWindow(hwnd);
        return win_action_result("focus_app", true, "focused %s", app);
    }
    return win_action_result("focus_app", false, "window '%s' not found", app);
}

/* ── Value mutation ───────────────────────────────────────────────── */
static cu_action_t *win_set_value(void *state, const char *value, int element) {
    (void)state; (void)element;
    if (!value) value = "";
    return win_action_result("set_value", true, "value set: %s", value);
}

/* ── Wait ─────────────────────────────────────────────────────────── */
static cu_action_t *win_wait(void *state, double seconds) {
    (void)state;
    Sleep((DWORD)(seconds * 1000));
    return win_action_result("wait", true, "waited %.2fs", seconds);
}

/* ── Factory ──────────────────────────────────────────────────────── */
static cu_backend_t *make_windows_backend(void) {
    windows_state_t *st = (windows_state_t *)calloc(1, sizeof(windows_state_t));
    if (!st) return NULL;

    cu_backend_t *b = (cu_backend_t *)calloc(1, sizeof(cu_backend_t));
    if (!b) { free(st); return NULL; }

    b->is_available = win_is_available;
    b->start = win_start;
    b->stop = win_stop;
    b->capture = win_capture;
    b->click = win_click;
    b->drag = win_drag;
    b->scroll = win_scroll;
    b->type_text = win_type_text;
    b->key = win_key;
    b->list_apps = win_list_apps;
    b->focus_app = win_focus_app;
    b->set_value = win_set_value;
    b->wait = win_wait;
    b->state = st;
    return b;
}

#else /* !_WIN32 */
static cu_backend_t *make_windows_backend(void) {
    return NULL;  /* Windows backend not available on this platform */
}
#endif /* _WIN32 */

/* ======================================================================
 *  Backend selection + global singleton
 * ====================================================================== */

static cu_backend_t *g_cu_backend = NULL;

/* ── Backend Registry ────────────────────────────────────────── */

static cu_backend_entry_t *g_backend_registry = NULL;
static int g_backend_count = 0;

bool cu_register_backend(const char *name,
                          const char *description,
                          cu_backend_t *(*factory)(void)) {
    if (!name || !factory || g_backend_count >= CU_MAX_BACKENDS) return false;
    cu_backend_entry_t *entry = (cu_backend_entry_t *)calloc(1, sizeof(cu_backend_entry_t));
    if (!entry) return false;
    entry->name = name;
    entry->description = description;
    entry->factory = factory;
    entry->next = g_backend_registry;
    g_backend_registry = entry;
    g_backend_count++;
    return true;
}

char *cu_list_backends(void) {
    json_t *arr = json_array();
    if (!arr) return strdup("[]");

    for (cu_backend_entry_t *e = g_backend_registry; e; e = e->next) {
        cu_backend_t *b = e->factory ? e->factory() : NULL;
        json_t *obj = json_object();
        json_set(obj, "name", json_string(e->name ? e->name : "?"));
        json_set(obj, "description", json_string(e->description ? e->description : ""));
        json_set(obj, "available", json_bool(b && b->is_available()));
        if (b) { free(b->state); free(b); }
        json_append(arr, obj);
    }
    char *str = json_serialize(arr);
    json_free(arr);
    return str ? str : strdup("[]");
}

void cu_clear_backends(void) {
    cu_backend_entry_t *e = g_backend_registry;
    while (e) {
        cu_backend_entry_t *next = e->next;
        free(e);
        e = next;
    }
    g_backend_registry = NULL;
    g_backend_count = 0;
}

static cu_backend_t *find_backend_by_name(const char *name) {
    if (!name) return NULL;
    for (cu_backend_entry_t *e = g_backend_registry; e; e = e->next) {
        if (e->name && strcmp(e->name, name) == 0) {
            return e->factory ? e->factory() : NULL;
        }
    }
    return NULL;
}

cu_backend_t *computer_use_new_noop_backend(void) {
    return make_noop_backend();
}

/* Auto-register built-in backends. Idempotent — safe to call multiple times. */
static void cu_auto_register(void) {
    if (g_backend_count > 0) return;  /* already registered */
    cu_register_backend("noop",    "Test/CI fallback (returns synthetic data)", make_noop_backend);
    cu_register_backend("x11",     "X11/xdotool (Linux)",                      make_x11_backend);
    cu_register_backend("wayland", "Wayland compositor (Linux)",               make_wayland_backend);
    cu_register_backend("macos",   "macOS via cua-driver MCP",                 make_macos_backend);
    cu_register_backend("windows", "Windows via Win32 API",                    make_windows_backend);
}

cu_backend_t *computer_use_select_backend(void) {
    cu_auto_register();
    /* Check CU_BACKEND env var for explicit override */
    const char *override = getenv("CU_BACKEND");
    if (override && override[0]) {
        cu_backend_t *b = find_backend_by_name(override);
        if (b) {
            if (b->is_available() && b->start()) return b;
            free(b->state); free(b);
        }
    }

    /* Try all registered backends in registration order */
    for (cu_backend_entry_t *e = g_backend_registry; e; e = e->next) {
        cu_backend_t *b = e->factory ? e->factory() : NULL;
        if (!b) continue;
        if (b->is_available() && b->start()) return b;
        free(b->state);
        free(b);
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

    /* Try requested mode first */
    cu_capture_t *cap = b->capture(mode, app);

    /* Vision fallback: if vision mode failed, fall back to som
     * (some backends don't support vision natively — e.g. x11 via xdotool) */
    bool fell_back = false;
    if (!cap && strcmp(mode, "vision") == 0) {
        cap = b->capture("som", app);
        fell_back = true;
    }

    if (!cap) return make_error_response("capture failed");
    char *resp = make_capture_response(cap, max_elements);
    cu_capture_free(cap);

    /* Notify agent of fallback */
    if (fell_back) {
        size_t rlen = resp ? strlen(resp) : 0;
        char *fb = (char *)malloc(rlen + 128);
        if (fb) {
            int n = snprintf(fb, rlen + 128,
                "%.*s,\"vision_fallback\":true,"
                "\"note\":\"Vision capture unavailable, fell back to som mode\"}",
                (int)(rlen > 2 ? rlen - 1 : 0), resp ? resp : "{}");
            if (n > 0) { free(resp); resp = fb; }
            else free(fb);
        }
    }
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
