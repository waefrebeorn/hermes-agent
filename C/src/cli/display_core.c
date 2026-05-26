/*
 * display_core.c — Terminal display for Hermes C.
 * ANSI escape codes only. No ncurses dependency.
 * Provides: display_init, display_reset, display_printf, progress, spinner, panel.
 */

#include "hermes_display.h"
#include "hermes_json.h"
#include "skin.h"
#include "ansi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>

/* ================================================================
 *  State
 * ================================================================ */

static bool is_tty = false;
static skin_t *g_display_skin = NULL;  /* active skin for display styling */

#define ANSI_ESC "\x1B["

/* ================================================================
 *  Initialization
 * ================================================================ */

void display_init(void) {
    is_tty = isatty(STDOUT_FILENO);
    /* V21: NO_COLOR env var or TERM=dumb disables color */
    if (is_tty) {
        const char *no_color = getenv("NO_COLOR");
        if (no_color) // NO_COLOR set = disable colors (per no-color.org spec)
            is_tty = 0;
        const char *term = getenv("TERM");
        if (term && strcmp(term, "dumb") == 0)
            is_tty = 0;
    }
    if (is_tty) {
        /* Enable alternate screen? No, keep it simple */
    }
}

void display_reset(void) {
    if (is_tty) {
        printf(ANSI_ESC "0m");
        fflush(stdout);
    }
}

/* ================================================================
 *  Color + Style
 * ================================================================ */

void display_set_fg(display_color_t color) {
    if (!is_tty) return;
    if (color == DISPLAY_DEFAULT)
        printf(ANSI_ESC "39m");
    else
        printf(ANSI_ESC "9%dm", (int)color);
    fflush(stdout);
}

void display_set_bg(display_color_t color) {
    if (!is_tty) return;
    if (color == DISPLAY_DEFAULT)
        printf(ANSI_ESC "49m");
    else
        printf(ANSI_ESC "10%dm", (int)color);
    fflush(stdout);
}

/* Truecolor (24-bit) support */
void display_set_fg_rgb(int r, int g, int b) {
    if (!is_tty) return;
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    printf(ANSI_ESC "38;2;%d;%d;%dm", r, g, b);
    fflush(stdout);
}

/* 256-color palette foreground */
void display_set_fg_256(int color) {
    if (!is_tty) return;
    if (color < 0) color = 0;
    if (color > 255) color = 255;
    printf(ANSI_ESC "38;5;%dm", color);
    fflush(stdout);
}

/* 256-color palette background */
void display_set_bg_256(int color) {
    if (!is_tty) return;
    if (color < 0) color = 0;
    if (color > 255) color = 255;
    printf(ANSI_ESC "48;5;%dm", color);
    fflush(stdout);
}

void display_set_bg_rgb(int r, int g, int b) {
    if (!is_tty) return;
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    printf(ANSI_ESC "48;2;%d;%d;%dm", r, g, b);
    fflush(stdout);
}

void display_set_style(display_style_t style) {
    if (!is_tty) return;
    if (style & DISPLAY_BOLD)
        printf(ANSI_ESC "1m");
    if (style & DISPLAY_DIM)
        printf(ANSI_ESC "2m");
    if (style & DISPLAY_ITALIC)
        printf(ANSI_ESC "3m");
    if (style & DISPLAY_UNDERLINE)
        printf(ANSI_ESC "4m");
    fflush(stdout);
}

/* ================================================================
 *  Printf with color
 * ================================================================ */

void display_printf(display_color_t color, display_style_t style,
                    const char *fmt, ...)
{
    if (!is_tty) {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
        fflush(stdout);
        return;
    }

    display_set_fg(color);
    display_set_style(style);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    display_reset();
}

/* Print with 24-bit truecolor foreground */
void display_printf_hex(const char *hex_fg, display_style_t style,
                         const char *fmt, ...)
{
    if (!is_tty || !hex_fg) {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
        fflush(stdout);
        return;
    }
    int r, g, b;
    if (!ansi_parse_hex(hex_fg, &r, &g, &b)) {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
        fflush(stdout);
        return;
    }
    display_set_fg_rgb(r, g, b);
    display_set_style(style);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    display_reset();
}

/* ================================================================
 *  Cursor control
 * ================================================================ */

void display_clear(void) {
    if (!is_tty) return;
    printf(ANSI_ESC "2J" ANSI_ESC "H");
    fflush(stdout);
}

void display_goto(int row, int col) {
    if (!is_tty) return;
    printf(ANSI_ESC "%d;%dH", row, col);
    fflush(stdout);
}

void display_save_pos(void) {
    if (!is_tty) return;
    printf(ANSI_ESC "s");
    fflush(stdout);
}

void display_restore_pos(void) {
    if (!is_tty) return;
    printf(ANSI_ESC "u");
    fflush(stdout);
}

/* ================================================================
 *  Progress bar
 * ================================================================ */

void display_progress_init(display_progress_t *bar, const char *label, int total) {
    if (!bar) return;
    bar->current = 0;
    bar->total = total > 0 ? total : 100;
    bar->width = 40;
    snprintf(bar->label, sizeof(bar->label), "%s", label ? label : "");
    if (is_tty) {
        display_printf(DISPLAY_WHITE, DISPLAY_DIM, "%s: ", bar->label);
        fflush(stdout);
    }
}

