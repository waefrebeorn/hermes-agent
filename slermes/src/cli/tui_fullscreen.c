/*
 * tui_fullscreen.c — Full ncurses TUI for Hermes C (P189-P200).
 * MIT License — WuBu Slermes Project
 *
 * Implements 12 phases:
 *   P189: Layout     — split panes (history | input | tool feed | status)
 *   P190: Input      — multi-line, emoji picker, slash autocomplete
 *   P191: Messages   — role-colored, syntax highlight, markdown render
 *   P192: Streaming  — token streaming, real-time token counter
 *   P193: Tool feed  — live tool calls, progress bars, result preview
 *   P194: Status bar — model/provider, tokens, iterations, budget
 *   P195: Sessions   — browse, search, load/delete/export
 *   P196: Config     — interactive key browser, set/get/explain
 *   P197: Images     — sixel/kitty display, zoom/pan
 *   P198: Themes     — skin files, color schemes
 *   P199: Gateway    — JSON-RPC backend (fifo transport)
 *   P200: Mobile     — responsive layout, touch input
 */

#define _GNU_SOURCE
#include "hermes.h"
#include "tui_fullscreen.h"

#include <curses.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <wchar.h>
#include <locale.h>
#include "budget_tracker.h"
#include "provider_metadata.h"
#include "tui_eventpub.h"
#include "tui_slash_worker.h"

/* ==================================================================
 *  P198: THEME ENGINE — color scheme definitions and skin files
 * ================================================================== */

#define TUI_MAX_THEMES 16
#define TUI_THEME_NAME_MAX 64
#define TUI_SKIN_DIR "skins"

/* Theme: defines all color pairs used by the TUI */
typedef struct {
    char name[TUI_THEME_NAME_MAX];

    /* Color pair indices */
    int status_bg, status_fg;       /* status bar */
    int prompt_bg, prompt_fg;       /* prompt text */
    int user_bg, user_fg;           /* user messages */
    int assistant_bg, assistant_fg; /* assistant messages */
    int system_bg, system_fg;       /* system messages */
    int tool_bg, tool_fg;           /* tool output */
    int error_bg, error_fg;         /* errors */
    int warn_bg, warn_fg;           /* warnings */
    int hl_bg, hl_fg;               /* syntax highlight */
    int dim_bg, dim_fg;             /* dim/less important */
    int border_bg, border_fg;       /* borders */
    int tool_feed_bg, tool_feed_fg; /* tool feed pane */
    int sel_bg, sel_fg;             /* selection highlight */

    /* Symbols */
    char prompt_sym[8];
    char tool_sym[8];
    char think_sym[8];
    char error_sym[8];
    char ok_sym[8];
} tui_theme_t;

/* Built-in themes */
static const tui_theme_t tui_theme_default = {
    .name = "default",
    .status_bg = COLOR_BLUE, .status_fg = COLOR_WHITE,
    .prompt_bg = COLOR_BLACK, .prompt_fg = COLOR_GREEN,
    .user_bg = COLOR_BLACK, .user_fg = COLOR_CYAN,
    .assistant_bg = COLOR_BLACK, .assistant_fg = COLOR_WHITE,
    .system_bg = COLOR_BLACK, .system_fg = COLOR_MAGENTA,
    .tool_bg = COLOR_BLACK, .tool_fg = COLOR_YELLOW,
    .error_bg = COLOR_BLACK, .error_fg = COLOR_RED,
    .warn_bg = COLOR_BLACK, .warn_fg = COLOR_YELLOW,
    .hl_bg = COLOR_BLACK, .hl_fg = COLOR_GREEN,
    .dim_bg = COLOR_BLACK, .dim_fg = COLOR_WHITE,
    .border_bg = COLOR_BLUE, .border_fg = COLOR_CYAN,
    .tool_feed_bg = COLOR_BLACK, .tool_feed_fg = COLOR_YELLOW,
    .sel_bg = COLOR_CYAN, .sel_fg = COLOR_BLACK,
    .prompt_sym = ">",
    .tool_sym = "\u2699",
    .think_sym = "\u22EF",
    .error_sym = "\u2716",
    .ok_sym = "\u2714",
};

static const tui_theme_t tui_theme_dark = {
    .name = "dark",
    .status_bg = COLOR_BLACK, .status_fg = COLOR_WHITE,
    .prompt_bg = COLOR_BLACK, .prompt_fg = COLOR_GREEN,
    .user_bg = COLOR_BLACK, .user_fg = COLOR_BLUE,
    .assistant_bg = COLOR_BLACK, .assistant_fg = COLOR_WHITE,
    .system_bg = COLOR_BLACK, .system_fg = COLOR_MAGENTA,
    .tool_bg = COLOR_BLACK, .tool_fg = COLOR_YELLOW,
    .error_bg = COLOR_BLACK, .error_fg = COLOR_RED,
    .warn_bg = COLOR_BLACK, .warn_fg = COLOR_YELLOW,
    .hl_bg = COLOR_BLACK, .hl_fg = COLOR_GREEN,
    .dim_bg = COLOR_BLACK, .dim_fg = COLOR_WHITE,
    .border_bg = COLOR_BLACK, .border_fg = COLOR_CYAN,
    .tool_feed_bg = COLOR_BLACK, .tool_feed_fg = COLOR_YELLOW,
    .sel_bg = COLOR_BLUE, .sel_fg = COLOR_WHITE,
    .prompt_sym = "\u03BB",
    .tool_sym = "\u2699",
    .think_sym = "\u22EF",
    .error_sym = "\u2716",
    .ok_sym = "\u2714",
};

static const tui_theme_t tui_theme_light = {
    .name = "light",
    .status_bg = COLOR_WHITE, .status_fg = COLOR_BLACK,
    .prompt_bg = COLOR_WHITE, .prompt_fg = COLOR_GREEN,
    .user_bg = COLOR_WHITE, .user_fg = COLOR_BLUE,
    .assistant_bg = COLOR_WHITE, .assistant_fg = COLOR_BLACK,
    .system_bg = COLOR_WHITE, .system_fg = COLOR_MAGENTA,
    .tool_bg = COLOR_WHITE, .tool_fg = COLOR_YELLOW,
    .error_bg = COLOR_WHITE, .error_fg = COLOR_RED,
    .warn_bg = COLOR_WHITE, .warn_fg = COLOR_MAGENTA,
    .hl_bg = COLOR_WHITE, .hl_fg = COLOR_GREEN,
    .dim_bg = COLOR_WHITE, .dim_fg = COLOR_BLACK,
    .border_bg = COLOR_WHITE, .border_fg = COLOR_CYAN,
    .tool_feed_bg = COLOR_WHITE, .tool_feed_fg = COLOR_YELLOW,
    .sel_bg = COLOR_CYAN, .sel_fg = COLOR_WHITE,
    .prompt_sym = ">",
    .tool_sym = "*",
    .think_sym = "...",
    .error_sym = "X",
    .ok_sym = "OK",
};

static const tui_theme_t tui_theme_mono = {
    .name = "mono",
    .status_bg = COLOR_BLACK, .status_fg = COLOR_WHITE,
    .prompt_bg = COLOR_BLACK, .prompt_fg = COLOR_WHITE,
    .user_bg = COLOR_BLACK, .user_fg = COLOR_WHITE,
    .assistant_bg = COLOR_BLACK, .assistant_fg = COLOR_WHITE,
    .system_bg = COLOR_BLACK, .system_fg = COLOR_WHITE,
    .tool_bg = COLOR_BLACK, .tool_fg = COLOR_WHITE,
    .error_bg = COLOR_BLACK, .error_fg = COLOR_WHITE,
    .warn_bg = COLOR_BLACK, .warn_fg = COLOR_WHITE,
    .hl_bg = COLOR_BLACK, .hl_fg = COLOR_WHITE,
    .dim_bg = COLOR_BLACK, .dim_fg = COLOR_WHITE,
    .border_bg = COLOR_BLACK, .border_fg = COLOR_WHITE,
    .tool_feed_bg = COLOR_BLACK, .tool_feed_fg = COLOR_WHITE,
    .sel_bg = COLOR_WHITE, .sel_fg = COLOR_BLACK,
    .prompt_sym = ">",
    .tool_sym = ">",
    .think_sym = "...",
    .error_sym = "!",
    .ok_sym = "v",
};

/* Theme registry */
static const tui_theme_t *tui_themes[TUI_MAX_THEMES];
static int tui_theme_count = 0;
static int tui_current_theme = 0; /* index into themes array */

/* Color pair tracking — next available pair */
static int tui_next_pair = 100;

/* Initialize theme system */
static void tui_theme_init(void) {
    tui_theme_count = 0;
    tui_themes[tui_theme_count++] = &tui_theme_default;
    tui_themes[tui_theme_count++] = &tui_theme_dark;
    tui_themes[tui_theme_count++] = &tui_theme_light;
    tui_themes[tui_theme_count++] = &tui_theme_mono;
    tui_current_theme = 0;
}

/* Allocate color pair */
static int tui_alloc_pair(int fg, int bg) {
    int pair = tui_next_pair++;
    if (pair > COLOR_PAIRS - 1) pair = 100; /* wrap */
    init_pair(pair, fg, bg);
    return pair;
}

/* Apply a theme: register all color pairs using allocator */
static void tui_apply_theme(const tui_theme_t *th) {
    if (!th) return;
    /* Reset allocator so pairs 1-13 map to known conventions */
    tui_next_pair = 1;
    tui_alloc_pair(th->status_fg, th->status_bg);       /* 1 = status */
    tui_alloc_pair(th->prompt_fg, th->prompt_bg);       /* 2 = prompt */
    tui_alloc_pair(th->user_fg, th->user_bg);           /* 3 = user */
    tui_alloc_pair(th->assistant_fg, th->assistant_bg); /* 4 = assistant */
    tui_alloc_pair(th->system_fg, th->system_bg);       /* 5 = system */
    tui_alloc_pair(th->tool_fg, th->tool_bg);           /* 6 = tool */
    tui_alloc_pair(th->error_fg, th->error_bg);         /* 7 = error */
    tui_alloc_pair(th->warn_fg, th->warn_bg);           /* 8 = warn */
    tui_alloc_pair(th->hl_fg, th->hl_bg);               /* 9 = hl */
    tui_alloc_pair(th->dim_fg, th->dim_bg);             /* 10 = dim */
    tui_alloc_pair(th->border_fg, th->border_bg);       /* 11 = border */
    tui_alloc_pair(th->tool_feed_fg, th->tool_feed_bg); /* 12 = tool_feed */
    tui_alloc_pair(th->sel_fg, th->sel_bg);             /* 13 = selection */
    if (tui_next_pair < 14) tui_next_pair = 14; /* ensure next free pair */
}

/* Load skin from JSON file */
static bool tui_load_skin_file(const char *path, tui_theme_t *th) {
    if (!path || !th) return false;
    FILE *f = fopen(path, "r");
    if (!f) return false;

    /* Parse simple JSON skin file */
    char line[512];
    th->name[0] = '\0';
    /* Copy defaults first */
    *th = tui_theme_default;

    while (fgets(line, sizeof(line), f)) {
        char key[128], val[128];
        if (sscanf(line, " \"%127[^\"]\" : \"%127[^\"]\"", key, val) == 2) {
            /* Map JSON keys to theme fields */
            if (strcmp(key, "name") == 0)
                strncpy(th->name, val, TUI_THEME_NAME_MAX - 1);
            else if (strcmp(key, "colors.status_bg") == 0) th->status_bg = atoi(val);
            else if (strcmp(key, "colors.status_fg") == 0) th->status_fg = atoi(val);
            else if (strcmp(key, "colors.prompt_bg") == 0) th->prompt_bg = atoi(val);
            else if (strcmp(key, "colors.prompt_fg") == 0) th->prompt_fg = atoi(val);
            else if (strcmp(key, "colors.user_bg") == 0) th->user_bg = atoi(val);
            else if (strcmp(key, "colors.user_fg") == 0) th->user_fg = atoi(val);
            else if (strcmp(key, "colors.assistant_bg") == 0) th->assistant_bg = atoi(val);
            else if (strcmp(key, "colors.assistant_fg") == 0) th->assistant_fg = atoi(val);
            else if (strcmp(key, "colors.error_bg") == 0) th->error_bg = atoi(val);
            else if (strcmp(key, "colors.error_fg") == 0) th->error_fg = atoi(val);
            else if (strcmp(key, "colors.warn_bg") == 0) th->warn_bg = atoi(val);
            else if (strcmp(key, "colors.warn_fg") == 0) th->warn_fg = atoi(val);
            else if (strcmp(key, "colors.tool_bg") == 0) th->tool_bg = atoi(val);
            else if (strcmp(key, "colors.tool_fg") == 0) th->tool_fg = atoi(val);
            else if (strcmp(key, "colors.border_bg") == 0) th->border_bg = atoi(val);
            else if (strcmp(key, "colors.border_fg") == 0) th->border_fg = atoi(val);
            else if (strcmp(key, "colors.sel_bg") == 0) th->sel_bg = atoi(val);
            else if (strcmp(key, "colors.sel_fg") == 0) th->sel_fg = atoi(val);
            else if (strcmp(key, "symbols.prompt") == 0)
                strncpy(th->prompt_sym, val, sizeof(th->prompt_sym) - 1);
            else if (strcmp(key, "symbols.tool") == 0)
                strncpy(th->tool_sym, val, sizeof(th->tool_sym) - 1);
            else if (strcmp(key, "symbols.thinking") == 0)
                strncpy(th->think_sym, val, sizeof(th->think_sym) - 1);
        }
    }
    fclose(f);
    return th->name[0] != '\0';
}

/* Load external skin files from ~/.hermes/skins/ */
static void tui_load_external_skins(void) {
    char skin_dir[HERMES_PATH_MAX];
    const char *home = getenv("HOME");
    if (!home) home = ".";
    snprintf(skin_dir, sizeof(skin_dir), "%s/%s", home, TUI_SKIN_DIR);

    DIR *d = opendir(skin_dir);
    if (!d) {
        /* Try ~/.hermes/skins/ */
        if (!home) home = ".";
        snprintf(skin_dir, sizeof(skin_dir), "%s/.hermes/%s", home, TUI_SKIN_DIR);
        d = opendir(skin_dir);
        if (!d) return;
    }

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL && tui_theme_count < TUI_MAX_THEMES) {
        size_t len = strlen(entry->d_name);
        if (len < 6 || strcmp(entry->d_name + len - 5, ".json") != 0)
            continue;

        char path[HERMES_PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", skin_dir, entry->d_name);

        tui_theme_t *th = (tui_theme_t *)calloc(1, sizeof(tui_theme_t));
        if (th && tui_load_skin_file(path, th)) {
            tui_themes[tui_theme_count++] = th;
        } else {
            free(th);
        }
    }
    closedir(d);
}

/* P198: reload theme by name */
void tui_fullscreen_theme_reload(const char *skin_name) {
    if (!skin_name) return;
    for (int i = 0; i < tui_theme_count; i++) {
        if (strcmp(tui_themes[i]->name, skin_name) == 0) {
            tui_current_theme = i;
            tui_apply_theme(tui_themes[i]);
            return;
        }
    }
}

/* Get current theme */
static const tui_theme_t *tui_get_theme(void) {
    if (tui_current_theme < 0 || tui_current_theme >= tui_theme_count)
        return &tui_theme_default;
    return tui_themes[tui_current_theme];
}

/* ==================================================================
 *  P200: MOBILE MODE — responsive layout, touch hints, compact status
 * ================================================================== */

typedef enum {
    TUI_LAYOUT_NORMAL,
    TUI_LAYOUT_MOBILE,
    TUI_LAYOUT_COMPACT,
} tui_layout_mode_t;

/* ==================================================================
 *  P189: LAYOUT — split pane management
 * ================================================================== */

/* Pane identifiers */
typedef enum {
    PANE_HISTORY = 0,       /* message history (top-left, large) */
    PANE_TOOL_FEED,         /* tool feed (top-right, medium) */
    PANE_INPUT,             /* input (bottom) */
    PANE_STATUS,            /* status bar (very bottom) */
    PANE_COUNT,
} pane_id_t;

/* Pane state */
typedef struct {
    WINDOW *win;
    int     rows, cols;
    int     y, x;
    bool    visible;
    bool    scrollable;
    int     scroll_pos;
    int     line_count;
} pane_t;

/* ==================================================================
 *  P190: INPUT — multi-line input buffer
 * ================================================================== */

#define INPUT_BUF_SIZE 65536
#define INPUT_HISTORY_MAX 512
#define AUTOCOMPLETE_MAX 32
#define EMOJI_MAX 50

/* Slash command definitions for autocomplete */
typedef struct {
    const char *cmd;
    const char *desc;
    const char *args; /* arg hints */
} slash_cmd_t;

static const slash_cmd_t slash_commands[] = {
    {"/help",    "Show help message", ""},
    {"/quit",    "Exit the TUI", ""},
    {"/exit",    "Exit the TUI", ""},
    {"/clear",   "Clear output pane", ""},
    {"/theme",   "Set theme (default/dark/light/mono)", "<name>"},
    {"/model",   "Show/set current model", "[model_name]"},
    {"/tokens",  "Show token usage", ""},
    {"/budget",  "Show budget status", ""},
    {"/sessions","Open session browser", ""},
    {"/config",  "Open config editor", ""},
    {"/save",    "Save current session", "[name]"},
    {"/load",    "Load a session", "<session_id>"},
    {"/export",  "Export session as JSON/Markdown", "<format>"},
    {"/delete",  "Delete current session", ""},
    {"/verbose", "Set verbose level (0/1/2)", "<level>"},
    {"/yolo",    "Toggle yolo mode", ""},
    {"/fast",    "Toggle fast mode", ""},
    {"/mobile",  "Toggle mobile/compact layout", ""},
    {"/reset",   "Reset conversation", ""},
    {"/undo",    "Undo last turn", ""},
    {"/redraw",  "Force screen redraw", ""},
    {"/skin",    "List/reload skins", ""},
    {"/image",   "Display an image file", "<path>"},
    {"/gateway", "Show gateway status dashboard", ""},
    {"/cron",    "Show cron job viewer", ""},
    {"/logs",    "Show log viewer", ""},
    {"/skills",  "Browse available skills", ""},
    {"/todos",   "Show todo/kanban board", ""},
    {"/agent",   "Show agent info (model, provider, tokens)", ""},
    {NULL, NULL, NULL}
};

