# Slermes C — Goal Mantra (v17)

P0: Full 1:1 drop-in replacement for Python Hermes.
**1,889 function-level parity gaps** (battleship-v14). 42% parity at function level.

## Phase Order
0. Display Parity (16) — inline diffs, multi-line, rich errors, TUI, voice, /recap, tips, helpers
1. CLI Args (40) — wire (void)args for 40 commands
2. Provider Parity (26) — deepen 8 + port 18 missing providers
3. Tool Features (60) — add missing features to existing C tools
4. Missing Tools (37) — port unported Python tool files
5. Gateway (51) — missing platform modules, deepening, infrastructure
6. Agent Modules (72) — unported agent modules + deepen existing
7. Plugins (13) — port remaining plugins
8. Libraries (19) — missing library features
9. Security (15) — hardening
10. Test Coverage (51) — tests for untested modules
11. Config/Infra (10) — config expansion, refactoring

## Loop
Read battleship → pick first gap → implement → build → test → update ALL docs → vault → commit → repeat.
