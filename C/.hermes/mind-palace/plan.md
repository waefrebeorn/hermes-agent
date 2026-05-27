# Slermes C — Plan (v22 — 1,886 Function Gaps)

## Verified State
Build clean. 72 unique tools, 80 CLI commands, 9 provider types + metadata, 19 gateways, 59 libs.
Suite: 227/0/24, 215 test files, 160 src .c files. Binary: 30MB.
~43% parity at function level (~1,412 C fns vs ~3,251 Python fns).

## DA v15 Key Findings
- Phase 2 provider deepen claims: HEAVILY STALE (8/8 deepen items already in C)
- 16/18 "missing" providers = PROVIDER_OPENAI aliases
- Only real stub: stub_cdp_handler in browser.c (dead code, unused)
- Zero gateway polling stubs

## Battleship v15: 1,886 function-level parity gaps (~366 items)
Phase 2 sector needs re-audit before next implementation pass.

## Phase Order
0. Display (16) — 14/16 done (V07 TUI, V08 Python TUI, V09 voice remain)
1. CLI Args (40) — ✅ ALL DONE
2. Providers (~20 real) — non-OpenAI port targets
3. Tool Features (52) — add missing features to existing C tools
4. Missing Tools (37) — port unported Python tool files
5. Gateway (51) — missing platform modules, deepening, infrastructure
6. Agent Modules (72) — unported agent modules + deepen existing
7. Plugins (13) — port remaining plugins
8. Libraries (19) — missing library features
9. Security (15) — hardening
10. Test Coverage (51) — tests for untested modules
11. Config/Infra (10) — config expansion, refactoring

## Next: Phase 3 — Tool Features (52 remaining)
Pick next unclosed gap from battleship-v15 1A table.

## Completed This Session
- Approval gateway prompt (#21) — gateway approval callbacks (send/wait), Telegram short-poll response capture, cross-platform condvar signaling. approval_set_gateway_send/wait, gw_approval_wait_response, telegram_poll_for_response. Suite: 226/0/25.
- Logger test suite (test_logger.c) — 20 tests, all pass
- Triple DA v15 audit — found Phase 2 claims stale, documented in state.md
- V4A multi-file patch mode added to patch.c (mode="patch" param with *** Begin/End Patch markers)
  - Supports: Update File (multi-hunk), Add File, Delete File
  - 3 tests in test_runner.sh, suite 227/0/24
- Skill dependency resolution: skill_manage deps action added
  - Parses depends_on from YAML frontmatter, resolves against installed skills
  - Suite still 227/0/24 (deps test via direct linkage)
- Walkway files bumped to v16/v21 with verified numbers
- HomeAssistant tool: ha_list_entities domain/area filter, ha_list_services domain filter, ha_call_service validation+blocklist+entity_id+data. 25 tests.
  - 3 gaps closed (#19 homeassistant). Suite: 227/0/24.
- File diff + permissions: unified diff + stat/chmod via difflib. 8 tests (35→43). Suite: 227/0/24.
- File hex view: hex dump with ASCII side panel, offset/limit. 5 tests (43→48). Suite: 227/0/24.
