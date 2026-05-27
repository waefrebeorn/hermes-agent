# Slermes C — State Dashboard (v32 — 2026-05-27)

## Build Metrics
Build clean. **79 unique tools** (registry_register). 98 CLI commands (table entries). 19 gateways. 9 provider types + metadata utility. 59 libs. 160 src/ .c files (non-deps). 220 test_*.c files. Binary: 30M. Suite: 231/0/24.

## 1:1 Parity Status (Triple DA v16)
Python: ~3,251 core functions (battleship-v16 baseline)
C: ~1,412 functions in core modules (agent/tools/cli/gateway)
~354 item-level gaps (battleship-v16 rows).

## DA v16 Findings (2026-05-26)
Phase 2 provider deepen claims: **HEAVILY STALE**.
- anthropic: cache_control, thinking blocks, tool_use stream, prompt caching — ALL exist
- openai: strict mode, service_tier, response_format schema — ALL exist
- deepseek: thinking config, FIM, reasoning_content — ALL exist
- xai: reasoning_effort exists
- openrouter: HTTP-Referer/X-Title headers exist
- gemini: parts array, safety settings, generation config, system instruction exist
- bedrock: Converse full exists
- azure: identity, API version management exist
- 16/18 "missing" providers routed as PROVIDER_OPENAI aliases

**New stubs found in v16 audit:**
- `llm_background_review` — fully implemented at llm_client.c:1518 but NOT YET WIRED into agent_loop background review path
- `api_server.c` — mock fallback when g_agent is NULL echoes messages instead of dispatching through LLM
- 10 missing tools from Python tree: feishu_drive_comment ×4, video_analyze, yuanbao ×4 (send_dm, query_group_info/members, send/search_sticker)

Zero gateway polling stubs — all 13 platforms with poll_messages have real implementations.

## Battleship
**v16 — ~355 item-level parity gaps**. Resolved items from v15 (#21 approval, #23 patch, #19 homeassistant) vaulted. 10 new missing tools added from Triple DA stub audit. 2 agent stubs documented. Recent: cronjob pause/resume/run + binary_extensions wired (-2 gaps).

## Phase Order
0. Display Parity (16 gaps) — ✅ 14/16 done (V07 TUI, V08 Python TUI, V09 voice remain)
1. CLI Args (34 gaps) — ✅ ALL DONE — all 98 commands wired with arg processing
2. Provider Parity (~20 real gaps) — deepen claims stale, only non-OpenAI providers remain
3. Tool Features (29 gaps) — add Python features to existing C tools
4. Missing Tools (46 gaps) — port remaining tool files (+10 newly found)
5. Gateway (51 gaps) — port 14 missing modules + deepen 20 platforms + 17 infra
6. Agent Modules (74 gaps) — port 52 unported + deepen 20 existing + 2 unwired stubs
7. Plugins (13 gaps) — port remaining plugins
8. Libraries (19 gaps) — add missing library features
9. Security (15 gaps) — security hardening
10. Test Coverage (50 gaps) — tests for untested modules
11. Config/Infra (10 gaps) — config expansion, refactoring
