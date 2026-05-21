# WuBu Slermes — C Translation of Hermes Agent

**HONEST STATUS: ~8% complete.** C/ translates NousResearch/hermes-agent from Python→C for zero-dependency, single-binary operation.

Built for speed. Built for portability. Built by rewriting everything.

---

## Dashboard (what you see on GitHub at a gas station)

| Metric | Value |
|--------|-------|
| **C LOC** | ~57,000 (Python: ~433,000+ = **8%**) |
| **Binary** | `C/hermes` alias `slermes` — ~1.4MB ELF |
| **Tools** | 53 registered (30 stubs, 15 still missing from Python) |
| **CLI commands** | 72 names (most printf stubs — ~45% real impl) |
| **Config keys** | 16 of 424+ (**3.8%** — biggest gap) |
| **Providers** | 3 of 29+ (OpenAI/Anthropic/Google) |
| **Gateway platforms** | 19 (feature depth still shallow) |
| **MCP** | **0%** — no dynamic tools |
| **Plugins** | **0%** — 17 plugin types, zero |
| **Agent loop** | 332 LOC vs Python's 12,000 (**3%**) |
| **TUI** | 926 LOC ncurses vs Python's 41K React/Ink (**2%**) |
| **Tests** | 43 basic unit tests vs Python's ~17,000 |
| **Session DB** | grep-based (no SQLite FTS5) |
| **Delegation** | basic subprocess only (no concurrency/orchestrator) |
| **Bugs fixed** | 5 (Content-Type, chunked TE, skin default, .env, WEBHOOK_PORT) |
| **Git commits** | 51+ on wubu/main |

**Can you use this as your daily driver? No.** Not even close. That's the honest answer.

---

## What's Done (the 8%)

- **Foundation libs:** JSON, HTTP, YAML, crypto, dotenv, cron, proc, template, ncurses, SQLite wrappers
- **Agent loop:** Basic loop (332 LOC) — no budget, no fallback, no credential pool
- **CLI shell:** Works. 72 commands exist, most need real implementation
- **53 tools:** Many work, 30 are minimal/stubs. Missing: discord, feishu, MoA, video, yuanbao
- **19 gateway platforms:** Telegram, Discord, Slack, Signal, Matrix, Mattermost, Email, SMS, Webhook, HomeAssistant, DingTalk, WeCom, Weixin, Feishu, QQBot, BlueBubbles, WhatsApp, MSGraph, Yuanbao
- **Security:** Basic URL safety + path traversal + Tirith policy + approval system
- **3 providers:** OpenAI, Anthropic, Google — no credential pool
- **Scheduler:** Basic cron loop (no SQLite persistence)

## What's LEFT (the 92% — 200-phase roadmap)

1. **Config keys (P1-P25):** 408 missing. **#1 blocker.** Nothing configurable without config.
2. **CLI commands (P26-P40):** Full implementations for all 69+ commands.
3. **Tools (P41-P55):** Port 15 missing tools, deepen 30 stubs.
4. **MCP (P56-P70):** Dynamic tool system. **#2 blocker.** 
5. **Providers (P71-P85):** 26 provider ports + credential pool + budget tracking.
6. **Agent loop (P86-P100):** Budget, fallback models, checkpoints, streaming.
7. **Gateway depth (P101-P115):** Full platform feature parity.
8. **Delegation (P116-P125):** Concurrent children, orchestrator mode.
9. **Plugin system (P126-P140):** 17 plugin types (memory, kanban, image_gen, etc.)
10. **Session DB (P141-P150):** SQLite FTS5 session search.
11. **Memory (P151-P158):** Provider-backed memory with TTL/dedup/search.
12. **Security (P159-P168):** Redaction, blocklist, allowlist, audit log.
13. **Cron (P169-P178):** SQLite-backed scheduler with retry/chaining.
14. **Skills (P179-P188):** Hub/sync/provenance/bundles.
15. **TUI (P189-P200):** React/Ink parity with split panes, streaming, theme engine.
16. **Testing:** 43 → ~17,000 tests.
17. **Infrastructure:** CI pipeline, docs site, ACP adapter, LSP integration.

Full 200-phase detail: `C/.hermes/mind-palace/plans/200-phase-roadmap.md`

---

## Quick Start

```bash
# Build
make -j$(nproc) -C C

# Run
./C/hermes "Hello world"
slermes "Hello world"   # same thing, aliased

# Build + test + parity
bash scripts/slermes-build.sh
```

## Remotes

| Remote | URL | Direction |
|--------|-----|-----------|
| `origin` | `https://github.com/NousResearch/hermes-agent.git` | Pull upstream |
| `wubu` | `git@github.com:waefrebeorn/hermes-agent.git` | Push our work |

## Why C?

- No Python runtime. No pip. No venv. No dependency hell.
- Single binary. SCP to any Linux box and run.
- No GIL, no interpreter overhead, no GC pauses.
- ~1.4MB executable with everything built in.

## Fork-only. No upstream PRs until full translation done.

Then triple Devil's Advocate audit with mind-palace structure before any merge request.