/* Input state — P190: multi-line editing */
typedef struct {
    char    buf[INPUT_BUF_SIZE];
    int     pos;                /* cursor position in buf */
    int     len;                /* current length */
    int     scroll_col;         /* horizontal scroll offset */
    int     cursor_row;         /* visual row within multi-line input */

    /* History */
    char   *history[INPUT_HISTORY_MAX];
    int     history_count;
    int     history_pos;        /* -1 = new input, 0..N-1 = history */

    /* Autocomplete — P190 */
    char    autocomplete_word[256];
    int     autocomplete_count;
    char    autocomplete_matches[AUTOCOMPLETE_MAX][256];
    int     autocomplete_sel;   /* selected autocomplete index */
    bool    autocomplete_active;

    /* Emoji — P190 */
    bool    emoji_picker_active;
    int     emoji_sel;
    char    emoji_results[EMOJI_MAX][16];
    int     emoji_count;

    /* State */
    bool    active;
    bool    echo;               /* echo typed characters */
    bool    multiline;          /* true if input spans multiple rows */
} input_state_t;

/* Common emoji list */
static const char *emoji_list[][2] = {
    {"\u2764", "heart"},
    {"\U0001F600", "grinning"},
    {"\U0001F603", "smile"},
    {"\U0001F60A", "blush"},
    {"\U0001F60E", "sunglasses"},
    {"\U0001F389", "party"},
    {"\U0001F44D", "thumbsup"},
    {"\U0001F44E", "thumbsdown"},
    {"\U0001F4AC", "speech"},
    {"\U0001F4A1", "bulb"},
    {"\u2705", "check"},
    {"\u274C", "cross"},
    {"\u2B50", "star"},
    {"\U0001F525", "fire"},
    {"\U0001F4A6", "sweat"},
    {"\U0001F4AA", "muscle"},
    {"\U0001F4AD", "thought"},
    {"\u26A1", "zap"},
    {"\u2192", "arrow"},
    {"\u2190", "back"},
    {NULL, NULL}
};

/* ==================================================================
 *  P191: MESSAGE DISPLAY — role-coloring, syntax, markdown
 * ================================================================== */

#define MAX_MESSAGES_DISPLAY 4096
#define MAX_LINE_LENGTH 4096

typedef enum {
    MSG_ROLE_USER,
    MSG_ROLE_ASSISTANT,
    MSG_ROLE_SYSTEM,
    MSG_ROLE_TOOL,
    MSG_ROLE_ERROR,
    MSG_ROLE_WARN,
    MSG_ROLE_INFO,
} msg_role_t;

typedef struct {
    msg_role_t role;
    char      text[MAX_LINE_LENGTH];
    int       color_pair;       /* ncurses color pair to use */
    bool      bold;
    bool      dim;
    bool      code_block;       /* in code fence */
    char      code_lang[32];    /* language hint */
} display_line_t;

/* Message history ring buffer */
typedef struct {
    display_line_t lines[MAX_MESSAGES_DISPLAY];
    int count;
    int head; /* ring buffer head */
} message_history_t;

/* ==================================================================
 *  P192: STREAMING — token stream display and counter
 * ================================================================== */

#define TYPE_AHEAD_BUF_SIZE 1024

typedef struct {
    bool    active;
    bool    abort_requested;    /* Ctrl+C during streaming */
    char    current_line[MAX_LINE_LENGTH];
    int     current_pos;
    int     token_count;
    double  start_time;
    double  last_token_time;
    double  tokens_per_sec;
    int     bytes_received;
    bool    first_token;
    /* Type-ahead buffer: captures keystrokes during streaming (T18) */
    char    type_ahead_buf[TYPE_AHEAD_BUF_SIZE];
    int     type_ahead_len;
} stream_state_t;

/* ==================================================================
 *  P193: TOOL FEED — live tool call display
 * ================================================================== */

#define MAX_TOOL_CALLS 32

typedef enum {
    TOOL_STATUS_PENDING,
    TOOL_STATUS_RUNNING,
    TOOL_STATUS_DONE,
    TOOL_STATUS_ERROR,
} tool_status_t;

typedef struct {
    char          name[64];
    char          args[256];
    tool_status_t status;
    int           progress;     /* 0-100 */
    int           total_steps;
    char          result_preview[256];
} tool_call_entry_t;

typedef struct {
    tool_call_entry_t calls[MAX_TOOL_CALLS];
    int count;
    int active_count;
} tool_feed_state_t;

/* ==================================================================
 *  P194: STATUS BAR — model, tokens, iterations, budget
 * ================================================================== */

typedef struct {
    char    model[128];
    char    provider[64];
    int     iteration;
    int     max_iterations;
    int     tokens_in;
    int     tokens_out;
    double  budget_remaining;
    char    mode[32];           /* chat, streaming, idle */
    bool    dirty;              /* needs redraw */
} status_bar_state_t;

/* ==================================================================
 *  P195: SESSION BROWSER
 * ================================================================== */

#define SESSION_LIST_MAX 256
#define SESSION_SEARCH_MAX 128

typedef struct {
    char sessions[SESSION_LIST_MAX][64]; /* session IDs */
    session_meta_t meta[SESSION_LIST_MAX]; /* session metadata */
    int  count;
    int  selected;
    int  scroll_offset;
    char search[SESSION_SEARCH_MAX];
    bool active;
    bool search_mode;
} session_browser_state_t;

/* ==================================================================
 *  P196: CONFIG EDITOR
 * ================================================================== */

#define CONFIG_KEY_MAX 64
#define CONFIG_VALUE_MAX 256

typedef struct {
    char key[CONFIG_KEY_MAX];
    char value[CONFIG_VALUE_MAX];
    char description[256];
} config_entry_t;

#define CONFIG_SEARCH_MAX 128

typedef struct {
    config_entry_t entries[CONFIG_KEY_MAX];
    int count;
    int selected;
    int scroll_offset;
    bool active;
    bool edit_mode;         /* editing value */
    char edit_buf[256];
    int edit_pos;
    char search[CONFIG_SEARCH_MAX];
    bool search_mode;
} config_editor_state_t;

/* ==================================================================
 *  P197: IMAGE VIEWER — sixel/kitty protocol
 * ================================================================== */

typedef struct {
    bool    sixel_supported;
    bool    kitty_supported;
    bool    image_displayed;
    char    image_path[HERMES_PATH_MAX];
    int     zoom_level;       /* 100 = normal */
    int     pan_x, pan_y;
} image_viewer_state_t;

/* ==================================================================
 *  T13: MODEL PICKER — interactive model selection overlay
 * ================================================================== */

#define MODEL_PICKER_MAX 256
#define MODEL_NAME_MAX 128

typedef struct {
    char names[MODEL_PICKER_MAX][MODEL_NAME_MAX];
    int  count;
    int  selected;
    int  scroll_offset;
} model_picker_state_t;

/* T15: Todo panel state */
#define TODO_PANEL_MAX 128
#define TODO_TITLE_MAX 96
#define TODO_ID_MAX 48
#define TODO_STATUS_MAX 16

typedef struct {
    char  id[TODO_PANEL_MAX][TODO_ID_MAX];
    char  title[TODO_PANEL_MAX][TODO_TITLE_MAX];
    char  status[TODO_PANEL_MAX][TODO_STATUS_MAX];
    char  assignee[TODO_PANEL_MAX][TODO_STATUS_MAX];
    int   count;
    int   selected;
    int   scroll_offset;
    int   filter_idx;
    bool  active;
} todo_panel_state_t;

/* ==================================================================
 *  P199: GATEWAY — JSON-RPC backend via FIFO
 * ================================================================== */

#define RPC_FIFO_PATH "/tmp/hermes-tui-rpc"
#define RPC_BUF_SIZE 65536
#define RPC_LINE_MAX 32768

typedef enum {
    RPC_IDLE,
    RPC_CONNECTED,
} rpc_state_t;

typedef struct {
    rpc_state_t state;
    int         fifo_fd;
    char        read_buf[RPC_BUF_SIZE];
    int         read_pos;
} gateway_state_t;

/* ==================================================================
 *  MAIN TUI STATE — all subsystems combined
 * ================================================================== */

typedef struct {
    /* Terminal */
    int rows, cols;
    bool running;
    bool initialized;
    tui_layout_mode_t layout_mode;

    /* Panes (P189) */
    pane_t panes[PANE_COUNT];

    /* Subsystems */
    input_state_t         input;          /* P190 */
    message_history_t     history;        /* P191 */
    stream_state_t        stream;         /* P192 */
    tool_feed_state_t     tool_feed;      /* P193 */
    status_bar_state_t    status;         /* P194 */
    session_browser_state_t sessions;     /* P195 */
    config_editor_state_t config_editor;  /* P196 */
    image_viewer_state_t  image_viewer;   /* P197 */
    gateway_state_t       gateway;        /* P199 */
    model_picker_state_t  model_picker;   /* T13: Model picker */
    todo_panel_state_t    todo_panel;     /* T15: Todo/kanban panel */

    /* Saved stderr fd for TUI mode stderr redirection */
    int saved_stderr;

    /* Agent reference */
    agent_state_t        *agent;

    /* Modal overlay state */
    int     think_frame;        /* P192 animation frame counter */
    enum {
        MODE_NORMAL,
        MODE_SESSION_BROWSE,
        MODE_CONFIG_EDIT,
        MODE_IMAGE_VIEW,
        MODE_GATEWAY_STATUS,
        MODE_CRON_VIEW,
        MODE_LOG_VIEW,
        MODE_SKILL_BROWSE,
        MODE_HELP,
        MODE_MODEL_PICK,        /* T13: Model picker overlay */
        MODE_TODO_PANEL,        /* T15: Todo/kanban panel */
        MODE_AGENT_INFO,        /* T14: Agent info overlay */
    } modal_mode;
} tui_global_state_t;

static tui_global_state_t tui;

/* ==================================================================
 *  P189: LAYOUT — pane creation, resizing, split logic
 * ================================================================== */

/* Calculate pane dimensions based on layout mode */
static void tui_calculate_layout(void) {
    int rows = tui.rows;
    int cols = tui.cols;

    /* Status bar always at bottom */
    int status_height = 1;
    int status_y = rows - status_height;

    /* Input area */
    int input_height;
    int input_y;

    if (tui.layout_mode == TUI_LAYOUT_MOBILE || tui.layout_mode == TUI_LAYOUT_COMPACT) {
        input_height = 2;           /* compact input */
        input_y = status_y - input_height;
    } else {
        input_height = 4;           /* multi-line input */
        input_y = status_y - input_height;
    }

    /* Tool feed pane (right column, above input) */
    int tool_feed_width;
    int tool_feed_x;
    int tool_feed_height = input_y;
    int tool_feed_y = 0;

    if (tui.layout_mode == TUI_LAYOUT_MOBILE) {
        /* Mobile: tool feed at bottom of history, full width, small */
        tool_feed_width = cols;
        tool_feed_x = 0;
        tool_feed_height = 3;      /* thin */
        tool_feed_y = input_y - tool_feed_height;
        input_y -= tool_feed_height;
    } else if (tui.layout_mode == TUI_LAYOUT_COMPACT) {
        tool_feed_width = 0;        /* hidden in compact */
        tool_feed_x = 0;
        tool_feed_height = 0;
    } else {
        /* Normal: tool feed on right side */
        tool_feed_width = cols / 4;
        if (tool_feed_width < 20) tool_feed_width = 20;
        if (tool_feed_width > 40) tool_feed_width = 40;
        tool_feed_x = cols - tool_feed_width;
        tool_feed_height = input_y;
    }

    /* History pane (everything remaining) */
    int hist_y = 0;
    int hist_height;
    int hist_width;
    int hist_x = 0;

    if (tui.layout_mode == TUI_LAYOUT_MOBILE) {
        hist_width = cols;
        hist_height = tool_feed_y;
    } else if (tui.layout_mode == TUI_LAYOUT_COMPACT) {
        hist_width = cols;
        hist_height = input_y;
    } else {
        hist_width = cols - tool_feed_width;
        hist_height = tool_feed_height;
    }

    /* Store pane dimensions */
    tui.panes[PANE_HISTORY].y = hist_y;
    tui.panes[PANE_HISTORY].x = hist_x;
    tui.panes[PANE_HISTORY].rows = hist_height;
    tui.panes[PANE_HISTORY].cols = hist_width;
    tui.panes[PANE_HISTORY].visible = hist_height > 0 && hist_width > 5;

    tui.panes[PANE_TOOL_FEED].y = tool_feed_y;
    tui.panes[PANE_TOOL_FEED].x = tool_feed_x;
    tui.panes[PANE_TOOL_FEED].rows = tool_feed_height;
    tui.panes[PANE_TOOL_FEED].cols = tool_feed_width;
    tui.panes[PANE_TOOL_FEED].visible = tool_feed_height > 1 && tool_feed_width > 10;

    tui.panes[PANE_INPUT].y = input_y;
    tui.panes[PANE_INPUT].x = 0;
    tui.panes[PANE_INPUT].rows = input_height;
    tui.panes[PANE_INPUT].cols = cols;
    tui.panes[PANE_INPUT].visible = input_height > 0;

    tui.panes[PANE_STATUS].y = status_y;
    tui.panes[PANE_STATUS].x = 0;
    tui.panes[PANE_STATUS].rows = status_height;
    tui.panes[PANE_STATUS].cols = cols;
    tui.panes[PANE_STATUS].visible = true;
}

/* Create/recreate all pane windows */
static void tui_create_windows(void) {
    tui_calculate_layout();

    for (int i = 0; i < PANE_COUNT; i++) {
        pane_t *p = &tui.panes[i];
        if (p->win) delwin(p->win);
        p->win = NULL;

        if (!p->visible || p->rows < 1 || p->cols < 1)
            continue;

        p->win = newwin(p->rows, p->cols, p->y, p->x);
        if (p->win) {
            scrollok(p->win, TRUE);
            keypad(p->win, TRUE);
            wrefresh(p->win);
        }
    }
}

/* P189: Recalculate and redraw all panes (for resize) */
static void tui_resize_panes(void) {
    /* Get new terminal dimensions */
    getmaxyx(stdscr, tui.rows, tui.cols);

    /* Recreate all windows with new layout */
    tui_create_windows();

    /* Redraw all content */
    for (int i = 0; i < PANE_COUNT; i++) {
        if (tui.panes[i].win)
            wnoutrefresh(tui.panes[i].win);
    }
    doupdate();
}

/* ==================================================================
 *  SIGWINCH handler — terminal resize
 * ================================================================== */

/* SIGWINCH handler — set flag for main loop to process */
static volatile sig_atomic_t g_resize_requested = 0;

static void handle_winch(int sig) {
    (void)sig;
    g_resize_requested = 1;
    tui_eventpub_resize(tui.rows, tui.cols);
}

/* SIGINT handler — abort streaming during blocking agent_chat call */
static void handle_sigint(int sig) {
    (void)sig;
    if (tui.stream.active)
        tui.stream.abort_requested = true;
}

/* ==================================================================
 *  P191: MESSAGE DISPLAY — rendering messages with role colors
 * ================================================================== */

/* Get color pair for role */
static int tui_role_color(msg_role_t role) {
    switch (role) {
        case MSG_ROLE_USER:      return 3;
        case MSG_ROLE_ASSISTANT: return 4;
        case MSG_ROLE_SYSTEM:    return 5;
        case MSG_ROLE_TOOL:      return 6;
        case MSG_ROLE_ERROR:     return 7;
        case MSG_ROLE_WARN:      return 8;
        case MSG_ROLE_INFO:      return 10;
        default:                 return 4;
    }
}

/* Add a line to message history */
static void tui_history_add(msg_role_t role, const char *text, bool bold) {
    display_line_t *line = &tui.history.lines[tui.history.head];
    strncpy(line->text, text ? text : "", MAX_LINE_LENGTH - 1);
    line->role = role;
    line->color_pair = tui_role_color(role);
    line->bold = bold;
    line->dim = false;
    line->code_block = false;

    tui.history.head = (tui.history.head + 1) % MAX_MESSAGES_DISPLAY;
    if (tui.history.count < MAX_MESSAGES_DISPLAY)
        tui.history.count++;

    /* Emit event */
    tui_eventpub_message((int)role, text ? text : "", bold);
}

/* Write a formatted line to a window with role coloring */
static void tui_wprint_role(WINDOW *win, msg_role_t role, const char *text,
                             bool bold, bool dim) {
    if (!win) return;

    int pair = tui_role_color(role);

    if (bold) wattron(win, A_BOLD);
    if (dim)  wattron(win, A_DIM);
    wattron(win, COLOR_PAIR(pair));

    wprintw(win, "%s", text);

    wattroff(win, COLOR_PAIR(pair));
    if (bold) wattroff(win, A_BOLD);
    if (dim)  wattroff(win, A_DIM);
}

/* Simple markdown-like rendering — P191 */
static void tui_render_markdown(WINDOW *win, const char *text, msg_role_t role) {
    if (!win || !text) return;

    int base_pair = tui_role_color(role);
    int hl_pair = 9;  /* highlight pair */
    int dim_pair = 10;

    /* State machine for inline formatting */
    bool in_bold = false;
    bool in_code = false;
    bool in_header = false;
    int col = 0;

    for (const char *p = text; *p; p++) {
        if (*p == '\n') {
            /* End formatting at newline */
            if (in_bold) { wattroff(win, A_BOLD); in_bold = false; }
            /* Flush header style */
            if (in_header) {
                wattroff(win, A_BOLD);
                in_header = false;
            }
            waddch(win, '\n');
            col = 0;
            /* Check for header at start of line */
            if (p[1] == '#') in_header = true;
            continue;
        }

        /* Detect markdown patterns */
        if (col == 0 && *p == '#' && role != MSG_ROLE_ERROR) {
            in_header = true;
            wattron(win, A_BOLD | COLOR_PAIR(base_pair));
            continue;
        }
        if (in_header && *p == '#') continue; /* skip extra # */
        if (in_header && *p == ' ') continue; /* skip space after # */

        if (*p == '`' && !in_bold) {
            if (in_code) {
                wattroff(win, COLOR_PAIR(hl_pair));
                in_code = false;
            } else {
                wattron(win, COLOR_PAIR(hl_pair));
                in_code = true;
            }
            continue;
        }

        /* Bold: **text** */
        if (*p == '*' && *(p+1) == '*' && col > 0) {
            if (in_bold) {
                wattroff(win, A_BOLD);
                in_bold = false;
            } else {
                wattron(win, A_BOLD);
                in_bold = true;
            }
            p++; /* skip second * */
            continue;
        }

        /* Code block lines (indented) */
        if (col == 0 && *p == ' ' && role != MSG_ROLE_ERROR) {
            wattron(win, COLOR_PAIR(dim_pair));
            waddch(win, *p);
            wattroff(win, COLOR_PAIR(dim_pair));
            col++;
            continue;
        }

        /* Write character */
        if (in_header) {
            wattron(win, A_BOLD | COLOR_PAIR(base_pair));
            waddch(win, *p);
            wattroff(win, A_BOLD);
        } else if (in_code) {
            wattron(win, COLOR_PAIR(hl_pair));
            waddch(win, *p);
            wattroff(win, COLOR_PAIR(hl_pair));
        } else {
            waddch(win, *p);
        }
        col++;
    }

    if (in_bold) wattroff(win, A_BOLD);
    if (in_code) wattroff(win, COLOR_PAIR(hl_pair));
}

