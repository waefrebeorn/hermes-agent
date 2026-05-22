# WuBu Slermes — C Translation of Hermes Agent

**HONEST STATUS: ~63% complete (329-gap scope).** C/ translates NousResearch/hermes-agent from Python→C for zero-dependency, single-binary operation.

LLM calls work (DeepSeek v4 Flash via OpenAI-compat). 19 gateway platforms. 74 tools. 72 CLI commands. All 200 original phases compile. **~1,422 test assertions pass across 58 test suites.**

**May 22: Re-baselined from 200-gap (94%) → 329-gap (63%).** Systematic walkthrough of Python codebase found 100+ new gaps across agent features, CLI, plugins, gateway depth, TUI, ACP adapter, batch serving, and i18n/web.

---

## Current Reality (DA v6, 2026-05-22)

| Subsystem | Completeness | Note |
|-----------|-------------|------|
| Config keys | **~99%** | All 322 leaf keys done |
| Providers | **90%** (26/29) | 3 ACP providers missing |
| CLI commands | **70%** | 72 done, 33 remain |
| Tools | **82%** (74/86) | 12 missing (feishu, video, MoA, yuanbao) |
| Gateway platforms | **80%** | 19 platforms + 10 depth features remain |
| MCP | **70%** | 4 phases remain |
| Plugin system | **30%** | 19 plugin types, only core loader done |
| Security | **85%** | All 5 subsystems tested |
| Agent loop | **55%** | 5 original + 15 new gaps |
| TUI | **35%** | 6 original + 10 new features |
| ACP Adapter | **0%** | 5 phases, 5K LOC to port |
| Batch/Serving | **0%** | 5 phases |
| I18N/Web | **0%** | 4 phases |
| Tests | **~2%** | 58 runner items vs 1,179 Python files |

**Binary:** `hermes` — ~3.4MB static ELF, zero runtime deps.
**C LOC:** ~47,700 (.c + .h) | **Source files:** 99 (.c) + 21 (.h)
**Test suites:** 58 runner items, ~1,422 assertions across 27 test files

---

## What's Done

- **Foundation libs:** JSON, HTTP, YAML, crypto, dotenv, cron, proc, template, ncurses TUI, SQLite, MCP, WebSocket, Protobuf, skin engine, plugin loader
- **All config keys:** 322/322 leaf keys. Full YAML parsing, env var override, validation, diff/show, export, schema, v0→v1 migration
- **26 providers:** OpenAI, Anthropic, Google, OpenRouter, DeepSeek, xAI, Azure, Bedrock, Custom + 17 OpenAI-compat aliases
- **Agent loop:** Budget (104 tests), interrupt, context compression (161 tests), checkpoints (44 tests), streaming, snapshot/undo
- **CLI shell:** 72 commands — /help, /tools, /commands, /config, /model, /sessions, /stats, /status, /profile, /plugins, /cron, /platform, /tools-verify, etc.
- **74 tools:** terminal, file, web, skills, patch, exec_code, clarify, memory, todo, process, send_message, cronjob, session_search, session_crud, tts, vision, delegate, x_search, browser suite (14), approval, voice, image_gen, homeassistant, kanban (10), computer_use, discord (2), MCP suite (6)
- **19 gateway platforms:** Telegram, Discord, Slack, Signal, Matrix, Mattermost, Email, SMS, Webhook, HomeAssistant, DingTalk, WeCom, Weixin, Feishu, QQBot, BlueBubbles, WhatsApp, MSGraph, Yuanbao
- **Security:** Redaction, credential pool, audit log (20 tests), rate limiting (168), URL safety (55), approvals (52), allowlist (34), file sandbox
- **Session DB:** SQLite FTS5, 410 sessions, tags, starred, JSON/MD export, branch, migrate
- **Memory, Cron, Skills:** Full suite with TTL, dedup, schedules, validation, curation

---

## What's LEFT

Full list: `C/.hermes/mind-palace/plans/300-gap-mega-roadmap.md`

**10 largest gaps:**
1. ACP adapter (server, session, tools, permissions) — 5K LOC
2. Agent features (error classifer, tool executor, prompt builder, context compressor, shell hooks, etc.)
3. TUI depth (16 phases — streaming markdown, model/session picker, agents overlay)
4. CLI commands (33 — auth, backup, doctor, undo, save, load, stats, model, etc.)
5. Plugins (19 types — memory providers, model providers, browser, Spotify, etc.)
6. Gateway depth (feishu comment, yuanbao proto/media/sticker, WeCom crypto)
7. Missing tools (feishu, video, MoA, yuanbao — 12 total)
8. Batch/serving (batch runner, MCP serve mode, trajectory compressor)
9. I18N/web (16 locales, 86-file web dashboard)
10. 3 ACP providers (Copilot, OpenCode Zen, Codex)

---

## Quick Start

```bash
make -j$(nproc)          # Build
./hermes --help           # Usage
./hermes --version        # Version string
./test_runner.sh          # 58 suites, ~1,422 assertions
echo "/tools" | ./hermes # 74 tools
./hermes "Hello"          # LLM call (needs API key)
```

## Build Targets

```bash
make phase1    # Libraries only
make phase2    # + Agent + CLI
make phase3    # + Tools
make phase4    # + Gateway
make phase5    # + Cron (full binary)
make tui       # + ncurses TUI (hermes-tui binary)
```
