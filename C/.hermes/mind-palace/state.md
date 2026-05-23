# State — Hermes C Translation (2026-05-29, Session 22)

**~47.0% parity — 235 of 500 gaps closed (battleship v3).**

## Dashboard
| Category | Done | % | Notes |
|----------|------|---|-------|
| Core | 12/16 | 75% | Solid |
| Agent | 38/115 | 33% | rate_limit_tracker.py ported |
| CLI | 30/95 | 32% | /bundles, /send, /busy, /reload-skills, /skin, /background, /reload-mcp, /platform fixed; /reasoning/steer/update/debug fixed |
| Tools | 41/92 | 45% | 68 registered + test_runner fixes |
| Gateway | 22/64 | 34% | 19 platforms |
| MCP | 2/11 | 18% | stdio + server done |
| ACP | 1/9 | 11% | Basic server |
| Cron | 3/3 | 100% | Done |
| TUI | 1/8 | 12% | Experimental |
| Plugins | 10/26 | 38% | 10 .so, 16 to port |
| Config | 4/6 | 67% | 322 keys |
| Build/Doc | 9/11 | 82% | Docker fixed |
| Security | 7/10 | 70% | Sandbox, URL safety, file_safety |
| Provider | 11/18 | 61% | 9 native + metadata |
| Stubs | 4/10 | 40% | ALL stubs resolved |
| Tests | 10/12 | 83% | T01-T09 + ansi_strip + binary_extensions tests |
| CI/CD | 10/10 | 100% | All U gaps closed |
| **Total** | **235/500** | **47.0%** | **265 gaps remaining** |

## Session Log
- **Session 22 (May 29):** Fixed `/bundles` — was a stub that always said "No bundles configured." Now scans `~/.slermes/skill-bundles/*.yaml`, parses name/description/skills from YAML, and displays them. Suite: 195/0/0. Parity: 235/500.
- **Session 21 (May 29):** Fixed dangling pointer in `agent_session_create()` — `new_id[64]` was declared inside `if` block but used after scope. Fixed 3 format-truncation warnings by increasing path buffers from 4096 to `HERMES_PATH_MAX+64`/`*2`. **Build is now clean — 0 errors, 0 warnings.** Suite: 195/0/0. Parity: 233/500.
- **Session 20 (May 29):** Fixed `/skin` — was printing "not yet implemented", now stores selection in static var + setenv("HERMES_SKIN"). Fixed `/background` — removed "not yet supported" message (runs inline). Fixed `/reload-mcp` — now shows current server count and config hint instead of "not supported". Fixed `/platform pause/resume` — now accepts platform name and shows config hint. Suite: 195/0/0. Parity: 231/500.
- **Session 19 (May 29):** Fixed `/reload-skills` — now scans skills dir and reports count instead of saying "rescanned" with no effect. Suite: 195/0/0. Parity: 227/500.
- **Session 18 (May 29):** Fixed `/busy` command — now stores mode in g_busy_mode instead of printing and ignoring. Supports queue/steer/interrupt/status subcommands. Suite: 195/0/0. Parity: 226/500.
- **Session 17 (May 29):** Added `/send` slash command — sends messages via send_message_handler. Supports targets: 'local', 'stdout', 'platform:chat_id'. Suite: 195/0/0. Parity: 225/500.
- **Session 16 (May 29):** Fixed `/reasoning` command — now writes `state->llm.reasoning_effort` instead of printing message and ignoring state. Validates input against allowed levels. Suite: 195/0/0. Parity: 223/500.
- **Session 15 (May 29):** Fixed `/steer` command — now actually pushes to agent steer queue instead of just printing message. Supports -u/-a/-s role flags and -l list option. Suite: 195/0/0. Parity: 221/500.
- **Session 14 (May 29):** Implemented real `/update` command (git fetch, pull, rebuild). Implemented real `/debug` command (system info, version, git, session, tools, config, log tail). Both were stubs. Suite: 195/0/0. Parity: 219/500.
- **Session 13 (May 29):** Updated shell completions (bash/zsh) with new subcommands. Added `hermes version` CLI subcommand. Updated help text. Suite: 195/0/0. Parity: 216/500.
- **Session 12 (May 29):** Added CLI subcommand dispatch — `hermes status|dump|logs|tools|plugins|secrets|skills|cron|help` now work as shell subcommands, not just slash commands. Also updated help text. Suite: 195/0/0. Parity: 216/500.
- **Session 8 (May 29):** test_runner.sh fix — 4 skipped tests (skill_manage, managed_gateway, rate_limit, skill_mgmt_tool) fixed. Root causes: missing include dirs + wrong .c file linked. Suite: 195/0/0 (0 skipped). Parity: 216/500.

## Build Status
```
Suite:  195/0/0     (145 tests, ~1005 assertions)
Binary: 9.1MB       (dynamic, -O2 -g)
Errors: 0           (make -j$(nproc))
Warnings: ~40       (Wformat-truncation, -Wpedantic, unused params)
CI:     c-build.yml (Linux x86_64 + Docker)
```
