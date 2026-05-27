# Slermes C — Goal Mantra (v34)

P0: Full 1:1 drop-in replacement for Python Hermes.
**~347 item-level parity gaps** (battleship-v16).

## Phase Order
0. Display Parity (16 gaps) — 14/16 done (V07 TUI, V08 Python TUI, V09 voice remain)
1. CLI Args (34) — ✅ ALL DONE
2. Provider Parity (~20) — deepen claims stale per DA v16; port uncommon API providers
3. Tool Features (21) — add missing features to existing C tools. Recent: web_get cookies param added.
4. Missing Tools (46) — port unported Python tool files (+10 newly found in v16 audit)
5. Gateway (51) — missing platform modules, deepening, infrastructure
6. Agent Modules (74) — 52 unported + 20 deepen + 2 unwired stubs (llm_background_review, api_server mock)
7. Plugins (13) — port remaining plugins
8. Libraries (19) — missing library features
9. Security (15) — hardening
10. Test Coverage (50) — tests for untested modules
11. Config/Infra (10) — config expansion, refactoring

## Next: Phase 3 — Tool Features (21 remaining)
Recent: web_get cookies param added. Next: pick next from battleship-v16 1A table (x_search filters, web_get rate limiting, vision OCR, etc.).

## Doc Discipline
After every implementation pass: update ALL walkway files + README + banner + battleship + testing + entry + index. Barnacle hunt stale numbers with search_files after every patch batch.