void display_progress_update(display_progress_t *bar, int current) {
    if (!bar || !is_tty) return;
    bar->current = current;
    int pct = (bar->total > 0) ? (current * 100 / bar->total) : 0;
    int filled = (bar->width * current) / bar->total;
    printf("\r%s: [", bar->label);
    for (int i = 0; i < bar->width; i++) {
        if (i < filled) putchar('=');
        else if (i == filled) putchar('>');
        else putchar(' ');
    }
    printf("] %d%%", pct);
    fflush(stdout);
}

void display_progress_done(display_progress_t *bar) {
    if (!bar) return;
    if (is_tty) {
        display_progress_update(bar, bar->total);
        printf("\n");
    }
    fflush(stdout);
}

/* ================================================================
 *  Spinner
 * ================================================================ */

static const char SPINNER_CHARS[] = {'|', '/', '-', '\\'};

void display_spinner_start(display_spinner_t *sp, const char *label) {
    if (!sp) return;
    sp->frame = 0;
    sp->frame_count = 0;
    sp->active = true;
    sp->face = NULL;
    if (label) {
        sp->label = strdup(label);
    } else {
        sp->label = NULL;
    }
}

void display_spinner_tick(display_spinner_t *sp) {
    if (!sp || !sp->active || !is_tty) return;
    printf("\r%c %s", SPINNER_CHARS[sp->frame % 4],
           sp->label ? sp->label : "");
    sp->frame++;
    sp->frame_count++;
    fflush(stdout);
}

void display_spinner_stop(display_spinner_t *sp, const char *done_msg) {
    if (!sp) return;
    sp->active = false;
    if (is_tty) {
        if (done_msg)
            printf("\r\xE2\x9C\x93 %s\n", done_msg);
        else
            printf("\r\xE2\x9C\x93 %s\n", sp->label ? sp->label : "done");
    }
    free(sp->label);
    sp->label = NULL;
    free(sp->face);
    sp->face = NULL;
    fflush(stdout);
}

/* ================================================================
 *  Kawaii Spinner — animated faces for LLM wait
 * ================================================================ */

/* V11: Spinner frame sets — mirrors Python KawaiiSpinner.SPINNERS */
static const char *SPINNER_DOTS_FRAMES[] = {
    "⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏",
};
static const int N_DOTS = 10;

static const char *SPINNER_BOUNCE_FRAMES[] = {
    "⠁", "⠂", "⠄", "⡀", "⢀", "⠠", "⠐", "⠈",
};
static const int N_BOUNCE = 8;

static const char *SPINNER_GROW_FRAMES[] = {
    "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█", "▇", "▆", "▅", "▄", "▃", "▂",
};
static const int N_GROW = 14;

static const char *SPINNER_ARROWS_FRAMES[] = {
    "←", "↖", "↑", "↗", "→", "↘", "↓", "↙",
};
static const int N_ARROWS = 8;

static const char *SPINNER_STAR_FRAMES[] = {
    "✶", "✷", "✸", "✹", "✺", "✹", "✸", "✷",
};
static const int N_STAR = 8;

static const char *SPINNER_MOON_FRAMES[] = {
    "🌑", "🌒", "🌓", "🌔", "🌕", "🌖", "🌗", "🌘",
};
static const int N_MOON = 8;

static const char *SPINNER_PULSE_FRAMES[] = {
    "◜", "◠", "◝", "◞", "◡", "◟",
};
static const int N_PULSE = 6;

static const char *SPINNER_BRAIN_FRAMES[] = {
    "🧠", "💭", "💡", "✨", "💫", "🌟", "💡", "💭",
};
static const int N_BRAIN = 8;

static const char *SPINNER_SPARKLE_FRAMES[] = {
    "⁺", "˚", "*", "✧", "✦", "✧", "*", "˚",
};
static const int N_SPARKLE = 8;

/* Get the current spinner frame character for the given type + index.
 * Returns the frame string, or NULL for kawaii face mode. */
static const char *spinner_get_frame(spinner_type_t type, int idx) {
    switch (type) {
        case SPINNER_DOTS:    return SPINNER_DOTS_FRAMES[idx % N_DOTS];
        case SPINNER_BOUNCE:  return SPINNER_BOUNCE_FRAMES[idx % N_BOUNCE];
        case SPINNER_GROW:    return SPINNER_GROW_FRAMES[idx % N_GROW];
        case SPINNER_ARROWS:  return SPINNER_ARROWS_FRAMES[idx % N_ARROWS];
        case SPINNER_STAR:    return SPINNER_STAR_FRAMES[idx % N_STAR];
        case SPINNER_MOON:    return SPINNER_MOON_FRAMES[idx % N_MOON];
        case SPINNER_PULSE:   return SPINNER_PULSE_FRAMES[idx % N_PULSE];
        case SPINNER_BRAIN:   return SPINNER_BRAIN_FRAMES[idx % N_BRAIN];
        case SPINNER_SPARKLE: return SPINNER_SPARKLE_FRAMES[idx % N_SPARKLE];
        default:              return NULL; /* kawaii face mode */
    }
}

