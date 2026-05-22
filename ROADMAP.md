# WuBu Slermes — C Translation Roadmap

**HONEST STATUS: ~57% complete.** 53K LOC C vs 431K Python. All 200 phases compile.
LLM runtime: WORKING (DeepSeek v4 Flash via OpenAI-compat). Runtime verified: config, CLI, tools, session DB, LLM.
NOT yet runtime-verified: Gateway live traffic, MCP, delegation, plugin depth, TUI features.

---

## Current Reality (DA v5, 2026-05-22)

| Subsystem | Completeness | Gap |
|-----------|------------|-----|
| Config keys | **64%** (206/322) | 116 missing. Terminal(22)/logging(5)/skills(5)/checkpoints(8) added in DA v5. Largest remaining: auxiliary(56), tts(17), discord(12), kanban(10). |
| Tools | **92%** (74 registered, 54 expected all found) | 14 missing: discord, feishu doc+drive (5), video analyze+generate (2), yuanbao (6), MoA (1). |
| CLI commands | **85%** (72/85) | Missing: /undo, /save/load, /stats, /compress, /plugins, /cron, /platform, /approve/deny, /reset/retry, /branch, /conv/history, /model, /topic. |
| Providers | **31%** (9/29) | 9 native: OpenAI, Anthropic, Google, OpenRouter, DeepSeek, xAI, Azure, Bedrock, Custom. 20 missing: Nous, Alibaba, StepFun, Minimax, Novita, ZAI, HuggingFace, Arcee, Ollama Cloud, Nvidia, GMI, KiloCode, Kimi, AI Gateway, Azure Foundry, Xiaomi, Qwen OAuth, Copilot ACP, OpenCode Zen, Codex. |
| Gateway platforms | **95%** (19/~20) | Full platform set. Missing OAuth flows, per-platform feature depth (inline queries, polls, forum topics). |
| MCP | **70%** | Core client + 6 tools, namespace collision handling, tool filtering. Missing: resource access, sampling, prompt templates, root filesystem. |
| Plugin system | **25%** | Core .so loader + 3 examples. Missing: 12+ plugin types (memory providers, context engine, model providers, image/video gen, browser, achievements, observability). |
| Session DB | **100%** | SQLite FTS5, 231 sessions, CRUD/search/export/branch/migrate. |
| Delegation | **80%** | Concurrent children, orchestrator, isolation, credential filtering. Missing: nested delegation depth, result aggregation, budget delegation. |
| Security | **70%** | Redaction, vault, audit log, rate limit, Tirith, approvals, file sandbox (P168). |
| Agent loop | **75%** | Budget tracking, parallel dispatch, interrupt handling, checkpoint manager (P98, 44/44 tests), compression. Missing: grace call, prefill messages, stream diagnostic, background review. |
| TUI | **50%** | Core ncurses split-pane. Missing: theme engine, session browser, config editor, image viewer, gateway backend, mobile mode. |
| Memory | **90%** | TTL, dedup, compression, search, auto-save, import/export. |
| Cron | **90%** | SQLite store, retry, chaining, script jobs, watchdog. |
| Skills | **90%** | Scan, validate, sync, bundles, curator, dependencies. |
| Tests | **~1%** (15 files, 1.8K LOC) — **CRITICAL GAP** | Provider metadata (52/52), provider smoke (131/131), checkpoint (44/44). No integration, CLI, tool, or gateway tests. |

---

## Next Priorities (highest impact first)

1. **Config: auxiliary sub-configs (56 keys)** — vision/web_extract/compression/skills_hub/approval/mcp/title/triage/kanban/profile/curator provider configs
2. **Providers: 6 OpenAI-compat ports** — Nous, Alibaba, StepFun, Minimax, Novita, ZAI (each ~100-150 LOC)
3. **Config: TTS (17 keys)** — provider, model, voice, cache, playback settings
4. **Tools: 7 missing** — discord, feishu doc+drive, video analyze+generate, MoA
5. **Tests: first test suites** — config, providers, CLI, tools, gateway

See `.hermes/mind-palace/plans/200-gap-roadmap.md` for the complete 200-item gap tracker.

## Build

```bash
cd C/
make -j$(nproc)
export HERMES_MODEL=deepseek-v4-flash
export HERMES_PROVIDER=deepseek
export HERMES_BASE_URL=https://api.deepseek.com/v1
export HERMES_API_KEY=sk-your-key
./hermes "Hello"
```
