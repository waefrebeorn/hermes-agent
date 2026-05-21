---
name: slermes-c-roadmap
description: >
  100-phase implementation roadmap for 1:1 parity C translation of Python Hermes.
  Current: ~8-12% complete (57K C vs 433K Python).
---

# Slermes C — 100-Phase Feature Roadmap (May 21)

**Current: ~8-12% complete** (57,428 LOC C vs 432,870 LOC Python)
**Target: 100% user-swappable parity**

## Phase Structure

Each phase = implement → compile → test → verify → commit.
Phases build on each other — do NOT skip phases.
P0 = blocks daily use. P1 = unlocks capabilities. P2 = polish. P3 = nice-to-have.

---

## FOUNDATION — Phases 1-20

### Phase 1: Config key expansion (408 missing → complete)
Config keys: `agent.*`, `approvals.*`, `display.*`, `security.*`, `terminal.*` sections.
Parse ALL 424 keys from config.yaml into hermes_config_t. Nested YAML parsing.
**P0 — without this, config doesn't work.**

### Phase 2: Environment variable bridge
Add `SLERMES_*` env vars for ALL 424 config keys.
Backward compat: `HERMES_*` fallback.
`.env` parser for all env vars.
**P0**

### Phase 3: Config validation + defaults
Add validation (type checking, range checking).
Add missing default values from Python's DEFAULT_CONFIG.
Config section for `_config_version`.
**P0**

### Phase 4: CLI command completeness audit
Review all 70 C command handlers. Replace `printf` stubs with real implementations.
Subcommand support for `/cron list`, `/skills search`, etc.
Gateway-only / CLI-only scoping flags.
**P1**

### Phase 5: Terminal backend abstraction
Python: `terminal.backend` supports local/Docker/SSH/Modal/Daytona/Singularity.
C: add `env_type` to config. Implement at minimum local + Docker via API.
Terminal config: cwd, timeout, lifetime_seconds, env_passthrough.
**P0 — needed for non-trivial terminal use.**

### Phase 6: File tools expansion (read_file, write_file, search_files)
Python: file_tools.py ~800 LOC. C: file.c ~260 LOC.
Add: binary file handling, image detection, atomic writes, file state tracking.
search_files: add ripgrep backend fallback, file_glob support.
**P1**

### Phase 7: Patch tool expansion
Python: multi-file patch support (V4A format), hunk-level operations.
C: add mode='patch' support for V4A multi-file patches.
**P1**

### Phase 8: Security config keys implementation
Add `security.*` keys: redact_secrets, tirith_enabled/path/timeout/fail_open, allow_lazy_installs, allow_private_urls, website_blocklist.
Implement website blocklist (DNS-based + pattern-based).
**P0 — security gaps affect daily use.**

### Phase 9: Approval system expansion
Python: 1,393 LOC. C: 355 LOC.
Add: approval timeout (`approvals.timeout`), TUI integration, cron mode approval (`approvals.cron_mode`), destructive slash confirm (`approvals.destructive_slash_confirm`).
Granular approval levels (tool-level vs command-level).
**P0 — approval prompts broken without these.**

### Phase 10: Delegation/subagent system
Python: delegate_tool.py 2,801 LOC. C: delegate.c 135 LOC.
Add: orchestrator mode, max concurrent children, spawn depth, child timeout.
Subagent config: model, provider, base_url, api_key, max_iterations, child_timeout.
Credential passing to children. Result collection.
**P0 — delegation is core workflow.**

### Phase 11: Agent loop — budget tracking
Python: iteration_budget.py 284 LOC.
Add: iteration budget, token budget, time budget.
Grace call for budget exhaustion.
**P1**

### Phase 12: Agent loop — fallback model chain
Implement `fallback_providers` config. On provider failure, try next in chain.
Model-level fallback (if model fails, try fallback model).
**P1**

### Phase 13: Agent loop — credential pool
Python: credential_pool.py 1,246 LOC.
Add credential pool with multi-key rotation, rate-limit awareness.
Credential sources: env vars, config file, keychain.
**P1**

### Phase 14: Agent loop — conversation checkpointing
Python: checkpoint_manager.py.
Add: auto-save snapshots, load from checkpoint, max_snapshots config.
**P1**

