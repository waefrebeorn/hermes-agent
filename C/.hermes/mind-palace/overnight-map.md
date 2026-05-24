# Hermes C — Overnight Navigation Map (2026-05-27)

## Active: T01 resolved (210/0/0 in <60s). Next: CLI depth or provider plugins.

## Session 55-56 Completed
- **T01: test suite parallelization** — all 175 test blocks now run in background. **210/0/0 completes in <60s** (was timeout at 120s).
- File-backed ok/fail/skip counters for subshell-safe result collection
- `wait` before summary blocks until all background tests done
- Previous sessions: /paste WSL clipboard, stale battleship claims corrected

## Next Workstreams (pick one)
**A — CLI module depth (~30 gaps, P0):** Port small Python hermes_cli/ modules to C
**B — Provider plugins (~19 gaps, P1):** Pick smallest missing provider (nous = 44 lines Python)
**C — Agent modules (~44 gaps, P0):** Port prompt_builder.py, process_bootstrap.py, etc.

## Data Not to Re-Derive
- CLI commands.c: 79 real cmds (3702 lines), 0 stubs
- S01 (browser CDP): already fixed — registered to real handler
- Total gaps: ~200 (down from ~270)
- 175 test blocks in test_runner.sh: 64 run_lib_test + 111 if gcc blocks