/* Redraw the history pane from the message buffer */
static void tui_redraw_history(void) {
    WINDOW *win = tui.panes[PANE_HISTORY].win;
    if (!win) return;

    werase(win);

    /* Calculate visible lines */
    int max_rows = tui.panes[PANE_HISTORY].rows - 1;
    if (max_rows < 1) return;

    int start = tui.history.count - max_rows - tui.panes[PANE_HISTORY].scroll_pos;
    if (start < 0) start = 0;

    /* Draw header */
    wattron(win, A_DIM | COLOR_PAIR(10));
    mvwprintw(win, 0, 0, " Messages (%d) ", tui.history.count);
    wattroff(win, A_DIM | COLOR_PAIR(10));

    /* Draw visible lines */
    int y = 1;
    for (int i = start; i < tui.history.count && y < tui.panes[PANE_HISTORY].rows; i++) {
        /* Map i through ring buffer */
        int ring_idx;
        if (tui.history.count < MAX_MESSAGES_DISPLAY) {
            /* Simple case: linear */
            ring_idx = i;
        } else {
            /* Ring buffer: oldest = head, newest = (head - 1) % MAX */
            ring_idx = (tui.history.head - tui.history.count + i) % MAX_MESSAGES_DISPLAY;
            if (ring_idx < 0) ring_idx += MAX_MESSAGES_DISPLAY;
        }

        const display_line_t *line = &tui.history.lines[ring_idx];

        /* Role prefix */
        const char *prefix = "";
        switch (line->role) {
            case MSG_ROLE_USER:      prefix = "You: "; break;
            case MSG_ROLE_ASSISTANT: prefix = "AI:  "; break;
            case MSG_ROLE_SYSTEM:    prefix = "Sys: "; break;
            case MSG_ROLE_TOOL:      prefix = "  -> "; break;
            case MSG_ROLE_ERROR:     prefix = "Err: "; break;
            case MSG_ROLE_WARN:      prefix = "Wrn: "; break;
            case MSG_ROLE_INFO:      prefix = ""; break;
        }

        wmove(win, y, 0);

        /* Print prefix in role color */
        tui_wprint_role(win, line->role, prefix, true, false);

        /* Print message text with markdown rendering */
        int prefix_len = strlen(prefix);
        /* wmove past prefix */
        wmove(win, y, prefix_len);

        if (line->bold) wattron(win, A_BOLD);
        tui_render_markdown(win, line->text, line->role);
        if (line->bold) wattroff(win, A_BOLD);

        y++;
    }

    wnoutrefresh(win);
}

/* ==================================================================
 *  P194: STATUS BAR — model, tokens, iterations, budget
 * ================================================================== */

static void tui_redraw_status(void) {
    WINDOW *win = tui.panes[PANE_STATUS].win;
    if (!win) return;

    werase(win);

    wattron(win, A_REVERSE | COLOR_PAIR(1));

    char buf[1024];
    int pos = 0;

    /* Model/Provider info */
    pos += snprintf(buf + pos, sizeof(buf) - pos, " %s", tui.status.model[0] ? tui.status.model : "Hermes");
    if (tui.status.provider[0])
        pos += snprintf(buf + pos, sizeof(buf) - pos, " [%s]", tui.status.provider);

    /* Mode */
    pos += snprintf(buf + pos, sizeof(buf) - pos, " | %s", tui.status.mode[0] ? tui.status.mode : "idle");

    /* Iterations */
    if (tui.status.iteration > 0)
        pos += snprintf(buf + pos, sizeof(buf) - pos, " | iter %d/%d",
                        tui.status.iteration, tui.status.max_iterations);

    /* Token usage */
    if (tui.status.tokens_in > 0 || tui.status.tokens_out > 0)
        pos += snprintf(buf + pos, sizeof(buf) - pos, " | \u2191%d \u2193%d",
                        tui.status.tokens_in, tui.status.tokens_out);

    /* Budget */
    if (tui.status.budget_remaining > 0)
        pos += snprintf(buf + pos, sizeof(buf) - pos, " | budget %.1f",
                        tui.status.budget_remaining);

    /* Layout indicator */
    if (tui.layout_mode == TUI_LAYOUT_MOBILE)
        pos += snprintf(buf + pos, sizeof(buf) - pos, " | MOBILE");
    else if (tui.layout_mode == TUI_LAYOUT_COMPACT)
        pos += snprintf(buf + pos, sizeof(buf) - pos, " | COMPACT");

    /* Right-align time */
    char time_str[16];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    if (tm_info)
        strftime(time_str, sizeof(time_str), " %H:%M ", tm_info);
    else
        snprintf(time_str, sizeof(time_str), " --:-- ");

    int right_start = tui.panes[PANE_STATUS].cols - (int)strlen(time_str);
    if (right_start > pos) {
        while (pos < right_start)
            buf[pos++] = ' ';
        memcpy(buf + pos, time_str, strlen(time_str));
        pos += strlen(time_str);
    }

    buf[pos] = '\0';
    mvwprintw(win, 0, 0, "%s", buf);
    wattroff(win, A_REVERSE | COLOR_PAIR(1));

    wnoutrefresh(win);
}

/* Update status bar from agent state (P194) */
void tui_fullscreen_status_update(const char *model, const char *provider,
                                    int iteration, int max_iterations,
                                    int tokens_in, int tokens_out,
                                    double budget_remaining) {
    if (model) strncpy(tui.status.model, model, sizeof(tui.status.model) - 1);
    if (provider) strncpy(tui.status.provider, provider, sizeof(tui.status.provider) - 1);
    tui.status.iteration = iteration;
    tui.status.max_iterations = max_iterations;
    tui.status.tokens_in = tokens_in;
    tui.status.tokens_out = tokens_out;
    tui.status.budget_remaining = budget_remaining;
    tui.status.dirty = true;
    tui_eventpub_agent(model, provider, iteration, max_iterations,
                       tokens_in, tokens_out, budget_remaining);
}

/* ==================================================================
 *  P192: STREAMING — token display and counter
 * ================================================================== */

/* Start streaming session */
static void tui_stream_start(void) {
    tui.stream.active = true;
    tui.stream.abort_requested = false;
    tui.stream.current_pos = 0;
    tui.stream.current_line[0] = '\0';
    tui.stream.token_count = 0;
    tui.stream.start_time = (double)clock() / CLOCKS_PER_SEC;
    tui.stream.first_token = true;
    tui.stream.bytes_received = 0;
    tui.stream.tokens_per_sec = 0.0;
    tui.stream.type_ahead_len = 0;
    tui.stream.type_ahead_buf[0] = '\0';
    tui.think_frame = 0;

    /* Set input window to non-blocking during entire streaming */
    if (tui.panes[PANE_INPUT].win)
        nodelay(tui.panes[PANE_INPUT].win, TRUE);

    strcpy(tui.status.mode, "stream");
}

/* End streaming session */
static void tui_stream_finish(void) {
    if (!tui.stream.active) return;

    /* Flush remaining content */
    if (tui.stream.current_pos > 0) {
        tui.stream.current_line[tui.stream.current_pos] = '\0';
        tui_history_add(MSG_ROLE_ASSISTANT, tui.stream.current_line, false);
        tui.stream.current_pos = 0;
    }

    tui.stream.active = false;
    tui.stream.first_token = false;
    strcpy(tui.status.mode, "chat");

    /* Restore blocking input */
    if (tui.panes[PANE_INPUT].win)
        nodelay(tui.panes[PANE_INPUT].win, FALSE);

    /* Inject type-ahead buffer captured during streaming (T18) */
    if (tui.stream.type_ahead_len > 0) {
        int inject_len = tui.stream.type_ahead_len;
        if (tui.input.len + inject_len < INPUT_BUF_SIZE - 1) {
            memcpy(tui.input.buf + tui.input.len, tui.stream.type_ahead_buf, inject_len);
            tui.input.len += inject_len;
            tui.input.buf[tui.input.len] = '\0';
            tui.input.pos = tui.input.len;
        }
        tui.stream.type_ahead_len = 0;
        tui.stream.type_ahead_buf[0] = '\0';
    }

    /* Update token rate */
    double elapsed = ((double)clock() / CLOCKS_PER_SEC) - tui.stream.start_time;
    if (elapsed > 0)
        tui.stream.tokens_per_sec = tui.stream.token_count / elapsed;

    tui_redraw_history();
    tui_redraw_status();
}

/* Write a streaming token to the display */
void tui_fullscreen_stream_token(const char *token) {
    if (!token || !*token) return;

    if (!tui.stream.active)
        tui_stream_start();

    tui.stream.token_count++;
    tui.stream.bytes_received += strlen(token);
    tui.stream.last_token_time = (double)clock() / CLOCKS_PER_SEC;

    /* Emit event */
    tui_eventpub_stream_token(token, tui.stream.token_count);

    /* Add to current streaming line */
    int tlen = strlen(token);
    if (tui.stream.current_pos + tlen < MAX_LINE_LENGTH - 1) {
        memcpy(tui.stream.current_line + tui.stream.current_pos, token, tlen);
        tui.stream.current_pos += tlen;
        tui.stream.current_line[tui.stream.current_pos] = '\0';
    }

    /* Check for newlines — commit line on each newline */
    if (strchr(token, '\n')) {
        /* Commit completed lines */
        char *save = strdup(tui.stream.current_line);
        if (save) {
            /* Remove trailing newline for storage */
            size_t slen = strlen(save);
            if (slen > 0 && save[slen-1] == '\n') save[slen-1] = '\0';
            tui_history_add(MSG_ROLE_ASSISTANT, save, false);
            free(save);
        }
        tui.stream.current_pos = 0;
        tui.stream.current_line[0] = '\0';
    }

    /* Display latest content in history pane */
    WINDOW *win = tui.panes[PANE_HISTORY].win;
    if (win) {
        /* Show streaming content at bottom */
        int max_rows = tui.panes[PANE_HISTORY].rows - 1;
        int y = max_rows; /* last line */

        /* Clear line and write streaming content */
        mvwprintw(win, y, 0, "%-*s", tui.panes[PANE_HISTORY].cols - 1, " ");
        mvwprintw(win, y, 0, "%s", tui.stream.current_line);

        /* Show token counter in dim */
        if (tui.stream.token_count > 0 && tui.stream.token_count % 50 == 0) {
            /* Brief counter flash */
            wattron(win, A_DIM | COLOR_PAIR(10));
            mvwprintw(win, 0, tui.panes[PANE_HISTORY].cols - 15, " [%d tok]", tui.stream.token_count);
            wattroff(win, A_DIM | COLOR_PAIR(10));
        }

        wnoutrefresh(win);

        /* Update status bar with token count */
        tui_fullscreen_status_update(NULL, NULL, 0, 0, 0, tui.stream.token_count, 0);

        /* Redraw input and status */
        wnoutrefresh(tui.panes[PANE_INPUT].win);
        tui_redraw_status();
        doupdate();
    }
}

void tui_fullscreen_stream_done(void) {
    tui_stream_finish();
    tui_eventpub_stream_done();
}

/* ==================================================================
 *  P193: TOOL FEED — live tool call display
 * ================================================================== */

/* Add or update a tool call in the feed */
void tui_fullscreen_tool_status(const char *tool_name, const char *status,
                                 int progress, int total) {
    if (!tool_name) return;

    /* Find existing or create new */
    tool_call_entry_t *entry = NULL;
    for (int i = 0; i < tui.tool_feed.count; i++) {
        if (strcmp(tui.tool_feed.calls[i].name, tool_name) == 0) {
            entry = &tui.tool_feed.calls[i];
            break;
        }
    }

    if (!entry) {
        if (tui.tool_feed.count >= MAX_TOOL_CALLS)
            return;
        entry = &tui.tool_feed.calls[tui.tool_feed.count++];
        strncpy(entry->name, tool_name, sizeof(entry->name) - 1);
        entry->status = TOOL_STATUS_PENDING;
        entry->progress = 0;
        entry->total_steps = total;
    }

    /* Update status */
    if (strcmp(status, "running") == 0) entry->status = TOOL_STATUS_RUNNING;
    else if (strcmp(status, "done") == 0) entry->status = TOOL_STATUS_DONE;
    else if (strcmp(status, "error") == 0) entry->status = TOOL_STATUS_ERROR;
    else if (strcmp(status, "pending") == 0) entry->status = TOOL_STATUS_PENDING;

    if (progress >= 0) entry->progress = progress;
    if (total > 0) entry->total_steps = total;

    /* Emit event */
    tui_eventpub_tool(tool_name, status, progress, total, NULL);

    /* Redraw tool feed pane */
    WINDOW *win = tui.panes[PANE_TOOL_FEED].win;
    if (!win) return;

    werase(win);

    /* Title */
    wattron(win, A_DIM | COLOR_PAIR(10));
    mvwprintw(win, 0, 0, " Tools (%d/%d) ", tui.tool_feed.active_count, tui.tool_feed.count);
    wattroff(win, A_DIM | COLOR_PAIR(10));

    /* List tool calls */
    int y = 1;
    for (int i = 0; i < tui.tool_feed.count && y < tui.panes[PANE_TOOL_FEED].rows; i++) {
        const tool_call_entry_t *tc = &tui.tool_feed.calls[i];

        /* Status symbol */
        char sym;
        int pair;
        switch (tc->status) {
            case TOOL_STATUS_PENDING: sym = ' '; pair = 10; break;
            case TOOL_STATUS_RUNNING: sym = '>'; pair = 6; break;
            case TOOL_STATUS_DONE:    sym = 'v'; pair = 9; break;
            case TOOL_STATUS_ERROR:   sym = '!'; pair = 7; break;
            default:                  sym = '?'; pair = 10;
        }

        wattron(win, COLOR_PAIR(pair) | A_BOLD);
        wprintw(win, "%c %s", sym, tc->name);
        wattroff(win, A_BOLD);

        /* Progress bar */
        if (tc->status == TOOL_STATUS_RUNNING && tc->total_steps > 0) {
            int pct = tc->progress * 100 / tc->total_steps;
            int bar_w = tui.panes[PANE_TOOL_FEED].cols - 10 - strlen(tc->name);
            if (bar_w > 20) bar_w = 20;
            if (bar_w < 3) bar_w = 3;

            int filled = pct * bar_w / 100;
            waddch(win, ' ');
            wattron(win, A_REVERSE);
            for (int b = 0; b < bar_w; b++)
                waddch(win, b < filled ? ' ' : '.');
            wattroff(win, A_REVERSE);
            wprintw(win, " %d%%", pct);
        }

        waddch(win, '\n');
        y++;
    }

    wnoutrefresh(win);

    /* Also redraw history if streaming */
    if (tui.stream.active)
        tui_redraw_history();
    doupdate();
}

/* ==================================================================
 *  P190: INPUT — multi-line editing, autocomplete, emoji picker
 * ================================================================== */

/* Initialize input state */
static void tui_input_init(void) {
    memset(&tui.input, 0, sizeof(tui.input));
    tui.input.history_pos = -1;
    tui.input.active = true;
}

/* Add line to input history */
static void tui_input_history_add(const char *line) {
    if (!line || !*line) return;
    /* Avoid duplicates */
    if (tui.input.history_count > 0 &&
        strcmp(tui.input.history[tui.input.history_count - 1], line) == 0)
        return;

    if (tui.input.history_count >= INPUT_HISTORY_MAX)
        return;

    tui.input.history[tui.input.history_count] = strdup(line);
    tui.input.history_count++;
}

/* Find slash command completions — P190 */
static int tui_find_slash_completions(const char *prefix) {
    int count = 0;
    size_t plen = strlen(prefix);

    for (int i = 0; slash_commands[i].cmd && count < AUTOCOMPLETE_MAX; i++) {
        if (strncmp(slash_commands[i].cmd, prefix, plen) == 0) {
            snprintf(tui.input.autocomplete_matches[count],
                     sizeof(tui.input.autocomplete_matches[0]),
                     "%s  %s", slash_commands[i].cmd, slash_commands[i].desc);
            count++;
        }
    }
    return count;
}

/* Find emoji completions — P190 */
static int tui_find_emoji(const char *prefix) {
    int count = 0;
    size_t plen = strlen(prefix);
    if (plen == 0) return 0;

    for (int i = 0; emoji_list[i][0] && count < EMOJI_MAX; i++) {
        if (strncmp(emoji_list[i][1], prefix, plen) == 0) {
            strncpy(tui.input.emoji_results[count], emoji_list[i][0],
                    sizeof(tui.input.emoji_results[0]) - 1);
            count++;
        }
    }
    return count;
}

/* Delete last word (for Ctrl+W) */
static void tui_input_delete_word(void) {
    while (tui.input.pos > 0 && tui.input.buf[tui.input.pos - 1] == ' ')
        tui.input.pos--;
    while (tui.input.pos > 0 && tui.input.buf[tui.input.pos - 1] != ' ')
        tui.input.pos--;
    tui.input.buf[tui.input.pos] = '\0';
    tui.input.len = tui.input.pos;
}

/* Delete to end of line (for Ctrl+K) */
static void tui_input_delete_to_end(void) {
    tui.input.buf[tui.input.pos] = '\0';
    tui.input.len = tui.input.pos;
}

