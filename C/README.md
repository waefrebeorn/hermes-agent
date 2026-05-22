# Slermes C — C Translation of Hermes Agent

**Status: ~45% complete.** 53K LOC C vs 431K Python. All 200 phases compile. LLM calls work (DeepSeek). Runtime verification: partial.

## What's Here

Full C reimplementation of NousResearch/hermes-agent. Single binary (~2.9MB static), zero Python deps.

| Metric | Current | Python target | Coverage |
|--------|---------|---------------|----------|
| Non-test LOC | ~53,000 | ~431,000 | 12% |
| Source files | 121 (.c) + 21 (.h) | ~900 | 13% |
| Binary size | 2.9MB static | N/A | N/A |
| Config keys | 154 | 318 leaf keys in DEFAULT_CONFIG | **48%** |
| Tools | 74 registered (54 expected, all found) | ~80 | **92%** |
| CLI commands | 72 | 85 | **85%** |
| Providers | 3 (openai/anthropic/google) | 29 plugins | **10%** |
| Gateway platforms | 19 | ~20 | **95%** |
| MCP | Core client + 6 tools | Full framework | **70%** |
| Plugins | Core system + 3 examples | 12+ | **25%** |
| Session DB | SQLite + FTS5, 231 sessions | Full | **100%** |
| Delegation | Concurrent, orchestrator, isolation | Full | **80%** |
| Security | Redaction, vault, audit, rate limit | Full | **70%** |
| Agent loop | Budget, parallel dispatch, interrupt, checkpoints | Full | **75%** |
| TUI | Core ncurses split-pane | React/Ink | **50%** |
| Memory | TTL, dedup, compression, search | Full | **90%** |
| Cron | SQLite store, retry, chaining | Full | **90%** |
| Skills | Scan, validate, sync, bundles, curator | Full | **90%** |
| Tests | 12 files, 1,328 LOC | ~900 files, 430K LOC | **<1%** |

## Build & Run

```bash
make -j$(nproc)
./hermes --help
./hermes "Ask me anything"

# Set config via env:
export SLERMES_HOME=~/.slermes
export HERMES_MODEL=deepseek-v4-flash
export HERMES_PROVIDER=deepseek
export HERMES_BASE_URL=https://api.deepseek.com/v1
export HERMES_API_KEY=sk-your-key
```

Config reads from `$SLERMES_HOME/config.yaml` + `$SLERMES_HOME/.env`.
All key-value pairs supported in .env: `SLERMES_API_KEY`, `DEEPSEEK_API_KEY`, `HERMES_MODEL`, `HERMES_PROVIDER`, `HERMES_BASE_URL`, etc.

## Key Files

| Path | Content |
|------|---------|
| `src/main.c` | Entry point: CLI, gateway, cron, TUI dispatcher |
| `src/cli/` | CLI orchestrator, config loading, 72 slash commands, display |
| `src/agent/` | Agent loop, LLM client, 3 providers, context, budget, checkpoints |
| `src/tools/` | 36 tool files → 74 registered tools |
| `src/gateway/platforms/` | 19 platform adapters (telegram, discord, slack, etc.) |
| `src/cron/` | Scheduler + SQLite-backed job store |
| `lib/` | 15 library modules (json, http, yaml, db, mcp, crypto, etc.) |
| `include/hermes.h` | Master header: all config structs, agent state, tool registry |
| `.hermes/mind-palace/` | Prestige system: plans, DA audits, status tracking |

## Full Roadmap

`../ROADMAP.md` at repo root for the complete 200-phase plan and honest gap analysis.

## Architecture

```
main.c
 ├── hermes_cli_main()     → Interactive CLI with 72 slash commands
 ├── hermes_gateway_main() → Multi-platform messaging gateway (19 platforms)
 ├── hermes_cron_main()    → Background job scheduler
 └── tui_fullscreen_run() → ncurses split-pane TUI

CLI flow:
 config.c → load YAML + .env → agent_init → tools_init_all → agent_chat → llm_client → provider_* → HTTP → JSON parse
```

All providers use OpenAI-compatible API format by default. Anthropic and Google use native formats. Config supports v1 nested YAML and flat v0 format.

## Testing

```bash
# Manual smoke tests:
./hermes --help
./hermes --version
echo "/status" | ./hermes
echo "/tools"  | ./hermes
echo "/sessions" | ./hermes

# LLM test (requires API key):
./hermes "Hello"
```
