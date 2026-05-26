     1|# Slermes C — Overnight Map (v9 — 349 Gaps)
     2|
     3|## Navigation
     4|- **Battleship:** battleship-v10.md — 349 gaps, 20 sectors
     5|- **State:** state.md — build metrics, stub stats
     6|- **Goal:** goal-mantra.md — loop, phase order
     7|
     8|## Phase 0a (8) — Entry Points
     9|I01 pipe multi-line → I02 unknown flags → I03 --tui → I04 --session → I05 logs → I06 config → I07 DeepSeek → I08 cron zero-state
    10|
## Phase 0b (9) — Display
V04 status bar → V04 status bar → V05 tool feed → V06 response box → V07 help → V08 256-color → V09 prompt → V10 markdown → V11 faces → V12 emoji
    13|
    14|## Phase 0c (40) — CLI Args
    15|A01-A40: 40 C commands use (void)args — silently discard user input. Python parses and respects all args.
    16|
    17|## Phase 0d (15) — Usages
    18|U01 config format → U02 live edit → U03 auto-save → U04 resume → U05 multi-line → U06 errors → U07 tool progress → U08 profiles → U09 plugins → U10 gateway config → U11 MCP → U12 env detect → U13 tool tab-complete → U14 keyboard shortcuts → U15 graceful restart
    19|
    20|## Key Insight
    21|40 commands accepting input that's silently discarded is the biggest single user-facing gap. Fix by replacing (void)args with arg parsing.
    22|
