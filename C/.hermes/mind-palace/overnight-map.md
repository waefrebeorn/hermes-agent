# Slermes C — Overnight Map (v12 — 182 Gaps)

## Navigation
- **Battleship:** battleship-v11.md — 182 sector gaps, 15 sectors
- **State:** state.md (v8) — build metrics, entry points fixed
- **Goal:** goal-mantra.md — loop, phase order

## Phase 0 Entry Points
- ✅ F01: Multi-line pipe (fgets line-by-loop)
- ✅ F05: --json standalone emits JSON status
- Remaining: F02 (C logger gap), F03 (--json cmd output), F06-F10 (partially functional)

## Phase 0b (1) — Display
V10 markdown + D12 TUI parity + D13 banner parity + D14 tool progress + D15 error formatting

## Phase 0c (40) — CLI Args
A01-A40: 40 C commands use (void)args — silently discard user input. Python parses and respects all args.

## Phase 0d (15) — Usages
U01 config format → U02 live edit → U03 auto-save → U04 resume → U05 multi-line → U06 errors → U07 tool progress → U08 profiles → U09 plugins → U10 gateway config → U11 MCP → U12 env detect → U13 tool tab-complete → U14 keyboard shortcuts → U15 graceful restart

## Key Insight
40 commands accepting input that's silently discarded is the biggest single user-facing gap. Fix by replacing (void)args with arg parsing.
