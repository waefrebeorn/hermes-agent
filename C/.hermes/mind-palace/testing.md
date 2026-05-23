# Hermes C — Testing (v11 — 2026-05-23)

## Suite
```bash
cd /home/wubu/hermes-agent-dev/C/
bash test_runner.sh              # Full suite
bash test_runner.sh --verbose    # Per-test output
```

**154 passed, 0 failed, 0 skipped** (117 files, ~573 assertions).

## Coverage by Area
| Area | Files | Key Tests |
|------|-------|-----------|
| **Libraries** | ~30 | json, yaml, http, crypto, cron, csv, datetime, path, hash, uuid, base64, html, textwrap, glob, signal, enum, difflib, regex, ansi, json5, toml, websocket, protobuf, dotenv, proc, template, skin, tui, display |
| **Providers** | ~12 | provider_error (225 assertions, 9 providers), anthropic/azure/bedrock/google/openrouter depth, metadata, finish_reason, json_mode |
| **Agent** | ~8 | context, title, fallback, budget, checkpoint, redact, sanitize, vault, secrets, tokenizer |
| **CLI** | ~5 | commands, paths, display, TUI |
| **Cron** | 4 | cron_lib, cron_tool, cron_sqlite, cron_extras |
| **Tools** | ~25 | file, web, terminal, exec_code, session, process, todo, memory, kanban, cronjob, skills, skill_mgmt, tts, vision, clarify, delegate, x_search, patch, tool_config, api_helpers, approval, url_safety, sandbox_escape, tirith, allowlist, result_storage |
| **Gateway** | ~5 | escape_mode, slack_blocks, discord_interactions, whatsapp_msg |
| **Plugins** | ~8 | honcho, kanban, spotify, achievements, disk_cleanup, file_memory, google_meet, image_gen, observability, skills |

## Known Gaps
- No per-platform gateway integration tests (19 platforms)
- No ACP protocol tests
- No TUI component tests
- No browser CDP or computer_use tests (external deps)
- No environment backend tests (Docker, SSH)
- No fuzz testing
- No memory leak detection (valgrind/ASan in CI)

## Test Infrastructure Needs (Sector T)
| ID | Gap | Priority |
|----|-----|----------|
| T01 | Gateway per-platform integration tests | P1 |
| T02 | CLI command coverage >80% | P1 |
| T03 | TUI component tests | P2 |
| T04 | MCP server/transport tests | P2 |
| T05 | ACP protocol tests | P2 |
| T06 | Browser CDP test harness | P2 |
| T07 | Plugin sandbox loading tests | P1 |
| T08 | Fuzz testing | P2 |
| T09 | Valgrind/ASan in CI | P1 |
| T10 | Thread safety tests | P2 |

## Build with Sanitizers
```bash
# AddressSanitizer
make CFLAGS="-O1 -g -fsanitize=address" LDFLAGS="-fsanitize=address" hermes

# UndefinedBehaviorSanitizer
make CFLAGS="-O1 -g -fsanitize=undefined" LDFLAGS="-fsanitize=undefined" hermes
```
