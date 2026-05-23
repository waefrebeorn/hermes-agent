# Hermes C — Testing (v10 — 2026-05-23)

Testing is consolidated in `test_runner.sh` at the project root.

## Suite Summary
```
Results: 154 passed, 0 failed, 0 skipped
Test files: 116
Assertions: ~573 (assert() calls in source)
Binary: 9.2M dynamically linked
```

## Run Tests
```bash
cd /home/wubu/hermes-agent-dev/C/
bash test_runner.sh              # Full suite
bash test_runner.sh --verbose    # Detailed per-test output
```

## Test Coverage Areas

| Area | Files | Key Tests |
|------|-------|-----------|
| **Libraries** | ~30 | json, yaml, http, crypto, cron, csv, datetime, path, hash, uuid, base64, html, textwrap, glob, signal, enum, difflib, regex, ansi, json5, toml, websocket, protobuf, dotenv, proc, template, skin, tui, display |
| **Providers** | ~12 | provider_error (225 assertions, 9 providers), anthropic_depth, azure_full, bedrock_full, google_full, openrouter_depth, provider_metadata, finish_reason, json_mode |
| **Agent** | ~8 | context, title, fallback_routing, budget_tracker, checkpoint, redact, sanitize, vault, secrets, tokenizer, xai_retirement |
| **CLI** | ~5 | commands, paths, display, TUI |
| **Cron** | 4 | cron_lib (51), cron_tool, cron_sqlite (48), cron_extras (41) |
| **Tools** | ~25 | file, web, terminal, exec_code, session_search, process, todo, memory, kanban, cronjob, skills, skill_mgmt, tts, vision, clarify, delegate, x_search, patch, tool_config, api_helpers, approval, url_safety, sandbox_escape, tirith, allowlist, result_storage |
| **Gateway** | ~5 | escape mode, slack_blocks, discord_interactions, whatsapp_msg |
| **Plugins** | ~8 | honcho, kanban, spotify, achievements, disk_cleanup, file_memory, google_meet, image_gen, observability, skills |

## Known Test Gaps
- No integration/E2E tests (gateway end-to-end, full agent conversation)
- No ACP adapter tests
- No TUI tests
- No computer_use or browser tool tests (external deps)
- No environment backend tests (Docker, SSH, Modal)

## Sanitizer Support
Build with AddressSanitizer by adding `-fsanitize=address` to CFLAGS in Makefile. Not enabled by default.