/* Parse a spinner style name (from display.spinner_style config) to type enum. */
spinner_type_t display_parse_spinner_type(const char *style) {
    if (!style || !style[0]) return SPINNER_KAWAII;
    if (strcmp(style, "dots") == 0)    return SPINNER_DOTS;
    if (strcmp(style, "bounce") == 0)  return SPINNER_BOUNCE;
    if (strcmp(style, "grow") == 0)    return SPINNER_GROW;
    if (strcmp(style, "arrows") == 0)  return SPINNER_ARROWS;
    if (strcmp(style, "star") == 0)    return SPINNER_STAR;
    if (strcmp(style, "moon") == 0)    return SPINNER_MOON;
    if (strcmp(style, "pulse") == 0)   return SPINNER_PULSE;
    if (strcmp(style, "brain") == 0)   return SPINNER_BRAIN;
    if (strcmp(style, "sparkle") == 0) return SPINNER_SPARKLE;
    if (strcmp(style, "kawaii") == 0)  return SPINNER_KAWAII;
    /* Also match classic/default for compatibility */
    if (strcmp(style, "classic") == 0 || strcmp(style, "default") == 0)
        return SPINNER_KAWAII;
    return SPINNER_KAWAII; /* fallback */
}

static const char *KAWAII_WAITING[] = {
    "(｡◕‿◕｡)", "(◕‿◕✿)", "٩(◕‿◕｡)۶", "(✿◠‿◠)", "( ˘▽˘)っ",
    "♪(´ε` )", "(◕ᴗ◕✿)", "ヾ(＾∇＾)", "(≧◡≦)", "(★ω★)",
};
static const int N_WAITING = 10;

static const char *KAWAII_THINKING[] = {
    "(｡•́︿•̀｡)", "(◔_◔)", "(¬‿¬)", "( •_•)>⌐■-■", "(⌐■_■)",
    "(´･_･`)", "◉_◉", "(°ロ°)", "( ˘⌣˘)♡", "ヽ(>∀<☆)☆",
    "٩(๑❛ᴗ❛๑)۶", "(⊙_⊙)", "(¬_¬)", "( ͡° ͜ʖ ͡°)", "ಠ_ಠ",
};
static const int N_THINKING = 15;

/* Thinking verbs — fallback when skin doesn't define spinner.thinking_verbs */
static const char *THINKING_VERBS[] = {
    "pondering", "contemplating", "musing", "cogitating", "ruminating",
    "deliberating", "mulling", "reflecting", "processing", "reasoning",
    "analyzing", "computing", "synthesizing", "formulating", "brainstorming",
};
static const int N_VERBS = 15;

/* Set the skin pointer for display styling. Used by KawaiiSpinner to
 * read spinner config (faces, verbs, wings) from the active skin. */
void display_set_skin(void *skin) {
    g_display_skin = (skin_t *)skin;
}

/* Load wings from skin spinner config. Returns true if wings set. */
static bool load_skin_wings(char *left, size_t left_sz, char *right, size_t right_sz) {
    if (!g_display_skin) return false;
    const json_t *wings_arr = (const json_t *)skin_get_json(g_display_skin, "spinner.wings");
    if (!wings_arr || wings_arr->type != JSON_ARRAY || json_len(wings_arr) == 0)
        return false;
    /* Get first wing pair */
    const json_t *pair = json_get(wings_arr, 0);
    if (!pair || pair->type != JSON_ARRAY || json_len(pair) < 2)
        return false;
    const json_t *l = json_get(pair, 0);
    const json_t *r = json_get(pair, 1);
    if (l && l->type == JSON_STRING) snprintf(left, left_sz, "%s", l->str_val);
    if (r && r->type == JSON_STRING) snprintf(right, right_sz, "%s", r->str_val);
    return left[0] != '\0' || right[0] != '\0';
}

void display_kawaii_start(display_kawaii_t *k, const char *label, bool thinking) {
    if (!k) return;
    k->frame = 0;
    k->active = true;
    k->thinking = thinking;
    k->face[0] = '\0';
    k->verb[0] = '\0';
    k->wing_left[0] = '\0';
    k->wing_right[0] = '\0';
    k->label = label ? strdup(label) : NULL;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    k->start_time = (double)tv.tv_sec + (double)tv.tv_usec / 1e6;
    /* Load wings from skin */
    load_skin_wings(k->wing_left, sizeof(k->wing_left),
                    k->wing_right, sizeof(k->wing_right));
    /* Print initial state */
    const char *frame = spinner_get_frame(k->type, 0);
    if (frame) {
        /* V11: Frame-based spinner */
        if (k->wing_left[0])
            printf("\r%s%s  %s  %s", k->wing_left, frame, k->label ? k->label : "", k->wing_right);
        else if (thinking && k->verb[0])
            printf("\r%s  %s  (%s)", frame, k->label ? k->label : "", k->verb);
        else
            printf("\r%s  %s", frame, k->label ? k->label : "");
    } else {
        if (thinking) {
            snprintf(k->face, sizeof(k->face), "%s", KAWAII_THINKING[0]);
            snprintf(k->verb, sizeof(k->verb), "%s", THINKING_VERBS[0]);
        } else {
            snprintf(k->face, sizeof(k->face), "%s", KAWAII_WAITING[0]);
        }
        if (k->wing_left[0])
            printf("\r%s%s  %s  %s", k->wing_left, k->face, k->label ? k->label : "", k->wing_right);
        else if (thinking && k->verb[0])
            printf("\r%s  %s  (%s)", k->face, k->label ? k->label : "", k->verb);
        else
            printf("\r%s  %s", k->face, k->label ? k->label : "");
    }
    fflush(stdout);
}