### Phase 15: Interrupt handling + state save
Clean interrupt with session state save.
Graceful shutdown on SIGINT/SIGTERM.
**P0 — prevents data loss.**

---

## TOOLS EXPANSION — Phases 16-45

### Phase 16: MCP infrastructure (Phase A — client)
**Python: 5,620 LOC across 4 files. C: 0 LOC.**
Add: MCP client (WebSocket/stdio transport), JSON-RPC message handling.
Tool registration from MCP servers. `mcp_servers` config parsing.
**P0 — MCP is a core feature.**

### Phase 17: MCP infrastructure (Phase B — OAuth)
MCP OAuth flow: authorization, token refresh, credential storage.
**P1**

### Phase 18: MCP infrastructure (Phase C — config management)
MCP server config management: add/remove/list servers.
`/reload-mcp` command implementation.
**P1**

### Phase 19-21: Web search backends (3 phases)
Add provider abstraction for web search. Implement: Tavily, Brave, Google.
Search backend config: `web.search_backend`, `web.extract_backend`.
Structured extraction via Firecrawl/Jina AI.
**P1**

### Phase 22-24: Vision system (3 phases)
Python: vision_tools.py 1,421 LOC.
Add: vision provider abstraction, video analysis, image routing.
URL image input with download timeout.
Multi-provider support via `auxiliary.vision.*` config.
**P1**

### Phase 25-27: Voice/TTS (3 phases)
Python: tts_tool.py 2,369 LOC.
Add TTS provider abstraction. Implement: Edge TTS, ElevenLabs, OpenAI TTS.
Add STT (speech-to-text) providers.
Transcription tools.
**P2**

### Phase 28-29: Image generation (2 phases)
Provider registry for image gen. Implement multiple backends.
Image routing + safety checks.
**P2**

### Phase 30-32: Browser — CDP headed browser (3 phases)
Build on existing WebSocket client. Add full CDP operations:
- Tab management (create/close/list tabs)
- Page navigation with load event tracking
- Full page rendering + element querying
- Screenshot capture with base64 output
- JavaScript execution (Runtime.evaluate)
- Console log capture
- Dialog handling (alert/confirm/prompt)
- Cookie/network control
- CDP session management
**P0 — text-only browser is insufficient.**

### Phase 33-34: Browser — Camofox integration (2 phases)
Camofox persistent browser sessions.
Browser supervision (process management, auto-restart).
**P2**

### Phase 35-37: Missing tools — Feishu (3 phases)
feishu_doc_read, feishu_drive_* (4 tools).
Feishu/Lark document and drive integration.
**P2**

### Phase 38-39: Missing tools — Discord (2 phases)
discord tool and discord_admin tool.
Direct message, channel management.
**P2**

### Phase 40: Missing tools — Video generation + analysis
video_generate, video_analyze tools.
**P3**

### Phase 41: Missing tools — Mixture of Agents
mixture_of_agents tool. Multi-LLM ensemble.
**P3**

### Phase 42: Missing tools — Yuanbao bot tools
yb_query_group_info, yb_query_group_members, yb_send_dm, yb_search_sticker, yb_send_sticker.
All 5 Yuanbao-specific tools.
**P2**

### Phase 43: Missing tools — Code execution expansion
Python: code_execution_tool.py ~600 LOC. C: exec_code.c 113 LOC.
Add: sandbox mode, Docker execution backend, timeout config.
**P1**

### Phase 44: process tool expansion
Python: process_registry.py ~400 LOC.
Add: background process lifecycle, notify on complete, watch patterns.
Process write/submit/close/EOF actions.
**P1**

### Phase 45: cron tool expansion
Python: cronjob_tools.py ~400 LOC.
Add: job scheduling at specific times, pause/resume, job history.
Max parallel jobs, wrap response config.
**P1**

---

## INFRASTRUCTURE — Phases 46-70

### Phase 46-48: Plugin system (3 phases)
Python: 39,406 LOC across 18 plugin types.
Build proper plugin architecture:
- Plugin discovery (scan `~/.slermes/plugins/`)
- Plugin metadata (name, version, author, deps)
- Plugin configuration
- Lifecycle hooks (on_boot, on_message, on_tool_call, on_shutdown)
- Plugin enable/disable commands
**P0 — plugins enable all provider backends.**

