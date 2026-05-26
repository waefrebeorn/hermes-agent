# Slermes C — Overnight Map (v16 — 169 Gaps)

## Navigation
|- **Battleship:** battleship-v12.md — 169 gaps, 14 sectors, S12 rebuilt with 11 real display gaps (V10 ✅)
- **State:** state.md (v10) — 13 display gaps documented
- **Phase 0 entry points:** ALL DONE (F01-F10)
- **Goal:** goal-mantra.md (v14)
- **Vault:** Phase 64 (15 stale retired), Phase 65 (6 new display gaps)

## Phase 0 Entry Points (4)
**ALL DONE** — F02 (C logger), F03 (--json pipe), F04 (chat subcmd), F09 (banner tool count)

## Phase 0b — Display (13)
**V10 ✅ done**, **V21 ✅ done** → V11 multi-spinner → V12 multi-line → V13 rich errors → V14 TUI parity → V15 Python TUI ecosystem → V16 voice mode → V17 /recap → V18 tips → V19 NO_COLOR → V20 output helpers

**Note:** V17 is the biggest gap — 15,220 LOC of TUI ecosystem (ui-tui/ + tui_gateway/) with NO C equivalent.

## Phase 0c (40) — CLI Args
A01-A40: 40 commands silently discard user input via (void)args

## Key Insight
The Python TUI ecosystem (Ink React + JSON-RPC gateway) dwarfs the C ncurses TUI by 4x+ LOC. If C wants full TUI parity, V17 is the single largest feature gap in the entire project.
