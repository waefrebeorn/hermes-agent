# Slermes C — Plan (v32 — ~352 Items)

## Verified State
Build clean. 78 unique tools, 98 CLI commands, 9 provider types + metadata, 19 gateways, 59 libs.
Suite: 231/0/24, 220 test files, 160 src .c files. Binary: 30M.
~43% parity at function level (~1,412 C fns vs ~3,251 Python fns).

## DA v16 Key Findings
- Phase 2 provider deepen claims: HEAVILY STALE (8/8 deepen items already in C)
- 16/18 "missing" providers = PROVIDER_OPENAI aliases
- **2 real stubs found:** llm_background_review unwired in agent_loop, api_server mock fallback
- **10 missing Python tools not in C:** feishu_drive_comment ×4, video_analyze, yuanbao ×4
- Zero gateway polling stubs

## Battleship v16: ~352 item-level parity gaps
v15 resolved items vaulted. 10 new missing tools added. 2 agent stubs documented.
Recent: cronjob pause/resume/run + binary_extensions wired (-2 gaps).

## Phase Order
0. Display (16) — 14/16 done (V07 TUI, V08 Python TUI, V09 voice remain)
1. CLI Args (40) — ✅ ALL DONE
2. Providers (~20 real) — non-OpenAI port targets
3. Tool Features (27) — add missing features to existing C tools
4. Missing Tools (46) — port unported Python tool files (+10 newly found)
5. Gateway (51) — missing platform modules, deepening, infrastructure
6. Agent Modules (74) — 52 unported + 20 deepen + 2 unwired stubs
7. Plugins (13) — port remaining plugins
8. Libraries (19) — missing library features
9. Security (15) — hardening
10. Test Coverage (50) — tests for untested modules
11. Config/Infra (10) — config expansion, refactoring

## Next: Phase 3 — Tool Features (27 remaining)
Pick next unclosed gap from battleship-v16 1A table (mcp_tool, file_operations, tts_tool are the biggest).

## Completed This Session
- File tool: registered file_hash tool (SHA-256/SHA-1/MD5) — existed in code but was never registered
- file_tool test: fixed compilation (added binary.h include path + binary.c) and added 5 hash tests
- 78 tools (was 77), suite 231/0/24 (file_tool now runs instead of skip)
- x_search: added enable_image_understanding + enable_video_understanding params (parity with Python schema)
- image_generate: added negative_prompt + style params (matching Python schema)