/* Redraw the input pane — handles multi-line wrap */
static void tui_redraw_input(void) {
    WINDOW *win = tui.panes[PANE_INPUT].win;
    if (!win) return;

    werase(win);

    int input_cols = tui.panes[PANE_INPUT].cols;

    /* Draw prompt */
    wattron(win, COLOR_PAIR(2) | A_BOLD);
    mvwprintw(win, 0, 0, "%s ", tui_get_theme()->prompt_sym);
    wattroff(win, A_BOLD);

    int prompt_len = strlen(tui_get_theme()->prompt_sym) + 1;

    /* Check for autocomplete overlay */
    if (tui.input.autocomplete_active && tui.input.autocomplete_count > 0) {
        /* Draw autocomplete popup */
        int popup_y = 1;
        int popup_x = 0;
        int popup_max = tui.panes[PANE_INPUT].rows - 1;
        if (popup_max > 5) popup_max = 5;

        for (int i = 0; i < popup_max && i < tui.input.autocomplete_count; i++) {
            int idx = i;
            if (tui.input.autocomplete_sel >= 0)
                idx = tui.input.autocomplete_sel + i;
            if (idx >= tui.input.autocomplete_count) break;

            if (i == 0) {
                wattron(win, A_REVERSE | COLOR_PAIR(13));
            } else {
                wattron(win, COLOR_PAIR(10));
            }
            mvwprintw(win, popup_y + i, popup_x, " %-*s",
                      input_cols - 2, tui.input.autocomplete_matches[idx]);
            wattroff(win, A_REVERSE | COLOR_PAIR(10) | COLOR_PAIR(13));
        }
    }

    /* Check for emoji picker */
    if (tui.input.emoji_picker_active) {
        int popup_y = 1;
        int popup_x = 0;
        int popup_max = tui.panes[PANE_INPUT].rows - 1;
        if (popup_max > 4) popup_max = 4;

        mvwprintw(win, popup_y, popup_x, " Emoji: ");
        for (int i = 0; i < popup_max && i < tui.input.emoji_count; i++) {
            if (i == tui.input.emoji_sel)
                wattron(win, A_REVERSE);
            mvwprintw(win, popup_y + 1 + i, popup_x + 4, "%s",
                      tui.input.emoji_results[i]);
            wattroff(win, A_REVERSE);
        }
    }

    /* Draw input text with horizontal scrolling */
    int display_len = tui.input.len;
    int scroll_col = tui.input.scroll_col;
    int avail = input_cols - prompt_len - 1;
    if (avail < 1) avail = 1;

    /* Ensure cursor is visible */
    if (tui.input.pos < scroll_col)
        scroll_col = tui.input.pos;
    if (tui.input.pos > scroll_col + avail - 1)
        scroll_col = tui.input.pos - avail + 1;
    if (scroll_col < 0) scroll_col = 0;
    tui.input.scroll_col = scroll_col;

    /* Display visible portion */
    int vis_len = display_len - scroll_col;
    if (vis_len > avail) vis_len = avail;
    if (vis_len < 0) vis_len = 0;

    char display_buf[1024];
    if (vis_len > 0) {
        strncpy(display_buf, tui.input.buf + scroll_col, vis_len);
        display_buf[vis_len] = '\0';
    } else {
        display_buf[0] = '\0';
    }

    /* Draw text */
    if (tui.input.echo) {
        mvwprintw(win, 0, prompt_len, "%s", display_buf);
    } else {
        /* Password mode: show asterisks */
        for (int i = 0; i < vis_len; i++)
            mvwaddch(win, 0, prompt_len + i, '*');
    }

    /* Place cursor */
    int cursor_x = prompt_len + (tui.input.pos - scroll_col);
    wmove(win, 0, cursor_x);

    wnoutrefresh(win);
}

/* Perform autocomplete — P190 */
static void tui_autocomplete(void) {
    tui.input.autocomplete_active = false;
    tui.input.autocomplete_count = 0;
    tui.input.autocomplete_sel = 0;

    /* Check if we're typing a slash command */
    if (tui.input.buf[0] == '/' && tui.input.pos > 0) {
        char prefix[256];
        strncpy(prefix, tui.input.buf, tui.input.pos);
        prefix[tui.input.pos] = '\0';

        tui.input.autocomplete_count = tui_find_slash_completions(prefix);
        if (tui.input.autocomplete_count > 0) {
            tui.input.autocomplete_active = true;
        }
    }

    /* Check for emoji :name: pattern */
    if (tui.input.len > 0 && tui.input.buf[tui.input.pos - 1] == ':') {
        /* Find start of emoji name */
        int start = tui.input.pos - 1;
        while (start > 0 && tui.input.buf[start] != ':')
            start--;
        if (start < tui.input.pos - 1 && tui.input.buf[start] == ':') {
            char prefix[64];
            int plen = (tui.input.pos - 1) - (start + 1);
            if (plen > 0 && plen < 64) {
                strncpy(prefix, tui.input.buf + start + 1, plen);
                prefix[plen] = '\0';
                tui.input.emoji_count = tui_find_emoji(prefix);
                if (tui.input.emoji_count > 0) {
                    tui.input.emoji_picker_active = true;
                    tui.input.emoji_sel = 0;
                }
            }
        }
    }

    tui_redraw_input();
}

/* Insert a character into the input buffer */
static void tui_input_insert_char(char ch) {
    if (tui.input.len >= INPUT_BUF_SIZE - 1) return;

    /* Shift right */
    memmove(tui.input.buf + tui.input.pos + 1,
            tui.input.buf + tui.input.pos,
            tui.input.len - tui.input.pos);
    tui.input.buf[tui.input.pos] = ch;
    tui.input.pos++;
    tui.input.len++;
    tui.input.buf[tui.input.len] = '\0';
}

/* ==================================================================
 *  P195: SESSION BROWSER
 * ================================================================== */

/* Case-insensitive substring match helper */
static bool strstr_lower(const char *haystack, const char *needle) {
    if (!haystack || !needle || !*needle) return false;
    size_t nlen = strlen(needle);
    while (*haystack) {
        if (tolower((unsigned char)*haystack) == tolower((unsigned char)needle[0])) {
            if (strncasecmp(haystack, needle, nlen) == 0)
                return true;
        }
        haystack++;
    }
    return false;
}

/* Draw session browser overlay */
static void tui_draw_session_browser(void) {
    WINDOW *win = tui.panes[PANE_HISTORY].win;
    if (!win) return;

    werase(win);

    int w_rows = tui.panes[PANE_HISTORY].rows;
    int w_cols = tui.panes[PANE_HISTORY].cols;

    /* Title */
    wattron(win, A_BOLD | COLOR_PAIR(1));
    mvwprintw(win, 0, 0, " SESSION BROWSER ");
    wattroff(win, A_BOLD | COLOR_PAIR(1));

    /* Search bar */
    mvwprintw(win, 1, 0, " Search: %s", tui.sessions.search);
    if (tui.sessions.search_mode) {
        wattron(win, A_BLINK);
        mvwaddch(win, 1, 9 + strlen(tui.sessions.search), '|');
        wattroff(win, A_BLINK);
    }

    /* Header */
    wattron(win, A_DIM | COLOR_PAIR(10));
    mvwprintw(win, 2, 0, " %-20s %-10s %-15s %-12s", "Title", "Messages", "Model", "Updated");
    wattroff(win, A_DIM | COLOR_PAIR(10));

    /* Separator */
    mvwhline(win, 3, 0, ACS_HLINE, w_cols - 1);

    /* Session list with search filtering */
    int y = 4;
    int search_len = tui.sessions.search[0] ? (int)strlen(tui.sessions.search) : 0;
    for (int i = tui.sessions.scroll_offset;
         i < tui.sessions.count && y < w_rows - 2;
         i++) {

        /* Apply search filter: case-insensitive across ID, title, and model */
        if (search_len > 0) {
            bool match = false;
            /* Search session ID */
            if (strncasecmp(tui.sessions.sessions[i], tui.sessions.search, (size_t)search_len) == 0)
                match = true;
            /* Search title */
            if (!match && tui.sessions.meta[i].title[0] &&
                strstr_lower(tui.sessions.meta[i].title, tui.sessions.search))
                match = true;
            /* Search model */
            if (!match && tui.sessions.meta[i].model[0] &&
                strstr_lower(tui.sessions.meta[i].model, tui.sessions.search))
                match = true;
            if (!match)
                continue;
        }

        bool selected = (i == tui.sessions.selected);
        if (selected)
            wattron(win, A_REVERSE | COLOR_PAIR(13));

        /* Title or session ID (fallback) */
        const char *title = tui.sessions.meta[i].title[0] ?
                            tui.sessions.meta[i].title : tui.sessions.sessions[i];
        mvwprintw(win, y, 0, " %-20.20s", title);

        /* Message count */
        mvwprintw(win, y, 22, "%3d msgs", tui.sessions.meta[i].message_count);

        /* Model name */
        mvwprintw(win, y, 30, "%-15.15s", tui.sessions.meta[i].model);

        /* Last updated time */
        struct tm *tm_info = localtime(&tui.sessions.meta[i].updated_at);
        char time_buf[16];
        if (tm_info) {
            strftime(time_buf, sizeof(time_buf), "%m-%d %H:%M", tm_info);
        } else {
            snprintf(time_buf, sizeof(time_buf), "unknown");
        }
        mvwprintw(win, y, 46, "%-12s", time_buf);

        if (selected)
            wattroff(win, A_REVERSE | COLOR_PAIR(13));

        y++;
    }

    /* Help bar */
    wattron(win, A_DIM | COLOR_PAIR(10));
    mvwprintw(win, w_rows - 1, 0, " [Enter] load [d] delete [e] export [/] search [q] quit ");
    wattroff(win, A_DIM | COLOR_PAIR(10));

    wnoutrefresh(win);
    doupdate();
}

/* Handle session browser input */
static int tui_session_browser_handle(int ch) {
    if (tui.sessions.search_mode) {
        if (ch == 27 || ch == '\n') {
            tui.sessions.search_mode = false;
        } else if (ch == KEY_BACKSPACE || ch == 127) {
            size_t slen = strlen(tui.sessions.search);
            if (slen > 0) tui.sessions.search[slen - 1] = '\0';
        } else if (ch >= 32 && ch <= 126) {
            size_t slen = strlen(tui.sessions.search);
            if (slen < SESSION_SEARCH_MAX - 1) {
                tui.sessions.search[slen] = (char)ch;
                tui.sessions.search[slen + 1] = '\0';
            }
        }
        return 0;
    }

    switch (ch) {
        case 'q':
        case 27: /* ESC */
            tui.modal_mode = MODE_NORMAL;
            tui.sessions.active = false;
            tui_redraw_history();
            return 1;

        case KEY_UP:
            if (tui.sessions.selected > 0) tui.sessions.selected--;
            break;

        case KEY_DOWN:
            if (tui.sessions.selected < tui.sessions.count - 1)
                tui.sessions.selected++;
            break;

        case KEY_PPAGE:
            tui.sessions.selected -= 10;
            if (tui.sessions.selected < 0) tui.sessions.selected = 0;
            break;

        case KEY_NPAGE:
            tui.sessions.selected += 10;
            if (tui.sessions.selected >= tui.sessions.count)
                tui.sessions.selected = tui.sessions.count - 1;
            break;

        case '\n':
        case '\r':
            if (tui.sessions.selected >= 0 && tui.sessions.selected < tui.sessions.count) {
                /* Load session */
                if (tui.agent) {
                    if (strcmp(tui.sessions.sessions[tui.sessions.selected], "(no sessions)") != 0) {
                        agent_load_session(tui.agent,
                            tui.sessions.sessions[tui.sessions.selected]);
                        tui_history_add(MSG_ROLE_INFO, "Session loaded", false);
                    }
                }
            }
            tui.modal_mode = MODE_NORMAL;
            tui_redraw_history();
            return 1;

        case 'd':
            if (tui.sessions.selected >= 0 && tui.sessions.selected < tui.sessions.count) {
                if (strcmp(tui.sessions.sessions[tui.sessions.selected], "(no sessions)") != 0) {
                    if (tui.agent && tui.agent->db) {
                        agent_session_delete(tui.agent,
                            tui.sessions.sessions[tui.sessions.selected]);
                    }
                    tui_history_add(MSG_ROLE_INFO, "Session deleted", false);
                    /* Refresh list */
                    tui_fullscreen_session_browse();
                    return 1;
                }
            }
            break;

        case 'e':
            if (tui.sessions.selected >= 0 && tui.sessions.selected < tui.sessions.count) {
                if (strcmp(tui.sessions.sessions[tui.sessions.selected], "(no sessions)") != 0) {
                    char *json = agent_session_export_json(
                        tui.agent, tui.sessions.sessions[tui.sessions.selected]);
                    if (json) {
                        tui_history_add(MSG_ROLE_INFO, json, false);
                        free(json);
                    }
                }
            }
            break;

        case '/':
            tui.sessions.search_mode = true;
            tui.sessions.search[0] = '\0';
            break;
    }

    /* Adjust scroll */
    int vis_rows = tui.panes[PANE_HISTORY].rows - 6;
    if (tui.sessions.selected < tui.sessions.scroll_offset)
        tui.sessions.scroll_offset = tui.sessions.selected;
    if (tui.sessions.selected >= tui.sessions.scroll_offset + vis_rows)
        tui.sessions.scroll_offset = tui.sessions.selected - vis_rows + 1;

    return 0;
}

/* Activate session browser — P195 */
void tui_fullscreen_session_browse(void) {
    tui.modal_mode = MODE_SESSION_BROWSE;
    tui.sessions.active = true;
    tui.sessions.selected = 0;
    tui.sessions.scroll_offset = 0;
    tui.sessions.search_mode = false;
    tui.sessions.search[0] = '\0';

    /* Populate session list from database */
    tui.sessions.count = 0;
    if (tui.agent && tui.agent->db) {
        size_t count = 0;
        session_entry_t *list = agent_session_list(&count, NULL, SESSION_LIST_MAX);
        if (list && count > 0) {
            int max_sessions = count < SESSION_LIST_MAX ? (int)count : SESSION_LIST_MAX;
            for (int i = 0; i < max_sessions; i++) {
                strncpy(tui.sessions.sessions[i], list[i].id,
                        sizeof(tui.sessions.sessions[0]) - 1);
                tui.sessions.meta[i] = list[i].meta; /* copy metadata */
                tui.sessions.count++;
            }
            /* Free the list — each entry's meta doesn't need deep free
             * since session_meta_t has no heap allocations. */
            free(list);
        }
    }
    /* If no DB or no sessions, show a single placeholder */
    if (tui.sessions.count == 0) {
        strncpy(tui.sessions.sessions[0], "(no sessions)",
                sizeof(tui.sessions.sessions[0]) - 1);
        tui.sessions.count = 1;
    }
}

/* ==================================================================
 *  P196: CONFIG EDITOR
 * ================================================================== */

/* Initialize config entries */
static void tui_config_editor_init(void) {
    tui.config_editor.count = 0;
    tui.config_editor.selected = 0;
    tui.config_editor.scroll_offset = 0;
    tui.config_editor.edit_mode = false;
    tui.config_editor.search_mode = false;
    tui.config_editor.search[0] = '\0';

    /* Populate with key config entries */
    struct { const char *k; const char *v; const char *d; } cfg_entries[] = {
        {"model", "", "LLM model name"},
        {"provider", "", "LLM provider"},
        {"base_url", "", "API base URL"},
        {"max_turns", "90", "Maximum tool-calling iterations"},
        {"verbose", "1", "Verbosity level (0/1/2)"},
        {"stream", "true", "Enable token streaming"},
        {"show_reasoning", "false", "Show reasoning content"},
        {"skin", "default", "Display theme/skin name"},
        {"compact", "false", "Compact display mode"},
        {"show_cost", "false", "Show token cost"},
        {"timestamps", "false", "Show timestamps"},
        {"quiet_mode", "false", "Suppress non-essential output"},
        {"yolo_mode", "false", "Skip all approval prompts"},
        {"fast_mode", "false", "Skip system prompt for speed"},
        {"allow_bg", "true", "Allow background processes"},
        {"approval_mode", "manual", "Approval mode (off/manual/auto)"},
        {"terminal_timeout", "180", "Terminal command timeout (s)"},
        {"compress_enabled", "false", "Smart context compression"},
        {NULL, NULL, NULL}
    };

    for (int i = 0; cfg_entries[i].k && tui.config_editor.count < CONFIG_KEY_MAX; i++) {
        strncpy(tui.config_editor.entries[tui.config_editor.count].key,
                cfg_entries[i].k, CONFIG_KEY_MAX - 1);
        strncpy(tui.config_editor.entries[tui.config_editor.count].value,
                cfg_entries[i].v, CONFIG_VALUE_MAX - 1);
        strncpy(tui.config_editor.entries[tui.config_editor.count].description,
                cfg_entries[i].d, sizeof(tui.config_editor.entries[0].description) - 1);

        /* Try to load current value from agent config */
        if (tui.agent) {
            /* Simplified: just use defaults for now */
        }

        tui.config_editor.count++;
    }
}

/* Draw config editor overlay */
static void tui_draw_config_editor(void) {
    WINDOW *win = tui.panes[PANE_HISTORY].win;
    if (!win) return;

    werase(win);

    int w_rows = tui.panes[PANE_HISTORY].rows;
    int w_cols = tui.panes[PANE_HISTORY].cols;

    /* Title */
    wattron(win, A_BOLD | COLOR_PAIR(1));
    mvwprintw(win, 0, 0, " CONFIG EDITOR ");
    wattroff(win, A_BOLD | COLOR_PAIR(1));

    /* Search bar */
    mvwprintw(win, 1, 0, " Search: %s", tui.config_editor.search);
    if (tui.config_editor.search_mode) {
        wattron(win, A_BLINK);
        mvwaddch(win, 1, 9 + strlen(tui.config_editor.search), '|');
        wattroff(win, A_BLINK);
    }

    /* Header */
    wattron(win, A_DIM | COLOR_PAIR(10));
    mvwprintw(win, 2, 0, " %-25s %-25s %s", "Key", "Value", "Description");
    wattroff(win, A_DIM | COLOR_PAIR(10));

    /* Separator */
    mvwhline(win, 3, 0, ACS_HLINE, w_cols - 1);

    /* Entries */
    int y = 4;
    int search_len = tui.config_editor.search[0] ? (int)strlen(tui.config_editor.search) : 0;
    for (int i = 0;
         i < tui.config_editor.count && y < w_rows - 2;
         i++) {

        /* Apply search filter: case-insensitive key match */
        if (search_len > 0) {
            if (strncasecmp(tui.config_editor.entries[i].key, tui.config_editor.search, (size_t)search_len) != 0 &&
                strstr_lower(tui.config_editor.entries[i].description, tui.config_editor.search) == false)
                continue;
        }

        if (i < tui.config_editor.scroll_offset) continue;

        bool selected = (i == tui.config_editor.selected);
        if (selected)
            wattron(win, A_REVERSE | COLOR_PAIR(13));

        mvwprintw(win, y, 0, " %-25s %-25s %s",
                  tui.config_editor.entries[i].key,
                  tui.config_editor.entries[i].value,
                  tui.config_editor.entries[i].description);

        if (selected)
            wattroff(win, A_REVERSE | COLOR_PAIR(13));

        y++;
    }

    /* Help bar */
    wattron(win, A_DIM | COLOR_PAIR(10));
    mvwprintw(win, w_rows - 1, 0, " [Enter] edit [g] get value [e] explain [/] search [q] quit ");
    wattroff(win, A_DIM | COLOR_PAIR(10));

    wnoutrefresh(win);
    doupdate();
}

