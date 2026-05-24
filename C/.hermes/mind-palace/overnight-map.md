# Hermes C — Overnight Navigation Map (DA v15 reset)

## Active: Battleship reset complete. Fresh gap survey done. All mind palace files rewritten.

## Fresh Findings (DA v15 — 2026-05-24)
- Old battleship-v3 (500 gaps, 34%) was stale — many phantom gaps from already-resolved stubs
- Fresh code survey found **~270 real, verifiable gaps**
- Biggest gap sectors: CLI (9% parity), Agent (57%), Providers (32%)
- 1 true stub: browser_cdp tool not wired to real CDP client
- 197 of 237 cmd_ functions in commands.c are printf stubs
- 33 of 77 agent modules ported
- 19 of 28 provider plugins missing
- 19 of 31 gateway platform modules done

## Key Paths
- Source: `/home/wubu/hermes-agent-dev/C/src/`
- Tests: `/home/wubu/hermes-agent-dev/C/tests/test_*.c`
- Test runner: `bash /home/wubu/hermes-agent-dev/C/test_runner.sh`
- Battleship v4: `.hermes/mind-palace/plans/battleship-v4.md`
- DA v15: `.hermes/mind-palace/da-audit-v15.md`

## Workstreams (pick one)
**A — CLI Depth** [P0]: Fix 197 stub commands in commands.c (biggest impact)
**B — Agent Ports** [P0]: Port agent_init.py, agent_runtime_helpers.py, prompt_builder.py
**C — Provider Plugins** [P1]: Port 19 missing provider plugins
**D — Gateway** [P1]: Port api_server, wecom helpers, feishu_comment

## Data Not to Re-Derive
- Python has 432 config keys, C has ~322
- Python has 77 agent .py files, C has 44 .c
- Python has 88 CLI .py files, C has 8 .c
- Python has 28 provider plugin dirs, C has 9 native
- Browser CDP: 300+ lines real code in browser.c, but registration still points to stub_cdp_handler()
- CLI stubs: grep '(void)state' src/cli/commands.c to find all 197 stubs

## Fallback
If all workstreams blocked: update battleship-v4 with more granular gaps, or verify the state.md numbers by running the test suite.
