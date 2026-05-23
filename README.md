# Hermes C — AI Agent Runtime in C

**One-binary drop-in for Python hermes-agent. Zero runtime deps beyond libc + libssl.**

| Status | |
|--------|---|
| **Suite** | 154/0/0 — 117 test files, ~573 assertions |
| **Parity** | ~32% (161/500 gaps closed per [battleship v3](.hermes/mind-palace/plans/battleship-v2.md)) |
| **Binary** | 9.1MB ELF (dynamically linked), 75.5K LOC src, 144 .c+.h files |
| **Build** | `gcc -O2 -g -Wall -Wextra -Wpedantic` — 0 errors, ~40 warnings |
| **CI** | [C Build & Test](.github/workflows/c-build.yml) — Linux x86_64, Docker image |

```text
CLI/Gateway → Agent Loop → LLM Client → 9 Providers (OpenAI-compat + native)
                 ↓
          Tool Registry (68 tools)
                 ↓
          Plugin Registry (10 .so)
                 ↓
          30 Library Units (compiled into binary)
```

## Quick Start

```bash
cd C/
make -j$(nproc)          # Build hermes binary
./hermes --help          # Usage
bash test_runner.sh      # 154/0/0
./hermes --version       # WuBu Hermes v0.14.1+
```

### Config

```bash
export SLERMES_HOME=~/.slermes
# Config:  $SLERMES_HOME/config.yaml (~322 keys)
# Env:     $SLERMES_HOME/.env
```

### Docker

```bash
docker build -t hermes-c -f C/Dockerfile .
docker run --rm hermes-c --help
# ~20MB slim image (stripped, bookworm-slim base)
```

## Build System

```
make hermes         # Full binary (phase5) — 0 errors, 9.1MB
make plugins        # 10 .so shared objects
make tui            # ncurses TUI → hermes-tui (experimental)
make libs           # 30 library compilation units
make docs           # Doxygen HTML docs (if doxygen available)
make install-plugins  # Build + copy .so to ~/.hermes/plugins/
```

5-phase build pipeline (`phase1`→`phase5`):
1. **phase1** — 30 libraries (json, yaml, http, crypto, mcp, path, cron, db, etc.)
2. **phase2** — Agent core + CLI + 9 LLM providers
3. **phase3** — 68 tool handlers
4. **phase4** — 19 gateway platform adapters
5. **phase5** — Cron scheduler, link final binary

## What Works

| Area | Coverage | Key Components |
|------|----------|---------------|
| **Agent** | 28% (30/115) | Loop, LLM client, context mgmt, title gen, fallback routing, budget tracking, checkpoint/resume, redact/sanitize, vault, credential pool, plugin interface |
| **Providers** | 61% (11/18 quirks) | 9 native: OpenAI, OpenRouter, DeepSeek, xAI, Anthropic, Google, Azure, Bedrock, Custom. Metadata registry. Rate limiting, cost tracking |
| **Tools** | 35% (32/92) | 68 registered: terminal, file r/w, web get/search, skills, MCP, cron, process, session, memory, vision, TTS, kanban, delegate, browser (text), voice, image gen, approve, HA, discord, x_search, patch, exec_code, clarify, todo, send_message, cronjob, rate_limit |
| **Gateway** | 34% (22/64) | 19 platforms: Telegram, Discord, Slack, Matrix, Mattermost, WhatsApp, Email, Signal, SMS, Feishu, WeCom, DingTalk, QQ Bot, BlueBubbles, MSGraph Webhook, WeChat, YuanBao, Webhook, Home Assistant |
| **CLI** | 14% (13/95) | ~148 command handlers. Config, paths, display, TUI, MCP serve |
| **Cron** | 100% (3/3) | Scheduler, job store (SQLite), locking, chaining, retry, templates |
| **Config** | 67% (4/6) | ~322 YAML keys, profiles, `${VAR}` expansion, `!include`, env overrides, migration |
| **MCP** | 18% (2/11) | stdio/server transports, tool discovery, server lifecycle, OAuth |
| **ACP** | 11% (1/9) | JSON-RPC protocol, ACP server |
| **Plugins** | 38% (10/26) | 10 .so: kanban, honcho, spotify, disk_cleanup, file_memory, achievements, observability, skills, image_gen, google_meet |
| **Security** | 60% (6/10) | Sandbox escape detection, URL safety, command allowlist, rate limiting, tirith |