### Phase 49-51: Model providers — generic framework (3 phases)
Python: 29+ providers. C: 3 hardcoded.
Build provider abstraction: plugin-based provider loading.
Provider config: name, api_key, base_url, model list.
Fallback chains. Credential pool integration.
Implement: OpenRouter, DeepSeek, xAI, Azure, AWS Bedrock.
**P0 — need more than 3 providers for daily use.**

### Phase 52-54: Model providers — remaining (3 phases)
Implement remaining 20+ providers: HuggingFace, NVIDIA, Novita, StepFun, Alibaba (Qwen), Kimi, Nous, etc.
Provider-specific features (e.g., Bedrock guardrails, OpenRouter scoring).
**P1**

### Phase 55: Service tier + reasoning effort
Implement `agent.service_tier` (fast/standard/priority) and `agent.reasoning_effort` (low/medium/high).
Model routing based on service tier.
**P2**

### Phase 56: Prompt caching
Implement `prompt_caching` config. Cache-aware message formatting.
Provider-specific caching headers (Anthropic, OpenAI).
**P2**

### Phase 57-58: Session DB (2 phases)
Python: hermes_state.py 3,273 LOC with SQLite FTS5.
C: implement SQLite-based session store with FTS5 full-text search.
Session metadata: title, message count, model, token usage, timestamps.
Auto-prune, vacuum. Session listing with pagination.
**P1 — grep-based search is slow.**

### Phase 59: Session search — FTS5 integration
Integrate session_search tool with new SQLite FTS5 backend.
Ranked results, highlighted snippets, relevance scoring.
**P1**

### Phase 60-61: Memory system (2 phases)
Python: plugins/memory/ Honcho + Mem0 + Supermemory.
C: add memory provider abstraction. Implement at minimum local file-based + Honcho.
Memory char limits, user profile enable/disable.
**P1**

### Phase 62-63: Skills system expansion (2 phases)
Python: skills_hub.py, skills_sync.py, skill_provenance.py, skills_guard.py.
C: add skills hub (search/browse remote skills), skills sync (git-based).
Skill provenance verification, security guard, usage tracking.
Skill bundles (multi-skill aliases).
**P2**

### Phase 64: Skills curator
Python: curator.py — background maintenance of skills.
C: implement curator tool. Skill health checks, auto-update.
**P3**

### Phase 65: Context engine
Python: context.engine config — compressor / drop-messages.
Add alternative context engines. Configurable compression strategies.
**P2**

---

## USER INTERFACE — Phases 66-80

### Phase 66-68: CLI improvements (3 phases)
Autocomplete (prompt_toolkit-like tab completion).
Rich formatting (tool output colors, syntax highlighting).
Status bar (model, provider, iteration count, token usage).
Persistent command history.
**P1**

### Phase 69-70: Display config keys (2 phases)
Python: display.* 25 keys. C: 2 keys.
Implement: compact mode, show_reasoning, show_cost, timestamps, language, inline_diffs, tool_progress, final_response_markdown, streaming display toggle, etc.
**P2**

### Phase 71-72: TUI — basic ncurses expansion (2 phases)
Add: split panes (agent output + tool status), session list view, file browser.
Mouse support, animated spinners, status indicators.
**P2**

### Phase 73-75: TUI — advanced (3 phases)
WebSocket-based communication with agent backend.
JSON-RPC protocol for real-time state updates.
Markdown rendering with syntax highlighting.
Model switching UI, platform status panel.
**P3**

### Phase 76-77: Gateway — remaining platforms (2 phases)
Add missing platform adapters: feishu_comment, signal_rate_limit, wecom_callback, etc.
API Server platform (REST API endpoint).
Platform helpers: shared HTTP client limits, message formatting, file upload.
**P2**

### Phase 78-79: Gateway — SSE streaming (2 phases)
Server-Sent Events for gateway streaming.
Platform-specific stream formatting (Telegram markdown, Discord embeds).
**P1 — gateway streaming is core.**

### Phase 80: Gateway — platform-specific features
Telegram reactions, Discord slash commands, Slack message formatting.
Per-platform config sections.
**P2**

---

## POLISH & PERFORMANCE — Phases 81-95

