# Visual Parity Plan: Python → C 1:1

**Date:** 2026-05-23  
**Scope:** C CLI display layer — achieve pixel/terminal-exact visual parity with Python Hermes  
**Codebases:** Python (`hermes-agent-dev/`) → C (`hermes-agent-dev/C/`)

---

## 1. Current State Assessment

### Python (baseline — what we're matching)

| Layer | Technology | Lines |
|-------|-----------|-------|
| Banner | Rich `Panel` + ASCII art logo + hero (caduceus) | `banner.py` (702L) |
| Skin engine | YAML/JSON data-driven, hex colors (#RRGGBB), 27 color slots | `skin_engine.py` (926L) |
| KawaiiSpinner | Threaded `\r`-based animation, kawaii faces, wings, elapsed timer | `display.py` (987L) |
| Tool activity feed | `┊` prefix + emoji + verb + detail + duration (get_cute_tool_message) | `display.py` (987L) |
| Help screen | Rich ANSI markup — bordered panels, categorized command lists | `cli.py` show_help() |
| Markdown in terminal | Rich handles ANSI markdown via Console; Rich markup in streaming | `cli.py` + Rich |
| Fullscreen TUI | React Ink (321 TS files) + Python JSON-RPC gateway | `ui-tui/` + `tui_gateway/` |

### C (current — what needs parity)

| Layer | Technology | Lines | Status |
|-------|-----------|-------|--------|
| Banner | Plain `printf()` text | `main.c` print_banner() (3L) | **Poor** — no ASCII art, no color |
| Banner (CLI) | `display_printf()` with skin color | `cli.c` print_banner() (35L) | **Partial** — ANSI 8-color only, no logo, no hero |
| Skin engine | JSON key-value, 7 named ANSI colors only | `lib/libskin/skin.c` (320L) | **Partial** — no hex/truecolor, 7 keys vs 27 |
| Spinner | `\|/-\` chars + `printf("\r%c %s")` | `display_core.c` (57L) | **Very basic** — no faces, no wings, no timer |
| Tool activity feed | ncurses-only in TUI, no ANSI/CLI mode | `tui_fullscreen.c` tool_feed (80L) | **TUI-only** — no ANSI `┊` output in CLI mode |
| Help screen | Basic `printf()` | `cli.c` --help (30L) | **Poor** — no panels, no categories |
| Markdown in terminal | None in ANSI path; basic in ncurses TUI | `tui_fullscreen.c` (90L) | **TUI-only** — no ANSI markdown rendering |
| Fullscreen ncurses TUI | ncurses, 2,882 LOC, 12 phases planned | `tui_fullscreen.c` (2,882L) | **In progress** — many phases unfinished |
| Truecolor (24-bit) | **Nonexistent** | nowhere | **Missing** — only 8 ANSI colors available |
| ANSI constants | `libansi` header (127L) with 4-bit colors | `lib/libansi/ansi.h` | **Exists** — limited to 16-color |

---

## 2. Gap Analysis

### 2.1 Critical Gaps (block visual parity)

| Gap | Detail |
|-----|--------|
| **No truecolor support** | Python uses `#FFD700`, `#CD7F32` hex colors everywhere. C uses 8 named ANSI colors only. Skin colors like `#FFBF00` (gold) have no C equivalent. |
| **No Rich-style banner** | Python renders a Rich `Panel` with ASCII art logo (6-line "HERMES" in gold block letters), a caduceus hero art (15 lines), and a grid of tools/skills/MCP on the right. C prints "WuBu Hermes v0.14.1". |
| **No KawaiiSpinner** | C spinner uses `\|/-\` chars. Python uses kawaii faces `(｡◕‿◕｡)`, skin-customizable thinking/waiting faces, wing decorations `⟪⚔⚔⟫`, elapsed time display, and `\r`-based animation. |
| **No ANSI tool activity feed** | C has no `┊`-prefixed tool output in CLI mode. Python renders per-tool lines with emoji, verb, detail, duration (e.g., `┊ 🔍 search    query 1.2s`). |
| **No ANSI markdown rendering** | Python's Rich console renders **bold**, `code`, headers, tables, links in streaming output. C `cli_stream_cb()` does `printf("%s", token)` — no formatting. |
| **No Rich-style bordered panels** | Python uses Rich `Panel` for banners, help, error blocks. C `display_panel()` draws box-drawing chars but no colored borders, no title styling. |

### 2.2 Secondary Gaps

| Gap | Detail |
|-----|--------|
| Skin color slots: 7 vs 27 | C: banner, prompt, response, error, dim, tool, thinking (7). Python: banner_border, banner_title, banner_accent, banner_dim, banner_text, ui_accent, ui_label, ui_ok, ui_error, ui_warn, prompt, input_rule, response_border, status_bar_bg/text/strong/dim/good/warn/bad/critical, session_label, session_border, selection_bg, completion_menu* (27). |
| Skin branding slots: 0 vs 7 | C: none. Python: agent_name, welcome, goodbye, response_label, prompt_symbol, help_header, tool_prefix. |
| Skin spinner customization: 0 vs 4 | C: none. Python: waiting_faces, thinking_faces, thinking_verbs, wings. |
| Skin tool_emojis: 0 | C: none. Python: per-tool emoji overrides. |
| Skin format flags: 2 vs 0 | C: banner_bold, show_model. Python: none (uses Rich markup). |
| Skin banner art: 0 vs 2 | C: none. Python: banner_logo (6-line ASCII), banner_hero (15-line ASCII art). |
| Help screen: basic vs Rich | C: plain `printf`. Python: colored bordered box, category headers, skill commands section, tips. |
| Response box | C: none. Python: Rich panel with response_label ("⚕ Hermes") in border. |
| Inline diff rendering | C: none. Python: colorized unified diffs with file header, hunk markers, green/red +/- lines. |
| Horchata (session line) | C: none. Python: OSC 8 clickable session hyperlink. |

---

## 3. Prioritized Implementation Plan

### Phase 1: truecolor + ANSI Markdown (foundation) ⭐
**Effort: Medium (3-5 days)**  
**Files to create/modify:** `lib/libansi/ansi.h`, `lib/libansi/ansi.c`, `src/cli/display_core.c`

1. **Add truecolor (24-bit RGB) support to hermes_display.h**
   - New function: `display_set_fg_rgb(int r, int g, int b)` → `\033[38;2;R;G;Bm`
   - New function: `display_set_bg_rgb(int r, int g, int b)` → `\033[48;2;R;G;Bm`
   - New function: `display_printf_rgb(int r, int g, int b, display_style_t, fmt...)`
   - Helper: `display_hex_color(const char *hex)` — parse `#FFD700` → (255,215,0)

2. **Add ANSI markdown rendering module**
   - New file: `src/cli/ansi_markdown.c` + `include/hermes_ansi_markdown.h`
   - Function: `char *ansi_markdown_render(const char *md)` — converts markdown to ANSI-escaped string
   - Support: `**bold**`, `*italic*`, `` `code` ``, ``` ```code blocks``` ```, `# headers`, `> blockquotes`, `[links](url)` (OSC 8 hyperlinks), tables (via pipe → grid alignment)
   - Replace `cli_stream_cb()` to use ansi_markdown_render per-token

### Phase 2: Skin Engine Truecolor Parity ⭐
**Effort: Medium (2-3 days)**  
**Files:** `lib/libskin/skin.h`, `lib/libskin/skin.c`

1. **Expand skin JSON schema to match Python's 27 color slots**
   - Add all banners: banner_border, banner_title, banner_accent, banner_dim, banner_text
   - Add all UI: ui_accent, ui_label, ui_ok, ui_error, ui_warn
   - Add prompt/input: prompt, input_rule, response_border
   - Add status_bar: bg, text, strong, dim, good, warn, bad, critical
   - Add session: session_label, session_border
   - Add TUI: selection_bg, completion_menu_bg, completion_menu_current_bg, completion_menu_meta_bg, completion_menu_meta_current_bg

2. **Add branding slots**
   - agent_name, welcome, goodbye, response_label, prompt_symbol, help_header, tool_prefix

3. **Add spinner customization slots**
   - waiting_faces, thinking_faces, thinking_verbs, wings arrays

4. **Add banner art slots**
   - banner_logo (string), banner_hero (string)

5. **Add hex color resolution**
   - `skin_color_rgb(skin, key, &r, &g, &b)` → parse #RGB or fall back to named ANSI
   - Support full Python skin YAML→JSON schema
   - Lookup chain: skin JSON → built-in default (Python default skin values) → fallback

### Phase 3: KawaiiSpinner C Port ⭐
**Effort: Small (1-2 days)**  
**Files:** `src/cli/display_core.c`, `include/hermes_display.h`

1. **Upgrade spinner to Python parity**
   - `display_spinner_start_custom()` — accept face list, wing list, verb list
   - Frame types: dots `⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏`, cute faces `(｡◕‿◕｡)`, skin faces `(⚔)(⛨)`
   - Wing display: `  ⟪⚔ ⠋ forging ⟫ (1.2s)` or `  ⠋ thinking (0.5s)`
   - Elapsed time display: `(12.3s)`
   - `\r` based animation with line clearing
   - Pause support: `HERMES_SPINNER_PAUSE` env var
   - Non-TTY fallback: `[tool] message` without animation

2. **Integrate with skin** — read face/wing/verb config from skin on start

### Phase 4: Tool Activity Feed (ANSI/CLI) ⭐
**Effort: Small (1-2 days)**  
**Files:** `src/cli/cli.c`, `include/hermes_display.h`

1. **Add `display_tool_feed_line()` function**
   - Format: `┊ {emoji} {verb:9} {detail}  {duration}s`
   - Skin-tool_prefix-aware (`, ┊, │, ╎`)
   - Emoji resolution per tool (like Python's get_tool_emoji)
   - Failure detection: `[exit 1]`, `[error]` suffix, red prefix
   - Duration formatting: `1.2s`, `5.0s`

2. **Integrate into agent loop** — call after each tool completion
   - Replace `printf("Tool %s returned...")` with colorized tool feed line

### Phase 5: Rich-Style Banner ⭐
**Effort: Medium (3-5 days)**  
**Files:** `src/cli/cli.c`, `src/cli/display_core.c`

1. **Add the ASCII art "HERMES AGENT" logo**
   - 6-line block letter logo with truecolor gradients (gold #FFD700 → bronze #CD7F32)
   - Use `display_printf_rgb()` with per-line color from skin

2. **Add the caduceus/hero ASCII art**
   - 15-line winged staff/symbol, skin-customizable via banner_hero
   - Multi-color per-line support

3. **Add Rich-style bordered panel**
   - Upgrade `display_panel()` to truecolor
   - Add title text in border (like Python's `Panel(title=..., border_style=...)`)
   - Use Unicode box-drawing: `┌── title ──┐`, `└──────────┘`

4. **Right panel: tools/skills/MCP grid**
   - If terminal ≥ 95 cols: logo on top, two-column layout (hero left, info right)
   - Tool list per toolset with disabled/lazy coloring
   - Skills summary, MCP server list, version label, update indicator

### Phase 6: Help Screen Upgrade
**Effort: Small (1 day)**  
**Files:** `src/cli/commands.c`, `include/hermes_display.h`

1. **Add `display_help_panel()` function** — colored bordered panel with title
2. **Categorized command display** — iterate registry by category, render with skin colors
3. **Skill commands section** — scan for installed skills, render with ANSI markdown

### Phase 7: Inline Diff Rendering
**Effort: Small (1 day)**  
**Files:** `src/cli/display_core.c`

1. **Add `display_diff()` function** — unified diff with ANSI coloring
   - File header: light magenta/purple
   - Hunk headers (`@@ .. @@`): dim blue
   - Removed lines (`-`): red bg + white fg
   - Added lines (`+`): green bg + white fg
   - Context lines: dim white
   - Follow Python's `_render_inline_unified_diff` pattern exactly

### Phase 8: Response Box
**Effort: Small (1 day)**  
**Files:** `src/cli/cli.c`

1. **Wrap agent response in a colored panel**
   - Border color: `skin.color("response_border", gold)`
   - Title: `skin.branding("response_label", "⚕ Hermes ")`
   - Content: ANSI-markdown-rendered response text
   - Use all supported ANSI escape codes for bold/code/tables within

### Phase 9: Session Line (OSC 8 Hyperlink)
**Effort: Very Small (< 1 day)**  
**Files:** `src/cli/cli.c`

1. **Add OSC 8 hyperlink support**
   - `\033]8;uri;id\a...\033]8;;\a`
   - Display session ID as clickable link
   - Match Python's format: `Session: <id>` with dim color

### Phase 10: TUI Fullscreen Parity Pass
**Effort: Large (5-8 days)**  
**Files:** `tui_fullscreen.c` (entire file)

1. **Truecolor in ncurses** — use `init_extended_color()` or fall back to closest ANSI
2. **Skin-driven theme** — all colors from skin object instead of hardcoded pair numbers
3. **Markdown rendering upgrade** — tables (via hermes_markdown_tables), links (underline + color), blockquotes, code blocks with background fill
4. **Tool feed styling** — emoji per tool, duration, progress bar refinement
5. **Status bar** — iteration count, token usage, context budget bar with color gradient (green→yellow→orange→red)
6. **Completion menu** — truecolor background per skin

---

## 4. Architectural Recommendations

### 4.1 Module Dependency Order

```
lib/libansi (truecolor helpers)
    ↓
lib/libskin (expanded schema + hex parsing)
    ↓
src/cli/display_core.c (upgraded display functions)
    ↓
src/cli/ansi_markdown.c (markdown→ANSI converter)
    ↓
src/cli/cli.c (banner, help, tool feed integration)
    ↓
tui_fullscreen.c (ncurses parity pass)
```

### 4.2 Key API Signatures to Add

```c
// truecolor
void display_set_fg_rgb(int r, int g, int b);
void display_set_bg_rgb(int r, int g, int b);
void display_printf_rgb(int r, int g, int b, display_style_t style, const char *fmt, ...);
void display_printf_hex(const char *hex, display_style_t style, const char *fmt, ...);

// skin
bool skin_has_color(const skin_t *s, const char *key);
bool skin_color_rgb(const skin_t *s, const char *key, int *r, int *g, int *b);
const char *skin_get_branding(const skin_t *s, const char *key, const char *def);
skin_spinner_t *skin_get_spinner(const skin_t *s);  // faces, verbs, wings
const char *skin_get_banner_logo(const skin_t *s, const char *def);
const char *skin_get_banner_hero(const skin_t *s, const char *def);

// markdown
char *ansi_markdown_render(const char *md);           // full render
char *ansi_markdown_inline(const char *md);            // inline only (no blocks)

// tool feed
void display_tool_feed(const char *tool_name, const char *emoji,
                       const char *detail, float duration,
                       bool is_error, const char *error_suffix);

// panel
void display_panel_rgb(const char *title, const char *content,
                       int border_r, int border_g, int border_b,
                       int title_r, int title_g, int title_b);

// help
void display_help_panel(const char *title, const char *category,
                        const char *items, int accent_r, int accent_g, int accent_b);

// diff
void display_diff(const char *diff_text);
```

### 4.3 Skin JSON Schema (C target, matching Python)

```json
{
  "name": "default",
  "colors": {
    "banner_border": "#CD7F32",
    "banner_title": "#FFD700",
    "banner_accent": "#FFBF00",
    "banner_dim": "#B8860B",
    "banner_text": "#FFF8DC",
    "ui_accent": "#FFBF00",
    "ui_label": "#DAA520",
    "ui_ok": "#4caf50",
    "ui_error": "#ef5350",
    "ui_warn": "#ffa726",
    "prompt": "#FFF8DC",
    "input_rule": "#CD7F32",
    "response_border": "#FFD700",
    "status_bar_bg": "#1a1a2e",
    "status_bar_text": "#C0C0C0",
    "status_bar_strong": "#FFD700",
    "status_bar_dim": "#8B8682",
    "status_bar_good": "#8FBC8F",
    "status_bar_warn": "#FFD700",
    "status_bar_bad": "#FF8C00",
    "status_bar_critical": "#FF6B6B",
    "session_label": "#DAA520",
    "session_border": "#8B8682",
    "selection_bg": "#333355",
    "completion_menu_bg": "#1a1a2e",
    "completion_menu_current_bg": "#333355",
    "completion_menu_meta_bg": "#1a1a2e",
    "completion_menu_meta_current_bg": "#333355"
  },
  "spinner": {
    "waiting_faces": ["(⚔)", "(⛨)"],
    "thinking_faces": ["(⌁)", "(<>)"],
    "thinking_verbs": ["forging", "plotting"],
    "wings": [["⟪⚔", "⚔⟫"], ["⟪▲", "▲⟫"]]
  },
  "branding": {
    "agent_name": "Hermes Agent",
    "welcome": "Welcome to Hermes Agent!",
    "goodbye": "Goodbye! ⚕",
    "response_label": " ⚕ Hermes ",
    "prompt_symbol": "❯",
    "help_header": "(^_^)? Available Commands",
    "tool_prefix": "┊"
  },
  "tool_emojis": {
    "terminal": "⚔",
    "web_search": "🔮"
  },
  "banner_logo": "...",
  "banner_hero": "..."
}
```

---

## 5. Effort Summary

| Phase | Description | Effort | Dependencies |
|-------|-------------|--------|--------------|
| **P1** | Truecolor + ANSI markdown foundation | 3-5 days | None |
| **P2** | Skin engine truecolor parity | 2-3 days | P1 |
| **P3** | KawaiiSpinner C port | 1-2 days | P1, P2 |
| **P4** | Tool activity feed (ANSI) | 1-2 days | P1, P2 |
| **P5** | Rich-style banner | 3-5 days | P1, P2 |
| **P6** | Help screen upgrade | 1 day | P1, P2 |
| **P7** | Inline diff rendering | 1 day | P1 |
| **P8** | Response box wrapping | 1 day | P1, P2 |
| **P9** | Session line OSC 8 hyperlink | <1 day | None |
| **P10** | TUI fullscreen parity pass | 5-8 days | P1, P2, P5 |

**Total estimated effort: 18-28 days (1 developer)**

---

## 6. Quick Wins (Do First)

These items give disproportionate visual improvement for little effort:

1. **`display_printf_hex()`** — single function adding truecolor support unlock all skin colors
2. **KawaiiSpinner faces** — replace `|/-\` with `⠋⠙⠹` (copy Python's dots frames) and add elapsed time
3. **Tool feed in CLI** — ~50 lines to add `┊` lines after each tool call
4. **ANSI inline markdown** — `**bold**` → ANSI bold, `` `code` `` → colored background, `# header` → bold+underline

---

## 7. Files Reference

### Python Source Files (reference implementations)

| File | Purpose | Key Functions/Classes |
|------|---------|----------------------|
| `hermes_cli/skin_engine.py` | Skin data model, built-in skins, YAML loading | `SkinConfig`, `get_active_skin()`, `set_active_skin()` |
| `agent/display.py` | KawaiiSpinner, tool preview, cute messages, diff rendering | `KawaiiSpinner`, `get_cute_tool_message()`, `_render_inline_unified_diff()` |
| `hermes_cli/banner.py` | ASCII art, welcome banner, version label | `HERMES_AGENT_LOGO`, `HERMES_CADUCEUS`, `build_welcome_banner()` |
| `cli.py` (show_help) | Help screen with categorized commands | `show_help()`, `show_tools()`, `show_toolsets()` |
| `hermes_cli/commands.py` | Command registry for autocomplete + help | `COMMAND_REGISTRY`, `COMMANDS_BY_CATEGORY` |

### C Target Files

| File | Purpose | Modification |
|------|---------|--------------|
| `lib/libansi/ansi.h` | ANSI escape constants | Add truecolor macros + helpers |
| `lib/libansi/ansi.c` | ANSI helper functions | Add hex color parsing, `ansi_hex_fg/bg()` |
| `lib/libskin/skin.h` | Skin API header | Expand to 27 colors, branding, spinner, banner art |
| `lib/libskin/skin.c` | Skin engine | New schema, hex color resolution, spinner config |
| `include/hermes_display.h` | Display function API | Add truecolor, spinner upgrade, tool feed, panels |
| `src/cli/display_core.c` | ANSI display implementations | truecolor `display_printf_rgb()`, upgraded `display_spinner_*()`, `display_tool_feed()`, `display_panel_rgb()` |
| **`src/cli/ansi_markdown.c`** (NEW) | Markdown→ANSI converter | `ansi_markdown_render()`, `ansi_markdown_inline()` |
| `src/cli/cli.c` | CLI orchestrator | Updated `print_banner()`, markdown-aware `cli_stream_cb()`, tool feed integration |
| `src/cli/tui_fullscreen.c` | ncurses fullscreen TUI | Truecolor theme, skin-driven colors, upgraded markdown |
