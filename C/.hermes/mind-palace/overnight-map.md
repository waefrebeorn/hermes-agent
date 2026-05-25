# Slermes C — Overnight Map (v5 — 365 Gaps)

## Navigation
- **Battleship:** battleship-v10.md — 365 gaps, 21 sectors
- **State:** state.md — build metrics, stub stats
- **Goal:** goal-mantra.md — loop, phase order

## Phase 0a (8) — Entry Points
I01 pipe multi-line → I02 unknown flags → I03 --tui → I04 --session → I05 logs → I06 config → I07 DeepSeek → I08 cron zero-state

## Phase 0b (12) — Display
V01 skin engine → V02 spinner → V03 banner → V04 status bar → V05 tool feed → V06 response box → V07 help → V08 256-color → V09 prompt → V10 markdown → V11 faces → V12 emoji

## Phase 0c (40) — CLI Args
A01-A40: 40 C commands use (void)args — silently discard user input. Python parses and respects all args.

## Phase 0d (15) — Usages
U01 config format → U02 live edit → U03 auto-save → U04 resume → U05 multi-line → U06 errors → U07 tool progress → U08 profiles → U09 plugins → U10 gateway config → U11 MCP → U12 env detect → U13 tool tab-complete → U14 keyboard shortcuts → U15 graceful restart

## Key Insight
40 commands accepting input that's silently discarded is the biggest single user-facing gap. Fix by replacing (void)args with arg parsing.