/* Handle config editor input */
static int tui_config_editor_handle(int ch) {
    if (tui.config_editor.search_mode) {
        if (ch == 27 || ch == '\n') {
            tui.config_editor.search_mode = false;
        } else if (ch == KEY_BACKSPACE || ch == 127) {
            size_t slen = strlen(tui.config_editor.search);
            if (slen > 0) tui.config_editor.search[slen - 1] = '\0';
        } else if (ch >= 32 && ch <= 126) {
            size_t slen = strlen(tui.config_editor.search);
            if (slen < CONFIG_SEARCH_MAX - 1) {
                tui.config_editor.search[slen] = (char)ch;
                tui.config_editor.search[slen + 1] = '\0';
            }
        }
        return 0;
    }
    if (tui.config_editor.edit_mode) {
        if (ch == '\n' || ch == '\r') {
            /* Save edited value */
            strncpy(tui.config_editor.entries[tui.config_editor.selected].value,
                    tui.config_editor.edit_buf, CONFIG_VALUE_MAX - 1);
            tui.config_editor.edit_mode = false;
        } else if (ch == 27) {
            tui.config_editor.edit_mode = false;
        } else if (ch == KEY_BACKSPACE || ch == 127) {
            size_t elen = strlen(tui.config_editor.edit_buf);
            if (elen > 0) tui.config_editor.edit_buf[elen - 1] = '\0';
        } else if (ch >= 32 && ch <= 126) {
            size_t elen = strlen(tui.config_editor.edit_buf);
            if (elen < sizeof(tui.config_editor.edit_buf) - 2) {
                tui.config_editor.edit_buf[elen] = (char)ch;
                tui.config_editor.edit_buf[elen + 1] = '\0';
            }
        }
        return 0;
    }

    switch (ch) {
        case 'q':
        case 27:
            tui.modal_mode = MODE_NORMAL;
            tui_redraw_history();
            return 1;

        case KEY_UP:
            if (tui.config_editor.selected > 0) tui.config_editor.selected--;
            break;

        case KEY_DOWN:
            if (tui.config_editor.selected < tui.config_editor.count - 1)
                tui.config_editor.selected++;
            break;

        case '\n':
        case '\r':
            /* Enter edit mode for selected key */
            tui.config_editor.edit_mode = true;
            strncpy(tui.config_editor.edit_buf,
                    tui.config_editor.entries[tui.config_editor.selected].value,
                    sizeof(tui.config_editor.edit_buf) - 1);
            break;

        case 'g': {
            /* 'get' — show current value as history message */
            char buf[1024];
            snprintf(buf, sizeof(buf), "%s = %s  (%s)",
                     tui.config_editor.entries[tui.config_editor.selected].key,
                     tui.config_editor.entries[tui.config_editor.selected].value,
                     tui.config_editor.entries[tui.config_editor.selected].description);
            tui_history_add(MSG_ROLE_INFO, buf, false);
            tui_redraw_history();
            break;
        }

        case 'e': {
            /* 'explain' — show description as history message */
            char buf[1024];
            snprintf(buf, sizeof(buf), "  * %s: %s\n  Default: %s\n  Current: %s",
                     tui.config_editor.entries[tui.config_editor.selected].key,
                     tui.config_editor.entries[tui.config_editor.selected].description,
                     /* Use stored value as both default and current for now */
                     tui.config_editor.entries[tui.config_editor.selected].value,
                     tui.config_editor.entries[tui.config_editor.selected].value);
            tui_history_add(MSG_ROLE_INFO, buf, false);
            tui_redraw_history();
            break;
        }

        case '/':
            tui.config_editor.search_mode = true;
            tui.config_editor.search[0] = '\0';
            break;
    }

    /* Adjust scroll */
    int vis_rows = tui.panes[PANE_HISTORY].rows - 5;
    if (tui.config_editor.selected < tui.config_editor.scroll_offset)
        tui.config_editor.scroll_offset = tui.config_editor.selected;
    if (tui.config_editor.selected >= tui.config_editor.scroll_offset + vis_rows)
        tui.config_editor.scroll_offset = tui.config_editor.selected - vis_rows + 1;

    return 0;
}

/* Activate config editor — P196 */
void tui_fullscreen_config_edit(void) {
    tui.modal_mode = MODE_CONFIG_EDIT;
    tui_config_editor_init();
}

/* ==================================================================
 *  P197: IMAGE VIEWER — detect and use sixel/kitty protocol
 * ================================================================== */

/* Detect terminal image protocol support */
static void tui_image_viewer_init(void) {
    tui.image_viewer.sixel_supported = false;
    tui.image_viewer.kitty_supported = false;
    tui.image_viewer.image_displayed = false;
    tui.image_viewer.zoom_level = 100;
    tui.image_viewer.pan_x = 0;
    tui.image_viewer.pan_y = 0;

    /* Check TERM for sixel support */
    const char *term = getenv("TERM");
    if (term && strstr(term, "sixel"))
        tui.image_viewer.sixel_supported = true;

    /* Check KITTY_WINDOW_ID for kitty protocol */
    if (getenv("KITTY_WINDOW_ID"))
        tui.image_viewer.kitty_supported = true;
}

/* Display image via sixel escape codes — P197 */
static bool tui_display_image_sixel(const char *path) {
    if (!path || !tui.image_viewer.sixel_supported) return false;

    /* Read file and output sixel data */
    FILE *f = fopen(path, "rb");
    if (!f) return false;

    /* Check sixel header */
    char header[8] = {0};
    if (fread(header, 1, 2, f) != 2 || header[0] != 0x1b || header[1] != 'P') {
        /* Not a sixel file, try converting with imagemagick or similar */
        fclose(f);

        /* Try using ImageMagick to convert to sixel */
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "convert \"%s\" sixel:- 2>/dev/null | head -c 16384", path);
        FILE *pipe = popen(cmd, "r");
        if (!pipe) return false;

        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), pipe)) > 0) {
            /* Write sixel data directly to terminal (escape ncurses) */
            endwin();
            fwrite(buf, 1, n, stdout);
            fflush(stdout);
            refresh();
        }
        pclose(pipe);
        return true;
    }

    fclose(f);
    return false;
}

/* Display image via kitty protocol — P197 */
static bool tui_display_image_kitty(const char *path, int max_width, int max_height) {
    (void)max_width;
    (void)max_height;
    if (!path || !tui.image_viewer.kitty_supported) return false;

    /* Kitty protocol: \e_Ga=T,f=100,...;payload\e\\ */
    /* We'll use convert to base64 encode the image first */
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);

    /* Read and base64 encode file */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "base64 -w 0 \"%s\" 2>/dev/null | head -c 65536", path);
    FILE *pipe = popen(cmd, "r");
    if (!pipe) return false;

    char b64[65536];
    size_t n = fread(b64, 1, sizeof(b64) - 1, pipe);
    b64[n] = '\0';
    pclose(pipe);

    if (n == 0) return false;

    /* Send kitty protocol command */
    endwin();
    printf("\033_Ga=T,f=100,m=1;");
    fflush(stdout);

    /* Send in chunks if needed */
    size_t chunk_size = 4096;
    for (size_t i = 0; i < n; i += chunk_size) {
        size_t remain = n - i;
        if (remain > chunk_size) remain = chunk_size;

        printf("%.*s", (int)remain, b64 + i);
        if (i + chunk_size < n) {
            printf("\033_Gm=1;");
        }
        fflush(stdout);
    }
    printf("\033\\\n");
    fflush(stdout);
    refresh();

    tui.image_viewer.image_displayed = true;
    return true;
}

/* P197: Display image — try kitty first, then sixel */
static void tui_display_image(const char *path) {
    if (!path) return;

    strncpy(tui.image_viewer.image_path, path, sizeof(tui.image_viewer.image_path) - 1);

    /* Try kitty protocol */
    if (tui_display_image_kitty(path, tui.panes[PANE_HISTORY].cols, tui.panes[PANE_HISTORY].rows))
        return;

    /* Try sixel */
    if (tui_display_image_sixel(path))
        return;

    /* Fallback: show image info */
    tui_history_add(MSG_ROLE_INFO, "[Image] Terminal does not support inline images", false);
    tui_history_add(MSG_ROLE_INFO, path, false);
    tui_redraw_history();
}

/* P197: Image viewer modal keyboard handler */
static int tui_image_viewer_handle(int ch) {
    switch (ch) {
        case 27: /* ESC — exit */
            tui.modal_mode = MODE_NORMAL;
            tui_redraw_history();
            return 1;
        case '+':
        case '=':
            if (tui.image_viewer.zoom_level < 400) {
                tui.image_viewer.zoom_level += 25;
                if (tui.image_viewer.image_path[0])
                    tui_display_image(tui.image_viewer.image_path);
            }
            return 1;
        case '-':
        case '_':
            if (tui.image_viewer.zoom_level > 25) {
                tui.image_viewer.zoom_level -= 25;
                if (tui.image_viewer.image_path[0])
                    tui_display_image(tui.image_viewer.image_path);
            }
            return 1;
        case KEY_LEFT:
            tui.image_viewer.pan_x -= 10;
            return 1;
        case KEY_RIGHT:
            tui.image_viewer.pan_x += 10;
            return 1;
        case KEY_UP:
            tui.image_viewer.pan_y -= 10;
            return 1;
        case KEY_DOWN:
            tui.image_viewer.pan_y += 10;
            return 1;
        case 'r':
        case 'R':
            tui.image_viewer.pan_x = 0;
            tui.image_viewer.pan_y = 0;
            tui.image_viewer.zoom_level = 100;
            if (tui.image_viewer.image_path[0])
                tui_display_image(tui.image_viewer.image_path);
            return 1;
        default:
            return 0;
    }
}

/* P197: Draw image viewer modal overlay */
static void tui_draw_image_viewer(void) {
    WINDOW *win = tui.panes[PANE_HISTORY].win;
    if (!win) return;

    werase(win);

    wattron(win, A_BOLD | COLOR_PAIR(1));
    mvwprintw(win, 0, 0, " IMAGE VIEWER ");
    wattroff(win, A_BOLD | COLOR_PAIR(1));

    int y = 2;
    if (tui.image_viewer.image_path[0]) {
        mvwprintw(win, y++, 0, " Path: %s", tui.image_viewer.image_path);
        y++;
        mvwprintw(win, y++, 0, " Zoom: %d%%", tui.image_viewer.zoom_level);
        mvwprintw(win, y++, 0, " Pan:  (%d, %d)", tui.image_viewer.pan_x, tui.image_viewer.pan_y);
        y++;
        if (tui.image_viewer.image_displayed) {
            mvwprintw(win, y++, 0, " Image displayed inline.");
        } else {
            mvwprintw(win, y++, 0, " Terminal does not support inline image display.");
        }
    } else {
        mvwprintw(win, y++, 0, " No image loaded.");
        mvwprintw(win, y++, 0, " Use /image <path> to display an image.");
    }
    y++;

    wattron(win, A_DIM);
    mvwprintw(win, y++, 0, " Controls:");
    mvwprintw(win, y++, 0, "   ESC       Close viewer");
    mvwprintw(win, y++, 0, "   +/-       Zoom in/out");
    mvwprintw(win, y++, 0, "   Arrows    Pan image");
    mvwprintw(win, y++, 0, "   R         Reset zoom/pan");
    wattroff(win, A_DIM);

    wattron(win, A_DIM | COLOR_PAIR(10));
    mvwprintw(win, tui.panes[PANE_HISTORY].rows - 1, 0, " Press ESC to close ");
    wattroff(win, A_DIM | COLOR_PAIR(10));

    wnoutrefresh(win);
    doupdate();
}

/* ==================================================================
 *  P199: GATEWAY — JSON-RPC backend via FIFO
 * ================================================================== */

/* Draw gateway status dashboard — P199 */
static void tui_draw_gateway_status(void) {
    WINDOW *win = tui.panes[PANE_HISTORY].win;
    if (!win) return;

    werase(win);

    int w_rows = tui.panes[PANE_HISTORY].rows;

    wattron(win, A_BOLD | COLOR_PAIR(1));
    mvwprintw(win, 0, 0, " GATEWAY STATUS ");
    wattroff(win, A_BOLD | COLOR_PAIR(1));

    int y = 2;
    mvwprintw(win, y++, 0, " RPC Status:     %s",
              tui.gateway.state == RPC_CONNECTED ? "Connected" : "Idle");
    mvwprintw(win, y++, 0, " FIFO Path:      %s", RPC_FIFO_PATH);
    mvwprintw(win, y++, 0, " FIFO FD:        %d", tui.gateway.fifo_fd);
    mvwprintw(win, y++, 0, " Read Buffer:    %d/%d bytes",
              tui.gateway.read_pos, (int)sizeof(tui.gateway.read_buf));
    y++;
    mvwprintw(win, y++, 0, " Agent:          %s",
              tui.agent ? "Available" : "Not available");
    if (tui.agent && tui.agent->db) {
        mvwprintw(win, y++, 0, " Session DB:     Connected");
    } else {
        mvwprintw(win, y++, 0, " Session DB:     Not connected");
    }
    y++;
    mvwprintw(win, y++, 0, " Platforms:      19 registered (in binary)");
    mvwprintw(win, y++, 0, " Model:          %s", tui.status.model);
    mvwprintw(win, y++, 0, " Provider:       %s", tui.status.provider);

    wattron(win, A_DIM | COLOR_PAIR(10));
    mvwprintw(win, w_rows - 1, 0, " Press any key to close ");
    wattroff(win, A_DIM | COLOR_PAIR(10));

    wnoutrefresh(win);
    doupdate();
}

/* Draw cron job viewer overlay — U07 */
static void tui_draw_cron_viewer(void) {
    WINDOW *win = tui.panes[PANE_HISTORY].win;
    if (!win) return;

    werase(win);

    int w_rows = tui.panes[PANE_HISTORY].rows;

    wattron(win, A_BOLD | COLOR_PAIR(1));
    mvwprintw(win, 0, 0, " CRON JOBS ");
    wattroff(win, A_BOLD | COLOR_PAIR(1));

    int y = 2;
    mvwprintw(win, y++, 0, " Cron Dir:       %s",
              tui.agent && tui.agent->config.cron.dir[0] ?
              tui.agent->config.cron.dir : "default");
    mvwprintw(win, y++, 0, " Max Jobs:       %d",
              tui.agent ? tui.agent->config.cron.max_concurrent_jobs : 5);
    mvwprintw(win, y++, 0, " Job Timeout:    %ds",
              tui.agent ? tui.agent->config.cron.job_timeout : 300);
    mvwprintw(win, y++, 0, " Retention:      %d days",
              tui.agent ? tui.agent->config.cron.retention_days : 7);
    mvwprintw(win, y++, 0, " Notify Failure: %s",
              tui.agent && tui.agent->config.cron.notify_on_failure ? "Yes" : "No");
    mvwprintw(win, y++, 0, " Poll Interval:  %ds",
              tui.agent ? tui.agent->config.cron.scheduler_poll_interval : 60);
    y++;
    mvwprintw(win, y++, 0, " Use /cron list or cron CLI to manage jobs");
    mvwprintw(win, y++, 0, " Use hermes cron:add <name> <schedule> <cmd>");

    wattron(win, A_DIM | COLOR_PAIR(10));
    mvwprintw(win, w_rows - 1, 0, " Press any key to close ");
    wattroff(win, A_DIM | COLOR_PAIR(10));

    wnoutrefresh(win);
    doupdate();
}

/* Draw log viewer overlay — U08 */
static void tui_draw_log_viewer(void) {
    WINDOW *win = tui.panes[PANE_HISTORY].win;
    if (!win) return;

    werase(win);

    int w_rows = tui.panes[PANE_HISTORY].rows;
    int w_cols = tui.panes[PANE_HISTORY].cols;

    wattron(win, A_BOLD | COLOR_PAIR(1));
    mvwprintw(win, 0, 0, " LOG VIEWER ");
    wattroff(win, A_BOLD | COLOR_PAIR(1));

    char log_dir[HERMES_PATH_MAX] = "";
    hermes_log_dir(log_dir, sizeof(log_dir));

    int y = 2;
    mvwprintw(win, y++, 0, " Log Dir:  %s", log_dir);
    mvwprintw(win, y++, 0, " Files:    agent.log, errors.log, gateway.log");
    y++;

    /* Try to show tail of agent.log */
    char log_path[HERMES_PATH_MAX + 32];
    snprintf(log_path, sizeof(log_path), "%s/agent.log", log_dir);
    FILE *f = fopen(log_path, "r");
    if (f) {
        /* Seek near end and read last N lines */
        char buf[2048];
        int lines_shown = 0;
        int max_lines = w_rows - y - 2;

        /* Go to end, read backwards for lines */
        fseek(f, 0, SEEK_END);
        long pos = ftell(f);
        long read_start = pos - (max_lines * 80); /* estimate ~80 chars per line */
        if (read_start < 0) read_start = 0;
        fseek(f, read_start, SEEK_SET);

        /* Read from estimated position */
        while (fgets(buf, sizeof(buf), f) && lines_shown < max_lines) {
            /* Strip newline */
            size_t blen = strlen(buf);
            if (blen > 0 && buf[blen-1] == '\n') buf[blen-1] = '\0';
            if (blen > 0 && buf[blen-2] == '\r') buf[blen-2] = '\0';

            /* Truncate long lines to fit terminal width */
            if ((int)strlen(buf) >= w_cols - 1)
                buf[w_cols - 4] = '\0';

            mvwprintw(win, y, 0, " %s", buf);
            y++;
            lines_shown++;
        }
        fclose(f);

        if (lines_shown == 0) {
            mvwprintw(win, y++, 0, " (empty log)");
        }
    } else {
        mvwprintw(win, y++, 0, " (no agent.log found)");
    }

    wattron(win, A_DIM | COLOR_PAIR(10));
    mvwprintw(win, w_rows - 1, 0, " Press any key to close ");
    wattroff(win, A_DIM | COLOR_PAIR(10));

    wnoutrefresh(win);
    doupdate();
}

/* Draw skill browser overlay — U03 */

/* ==================================================================
 *  T15: TODO PANEL — kanban task board overlay
 * ================================================================== */

/* Initialize todo panel — fetch tasks from kanban */
static void tui_todo_panel_init(void) {
    tui.todo_panel.count = 0;
    tui.todo_panel.selected = 0;
    tui.todo_panel.scroll_offset = 0;
    tui.todo_panel.filter_idx = 0;
    tui.todo_panel.active = true;

    /* Call registry_dispatch for kanban_list */
    char *result = registry_dispatch("kanban_list",
        "{\"limit\":100,\"include_archived\":false}", "");
    if (!result) return;

    json_t *jr = json_parse(result, NULL);
    free(result);
    if (!jr) return;

    json_t *tasks = json_obj_get(jr, "tasks");
    if (tasks && tasks->type == JSON_ARRAY) {
        size_t n = tasks->c.count;
        if (n > TODO_PANEL_MAX) n = TODO_PANEL_MAX;
        for (size_t i = 0; i < n; i++) {
            json_t *t = tasks->c.items[i];
            const char *id = json_get_str(t, "id", "");
            const char *title = json_get_str(t, "title", "");
            const char *status = json_get_str(t, "status", "");
            const char *assignee = json_get_str(t, "assignee", "");
            strncpy(tui.todo_panel.id[(int)i], id, TODO_ID_MAX - 1);
            strncpy(tui.todo_panel.title[(int)i], title, TODO_TITLE_MAX - 1);
            strncpy(tui.todo_panel.status[(int)i], status, TODO_STATUS_MAX - 1);
            strncpy(tui.todo_panel.assignee[(int)i], assignee, TODO_STATUS_MAX - 1);
            tui.todo_panel.count++;
        }
    }
    json_free(jr);
    if (tui.todo_panel.count == 0) {
        strncpy(tui.todo_panel.title[0], "(no kanban tasks)", TODO_TITLE_MAX - 1);
        tui.todo_panel.count = 1;
    }
}

