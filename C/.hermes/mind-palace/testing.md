# Slermes C — Testing (v3 — 2026-05-26)

## Suite
```bash
cd /home/wubu/hermes-agent-dev/C/
bash test_runner.sh              # Full suite
bash test_runner.sh --verbose    # Per-test output
```

**226 passed, 0 failed, 23 skipped** (zero failures).

## Coverage by Area
| Area | Files | Key Tests |
|------|-------|-----------|
| **Libraries** | ~58 | json, yaml, http, crypto, cron, csv, datetime, path, hash, uuid, base64, html, textwrap, glob, signal, enum, difflib, regex, ansi, json5, toml, websocket, protobuf, dotenv, proc, template, skin, tui, display, ratelimit, rate_guard, filestate, tool_dispatch, etc. |
| **Providers** | 10 files | provider_error, anthropic/azure/bedrock/google/openrouter/xai/deepseek/custom depth, metadata, finish_reason, json_mode |
| **Agent** | ~10 | context, title, fallback, budget, checkpoint, redact, sanitize, vault, secrets, tokenizer, subdir_hints |
| **CLI** | ~5 | commands, paths, display, TUI |
| **Cron** | 4 | cron_lib, cron_tool, cron_sqlite, cron_extras |
| **Tools** | ~30 | file, web, terminal, exec_code, session, process, todo, memory, kanban, cronjob, skills, skill_mgmt, tts, vision, clarify, delegate, x_search, patch, tool_config, api_helpers, approval, url_safety, sandbox_escape, tirith, allowlist, result_storage, session_crud, shell_hooks, curator, usage_pricing |
| **Gateway** | ~5 | escape_mode, slack_blocks, discord_interactions, whatsapp_msg |
| **Memory** | 1 | memory operations (basic SQLite) |

## Tool Depth Notes
- browser.c: 1,598 LOC (Python: 3,796) — missing autofill, cookies, PDF, HAR export
- vision.c: 203 LOC (Python: 1,421) — missing OCR, face detection, barcode, EXIF
- web.c: 466 LOC (Python: 1,561) — missing cookie jar, sessions, proxy, form fill
- mcp_tool.c: 1,623 LOC (Python: 3,584) — missing SSE transport, OAuth, subscriptions
- file.c: 561 LOC (Python: 1,220) — missing glob, watch, diff, hex view

## Codebase Size
- Source `.c` files: 439 (src/ + lib/ + tests/)
- Source code lines: 84,164 (src/)
- Library lines: 286,003 (lib/)
- Test lines: 48,262 (tests/)
- Header lines: 8,462 (include/)
- Total: **~419K lines**

## Known Gaps
- No per-platform gateway integration tests (19 platforms)
- No ACP protocol tests
- No TUI component tests
- No browser CDP or computer_use tests (external deps)
- No environment backend tests (Docker, SSH)
- No fuzz testing
- No memory leak detection (valgrind/ASan in CI)

## Build with Sanitizers
```bash
# AddressSanitizer
make CFLAGS="-O1 -g -fsanitize=address" LDFLAGS="-fsanitize=address" slermes

# UndefinedBehaviorSanitizer
make CFLAGS="-O1 -g -fsanitize=undefined" LDFLAGS="-fsanitize=undefined" slermes
```