void display_kawaii_tick(display_kawaii_t *k) {
    if (!k || !k->active || !is_tty) return;
    k->frame++;
    int idx = k->frame;
    const char *frame = spinner_get_frame(k->type, idx);
    if (frame) {
        /* V11: Frame-based spinner */
        if (k->wing_left[0])
            printf("\r%s%s  %s  %s", k->wing_left, frame, k->label ? k->label : "", k->wing_right);
        else if (k->thinking && k->verb[0])
            printf("\r%s  %s  (%s)", frame, k->label ? k->label : "", k->verb);
        else
            printf("\r%s  %s", frame, k->label ? k->label : "");
    } else {
        const char *face;
        if (k->thinking) {
            face = KAWAII_THINKING[idx % N_THINKING];
            const char *verb = THINKING_VERBS[idx % N_VERBS];
            snprintf(k->verb, sizeof(k->verb), "%s", verb);
        } else {
            face = KAWAII_WAITING[idx % N_WAITING];
        }
        snprintf(k->face, sizeof(k->face), "%s", face);
        if (k->wing_left[0])
            printf("\r%s%s  %s  %s", k->wing_left, face, k->label ? k->label : "", k->wing_right);
        else if (k->thinking && k->verb[0])
            printf("\r%s  %s  (%s)", face, k->label ? k->label : "", k->verb);
        else
            printf("\r%s  %s", face, k->label ? k->label : "");
    }
    fflush(stdout);
}

void display_kawaii_stop(display_kawaii_t *k, const char *done_msg) {
    if (!k) return;
    k->active = false;
    int idx = k->frame;
    const char *frame = spinner_get_frame(k->type, idx);
    const char *display_char;
    if (frame) {
        display_char = frame;
    } else if (k->thinking) {
        display_char = KAWAII_THINKING[idx % N_THINKING];
    } else {
        display_char = KAWAII_WAITING[idx % N_WAITING];
    }
    if (is_tty) {
        if (k->wing_left[0])
            printf("\r%s%s  ", k->wing_left, display_char);
        else
            printf("\r%s  ", display_char);
        if (done_msg) {
            display_printf(DISPLAY_GREEN, DISPLAY_NORMAL, "✓ ");
            printf("%s\n", done_msg);
        } else {
            display_printf(DISPLAY_GREEN, DISPLAY_NORMAL, "✓ ");
            printf("%s\n", k->label ? k->label : "done");
        }
    }
    free(k->label);
    k->label = NULL;
    fflush(stdout);
}

/* ================================================================
 *  Tool preview builder — extract primary arg from tool call JSON
 * ================================================================ */

char *display_tool_preview(const char *tool_name, const char *args_json) {
    if (!tool_name || !args_json) return NULL;

    /* Parse JSON args */
    json_t *args = json_parse(args_json, NULL);
    if (!args || args->type != JSON_OBJECT) {
        json_free(args);
        return NULL;
    }

    const char *preview = NULL;
    char buf[512];

    /* Primary arg key per tool */
    if (strcmp(tool_name, "terminal") == 0)
        preview = json_get_str(args, "command", NULL);
    else if (strcmp(tool_name, "web_search") == 0)
        preview = json_get_str(args, "query", NULL);
    else if (strcmp(tool_name, "read_file") == 0 || strcmp(tool_name, "write_file") == 0
             || strcmp(tool_name, "patch") == 0)
        preview = json_get_str(args, "path", NULL);
    else if (strcmp(tool_name, "search_files") == 0)
        preview = json_get_str(args, "pattern", NULL);
    else if (strcmp(tool_name, "execute_code") == 0)
        preview = json_get_str(args, "code", NULL);
    else if (strcmp(tool_name, "delegate_task") == 0)
        preview = json_get_str(args, "goal", NULL);
    else if (strcmp(tool_name, "vision_analyze") == 0)
        preview = json_get_str(args, "question", NULL);
    else if (strcmp(tool_name, "image_generate") == 0 || strcmp(tool_name, "text_to_speech") == 0)
        preview = json_get_str(args, "prompt", NULL);
    else if (strcmp(tool_name, "cronjob") == 0)
        preview = json_get_str(args, "action", NULL);
    else if (strcmp(tool_name, "memory") == 0)
        preview = json_get_str(args, "action", NULL);
    else if (strcmp(tool_name, "session_search") == 0)
        preview = json_get_str(args, "query", NULL);
    else if (strcmp(tool_name, "skill_view") == 0)
        preview = json_get_str(args, "name", NULL);
    else if (strcmp(tool_name, "send_message") == 0) {
        preview = json_get_str(args, "target", NULL);
        if (preview) {
            snprintf(buf, sizeof(buf), "→ %s", preview);
            preview = buf;
        }
    } else if (strcmp(tool_name, "todo") == 0) {
        json_t *todos = json_obj_get(args, "todos");
        if (todos && todos->type == JSON_ARRAY) {
            snprintf(buf, sizeof(buf), "%zu tasks", json_len(todos));
        } else {
            preview = json_get_str(args, "merge", NULL);
            if (preview && strcmp(preview, "true") == 0)
                snprintf(buf, sizeof(buf), "updating tasks");
            else
                snprintf(buf, sizeof(buf), "planning");
        }
        preview = buf;
    } else if (strcmp(tool_name, "process") == 0) {
        preview = json_get_str(args, "action", NULL);
    } else if (strcmp(tool_name, "clarify") == 0) {
        preview = json_get_str(args, "question", NULL);
    } else if (strcmp(tool_name, "skill_manage") == 0) {
        preview = json_get_str(args, "name", NULL);
        if (!preview) preview = json_get_str(args, "action", NULL);
    } else if (strcmp(tool_name, "browser_navigate") == 0) {
        preview = json_get_str(args, "url", NULL);
    } else if (strcmp(tool_name, "browser_click") == 0) {
        preview = json_get_str(args, "ref", NULL);
    } else if (strcmp(tool_name, "browser_type") == 0) {
        preview = json_get_str(args, "text", NULL);
    }

    if (!preview) {
        /* Fallback: try common keys */
        const char *fallbacks[] = {"query", "text", "command", "path", "name", "prompt", "code", "goal", "message", "content", "action", NULL};
        for (int i = 0; fallbacks[i]; i++) {
            preview = json_get_str(args, fallbacks[i], NULL);
            if (preview) break;
        }
    }

    char *result = NULL;
    if (preview) {
        /* Collapse whitespace, truncate */
        char cleaned[128];
        int j = 0;
        int last_space = 0;
        for (int i = 0; preview[i] && j < (int)sizeof(cleaned) - 4; i++) {
            if (preview[i] == ' ' || preview[i] == '\t' || preview[i] == '\n') {
                if (!last_space) { cleaned[j++] = ' '; last_space = 1; }
            } else {
                cleaned[j++] = preview[i];
                last_space = 0;
            }
        }
        /* Truncate with ... */
        if (j > 60) {
            j = 57;
            cleaned[j++] = '.';
            cleaned[j++] = '.';
            cleaned[j++] = '.';
        }
        cleaned[j] = '\0';
        result = strdup(cleaned);
    }

    json_free(args);
    return result;
}

