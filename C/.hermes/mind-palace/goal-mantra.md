# Slermes C — Goal Mantra (v22)

P0: Full 1:1 drop-in replacement for Python Hermes.
**1,885 function-level parity gaps** (battleship-v15). ~43% parity.

## Phase Order
0. Display Parity (16 gaps) — 14/16 done (V07 TUI, V08 Python TUI, V09 voice remain)
1. CLI Args (40) — ✅ ALL DONE
2. Provider Parity (~20) — deepen claims stale per DA v15; port uncommon API providers
3. Tool Features (60) — add missing features to existing C tools
4. Missing Tools (37) — port unported Python tool files
5. Gateway (51) — missing platform modules, deepening, infrastructure
6. Agent Modules (72) — unported agent modules + deepen existing
7. Plugins (13) — port remaining plugins
8. Libraries (19) — missing library features
9. Security (15) — hardening
10. Test Coverage (51) — tests for untested modules
11. Config/Infra (10) — config expansion, refactoring

## Next: Phase 2 — Provider Parity (~20 real gaps)
DA v15 confirmed: 8 deepen claims mostly stale (features already in C).
Real gaps: port non-OpenAI-compatible providers (copilot, opencode-zen, openai-codex).
Pick first gap from battleship-v15 Sector 3B that's not a PROVIDER_OPENAI alias.

## Doc Discipline
After every implementation pass: update ALL walkway files + README + banner + battleship + testing + entry + index. Barnacle hunt stale numbers with search_files after every patch batch.

## Session Loop
read state.md → battleship → prestige → pick first unclosed gap → implement → build → test → update ALL docs → search_files for stale numbers → vault resolved gap → commit → repeat. No questions, no choices.