### Phase 81: Streaming — tool call streaming
Stream tokens AND detect tool calls mid-stream.
Parallel streaming + tool dispatch.
**P1**

### Phase 82: Streaming — diagnostics
Stream diagnostics: token rate, time to first token, latency breakdown.
**P3**

### Phase 83: Smart context compression tuning
Implement compression.protect_last_n, protect_first_n, target_ratio, threshold.
Hygiene hard message limit. Abort on summary failure config.
**P2**

### Phase 84: Checkpoints
Checkpoint manager: auto-save, max_snapshots, max_total_size_mb, auto_prune, retention_days.
**P2**

### Phase 85: Trajectory saving
Trajectory recording and replay. Save tool calls, LLM responses.
**P3**

### Phase 86: Tool loop guardrails
Python: tool_loop_guardrails.* 11 keys.
Implement: max_tool_calls_per_turn, max_consecutive_same_tool, toll-free tool designations.
**P2**

### Phase 87: File mutation verifier
Python: display.file_mutation_verifier.
After write_file/patch, verify the file was written correctly.
**P2**

### Phase 88: Redact secrets
Python: security.redact_secrets + agent/redact.py.
Scrub API keys, passwords, tokens from tool output and log files.
**P1 — security-critical.**

### Phase 89: Privacy — PII redaction
Python: privacy.redact_pii.
Personally identifiable information redaction.
**P2**

### Phase 90: Website blocklist
Implement security.website_blocklist.*: enabled, allowlist, blocklist, mode (block/warn).
**P1 — prevents SSRF to malicious sites.**

### Phase 91: Command allowlist
Implement command_allowlist. Approved commands only. Deny by default.
**P1 — security-critical.**

### Phase 92: Error classification
Python: agent/error_classifier.py.
Classify LLM errors (auth, rate limit, timeout, model overload).
Provide actionable error messages.
**P2**

### Phase 93: Background review system
Python: agent/background_review.py.
Background agent for reviewing tool results and suggesting improvements.
**P3**

### Phase 94: Human delay simulation
Python: human_delay.mode (off/auto/fixed).
Add delay between tool calls to simulate human reading time.
**P3**

### Phase 95: Goal system
Python: goals.* config.
Goal management: set, track, complete. Goal-based conversation steering.
**P2**

---

## TESTING & VERIFICATION — Phases 96-100

### Phase 96: Test infrastructure
Python: ~17,000 tests across ~900 files.
C: build test framework (Unity/CTest). Add unit tests for:
- Config parsing (all 424 keys)
- All tool handlers (all 61 tools)
- CLI commands (all 69+ commands)
- Security (all dangerous patterns)
**P1 — without tests, nothing is verified.**

### Phase 97: Integration tests
End-to-end tests: CLI → agent loop → LLM → tool execution → response.
Gateway integration tests: webhook → agent → response.
Session DB: save/load/search.
**P1**

### Phase 98: Performance benchmarks
Latency: time-to-first-token, total response time.
Memory: RSS at idle, RSS during agent loop.
Size: binary size, config file size.
Compare against Python hermes baseline.
**P2**

### Phase 99: DA audit — comprehensive
Full 4-phase Devil's Advocate audit comparing every subsystem.
Cross-reference all claims against actual binary output.
Update all documentation.
**P1 — must pass before declaring done.**

### Phase 100: Release — user swap test
Document swap procedure: `alias hermes=./slermes`.
User acceptance test: use slermes for 1 full day.
Fix any issues found. Declare parity achieved.
**P0 — the final goal.**

---

## Summary

| Phase Range | Area | Phases | Effort | Dependencies |
|------------|------|--------|--------|-------------|
| 1-15 | Foundation | 15 | **HIGH** | None |
| 16-45 | Tools | 30 | **VERY HIGH** | Phases 1-15 |
| 46-70 | Infrastructure | 25 | **VERY HIGH** | Phases 16-45 |
| 66-80 | UI | 15 | **HIGH** | Phases 1-15 |
| 81-95 | Polish | 15 | **MEDIUM** | Phases 16-65 |
| 96-100 | Testing | 5 | **HIGH** | All above |

**Total: ~100 phases. Estimated C LOC target: ~400,000 (from current 57K).**