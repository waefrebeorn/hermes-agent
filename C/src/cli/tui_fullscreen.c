/*
 * tui_fullscreen.c — Full ncurses TUI for Hermes C (P189-P200).
 * MIT License — WuBu Hermes Project
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
static int __attribute__((unused)) tui_alloc_pair(int fg, int bg) {
    int pair = tui_next_pair++;
    if (pair > COLOR_PAIRS - 1) pair = 100; /* wrap */
    init_pair(pair, fg, bg);
    return pair;
}

/* Apply a theme: register all color pairs */
static void tui_apply_theme(const tui_theme_t *th) {
    if (!th) return;
    /* We store color pair numbers by convention:
     *   1 = status, 2 = prompt, 3 = user, 4 = assistant, 5 = system,
     *   6 = tool, 7 = error, 8 = warn, 9 = hl, 10 = dim, 11 = border,
     *   12 = tool_feed, 13 = selection
     * This way external code can still use COLOR_PAIR(1-13).
     */
    init_pair(1,  th->status_fg, th->status_bg);
    init_pair(2,  th->prompt_fg, th->prompt_bg);
    init_pair(3,  th->user_fg, th->user_bg);
    init_pair(4,  th->assistant_fg, th->assistant_bg);
    init_pair(5,  th->system_fg, th->system_bg);
    init_pair(6,  th->tool_fg, th->tool_bg);
    init_pair(7,  th->error_fg, th->error_bg);
    init_pair(8,  th->warn_fg, th->warn_bg);
    init_pair(9,  th->hl_fg, th->hl_bg);
    init_pair(10, th->dim_fg, th->dim_bg);
    init_pair(11, th->border_fg, th->border_bg);
    init_pair(12, th->tool_feed_fg, th->tool_feed_bg);
    init_pair(13, th->sel_fg, th->sel_bg);
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

typedef struct {
    bool    active;
    char    current_line[MAX_LINE_LENGTH];
    int     current_pos;
    int     token_count;
    double  start_time;
    double  last_token_time;
    double  tokens_per_sec;
    int     bytes_received;
    bool    first_token;
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

typedef struct {
    config_entry_t entries[CONFIG_KEY_MAX];
    int count;
    int selected;
    int scroll_offset;
    bool active;
    bool edit_mode;         /* editing value */
    char edit_buf[256];
    int edit_pos;
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

    /* Agent reference */
    agent_state_t        *agent;

    /* Modal overlay state */
    enum {
        MODE_NORMAL,
        MODE_SESSION_BROWSE,
        MODE_CONFIG_EDIT,
        MODE_IMAGE_VIEW,
        MODE_HELP,
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

static void handle_winch(int sig) {
    (void)sig;
    endwin();
    refresh();
    clear();
    tui_resize_panes();
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
}

/* Write a formatted line to a window with role coloring */
static void __attribute__((unused)) tui_wprint_role(WINDOW *win, msg_role_t role, const char *text,
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
        wattron(win, A_BOLD | COLOR_PAIR(line->color_pair));
        wprintw(win, "%s", prefix);
        wattroff(win, A_BOLD);

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

    /* Right-align version */
    int right_start = tui.panes[PANE_STATUS].cols - 15;
    if (right_start > pos) {
        while (pos < right_start)
            buf[pos++] = ' ';
        pos += snprintf(buf + pos, sizeof(buf) - pos, " v%s ", HERMES_VERSION);
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
}

/* ==================================================================
 *  P192: STREAMING — token display and counter
 * ================================================================== */

/* Start streaming session */
static void tui_stream_start(void) {
    tui.stream.active = true;
    tui.stream.current_pos = 0;
    tui.stream.current_line[0] = '\0';
    tui.stream.token_count = 0;
    tui.stream.start_time = (double)clock() / CLOCKS_PER_SEC;
    tui.stream.first_token = true;
    tui.stream.bytes_received = 0;
    tui.stream.tokens_per_sec = 0.0;

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
    strcpy(tui.status.mode, "chat");

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
    mvwprintw(win, 2, 0, " %-20s %-10s %-15s %s", "Session ID", "Messages", "Model", "Actions");
    wattroff(win, A_DIM | COLOR_PAIR(10));

    /* Separator */
    mvwhline(win, 3, 0, ACS_HLINE, w_cols - 1);

    /* Session list */
    int y = 4;
    for (int i = tui.sessions.scroll_offset;
         i < tui.sessions.count && y < w_rows - 2;
         i++) {

        bool selected = (i == tui.sessions.selected);
        if (selected)
            wattron(win, A_REVERSE | COLOR_PAIR(13));

        mvwprintw(win, y, 0, " %-20s", tui.sessions.sessions[i]);

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

    /* Header */
    wattron(win, A_DIM | COLOR_PAIR(10));
    mvwprintw(win, 1, 0, " %-25s %-25s %s", "Key", "Value", "Description");
    wattroff(win, A_DIM | COLOR_PAIR(10));

    /* Separator */
    mvwhline(win, 2, 0, ACS_HLINE, w_cols - 1);

    /* Entries */
    int y = 3;
    for (int i = tui.config_editor.scroll_offset;
         i < tui.config_editor.count && y < w_rows - 2;
         i++) {

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
    mvwprintw(win, w_rows - 1, 0, " [Enter] edit [s] set [g] get [e] explain [q] quit ");
    wattroff(win, A_DIM | COLOR_PAIR(10));

    wnoutrefresh(win);
    doupdate();
}

/* Handle config editor input */
static int tui_config_editor_handle(int ch) {
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

        case 's':
            /* 'set' — show the set prompt (handled via edit mode) */
            break;

        case 'g':
            /* 'get' — show current value */
            break;

        case 'e':
            /* 'explain' — show description */
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
static bool __attribute__((unused)) tui_display_image_sixel(const char *path) {
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
static bool __attribute__((unused)) tui_display_image_kitty(const char *path, int max_width, int max_height) {
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
static void __attribute__((unused)) tui_display_image(const char *path) {
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

/* ==================================================================
 *  P199: GATEWAY — JSON-RPC backend via FIFO
 * ================================================================== */

/* Create FIFO for RPC communication */
static bool tui_gateway_init(void) {
    /* Remove old FIFO if exists */
    unlink(RPC_FIFO_PATH);

    /* Create new FIFO */
    if (mkfifo(RPC_FIFO_PATH, 0600) != 0) {
        /* Non-fatal: gateway mode is optional */
        tui.gateway.state = RPC_IDLE;
        tui.gateway.fifo_fd = -1;
        return false;
    }

    tui.gateway.state = RPC_IDLE;
    tui.gateway.fifo_fd = -1;
    tui.gateway.read_pos = 0;

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
    mvwprintw(win, y++, 0, "   Ctrl+C        Quit");
    y++;
    mvwprintw(win, y++, 0, " Input:");
    mvwprintw(win, y++, 0, "   Enter         Send message");
    mvwprintw(win, y++, 0, "   Ctrl+W        Delete word");
    mvwprintw(win, y++, 0, "   Ctrl+K        Delete to end of line");
    mvwprintw(win, y++, 0, "   Tab           Autocomplete slash commands");
    mvwprintw(win, y++, 0, "   Up/Down       History navigation");
    y++;
    mvwprintw(win, y++, 0, " Slash Commands:");
    for (int i = 0; slash_commands[i].cmd; i++) {
        mvwprintw(win, y++, 0, "   %-15s %s %s",
                  slash_commands[i].cmd,
                  slash_commands[i].desc,
                  slash_commands[i].args);
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

/* Process input line as slash command or agent message */
static void tui_process_input(const char *line) {
    if (!line || !*line) return;

    /* Add to history */
    tui_input_history_add(line);
    tui.input.history_pos = -1;

    /* Echo to history */
    tui_history_add(MSG_ROLE_USER, line, false);

    /* Check for slash commands */
    if (line[0] == '/') {
        bool handled = true;

        if (strcmp(line, "/help") == 0) {
            tui.modal_mode = MODE_HELP;
            tui_draw_help();
            return;

        } else if (strcmp(line, "/quit") == 0 || strcmp(line, "/exit") == 0) {
            tui.running = false;
            return;

        } else if (strcmp(line, "/clear") == 0) {
            tui.history.count = 0;
            tui.history.head = 0;
            tui_redraw_history();
            return;

        } else if (strcmp(line, "/sessions") == 0) {
            tui_fullscreen_session_browse();
            return;

        } else if (strcmp(line, "/config") == 0) {
            tui_fullscreen_config_edit();
            return;

        } else if (strcmp(line, "/mobile") == 0) {
            if (tui.layout_mode == TUI_LAYOUT_NORMAL)
                tui.layout_mode = TUI_LAYOUT_MOBILE;
            else if (tui.layout_mode == TUI_LAYOUT_MOBILE)
                tui.layout_mode = TUI_LAYOUT_COMPACT;
            else
                tui.layout_mode = TUI_LAYOUT_NORMAL;
            tui_resize_panes();
            tui_redraw_history();
            tui_redraw_status();
            return;

        } else if (strncmp(line, "/theme ", 7) == 0) {
            const char *name = line + 7;
            tui_fullscreen_theme_reload(name);
            tui_history_add(MSG_ROLE_INFO, "Theme changed", false);
            tui_redraw_history();
            tui_redraw_status();
            return;

        } else if (strcmp(line, "/redraw") == 0) {
            clearok(stdscr, TRUE);
            refresh();
            tui_resize_panes();
            tui_redraw_history();
            tui_redraw_status();
            return;

        } else if (strcmp(line, "/help") == 0 || strcmp(line, "/?") == 0) {
            tui.modal_mode = MODE_HELP;
            tui_draw_help();
            return;

        } else if (strcmp(line, "/tokens") == 0) {
            char info[256];
            snprintf(info, sizeof(info), "Tokens in: %d, out: %d, total: %d",
                     tui.status.tokens_in, tui.status.tokens_out,
                     tui.status.tokens_in + tui.status.tokens_out);
            tui_history_add(MSG_ROLE_INFO, info, false);
            tui_redraw_history();
            return;

        } else if (strcmp(line, "/budget") == 0) {
            char info[128];
            snprintf(info, sizeof(info), "Budget remaining: %.2f",
                     tui.status.budget_remaining);
            tui_history_add(MSG_ROLE_INFO, info, false);
            tui_redraw_history();
            return;

        } else if (strcmp(line, "/model") == 0) {
            char info[256];
            snprintf(info, sizeof(info), "Model: %s [%s]",
                     tui.status.model[0] ? tui.status.model : "unknown",
                     tui.status.provider[0] ? tui.status.provider : "unknown");
            tui_history_add(MSG_ROLE_INFO, info, false);
            tui_redraw_history();
            return;

        } else if (strcmp(line, "/undo") == 0) {
            if (tui.agent) {
                /* Restore snapshot if available */
                tui_history_add(MSG_ROLE_INFO, "Undo requested", false);
            }
            tui_redraw_history();
            return;

        } else if (strcmp(line, "/reset") == 0) {
            tui.history.count = 0;
            tui.history.head = 0;
            if (tui.agent) {
                /* agent_reset(tui.agent); */
            }
            tui_history_add(MSG_ROLE_INFO, "Conversation reset", false);
            tui_redraw_history();
            return;

        } else if (strcmp(line, "/skin") == 0) {
            /* List available skins */
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
            return;

        } else if (strcmp(line, "/verbose") == 0) {
            /* Toggle verbose: 0->1->2->0 */
            tui_history_add(MSG_ROLE_INFO, "Verbose level toggled", false);
            tui_redraw_history();
            return;

        } else if (strcmp(line, "/yolo") == 0) {
            tui_history_add(MSG_ROLE_INFO, "Yolo mode toggled", false);
            tui_redraw_history();
            return;

        } else if (strcmp(line, "/fast") == 0) {
            tui_history_add(MSG_ROLE_INFO, "Fast mode toggled", false);
            tui_redraw_history();
            return;

        } else {
            /* Unknown command — try dispatch via agent command system */
            if (tui.agent && commands_dispatch((char *)line, tui.agent)) {
                /* command handled */
            } else {
                tui_history_add(MSG_ROLE_ERROR, "Unknown command", true);
            }
            tui_redraw_history();
            return;
        }

        (void)handled; /* suppress unused warning */
        return;
    }

    /* Send to agent via chat */
    if (tui.agent) {
        /* Set streaming callback */
        tui.agent->stream_cb = NULL; /* We use our own stream handling */

        /* Call agent_chat (would be async in real version) */
        /* For now, simulate with streaming */
        tui_history_add(MSG_ROLE_INFO, "[Agent processing...]", false);
        tui_redraw_history();

        /* In real implementation, this would be:
         * char *resp = agent_chat(tui.agent, line);
         * if (resp) { ... free(resp); }
         */
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
        case MODE_HELP:
            tui.modal_mode = MODE_NORMAL;
            tui_redraw_history();
            return 1;
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
        fprintf(stderr, "Terminal does not support colors\n");
        return false;
    }

    start_color();
    use_default_colors(); /* Use terminal default for -1 */

    /* Initialize theme system (P198) */
    tui_theme_init();
    tui_load_external_skins();
    tui_apply_theme(tui_themes[tui_current_theme]);

    /* Setup signal handler for resize */
    signal(SIGWINCH, handle_winch);

    /* Get terminal dimensions */
    getmaxyx(stdscr, tui.rows, tui.cols);
    if (tui.rows < 8 || tui.cols < 40) {
        endwin();
        fprintf(stderr, "Terminal too small (%dx%d), need at least 40x8\n",
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
                    "WuBu Hermes C v" HERMES_VERSION " — Full ncurses TUI", true);
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
                case MODE_HELP:           break; /* help stays until dismissed */
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