/* ================================================================
 *  Tool activity line — ┊ prefix + emoji + tool_name + preview
 * ================================================================ */

void display_tool_activity(const char *tool_name, const char *preview,
                           display_color_t color) {
    if (!tool_name) return;

    /* Emoji map — check skin tool_emojis first, fall back to hardcoded */
    const char *emoji = NULL;
    if (g_display_skin) {
        char key[128];
        snprintf(key, sizeof(key), "tool_emojis.%s", tool_name);
        emoji = skin_get(g_display_skin, key, NULL);
    }
    if (!emoji) {
        /* Hardcoded defaults per tool */
        if (strcmp(tool_name, "terminal") == 0) emoji = "$ ";
        else if (strcmp(tool_name, "write_file") == 0) emoji = "📝";
        else if (strcmp(tool_name, "read_file") == 0) emoji = "📖";
        else if (strcmp(tool_name, "patch") == 0) emoji = "🩹";
        else if (strcmp(tool_name, "web_search") == 0) emoji = "🔍";
        else if (strcmp(tool_name, "search_files") == 0) emoji = "🔎";
        else if (strcmp(tool_name, "execute_code") == 0) emoji = "🐍";
        else if (strcmp(tool_name, "delegate_task") == 0) emoji = "📋";
        else if (strcmp(tool_name, "vision_analyze") == 0) emoji = "👁️";
        else if (strcmp(tool_name, "image_generate") == 0) emoji = "🎨";
        else if (strcmp(tool_name, "text_to_speech") == 0) emoji = "🔊";
        else if (strcmp(tool_name, "send_message") == 0) emoji = "📤";
        else if (strcmp(tool_name, "memory") == 0) emoji = "🧠";
        else if (strcmp(tool_name, "session_search") == 0) emoji = "📚";
        else if (strcmp(tool_name, "skill_view") == 0 || strcmp(tool_name, "skill_manage") == 0) emoji = "🛠️";
        else if (strcmp(tool_name, "cronjob") == 0) emoji = "⏰";
        else if (strcmp(tool_name, "todo") == 0) emoji = "✅";
        else if (strcmp(tool_name, "clarify") == 0) emoji = "❓";
        else if (strcmp(tool_name, "browser_navigate") == 0 || strcmp(tool_name, "browser_click") == 0
                 || strcmp(tool_name, "browser_type") == 0) emoji = "🌐";
        else emoji = "⚡";
    }

    printf("  %s ", emoji);
    display_printf(color, DISPLAY_BOLD, "%s", tool_name);
    if (preview) {
        printf(" ");
        display_printf(DISPLAY_DEFAULT, DISPLAY_DIM, "%s", preview);
    }
    printf("\n");
    fflush(stdout);
}

/* ================================================================
 *  Inline diff rendering — colored unified diff
 * ================================================================ */

char *display_inline_diff(const char *diff_text) {
    if (!diff_text) return NULL;

    /* Calculate output buffer size (diff_text can be up to ~50KB) */
    size_t in_len = strlen(diff_text);
    size_t cap = in_len * 3 + 4096; /* ANSI expansion */
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    size_t pos = 0;

    const char *p = diff_text;
    const char *nl;
    while ((nl = strchr(p, '\n')) != NULL) {
        size_t line_len = (size_t)(nl - p);
        char line[4096];
        size_t lcpy = line_len < sizeof(line) - 1 ? line_len : sizeof(line) - 1;
        memcpy(line, p, lcpy);
        line[lcpy] = '\0';

        if (line_len > 0 && line[0] == '+') {
            pos += snprintf(out + pos, cap - pos, "\033[38;2;255;255;255;48;2;20;90;20m%s\033[0m\n", line);
        } else if (line_len > 0 && line[0] == '-') {
            pos += snprintf(out + pos, cap - pos, "\033[38;2;255;255;255;48;2;120;20;20m%s\033[0m\n", line);
        } else if (line_len > 1 && line[0] == '@' && line[1] == '@') {
            pos += snprintf(out + pos, cap - pos, "\033[38;2;120;120;140m%s\033[0m\n", line);
        } else if (line_len > 3 && strncmp(line, "+++ ", 4) == 0) {
            pos += snprintf(out + pos, cap - pos, "\033[38;2;180;160;255m%s\033[0m\n", line);
        } else if (line_len > 3 && strncmp(line, "--- ", 4) == 0) {
            pos += snprintf(out + pos, cap - pos, "\033[38;2;180;160;255m%s\033[0m\n", line);
        } else if (line_len > 0 && line[0] == ' ') {
            pos += snprintf(out + pos, cap - pos, "\033[38;2;150;150;150m%s\033[0m\n", line);
        } else {
            pos += snprintf(out + pos, cap - pos, "%s\n", line);
        }

        if (pos >= cap - 256) break; /* safety */
        p = nl + 1;
    }

    /* Handle last line without trailing newline */
    if (*p) {
        pos += snprintf(out + pos, cap - pos, "\033[38;2;150;150;150m%s\033[0m\n", p);
    }

    if (pos == 0) { free(out); return NULL; }
    return out;
}

