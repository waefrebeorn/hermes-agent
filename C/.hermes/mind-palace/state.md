     1|# Slermes C — State Dashboard (v10 — 2026-05-26)
     2|
     3|## Build Metrics
     4|Build clean. **85 tools** (registry_register). 118 slash commands. 19 gateways. 10 providers. 59 libs. 164 src/ .c files. 214 test_*.c files.
     5|
     6|## Recent Milestones
     7|- **F01/F05**: Pipe fix + --json standalone
- **F02/F03/F04/F09**: C logger, --json pipe mode, chat subcommand, banner tool count
     8|- **F06-F10**: Stubs wired (background, agents, restart, review)
     9|- **S06-S10**: 5 stub functions wired to active code
    10|- **S11**: All 5 dead code entries (X01-X05) wired
    11|- **M04**: --profile flag wired
    12|- **S12 D13/D14**: Corrected stale claims (banner + tool feed are full parity)
    13|
    14|## Battleship
    15|**v12 — 171 active gaps** across 14 sectors.
    16|
    17|## Display Parity (Phase 0b) — 13 gaps
    18|| ID | Gap | LOC Missing | Priority | 
    19||----|-----|-------------|----------|
    20|| V10 | Rich markdown rendering | ~1000 | P0 |
    21|| V11 | Inline edit diffs in agent loop | ~200 | P1 |
    22|| V12 | Dynamic banner stats from registry | ~50 | P1 |
    23|| V13 | Multiple spinner types (9 types) | ~100 | P2 |
    24|| V14 | Multi-line prompt input | ~500 | P2 |
    25|| V15 | Rich error formatting | ~100 | P2 |
    26|| V16 | TUI (ncurses) functional parity | ~2000 | P2 |
    27|| V17 | Python TUI ecosystem (ui-tui + tui_gateway) | ~15,220 | P2 |
    28|| V18 | Voice mode CLI infrastructure | ~846 | P2 |
    29|| V19 | /recap session summary | ~316 | P2 |
    30|| V20 | Startup tips system | ~485 | P2 |
    31|| V21 | NO_COLOR / TERM=dumb support | ~38 | P2 |
    32|| V22 | Structured CLI output helpers | ~78 | P2 |
    33|