## Verified Stubs (Critical Gaps)

| Stub | File | Impact | ETA |
|------|------|--------|-----|
| **computer_use** | `src/tools/computer_use.c` | 5 registered tools return "not available" | Needs macOS cua-driver MCP |
| **browser CDP** | `src/tools/browser.c` | 5/11 browser tools need Chrome CDP server | WebSocket CDP client |
| **image_gen** | `src/plugins/plugin_image_gen.c` | Generates fake URLs, no real generation | Fal AI REST client |
| **TUI sessions** | `src/cli/tui_fullscreen.c` | Session browser shows "current" placeholder | DB query integration |

*See [Stub Sector S](.hermes/mind-palace/plans/battleship-v2.md#new-sector-s-stub-remediation-10-gaps) in battleship for remediation plan.*

## Test Suite

```bash
bash test_runner.sh                    # Full suite — 117 test files
bash test_runner.sh --verbose          # Per-test output
```

**154 passed, 0 failed, 0 skipped** as of 2026-05-23.

Coverage:
- **Libraries**: json, yaml, http, crypto, cron, csv, datetime, path, hash, uuid, base64, html, textwrap, glob, signal, enum, difflib, regex, ansi, json5, toml, websocket, protobuf, dotenv, proc, template, skin, tui, display
- **Agent**: 225 assertions across 9 providers, metadata, fallback, budget, checkpoint, context, title, redact, sanitize, vault, secrets, tokenizer
- **CLI**: commands, paths, display, TUI
- **Cron**: lib, tool, sqlite, extras (4 files)
- **Tools**: file, web, terminal, exec_code, session, process, todo, memory, kanban, cronjob, skills, tts, vision, clarify, delegate, x_search, patch, approval, sandbox_escape, tirith, result_storage
- **Gateway**: escape_mode, slack_blocks, discord_interactions, whatsapp_msg
- **Plugins**: All 10 .so tested
- **Cross-cut**: file_permissions, audit, audit_rotate, redact

## Bugfix History

- temperature=0.0 silent drop — all 9 providers
- response_format use-after-free — all 9 providers (json_object_set → json_copy)
- NULL stream chunk SIGSEGV — 6 providers
- cron_job_reset_retry(NULL) — strcmp on NULL SEGV
- Secrets ow pointer not advanced
- x_search auth header — literal `***` instead of `%s`
- Dockerfile wrong working dir — `RUN make` at repo root (no Makefile)
- glob.h shadowing system `<glob.h>` — renamed to `hermes_glob.h`
- path.c missing `_GNU_SOURCE` — `glob()` undeclared

## Project Structure

```
C/
├── src/                    # 75.5K LOC source
│   ├── agent/              # Provider adapters, LLM client, fallback, budget
│   ├── cli/                # CLI orchestrator, commands, config, display
│   ├── cron/               # Scheduler, job store, locking, chaining
│   ├── gateway/            # 19 platform adapters (Telegram → YuanBao)
│   │   └── platforms/      # Individual adapter implementations
│   ├── plugins/            # 10 .so plugin implementations
│   ├── tools/              # 68 tool handlers
│   ├── acp/                # ACP JSON-RPC server
│   └── main.c              # Entry point
├── lib/                    # 30 library units (compiled directly, no .a)
│   ├── libjson/            # JSON parser
│   ├── libhttp/            # HTTP client
│   ├── libmcp/             # MCP transport/client
│   ├── libdb/              # SQLite wrapper
│   ├── libcrypto/          # AES, HMAC, base64
│   └── ...                 # 25 more
├── include/                # 29 public headers
├── tests/                  # 117 test files
├── .hermes/                # Project state, plans, DA audits
│   └── mind-palace/        # Prestige system (plans, state, battleship)
│       └── plans/          # Roadmaps, DA reports
├── Dockerfile              # Multi-stage Docker build
└── Makefile                # 5-phase build pipeline
```

## Current Reality (DA v11 — 2026-05-23)

| Metric | Value |
|--------|-------|
| Source files | 115 .c + 29 .h = 144 |
| LOC (non-lib) | 75,563 |
| Libraries | 30 units (compiled as .o, linked directly) |
| Plugins | 10 .so |
| Tools registered | 68 |
| Gateway platforms | 19 |
| CLI commands | ~148 |
| Test files | 117 |
| Test assertions | ~573 |
| Binary size | 9.1MB (stripped: ~3MB) |
| CI workflows | 3 (C build, Docker, nix lockfile) |
| Upstream delta | 0 behind, 400 ahead (183 Python commits merged) |
| Total gaps | 500 (339 remaining) |

## Battleship Roadmap

Tracked via [battleship-v2.md](.hermes/mind-palace/plans/battleship-v2.md) — 500 gaps across 19 sectors:

| Sector | Gaps | Done | % |
|--------|------|------|---|
| A: Core | 16 | 12 | 75% |
| B: Agent | 115 | 30 | 26% |
| C: CLI | 95 | 13 | 14% |
| D: Tools | 92 | 32 | 35% |
| E: Gateway | 64 | 22 | 34% |
| F: MCP | 11 | 2 | 18% |
| G: ACP | 9 | 1 | 11% |
| I: TUI | 8 | 1 | 12% |
| J: Plugins | 26 | 10 | 38% |
| L: Config | 6 | 4 | 67% |
| P: Security | 10 | 6 | 60% |
| R: Provider | 18 | 11 | 61% |
| **S: Stubs** | **10** | **0** | **0%** |
| **T: Tests** | **12** | **0** | **0%** |
| **U: CI/CD** | **10** | **0** | **0%** |

*Remaining 100% sectors: N (Build/Doc, 82%), H (Cron, 100%), Q (Cross-cut, 100%)*

## Development

```bash
make -j$(nproc)           # Build
bash test_runner.sh       # Test
./hermes --help           # Smoke test

# Debug build (more warnings)
make CFLAGS="-O0 -g3 -Wall -Wextra -Wpedantic -Werror" hermes

# Address sanitizer
make CFLAGS="-O1 -g -fsanitize=address" LDFLAGS="-fsanitize=address" hermes
```

### Markers for SIGSEGV Debugging

When the release binary crashes (CUDA, large context, async):
1. Insert `fprintf(stderr, "MARK_fnname\\n");` at function boundaries
2. Rebuild the affected `.o` and run
3. Last visible marker = crash point
4. Read that section, fix error, remove markers

*See [caveman-debug workflow](https://hermes-agent.nousresearch.com/docs) for full pattern.*

## Project State Files

| File | Purpose |
|------|---------|
| `.hermes/mind-palace/state.md` | Session tracking, current work |
| `.hermes/mind-palace/plans/battleship-index.md` | Quick dashboard |
| `.hermes/mind-palace/plans/battleship-v2.md` | Full 500-gap roadmap |
| `.hermes/mind-palace/prestige_prompt.md` | Priority queue |
| `.hermes/mind-palace/da-audit-v11-500-goals.md` | Triple DA essay |

## Upstream

Python hermes-agent at [github.com/NousResearch/hermes-agent](https://github.com/NousResearch/hermes-agent). C branch is 0 behind upstream (183 Python commits merged), 400 ahead (C-specific commits).

## License

MIT — see root LICENSE file.
