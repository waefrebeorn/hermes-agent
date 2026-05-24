# Hermes C — Overnight Navigation Map (DA v15 reset)

## Active: Battleship reset complete. Fresh gap survey done. All mind palace files rewritten.

## Fresh Findings (2026-05-27 — code survey update)
- Old DA v15 claim of "197 stub commands" was stale — commands.c has 79 real cmds (3702 lines)
- Browser CDP handler (S01) already fixed — real handler registered, not stub
- CLI gap is MODULE depth (8 .c vs 88 .py files), not command stubs
- 1 true gap closed this session: /paste now reads WSL clipboard via powershell.exe
- **Real gap count: ~200 (down from ~270)**

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
