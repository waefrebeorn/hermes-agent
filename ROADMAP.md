# WuBu Slermes — C Translation Roadmap

**HONEST STATUS: ~45% complete.** 53K LOC C vs 431K Python. All 200 phases compiled.
Runtime: partial (CLI works, LLM requires API key). Verified against Python: NOT YET.

---

## Current Reality (DA audit 2026-05-21)

| Subsystem | Completeness | Gap |
|-----------|------------|-----|
| Config keys | **48%** (154/318) | Flat keys OK; v1 nested model.provider not mapped. Missing: credential_pool, fallback_providers, toolset arrays, platform-specific keys, logging, stt/tts/vision/auxiliary/kanban sub-dicts. |
| Tools | **92%** (74 registered, 54 expected all found) | Missing: some MCP resource tools, kanban sub-tools, environment backends (docker/ssh). |
| CLI commands | **85%** (72/85) | Missing: /status, /logs, /doctor, /setup, /update, /completion as subcommands. |
| Providers | **10%** (3/29) | 3 implemented (openai, anthropic, google) + metadata. Missing: 26 providers (deepseek, xai, openrouter, bedrock, azure, groq, together, etc.) |
| Gateway platforms | **95%** (19/~20) | Full set. Missing OAuth flows, bot token verification. |
| MCP | **70%** | Core protocol, stdio/SSE, auth, resources, prompts, tools. Missing: streaming, progress notifications, sampling, roots. |
| Plugin system | **25%** | Core .so plugin system works. Missing: 12+ plugin types (memory providers, model providers, kanban, achievements, observability). |
| Session DB | **100%** | SQLite with FTS5, metadata, CRUD, export, branch, migration. 231 real sessions loaded. |
| Delegation | **80%** | Concurrent children, orchestrator, isolation, credential filtering. Missing: nested delegation. |
| Security | **70%** | Redaction, blocklist, vault, audit log, rate limiting, sandbox. Missing: TIRITH path validation. |
| Agent loop | **75%** | Full conversation loop, budget, parallel dispatch, interrupt, compression, checkpoints, background review, system prompt caching. Missing: some edge cases. |
| TUI | **50%** | Core ncurses split-pane TUI builds. Missing: HTML rendering, tab completion, config editor. |
| Tests | **<1%** (12 files, 1.3K LOC) | No pytest-equivalent. Manual verification only. CRITICAL GAP. |
| Memory | **90%** | Storage abstraction, TTL, dedup, compression, search, export/import. |
| Cron | **90%** | SQLite store, retry, chaining, CLI management. |
| Skills | **90%** | Scanning, validation, sync, bundles, curator, dependencies. |

---

## Immediate Next Steps

1. **Config v1 nesting** — Fix C config loader to read nested model/provider/base_url from YAML sections (e.g. model: { api_mode, max_tokens }) — currently reads flat keys only
2. **Runtime parity testing** — Compare C output vs Python on: /sessions count, /tools list, /status output
3. **Provider expansion** — Add deepseek, xai, openrouter (most-used after big 3)
4. **Test harness** — Write C test runner to probe each subsystem

## Long-term Priority

| Priority | Area | Why |
|----------|------|-----|
| P0 | Config | Blocks everything — model/provider not loaded from file |
| P0 | Providers | Blocks LLM calls beyond OpenAI |
| P0 | Tests | Without tests, nothing is verified |
| P1 | Gateway features | OAuth, webhook verification |
| P1 | MCP depth | Streaming, progress, sampling |
| P2 | Plugins | Remaining 12+ plugin types |
| P2 | Remaining config keys | 164 missing keys |
| P3 | ACP/LSP | IDE integration |
