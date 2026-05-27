# Slermes C — Testing (v34 — 2026-05-27)

## Suite
```bash
cd /home/wubu/hermes-agent-dev/C/
bash test_runner.sh              # Full suite
bash test_runner.sh --verbose    # Per-test output
```

**229 passed, 0 failed, 26 skipped** (zero failures).

## Coverage by Area
| Area | Files | Key Tests |
|------|-------|-----------|
| **Libraries** | ~59 | json, yaml, http, crypto, cron, csv, datetime, path, hash, uuid, base64, html, textwrap, glob, signal, enum, difflib, regex, ansi, json5, websocket, protobuf, dotenv, proc, template, skin, tui, display, ratelimit, rate_guard, filestate, tool_dispatch, etc. |
| **Providers** | 10 files | provider_error, anthropic/azure/bedrock/google/openrouter/xai/deepseek/custom depth, metadata, finish_reason, json_mode |
| **Agent** | ~11 | context, title, fallback, budget, checkpoint, redact, sanitize, vault, secrets, tokenizer, subdir_hints, logger |
| **CLI** | ~5 | commands, paths, display, TUI |
| **Cron** | 4 | cron_lib, cron_tool, cron_sqlite, cron_extras |
| **Tools** | ~30 | file, web, terminal, exec_code, session, process, todo, memory, kanban, cronjob, skills, skill_mgmt, tts, vision, clarify, delegate, x_search, patch, tool_config, api_helpers, approval, url_safety, sandbox_escape, tirith, allowlist, result_storage, session_crud, shell_hooks, curator, usage_pricing |
| **Gateway** | ~5 | escape_mode, slack_blocks, discord_interactions, whatsapp_msg |
| **Memory** | 1 | memory operations (basic SQLite) |

## Tool Depth Notes
- browser.c: 1,598 LOC (Python: 3,796) — missing autofill, cookies, PDF, HAR export
- web.c: 601 LOC — cookies param added to web_get
- x_search.c: lang filter added to searxng backend
