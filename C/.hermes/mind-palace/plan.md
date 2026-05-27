# Slermes C — Plan (v25 — 1,915 Function Gaps)

## Verified State
Build clean. 77 unique tools, 80 CLI commands, 9 provider types + metadata, 19 gateways, 59 libs.
Suite: 226/0/25, 216 test files, 166 src .c files. Binary: 31MB.
~43% parity at function level (~1,412 C fns vs ~3,251 Python fns).

## DA v16 Key Findings
- Phase 2 provider deepen claims: HEAVILY STALE (8/8 deepen items already in C)
- 16/18 "missing" providers = PROVIDER_OPENAI aliases
- **2 real stubs found:** llm_background_review unwired in agent_loop, api_server mock fallback
- **10 missing Python tools not in C:** feishu_drive_comment ×4, video_analyze, yuanbao ×4
- Zero gateway polling stubs

## Battleship v16: 1,915 function-level parity gaps (~373 items)
v15 resolved items vaulted. 10 new missing tools added. 2 agent stubs documented.

## Phase Order
0. Display (16) — 14/16 done (V07 TUI, V08 Python TUI, V09 voice remain)
1. CLI Args (40) — ✅ ALL DONE
2. Providers (~20 real) — non-OpenAI port targets
3. Tool Features (49) — add missing features to existing C tools
4. Missing Tools (47) — port unported Python tool files (+10 newly found)
5. Gateway (51) — missing platform modules, deepening, infrastructure
6. Agent Modules (74) — 52 unported + 20 deepen + 2 unwired stubs
7. Plugins (13) — port remaining plugins
8. Libraries (19) — missing library features
9. Security (15) — hardening
10. Test Coverage (51) — tests for untested modules
11. Config/Infra (10) — config expansion, refactoring

## Next: Phase 3 — Tool Features (49 remaining)
Pick next unclosed gap from battleship-v16 1A table (mcp_tool, file_operations, tts_tool are the biggest).

## Completed This Session
- Triple DA v16 audit: found 10 missing Python tools, 2 agent stubs, browser CDP dead code
- Vaulted resolved v15 items (#21 approval, #23 patch, #19 homeassistant)
- Built battleship-v16 with fresh 1,915 function gaps across ~373 items
- Updated ALL walkway files, README, BANNER, entry, index, plan, prestige, overnight, goal-mantra
