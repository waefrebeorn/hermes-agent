# Slermes C — State Dashboard (v11 — 2026-05-26)

## Build Metrics
Build clean. **85 tools** (registry_register). 118 slash commands. 19 gateways. 10 providers. 59 libs. 164 src/ .c files. 214 test_*.c files.

## Recent Milestones
- **V10**: Rich markdown rendering (markdown_render.c) — headers, bold, italic, code, links
- **F01/F05**: Pipe fix + --json standalone
- **F02/F03/F04/F09**: C logger, --json pipe mode, chat subcommand, banner tool count
- **F06-F10**: Stubs wired (background, agents, restart, review)
- **S06-S10**: 5 stub functions wired to active code
- **S11**: All 5 dead code entries (X01-X05) wired
- **M04**: --profile flag wired
- **S12 D13/D14**: Corrected stale claims (banner + tool feed are full parity)
|- **V10b**: Streaming bug fix — 5 providers (OpenAI, Azure, Custom, OpenRouter, xAI) + llm_client.c fallback all had `data:` prefix assumption. HTTP layer strips prefix, raw JSON streamed to terminal. All 6 files fixed.
|- **Agent linkage**: `agent_configure_from_config()` had ZERO callers. 28 config fields never wired to agent state (max_retries, temperature, top_p, fallback, etc.). FIXED: wired into CLI init path.

## Battleship
**v12 — 171 active gaps** across 14 sectors.

## Display Parity (Phase 0b) — 12 gaps (V10 ✅)
| ID | Gap | LOC Missing | Priority |
|----|-----|-------------|----------|
| V10 | Rich markdown rendering | ✅ ~1000 | P0 |
| V11 | Inline edit diffs in agent loop | ~200 | P1 |
| V12 | Dynamic banner stats from registry | ~50 | P1 |
| V13 | Multiple spinner types (9 types) | ~100 | P2 |
| V14 | Multi-line prompt input | ~500 | P2 |
| V15 | Rich error formatting | ~100 | P2 |
| V16 | TUI (ncurses) functional parity | ~2000 | P2 |
| V17 | Python TUI ecosystem (ui-tui + tui_gateway) | ~15,220 | P2 |
| V18 | Voice mode CLI infrastructure | ~846 | P2 |
| V19 | /recap session summary | ~316 | P2 |
| V20 | Startup tips system | ~485 | P2 |
| V21 | NO_COLOR / TERM=dumb support | ~38 | P2 |
| V22 | Structured CLI output helpers | ~78 | P2 |
