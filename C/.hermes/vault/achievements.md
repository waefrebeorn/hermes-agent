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
- Battleship v9 replaces v8 as canonical