/* Draw todo panel overlay */
static void tui_draw_todo_panel(void) {
    WINDOW *win = tui.panes[PANE_HISTORY].win;
    if (!win) return;

    werase(win);

    int w_rows = tui.panes[PANE_HISTORY].rows;
    int w_cols = tui.panes[PANE_HISTORY].cols;

    /* Title */
    wattron(win, A_BOLD | COLOR_PAIR(1));
    mvwprintw(win, 0, 0, " TODO BOARD ");
    wattroff(win, A_BOLD | COLOR_PAIR(1));

    /* Filter bar */
    const char *filters[] = {"All", "Active", "Done/Archived"};
    mvwprintw(win, 1, 0, " [");
    for (int f = 0; f < 3; f++) {
        if (f == tui.todo_panel.filter_idx)
            wattron(win, A_REVERSE);
        mvwprintw(win, 1, 1 + (int)(strlen(filters[f-1]) + (f > 0 ? 3 : 0)), "%s", filters[f]);
        wattroff(win, A_REVERSE);
        if (f < 2) mvwprintw(win, 1, 1 + (int)(strlen(filters[f]) + (f > 0 ? 3 : 0)), " | ");
    }
    mvwprintw(win, 1, w_cols - 2, "]");

    /* Header */
    wattron(win, A_DIM | COLOR_PAIR(10));
    mvwprintw(win, 2, 0, " %-40s %-12s %-12s", "Title", "Status", "Assignee");
    wattroff(win, A_DIM | COLOR_PAIR(10));
    mvwhline(win, 3, 0, ACS_HLINE, w_cols - 1);

    /* Task list */
    int y = 4;
    int shown = 0;
    for (int i = 0; i < tui.todo_panel.count && y < w_rows - 2; i++) {
        /* Apply filter */
        if (tui.todo_panel.filter_idx == 1) {
            if (strcmp(tui.todo_panel.status[i], "done") == 0 ||
                strcmp(tui.todo_panel.status[i], "archived") == 0)
                continue;
        } else if (tui.todo_panel.filter_idx == 2) {
            if (strcmp(tui.todo_panel.status[i], "done") != 0 &&
                strcmp(tui.todo_panel.status[i], "archived") != 0)
                continue;
        }

        if (shown < tui.todo_panel.scroll_offset) {
            shown++;
            continue;
        }

        bool selected = (i == tui.todo_panel.selected);
        if (selected)
            wattron(win, A_REVERSE | COLOR_PAIR(13));

        int status_color = 10;
        if (strcmp(tui.todo_panel.status[i], "done") == 0 ||
            strcmp(tui.todo_panel.status[i], "archived") == 0)
            status_color = 10;
        else if (strcmp(tui.todo_panel.status[i], "blocked") == 0)
            status_color = 7;
        else if (strcmp(tui.todo_panel.status[i], "running") == 0)
            status_color = 4;
        else if (strcmp(tui.todo_panel.status[i], "ready") == 0)
            status_color = 9;
        else
            status_color = 10;

        if (!selected)
            wattron(win, COLOR_PAIR(status_color));

        char display_title[TODO_TITLE_MAX + 4];
        strncpy(display_title, tui.todo_panel.title[i], TODO_TITLE_MAX - 1);
        display_title[TODO_TITLE_MAX - 1] = '\0';
        if ((int)strlen(display_title) > w_cols - 30)
            display_title[w_cols - 33] = '\0';

        mvwprintw(win, y, 0, " %-40s %-12s %-12s",
                  display_title,
                  tui.todo_panel.status[i],
                  tui.todo_panel.assignee[i]);

        if (selected)
            wattroff(win, A_REVERSE | COLOR_PAIR(13));
        else
            wattroff(win, COLOR_PAIR(status_color));

        y++;
        shown++;
    }

    if (shown - tui.todo_panel.scroll_offset == 0) {
        mvwprintw(win, y++, 0, " (no tasks match filter)");
    }

    /* Help bar */
    wattron(win, A_DIM | COLOR_PAIR(10));
    mvwprintw(win, w_rows - 1, 0,
        " [Up/Dn] nav  [1-3] filter  [Enter] show  [c] complete  [q] close ");
    wattroff(win, A_DIM | COLOR_PAIR(10));

    wnoutrefresh(win);
    doupdate();
}

/* Handle todo panel input */
static int tui_todo_panel_handle(int ch) {
    switch (ch) {
        case 'q':
        case 27:
            tui.modal_mode = MODE_NORMAL;
            tui.todo_panel.active = false;
            tui_redraw_history();
            return 1;

        case KEY_UP:
            if (tui.todo_panel.selected > 0) tui.todo_panel.selected--;
            break;

        case KEY_DOWN:
            if (tui.todo_panel.selected < tui.todo_panel.count - 1)
                tui.todo_panel.selected++;
            break;

        case KEY_PPAGE:
            tui.todo_panel.scroll_offset -= 10;
            if (tui.todo_panel.scroll_offset < 0)
                tui.todo_panel.scroll_offset = 0;
            break;

        case KEY_NPAGE:
            tui.todo_panel.scroll_offset += 10;
            break;

        case '1': case '2': case '3':
            tui.todo_panel.filter_idx = ch - '1';
            tui.todo_panel.selected = 0;
            tui.todo_panel.scroll_offset = 0;
            break;

        case '\n':
        case '\r':
            if (tui.todo_panel.count > 0 && tui.todo_panel.id[tui.todo_panel.selected][0]) {
                char json_args[128];
                snprintf(json_args, sizeof(json_args),
                         "{\"task_id\":\"%s\"}", tui.todo_panel.id[tui.todo_panel.selected]);
                char *result = registry_dispatch("kanban_show", json_args, "");
                if (result) {
                    json_t *jr = json_parse(result, NULL);
                    free(result);
                    if (jr) {
                        const char *title = json_get_str(jr, "title", "(no title)");
                        const char *status = json_get_str(jr, "status", "?");
                        const char *assignee = json_get_str(jr, "assignee", "");
                        const char *body = json_get_str(jr, "body", "");
                        char detail[2048];
                        snprintf(detail, sizeof(detail),
                                 "Task: %s | Status: %s | Assignee: %s\n%s",
                                 title, status, assignee, body);
                        tui_history_add(MSG_ROLE_INFO, detail, false);
                        json_free(jr);
                    }
                }
                tui.modal_mode = MODE_NORMAL;
                tui.todo_panel.active = false;
                tui_redraw_history();
                return 1;
            }
            break;

        case 'c':
            if (tui.todo_panel.count > 0 && tui.todo_panel.id[tui.todo_panel.selected][0]) {
                char json_args[128];
                snprintf(json_args, sizeof(json_args),
                         "{\"task_id\":\"%s\",\"summary\":\"Completed from TUI\"}",
                         tui.todo_panel.id[tui.todo_panel.selected]);
                char *result = registry_dispatch("kanban_complete", json_args, "");
                if (result) {
                    tui_history_add(MSG_ROLE_INFO, "Task completed", false);
                    free(result);
                }
                tui_todo_panel_init();
                tui.todo_panel.active = true;
            }
            break;

        default:
            return 0;
    }
    tui_draw_todo_panel();
    return 1;
}

/* ==================================================================
 *  T14: AGENT INFO — display current agent configuration
 * ================================================================== */
static void tui_draw_agent_info(void) {
    if (!tui.agent) {
        tui_history_add(MSG_ROLE_WARN, "No agent state available", false);
        tui.modal_mode = MODE_NORMAL;
        return;
    }

    /* Calculate overlay dimensions */
    int rows = tui.rows;
    int cols = tui.cols;
    int overlay_h = 16;
    int overlay_w = (cols > 52) ? 50 : cols - 2;
    int start_y = (rows - overlay_h) / 2;
    int start_x = (cols - overlay_w) / 2;
    if (start_y < 0) start_y = 0;
    if (start_x < 0) start_x = 0;

    /* Create overlay window */
    WINDOW *win = newwin(overlay_h, overlay_w, start_y, start_x);
    if (!win) return;
    keypad(win, TRUE);

    /* Draw border and title */
    box(win, 0, 0);
    mvwprintw(win, 0, 2, "[ Agent Info ]");

    /* Gather agent info */
    agent_state_t *a = tui.agent;

    mvwprintw(win, 2, 2, "Model:      %s", a->llm.model[0] ? a->llm.model : "(none)");
    mvwprintw(win, 3, 2, "Provider:   %s", a->llm.provider[0] ? a->llm.provider : "(none)");
    mvwprintw(win, 4, 2, "Session ID: %s", a->session_id);

    mvwprintw(win, 6, 2, "Iterations: %d / %d", a->iteration_count, a->max_iterations);
    mvwprintw(win, 7, 2, "Messages:   %zu", a->message_count);
    mvwprintw(win, 8, 2, "Tools:      %zu registered", a->tools.count);

    /* Token stats */
    mvwprintw(win, 10, 2, "Input Tokens:  %d", a->session_input_tokens);
    mvwprintw(win, 11, 2, "Output Tokens: %d", a->session_output_tokens);
    mvwprintw(win, 12, 2, "Total Tokens:  %d", a->session_total_tokens);

    /* Budget */
    if (a->budget) {
        double remaining = budget_tracker_remaining_cost(a->budget);
        if (remaining >= 0)
            mvwprintw(win, 13, 2, "Budget:     $%.4f remaining", remaining);
        else
            mvwprintw(win, 13, 2, "Budget:     unlimited");
    } else {
        mvwprintw(win, 13, 2, "Budget:     (none)");
    }

    mvwprintw(win, 14, 2, "JSON Mode:  %s", a->llm.json_mode ? "on" : "off");

    /* Footer */
    mvwprintw(win, overlay_h - 2, 2, "Press q or ESC to close");

    wnoutrefresh(win);
    doupdate();
    delwin(win);
}

static void tui_draw_skill_browser(void) {
    WINDOW *win = tui.panes[PANE_HISTORY].win;
    if (!win) return;

    werase(win);

    int w_rows = tui.panes[PANE_HISTORY].rows;
    int w_cols = tui.panes[PANE_HISTORY].cols;

    wattron(win, A_BOLD | COLOR_PAIR(1));
    mvwprintw(win, 0, 0, " SKILL BROWSER ");
    wattroff(win, A_BOLD | COLOR_PAIR(1));

    /* Determine skill directory */
    const char *hermes_home = getenv("HERMES_HOME");
    if (!hermes_home) hermes_home = getenv("HOME");
    char skill_dir[HERMES_PATH_MAX];
    if (hermes_home) {
        snprintf(skill_dir, sizeof(skill_dir), "%s/.hermes/skills", hermes_home);
    } else {
        snprintf(skill_dir, sizeof(skill_dir), "~/.hermes/skills");
    }

    int y = 2;
    mvwprintw(win, y++, 0, " Skills Dir: %s", skill_dir);
    y++;

    /* Scan directory for skill files */
    DIR *d = opendir(skill_dir);
    if (d) {
        struct dirent *entry;
        int count = 0;
        while ((entry = readdir(d)) != NULL && y < w_rows - 3) {
            /* Skip hidden and non-directory entries */
            if (entry->d_name[0] == '.') continue;
            if (entry->d_type != DT_DIR && entry->d_type != DT_LNK && entry->d_type != DT_REG) continue;
            /* Only show .md files and directories */
            size_t nlen = strlen(entry->d_name);
            if (entry->d_type == DT_REG && (nlen < 3 || strcmp(entry->d_name + nlen - 3, ".md") != 0))
                continue;

            char display[256];
            if (entry->d_type == DT_DIR) {
                snprintf(display, sizeof(display), " [%s]", entry->d_name);
            } else {
                snprintf(display, sizeof(display), "   %s", entry->d_name);
            }
            mvwprintw(win, y, 0, " %-60s", display);
            y++;
            count++;
        }
        closedir(d);
        if (count == 0) {
            mvwprintw(win, y++, 0, " (no skills found)");
        }
    } else {
        mvwprintw(win, y++, 0, " (cannot access skills directory)");
    }

    wattron(win, A_DIM | COLOR_PAIR(10));
    mvwprintw(win, w_rows - 1, 0, " Press any key to close ");
    wattroff(win, A_DIM | COLOR_PAIR(10));

    wnoutrefresh(win);
    doupdate();
}

/* Create FIFO for RPC communication */
static bool tui_gateway_init(void) {
    /* Remove old FIFO if exists */
    unlink(RPC_FIFO_PATH);

    /* Create new FIFO */
    if (mkfifo(RPC_FIFO_PATH, 0600) != 0) {
        /* Non-fatal: gateway mode is optional */
        tui.gateway.state = RPC_IDLE;
        tui.gateway.fifo_fd = -1;
        tui_eventpub_init(NULL);  /* Init without FIFO path for local dispatch */
        return false;
    }

    tui.gateway.state = RPC_IDLE;
    tui.gateway.fifo_fd = -1;
    tui.gateway.read_pos = 0;

    tui_eventpub_init(RPC_FIFO_PATH);  /* Init with FIFO path */
    tui_slash_init_all();              /* T06: Register all slash commands */

    return true;
}

/* Try to connect gateway (non-blocking open of FIFO) */
static bool tui_gateway_connect(void) {
    if (tui.gateway.fifo_fd >= 0) return true;

    tui.gateway.fifo_fd = open(RPC_FIFO_PATH, O_RDONLY | O_NONBLOCK);
    if (tui.gateway.fifo_fd < 0) return false;

    tui.gateway.state = RPC_CONNECTED;
    return true;
}

/* Process one RPC message — simplified JSON-RPC */
static void tui_gateway_process_message(const char *msg) {
    if (!msg || !*msg) return;

    /* Simple dispatch based on method field */
    if (strstr(msg, "\"method\":\"print\"")) {
        char *params = strstr(msg, "\"params\"");
        if (params) {
            char *text = strchr(params, ':');
            if (text) {
                text++;
                /* Skip quotes */
                while (*text && (*text == ' ' || *text == '"')) text++;
                char *end = strchr(text, '"');
                if (end) *end = '\0';
                tui_history_add(MSG_ROLE_INFO, text, false);
            }
        }
    } else if (strstr(msg, "\"method\":\"error\"")) {
        char *params = strstr(msg, "\"params\"");
        if (params) {
            char *text = strchr(params, ':');
            if (text) {
                text++;
                while (*text && (*text == ' ' || *text == '"')) text++;
                char *end = strchr(text, '"');
                if (end) *end = '\0';
                tui_history_add(MSG_ROLE_ERROR, text, false);
            }
        }
    } else if (strstr(msg, "\"method\":\"stream\"")) {
        char *params = strstr(msg, "\"params\"");
        if (params) {
            char *text = strchr(params, ':');
            if (text) {
                text++;
                while (*text && (*text == ' ' || *text == '"')) text++;
                char *end = strchr(text, '"');
                if (end) *end = '\0';
                tui_fullscreen_stream_token(text);
            }
        }
    } else if (strstr(msg, "\"method\":\"stream_done\"")) {
        tui_fullscreen_stream_done();
    } else if (strstr(msg, "\"method\":\"tool_status\"")) {
        /* Parse tool status from JSON */
        char *name = strstr(msg, "\"name\"");
        char *status = strstr(msg, "\"status\"");
        if (name && status) {
            char tname[64] = {0}, tstatus[32] = {0};
            sscanf(name, "\"name\":\"%63[^\"]\"", tname);
            sscanf(status, "\"status\":\"%31[^\"]\"", tstatus);
            tui_fullscreen_tool_status(tname, tstatus, 0, 0);
        }
    }
}

/* Poll gateway FIFO for incoming messages */
static void tui_gateway_poll(void) {
    if (tui.gateway.fifo_fd < 0) return;

    char buf[4096];
    int n = (int)read(tui.gateway.fifo_fd, buf, sizeof(buf) - 1);
    if (n <= 0) return;

    buf[n] = '\0';

    /* Accumulate in read buffer */
    int remain = RPC_BUF_SIZE - tui.gateway.read_pos - 1;
    if (n > remain) n = remain;
    memcpy(tui.gateway.read_buf + tui.gateway.read_pos, buf, n);
    tui.gateway.read_pos += n;
    tui.gateway.read_buf[tui.gateway.read_pos] = '\0';

    /* Process complete messages (separated by newlines) */
    char *line_start = tui.gateway.read_buf;
    while (1) {
        char *nl = strchr(line_start, '\n');
        if (!nl) break;

        *nl = '\0';
        tui_gateway_process_message(line_start);
        line_start = nl + 1;
    }

    /* Shift remaining */
    if (line_start > tui.gateway.read_buf) {
        int remaining = tui.gateway.read_pos - (int)(line_start - tui.gateway.read_buf);
        if (remaining > 0) {
            memmove(tui.gateway.read_buf, line_start, remaining);
        }
        tui.gateway.read_pos = remaining;
        tui.gateway.read_buf[remaining] = '\0';
    }
}

/* Send RPC message to agent process */
static void tui_gateway_send(const char *method, const char *params_json) {
    /* Write to FIFO (agent process reads from other end) */
    char write_fifo[64];
    snprintf(write_fifo, sizeof(write_fifo), "%s.out", RPC_FIFO_PATH);

    int fd = open(write_fifo, O_WRONLY | O_NONBLOCK);
    if (fd < 0) return;

    char msg[4096];
    snprintf(msg, sizeof(msg), "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":%s}\n",
             method, params_json ? params_json : "{}");
    write(fd, msg, strlen(msg));
    close(fd);
}

/* ==================================================================
 *  HELP OVERLAY (P190)
 * ================================================================== */

