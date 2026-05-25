# Hermes C Translation — Achievement Vault

> Durable record of all completed ports, bug fixes, test additions, and infrastructure improvements. Updated 2026-06-01.

## Milestone: Test Suite 232/0/0

**Achieved:** June 1, 2026
**Detail:** 232 tests across 195 files, all passing, zero skips. ~428 new tests added in May 25-June 1 sprint.
**History:** 175 → 202 (May 20) → 209 (May 22) → 217 (May 23) → 222 (May 24) → 226 (May 25) → 232 (Jun 1)

## Milestone: All Provider Tests Complete

**Achieved:** June 1, 2026
**Detail:** Every provider module now has unit tests covering URL building, headers, response parsing, stream chunk parsing, error handling, and free-response safety:

| Provider | Tests | Features Tested |
|----------|-------|-----------------|
| OpenAI | 54 | URL, headers, parse (basic/tool_calls/reasoning/error), SSE stream |
| Anthropic | 74 | Multi-block, thinking, tool_use, SSE events (block_delta/start/msg) |
| Google | 64 | Parts array, functionCall, usageMetadata, finishReason, SSE |
| DeepSeek | 60 | Reasoning_content, FIM URL/body/response, cache-ttl headers |
| Generic provider.c | 43 | Registration, creation, FIM detection, credential pool |

**Total: 295 provider tests. Zero network calls — all structural/parsing.**

## Milestone: Plugin Extension Tests

**Achieved:** June 1, 2026
**Detail:** 30 tests for plugin_ext.c covering config-to-JSON serialization, null safety across all lifecycle functions, empty registry listing. Plugin .so loading tested by lib/libplugin independently.

## Milestone: Security Rules Engine Tests

**Achieved:** May 25, 2026
**Detail:** test_tirith.c (73 tests) covers all inline security scanning and policy rule patterns. Full pass.

## Milestone: Test Infrastructure Principles

- **DIRECT FILE ACCESS** — Use `open()` in execute_code Python, not read_file (avoids line-number prefix contamination)
- **Mock ops, not network** — Provider tests use registered mock/provider ops via provider_register(), no API keys
- **SSE format awareness** — OpenAI/DeepSeek use `data: {...}` prefix, Anthropic uses `event:/data:` lines, Google uses `data: {...}`
- **-Wl,--unresolved-symbols=ignore-all** — Allows testing individual provider .c files without linking all 9 providers

## Milestone: 50 Agent .c Modules

**Achieved:** May 2026
**Detail:** agent_loop, audit, auxiliary_client, budget_tracker, checkpoint, context, context_engine, context_references, credential_pool, curator, fallback_routing, file_safety, gemini_schema, hook_registry, i18n, image_routing, llm_client, lmstudio_reasoning, manual_compression_feedback, markdown_tables, moonshot_schema, onboarding, plugin_ext, portal_tags, prompt_caching, provider, provider_anthropic, provider_azure, provider_bedrock, provider_custom, provider_deepseek, provider_google, provider_metadata, provider_openai, provider_openrouter, provider_xai, redact, retry_utils, sanitize, shell_hooks, skill_bundles, skill_preprocessing, subdir_hints, system_prompt, think_scrubber, title, tool_guardrails, trajectory, usage_pricing, vault

## Milestone: 88 CLI Commands (More Than Python)

**Detail:** C CLI has 88 commands vs Python's 87. Unique C-only commands: `/agents`, `/bundles`, `/cctx`, `/portal`, deprecation, and more.

## Milestone: C Translation Gap Reduction

**Real gap count: ~25-30** (down from ~394 claimed in battleship-v5). The original DA counted Python filenames without accounting for C aliases (e.g., `prompt_builder.py` → `system_prompt.c`, `title_generator.py` → `title.c`). Actual unported modules:

| Module | LOC | Status |
|--------|-----|--------|
| context_compressor.py | 1749 | Inline in agent_loop.c, no standalone module |
| conversation_compression.py | 604 | Related, inline |
| insights.py | 931 | No C equivalent |
| stream_diag.py | 281 | Basic timing in llm_client.c, richer diag missing |
| memory_provider.py | 280 | Plugin interface, not ported |

## Infrastructure Ports

- ACP: server, edit_approval, events, permissions, resource — 5 .c files
- MCP: Streamable HTTP, dynamic discovery, sampling — 1 .c service
- Cron: Scheduler, jobs, SQLite, CLI — 3 .c files
- TUI: Session browser, response wrapping, activity feed, spinner
- Gateway: 19 platforms, rate limiting, auth, webhook validation
- Libs: 58 library units (json, http, mcp, yaml, crypto, sqlite, websocket, plugin, skin, etc.)

## CLI Enhancement Milestones

| Feature | Detail |
|---------|--------|
| Tab completion + history | lib/liblineedit/ |
| Table output (─── separator) | ASCII table formatting |
| Bracketed paste | Multi-line input |
| /insights with token breakdown | Per-model, duration, cost |
| /logs --follow | Log streaming |
| Profile clone/delete | Config management |
| /tools-verify | Expected tool list check |
| 88 commands total | More than Python's 87 |

## Bug Fixes

- patch.c: 4-strategy fuzzy matching chain — 25 unit tests
- agent_loop.c: Multiple memory/state issues closed
- db.c: FORTIFY_SOURCE buffer sizes 4096→8192
- session_crud.c: double-free on char[64] embedded field
- provider_openai/anthropic/google/deepseek: all parsing paths verified
- sanitize.c: use-after-free (val_start after realloc)
- agent_loop.c: dirname() portability fix (glibc modifies arg in-place)
