# WuBu Slermes — C Translation of Hermes Agent

**HONEST STATUS: ~45% complete.** C/ translates NousResearch/hermes-agent from Python→C for zero-dependency, single-binary operation.

All 200 phases compile. LLM calls work (DeepSeek, OpenAI, Anthropic, Google). 19 gateway platforms. 74 tools. 72 CLI commands.

---

## Current Reality (DA audit 2026-05-21)

| Subsystem | Completeness | Note |
|-----------|-------------|------|
| Config keys | **48%** (154/318) | Flat + v1 nested YAML, env var override, profiles |
| Tools | **92%** (74 registered) | 54 expected, all found. Missing: feishu, video, yuanbao, MoA |
| CLI commands | **85%** (72/85) | All dispatch, most feature-complete. Missing: /clear |
| Providers | **10%** (3/29) | OpenAI, Anthropic, Google + OpenAI-compat (DeepSeek, any) |
| Gateway platforms | **95%** (19/~20) | Telegram, Discord, Slack, Signal, Matrix, WhatsApp, etc. |
| MCP | **70%** | Client + 6 tools: status, call, auth, resources, prompts |
| Plugin system | **25%** | Core .so loader + 3 examples. 12+ plugin types missing |
| Session DB | **100%** | SQLite FTS5, 231 sessions, CRUD, search, export, branch |
| Delegation | **80%** | Concurrent children, orchestrator, isolation, credential filter |
| Security | **70%** | Redaction, vault, audit, rate limit, Tirith policy, approvals |
| Agent loop | **75%** | Budget, interrupt, parallel dispatch, checkpoints, compression |
| TUI | **50%** | ncurses split-pane. Missing: theme engine, session browser |
| Memory | **90%** | TTL, dedup, compression, search, auto-save, import/export |
| Cron | **90%** | SQLite store, retry, chaining, script jobs, watchdog mode |
| Skills | **90%** | Scan, validate, sync, bundles, curator, dependencies |
| Tests | **<1%** (12 files, 1.3K LOC) | **CRITICAL GAP.** Manual verification only |

**Binary:** `C/hermes` — ~2.9MB static ELF, zero runtime deps.
**C LOC:** ~53,000 (Python: ~431,000 non-test = **12%** by size)
**Source files:** 121 (.c) + 21 (.h)
**Tool schemas:** All include `"type": "object"` — works with strict providers (DeepSeek, etc.)

---

## What's Done (the 45%)

- **Foundation libs:** JSON, HTTP, YAML, crypto, dotenv, cron, proc, template, ncurses, SQLite, MCP, WebSocket, Protobuf, skin engine, plugin loader
- **Agent loop:** Full loop with budget tracking, interrupt handling, parallel tool dispatch, context compression, checkpoints, grace call, system prompt caching, prefill messages, streaming
- **CLI shell:** 72 commands, all dispatch — /help, /tools, /commands, /config, /model, /sessions, /stats, /status, /whoami, /profile, /plugins, /cron, /platform, /tools-verify, etc.
- **74 tools:** terminal, file, web, skills, patch, exec_code, clarify, memory, todo, process, send_message, cronjob, session_search, session_crud, tts, vision, delegate, x_search, browser suite, approval, voice, image_gen, homeassistant, kanban, computer_use, discord, discord_admin, MCP suite (status, tool_call, auth, resources, prompts)
- **19 gateway platforms:** Telegram, Discord, Slack, Signal, Matrix, Mattermost, Email, SMS, Webhook, HomeAssistant, DingTalk, WeCom, Weixin, Feishu, QQBot, BlueBubbles, WhatsApp, MSGraph, Yuanbao
- **3 providers:** OpenAI (chat/stream/tools/vision), Anthropic (Messages API/streaming/tools/thinking), Google (Gemini API/tool calling)
- **Security:** Secrets redaction, credential vault (AES-encrypted), audit log, rate limiting, Tirith policy, URL safety, approvals, website blocklist
- **Session DB:** Full SQLite with FTS5, ranked search, tags, starred sessions, JSON/Markdown export, branch at message N, schema migration
- **Memory:** Storage abstraction, TTL, dedup, priority, search, compression, import/export
- **Cron:** SQLite job store, full 5-field cron, locking, retry, chaining, script jobs, watchdog mode
- **Skills:** Scanning, validation, provenance, sync from remote hub, bundles, usage tracking, LRU caching, curator, dependency resolution
- **Config:** 154 keys across 15 groups (provider, display, agent, tools, delegation, browser, memory, compression, cron, notification, security, session, MCP, display), env var overrides, profiles, diff/show, export/import, v0→v1 migration

## What's LEFT (the 55%)

1. **Config keys (P1-P25 gap):** 164 missing keys — credential_pool, fallback_providers, toolset arrays, web.*, approval subkeys, logging.*, kanban.*, stt/tts/vision/auxiliary sub-dicts, gateway timeouts
2. **Providers (P71-P85 gap):** 26 missing — OpenRouter, Groq, Together, Bedrock, Azure, xAI, etc. **Biggest gap by count.**
3. **Tools (P41-P55 gap):** 14 missing — feishu (5), video (2), yuanbao (6), mixture_of_agents (1)
4. **Plugins (P126-P140):** 12+ plugin types — memory providers, context engine, model providers, image/video gen, browser, achievements, observability
5. **TUI (P189-P200):** Split panes exist, missing: theme engine, session browser, config editor, image viewer, gateway backend, mobile mode
6. **Security (P159-P168 gap):** File sandbox (directory restriction)
7. **Tests:** ~17,000 test cases from Python. **CRITICAL GAP.**
8. **Runtime verification:** Not yet verified vs Python ground truth for gateway, MCP, delegation, plugin, cron, TUI

Full 200-phase detail: `C/.hermes/mind-palace/plans/200-phase-roadmap.md`

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
  → llm_client → provider_* (OpenAI-compat / Anthropic / Google)
  → HTTP request → JSON parse → response

All tools: src/tools/ (36 files → 74 tools)
All providers: src/agent/provider*.c (5 files → 3 provider implementations)
All platforms: src/gateway/platforms/ (19 .c files)
```

## Why C?

- No Python runtime. No pip. No venv. No dependency hell.
- Single binary (~2.9MB). SCP to any Linux box and run.
- No GIL, no interpreter overhead, no GC pauses.
- All 3rd-party deps built in (libjson, libhttp, libyaml, libcrypto, libmcp, libcron, etc.)

## Fork-only. No upstream PRs until full translation done.

Then triple Devil's Advocate audit with mind-palace structure before any merge request.
