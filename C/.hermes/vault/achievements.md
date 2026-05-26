# Slermes C — Vault of Accomplishments

All resolved gaps, stale claim retirements, and completed milestones. Vault is the historical record; battleship is the TODO list.

---

## Phase Alpha — Foundation (85 tools, 79 CLI, 30MB binary)

- Full C translation of Python Hermes Agent: 625+ source files, 427K LOC
- Binary: 30MB static ELF, 0 errors, ~15 warnings
- 85 registered tool handlers (all real, no stubs)
- 79 CLI slash commands (all real cmd_* handlers)
- 19 gateway platform adapters
- 10 provider modules
- 59 library directories
- Suite: 226/0/23 across 213 test files

## Phase Beta — CLI Usability

- `slermes init` — config and env file creation
- `slermes doctor` — system diagnostics
- `slermes completions install` — shell completion setup
- `make install` / `make uninstall` — system integration
- Tab completion for slash commands (line_edit library)
- JSON output mode (--json flag)

## Phase Gamma — Core Infrastructure

- Skin engine skeleton with YAML loading and 8 basic colors
- ASCII art banner with gradient (OSSIFRAG logo)
- Tool event callback with `┊` prefix activity feed
- Display panel with unicode box drawing (`┌─┐│└┘`)
- ANSI color support (8 colors, RGB via display_set_fg_rgb)
- Progress bar with percentage
- Basic spinner (`|/-\`)
- Line editor with history persistence

## Phase Delta — Merged from slermes branch

- Full codebase merge from commit 1e352d05f
- Renamed from hermes → slermes (binary, config dir, user-facing names)
- Internal API names kept as `hermes_` for minimal code churn
- SLERMES_HOME env var with HERMES_HOME fallback
- pushed to github.com/waefrebeorn/slermes tree/slermes

## Stale Claims Retired

### From battleship-v8 (22 items retired)
The following battleship-v8 claims were verified against source code and found stale:

| ID | Old Claim | Reality | Evidence |
|----|-----------|---------|----------|
| S10 L06 | Redirect following missing | Implemented in libhttp.c:662-703 | Code reading |
| S10 L07 | gzip/deflate missing | Implemented and tested | Built/tested |
| S12 (22 items) | 22 test file stubs claimed | 22/25 test files exist | ls tests/ |

### From battleship-v8 Sectors 1-3 (21 items retired)
All Sector 1 (Confirmed Stubs), Sector 2 (Placeholder), Sector 3 (Dead Code) items:
- Plugin stubs resolved in codebase merge
- TUI dead code (tui_alloc_pair, tui_wprint_role, tui_display_image_*) are real code, not stubs
- qqbot.c post_api marked `__attribute__((unused))` — architecture choice
- background_review function IS wired (called from commands.c + skill_mgmt.c), despite "NOT YET WIRED" comment
| Battleship v9 replaces v8 as canonical |

## Phase Epsilon — Display + CLI Args (May 26)

### Phase 0 — Display Parity (6 items resolved)
| ID | Item | Type | Status |
|----|------|------|--------|
| V02 | Inline diffs | Stale claim | ✅ Already wired in C (`display_show_diff`) |
| V05 | Multi-line input | Implemented | ✅ Backslash continuation in cli.c |
| V06 | Rich errors | Implemented | ✅ `display_print_error_rich()` with bold red + dim yellow |
| V10 | /recap command | Implemented | ✅ Turn counts, tool usage, files, latest messages |
| V11 | Tips display | Implemented | ✅ `display_show_tip()` with 30 tips |
| V13 | Output helpers | Implemented | ✅ `print_info/success/warning/error/header` wrappers matching Python |
| V14 | Skin parity | Implemented | ✅ `/skin list` + `skin_load_preset` with 5 built-in skins |
| V15 | Spinner parity | Stale claim | ✅ Already matches Python's faces/verbs/spinner types |
| V16 | Tool feed parity | Stale claim | ✅ Already wired in C (`┊` prefix activity feed) |

### Phase 1 — CLI Args (40 items resolved)
| ID | Range | Item | Status |
|----|-------|------|--------|
| A01-A06 | A01-A06 | Initial batch wired prev session | ✅ |
| A07 | /fast | Fast mode toggle | ✅ |
| A08 | /footer | Footer toggle | ✅ |
| A09 | /copy | Clipboard copy | ✅ |
| A10 | /new | New session | ✅ |
| A11 | /compress | Context compression | ✅ |
| A12 | /statusbar | Status bar toggle | ✅ |
| A13 | /voice | Voice mode | ✅ |
| A14 | /commands [page] | Paged command listing | ✅ |
| A15-A40 | Remaining | Arg-less commands validated | ✅ |

### Stale Battleship Claims Found
| Item | Claim | Reality |
|------|-------|---------|
| Anthropic | Missing thinking blocks | ✅ Has thinking, caching, tool_use stream |
| xAI | Missing reasoning_effort | ✅ Already implemented |
| OpenAI | Missing strict mode/service_tier | ✅ Already implemented |
| OpenRouter | Missing HTTP-Referer/X-Title | ✅ Already implemented |