/* ================================================================
 *  Word Wrapping
 * ================================================================ */

/* Word-wrap text to max_width columns. Preserves existing newlines (paragraphs).
 * Returns malloc'd string (caller free). Tabs treated as single space.
 * ANSI escape sequences are NOT stripped — caller should wrap plain text. */
char *display_word_wrap(const char *text, int max_width) {
    if (!text || max_width < 1) return strdup(text ? text : "");
    size_t in_len = strlen(text);
    /* Upper bound: each char could need \n before it */
    size_t cap = in_len + (in_len / (max_width > 1 ? max_width : 1)) + 1;
    char *out = (char *)malloc(cap);
    if (!out) return strdup(text);
    size_t pos = 0;
    int col = 0;        /* current display column */
    int word_start = -1; /* buffer position of current word start (-1 = in whitespace) */
    int word_width = 0;

    for (size_t i = 0; i < in_len && pos < cap - 1; i++) {
        char ch = text[i];
        if (ch == '\n') {
            /* Flush current word if any */
            if (word_start >= 0) {
                size_t wlen = i - (size_t)word_start;
                if (pos + wlen + 1 >= cap) break;
                memcpy(out + pos, text + word_start, wlen);
                pos += wlen;
                col += word_width;
                word_start = -1;
                word_width = 0;
            }
            out[pos++] = '\n';
            col = 0;
            continue;
        }
        if (ch == ' ' || ch == '\t') {
            if (word_start >= 0) {
                /* Check if word fits on current line */
                if (col + word_width > max_width) {
                    /* Insert newline before the word */
                    out[pos++] = '\n';
                    col = 0;
                }
                size_t wlen = i - (size_t)word_start;
                if (pos + wlen + 1 >= cap) break;
                memcpy(out + pos, text + word_start, wlen);
                pos += wlen;
                col += word_width;
                word_start = -1;
                word_width = 0;
            }
            /* Emit the space (only if not at column 0) */
            if (col > 0 && pos < cap - 1) {
                out[pos++] = ' ';
                col++;
            }
            continue;
        }
        /* Regular character: part of a word */
        if (word_start < 0) word_start = (int)i;
        /* Increment display width for this char (skip UTF-8 continuation bytes) */
        if (((unsigned char)ch & 0xC0) != 0x80) word_width++;
    }
    /* Flush last word */
    if (word_start >= 0) {
        size_t wlen = in_len - (size_t)word_start;
        if (col + word_width > max_width && col > 0) {
            out[pos++] = '\n';
            col = 0;
        }
        if (pos + wlen + 1 >= cap) {
            if (pos < cap) out[pos] = '\0';
        } else {
            memcpy(out + pos, text + word_start, wlen);
            pos += wlen;
        }
    }
    out[pos] = '\0';
    return out;
}

/* Panel with TrueColor hex border (wraps display_panel with hex→RGB) */
void display_panel_hex(const char *title, const char *content, const char *border_hex) {
    if (!content || !border_hex) return;
    int r, g, b;
    if (!ansi_parse_hex(border_hex, &r, &g, &b)) {
        display_panel(title, content, DISPLAY_WHITE);
        return;
    }
    int term_width = display_width();

    /* Calculate inner width */
    int inner = term_width > 80 ? 80 : (term_width < 40 ? 40 : term_width);
    inner = (inner * 8) / 10;

    /* Top border with title */
    display_set_fg_rgb(r, g, b);
    display_set_style(DISPLAY_BOLD);
    if (title && title[0]) {
        printf("\n\xE2\x94\x8C %s ", title);
        int remaining = inner - (int)strlen(title) - 3;
        for (int i = 0; i < remaining && i < 60; i++)
            printf("\xE2\x94\x80");
        printf("\xE2\x94\x90\n");
    } else {
        printf("\n\xE2\x94\x8C");
        for (int i = 0; i < inner; i++)
            printf("\xE2\x94\x80");
        printf("\xE2\x94\x90\n");
    }
    display_reset();

    /* Content */
    printf("%s\n", content);

    /* Bottom border */
    display_set_fg_rgb(r, g, b);
    display_set_style(DISPLAY_BOLD);
    printf("\xE2\x94\x94");
    for (int i = 0; i < inner; i++)
        printf("\xE2\x94\x80");
    printf("\xE2\x94\x98\n");
    display_reset();
    fflush(stdout);
}

/* Horizontal rule with TrueColor hex */
void display_hr_hex(const char *hex_fg) {
    if (!hex_fg) { display_hr(DISPLAY_WHITE); return; }
    int r, g, b;
    if (!ansi_parse_hex(hex_fg, &r, &g, &b)) { display_hr(DISPLAY_WHITE); return; }
    int w = display_width();
    if (w > 78) w = 78;
    display_set_fg_rgb(r, g, b);
    for (int i = 0; i < w; i++)
        printf("\xE2\x94\x80");
    printf("\n");
    display_reset();
    fflush(stdout);
}

