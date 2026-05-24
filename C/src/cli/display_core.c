/*
 * display_core.c — Terminal display for Hermes C.
 * ANSI escape codes only. No ncurses dependency.
 * Provides: display_init, display_reset, display_printf, progress, spinner, panel.
 */

#include "hermes_display.h"
#include "hermes_json.h"
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

#define ANSI_ESC "\x1B["

/* ================================================================
 *  Initialization
 * ================================================================ */

void display_init(void) {
    is_tty = isatty(STDOUT_FILENO);
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

void display_kawaii_start(display_kawaii_t *k, const char *label, bool thinking) {
    if (!k) return;
    k->frame = 0;
    k->active = true;
    k->thinking = thinking;
    k->face[0] = '\0';
    k->label = label ? strdup(label) : NULL;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    k->start_time = (double)tv.tv_sec + (double)tv.tv_usec / 1e6;
    /* Print initial state */
    if (thinking)
        snprintf(k->face, sizeof(k->face), "%s", KAWAII_THINKING[0]);
    else
        snprintf(k->face, sizeof(k->face), "%s", KAWAII_WAITING[0]);
    printf("\r%s  %s", k->face, k->label ? k->label : "");
    fflush(stdout);
}

void display_kawaii_tick(display_kawaii_t *k) {
    if (!k || !k->active || !is_tty) return;
    k->frame++;
    int idx = k->frame;
    const char *face;
    if (k->thinking) {
        face = KAWAII_THINKING[idx % N_THINKING];
    } else {
        face = KAWAII_WAITING[idx % N_WAITING];
    }
    snprintf(k->face, sizeof(k->face), "%s", face);
    printf("\r%s  %s", face, k->label ? k->label : "");
    fflush(stdout);
}

void display_kawaii_stop(display_kawaii_t *k, const char *done_msg) {
    if (!k) return;
    k->active = false;
    int idx = k->frame;
    const char *face;
    if (k->thinking)
        face = KAWAII_THINKING[idx % N_THINKING];
    else
        face = KAWAII_WAITING[idx % N_WAITING];
    if (is_tty) {
        printf("\r%s  ", face);
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

    /* Emoji map */
    const char *emoji = "⚡"; /* default */
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

    /* Content */
    display_printf(color, DISPLAY_NORMAL, "%s", content);
    if (content[strlen(content) - 1] != '\n')
        printf("\n");

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
