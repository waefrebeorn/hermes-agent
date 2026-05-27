# Slermes C — Plan (v34 — ~347 Items)

## Verified State
Build clean. 83 unique tools, 117 CLI commands, 10 provider types + metadata, 19 gateways, 59 libs.
Suite: 229/0/26, 221 test files, 165 src .c files. Binary: 30M.
~43% parity at function level (~1,412 C fns vs ~3,251 Python fns).

## DA v16 Key Findings
- Phase 2 provider deepen claims: HEAVILY STALE (8/8 deepen items already in C)
- 16/18 "missing" providers = PROVIDER_OPENAI aliases
- **2 real stubs found:** llm_background_review unwired in agent_loop, api_server mock fallback
- **10 missing Python tools not in C:** feishu_drive_comment ×4, video_analyze, yuanbao ×4
- Zero gateway polling stubs

## Battleship v16: ~347 item-level parity gaps
v15 resolved items vaulted. 10 new missing tools added. 2 agent stubs documented.
Recent: web_get cookies param added (-1 Phase 3 gap).

## Phase Order
0. Display (16) — 14/16 done (V07 TUI, V08 Python TUI, V09 voice remain)
1. CLI Args (40) — ✅ ALL DONE
2. Providers (~20 real) — non-OpenAI port targets
3. Tool Features (22→21) — web_get cookies param added. Next: x_search filters, web_get rate limiting, etc.
4. Missing Tools (46) — port unported Python tool files (+10 newly found)
5. Gateway (51) — missing platform modules, deepening, infrastructure
6. Agent Modules (74) — 52 unported + 20 deepen + 2 unwired stubs
7. Plugins (13) — port remaining plugins
8. Libraries (19) — missing library features
9. Security (15) — hardening
10. Test Coverage (50) — tests for untested modules
11. Config/Infra (10) — config expansion, refactoring
