# WuBu Slermes — C Translation of Hermes Agent

**HONEST STATUS: ~57% complete.** C/ translates NousResearch/hermes-agent from Python→C for zero-dependency, single-binary operation.

All 200 phases compile. LLM calls work (DeepSeek via OpenAI-compat). 19 gateway platforms. 74 tools. 72 CLI commands.

---

## Current Reality (DA v5, 2026-05-22)

| Subsystem | Completeness | Note |
|-----------|-------------|------|
| Config keys | **64%** (206/322) | 18 config groups: provider, display, agent, tools, terminal, delegation, browser, memory, compression, cron, notification, security, session, plugin, MCP, logging, skills, checkpoints |
| Tools | **92%** (74 registered) | 54 expected, all found. Missing: feishu, video, yuanbao, MoA, discord |
| CLI commands | **85%** (72/85) | All dispatch, most feature-complete |
| Providers | **31%** (9/29) | OpenAI, Anthropic, Google, OpenRouter, DeepSeek, xAI, Azure, Bedrock, Custom + OpenAI-compat |
| Gateway platforms | **95%** (19/~20) | Telegram, Discord, Slack, Signal, Matrix, WhatsApp, etc. |
| MCP | **70%** | Client + 6 tools, namespace collision handling, tool filtering |
| Plugin system | **25%** | Core .so loader + 3 examples. 12+ plugin types missing |
| Session DB | **100%** | SQLite FTS5, 231 sessions, CRUD, search, export, branch, migrate |
| Delegation | **80%** | Concurrent children, orchestrator, isolation, credential filter |
| Security | **70%** | Redaction, vault, audit, rate limit, Tirith, approvals, file sandbox |
| Agent loop | **75%** | Budget, interrupt, parallel dispatch, checkpoints (44/44 tests), compression |
| TUI | **50%** | ncurses split-pane. Missing: theme engine, session browser, config editor |
| Memory | **90%** | TTL, dedup, compression, search, auto-save, import/export |
| Cron | **90%** | SQLite store, retry, chaining, script jobs, watchdog |
| Skills | **90%** | Scan, validate, sync, bundles, curator, dependencies |
| Tests | **~1%** (15 files, 1.8K LOC) | **CRITICAL GAP.** Metadata (52/52), smoke (131/131), checkpoint (44/44) |

**Binary:** `C/hermes` — ~3.1MB static ELF, zero runtime deps.
**C LOC:** ~53,000 (Python: ~431,000 non-test = **12%** by size)
**Source files:** 121 (.c) + 21 (.h)

---

## What's Done

- **Foundation libs:** JSON, HTTP, YAML, crypto, dotenv, cron, proc, template, ncurses, SQLite, MCP, WebSocket, Protobuf, skin engine, plugin loader
- **18 config groups:** 206/322 keys. Full YAML parsing, env var override, validation, diff/show, export, v0→v1 migration
- **Agent loop:** Budget, parallel dispatch, interrupt, context compression, checkpoints, system prompt caching, streaming
- **CLI shell:** 72 commands — /help, /tools, /commands, /config, /model, /sessions, /stats, /status, /profile, /plugins, /cron, /platform, /tools-verify, /config validate/diff/export, /rollback, etc.
- **74 tools:** terminal, file, web, skills, patch, exec_code, clarify, memory, todo, process, send_message, cronjob, session_search, session_crud, tts, vision, delegate, x_search, browser suite, approval, voice, image_gen, homeassistant, kanban, computer_use, MCP suite
- **19 gateway platforms:** Telegram, Discord, Slack, Signal, Matrix, Mattermost, Email, SMS, Webhook, HomeAssistant, DingTalk, WeCom, Weixin, Feishu, QQBot, BlueBubbles, WhatsApp, MSGraph, Yuanbao
- **9 providers (native):** OpenAI, Anthropic, Google, OpenRouter, DeepSeek, xAI, Azure, Bedrock, Custom
- **Security:** Secrets redaction, credential vault (AES), audit log, rate limiting, Tirith policy, URL safety, approvals, file sandbox
- **Session DB:** SQLite FTS5, ranked search, tags, starred, JSON/Markdown export, branch, schema migration
- **Memory:** TTL, dedup, compression, search, import/export
- **Cron:** SQLite job store, 5-field cron, locking, retry, chaining, script jobs, watchdog
- **Skills:** Scanning, validation, provenance, sync, bundles, usage tracking, curator, dependencies
- **Config groups added in DA v5:** terminal (22 keys), logging (5), skills (5), checkpoints (8)

