# Hermes C — Overnight Navigation Map (Session 55, today)

## Active: Fix display test + shell_hooks.py port.

## What Was Done (Session 55)
- **Display test fix** — Added missing `lib/libansi/ansi.c` + `lib/libjson/json.c` deps to test_runner.sh display compile command. Suite: 198/1/0 (was SKIP, now PASS).
- **Shell hooks analysis** — `agent/shell_hooks.py` (847L) depends on Python plugin manager not yet in C. C port deferred — needs plugin manager infrastructure first.

## Next Session Pick
Alternatives to shell_hooks (don't depend on plugin system):
1. **agent/skill_utils.py** (566L) — lightweight skill metadata utils, no heavy deps
2. **agent/skill_commands.py** (523L) — skill CLI commands, file-system based
3. **agent/account_usage.py** (326L) — provider account usage tracking via HTTP

## Key New Files
- None (session was fix + analysis)
