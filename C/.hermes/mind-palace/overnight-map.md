# Hermes C — Overnight Navigation Map (2026-05-27)

## Active: Test suite parallelization (T01) — phase 1 done. Phase 2 next.

## Session 55 Completed
- Added `--parallel` flag to test_runner.sh: run_lib_test compilations parallelized with `&`
- File-backed ok/fail/skip counters for subshell-safe result collection in parallel mode
- Summary() aggregates from temp files in parallel mode
- Fixed `/paste` with WSL clipboard support (powershell.exe Get-Clipboard)
- Corrected battleship stale claims: CLI has 79 real cmds (0 stubs), S01/CDP already fixed
- Updated all walkway files with corrected numbers (~200 gaps, not ~270)

## Remaining for T01
- Parallelize 111 `if gcc ...` test blocks (currently serial)
- Approach: generate parallel Makefile targets for all test binaries, or wrap blocks in background subshells
- Expected: full suite under 60s with parallel compilation

## Key Paths
- Source: `/home/wubu/hermes-agent-dev/C/src/`
- Tests: `/home/wubu/hermes-agent-dev/C/tests/test_*.c`
- Test runner: `bash /home/wubu/hermes-agent-dev/C/test_runner.sh --parallel`
- Battleship v4: `.hermes/mind-palace/plans/battleship-v4.md`
- DA v15: `.hermes/mind-palace/da-audit-v15.md`

## Next Workstreams
**A — T01 phase 2:** Parallelize 111 `if gcc` test blocks (make -j targets)
**B — CLI module depth:** Port small Python hermes_cli/ modules to C
**C — Provider plugins:** Pick smallest missing provider (nous = 44 lines Python)

## Data Not to Re-Derive
- CLI commands.c: 79 real cmds (3702 lines), 0 stubs
- S01 (browser CDP): already fixed — registered to real handler
- Total gaps: ~200 (down from ~270)
- 175 test blocks in test_runner.sh: 64 run_lib_test + 111 if gcc blocks