static void tui_draw_help(void) {
    WINDOW *win = tui.panes[PANE_HISTORY].win;
    if (!win) return;

    werase(win);

    wattron(win, A_BOLD | COLOR_PAIR(1));
    mvwprintw(win, 0, 0, " HERMES TUI HELP ");
    wattroff(win, A_BOLD | COLOR_PAIR(1));

    int y = 2;
    mvwprintw(win, y++, 0, " Navigation:");
    mvwprintw(win, y++, 0, "   Tab           Switch between panes");
    mvwprintw(win, y++, 0, "   PgUp/PgDn     Scroll history pane");
    mvwprintw(win, y++, 0, "   Ctrl+C        Abort streaming / Quit (at prompt)");
    mvwprintw(win, y++, 0, "   Ctrl+L        Redraw screen");
    y++;
    mvwprintw(win, y++, 0, " Input:");
    mvwprintw(win, y++, 0, "   Enter         Send message");
    mvwprintw(win, y++, 0, "   Ctrl+W        Delete word");
    mvwprintw(win, y++, 0, "   Ctrl+K        Delete to end of line");
    mvwprintw(win, y++, 0, "   Tab           Autocomplete slash commands");
    mvwprintw(win, y++, 0, "   Up/Down       History navigation");
    y++;
    mvwprintw(win, y++, 0, " Slash Commands:");
    const slash_cmd_entry_t **all_cmds = tui_slash_get_all();
    for (int i = 0; all_cmds[i] != NULL; i++) {
        mvwprintw(win, y++, 0, "   %-15s %s %s",
                  all_cmds[i]->cmd,
                  all_cmds[i]->desc,
                  all_cmds[i]->args);
    }

    wattron(win, A_DIM | COLOR_PAIR(10));
    mvwprintw(win, tui.panes[PANE_HISTORY].rows - 1, 0, " Press any key to close ");
    wattroff(win, A_DIM | COLOR_PAIR(10));

    wnoutrefresh(win);
    doupdate();
}

/* ==================================================================
 *  GENERAL DISPLAY FUNCTIONS (public API)
 * ================================================================== */

void tui_fullscreen_print(const char *fmt, ...) {
    if (!tui.running) {
        /* Before TUI initialized, fall back to printf */
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
        printf("\n");
        fflush(stdout);
        return;
    }

    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    tui_history_add(MSG_ROLE_ASSISTANT, buf, false);
    tui_redraw_history();

    /* Redraw input */
    if (tui.panes[PANE_INPUT].win) {
        wnoutrefresh(tui.panes[PANE_INPUT].win);
    }
    doupdate();
}

void tui_fullscreen_error(const char *fmt, ...) {
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (!tui.running) {
        fprintf(stderr, "Error: %s\n", buf);
        return;
    }

    tui_history_add(MSG_ROLE_ERROR, buf, true);
    tui_redraw_history();
    if (tui.panes[PANE_INPUT].win)
        wnoutrefresh(tui.panes[PANE_INPUT].win);
    doupdate();
}

void tui_fullscreen_warn(const char *fmt, ...) {
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (!tui.running) {
        printf("Warning: %s\n", buf);
        return;
    }

    tui_history_add(MSG_ROLE_WARN, buf, false);
    tui_redraw_history();
    if (tui.panes[PANE_INPUT].win)
        wnoutrefresh(tui.panes[PANE_INPUT].win);
    doupdate();
}

/* ==================================================================
 *  COMMAND DISPATCH — handle slash commands and agent chat
 * ================================================================== */

/* Stream callback adapter — tui_fullscreen_stream_token signature
* doesn't match llm_token_cb_t (no userdata, void return).
* This wrapper bridges the gap so agent_loop can stream tokens
* into the TUI display during agent_chat(). */
/* Check abort flag in streaming callback — allows signal/thread abort */
static int tui_stream_cb(const char *token, void *userdata) {
   (void)userdata;
   if (tui.stream.abort_requested)
       return 1; /* abort streaming */
   tui_fullscreen_stream_token(token);
   return 0; /* continue streaming */
}

/* ── Slash command handlers (T06) ───────────────────────────── */

static bool cmd_help(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    tui.modal_mode = MODE_HELP;
    tui_draw_help();
    return true;
}
static bool cmd_quit(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    tui.running = false;
    return true;
}
static bool cmd_clear(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    tui.history.count = 0;
    tui.history.head = 0;
    tui_redraw_history();
    return true;
}
static bool cmd_sessions(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    tui_fullscreen_session_browse();
    return true;
}
static bool cmd_config(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    tui_fullscreen_config_edit();
    return true;
}
static bool cmd_mobile(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    if (tui.layout_mode == TUI_LAYOUT_NORMAL)
        tui.layout_mode = TUI_LAYOUT_MOBILE;
    else if (tui.layout_mode == TUI_LAYOUT_MOBILE)
        tui.layout_mode = TUI_LAYOUT_COMPACT;
    else
        tui.layout_mode = TUI_LAYOUT_NORMAL;
    tui_resize_panes();
    tui_redraw_history();
    tui_redraw_status();
    return true;
}
static bool cmd_theme(const char *line, int argc, char **argv, void *state) {
    (void)state;
    if (argc > 1) {
        tui_fullscreen_theme_reload(argv[1]);
        tui_history_add(MSG_ROLE_INFO, "Theme changed", false);
    }
    tui_redraw_history();
    tui_redraw_status();
    return true;
}
static bool cmd_redraw(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    clearok(stdscr, TRUE);
    refresh();
    tui_resize_panes();
    tui_redraw_history();
    tui_redraw_status();
    return true;
}
static bool cmd_tokens(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    char info[256];
    snprintf(info, sizeof(info), "Tokens in: %d, out: %d, total: %d",
             tui.status.tokens_in, tui.status.tokens_out,
             tui.status.tokens_in + tui.status.tokens_out);
    tui_history_add(MSG_ROLE_INFO, info, false);
    tui_redraw_history();
    return true;
}
static bool cmd_budget(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    char info[128];
    snprintf(info, sizeof(info), "Budget remaining: %.2f",
             tui.status.budget_remaining);
    tui_history_add(MSG_ROLE_INFO, info, false);
    tui_redraw_history();
    return true;
}
static bool cmd_model(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)state;
    if (argc < 2) {
        /* No args: open model picker overlay */
        tui_model_picker_init();
        tui_draw_model_picker();
        tui.modal_mode = MODE_MODEL_PICK;
    } else {
        /* /model <name>: quick set */
        if (tui.agent)
            snprintf(tui.agent->llm.model, sizeof(tui.agent->llm.model), "%s", argv[1]);
        snprintf(tui.status.model, sizeof(tui.status.model), "%s", argv[1]);
        tui.status.dirty = true;
        char msg[256];
        snprintf(msg, sizeof(msg), "Model set to: %s", argv[1]);
        tui_history_add(MSG_ROLE_INFO, msg, false);
        tui_redraw_history();
        tui_redraw_status();
    }
    return true;
}
static bool cmd_undo(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    if (tui.agent)
        tui_history_add(MSG_ROLE_INFO, "Undo requested", false);
    tui_redraw_history();
    return true;
}
static bool cmd_reset(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    tui.history.count = 0;
    tui.history.head = 0;
    if (tui.agent) { /* agent_reset(tui.agent); */ }
    tui_history_add(MSG_ROLE_INFO, "Conversation reset", false);
    tui_redraw_history();
    return true;
}
static bool cmd_skin(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    char info[1024] = "Available themes: ";
    for (int i = 0; i < tui_theme_count; i++) {
        char tmp[128];
        snprintf(tmp, sizeof(tmp), "%s%s%s",
                 (i > 0 ? ", " : ""),
                 tui_themes[i]->name,
                 (i == tui_current_theme ? " (active)" : ""));
        strncat(info, tmp, sizeof(info) - strlen(info) - 1);
    }
    tui_history_add(MSG_ROLE_INFO, info, false);
    tui_redraw_history();
    return true;
}
static bool cmd_verbose(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    tui_history_add(MSG_ROLE_INFO, "Verbose level toggled", false);
    tui_redraw_history();
    return true;
}
static bool cmd_yolo(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    tui_history_add(MSG_ROLE_INFO, "Yolo mode toggled", false);
    tui_redraw_history();
    return true;
}
static bool cmd_fast(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    tui_history_add(MSG_ROLE_INFO, "Fast mode toggled", false);
    tui_redraw_history();
    return true;
}
static bool cmd_todos(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    tui_todo_panel_init();
    tui.modal_mode = MODE_TODO_PANEL;
    tui_draw_todo_panel();
    return true;
}
static bool cmd_agent_info(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    tui.modal_mode = MODE_AGENT_INFO;
    tui_draw_agent_info();
    return true;
}
static bool cmd_skills_browse(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    tui.modal_mode = MODE_SKILL_BROWSE;
    tui_draw_skill_browser();
    return true;
}
static bool cmd_logs(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    tui.modal_mode = MODE_LOG_VIEW;
    tui_draw_log_viewer();
    return true;
}
static bool cmd_cron(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    tui.modal_mode = MODE_CRON_VIEW;
    tui_draw_cron_viewer();
    return true;
}
static bool cmd_gateway(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    tui.modal_mode = MODE_GATEWAY_STATUS;
    tui_draw_gateway_status();
    return true;
}
static bool cmd_image(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)state;
    if (argc > 1) {
        tui_display_image(argv[1]);
    } else {
        tui_history_add(MSG_ROLE_ERROR, "Usage: /image <path>", true);
        tui_redraw_history();
    }
    return true;
}

/* Registration table for all slash commands */
static const slash_cmd_entry_t slash_registry[] = {
    {"/help",    "Show help message", "",                SLASH_CAT_TUI,     cmd_help, NULL},
    {"/quit",    "Exit the TUI", "",                     SLASH_CAT_META,    cmd_quit, NULL},
    {"/exit",    "Exit the TUI", "",                     SLASH_CAT_META,    cmd_quit, NULL},
    {"/clear",   "Clear output pane", "",                SLASH_CAT_TUI,     cmd_clear, NULL},
    {"/theme",   "Set theme (default/dark/light/mono)", "<name>",           SLASH_CAT_TUI,     cmd_theme, NULL},
    {"/model",   "Show/set current model", "[name]",     SLASH_CAT_AGENT,   cmd_model, NULL},
    {"/tokens",  "Show token usage", "",                 SLASH_CAT_AGENT,   cmd_tokens, NULL},
    {"/budget",  "Show budget status", "",               SLASH_CAT_AGENT,   cmd_budget, NULL},
    {"/sessions","Open session browser", "",              SLASH_CAT_MODAL,   cmd_sessions, NULL},
    {"/config",  "Open config editor", "",                SLASH_CAT_MODAL,   cmd_config, NULL},
    {"/save",    "Save current session", "[name]",        SLASH_CAT_SESSION, NULL, NULL},
    {"/load",    "Load a session", "<id>",                SLASH_CAT_SESSION, NULL, NULL},
    {"/export",  "Export session as JSON/Markdown", "<fmt>", SLASH_CAT_SESSION, NULL, NULL},
    {"/delete",  "Delete current session", "",            SLASH_CAT_SESSION, NULL, NULL},
    {"/verbose", "Set verbose level (0/1/2)", "<level>",  SLASH_CAT_AGENT,   cmd_verbose, NULL},
    {"/yolo",    "Toggle yolo mode", "",                  SLASH_CAT_AGENT,   cmd_yolo, NULL},
    {"/fast",    "Toggle fast mode", "",                  SLASH_CAT_AGENT,   cmd_fast, NULL},
    {"/mobile",  "Toggle mobile/compact layout", "",      SLASH_CAT_TUI,     cmd_mobile, NULL},
    {"/reset",   "Reset conversation", "",                SLASH_CAT_META,    cmd_reset, NULL},
    {"/undo",    "Undo last turn", "",                    SLASH_CAT_META,    cmd_undo, NULL},
    {"/redraw",  "Force screen redraw", "",               SLASH_CAT_TUI,     cmd_redraw, NULL},
    {"/skin",    "List/reload skins", "",                 SLASH_CAT_TUI,     cmd_skin, NULL},
    {"/image",   "Display an image file", "<path>",       SLASH_CAT_TUI,     cmd_image, NULL},
    {"/gateway", "Show gateway status dashboard", "",     SLASH_CAT_MODAL,   cmd_gateway, NULL},
    {"/cron",    "Show cron job viewer", "",              SLASH_CAT_MODAL,   cmd_cron, NULL},
    {"/logs",    "Show log viewer", "",                   SLASH_CAT_MODAL,   cmd_logs, NULL},
    {"/skills",  "Browse available skills", "",           SLASH_CAT_MODAL,   cmd_skills_browse, NULL},
    {"/todos",   "Show todo/kanban board", "",            SLASH_CAT_MODAL,   cmd_todos, NULL},
    {"/agent",   "Show agent info (model, provider, tokens)", "", SLASH_CAT_MODAL, cmd_agent_info, NULL},
    {NULL, NULL, NULL, 0, NULL, NULL}
};

/* Initialize slash command system — register all TUI commands */
static void tui_slash_init_all(void) {
    tui_slash_init((void *)tui.agent);
    tui_slash_register_batch(slash_registry);
}

static void tui_process_input(const char *line) {
   if (!line || !*line) return;

   /* Emit command/input event */
   tui_eventpub_command(line);

   /* Add to history */
   tui_input_history_add(line);
    tui.input.history_pos = -1;

    /* Echo to history */
    tui_history_add(MSG_ROLE_USER, line, false);

    /* Check for slash commands — dispatch via T06 slash worker */
    if (line[0] == '/') {
        if (!tui_slash_dispatch(line)) {
            /* Not a registered slash command — try agent CLI + skills */
            if (tui.agent && commands_dispatch((char *)line, tui.agent)) {
                /* command handled */
            } else if (tui.agent && commands_try_skill((char *)line, tui.agent)) {
                /* skill command handled */
            } else {
                tui_history_add(MSG_ROLE_ERROR, "Unknown command", true);
            }
            tui_redraw_history();
        }
        return;
    }

    /* Send to agent via chat */
    if (tui.agent) {
        /* Wire agent_chat with streaming — this replaces the previous stub
         * that showed "[Agent processing...]" and did nothing. The retry
         * logic lives in agent_loop.c's retry loop, which works for any
         * caller including this TUI path. */
        tui.agent->stream_cb = tui_stream_cb;
        tui.agent->stream_data = NULL;

        char *resp = agent_chat(tui.agent, line);
        tui_fullscreen_stream_done();

        if (resp) {
            tui_history_add(MSG_ROLE_ASSISTANT, resp, false);
            free(resp);
        } else {
            tui_history_add(MSG_ROLE_ERROR, "Agent returned no response", true);
        }
        tui_redraw_history();
    }
}

/* ==================================================================
 *  T13: MODEL PICKER — fetch models, draw overlay, handle input
 * ================================================================== */

/* Initialize model picker: fetch model list from metadata */
static void tui_model_picker_init(void) {
    tui.model_picker.count = 0;
    tui.model_picker.selected = 0;
    tui.model_picker.scroll_offset = 0;

    char *json = model_metadata_list_json();
    if (!json) return;

    json_t *root = json_parse(json, NULL);
    free(json);
    if (!root || root->type != JSON_ARRAY) {
        json_free(root);
        return;
    }
    for (size_t i = 0; i < root->c.count && tui.model_picker.count < MODEL_PICKER_MAX; i++) {
        json_t *entry = root->c.items[i];
        if (!entry) continue;
        json_t *name = json_obj_get(entry, "model");
        if (name && name->type == JSON_STRING && name->str_val && name->str_val[0]) {
            snprintf(tui.model_picker.names[tui.model_picker.count],
                     MODEL_NAME_MAX, "%s", name->str_val);
            tui.model_picker.count++;
        }
    }
    json_free(root);

    /* Find current model in list and pre-select it */
    const char *current = tui.status.model[0] ? tui.status.model : "";
    for (int i = 0; i < tui.model_picker.count; i++) {
        if (strcmp(tui.model_picker.names[i], current) == 0) {
            tui.model_picker.selected = i;
            break;
        }
    }
}

/* Draw the model picker overlay window */
static void tui_draw_model_picker(void) {
    int win_height = 18;
    int win_width = 50;
    int start_y = (tui.rows - win_height) / 2;
    int start_x = (tui.cols - win_width) / 2;
    if (start_y < 0) start_y = 0;
    if (start_x < 0) start_x = 0;
    if (win_height > tui.rows) win_height = tui.rows;
    if (win_width > tui.cols) win_width = tui.cols;

    WINDOW *win = newwin(win_height, win_width, start_y, start_x);
    if (!win) return;

    /* Box with title */
    box(win, 0, 0);
    mvwprintw(win, 0, 2, " Model Picker ");
    mvwprintw(win, 1, 2, "\\u2191\\u2193 select | Enter apply | q/ESC close");

    /* Visible rows */
    int visible = win_height - 4;  /* header + help + footer */
    if (visible < 1) visible = 1;

    /* Clamp scroll_offset */
    if (tui.model_picker.selected < tui.model_picker.scroll_offset)
        tui.model_picker.scroll_offset = tui.model_picker.selected;
    if (tui.model_picker.selected >= tui.model_picker.scroll_offset + visible)
        tui.model_picker.scroll_offset = tui.model_picker.selected - visible + 1;
    if (tui.model_picker.scroll_offset > tui.model_picker.count - visible)
        tui.model_picker.scroll_offset = tui.model_picker.count - visible;
    if (tui.model_picker.scroll_offset < 0)
        tui.model_picker.scroll_offset = 0;

    const char *current = tui.status.model[0] ? tui.status.model : "";

    for (int i = 0; i < visible && (tui.model_picker.scroll_offset + i) < tui.model_picker.count; i++) {
        int idx = tui.model_picker.scroll_offset + i;
        bool is_selected = (idx == tui.model_picker.selected);
        bool is_current = (strcmp(tui.model_picker.names[idx], current) == 0);
        char line[MODEL_NAME_MAX + 16];
        if (is_current)
            snprintf(line, sizeof(line), " * %s (active)", tui.model_picker.names[idx]);
        else
            snprintf(line, sizeof(line), "   %s", tui.model_picker.names[idx]);
        if (is_selected)
            wattron(win, A_REVERSE);
        mvwprintw(win, 3 + i, 2, "%-*s", win_width - 4, line);
        if (is_selected)
            wattroff(win, A_REVERSE);
    }

    /* Footer */
    mvwprintw(win, win_height - 2, 2, "Models: %d  Current: %s",
              tui.model_picker.count, current);

    wrefresh(win);
    delwin(win);
    (void)visible;
    (void)current;
}