/* ================================================================
 *  Status Bar
 * ================================================================ */

/* Get a hex color from the active skin or fallback, parsed into RGB.
 * Returns true if color was available. */
static bool skin_color_rgb(const char *key, const char *fallback_hex,
                           int *r, int *g, int *b) {
    if (!g_display_skin) return ansi_parse_hex(fallback_hex, r, g, b);
    const char *val = skin_get(g_display_skin, key, NULL);
    if (!val) val = fallback_hex;
    return ansi_parse_hex(val, r, g, b);
}

/* Display a status bar line */
void display_statusbar(const char *model, const char *session_id,
                        int turn_count, int context_pct) {
    if (!is_tty) return;

    /* Get skin colors */
    int bg_r, bg_g, bg_b, fg_r, fg_g, fg_b, dim_r, dim_g, dim_b;
    int good_r, good_g, good_b, warn_r, warn_g, warn_b;
    int bad_r, bad_g, bad_b, crit_r, crit_g, crit_b;

    if (!skin_color_rgb("colors.status_bar_bg", "#1a1a2e", &bg_r, &bg_g, &bg_b))
        { bg_r=26; bg_g=26; bg_b=46; }
    if (!skin_color_rgb("colors.status_bar_strong", "#FFD700", &fg_r, &fg_g, &fg_b))
        { fg_r=255; fg_g=215; fg_b=0; }
    skin_color_rgb("colors.status_bar_dim", "#8B8682", &dim_r, &dim_g, &dim_b);
    skin_color_rgb("colors.status_bar_good", "#8FBC8F", &good_r, &good_g, &good_b);
    skin_color_rgb("colors.status_bar_warn", "#FFD700", &warn_r, &warn_g, &warn_b);
    skin_color_rgb("colors.status_bar_bad", "#FF8C00", &bad_r, &bad_g, &bad_b);
    skin_color_rgb("colors.status_bar_critical", "#FF6B6B", &crit_r, &crit_g, &crit_b);

    /* Determine context color based on percentage */
    int ctx_r, ctx_g, ctx_b;
    if (context_pct < 50) { ctx_r=good_r; ctx_g=good_g; ctx_b=good_b; }
    else if (context_pct < 75) { ctx_r=warn_r; ctx_g=warn_g; ctx_b=warn_b; }
    else if (context_pct < 90) { ctx_r=bad_r; ctx_g=bad_g; ctx_b=bad_b; }
    else { ctx_r=crit_r; ctx_g=crit_g; ctx_b=crit_b; }

    /* Get terminal width */
    int w = display_width();
    if (w > 100) w = 100;
    if (w < 40) { w = 40; } /* Minimum */

    /* Build status bar segments: [model] | [session] | [context%] | [turns] */
    char left[256], right[256];
    snprintf(left, sizeof(left), " %s ", model ? model : "default");
    snprintf(right, sizeof(right), " ctx:%d%% t:%d %s",
             context_pct, turn_count, session_id ? session_id : "");

    /* Truncate if too long */
    int max_left = w / 2 - 4;
    if ((int)strlen(left) > max_left) left[max_left] = '\0';
    if ((int)strlen(right) > w - (int)strlen(left) - 4)
        right[w - (int)strlen(left) - 4] = '\0';

    /* Paint background */
    printf("\n\x1B[48;2;%d;%d;%dm", bg_r, bg_g, bg_b);

    /* Left: model in fg color */
    printf("\x1B[38;2;%d;%d;%dm%s", fg_r, fg_g, fg_b, left);

    /* Right: aligned, session in dim, context in context color */
    int pad = w - (int)strlen(left) - (int)strlen(right);
    if (pad < 1) pad = 1;
    for (int i = 0; i < pad; i++) putchar(' ');

    /* Session/turn in dim */
    printf("\x1B[38;2;%d;%d;%dm", dim_r, dim_g, dim_b);
    for (const char *p = right; *p; p++) {
        if (*p == '%') {
            /* Context percentage in context color */
            printf("\x1B[38;2;%d;%d;%dm%%\x1B[38;2;%d;%d;%dm",
                   ctx_r, ctx_g, ctx_b, dim_r, dim_g, dim_b);
        } else {
            putchar(*p);
        }
    }

    /* Reset */
    printf("\x1B[0m\n");
    fflush(stdout);
}

/* ================================================================
 *  Panel / Box
 * ================================================================ */

