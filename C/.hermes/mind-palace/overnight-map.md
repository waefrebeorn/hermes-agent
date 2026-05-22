# Overnight Map — 2026-05-27

## Active Session: O14 Sandbox Escape Detection ✅ CLOSED

**Build/doc: 90% → 95%** (1 gap remains: O02 Windows build)
**Suite: 106/0/0** (+1 test file, 14 assertions)

## What Was Done

- New sandbox_escape.c module: 48 escape patterns across 7 categories
- Wired into terminal_handler() + exec_code_handler() — blocks escape before subprocess
- sandbox_escape_check_path() for standalone path scanning
- sandbox_escape_add_pattern() for custom patterns
- test_sandbox_escape.c — 14 tests
- Removed over-broad `&&`/`||`/`${` from default patterns

## Remaining Build/doc gap

- **O02: Windows build support** — MSVC/MinGW detection, _WIN32 ifdefs. Big infra change.

## P1 Gap Options (non-build)

1. **M09: Slack block formatting tests** — block kit serialization/deserialization
2. **B33: DeepSeek context caching** — prefix caching API
3. **H14: JSON output mode** — `--json` CLI flag
4. **M10: WhatsApp message format tests** — template, interactive button tests
5. **M32-M44: Per-tool tests** — delegate, vision, TTS, memory, cron, etc.