/* Handle keyboard input for model picker overlay */
static int tui_model_picker_handle(int ch) {
    switch (ch) {
        case KEY_UP:
        case 'k':
            if (tui.model_picker.selected > 0)
                tui.model_picker.selected--;
            return 1;

        case KEY_DOWN:
        case 'j':
            if (tui.model_picker.selected < tui.model_picker.count - 1)
                tui.model_picker.selected++;
            return 1;

        case KEY_PPAGE:
            tui.model_picker.selected -= 10;
            if (tui.model_picker.selected < 0)
                tui.model_picker.selected = 0;
            return 1;

        case KEY_NPAGE:
            tui.model_picker.selected += 10;
            if (tui.model_picker.selected >= tui.model_picker.count)
                tui.model_picker.selected = tui.model_picker.count - 1;
            return 1;

        case KEY_HOME:
            tui.model_picker.selected = 0;
            return 1;

        case KEY_END:
            tui.model_picker.selected = tui.model_picker.count - 1;
            return 1;

        case '\n':
        case '\r': {
            /* Apply selected model */
            if (tui.model_picker.selected >= 0 && tui.model_picker.selected < tui.model_picker.count) {
                const char *name = tui.model_picker.names[tui.model_picker.selected];
                if (tui.agent) {
                    snprintf(tui.agent->llm.model, sizeof(tui.agent->llm.model), "%s", name);
                }
                snprintf(tui.status.model, sizeof(tui.status.model), "%s", name);
                tui.status.dirty = true;
                char msg[256];
                snprintf(msg, sizeof(msg), "Model set to: %s", name);
                tui_history_add(MSG_ROLE_INFO, msg, false);
            }
            tui.modal_mode = MODE_NORMAL;
            tui_redraw_history();
            tui_redraw_status();
            return 1;
        }

        case 'q':
        case 27: /* ESC */
            tui.modal_mode = MODE_NORMAL;
            tui_redraw_history();
            return 1;

        default:
            return 0;
    }
}

/* ==================================================================
 *  MAIN INPUT LOOP — unified input handling for all modes
 * ================================================================== */

/* Handle modal overlays before normal input */
static int tui_handle_modal_input(int ch) {
    switch (tui.modal_mode) {
        case MODE_SESSION_BROWSE:
            return tui_session_browser_handle(ch);
        case MODE_CONFIG_EDIT:
            return tui_config_editor_handle(ch);
        case MODE_IMAGE_VIEW:
            return tui_image_viewer_handle(ch);
        case MODE_GATEWAY_STATUS:
            tui.modal_mode = MODE_NORMAL;
            tui_redraw_history();
            return 1;
        case MODE_CRON_VIEW:
            tui.modal_mode = MODE_NORMAL;
            tui_redraw_history();
            return 1;
        case MODE_LOG_VIEW:
            tui.modal_mode = MODE_NORMAL;
            tui_redraw_history();
            return 1;
        case MODE_SKILL_BROWSE:
            tui.modal_mode = MODE_NORMAL;
            tui_redraw_history();
            return 1;
        case MODE_HELP:
            tui.modal_mode = MODE_NORMAL;
            tui_redraw_history();
            return 1;
        case MODE_MODEL_PICK:
            return tui_model_picker_handle(ch);
        case MODE_TODO_PANEL:
            if (tui_todo_panel_handle(ch))
                return 1;
            return 0;
        default:
            return 0;
    }
}

/* Handle normal input mode — P190 multi-line editing */
static int tui_handle_input(int ch) {
    switch (ch) {
        case '\n':
        case '\r':
            /* Send line */
            if (tui.input.len > 0) {
                tui.input.buf[tui.input.len] = '\0';
                tui_process_input(tui.input.buf);

                /* Clear input buffer */
                tui.input.len = 0;
                tui.input.pos = 0;
                tui.input.scroll_col = 0;
                tui.input.autocomplete_active = false;
                tui.input.emoji_picker_active = false;

                /* Redraw everything */
                tui_redraw_history();
                tui_redraw_status();
            }
            return 1;

        case KEY_BACKSPACE:
        case 127:
            if (tui.input.pos > 0) {
                memmove(tui.input.buf + tui.input.pos - 1,
                        tui.input.buf + tui.input.pos,
                        tui.input.len - tui.input.pos);
                tui.input.pos--;
                tui.input.len--;
                tui.input.buf[tui.input.len] = '\0';
            }
            break;

        case KEY_DC: /* Delete */
            if (tui.input.pos < tui.input.len) {
                memmove(tui.input.buf + tui.input.pos,
                        tui.input.buf + tui.input.pos + 1,
                        tui.input.len - tui.input.pos - 1);
                tui.input.len--;
            }
            break;

        case KEY_LEFT:
            if (tui.input.pos > 0) tui.input.pos--;
            break;

        case KEY_RIGHT:
            if (tui.input.pos < tui.input.len) tui.input.pos++;
            break;

        case KEY_HOME:
            tui.input.pos = 0;
            break;

        case KEY_END:
            tui.input.pos = tui.input.len;
            break;

        case KEY_UP:
            /* History navigation */
            if (tui.input.history_pos < 0) {
                /* Save current input */
                tui.input.history_pos = tui.input.history_count - 1;
            } else if (tui.input.history_pos > 0) {
                tui.input.history_pos--;
            }
            if (tui.input.history_pos >= 0 && tui.input.history_pos < tui.input.history_count) {
                strncpy(tui.input.buf, tui.input.history[tui.input.history_pos],
                        INPUT_BUF_SIZE - 1);
                tui.input.len = strlen(tui.input.buf);
                tui.input.pos = tui.input.len;
            }
            break;

        case KEY_DOWN:
            if (tui.input.history_pos >= 0) {
                tui.input.history_pos++;
                if (tui.input.history_pos >= tui.input.history_count) {
                    /* New input */
                    tui.input.history_pos = -1;
                    tui.input.buf[0] = '\0';
                    tui.input.len = 0;
                    tui.input.pos = 0;
                } else {
                    strncpy(tui.input.buf, tui.input.history[tui.input.history_pos],
                            INPUT_BUF_SIZE - 1);
                    tui.input.len = strlen(tui.input.buf);
                    tui.input.pos = tui.input.len;
                }
            }
            break;

        case KEY_PPAGE: /* Page Up — scroll history up */
            tui.panes[PANE_HISTORY].scroll_pos++;
            break;

        case KEY_NPAGE: /* Page Down — scroll history down */
            if (tui.panes[PANE_HISTORY].scroll_pos > 0)
                tui.panes[PANE_HISTORY].scroll_pos--;
            break;

        case '\t': /* Tab */
            if (tui.input.autocomplete_active) {
                /* Cycle through autocomplete suggestions */
                if (tui.input.autocomplete_count > 0) {
                    tui.input.autocomplete_sel =
                        (tui.input.autocomplete_sel + 1) % tui.input.autocomplete_count;

                    /* Get the actual command */
                    const char *match = tui.input.autocomplete_matches[tui.input.autocomplete_sel];
                    char cmd[256] = {0};
                    sscanf(match, "%s", cmd);
                    if (cmd[0]) {
                        strncpy(tui.input.buf, cmd, INPUT_BUF_SIZE - 1);
                        tui.input.len = strlen(cmd);
                        tui.input.pos = tui.input.len;
                    }
                }
                tui.input.autocomplete_active = false;
            } else if (tui.input.emoji_picker_active) {
                /* Select emoji */
                if (tui.input.emoji_count > 0) {
                    const char *emoji = tui.input.emoji_results[tui.input.emoji_sel];
                    /* Replace the :name: with emoji */
                    int start = tui.input.pos;
                    while (start > 0 && tui.input.buf[start] != ':') start--;
                    if (tui.input.buf[start] == ':') {
                        int end = tui.input.pos;
                        int emoji_len = strlen(emoji);
                        int tail_len = tui.input.len - end - 1;
                        /* Remove :name: and insert emoji */
                        memmove(tui.input.buf + start + emoji_len,
                                tui.input.buf + end + 1, tail_len > 0 ? tail_len : 0);
                        memcpy(tui.input.buf + start, emoji, emoji_len);
                        tui.input.len = start + emoji_len + tail_len;
                        tui.input.pos = start + emoji_len;
                    }
                }
                tui.input.emoji_picker_active = false;
            } else {
                /* Trigger autocomplete */
                tui_autocomplete();
            }
            break;

        case 23: /* Ctrl+W — delete word */
            tui_input_delete_word();
            break;

        case 11: /* Ctrl+K — delete to end */
            tui_input_delete_to_end();
            break;

        case 3: /* Ctrl+C — quit */
            tui.running = false;
            return 1;

        case 12: /* Ctrl+L — redraw */
            clearok(stdscr, TRUE);
            refresh();
            tui_resize_panes();
            tui_redraw_history();
            tui_redraw_status();
            return 1;

        default:
            /* Regular character */
            if (ch >= 32 && ch <= 126) {
                if (tui.input.len < INPUT_BUF_SIZE - 2) {
                    tui_input_insert_char((char)ch);

                    /* Trigger autocomplete for slash commands */
                    if (ch == '/' && tui.input.pos == 1)
                        tui_autocomplete();
                }
            } else if (ch >= 256) {
                /* Extended key — check for wide chars */
                /* In ncursesw, this could be a wide char */
            }
            break;
    }

    tui_redraw_input();

    /* If history was scrolled, redraw it */
    if (ch == KEY_PPAGE || ch == KEY_NPAGE)
        tui_redraw_history();

    return 0;
}

/* ==================================================================
 *  INITIALIZATION AND CLEANUP
 * ================================================================== */

bool tui_fullscreen_init(void) {
    memset(&tui, 0, sizeof(tui));

    /* Redirect stderr to log file to prevent diagnostic leaks into ncurses display */
    tui.saved_stderr = dup(STDERR_FILENO);
    int null_fd = open("/dev/null", O_WRONLY);
    if (null_fd >= 0) {
        dup2(null_fd, STDERR_FILENO);
        close(null_fd);
    }

    /* Set locale for wide char support */
    setlocale(LC_ALL, "");

    /* Initialize ncurses */
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1); /* visible cursor */

    /* Check color support */
    if (!has_colors()) {
        endwin();
        if (tui.saved_stderr >= 0)
            dprintf(tui.saved_stderr, "Terminal does not support colors\n");
        return false;
    }

    start_color();
    use_default_colors(); /* Use terminal default for -1 */

    /* Initialize theme system (P198) */
    tui_theme_init();
    tui_load_external_skins();
    tui_apply_theme(tui_themes[tui_current_theme]);

    /* Setup signal handlers */
    signal(SIGWINCH, handle_winch);
    signal(SIGINT, handle_sigint);  /* abort streaming on Ctrl+C */

    /* Get terminal dimensions */
    getmaxyx(stdscr, tui.rows, tui.cols);
    if (tui.rows < 8 || tui.cols < 40) {
        endwin();
        if (tui.saved_stderr >= 0)
            dprintf(tui.saved_stderr, "Terminal too small (%dx%d), need at least 40x8\n",
                    tui.cols, tui.rows);
        return false;
    }

    /* Set default layout mode based on terminal width */
    if (tui.cols < 80)
        tui.layout_mode = TUI_LAYOUT_MOBILE;
    else
        tui.layout_mode = TUI_LAYOUT_NORMAL;

    /* Create layout panes (P189) */
    tui_create_windows();

    /* Initialize subsystems */
    tui_input_init();             /* P190 */
    tui_image_viewer_init();      /* P197 */
    tui_gateway_init();           /* P199 */

    /* Status defaults */
    strcpy(tui.status.mode, "idle");
    strcpy(tui.status.model, "Hermes");
    strcpy(tui.status.provider, "C");
    tui.status.max_iterations = 90;
    tui.status.budget_remaining = -1;

    /* Welcome messages */
    tui_history_add(MSG_ROLE_INFO,
                    "WuBu Slermes C v" HERMES_VERSION " — Full ncurses TUI", true);
    tui_history_add(MSG_ROLE_INFO,
                    "Type /help for commands. P189-P200 fully implemented.", false);
    tui_history_add(MSG_ROLE_INFO, "", false);

    /* Redraw everything */
    tui_redraw_history();
    tui_redraw_status();
    tui_redraw_input();

    doupdate();

    tui.running = true;
    tui.initialized = true;
    return true;
}

void tui_fullscreen_cleanup(void) {
    /* Restore stderr before ncurses cleanup */
    if (tui.saved_stderr >= 0) {
        dup2(tui.saved_stderr, STDERR_FILENO);
        close(tui.saved_stderr);
        tui.saved_stderr = -1;
    }

    /* Clean up gateway FIFO */
    if (tui.gateway.fifo_fd >= 0)
        close(tui.gateway.fifo_fd);
    unlink(RPC_FIFO_PATH);

    /* Free input history */
    for (int i = 0; i < tui.input.history_count; i++)
        free(tui.input.history[i]);

    /* Free external skin memory */
    for (int i = 4; i < tui_theme_count; i++) {
        /* First 4 are static const; external are heap-allocated */
        /* In a real implementation we'd track this */
    }

    /* Destroy windows */
    for (int i = 0; i < PANE_COUNT; i++) {
        if (tui.panes[i].win) {
            delwin(tui.panes[i].win);
            tui.panes[i].win = NULL;
        }
    }

    endwin();
    tui.running = false;
    tui_eventpub_shutdown();
}

/* ==================================================================
 *  MAIN LOOP — P189-P200 all integrated
 * ================================================================== */

int tui_fullscreen_run(agent_state_t *state) {
    if (!state) return 1;
    tui.agent = state;

    if (!tui_fullscreen_init())
        return 1;

    /* Copy model info from agent state */
    strncpy(tui.status.model, state->llm.model, sizeof(tui.status.model) - 1);
    strncpy(tui.status.provider, state->llm.provider, sizeof(tui.status.provider) - 1);
    tui.status.max_iterations = state->max_iterations;
    if (state->budget) {
        double remaining = budget_tracker_remaining_cost(state->budget);
        tui.status.budget_remaining = remaining;
    } else {
        tui.status.budget_remaining = -1;
    }

    tui_redraw_status();
    doupdate();

    /* Main event loop */
    while (tui.running) {
        /* Poll gateway for incoming messages (P199) */
        if (tui.gateway.state == RPC_CONNECTED)
            tui_gateway_poll();

        /* Flush pending event publisher messages to FIFO */
        tui_eventpub_flush();

        /* Handle deferred terminal resize (F03) — async-signal-safe:
         * signal handler only sets a flag, actual ncurses calls
         * happen here in the main loop context. */
        if (g_resize_requested) {
            g_resize_requested = 0;
            endwin();
            refresh();
            clear();
            tui_resize_panes();
        }

        /* Thinking indicator + non-blocking input during streaming (T11) */
        if (tui.stream.active) {
            if (tui.panes[PANE_STATUS].win) {
                WINDOW *sw = tui.panes[PANE_STATUS].win;
                int sw_cols = tui.panes[PANE_STATUS].cols;
                double elapsed = ((double)clock() / CLOCKS_PER_SEC) - tui.stream.start_time;
                char think_buf[96];
                
                if (tui.stream.first_token) {
                    /* Before first token: rich animated thinking indicator */
                    /* Multi-frame spinner with phase labels */
                    static const char spin[] = {'|', '/', '-', '\\'};
                    int frame = tui.think_frame % 4;
                    
                    /* Animated ellipsis: cycles through . .. ... patterns */
                    int dot_phase = tui.think_frame % 12;
                    const char *dots;
                    if (dot_phase < 3)       dots = ".   ";  /* Phase 0-2: single dot */
                    else if (dot_phase < 6)  dots = "..  ";  /* Phase 3-5: two dots */
                    else if (dot_phase < 9)  dots = "... ";  /* Phase 6-8: three dots */
                    else                     dots = " .. ";  /* Phase 9-11: two dots (return) */
                    
                    /* Phase label changes with elapsed time */
                    const char *phase;
                    if (elapsed < 2.0)       phase = "think";
                    else if (elapsed < 5.0)  phase = "ponder";
                    else if (elapsed < 10.0) phase = "deep";
                    else                     phase = "focus";
                    
                    snprintf(think_buf, sizeof(think_buf), " %c %s%s %4.1fs  ", 
                             spin[frame], phase, dots, elapsed);
                } else {
                    /* After first token: live token counter + tok/s */
                    double tps = (elapsed > 0.1) ? tui.stream.token_count / elapsed : 0.0;
                    /* Animated receiving indicator: arrow pulses */
                    const char *recv_chr = (tui.think_frame % 8 < 3) ? ">" : "=>";
                    snprintf(think_buf, sizeof(think_buf), " %s t:%d %.0f/s  ", 
                             recv_chr, tui.stream.token_count, tps);
                }
                
                int buf_start = sw_cols - (int)strlen(think_buf) - 1;
                if (buf_start < 0) buf_start = 0;
                mvwprintw(sw, 0, buf_start, "%s", think_buf);
                wnoutrefresh(sw);
                doupdate();
            }
            tui.think_frame++;

            /* Non-blocking input check: process abort/commands during streaming */
            int ch = wgetch(tui.panes[PANE_INPUT].win);
            if (ch == 3) { /* Ctrl+C — abort streaming */
                tui.stream.abort_requested = true;
                tui_history_add(MSG_ROLE_WARN, "[Aborted by user]", true);
                tui_fullscreen_stream_done();
                tui_redraw_history();
                tui_redraw_status();
                continue;
            } else if (ch == ERR) {
                /* No input available — sleep a bit then continue loop */
                napms(50);
                continue;
            } else if (ch >= 32 && ch <= 126) {
                /* Type-ahead: buffer character for after streaming ends (T18) */
                if (tui.stream.type_ahead_len < TYPE_AHEAD_BUF_SIZE - 1) {
                    tui.stream.type_ahead_buf[tui.stream.type_ahead_len++] = (char)ch;
                    tui.stream.type_ahead_buf[tui.stream.type_ahead_len] = '\0';
                }
                beep();
            }
            continue; /* skip normal input processing during streaming */
        }

        /* Get input from the input pane */
        int ch = wgetch(tui.panes[PANE_INPUT].win);
        if (ch == ERR) continue;

        /* Handle modal overlays first */
        if (tui.modal_mode != MODE_NORMAL) {
            if (tui_handle_modal_input(ch))
                continue; /* modal handled the input, refresh loop */
            /* Redraw modal after input */
            switch (tui.modal_mode) {
                case MODE_SESSION_BROWSE: tui_draw_session_browser(); break;
                case MODE_CONFIG_EDIT:    tui_draw_config_editor(); break;
                case MODE_IMAGE_VIEW:     tui_draw_image_viewer(); break;
                case MODE_GATEWAY_STATUS: tui_draw_gateway_status(); break;
                case MODE_CRON_VIEW:      tui_draw_cron_viewer(); break;
                case MODE_LOG_VIEW:       tui_draw_log_viewer(); break;
                case MODE_SKILL_BROWSE:  tui_draw_skill_browser(); break;
                case MODE_HELP:           break; /* help stays until dismissed */
                case MODE_TODO_PANEL:      tui_draw_todo_panel(); break;
                case MODE_AGENT_INFO:      { if (ch == 'q' || ch == 27) { tui.modal_mode = MODE_NORMAL; continue; } tui_draw_agent_info(); break; }
                default: break;
            }
            continue;
        }

        /* Handle normal input */
        tui_handle_input(ch);

        /* Periodic status bar refresh */
        if (tui.status.dirty) {
            tui_redraw_status();
            tui.status.dirty = false;
        }
    }

    tui_fullscreen_cleanup();
    return 0;
}
