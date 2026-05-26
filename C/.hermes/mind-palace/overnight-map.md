# Slermes C — Overnight Map (v18 — 1,889 Gaps)

## Navigation
- **Battleship:** battleship-v14.md — 1,889 function-level parity gaps, 11 layers, Triple DA v14
- **State:** state.md (v13) — 42% function parity
- **Phase Order:** Display → CLI Args → Providers → Tools → Gateways → Agents → Plugins → Libraries → Security → Tests → Config
- **Goal:** goal-mantra.md (v17)

## Phase 0 — Display (16 gaps)
inline diffs (V02), multi-line input (V05), rich errors (V06), TUI parity (V07-V08), voice mode (V09), /recap (V10), tips (V11), output helpers (V13), skin full parity (V14), spinner full (V15), tool feed (V16)

## Phase 1 — CLI Args (40 gaps)
A01-A40: 40 commands with (void)args that need to accept and process user input

## Phase 2-11 (414+ items, 1,889 function gaps)
Providers → Tool features → Missing tools → Gateway → Agent modules → Plugins → Libraries → Security → Tests → Config

## Key Metrics
- 1,889 function gaps = number of Python functions without C equivalents
- Each gap = 1 or more Python functions to port
- 3,251 Python total vs 1,362 C total = 42% function parity