---

## What's LEFT

1. **Config keys:** 116 missing — auxiliary(56), tts(17), discord(12), kanban(10), + small groups
2. **Providers:** 20 missing — Nous, Alibaba, StepFun, Minimax, Novita, ZAI, HuggingFace, Arcee, Ollama Cloud, Nvidia, GMI, KiloCode, Kimi, AI Gateway, Azure Foundry, Xiaomi, Qwen OAuth, Copilot ACP, OpenCode Zen, Codex
3. **Tools:** 14 missing — feishu (5), video (2), yuanbao (6), mixture_of_agents (1)
4. **Plugins:** 12+ types — memory providers, context engine, model providers, image/video gen, browser, achievements, observability
5. **TUI:** 6/12 phases — theme engine, session browser, config editor, image viewer, gateway backend, mobile mode
6. **MCP depth:** Resource access, sampling, prompt templates, root filesystem
7. **Tests:** ~17,000 Python test cases. **CRITICAL GAP.**

Full 200-gap tracker: `C/.hermes/mind-palace/plans/200-gap-roadmap.md`

---

## Quick Start

```bash
# Build
make -j$(nproc) -C C

# Run
./C/hermes --help
./C/hermes --version
./C/hermes "Ask me anything"

# Config (set env vars or use .env file)
export SLERMES_HOME=~/.slermes
export HERMES_MODEL=deepseek-v4-flash
export HERMES_PROVIDER=deepseek
export HERMES_BASE_URL=https://api.deepseek.com/v1
export HERMES_API_KEY=sk-your-key
```

Config reads from `$SLERMES_HOME/config.yaml` + `$SLERMES_HOME/.env`.
All key-value pairs supported in .env: `SLERMES_API_KEY`, `DEEPSEEK_API_KEY`, `HERMES_MODEL`, `HERMES_PROVIDER`, `HERMES_BASE_URL`, etc.

## Smoke Tests

```bash
./C/hermes --help           # Usage info
./C/hermes --version         # Version
echo "/status" | ./C/hermes  # Session status
echo "/tools"  | ./C/hermes  # 74 registered tools
echo "/sessions" | ./C/hermes # 231 sessions in DB
./C/hermes "Hello"           # LLM call (requires API key)
```

## Remotes

| Remote | URL | Direction |
|--------|-----|-----------|
| `origin` | `https://github.com/NousResearch/hermes-agent.git` | Pull upstream |
| `wubu` | `git@github.com:waefrebeorn/hermes-agent.git` | Push our work |

## Architecture

```
main.c
 ├── hermes_cli_main()     → Interactive CLI (72 slash commands)
 ├── hermes_gateway_main() → Multi-platform messaging gateway (19 platforms)
 ├── hermes_cron_main()    → Background job scheduler (SQLite-backed)
 └── tui_fullscreen_run() → ncurses split-pane TUI

CLI flow:
 config.c → load YAML + .env → agent_init → tools_init_all → agent_chat
  → llm_client → provider_* (9 native implementations)
  → HTTP request → JSON parse → response

All tools: src/tools/ (36 files → 74 tools)
All providers: src/agent/provider_*.c (11 files → 9 provider implementations)
All platforms: src/gateway/platforms/ (19 .c files)
```

## Why C?

- No Python runtime. No pip. No venv. No dependency hell.
- Single binary (~3.1MB). SCP to any Linux box and run.
- No GIL, no interpreter overhead, no GC pauses.
- All 3rd-party deps built in (libjson, libhttp, libyaml, libcrypto, libmcp, libcron, etc.)

## Fork-only. No upstream PRs until full translation done.

Then triple Devil's Advocate audit with mind-palace structure before any merge request.
