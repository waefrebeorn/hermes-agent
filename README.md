# Hermes C — AI Agent Runtime

**Standalone C translation of the Python [Hermes Agent](https://github.com/NousResearch/hermes-agent).** One binary. Zero runtime deps beyond libc + libssl. 9.1MB ELF.

```
Suite:  154/0/0  (117 tests, ~573 assertions)
Binary: 9.1MB   (stripped: ~3MB)
Source: 115 .c + 29 .h = 144 files, 75.5K LOC
Parity: ~32%   (161/500 gaps, battleship v3)
```

## Quick Start

```bash
cd C/
make -j$(nproc)          # Build
./hermes --help          # Smoke test
bash test_runner.sh      # 154/0/0
```

### Docker

```bash
docker build -t hermes-c -f C/Dockerfile .
docker run --rm hermes-c --help
```

### Config

```bash
export SLERMES_HOME=~/.slermes
# $SLERMES_HOME/config.yaml (~322 keys)
# $SLERMES_HOME/.env
```

## Architecture

```
CLI/Gateway → Agent Loop → LLM Client → 9 Providers (OpenAI-compat + native)
                 ↓
          68 Tools Registry
                 ↓
          10 Plugin .so
                 ↓
          30 Library Compilation Units
```

## Build

```bash
make hermes         # Full binary (phase5)
make plugins        # 10 .so shared objects
make tui            # ncurses TUI (experimental)
make libs           # 30 library units
make install-plugins  # Copy .so → ~/.hermes/plugins/
```

5-phase pipeline: libs → agent/CLI → tools → gateway → cron + link.

## What's Inside

| Area | Status | Key Components |
|------|--------|---------------|
| **Providers** | 9 native + metadata | OpenAI, OpenRouter, DeepSeek, xAI, Anthropic, Google, Azure, Bedrock, Custom |
| **Tools** | 68 registered | terminal, file, web, skills, MCP, cron, session, memory, vision, TTS, kanban, browser (text), delegate, patch, exec_code, voice, image, HA, discord, x_search, approval, process, todo, clarify, send_message, cronjob, rate_limit |
| **Gateway** | 19 platforms | Telegram, Discord, Slack, Matrix, Mattermost, WhatsApp, Email, Signal, SMS, Feishu, WeCom, DingTalk, QQ Bot, BlueBubbles, MSGraph Webhook, WeChat, YuanBao, Webhook, Home Assistant |
| **CLI** | ~148 commands | Config (322 keys), paths, display, TUI, MCP serve |
| **Plugins** | 10 .so | kanban, honcho, spotify, disk_cleanup, file_memory, achievements, observability, skills, image_gen, google_meet |
| **Cron** | 100% | Scheduler, SQLite store, retry, chaining, templates |
| **MCP** | stdio + server | Tool discovery, server lifecycle, OAuth |
| **ACP** | JSON-RPC | ACP protocol server |
| **Config** | ~322 keys | Profiles, `${VAR}` expansion, `!include`, env overrides, migration |

## Verified Stubs (4 Critical)

| Feature | Why Stub | File | Remediation |
|---------|----------|------|-------------|
| **computer_use** | Needs macOS cua-driver MCP | `src/tools/computer_use.c` | MCP client for cua-driver |
| **browser CDP** | Needs Chrome/Playwright CDP | `src/tools/browser.c` (5/11 tools) | WebSocket CDP client |
| **image_gen** | Generates fake URLs | `src/plugins/plugin_image_gen.c` | Fal AI REST client |
| **TUI sessions** | Hardcoded "current" entry | `src/cli/tui_fullscreen.c` | DB-backed session list |

## Test Suite

```bash
bash test_runner.sh                    # Full suite
bash test_runner.sh --verbose          # Per-test output
```

**154 passed, 0 failed, 0 skipped** (2026-05-23).

Coverage: libraries (all 30), agent (225 assertions, 9 providers), CLI, cron (4 files), tools (25+), gateway (5 platforms), plugins (all 10), cross-cut.

## Bugfix History

- temperature=0.0 silent drop — all 9 providers
- response_format use-after-free — all 9 providers (json_copy fix)
- NULL stream chunk SIGSEGV — 6 providers
- cron_job_reset_retry(NULL) — strcmp on NULL SEGV
- Secrets ow pointer not advanced
- x_search auth header — literal `***` instead of `%s`
- Dockerfile wrong working dir — `RUN make` at repo root (no Makefile)
- glob.h shadowing system `<glob.h>` — renamed to `hermes_glob.h`
- path.c missing `_GNU_SOURCE` — `glob()` undeclared

## Project Structure

```
hermes-agent/                   # ← You are here (root of this repo)
├── C/                          # All source code
│   ├── src/                    #   75.5K LOC
│   │   ├── agent/              #   Provider adapters, LLM client, fallback
│   │   ├── cli/                #   CLI orchestrator, commands, config
│   │   ├── cron/               #   Scheduler, job store
│   │   ├── gateway/            #   19 platform adapters
│   │   │   └── platforms/
│   │   ├── plugins/            #   10 .so implementations
│   │   ├── tools/              #   68 tool handlers
│   │   ├── acp/                #   ACP server
│   │   └── main.c
│   ├── lib/                    #   30 library units
│   ├── include/                #   29 public headers
│   ├── tests/                  #   117 test files
│   └── Dockerfile
├── .github/workflows/          # CI (C build, Docker, nix lockfile)
├── .hermes/                    # Agent state (for development sessions)
├── nix/                        # Nix packaging (Python side, upstream)
└── README.md                   # ← This file
```

## Roadmap

500 gaps across 19 sectors tracked in `C/.hermes/mind-palace/plans/battleship-v2.md`:

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
| S: Stubs | 10 | 0 | 0% |
| T: Tests | 12 | 0 | 0% |
| U: CI/CD | 10 | 0 | 0% |
| **Total** | **~500** | **~161** | **32%** |

Top priorities: stub remediation (S), test infrastructure (T), CI/CD hardening (U).

## Development

```bash
# Debug build (more warnings, no optimization)
make CFLAGS="-O0 -g3 -Wall -Wextra -Wpedantic -Werror" hermes

# Address sanitizer
make CFLAGS="-O1 -g -fsanitize=address" LDFLAGS="-fsanitize=address" hermes

# Marker-based SIGSEGV debugging
# 1. Add fprintf(stderr, "MARK_fnname\\n") at function boundaries
# 2. Rebuild, run. Last visible marker = crash point
# 3. Fix, remove markers, verify
```

## Upstream

Python [Hermes Agent](https://github.com/NousResearch/hermes-agent) by Nous Research.
C branch is 0 behind upstream (183 Python commits merged), 400 ahead (C-specific commits).

## License

MIT.