void display_panel(const char *title, const char *content, display_color_t color) {
    if (!content) return;
    int term_width = display_width();

    /* Calculate inner width (80% of terminal, min 40, max 80) */
    int inner = term_width > 80 ? 80 : (term_width < 40 ? 40 : term_width);
    inner = (inner * 8) / 10;

    /* Top border */
    if (title && title[0]) {
        display_printf(color, DISPLAY_BOLD, "\n\xE2\x94\x8C");
        printf(" %s ", title);
        int remaining = inner - (int)strlen(title) - 3;
        for (int i = 0; i < remaining && i < 60; i++)
            printf("\xE2\x94\x80");
        printf("\xE2\x94\x90\n");
    } else {
        display_printf(color, DISPLAY_BOLD, "\n\xE2\x94\x8C");
        for (int i = 0; i < inner; i++)
            printf("\xE2\x94\x80");
        printf("\xE2\x94\x90\n");
    }

    /* Content with word-wrap */
    int wrap_width = inner - 2; /* leave 1-char padding on each side */
    if (wrap_width < 20) wrap_width = 20;
    char *wrapped = display_word_wrap(content, wrap_width);
    if (wrapped) {
        /* Print each wrapped line with left-padding for inner box */
        const char *line_start = wrapped;
        const char *nl;
        while ((nl = strchr(line_start, '\n')) != NULL) {
            size_t line_len = (size_t)(nl - line_start);
            printf(" ");
            if (line_len > 0) {
                printf("%.*s", (int)line_len, line_start);
            }
            printf("\n");
            line_start = nl + 1;
        }
        /* Last line (no trailing newline) */
        if (*line_start) {
            printf(" %s\n", line_start);
        }
        free(wrapped);
    } else {
        display_printf(color, DISPLAY_NORMAL, "%s", content);
        if (content[strlen(content) - 1] != '\n')
            printf("\n");
    }

    /* Bottom border */
    display_printf(color, DISPLAY_BOLD, "\xE2\x94\x94");
    for (int i = 0; i < inner; i++)
        printf("\xE2\x94\x80");
    display_printf(color, DISPLAY_BOLD, "\xE2\x94\x98\n\n");
}

void display_hr(display_color_t color) {
    int w = display_width();
    if (w > 80) w = 80;
    display_printf(color, DISPLAY_DIM, "");
    for (int i = 0; i < w; i++)
        printf("\xE2\x94\x80");
    printf("\n");
    fflush(stdout);
}

/* ================================================================
 *  ASCII Table
 * ================================================================ */

void display_table(int columns, const char **headers,
                   const char **rows, int num_rows,
                   display_color_t color) {
    if (columns < 1) return;

    /* Calculate max column widths */
    int *widths = calloc(columns, sizeof(int));
    if (!widths) return;

    /* Measure headers */
    for (int c = 0; c < columns && headers; c++) {
        int len = (int)strlen(headers[c] ? headers[c] : "");
        if (len > widths[c]) widths[c] = len;
    }

    /* Measure rows — split by tab */
    for (int r = 0; r < num_rows; r++) {
        const char *p = rows[r];
        for (int c = 0; c < columns; c++) {
            const char *next = strchr(p, '\t');
            int len = next ? (int)(next - p) : (int)strlen(p);
            if (len > widths[c]) widths[c] = len;
            p = next ? next + 1 : p + strlen(p);
        }
    }

    /* Clamp to terminal width */
    int term_w = display_width();
    int total = 1; /* left border */
    for (int c = 0; c < columns; c++) {
        if (widths[c] > 40) widths[c] = 40;
        total += widths[c] + 3; /* padding + border */
        if (total > term_w - 4) {
            /* Shrink proportionally — simple approach: clamp last */
            widths[c] = term_w - total + 40 - 3;
            if (widths[c] < 5) widths[c] = 5;
            break;
        }
    }

    /* Top border */
    display_printf(color, DISPLAY_BOLD, "\xE2\x94\x8C");
    for (int c = 0; c < columns; c++) {
        for (int i = 0; i < widths[c] + 2; i++)
            printf("\xE2\x94\x80");
        if (c < columns - 1)
            printf("\xE2\x94\xAC");
    }
    display_printf(color, DISPLAY_BOLD, "\xE2\x94\x90\n");

    /* Header row */
    if (headers) {
        display_printf(color, DISPLAY_BOLD, "\xE2\x94\x82");
        for (int c = 0; c < columns; c++) {
            const char *h = headers[c] ? headers[c] : "";
            int hlen = (int)strlen(h);
            printf(" %s", h);
            for (int p = hlen; p < widths[c]; p++) printf(" ");
            printf(" \xE2\x94\x82");
        }
        printf("\n");

        /* Header separator */
        display_printf(color, DISPLAY_BOLD, "\xE2\x94\x9C");
        for (int c = 0; c < columns; c++) {
            for (int i = 0; i < widths[c] + 2; i++)
                printf("\xE2\x94\x80");
            if (c < columns - 1)
                printf("\xE2\x94\xBC");
        }
        display_printf(color, DISPLAY_BOLD, "\xE2\x94\xA4\n");
    }

    /* Data rows */
    for (int r = 0; r < num_rows; r++) {
        display_printf(color, DISPLAY_NORMAL, "\xE2\x94\x82");
        const char *p = rows[r];
        for (int c = 0; c < columns; c++) {
            const char *next = strchr(p, '\t');
            int len = next ? (int)(next - p) : (int)strlen(p);
            printf(" ");
            if (len > 0) printf("%.*s", len, p);
            for (int pad = len; pad < widths[c]; pad++) printf(" ");
            printf(" \xE2\x94\x82");
            p = next ? next + 1 : p + len;
        }
        printf("\n");
    }

    /* Bottom border */
    display_printf(color, DISPLAY_BOLD, "\xE2\x94\x94");
    for (int c = 0; c < columns; c++) {
        for (int i = 0; i < widths[c] + 2; i++)
            printf("\xE2\x94\x80");
        if (c < columns - 1)
            printf("\xE2\x94\xB4");
    }
    display_printf(color, DISPLAY_BOLD, "\xE2\x94\x98\n");

    free(widths);
}

/* ================================================================
 *  Utility
 * ================================================================ */

bool display_has_color(void) {
    return is_tty;
}

int display_width(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        return (int)ws.ws_col;
    return 80;
}
