# Hermes C — AI Agent Runtime in C

**Standalone C translation of the Python [Hermes Agent](https://github.com/NousResearch/hermes-agent).**
One binary. Zero runtime deps beyond libc + libssl. 9.1MB ELF.

```text
Suite:  154/0/0  (117 tests, ~573 assertions)
Binary: 9.1MB   (stripped: ~3MB)
Source: 115 .c + 29 .h = 144 files, 75.5K LOC
Parity: ~32%   (161/500 gaps, [battleship v3](.hermes/mind-palace/plans/battleship-v2.md))
```

## Quick Start

```bash
make -j$(nproc)          # Build
./hermes --help          # Smoke test
bash test_runner.sh      # 154/0/0
```

### Docker

```bash
docker build -t hermes-c -f Dockerfile .
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
CLI/Gateway → Agent Loop → LLM Client → 9 Providers
                 ↓
          68 Tools Registry
                 ↓
          10 Plugin .so
                 ↓
          30 Library Compilation Units
```

**Data flow:** User message → CLI/Gateway → Agent Loop → LLM provider call → tool execution → plugin dispatch → response.

## Build

```bash
make hermes         # Full binary (phase5)
make plugins        # 10 .so shared objects
make tui            # ncurses TUI (experimental)
make libs           # 30 library units
make install-plugins  # Copy .so → ~/.hermes/plugins/
```

5 phases: libs → agent/CLI → tools → gateway → cron + link.

## What's Inside

| Area | Status | Highlights |
|------|--------|------------|
| **Providers** | 9 native + metadata | OpenAI, Anthropic, Google, Azure, Bedrock, DeepSeek, xAI, OpenRouter, Custom |
| **Tools** | 68 registered | terminal, file, web, skills, MCP, cron, session, memory, vision, TTS, kanban, browser, delegate, patch, exec_code, 11 more |
| **Gateway** | 19 platforms | Telegram, Discord, Slack, Matrix, WhatsApp, Email, Signal, SMS, 12 more |
| **CLI** | ~148 commands | Config, paths, display, TUI, MCP serve |
| **Plugins** | 10 .so | kanban, honcho, spotify, disk_cleanup, file_memory, achievements, observability, skills, image_gen, google_meet |
| **Cron** | 100% | Scheduler, SQLite store, retry, chaining, templates |
| **MCP** | stdio + server | Tool discovery, server lifecycle, OAuth (in progress) |

## Verified Stubs (4 Critical)

| Feature | Why Stub | Files Affected |
|---------|----------|----------------|
| **computer_use** | Needs macOS cua-driver MCP | `src/tools/computer_use.c` |
| **browser CDP** | Needs Chrome/Playwright WebSocket | `src/tools/browser.c` (5/11 tools) |
| **image_gen** | Generates fake URLs | `src/plugins/plugin_image_gen.c` |
| **TUI sessions** | Hardcoded "current" entry | `src/cli/tui_fullscreen.c` |

## Project Structure

```
├── src/
│   ├── agent/           # Provider adapters, LLM client, fallback, budget
│   ├── cli/             # CLI orchestrator, commands, config, display
│   ├── cron/            # Scheduler, job store, locking
│   ├── gateway/         # 19 platform adapters
│   │   └── platforms/   # Individual adapters
│   ├── plugins/         # 10 .so implementations
│   ├── tools/           # 68 tool handlers
│   ├── acp/             # ACP JSON-RPC server
│   └── main.c           # Entry point
├── lib/                 # 30 library units (compiled into binary)
├── include/             # Public headers
├── tests/               # 117 test files
├── .hermes/mind-palace/ # Project state, plans, DA audits
│   └── plans/           # Roadmaps, battleship index
└── Dockerfile           # Multi-stage build
```

## Test Coverage

```bash
bash test_runner.sh --verbose
```

Areas: Libraries (all 30), Agent (225 assertions, 9 providers), CLI, Cron, Tools (25+), Gateway (5 platforms), Plugins (all 10), Cross-cut.

## Roadmap

500 gaps across 19 sectors tracked in [battleship-v2.md](.hermes/mind-palace/plans/battleship-v2.md).
Top priorities: stub remediation (S sector), test infrastructure (T), CI/CD hardening (U).

## License

MIT.
